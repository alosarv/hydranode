/*
 *  Copyright (C) 2005-2006 Gaubatz Patrick <patrick@gaubatz.at>
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

#include <hncore/http/connection.h>
#include <hncore/http/client.h>
#include <hnbase/log.h>
#include <boost/regex.hpp>
#include <hnbase/timed_callback.h>
#include <boost/algorithm/string/find.hpp>
#include <hncore/metadata.h>


namespace Http {
namespace Detail {

const std::string TRACE = "http.connection";


Connection::Connection(ParsedUrl url, Detail::FilePtr file) try
:
	BaseClient(&HttpClient::instance()), m_file(file), m_url(url),
	m_socket(0), m_parser(0), m_used(), m_locked(),	m_addr(),
	m_checked(false)
{
	logTrace(TRACE, boost::format("new Connection(%p)") % this);
	if (m_file->getPartData()) {
		BaseClient::setSource(m_file->getPartData());
	}

	m_socket.reset(new HttpSocket(this, &Connection::onSocketEvent));
	m_parser.reset(new Parser);
	CHECK_THROW(m_socket);
	CHECK_THROW(m_parser);

	m_parser->onEvent.connect(
		boost::bind(&Connection::onParserEvent, this, _b1, _b2)
	);
	m_parser->sendData.connect(
		boost::bind(&Connection::sendData, this, _b1, _b2)
	);
	m_parser->writeData.connect(
		boost::bind(&Connection::writeData, this, _b1, _b2, _b3)
	);

	// check if url.getHost() is an IP address or a hostname:
	boost::regex reg("^(\\d+)(\\.\\d+){3}$");

	if (HttpClient::instance().useProxy()) {
		setAddr(HttpClient::instance().getProxy());
		connect();

	} else if (boost::regex_match(url.getHost(), reg)) {
		// immediatley connect, as we already have the IP:
		IPV4Address addr(url.getHost(), url.getPort());
		setAddr(addr);
		connect();

	} else {
		// we need to resolve the hostname first:
		hostLookup();
	}
} catch (std::exception &e) {
	LOG_EXCEPTION(e);
} MSVC_ONLY(;)


Connection::~Connection() {
	logTrace(TRACE, boost::format("~Connection(%p)") % this);
	if (m_speeder.connected()) {
		m_speeder.disconnect();
	}
}


void Connection::setAddr(std::vector<IPV4Address> addr) {
	CHECK_THROW(addr.size());
	m_addr = addr;
	m_curAddr = m_addr.begin();
	for (AddrIter it = m_curAddr; it != m_addr.end(); ++it) {
		it->setPort(m_url.getPort());
		logTrace(TRACE,
			boost::format("setAddr(): got address: %s")
			% it->getStr()
		);
	}
}


void Connection::setAddr(IPV4Address addr) {
	setAddr(AddrVec(addr));
}


void Connection::tryReconnect() {
	logTrace(TRACE, "tryReconnect()");
	if (m_file->getPartData() && (m_file->getPartData()->isComplete() || !m_file->getPartData()->isRunning())) {
		return logTrace(TRACE,
			"tryReconnect(): PartData is complete or paused!"
		);
	}
	if (!m_socket->isConnected()) {
		connect();
	} else {
		CHECK_RET_MSG(0, "Should not reach here!");
	}
}


void Connection::tryNextAddr() {
	++m_curAddr;

	//restart from the beginning of the list...
	if (m_curAddr == m_addr.end()) {
		m_curAddr = m_addr.begin();
	}
}


void Connection::initSpeeder() {
	if (!m_file->getPartData()) {
		return;
	}
	m_speeder.disconnect();
	m_speeder = m_file->getPartData()->getDownSpeed.connect(
		boost::bind(&HttpSocket::getDownSpeed, m_socket.get())
	);
}


void Connection::connect() try {
	if (m_socket->isConnecting()) {
		logError("Socket is already connecting.");
		return;

	} else if (m_socket->isConnected()) {
		logError("Socket is already connected.");
		reset();
		Utils::timedCallback(
			boost::bind(&Connection::connect, this), 1000
		);
		return;
	}

	m_socket->connect(*m_curAddr);
	logTrace(TRACE, "connect() to: " + m_curAddr->getStr());

} catch (std::exception &e) {
	LOG_EXCEPTION(e);
} MSVC_ONLY(;)


void Connection::reset() {
	logTrace(TRACE, "reset()");
	resetUsed();
	resetLocked();
	m_parser->reset();
	m_socket->disconnect();
	m_socket.reset(new HttpSocket(this, &Connection::onSocketEvent));
	initSpeeder();
	setConnected(false);
	if (m_file->getPartData()) {
		setDownloading(false, m_file->getPartData());
	}
}


void Connection::onSocketEvent(HttpSocket *s, SocketEvent evt) try {
	s->setTimeout(30 * 1000); // 30 seconds

	if (evt == SOCK_READ) {
		// all incoming data has to be parsed by a Parser:
		m_parser->parse(s->getData());

	} else if (evt == SOCK_CONNECTED) {
		setConnected(true);
		doGet();
		onConnected(shared_from_this());

	} else if (evt == SOCK_LOST) {
		if (m_file->getPartData()) {
			setDownloading(false, m_file->getPartData());
		}
		setConnected(false);
		reset();
		onLost(shared_from_this());

	} else if (evt == SOCK_TIMEOUT || evt == SOCK_CONNFAILED) {
		uint32_t retry = 60; //XXX: maybe a config option would be nice
		logMsg(
			boost::format(
				COL_GREEN "Connection to " COL_MAGENTA "%i"
				COL_GREEN " timed out or failed, retrying in "
				COL_MAGENTA "%i seconds" COL_GREEN "..."
				COL_NONE
			) % (*m_curAddr).getStr() % retry
		);
		setConnected(false);
		reset();
		tryNextAddr();
		Utils::timedCallback(
			boost::bind(&Connection::connect, this), 1000 * retry
		);
		onFailure(shared_from_this());

	} else if (evt != SOCK_WRITE) {
		logTrace(TRACE,
			boost::format("Got an unhandled SocketEvent: %i") % evt
		);
	}
} catch (std::exception &e) {
	LOG_EXCEPTION(e);
} MSVC_ONLY(;)


bool Connection::requestChunk() {
	std::vector<std::string> noRanges;
	std::vector<std::string>::iterator it;

	noRanges.push_back("rapidshare.de");
	noRanges.push_back("heise.de");

	for (it = noRanges.begin(); it != noRanges.end(); ++it) {
		if (boost::algorithm::find_first(m_url.getHost(), (*it))) {
			return false;
		}
	}

	return true;
}


void Connection::doGet() try {
	logTrace(TRACE, "doGet()");
	m_parser->setRequestUrl(HttpClient::instance().useProxy());

	if (!m_checked) {
		m_parser->getInfo(m_url);

	} else if (requestChunk() && m_file->getSize() > 512 * 1024) try {
		//if the filesize is bigger than 512k
		//chunk the file up...
		//try to get chunks of 10mb
		uint32_t reqSize = 10 * 1024 * 1024;

		getLocks(reqSize);
		CHECK_RET(m_locked);
		Range64 reqRange(m_locked->begin(), m_locked->end());
		CHECK_RET(reqRange.length() > 1);
		m_parser->getChunk(m_url, reqRange);

	} catch (std::exception &e) {
		logTrace(TRACE,
			boost::format(
				"doGet() failed: \"%s\". Retry in 15 seconds..."
			) % e.what()
		);
		Utils::timedCallback(
			boost::bind(&Connection::doGet, this), 15000
		);

	} else {
		if (m_file->getPartData()) {
			if (m_file->getPartData()->getCompleted() > 0) {
				logTrace(TRACE,
					"PartData already got some data "
					"from the file, setting everything "
					"to corrupted state and re-get all..."
				);
				resetUsed();
				resetLocked();
				m_file->getPartData()->corruption(
					Range64(0, m_file->getSize() - 1)
				);
				Utils::timedCallback(
					boost::bind(&Connection::doGet, this),
					500
				);
				return;
			}
			getLocks(m_file->getSize());
		}
		m_parser->getFile(m_url);
	}

} catch (std::exception &e) {
	LOG_EXCEPTION(e);
} MSVC_ONLY(;)


void Connection::onResolverEvent(HostInfo info) try {
	if (info.error()) {
		logError(
			boost::format("[%s] Host lookup failed: %s")
			% info.getName() % info.errorMsg()
		);
		Utils::timedCallback(this, &Connection::hostLookup, 60000);
	} else {
		setAddr(info.getAddresses());
		connect();
	}
} catch (std::exception &e) {
	LOG_EXCEPTION(e);
} MSVC_ONLY(;)


void Connection::sendData(Parser *p, const std::string &data) {
	CHECK_RET(m_socket->isConnected());
	m_socket->write(data);
}


void Connection::writeData(
	Parser *p, const std::string &data, uint64_t offset
) try {
	if (m_locked) {
		m_file->write(data, offset, m_locked);
	} else {
		m_file->write(data, offset);
	}

} catch (std::exception &e) {
	LOG_EXCEPTION(e);
} MSVC_ONLY(;)


void Connection::getLocks(uint32_t chunksize) {
	if (!chunksize) { 
		return; //silently ignore...
	}
	logTrace(TRACE,
		"getLocks(): chunksize=" + Utils::bytesToString(chunksize)
	);	
	CHECK_THROW(m_file->getPartData());	

	if (m_used && !m_locked) {
		m_locked = m_used->getLock(chunksize);
		if (!m_locked) {
			resetUsed();
			return getLocks(chunksize);
		} else {
			std::string tmp = Utils::bytesToString(
				m_locked->end() - m_locked->begin()
			);
			logTrace(TRACE,
				boost::format(
					"getLocks(): got LockedRange: "
					"[%i-%i] (chunksize=%i)"
				) % m_locked->begin() % m_locked->end() % tmp
			);
		}
	} else if (!m_used) {
		m_used = m_file->getPartData()->getRange(chunksize);
		if (m_used) {
			std::string tmp = Utils::bytesToString(
				m_used->end() - m_used->begin()
			);
			logTrace(TRACE,
				boost::format(
					"getLocks(): got UsedRange: "
					"[%i-%i] (chunksize=%i)"
				) % m_used->begin() % m_used->end() % tmp
			);
			return getLocks(chunksize);
		} else {
			resetUsed();
			resetLocked();
			throw std::runtime_error("Failed to get range");
		}
	} else {
		CHECK_RET_MSG(0, "Should not reach here.");
	}
}


void Connection::hostLookup() {
	DNS::lookup(m_url.getHost(), this, &Connection::onResolverEvent);
}


void Connection::onParserEvent(Parser *p, ParserEvent evt) {
	// simply redirect to the Download-object:
	onParser(shared_from_this(), p, evt);
}


std::string Connection::getSoft() const {
	CHECK_THROW(m_parser);
	std::string tmp = m_parser->getHeader("server");
	return tmp.empty() ? "unknown" : tmp;
}


uint64_t Connection::getSessionDownloaded() const {
	return m_socket ? m_socket->getDownloaded() : 0;
}


uint64_t Connection::getTotalDownloaded() const {
	return m_socket ? m_socket->getDownloaded() : 0;
}


uint32_t Connection::getDownloadSpeed() const {
	return m_socket ? m_socket->getDownSpeed() : 0;
}

} // End namespace Detail
} // End namespace Http
