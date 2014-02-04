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
 * \file mailnotify.cpp Implementation of email notifier plugin
 */

#include <hncore/mailnotify/mailnotify.h>
#include <hnbase/log.h>
#include <hnbase/prefs.h>
#include <hncore/fwd.h>
#include <hncore/partdata.h>
#include <hncore/sharedfile.h>
#include <hnbase/timed_callback.h>
#include <boost/lambda/core.hpp>
#include <boost/lambda/construct.hpp>
#include <boost/lambda/bind.hpp>

namespace MNotify {

IMPLEMENT_MODULE(MailNotify);

bool MailNotify::onInit() {
	m_socket = 0;
	migrateSettings();

	Prefs::instance().setPath("/mailnotify");
	m_mailServer = Prefs::instance().read<std::string>("Server", "");
	m_mailFrom = Prefs::instance().read<std::string>("From", "");
	m_mailTo = Prefs::instance().read<std::string>("To", "");

	SharedFile::getEventTable().addAllHandler(this, &MailNotify::onSFEvent);
	return true;
}

int MailNotify::onExit() {
	if (m_socket) {
		delete m_socket;
	}
	return 0;
}

void MailNotify::migrateSettings() {
	std::string oldMailServer, oldMailFrom, oldMailTo;
	Prefs::instance().setPath("/MailNotify");
	oldMailServer = Prefs::instance().read<std::string>("Server", "");
	oldMailFrom = Prefs::instance().read<std::string>("From", "");
	oldMailTo = Prefs::instance().read<std::string>("To", "");
	std::string newMailServer, newMailFrom, newMailTo;
	Prefs::instance().setPath("/mailnotify");
	newMailServer = Prefs::instance().read<std::string>("Server", "");
	newMailFrom = Prefs::instance().read<std::string>("From", "");
	newMailTo = Prefs::instance().read<std::string>("To", "");
	if (oldMailServer.size() && !newMailServer.size()) {
		Prefs::instance().write("Server", oldMailServer);
		Prefs::instance().erase("/MailNotify/Server");
	}
	if (oldMailFrom.size() && !newMailFrom.size()) {
		Prefs::instance().write("From", oldMailFrom);
		Prefs::instance().erase("/MailNotify/From");
	}
	if (oldMailTo.size() && !newMailTo.size()) {
		Prefs::instance().write("To", oldMailTo);
		Prefs::instance().erase("/MailNotify/To");
	}
}

void MailNotify::onSFEvent(SharedFile *sf, int evt) {
	if (evt != SF_DL_COMPLETE || !m_mailServer.size()) {
		return;
	}

	if (!m_socket) {
		m_socket = new MailSocket(this, &MailNotify::onSocketEvent);
	}

	if (!m_socket->isConnected() && !m_socket->isConnecting()) {
		m_pendingMessages.push_back(sf);
		try {
			m_socket->connect(IPV4Address(m_mailServer, 25));
		} catch (std::exception &e) {
			logDebug(
				boost::format("MailNotify error: %s") 
				% e.what()
			);
		}
	} else {
		sendNotify(sf);
	}
}

void MailNotify::onSocketEvent(MailSocket *sock, SocketEvent evt) {
	using namespace boost::lambda;

	if (sock != m_socket) {
		return;
	}

	if (evt == SOCK_CONNECTED) {
		while (m_pendingMessages.size()) {
			sendNotify(m_pendingMessages.back());
			m_pendingMessages.pop_back();
		}
	} else if (evt == SOCK_WRITE) {
		if (!m_pendingMessages.size()) {
			m_socket = 0;
			Utils::timedCallback(bind(delete_ptr(), sock), 30000);
		} else while (m_pendingMessages.size()) {
			sendNotify(m_pendingMessages.back());
			m_pendingMessages.pop_back();
		}
	} else if (evt == SOCK_READ) {
		logDebug("[MailNotify] Server said: ");
		logDebug(sock->getData());
	} else switch (evt) {
		case SOCK_LOST:
		case SOCK_CONNFAILED:
		case SOCK_ERR:
		case SOCK_TIMEOUT:
			logError("MailNotify: Socket error.");
			break;
		default: break;
	}
}

void MailNotify::sendNotify(SharedFile *sf) {
	CHECK_THROW(m_socket);
	CHECK_THROW(m_socket->isConnected());
	std::string from = m_mailFrom.substr(m_mailFrom.find_last_of('@') + 1);

	*m_socket << "ehlo " << from << Socket::Endl;
	*m_socket << "mail from: " << m_mailFrom << Socket::Endl;
	*m_socket << "rcpt to: " << m_mailTo << Socket::Endl;
	*m_socket << "data" << Socket::Endl;

	*m_socket << "From: Hydranode Mailer <";
	*m_socket << m_mailFrom << ">" << Socket::Endl;

	*m_socket << "Subject: [Hydranode] completed download: ";
	*m_socket << sf->getName() << Socket::Endl;
	*m_socket << "To: " << m_mailTo << Socket::Endl;

	*m_socket << "Hydranode has completed the following download: ";
	*m_socket << Socket::Endl;
	*m_socket << sf->getName() << Socket::Endl << Socket::Endl;
	*m_socket << "The file has been moved to incoming directory at: ";
	*m_socket << Socket::Endl;
	*m_socket << sf->getLocation() << Socket::Endl;
	*m_socket << "." << Socket::Endl;

	logMsg(
		boost::format("[MailNotify] e-mail notification sent to %s")
		% m_mailTo
	);
}

}

