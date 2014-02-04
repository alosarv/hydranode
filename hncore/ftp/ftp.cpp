/*
 *  Copyright (C) 2004-2006 Alo Sarv <madcat_@users.sourceforge.net>
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

/**
 * \file ftp.cpp Implementation of Ftp module
 */

#include <hncore/ftp/ftp.h>
#include <hnbase/sockets.h>
#include <hnbase/ssocket.h>
#include <boost/tokenizer.hpp>

/**
 * \namespace Ftp Ftp module is contained in Ftp namespace
 */
namespace Ftp {

// FtpModule class
// ---------------
IMPLEMENT_MODULE(FtpModule);

bool FtpModule::onInit() {
	Operation opConn("connect", true);
	opConn.addArg(Operation::Argument("url", true, ODT_STRING));
	opConn.addArg(Operation::Argument("username", false, ODT_STRING));
	opConn.addArg(Operation::Argument("password", false, ODT_STRING));
	addOperation(opConn);
	Operation opDisc("disconnect", false);
	opDisc.addArg(Operation::Argument("session", false, ODT_STRING));
	addOperation(opDisc);

	return true;
}

int FtpModule::onExit() {
	return 0;
}

void FtpModule::addOperation(const Operation &op) {
	m_ops.push_back(op);
}

uint8_t FtpModule::getOperCount() const {
	return m_ops.size();
}

Object::Operation FtpModule::getOper(uint8_t n) const {
	return m_ops.at(n);
}

void FtpModule::doOper(const Operation &op) {
	if (op.getName() == "connect") try {
		std::string url, username, password;
		for (uint32_t i = 0; i < op.getArgCount(); ++i) {
			if (op.getArg(i).getName() == "url") {
				url = op.getArg(i).getValue();
			} else if (op.getArg(i).getName() == "username") {
				username = op.getArg(i).getValue();
			} else if (op.getArg(i).getName() == "password") {
				password = op.getArg(i).getValue();
			}
		}
		CHECK_THROW(url.size());
		Session *sess = 0;
		if (username.size() || password.size()) {
			sess = new Session(url, username, password);
		} else {
			sess = new Session(url);
		}
		m_sessions.insert(std::make_pair(sess->getName(), sess));
	} catch (std::exception &e) {
		logError(
			boost::format("FtpSession creation failed: %s")
			% e.what()
		);
	} else if (op.getName() == "disconnect") {
		if (op.getArgCount() > 1) {
			SIter it = m_sessions.find(op.getArg(1).getValue());
			if (it == m_sessions.end()) {
				logError("No such session");
			} else {
				Session *s = (*it).second;
				m_sessions.erase(it);
				delete s;
			}
		} else {
			while (m_sessions.size()) {
				Session *s = (*m_sessions.begin()).second;
				m_sessions.erase(m_sessions.begin());
				delete s;
			}
		}
	}
}

// Session class
// -------------
Session::Session(
	const std::string &url,
	const std::string &username, const std::string &password
) : Object(&FtpModule::instance(), "unnamed"), m_username(username),
m_password(password), m_cSocket() {
	CHECK_THROW(url.size() > 10);
	CHECK_THROW(url.substr(0, 6) == "ftp://");
	size_t i = url.find_first_of('/', 6);
	CHECK_THROW(i != std::string::npos);
	m_serverName = url.substr(6, i - 6);
	m_serverPath = url.substr(i);
	setName(m_serverName);

	logMsg("FtpSession started:");
	logMsg(
		boost::format("URL: %s ServerName: %s ServerPath: %s")
		% url % m_serverName % m_serverPath
	);

	connect();
}

void Session::connect() {
	m_cSocket = new FtpSocket(this, &Session::onSocketEvent);
	m_cSocket->connect(IPV4Address(m_serverName, 21));
}

void Session::onSocketEvent(FtpSocket *sock, SocketEvent evt) {
	if (evt == SOCK_CONNECTED) {
		logDebug("[controlsocket] Connection established");
		init();
	} else if (evt == SOCK_READ) {
		std::string data = sock->getData();
		m_buffer.append(data);
		while (parseBuffer());
	}
}

void Session::onDataEvent(FtpSocket *sock, SocketEvent evt) {
	if (evt == SOCK_READ) {
		logDebug(boost::format("Data:\n%s") % sock->getData());
	} else if (evt == SOCK_CONNECTED) {
		logDebug("DataSocket connected.");
	}
}

void Session::init() {
	*m_cSocket << "user " << m_username << Socket::Endl;
	*m_cSocket << "pass " << m_password << Socket::Endl;
	*m_cSocket << "type i" << Socket::Endl;
	*m_cSocket << "cwd " << m_serverPath << Socket::Endl;
	reqData("list");
}

bool Session::parseBuffer() {
	size_t i = m_buffer.find_first_of('\n');
	if (i == std::string::npos) {
		return false;
	}
	std::string line(m_buffer.substr(0, i));
	m_buffer.erase(0, i + 1);
	if (line.at(line.size() - 1) == '\r') {
		line.erase(line.size() - 1, 1);
	}
	if (line.size() < 3) {
		return true;
	}

	logDebug(boost::format("[controlsocket] %s") % line);

	uint32_t opCode = boost::lexical_cast<uint32_t>(line.substr(0, 3));
	bool ignore = line.at(4) == '-';
	if (ignore) {
		return true;
	}
	switch (opCode) {
		case 227: // Entering Passive Mode (xxx,xxx,xxx,xxx,yyy,yyy)
			getPassiveAddr(line);
			break;
		case 150: // Connection accepted
			break;
		case 226: // Transfer OK
//			if (m_dSocket) {
//				SchedBase::instance().handleEvents();
//				onDataEvent(m_dSocket, SOCK_READ);
//				delete m_dSocket;
//				m_dSocket = 0;
//			}
			break;
		default:
			break;
	}
	return true;
}

void Session::reqData(const std::string &msg) {
	CHECK_RET(!m_pendingReq.size());
	m_pendingReq = msg;
	*m_cSocket << "pasv" << Socket::Endl;
	*m_cSocket << msg << Socket::Endl;
}

void Session::getPassiveAddr(const std::string &line) {
	typedef boost::char_separator<char> Separator;
	typedef boost::tokenizer<Separator> Tokenizer;

	size_t beg = line.find_first_of('(');
	CHECK_RET(beg != std::string::npos);
	size_t end = line.find_first_of(')');
	CHECK_RET(end != std::string::npos);

	Separator sep(",");
	Tokenizer tok(line.substr(beg + 1, (end - beg) - 1), sep);
	uint32_t cnt = 0;
	std::string ip;
	uint16_t port = 0;
	for (Tokenizer::iterator it = tok.begin(); it != tok.end(); ++it) {
		if (cnt < 4) {
			ip += *it;
		}
		if (cnt < 3) {
			ip += ".";
		}
		if (cnt == 4) {
			port = boost::lexical_cast<uint16_t>(*it);
			port <<= 8;
		} else if (cnt == 5) {
			port += boost::lexical_cast<uint16_t>(*it);
		}
		++cnt;
	}
	m_dataAddr = IPV4Address(ip, port);

	if (!m_dSocket && m_pendingReq.size()) {
		m_dSocket = new FtpSocket(this, &Session::onDataEvent);
		m_dSocket->connect(m_dataAddr);
		*m_cSocket << m_pendingReq << Socket::Endl;
		m_pendingReq = "";
	}
}

} // namespace Ftp

