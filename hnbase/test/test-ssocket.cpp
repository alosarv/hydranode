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

#include <hnbase/ssocket.h>
#include <hnbase/sockets.h>
#include <hnbase/log.h>

class ED2K {
public:
	static ED2K &instance() {
		static ED2K s_instance;

		return s_instance;
	}

	void addSocket() { };
	void delSocket() { };
	static uint8_t getPriority() { return 0; }

	void addUploaded(uint32_t amount) { }
	void addDownloaded(uint32_t amount) { }

	boost::signal<uint32_t(), Utils::Sum<uint32_t> > getDownSpeed;
	boost::signal<uint32_t(), Utils::Sum<uint32_t> > getUpSpeed;
};

typedef SSocket<ED2K, Socket::Server> ED2KServer;
typedef SSocket<ED2K, Socket::Client> ED2KClient;

void onClientEvent(ED2KClient *c, SocketEvent evt) {
	switch (evt) {
		case SOCK_READ: {
			std::string buf;
			c->read(&buf);
			logMsg(
				boost::format("Received data from client: %s")
				% buf
			);
			c->write("Got your data. Bye.\r\n");
			c->disconnect();
			delete c;
			break;
		}
		case SOCK_WRITE:
			logDebug("Client socket became writable.");
			break;
		case SOCK_CONNECTED:
			logDebug("Client socket connection established.");
			break;
		case SOCK_LOST:
			logDebug("Client socket connection was lost.");
			break;
		case SOCK_ERR:
			logDebug("Client socket became erronous.");
			break;
		default:
			logWarning("Unknown client socket event.");
			break;
	}
}

void onServerEvent(ED2KServer *s, SocketEvent evt) {
	if (evt == SOCK_ACCEPT) {
		ED2KClient *c = s->accept();
		logMsg("Incoming connection accepted. Sending welcome message.");
		c->setHandler(&onClientEvent);
		c->write("Welcome.\r\n");
	} else {
		logWarning("Unknown server socket event.");
	}
}

int main() {
	Log::instance().enableTraceMask(TRACE_SOCKET, "Socket");
	ED2KServer *s = new ED2KServer();
	s->listen(IPV4Address(0, 2000));
	s->setHandler(&onServerEvent);
	EventMain::initialize();
	SchedBase::instance();
	// test socket destruction while connect() is in progress
	{
		ED2KClient c;

		c.connect(IPV4Address("129.241.210.221", 4242));
	}

	// test socket destruction while connect() is in progress (during loop)
	boost::scoped_ptr<ED2KClient> c(new ED2KClient);
	c->connect(IPV4Address("129.241.210.221", 4242));

	while(true) {
		EventMain::instance().process();
		if (c) {
			c.reset();
		}
	}
}

