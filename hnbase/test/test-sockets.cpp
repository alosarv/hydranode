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
 * \file test-sockets.cpp Test app for Networking Subsystem
 *
 * This is a regress-testing application for HydraNode Networking Subsystem. It
 * creates a number of listening servers and a number of threads connecting to
 * those servers at random times, sending and receiving data over loopback
 * connection, simulating "real world" clients.
 *
 * \todo Test large-bandwidth transfer's too (e.g. 10+mb transfers)
 */

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#include <hnbase/log.h>
#include <hnbase/sockets.h>
#include <map>
#include <vector>
static const int THREADCOUNT = 1;
static const int SERVCOUNT   = 5;

/**
 * TestSockets class performs Sockets API testing
 */
class TestSockets {
public:
	//! App entry point
	int onRun();
	//! @name Event handlers
	//{@
	void onServSockEvent(SocketServer *server, SocketEvent evt);
	void onClieSockEvent(SocketClient *client, SocketEvent evt);
	//@}
private:
	//! Servers list
	std::vector<SocketServer*> m_servers;
	//! Clients list
	std::list<SocketClient*> m_clients;
};

/* TestSockets IMPLEMENTATION */
int TestSockets::onRun() {
	Log::instance().enableTraceMask(TRACE_SOCKET, "Sockets");
	logMsg("Initializing Sockets Test Sequence.");
	logMsg(
		boost::format("-> Creating %d listener servers on ports "
		"2000-%d") % SERVCOUNT % (SERVCOUNT+2000)
	);

	if (!SocketWatcher::initialize()) {
		return false; // Socket initialization failed
	}

	/**
	 * Construct and place four servers into listening states on the ports.
	 * Set event handlers for each of those servers and place them in
	 * vector.
	 */
	for (int i = 0; i < SERVCOUNT; i++) {
		SocketServer *serv = new SocketServer(
			boost::bind(&TestSockets::onServSockEvent, this, _1, _2)
		);
		IPV4Address addr(0, 2000+i);
		serv->listen(addr);
		m_servers.push_back(serv);
	}

	// Enter application main loop
	EventMain::instance().mainLoop();

	// We return here when application is exiting

	// Handle any leftover events
	EventMain::instance().process();

	// Return to system
	return 0;
}

void TestSockets::onServSockEvent(SocketServer *server, SocketEvent evt) {
	logMsg(boost::format("TestSockets::OnServSockEvent(), evt=%d") % evt);
	switch (evt) {
		case SOCK_ACCEPT: {
			char msg[] = "Hello from server!";
			SocketClient *client = server->accept(
				boost::bind(
					&TestSockets::onClieSockEvent,
					this, _1, _2
				)
			);
			client->write(msg, 18);
			m_clients.push_back(client);
			logMsg("[TestSockets] Incoming connection accepted.");
			break;
		}
  		default:
			logWarning(
				boost::format("Unhandled event type %d") % evt
			);
			break;
	}
}

void TestSockets::onClieSockEvent(SocketClient *client, SocketEvent evt) {
	logMsg(boost::format("TestSockets::OnClieSockEvent(), evt=%d") % evt);
	switch (evt) {
		case SOCK_CONNECTED:
			logMsg(
				"[TestSockets] Outgoing connection "
				"established."
			);
			break;
		case SOCK_READ: {
			char buf[1024];
			int count = client->read(&buf, 1024);
			buf[count] = '\0';
			logMsg(
				boost::format("[TestSockets] Read: %d bytes. "
				"Data: '%s'") % count % buf
			);
			break;
		}
		case SOCK_LOST: {
			logMsg("[TestSockets] Client lost.");
			m_clients.remove(client);
			client->destroy();
			break;
		}
		default:
			logWarning(
				boost::format("Unhandled event type %d") %
				evt
			);
			break;
	}
}

int main() {
	TestSockets ts;
	ts.onRun();
}

#endif
