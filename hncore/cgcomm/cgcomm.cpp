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

#include <hncore/cgcomm/cgcomm.h>
#include <hncore/cgcomm/client.h>
#include <hnbase/prefs.h>
#include <hnbase/log.h>
#include <hnbase/lambda_placeholders.h>

namespace CGComm {

IMPLEMENT_MODULE(ModMain);

bool ModMain::onInit() {
	logMsg("Initializing Core/GUI communication...");
	m_listener = new SocketServer();
	std::string listenIp(
		Prefs::instance().read<std::string>(
			"/cgcomm/ListenIp", "127.0.0.1"
		)
	);
	Prefs::instance().write("/cgcomm/ListenIp", listenIp);
	int port = 9990;
	while (port > 9980) {
		try {
			m_listener->listen(IPV4Address(listenIp, port));
			break;
		} catch (std::exception &e) {
			logError(
				boost::format(
					"Unable to start CGComm listener: %s"
				) % e.what()
			);
			--port;
		}
	}
	if (!m_listener->isListening()) {
		logError("Fatal: Cannot start CGComm module.");
		return false;
	}

	m_listener->setHandler(
		boost::bind(&ModMain::onIncoming, this, _1, _2)
	);
	logMsg(
		boost::format("Core/GUI Communication listening on port %d") 
		% port
	);
	return true;
}

int ModMain::onExit() {
	m_listener->destroy();
	return 0;
}

void ModMain::onIncoming(SocketServer *sock, SocketEvent) {
	Client *c = new Client(sock->accept());
	c->getEventTable().addHandler(c, this, &ModMain::onClientEvent);
	m_clients.insert(c);
}

void ModMain::onClientEvent(Client *c, int evt) {
	if (evt == EVT_DESTROY && m_clients.erase(c)) {
		delete c;
	}
}

} // namespace CGComm

