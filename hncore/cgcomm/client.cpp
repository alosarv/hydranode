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

#include <hncore/cgcomm/client.h>
#include <hncore/cgcomm/subsysbase.h>
#include <hncore/cgcomm/sub_auth.h>
#include <hncore/cgcomm/sub_config.h>
#include <hncore/cgcomm/sub_download.h>
#include <hncore/cgcomm/sub_ext.h>
#include <hncore/cgcomm/sub_hasher.h>
#include <hncore/cgcomm/sub_log.h>
#include <hncore/cgcomm/sub_modules.h>
#include <hncore/cgcomm/sub_search.h>
#include <hncore/cgcomm/sub_shared.h>
#include <hncore/cgcomm/sub_network.h>
#include <hncore/cgcomm/opcodes.h>
#include <hncore/hydranode.h>

namespace CGComm {

IMPLEMENT_EVENT_TABLE(Client, Client*, int);

Client::Client(SocketClient *sock) : m_socket(sock) {
	boost::function<void (const std::string&)> sendFunc;
	sendFunc = boost::bind(&Client::sendData, this, _1);

	m_subMap[SUB_AUTH    ].reset(new Subsystem::Auth    (sendFunc));
	m_subMap[SUB_SEARCH  ].reset(new Subsystem::Search  (sendFunc));
	m_subMap[SUB_DOWNLOAD].reset(new Subsystem::Download(sendFunc));
	m_subMap[SUB_SHARED  ].reset(new Subsystem::Shared  (sendFunc));
	m_subMap[SUB_CONFIG  ].reset(new Subsystem::Config  (sendFunc));
	m_subMap[SUB_HASHER  ].reset(new Subsystem::Hasher  (sendFunc));
	m_subMap[SUB_MODULES ].reset(new Subsystem::Modules (sendFunc));
	m_subMap[SUB_LOG     ].reset(new Subsystem::Log     (sendFunc));
	m_subMap[SUB_EXT     ].reset(new Subsystem::Ext     (sendFunc));
	m_subMap[SUB_NETWORK ].reset(new Subsystem::Network (sendFunc));

	m_socket->setHandler(
		boost::bind(&Client::onSocketEvent, this, _1, _2)
	);

	logMsg(
		boost::format("CGComm> UI connected from %s")
		% m_socket->getPeer()
	);
}

void Client::parse(const std::string &data) {
	m_inBuf.append(data);
	while (m_inBuf.size() >= 6) {
		std::istringstream tmp(m_inBuf);
		uint8_t subsys = Utils::getVal<uint8_t>(tmp);
		uint32_t size = Utils::getVal<uint32_t>(tmp);
		if (subsys == 0x00 && size == 1) {
			// subsys 0, opcode 1 -> SHUTDOWN
			if (Utils::getVal<uint8_t>(tmp) == 0x01) {
				logMsg("CGComm> Received shutdown command.");
				Hydranode::instance().exit();
			}
			m_inBuf.erase(0, 6);
			continue;
		}
		Iter it = m_subMap.find(subsys);
		if (it == m_subMap.end()) {
			logWarning(
				boost::format("Unknown subsystem %s")
				% Utils::hexDump(subsys)
			);
			m_inBuf.erase(0, size + 5);
			continue;
		}

		if (m_inBuf.size() < size + 5u) {
			return;
		}

		std::istringstream packet(m_inBuf.substr(5, size));
		try {
			(*it).second->handle(packet);
		} catch (std::runtime_error &e) {
			logError(
				boost::format("CGComm:%s: %s")
				% (*it).first % e.what()
			);
		}
		m_inBuf.erase(0, size + 5);
	}
}

void Client::onSocketEvent(SocketClient *sock, SocketEvent evt) {
	if (evt == SOCK_READ) {
		std::string buf;
		*sock >> buf;
		return parse(buf);
	} else if (evt == SOCK_WRITE && m_outBuf.size()) try {
		uint32_t r = m_socket->write(m_outBuf.data(), m_outBuf.size());
		if (r < m_outBuf.size()) {
			m_outBuf.erase(0, r);
		} else {
			m_outBuf.clear();
		}
	} catch (std::exception &e) {
		logDebug(boost::format("CGComm: Sending data: %s") % e.what());
		destroy();
	} else if (evt == SOCK_LOST || evt == SOCK_TIMEOUT || evt == SOCK_ERR) {
		destroy();
	}
}

void Client::destroy() {
	getEventTable().postEvent(this, EVT_DESTROY);
	m_socket->destroy();
}

void Client::sendData(const std::string &data) {
	if (m_outBuf.size()) {
		m_outBuf.append(data);
	} else try {
		uint32_t ret = m_socket->write(data.data(), data.size());
		if (ret < data.size()) {
			m_outBuf.append(data.substr(ret));
		}
	} catch (std::exception &e) {
		logDebug(boost::format("CGComm: Sending data: %s") % e.what());
		destroy();
	}
}

} // namespace CGComm

