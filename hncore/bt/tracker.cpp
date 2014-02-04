/*
 *  Copyright (C) 2006 Alo Sarv <madcat_@users.sourceforge.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <hncore/bt/tracker.h>
#include <hncore/bt/protocol.h>
#include <hncore/bt/bittorrent.h>
#include <hncore/partdata.h>
#include <boost/spirit.hpp>
#include <boost/spirit/dynamic/if.hpp>
#include <boost/spirit/dynamic/while.hpp>
#include <boost/spirit/actor/clear_actor.hpp>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string/case_conv.hpp>

namespace Bt {

const std::string TRACKER("bt.tracker");

//! Temporary structure for usage during tracker response parsing
struct Peer {
	Peer() : m_cip(), m_port() {}
	std::string m_id;
	std::string m_ip;
	uint32_t m_cip;
	uint16_t m_port;
	void clear() {
		m_id.clear();
		m_ip.clear();
		m_port = 0;
		m_cip = 0;
	}
};

// Tracker class
// -------------
Tracker::Tracker(
	const std::string &host, const std::string &url, uint16_t port,
	TorrentInfo info
) : m_info(info), m_host(host), m_url(url), m_port(port), m_interval(), 
m_minInterval(), m_completeSrc(), m_partialSrc() {
	CHECK_THROW(host.size());
	CHECK_THROW(url.size());
	CHECK_THROW(port);

	IPV4Address addr(host, port);
	if (addr.getAddr() < std::numeric_limits<uint32_t>::max()) {
		m_addrs.push_back(addr);
		m_curAddr = m_addrs.begin();
		connect(addr);
	} else {
		m_curAddr = m_addrs.end();
		hostLookup();
	}
}

void Tracker::hostLookup() {
	DNS::lookup(m_host, this, &Tracker::hostResolved);
	logMsg("Looking up host " + m_host + "... ");
}

void Tracker::hostResolved(HostInfo info) {
	if (info.error()) {
		logError(
			boost::format("[%s] Host lookup failed: %s")
			% info.getName() % info.errorMsg()
		);
		Utils::timedCallback(this, &Tracker::hostLookup, 60000);
	} else {
		m_addrs = info.getAddresses();
		for (uint32_t i = 0; i < m_addrs.size(); ++i) {
			m_addrs[i].setPort(m_port);
		}
		m_curAddr = m_addrs.begin();
		connect(*m_curAddr);
	}
}

void Tracker::connect(IPV4Address addr) try {
	logMsg(
		boost::format("[%s] Connecting to tracker %s[%s]:%d...")
		% getName() % m_host % addr.getAddrStr() % m_port
	);
	m_socket.reset(new TcpSocket(this, &Tracker::onSocketEvent));
	m_socket->connect(addr);
} catch (std::exception &e) {
	logError(
		boost::format("[%s] Error connecting to tracker[%s]: %s")
		% getName() % m_host % e.what()
	);
	onSocketEvent(m_socket.get(), SOCK_ERR);
}

void Tracker::onSocketEvent(TcpSocket *sock, SocketEvent evt) {
	CHECK_RET(sock == m_socket.get());

	if (evt == SOCK_CONNECTED) {
		sendGetRequest();
	} else if (evt == SOCK_READ) {
		*m_socket >> m_inBuffer;
		parseBuffer();
	} else if (
		evt == SOCK_CONNFAILED || evt == SOCK_TIMEOUT ||
		evt == SOCK_ERR || evt == SOCK_LOST
	) {
		logError(
			boost::format(
				"[%s] Connection to tracker failed/lost[%s]."
			) % getName() % m_host
		);
		if (m_inBuffer.size()) {
			parseBuffer();
		}
		if (m_inBuffer.size()) {
			logDebug(
				"Buffered data but did not parse: " 
				+ Utils::hexDump(m_inBuffer)
			);
		}
		m_socket.reset();
		m_inBuffer.clear();
		if (m_addrs.size() > 1) {
			if (m_curAddr != m_addrs.end()) {
				++m_curAddr;
			}
			if (m_curAddr == m_addrs.end()) {
				m_curAddr = m_addrs.begin();
			}
			connect(*m_curAddr);
		} else if (m_interval) {
			Utils::timedCallback(
				boost::bind(&Tracker::connect, this,*m_curAddr), 
				m_interval * 1000
			);
			logMsg(
				boost::format("[%s] Re-trying in %dm %ds.")
				% m_host % (m_interval / 60)
				% (m_interval % 60)
			);
		} else if (!m_interval) {
			Utils::timedCallback(
				boost::bind(&Tracker::connect, this,*m_curAddr), 
				60000
			);
		}
	} else if (evt == SOCK_BLOCKED) {
		logError(
			boost::format(
				"[%s] Connection attempt to the Tracker[%s] "
				"was blocked by IpFilter!"
			) % getName() % m_host
		);
	}
}

void Tracker::parseBuffer() {
	using namespace boost::spirit;
	uint32_t rc = 0;
	std::string rcText;
	uint32_t clen = 0;
	std::string ctype;
	std::string cenc, tenc;

	std::string sepc("\r\n");
	uint32_t headerEnd = m_inBuffer.find(sepc + sepc);
	if (headerEnd == std::string::npos) {
		headerEnd = m_inBuffer.find("\n\n");
		if (headerEnd == std::string::npos) {
			return;
		} else {
			sepc = "\n";
		}
	}
	std::string headerData = m_inBuffer.substr(0, headerEnd);
	std::string contentData = m_inBuffer.substr(headerEnd + sepc.size()*2);
	boost::char_separator<char> sep(sepc.c_str());
	boost::tokenizer<boost::char_separator<char> > tok(headerData, sep);
	boost::tokenizer<boost::char_separator<char> >::iterator it(
		tok.begin()
	);
	while (it != tok.end()) {
		std::string tmp(*it);
		parse_info<> info = parse(
			tmp.c_str(),
			(
				as_lower_d["content-length:"]
				>> uint_p[assign_a(clen)]
			) | (
				as_lower_d["content-type:"]
				>> *anychar_p[push_back_a(ctype)]
			) | (
				as_lower_d["content-encoding:"]
				>> *anychar_p[push_back_a(cenc)]
			) | (
				as_lower_d["transfer-encoding:"]
				>> *anychar_p[push_back_a(tenc)]
			) | (
				as_lower_d["http/"] >> uint_p >> '.' >> uint_p
				>> uint_p[assign_a(rc)]
				>> *anychar_p[push_back_a(rcText)]
			) | *anychar_p
			, space_p
		);
		++it;
	}
	if (clen && contentData.size() != clen) {
		logTrace(TRACKER, "Dont have full content yet.");
		return;
	}
	if (tenc == "chunked") {
		// HTTP/1.1 protocol - doesn't work yet properly
		logTrace(TRACKER, "====" + contentData + "====");
		int chunkLen;
		std::string content;
		parse_info<> info = parse(
			contentData.c_str(),
			hex_p[assign_a(chunkLen)] >> eol_p
			>> repeat_p(boost::ref(chunkLen))
			   [anychar_p[push_back_a(content)]]
			>> eol_p >> "0" >> eol_p >> eol_p
		);
		if (info.full) {
			parseContent(content);
		} else {
			logDebug(
				boost::format("Parse failed at: %s") % info.stop
			);
			logDebug(boost::format("chunklen = %d") % chunkLen);
			logDebug(boost::format("content = %s") % content);
			return;
		}
	}
	logMsg(boost::format("HTTP reply from tracker: %d %s") % rc % rcText);
#ifndef NDEBUG
	std::string tmpContent(contentData);
	// get rid of non-printable characters
	for (uint32_t i = 0; i < tmpContent.size(); ++i) {
		if (tmpContent[i] < 32 || tmpContent[i] > 126) {
			tmpContent.replace(i, 1, ".");
		}
	}
	logTrace(TRACKER, "Content data: " + tmpContent);
#endif
	parseContent(contentData);
}

void Tracker::parseContent(const std::string &data) {
	INIT_PARSER();

	std::string errorMsg, warningMsg, cpList;

	std::vector<Peer> peerList;
	Peer tmpPeer;
	rule<> peer_list = 'l' >> *(
		'd' >> *( ("7:peer id" >> BSTR(tmpPeer.m_id))
			| ("2:ip"      >> BSTR(tmpPeer.m_ip))
			| ("4:port"    >> BINT(tmpPeer.m_port))
			| unknown
			)
		>> ch_p('e')[push_back_a(peerList, tmpPeer)][clear_a(tmpPeer)]
	) >> 'e';

	parse_info<> info = parse(
		data.data(), data.data() + data.size(),
		'd' >> *( ("14:failure reason"  >> BSTR(errorMsg))
			| ("15:warning message" >> BSTR(warningMsg))
			| ("8:interval"         >> BINT(m_interval))
			| ("12:min interval"    >> BINT(m_minInterval))
			| ("10:tracker id"      >> BSTR(m_id))
			| ("8:complete"         >> BINT(m_completeSrc))
			| ("10:incomplete"      >> BINT(m_partialSrc))
			| ("5:peers"            >> (peer_list | BSTR(cpList)))
			| unknown
			)
		>> 'e'
	);
	if (!info.full) {
		logTrace(TRACKER,
			boost::format("Tracker response:\n%s") 
			% Utils::hexDump(data)
		);
		logTrace(TRACKER,
			boost::format("Parsing failed at: %s") 
			% Utils::hexDump(info.stop)
		);
		return;
	}

	if (errorMsg.size()) {
		logError(getName() + ": " + errorMsg);
	}
	if (warningMsg.size()) {
		logWarning(getName() + ": " + warningMsg);
	}
	if (cpList.size() && !peerList.size()) {
		CHECK(cpList.size() % 6 == 0);
		std::istringstream i(cpList);
		tmpPeer.clear();
		for (uint32_t j = 0; j < cpList.size() / 6; ++j) {
			tmpPeer.m_cip = Utils::getVal<uint32_t>(i);
			tmpPeer.m_port = Utils::getVal<uint16_t>(i);
			tmpPeer.m_port = SWAP16_ON_LE(tmpPeer.m_port);
			peerList.push_back(tmpPeer);
		}
	}
	if (m_minInterval) {
		logMsg(
			boost::format("Minimum tracker reask interval: %dm%ds")
			% (m_minInterval / 60) % (m_minInterval % 60)
		);
	}
	if (m_interval) {
		logMsg(
			boost::format("Tracker reask interval:         %dm%ds")
			% (m_interval / 60) % (m_interval % 60)
		);
	}
	if (m_completeSrc) {
		logMsg(
			boost::format("Complete sources:               %d")
			% m_completeSrc
		);
	}
	if (m_partialSrc) {
		logMsg(
			boost::format("Partial sources:                %d")
			% m_partialSrc
		);
	}
	if (peerList.size()) {
		logMsg(
			boost::format("Received %d peers from tracker.")
			% peerList.size()
		);
		for (uint32_t i = 0; i < peerList.size(); ++i) {
			IPV4Address addr;
			if (peerList[i].m_cip) {
				addr.setAddr(peerList[i].m_cip);
			} else {
				addr.setAddr(peerList[i].m_ip);
			}
			addr.setPort(peerList[i].m_port);
			try {
				foundPeer(addr);
			} catch (std::exception &e) {
				logDebug(
					boost::format(
						"Error while creating BT "
						"peer connection: %s"
					) % e.what()
				);
			}
		}
	}
	if (!m_interval) {
		m_interval = 60;
	}

	Utils::timedCallback(
		boost::bind(&Tracker::connect, this, *m_curAddr),
		m_interval * 1000
	);
	m_socket->disconnect();
	m_socket.reset();
	m_inBuffer.clear();
}

void Tracker::sendGetRequest() {
	boost::format fmt(
		"GET /%s?%s HTTP/1.0\r\n"
//		"User-Agent: Hydranode 0.1.2;Linux\r\n" // optional
		"Connection: close\r\n"
//		"Accept-Encoding: gzip\r\n"  // we don't have gzip decompressor
		"Host: %s:%d\r\n"
		// these are just to be nice (Azureus sends them)
//		"Content-Type: application/x-www-form-urlencoded"
//		"Accept: text/html, image/gif, image/jpeg, *; q=.2, */*; q=.2"
	);
	fmt % m_url;
	std::ostringstream args;
	args << "info_hash="  << Utils::urlEncode(
		m_info.getInfoHash().toString(), true
	) << "&";
	args << "peer_id="    << BitTorrent::instance().getId() << "&";
	args << "key="        << BitTorrent::instance().getKey() << "&";
	args << "port="       << BitTorrent::instance().getPort() << "&";
	args << "uploaded="   << getUploaded() << "&";
	args << "downloaded=" << getDownloaded() << "&";
	if (getPartData()) {
		args << "left=";
		args << getPartData()->getSize() -getPartData()->getCompleted();
		args << "&";
	} else {
		args << "left=0&";
	}
	if (!m_interval) { // if no interval, means we just started up
		args << "event="      << "started" << "&";
	}
	args << "num_want="   << 80 << "&";
	args << "compact="    << 1;
	fmt % args.str() % m_host % m_port;

	std::string final = fmt.str();
	logTrace(TRACKER, "Sending HTTP GET request:\n" + final);
	*m_socket << final << Socket::Endl << Socket::Endl;
	float ratio = 0.0;
	if (getDownloaded()) {
		ratio = static_cast<float>(getUploaded()) / getDownloaded();
	}
	logMsg(
		boost::format(
			"Torrent Statistics: Uploaded: %s "
			"Downloaded: %s Ratio: %5.3f"
		) % Utils::bytesToString(getUploaded())
		% Utils::bytesToString(getDownloaded())
		% ratio
	);
}

} // end namespace Bt
