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
 * @file hnsh.cpp Hydranode Shell Module
 */

#include <hncore/hnsh/hnsh.h>
#include <hncore/hnsh/shellclient.h>
#include <hnbase/log.h>
#include <hnbase/prefs.h>

namespace Shell {

// HNShell class implementation
// ----------------------------
IMPLEMENT_MODULE(HNShell);

// Called from main app, perform module-dependant initialization here
bool HNShell::onInit() {
	m_server = new SocketServer;
	m_server->setHandler(
		boost::bind(&HNShell::serverEventHandler, this, _1, _2)
	);

	migrateSettings();

	Prefs::instance().setPath("/hnsh");
	std::string listenIp = Prefs::instance().read<std::string>(
		"ListenIp", "127.0.0.1"
	);
	uint32_t port = Prefs::instance().read<uint32_t>("ListenPort", 9999);

	// write back before they get modified
	Prefs::instance().write<std::string>("ListenIp", listenIp);
	Prefs::instance().write<uint32_t>("ListenPort", port);

	uint32_t limit = port - 10;
	while (port > limit) {
		try {
			if (listenIp == "*") {
				m_server->listen(IPV4Address(0, port));
			} else {
				m_server->listen(IPV4Address(listenIp, port));
			}
			break;
		} catch (SocketError &e) {
			logWarning(
				boost::format(
					"Error starting shell listener on "
					"%s:%d: %s"
				) % listenIp % port % e.what()
			);
			--port;
		}
	}
	if (!m_server->isListening()) {
		// Bail out
		logError("Unable to start shell listener.");
		m_server->destroy();
		m_server = 0;
		return false;
	}
	logMsg(
		boost::format(
			COL_BGREEN "Hydranode Shell Server " COL_NONE
			"online at " COL_BCYAN "%s" COL_NONE  ":"
			COL_BCYAN "%d" COL_NONE
		) % listenIp % port
	);
	ShellClient::getEventTable().addAllHandler(this, &HNShell::onShellEvt);

	// Shells networking priority must be high for best response times
	setPriority(PR_HIGH);

	// register trace masks
	Log::instance().addTraceMask("shell.client");

	return true;
}

// called from main app, perform module-dependant cleanup here
int HNShell::onExit() {
	logMsg("Hydranode Shell Server exiting.");
	while (m_clients.size()) {
		removeClient(*m_clients.begin());
	}
	m_server->destroy();
	m_server = 0;
	return 0;
}

// Remove a client from clients list.
void HNShell::removeClient(ShellClient *c) {
	std::set<ShellClient*>::iterator i = m_clients.find(c);
	if (i == m_clients.end()) {
		logDebug(
			"HNSHell: Attempt to remove a client "
			"which is not listed."
		);
		return;
	}
	delete *i;
	m_clients.erase(i);
}

// Handles events on m_server member
void HNShell::serverEventHandler(SocketServer *, SocketEvent evt) {
	logTrace("shell.client",
		boost::format("Shell(%p): Client connected.")
		% this
	);
	switch (evt) {
		case SOCK_ACCEPT: try {
			ShellClient *c = new ShellClient(m_server->accept());
			m_clients.insert(c);
			break;
		} catch (std::exception &e) {
			logError(
				boost::format(
					"Accepting incoming shellclient: %s"
				) % e.what()
			);
			break;
		}
		default:
			break;
	}
}

void HNShell::onShellEvt(ShellClient *c, ShellClientEvent evt) {
	if (evt == EVT_DESTROY) {
		m_clients.erase(c);
		delete c;
	}
}

void HNShell::migrateSettings() {
	std::string oldListenIp = Prefs::instance().read<std::string>(
		"/HNShell Listen IP", "127.0.0.1"
	);
	uint16_t oldPort = Prefs::instance().read<uint32_t>("/HNShell Port", 9999);

	std::string newListenIp = Prefs::instance().read<std::string>(
		"/hnsh/ListenIp", ""
	);
	uint16_t newPort = Prefs::instance().read<uint16_t>("/hnsh/ListenPort", 0);
	bool migrated = false;
	if (oldListenIp.size() && !newListenIp.size()) {
		Prefs::instance().write<std::string>("/hnsh/ListenIp", oldListenIp);
		Prefs::instance().erase("/HNShell Listen IP");
		migrated = true;
	}
	if (oldPort && !newPort) {
		Prefs::instance().write<uint16_t>("/hnsh/ListenPort", oldPort);
		Prefs::instance().erase("/HNShell Port");
		migrated = true;
	}
	if (migrated) {
		logMsg(
			"Info: HNShell configuration options "
			"have moved to [hnsh] section."
		);
	}
}

} // namespace Shell
