/*
 *  Copyright (C) 2005-2006 Alo Sarv <madcat_@users.sourceforge.net>
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

#include <hncore/bt/client.h>
#include <hncore/bt/torrent.h>
#include <hncore/bt/files.h>
#include <hncore/bt/bittorrent.h>
#include <hnbase/timed_callback.h>
#include <boost/spirit.hpp>
#include <stack>

#define COL_SEND COL_CYAN
#define COL_RECV COL_GREEN

namespace Bt {
const std::string TRACE("bt.client");
std::map<std::string, std::string> s_clientNames;

void fillClientNames() {
	s_clientNames["AZ"] = "Azureus";
	s_clientNames["BB"] = "BitBuddy";
	s_clientNames["BC"] = "BitComet";
	s_clientNames["CT"] = "CTorrent";
	s_clientNames["ML"] = "MoonlightTorrent";
	s_clientNames["LT"] = "libtorrent";
	s_clientNames["BX"] = "Bittorrent X";
	s_clientNames["TS"] = "Torrentstorm";
	s_clientNames["TN"] = "TorrentDotNET";
	s_clientNames["SS"] = "SwarmScope";
	s_clientNames["XT"] = "XanTorrent";
	s_clientNames["BS"] = "BTSlave";
	s_clientNames["ZT"] = "ZipTorrent";
	s_clientNames["AR"] = "Arctic";
	s_clientNames["SB"] = "Swiftbit";
	s_clientNames["MP"] = "MooPolice";
	s_clientNames["HN"] = "Hydranode";
	s_clientNames["KT"] = "KTorrent";
	s_clientNames["UT"] = "µTorrent";
	s_clientNames["lt"] = "libtorrent";
	s_clientNames["S"] = "Shadow's client";
	s_clientNames["U"] = "UPnP NAT Bit Torrent";
	s_clientNames["T"] = "BitTornado";
	s_clientNames["A"] = "ABC";
	s_clientNames["O"] = "Osprey Permaseed";
}

// Client::Request class
// ---------------------
Client::Request::Request() : m_index(), m_offset(), m_length() {}

Client::Request::Request(uint64_t index, uint64_t offset, uint64_t length)
: m_index(index), m_offset(offset), m_length(length) {}

Client::Request::Request(::Detail::LockedRangePtr r, Torrent *t)
: m_index(), m_offset(), m_length(), m_locked(r) {
	m_index  = m_locked->begin() / t->getChunkSize();
	m_offset = m_locked->begin() - t->getChunkSize() * m_index;
	m_length = m_locked->length();
}

// Client class
// ------------
Client::Client(TcpSocket *sock) : BaseClient(&BitTorrent::instance()),
m_socket(sock), m_addr(sock->getPeer()), m_isChoking(true), m_isInterested(),
m_amChoking(true), m_amInterested(), m_handshakeSent(), m_needParts(), m_file(),
m_partData(), m_torrent(), m_sourceMaskAdded() {
	if (!s_clientNames.size()) {
		fillClientNames();
	}

	logTrace(TRACE, "Client connected.");

	setConnected(true);
	sock->setHandler(this, &Client::onSocketEvent);
	m_inBuffer.append(sock->getData());
	if (m_inBuffer.size()) {
		parseBuffer();
	}
	Utils::timedCallback(boost::bind(&Client::sendPing, this), 2*60*1000);
}

Client::Client(IPV4Address addr) : BaseClient(&BitTorrent::instance()),
m_socket(new TcpSocket), m_addr(addr), m_isChoking(true), m_isInterested(),
m_amChoking(true), m_amInterested(), m_handshakeSent(), m_needParts(), m_file(),
m_partData(), m_torrent(), m_sourceMaskAdded() {
	if (!s_clientNames.size()) {
		fillClientNames();
	}

	logTrace(TRACE, boost::format("Connecting to %s") % addr);

	m_socket->setHandler(this, &Client::onSocketEvent);
	m_socket->connect(addr, 30000);
	Utils::timedCallback(boost::bind(&Client::sendPing, this), 2*60*1000);
}

Client::~Client() {
	m_currentSpeedMeter.disconnect();
	m_currentUploadMeter.disconnect();
	if (m_partData && m_torrent && m_sourceMaskAdded) try {
		m_partData->delSourceMask(m_torrent->getChunkSize(),m_bitField);
	} catch (std::exception &e) {
		logTrace(TRACE,
			boost::format(
				"[%s] Exception while removing source-mask: %s"
			) % m_addr % e.what()
		);
	}
}

void Client::onSocketEvent(TcpSocket *sock, SocketEvent evt) {
	if (
		(m_outRequests.size() || m_requests.size())
		&& m_torrent->getPeerCount() > 50
	) {
		// when downloading or uploading is in progress, 10s timeout
		sock->setTimeout(10000);
	} else {
		sock->setTimeout(2*70*1000); // 140 seconds timeout
	}

	if (evt == SOCK_READ) {
		m_inBuffer.append(sock->getData());
		parseBuffer();
	} else if (evt == SOCK_WRITE && m_requests.size()) {
		sendNextChunk();
	} else if (evt == SOCK_CONNECTED) {
		logTrace(TRACE,
			boost::format("[%s] Connection established.") % m_addr
		);
		setConnected(true);
		sendHandshake();
	} else if (!sock->isConnected()) {
		boost::format fmt("[%s] %s");
		fmt % m_addr;
		switch (evt) {
			case SOCK_LOST:
				fmt % "Remote host closed connection";
				break;
			case SOCK_TIMEOUT:
				fmt % "Connection timed out (no events)";
				break;
			case SOCK_ERR:
				fmt % "Socket error "
					"(remote abnormal disconnection)";
				break;
			case SOCK_CONNFAILED:
				fmt % "Connection attempt refused";
				break;
			default:
				fmt % "Connection lost due to unknown reason";
				break;
		}
		logTrace(TRACE, fmt);
		setConnected(false);
		connectionLost(this);
	}
}

void Client::parsePeerId(const std::string &peerId) {
	if (!peerId.size()) {
		m_clientSoft = "Unknown (no peer-id sent)";
	}
	std::string tmp;
	std::string ver;
	using namespace boost::spirit;
	if (parse(peerId.data(), peerId.data() + peerId.size(),
		'-' >> repeat_p(2)[graph_p[push_back_a(tmp)]]
		>> repeat_p(4)[graph_p[push_back_a(ver)]] >> '-' >> *anychar_p
	).full && s_clientNames[tmp] != "") {
		boost::format fmt("v%d.%d.%d.%d");
		m_clientSoft = s_clientNames[tmp];
		fmt % ver[0] % ver[1] % ver[2] % ver[3];
		m_clientVersion = fmt.str();
	} else if (parse(peerId.data(), peerId.data() + peerId.size(),
		alpha_p[assign_a(tmp)] >> repeat_p(3)[
			alnum_p[push_back_a(ver)]
		] >> "--" >> *anychar_p
	).full && s_clientNames[tmp] != "") {
		boost::format fmt("v%d.%d.%d");
		m_clientSoft = s_clientNames[tmp];
		fmt % ver[0] % ver[1] % ver[2];
		m_clientVersion = fmt.str();
	} else if (parse(peerId.data(), peerId.data() + peerId.size(),
		'M' >> graph_p[push_back_a(ver)] >> '-'
		>> graph_p[push_back_a(ver)] >> '-'
		>> graph_p[push_back_a(ver)] >> "--" >> *anychar_p
	).full) {
		boost::format fmt("v%d.%d.%d");
		fmt % ver[0] % ver[1] % ver[2];
		m_clientSoft = "Mainline";
		m_clientVersion = fmt.str();
	} else if (parse(peerId.data(), peerId.data() + peerId.size(),
		"OP" >> repeat_p(4)[anychar_p[push_back_a(ver)]] >> *anychar_p
	).full) {
		boost::format fmt("build %d%d%d%d");
		m_clientSoft = "Opera";
		fmt % ver[0] % ver[1] % ver[2] % ver[3];
		m_clientVersion = fmt.str();
	} else if (parse(peerId.data(), peerId.data() + peerId.size(),
		"XBT" >> repeat_p(4)[anychar_p[push_back_a(ver)]] >> *anychar_p
	).full) {
		boost::format fmt("v%d.%d.%d %s");
		fmt % ver[0] % ver[1] % ver[2];
		if (ver[3] == 'd') {
			fmt % "(debug build)";
		} else {
			fmt % "";
		}
		m_clientSoft = "XBT Client";
		m_clientVersion = fmt.str();
	} else if (parse(peerId.data(), peerId.data() + peerId.size(),
		"-ML" >> anychar_p[push_back_a(ver)] >> '.'
		>> anychar_p[push_back_a(ver)] >> '.' >>
		anychar_p[push_back_a(ver)] >> *anychar_p
	).full) {
		m_clientSoft = "MLDonkey";
		boost::format fmt("v%s.%s.%s");
		m_clientVersion = (fmt % ver[0] % ver[1] % ver[2]).str();
	} else if (parse(peerId.data(), peerId.data() + peerId.size(),
		"exbc" >> *anychar_p
	).full) {
		m_clientSoft = "BitComet (old)";
	} else {
		m_clientSoft = "Unknown client: " + peerId;
	}
}

void Client::sendPing() {
	BEOStream tmp;
	Utils::putVal<uint32_t>(tmp, 0); // len = 0, no payload
	*m_socket << tmp.str();

	logTrace(TRACE,
		boost::format(COL_SEND "[%s] << PING" COL_NONE)
		% m_addr
	);
}

void Client::sendChoke() {
	if (m_amChoking) {
		return;
	}
	bool dontChoke(false);

	m_amChoking = true;
	m_torrent->tryUnchoke(this, &dontChoke);

	// we should continue uploading, retry in 30 seconds
	if (dontChoke) {
		Utils::timedCallback(
			boost::bind(&Client::sendChoke, this),
			27000 + (Utils::getRandom() % 7) * 1000
		);
		m_amChoking = false;
		return;
	}

	BEOStream tmp;
	Utils::putVal<uint32_t>(tmp, 1);  // len = 1
	Utils::putVal<uint8_t >(tmp, 0);  // id  = 0
	*m_socket << tmp.str();

	logTrace(TRACE,
		boost::format(COL_SEND "[%s] <= CHOKE" COL_NONE)
		% m_addr
	);
	m_amChoking = true;
	m_requests.clear();
	setUploading(false, m_file);
}

void Client::sendUnchoke() {
	if (!m_amChoking) {
		return;
	}

	BEOStream tmp;
	Utils::putVal<uint32_t>(tmp, 1);  // len = 1
	Utils::putVal<uint8_t >(tmp, 1);  // id  = 1
	*m_socket << tmp.str();

	logTrace(TRACE,
		boost::format(COL_SEND "[%s] <= UNCHOKE" COL_NONE)
		% m_addr
	);
	m_amChoking = false;
	Utils::timedCallback(
		boost::bind(&Client::sendChoke, this),
		27000 + (Utils::getRandom() % 7) * 1000
	);
}

void Client::sendInterested() {
	if (m_amInterested) {
		return;
	}

	BEOStream tmp;
	Utils::putVal<uint32_t>(tmp, 1);  // len = 1
	Utils::putVal<uint8_t >(tmp, 2);  // id  = 2
	*m_socket << tmp.str();

	logTrace(TRACE,
		boost::format(COL_SEND "[%s] <= INTERESTED" COL_NONE)
		% m_addr
	);
	m_amInterested = true;
}

void Client::sendUninterested() {
	if (!m_amInterested) {
		return;
	}

	BEOStream tmp;
	Utils::putVal<uint32_t>(tmp, 1);  // len = 1
	Utils::putVal<uint8_t >(tmp, 3);  // id  = 3
	*m_socket << tmp.str();

	logTrace(TRACE,
		boost::format(COL_SEND "[%s] <= UNINTERESTED" COL_NONE)
		% m_addr
	);
	m_amInterested = false;
}

void Client::sendHave(uint32_t index) {
	BEOStream tmp;
	Utils::putVal<uint32_t>(tmp, 5);     // len = 5
	Utils::putVal<uint8_t >(tmp, 4);     // id  = 4
	Utils::putVal<uint32_t>(tmp, index); // piece index
	*m_socket << tmp.str();

	logTrace(TRACE,
		boost::format(COL_SEND "[%s] <= HAVE index=%d" COL_NONE)
		% m_addr % index
	);
}

void Client::sendBitfield() {
	CHECK_THROW(m_torrent);
	// if we don't have anything yet, don't send this
	if (m_partData && !m_partData->getCompleted()) {
		return;
	}
	std::string bitField = m_torrent->getBitfield();

	BEOStream tmp;
	Utils::putVal<uint32_t>(tmp, bitField.size() + 1);   // len = 1 + X
	Utils::putVal<uint8_t >(tmp, 5);                     // id = 5
	Utils::putVal<std::string>(tmp, bitField, bitField.size());
	*m_socket << tmp.str();

	logTrace(TRACE,
		boost::format(COL_SEND "[%s] <= BITFIELD" COL_NONE)
		% m_addr
	);
}

void Client::sendRequest(uint32_t index, uint32_t offset, uint32_t length) {
	CHECK(m_bitField.at(index));
	CHECK(offset + length - 1 <= m_torrent->getChunkSize());

	BEOStream tmp;
	Utils::putVal<uint32_t>(tmp, 13);      // len = 13
	Utils::putVal<uint8_t >(tmp,  6);      // id  = 6
	Utils::putVal<uint32_t>(tmp, index);   // piece index
	Utils::putVal<uint32_t>(tmp, offset);  // relative offset to piece begin
	Utils::putVal<uint32_t>(tmp, length);  // chunk length
	*m_socket << tmp.str();

	logTrace(TRACE,
		boost::format(
			COL_SEND
			"[%s] <= REQUEST index=%d offset=%d length=%d"
			COL_NONE
		) % m_addr % index % offset % length
	);
}

void Client::sendPiece(uint32_t index,uint32_t offset,const std::string &data) {
	BEOStream tmp;
	Utils::putVal<uint32_t>(tmp, 9 + data.size());      // len = 9 + X
	Utils::putVal<uint8_t >(tmp, 7);                    // id  = 7
	Utils::putVal<uint32_t>(tmp, index);                // piece index
	Utils::putVal<uint32_t>(tmp, offset);               // relative offset
	Utils::putVal<std::string>(tmp, data, data.size()); // data
	*m_socket << tmp.str();

	logTrace(TRACE,
		boost::format(
			COL_SEND "[%s] <= PIECE index=%d offset=%d length=%d"
			COL_NONE
		) % m_addr % index % offset % data.size()
	);
}

void Client::sendCancel(uint32_t index, uint32_t offset, uint32_t length) {
	BEOStream tmp;
	Utils::putVal<uint32_t>(tmp, 13);      // len = 13
	Utils::putVal<uint8_t >(tmp,  8);      // id  =  8
	Utils::putVal<uint32_t>(tmp, index);   // piece index
	Utils::putVal<uint32_t>(tmp, offset);  // relative offset to piece begin
	Utils::putVal<uint32_t>(tmp, length);  // chunk length
	*m_socket << tmp.str();

	logTrace(TRACE,
		boost::format(
			COL_SEND
			"[%s] <= CANCEL index=%d offset=%d length=%d"
			COL_NONE
		) % m_addr % index % offset % length
	);
}

// note: in logfile, it appears as if we always send handshake first (even for
// incoming clients), even though BT protocol spec says to wait for handshake
// in incoming clients; this is not so, the log is wrong, and we wait for remote
// handshake before sending ours for incoming connections; it appears
// differently in log due to some internal code logic in this function.
void Client::parseBuffer() {
	if (m_inBuffer.size() < 4) {
		return;
	}
	if (!m_peerId.size() && m_inBuffer.size() >= 48) {
		using namespace boost::spirit;
		uint32_t strLen;
		std::string protName;
		std::string protFlags;
		std::string tmpHash;
		parse_info<> info = parse(
			m_inBuffer.data(), m_inBuffer.data()+ m_inBuffer.size(),
			anychar_p[assign_a(strLen)]
			>> repeat_p(boost::ref(strLen))[
				anychar_p[push_back_a(protName)]
			] >> repeat_p(8)[anychar_p[push_back_a(protFlags)]]
			>> repeat_p(20)[anychar_p[push_back_a(tmpHash)]]
			>> repeat_p(20)[anychar_p[push_back_a(m_peerId)]]
		);

		if (tmpHash.size() == 20) {
			m_infoHash = tmpHash;
			handshakeReceived(this);

			// handshakeReceived signal may result in this being
			// deleted, when the info_hash was unknown; return
			// quickly then.
			if (!m_torrent) {
				return;
			}

			if (!m_handshakeSent) {
				sendHandshake();
				m_handshakeSent = true;
			}
		}
		if (m_peerId.size()) {
			// get rid of non-printable characters
			for (uint32_t i = 0; i < m_peerId.size(); ++i) {
				if (m_peerId[i] < 32 || m_peerId[i] > 126) {
					m_peerId.replace(i, 1, ".");
				}
			}
			parsePeerId(m_peerId);
			logTrace(TRACE,
				boost::format(
					COL_RECV "[%s] => HANDSHAKE from %s %s"
					COL_NONE
				) % m_addr % getSoft() % getSoftVersion()
			);
			m_inBuffer.erase(0, 48 + strLen + 1);
		}
	}

	if (m_peerId.size()) {
		while (m_inBuffer.size() >= 4) {
			BEIStream tmp(m_inBuffer.substr(0, 4));
			uint32_t len = Utils::getVal<uint32_t>(tmp);
			if (len == 0) {
				onPing();
			} else {
				if (m_inBuffer.size() < len + 4) {
					break;
				}
				BEIStream packet(m_inBuffer.substr(4, len));
				try {
					parsePacket(packet, len);
				} catch (std::exception &e) {
					logDebug(
						boost::format("[%s] %s")
						% m_addr % e.what()
					);
					connectionLost(this);
					return;
				}
			}
			m_inBuffer.erase(0, len + 4);
		}
	}
}

void Client::parsePacket(BEIStream &i, uint32_t len) {
	switch (Utils::getVal<uint8_t>(i)) {
		case 0x00:
			onChoke();
			break;
		case 0x01:
			onUnchoke();
			break;
		case 0x02:
			onInterested();
			break;
		case 0x03:
			onUninterested();
			break;
		case 0x04:
			onHave(Utils::getVal<uint32_t>(i));
			break;
		case 0x05:
			onBitfield(Utils::getVal<std::string>(i, len - 1));
			break;
		case 0x06: {
			uint32_t index  = Utils::getVal<uint32_t>(i);
			uint32_t offset = Utils::getVal<uint32_t>(i);
			uint32_t length = Utils::getVal<uint32_t>(i);
			onRequest(index, offset, length);
			break;
		}
		case 0x07: {
			uint32_t index   = Utils::getVal<uint32_t>(i);
			uint32_t offset  = Utils::getVal<uint32_t>(i);
			std::string data = i.str().substr(9);
			onPiece(index, offset, data);
			break;
		}
		case 0x08: {
			uint32_t index  = Utils::getVal<uint32_t>(i);
			uint32_t offset = Utils::getVal<uint32_t>(i);
			uint32_t length = Utils::getVal<uint32_t>(i);
			onCancel(index, offset, length);
			break;
		}
		default:
			logTrace(TRACE,
				boost::format(
					"[%s] Received unknown packet: %s"
				) % m_addr % Utils::hexDump(i.str())
			);
			break;
	}
}

void Client::sendHandshake() {
	std::ostringstream tmp; // no endianess info needed in handshake packet
	Utils::putVal<uint8_t>(tmp, 19);
	Utils::putVal<std::string>(tmp, "BitTorrent protocol", 19);
	Utils::putVal<uint64_t>(tmp, 0); // 8 zero bytes
	Utils::putVal<std::string>(tmp, m_infoHash.toString(), 20);
	Utils::putVal<std::string>(tmp, BitTorrent::instance().getId(), 20);

	logTrace(TRACE,
		boost::format(COL_SEND "[%s] <= HANDSHAKE " COL_NONE) % m_addr
	);

	*m_socket << tmp.str();
	m_handshakeSent = true;
	sendBitfield();
}

void Client::onPing() {
	logTrace(TRACE,
		boost::format(COL_RECV "[%s] => PING" COL_NONE) % m_addr
	);
}

void Client::onChoke() {
	logTrace(TRACE,
		boost::format(COL_RECV "[%s] => CHOKE" COL_NONE) % m_addr
	);
	m_isChoking = true;
	setDownloading(false, m_partData);
	m_usedRange.reset();
	m_outRequests.clear();
}

void Client::onUnchoke() {
	logTrace(TRACE,
		boost::format(COL_RECV "[%s] => UNCHOKE" COL_NONE) % m_addr
	);
	m_isChoking = false;
	if (m_amInterested && m_partData) {
		setDownloading(true, m_partData);
		sendRequests();
	}
}

void Client::onInterested() {
	logTrace(TRACE,
		boost::format(COL_RECV "[%s] => INTERESTED" COL_NONE) % m_addr
	);
	m_isInterested = true;
}

void Client::onUninterested() {
	logTrace(TRACE,
		boost::format(COL_RECV "[%s] => UNINTERESTED" COL_NONE) % m_addr
	);
	m_isInterested = false;
}

void Client::onHave(uint32_t index) {
	CHECK_THROW(m_torrent);

	logTrace(TRACE,
		boost::format(COL_RECV "[%s] => HAVE %d" COL_NONE)
		% m_addr % index
	);
	if (m_bitField.size()) try {
		m_bitField.at(index) = true;
	} catch (std::out_of_range&) {
		logTrace(TRACE,
			boost::format("[%s] sent invalid HAVE message") % m_addr
		);
	}
	if (m_partData) {
		m_partData->addAvail(m_torrent->getChunkSize(), index);
		m_sourceMaskAdded = true;
		Range64 tmp(
			index * m_torrent->getChunkSize(),
			(index + 1) * m_torrent->getChunkSize() - 1
		);
		if (!m_amInterested && !m_partData->isComplete(tmp)) {
			checkNeedParts();
			if (m_needParts) {
				sendInterested();
			}
		}
		if (m_amInterested && !m_isChoking) {
			setDownloading(true, m_partData);
			sendRequests();
		}
	}
}

void Client::onBitfield(const std::string &bits) {
	CHECK_THROW(m_torrent);

	m_bitField.clear();
	uint32_t trueBits = 0;
	for (uint32_t i = 0; i < bits.size(); ++i) {
		std::bitset<8> b((unsigned)bits[i]);
		for (int8_t j = 7; j >= 0; --j) {
			m_bitField.push_back(b[j]);
			trueBits += b[j];
			if (m_bitField.size() == m_torrent->getChunkCnt()) {
				break;
			}
		}
		if (m_bitField.size() == m_torrent->getChunkCnt()) {
			break;
		}
	}
	CHECK_THROW(m_bitField.size() == m_torrent->getChunkCnt());

	logTrace(TRACE,
		boost::format(COL_RECV "[%s] => BITFIELD (%5.2f%%)" COL_NONE)
		% m_addr % (trueBits * 100.0 / m_bitField.size())
	);

	if (m_bitField.size() == trueBits) {
		// in Hydranode, empty bitfield indicates 'seed'
		// note that this is different from BT spec, where empty/missing
		// bitfield means the client has nothing at all
		m_bitField.clear();
	}

	if (m_partData) {
		m_partData->addSourceMask(m_torrent->getChunkSize(),m_bitField);
		m_sourceMaskAdded = true;
	}

	checkNeedParts();

	if (m_needParts) {
		sendInterested();
	} else {
		logTrace(TRACE,
			boost::format(
				"[%s] We don't need anything from this client."
			) % m_addr
		);
	}
}

void Client::onRequest(uint32_t index, uint32_t offset, uint32_t length) {
	CHECK_THROW(m_torrent);
	if (m_amChoking) { // ignore requests from choked clients
		logTrace(TRACE,
			boost::format(
				"[%s] Ignoring request (client is choked)"
			) % m_addr
		);
		return;
	}
	logTrace(TRACE,
		boost::format(
			COL_RECV "[%s] => REQUEST index=%d offset=%d length=%d"
			COL_NONE
		) % m_addr % index % offset % length
	);
	if (!isUploading()) {
		setUploading(true, m_file);
	}
	m_requests.push_back(Request(index, offset, length));
	if (m_socket->isWritable() && m_requests.size() == 1) {
		sendNextChunk();
	}
}

void Client::onPiece(uint32_t index, uint32_t offset, const std::string &data) {
	if (!m_outRequests.size()) {
		sendUninterested();
		return;
	}
	CHECK_THROW(m_torrent);

	logTrace(TRACE,
		boost::format(
			COL_RECV "[%s] => PIECE index=%d offset=%d length=%d"
			COL_NONE
		) % m_addr % index % offset % data.size()
	);

	Request r = m_outRequests.front();
	m_torrent->delRequest(r);
	m_outRequests.pop_front();
	if (Request(index, offset, data.size()) == r) try {
		r.m_locked->write(r.m_locked->begin(), data);
	} catch (std::exception &e) {
		logDebug(
			boost::format("[%s] Writing data: %s") % m_addr
			% e.what()
		);
	} else try {
		uint64_t beg = index * m_torrent->getChunkSize() + offset;
		m_partData->write(beg, data);
	} catch (std::exception &e) {
		(void)e;
		logTrace(TRACE,
			boost::format("[%s] Ignoring %d bytes duplicate data.")
			% m_addr % data.size()
		);
	}
	sendRequests();
	m_torrent->addDownloaded(data.size());
}

void Client::onCancel(uint32_t index, uint32_t offset, uint32_t length) {
	logTrace(TRACE,
		boost::format(
			COL_RECV "[%s] => CANCEL index=%d offset=%d length=%d"
			COL_NONE
		) % m_addr % index % offset % length
	);
	std::list<Request>::iterator it = m_requests.begin();
	Request toCancel(index, offset, length);
	while (it != m_requests.end()) {
		if (*it == toCancel) {
			logTrace(TRACE,
				boost::format("[%s] Canceling request.")
				% m_addr
			);
			m_requests.erase(it);
			it = m_requests.begin();
		} else {
			++it;
		}
	}
}

void Client::sendNextChunk() try {
	CHECK_RET(m_requests.size());
	CHECK_THROW(m_torrent);
	CHECK_THROW(m_file);

	Request r = m_requests.front();
	uint64_t begin = m_torrent->getChunkSize() * r.m_index + r.m_offset;
	std::string data(m_file->read(begin, begin + r.m_length - 1));
	assert(data.size() == r.m_length);
	sendPiece(r.m_index, r.m_offset, data);
	m_requests.pop_front();

	updateSignals();
	m_torrent->addUploaded(data.size());
	TorrentFile *file = dynamic_cast<TorrentFile*>(m_file);
	if (file) try {
		SharedFile *curFile = file->getContains(
			Range64(begin, data.size())
		);
		curFile->addUploaded(data.size());
	} catch (...) {}

} catch (SharedFile::ReadError &e) {
	if (e.reason() == SharedFile::ETRY_AGAIN_LATER) {
		Utils::timedCallback(
			boost::bind(&Client::sendNextChunk, this), 3000
		);
	} else {
		logError(
			boost::format("[%s] Sending next chunk: %s") 
			% m_addr % e.what()
		);
	}
} catch (std::exception &e) {
	logError(
		boost::format("[%s] Sending next chunk: %s") 
		% m_addr % e.what()
	);
} MSVC_ONLY(;)

void Client::setTorrent(Torrent *t) {
	assert(!m_torrent || m_torrent == t);

	m_torrent = t;
	m_infoHash = t->getInfoHash();
}

void Client::setFile(SharedFile *file) {
	assert(!m_file || m_file == file);

	m_file = file;
}

void Client::setFile(PartData *file) {
	assert(!m_partData || m_partData == file);

	m_partData = file;
	if (m_partData) {
		m_partData->onVerified.connect(
			boost::bind(&Client::onVerified, this, _1, _2, _3)
		);
		m_partData->onDestroyed.connect(
			boost::bind(&Client::onDestroyed, this, _1)
		);
	}
}

void Client::checkNeedParts() {
	m_needParts = false;
	if (m_partData) {
		CHECK_THROW(m_torrent);
		::Detail::UsedRangePtr ret;
		ret = m_partData->getRange(
			m_torrent->getChunkSize(), m_bitField
		);
		if (ret) {
			m_needParts = true;
		} else {
			logDebug(
				boost::format(
					"[%s] Client has no needed chunks."
				) % m_addr
			);
		}
	}
}

void Client::onVerified(PartData *file, uint32_t chunkSize, uint32_t chunk) {
	CHECK_RET(file == m_partData);
	CHECK_THROW(m_torrent);

	if (!m_socket->isConnected()) {
		return;
	}

	if (chunkSize == m_torrent->getChunkSize()) {
		sendHave(chunk);
	}

	uint32_t removed = 0;
	std::deque<Request>::iterator i(m_outRequests.begin());
	while (i != m_outRequests.end()) {
		if ((*i).m_index == chunk) {
			sendCancel((*i).m_index, (*i).m_offset, (*i).m_length);
			m_outRequests.erase(i);
			i = m_outRequests.begin();
			++removed;
		} else {
			++i;
		}
	}
	if (removed) {
		logTrace(TRACE,
			boost::format("[%s] Canceled %d requests from queue.")
			% m_addr % removed
		);
	}
	if (m_amInterested && !m_isChoking) {
		sendRequests();
	}
}

/**
 * When adding multiple requests in one go, we want to add the requests to
 * the Torrent object in FILO ordering, AFTER all requests have been generated.
 * Hence, this object temporarly stores the requests made, and upon destruction,
 * submits them to the specified Torrent object (in FILO ordering).
 */
struct RequestAdder : public std::stack<Client::Request> {
	RequestAdder(Torrent *t) : m_t(t) {}
	~RequestAdder() {
		while (size()) {
			m_t->addRequest(top()); pop();
		}
	}
	Torrent *m_t;
};

void Client::sendRequests() try {
	CHECK_THROW(m_partData);
	CHECK_THROW(m_torrent);

	boost::scoped_ptr<RequestAdder> toAdd(new RequestAdder(m_torrent));
	while (m_outRequests.size() < 5 && m_partData) {
		if (!m_usedRange) {
			m_usedRange = m_partData->getRange(
				m_torrent->getChunkSize(), m_bitField
			);
		}
		if (!m_usedRange) {
			Request r(m_torrent->getRequest(m_bitField));
			std::deque<Request>::iterator it(m_outRequests.begin());
			while (it != m_outRequests.end()) {
				CHECK_THROW(*it != r);
				++it;
			}
			sendRequest(r.m_index, r.m_offset, r.m_length);
			m_outRequests.push_back(r);
		}
		if (m_usedRange) {
			::Detail::LockedRangePtr l(m_usedRange->getLock(16384));
			if (!l) {
				m_usedRange.reset();
				continue;
			}
			Request r(l, m_torrent);
			sendRequest(r.m_index, r.m_offset, r.m_length);
			m_outRequests.push_back(r);
			if (m_socket->getDownSpeed() < 1024) {
				toAdd->push(r);
			} else {
				logTrace(TRACE,
					boost::format("[%s] <- Fast source.")
					% m_addr
				);
			}
		}
	}
	updateSignals();
} catch (...) {
	if (!m_outRequests.size()) {
		sendUninterested();
	} else {
		updateSignals();
	}
}
MSVC_ONLY(;)

uint32_t Client::getUploadSpeed() const {
	return m_socket->getUpSpeed();
}
uint32_t Client::getDownloadSpeed() const {
	return m_socket->getDownSpeed();
}
uint64_t Client::getSessionUploaded() const {
	return m_socket->getUploaded();
}
uint64_t Client::getSessionDownloaded() const {
	return m_socket->getDownloaded();
}
uint64_t Client::getTotalUploaded() const {
	return m_socket->getUploaded();
}
uint64_t Client::getTotalDownloaded() const {
	return m_socket->getDownloaded();
}

void Client::updateSignals() try {
	CHECK_THROW(m_socket);
	CHECK_THROW(m_file);

	TorrentFile *file = dynamic_cast<TorrentFile*>(m_file);
	if (file && m_requests.size()) {
		m_currentUploadMeter.disconnect();
		Request r = m_requests.front();
		uint64_t cz = m_torrent->getChunkSize();
		uint64_t begin = cz * r.m_index + r.m_offset;
		uint64_t end   = begin + r.m_length - 1;
		SharedFile *curFile = file->getContains(Range64(begin, end));
		m_currentUploadMeter = curFile->getUpSpeed.connect(
			boost::bind(&TcpSocket::getUpSpeed, m_socket.get())
		);
	} else if (!m_currentUploadMeter.connected()) {
		m_currentUploadMeter = m_file->getUpSpeed.connect(
			boost::bind(&TcpSocket::getUpSpeed, m_socket.get())
		);
	}

	if (!m_partData) { return; }

	PartialTorrent *tmp = dynamic_cast<PartialTorrent*>(m_partData);
	if (tmp && m_outRequests.size() && m_outRequests.front().m_locked) {
		m_currentSpeedMeter.disconnect();
		PartData *curFile = tmp->getContains(
			*m_outRequests.front().m_locked
		);
		m_currentSpeedMeter = curFile->getDownSpeed.connect(
			boost::bind(&TcpSocket::getDownSpeed, m_socket.get())
		);
	} else if (!m_currentSpeedMeter.connected()) {
		m_currentSpeedMeter = m_partData->getDownSpeed.connect(
			boost::bind(&TcpSocket::getDownSpeed, m_socket.get())
		);
	}
} catch (std::exception &) {
	// probably downloading to cache - connect to top-level file then
	if (m_partData) {
		m_currentSpeedMeter.disconnect();
		m_currentSpeedMeter = m_partData->getDownSpeed.connect(
			boost::bind(&TcpSocket::getDownSpeed, m_socket.get())
		);
	}
	if (m_file) {
		m_currentUploadMeter.disconnect();
		m_currentUploadMeter = m_file->getUpSpeed.connect(
			boost::bind(&TcpSocket::getUpSpeed, m_socket.get())
		);
	}
}
MSVC_ONLY(;)

void Client::onDestroyed(PartData *file) {
	if (m_partData && file == m_partData) {
		m_outRequests.clear();
		m_usedRange.reset();
		m_partData = 0;
	}
}

} // end Bt namespace
