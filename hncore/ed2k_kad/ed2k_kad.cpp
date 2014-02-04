/*
 *  Copyright (C) 2005 Andrea Leofreddi <andrea.leofreddi@libero.it>
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
 * \file ed2k_kad.cpp Implementation of ED2K Kad Module entry point
 */

#include <hnbase/timed_callback.h>
#include <hnbase/hash.h>
#include <hnbase/md4transform.h>
#include <hncore/kademlia.h>
#include <hncore/ed2k/opcodes.h>
#include <hncore/ed2k_kad/ed2k_kad.h>
#include <hncore/ed2k_kad/opcodes.h>
#include <hncore/ed2k_kad/packets.h>

// Convenience macro
#define DECLARE_PACKET(Packet) \
 DECLARE_PACKET_FACTORY(PR_KADEMLIA, Packet, ED2KPacket::Packet::OPCODE); \
 DECLARE_PACKET_HANDLER3( \
  ED2KKad::Listener, ED2KKad::Listener, ED2KNetProtocolUDP, Packet \
 )

using namespace Donkey;

// Declare Kad packet factories
DECLARE_PACKET(KadBootstrapRequest);
DECLARE_PACKET(KadBootstrapResponse);

DECLARE_PACKET(KadHelloRequest);
DECLARE_PACKET(KadHelloResponse);

DECLARE_PACKET(KadFirewalledRequest);
DECLARE_PACKET(KadFirewalledResponse);

DECLARE_PACKET(KadRequest);
DECLARE_PACKET(KadResponse);

namespace ED2KKad {

IMPLEMENT_MODULE(Module);

static const std::string TRACE_LISTENER("ed2k_kad.listener");

bool Module::onInit() {
	logMsg("Starting ED2KKad...");

	// Register trace masks
	Log::instance().addTraceMask("ed2k.parser");
	Log::instance().addTraceMask("ed2k_kad.listener");

	// Force instantiation of listener
	Listener &listener = Listener::instance();

	// Add search handler
	Search::addQueryHandler(
		boost::bind(&Listener::onSearch, &Listener::instance(), _1)
	);

	// Open nodes.dat
	std::ifstream f("nodes.dat");

	CHECK_THROW_MSG(f, "Unable to load nodes.dat");

	uint32_t numEntries = Utils::getVal<uint32_t>(f);

	logMsg(boost::format("%i in nodes.dat") % numEntries);

	while(numEntries--) {
		Contact c = Utils::getVal<Contact>(f);

		logTrace(TRACE_LISTENER,
			boost::format("Parsed from nodes.dat: %s") % c.getStr()
		);

		listener.m_route.add(c);
	}

	//logDebug("Ok, RoutingZone populated, issuing a dump:\n");
	//Listener::instance().m_route.dump();

	Utils::timedCallback(boost::bind(
		&Listener::timedCallback, &listener
	), 1000);

	return true;
}

int Module::onExit() {
	return 0;
}

Listener* Listener::s_instance;

void Listener::onPacket(const ED2KPacket::KadBootstrapRequest& packet) {
	logTrace(TRACE_LISTENER,
		boost::format("KadBootstrapRequest received from %s")
		% m_srcAddr.getStr()
	);

	ED2KPacket::KadBootstrapResponse response;

	std::vector<ED2KKad::Contact> contacts;
	contacts.push_back(m_route.m_self);
	response.setContacts(contacts);

	sendPacketTo(response, m_srcAddr);
}

void Listener::onPacket(const ED2KPacket::KadBootstrapResponse& packet) {
	logTrace(TRACE_LISTENER,
		boost::format("KadBootstrapResponse received from %s")
		% m_srcAddr.getStr()
	);

	const std::vector<ED2KKad::Contact>& contacts = packet.getContacts();

	std::vector<ED2KKad::Contact>::const_iterator iter = contacts.begin();

	// Add parsed contacts to our contact tree
	while(iter != contacts.end()) {
		m_route.add(*iter++);
	}
}

void Listener::onPacket(const ED2KPacket::KadHelloRequest& packet) {
	logTrace(TRACE_LISTENER,
		boost::format("KadHelloRequest received from %s")
		% m_srcAddr.getStr()
	);

	ED2KPacket::KadHelloResponse response(m_route.m_self);

	sendPacketTo(response, m_srcAddr);
}

void Listener::onPacket(const ED2KPacket::KadHelloResponse& packet) {
	logTrace(TRACE_LISTENER,
		boost::format("KadHelloResponse received from %s")
		% m_srcAddr.getStr()
	);
}

void Listener::onPacket(const ED2KPacket::KadFirewalledRequest& packet) {
	logTrace(TRACE_LISTENER,
		boost::format("KadFirewalledRequest received from %s")
		% m_srcAddr.getStr()
	);

	ED2KPacket::KadFirewalledResponse response(m_srcAddr);

	sendPacketTo(response, m_srcAddr);
}

void Listener::onPacket(const ED2KPacket::KadFirewalledResponse& packet) {
	logTrace(TRACE_LISTENER,
		boost::format("KadFirewalledResponse received from %s")
		% m_srcAddr.getStr()
	);
	logTrace(TRACE_LISTENER,
		boost::format("KadFirewalledResponse: peer sees us as %s")
		% packet.getAddr().getStr()
	);
}

void Listener::onPacket(const ED2KPacket::KadRequest& packet) {
	logTrace(TRACE_LISTENER,
		boost::format("KadRequest received from %s")
		% m_srcAddr.getStr()
	);

	logTrace(TRACE_LISTENER,
		boost::format("Request type %i, for %s, from source %s")
		% (int)packet.m_type
		% Kad::KUtils::bitsetHexDump(packet.m_a)
		% Kad::KUtils::bitsetHexDump(packet.m_b)
	);

	ED2KPacket::KadResponse response;

	response.setTarget(packet.m_a);

	std::vector<ED2KKad::Contact> contacts;
	contacts.push_back(m_route.m_self);
	response.setContacts(contacts);

	sendPacketTo(response, m_srcAddr);
}

void Listener::onPacket(const ED2KPacket::KadResponse& packet) {
	logTrace(TRACE_LISTENER,
		boost::format("KadResponse received from %s")
		% m_srcAddr.getStr()
	);
}

Listener &Listener::instance() {
	if(!s_instance) {
		s_instance = new Listener;
	}

	return *s_instance;
}

Listener::Listener()
: m_route(boost::function<void (void)>(boost::bind(&Listener::RPC, this))),
m_socket(0), m_parser(new Parser(this))
{
	unsigned startPort = 4672; // lowest port
	unsigned retries   = 2048; // number of tries

	// (1) Setup listening udp socket
	m_socket = new UDPSocket;

	while(retries--) {
		logTrace(TRACE_LISTENER,
			boost::format("ED2KKad: trying to bind port %i")
			% startPort
		);
		try {
			m_socket->listen(IPV4Address(0, startPort++));

			break;
		} catch(SocketError) {
			/* do nothing */
		}
	}

	CHECK_THROW_MSG(retries, "Unable to bind ED2KKad socket.");

	UDPSocket::getEventTable().addHandler(
		m_socket, this, &Listener::onSocketEvent
	);

	logMsg(
		boost::format("ED2KKad is listening on port %i")
		% (startPort - 1)
	);

	m_port = startPort - 1;

	// (2) Creates a random id
	m_myself.m_id = Kad::KUtils::randomBitset(
		Kad::KUtils::TypeToType<Contact::IdType>()
	);
	m_myself.m_type = 1;
	//m_myself.m_addr.setAddr(0);
	//m_myself.m_addr.setPort(4672);
	m_myself.m_addr = IPV4Address("10.10.0.1", 4672);
	m_myself.m_tcpPort = 4665;

	m_route.m_self = m_myself;

	logMsg(
		boost::format("ED2KKad is using random id %s")
		% Kad::KUtils::bitsetHexDump(m_myself.m_id)
	);
}

void Listener::RPC() {
}

void Listener::timedCallback() {
#if 0
	logDebug("Time event on Listener\n");

	Contact &e = m_route.getRandom();

	std::stringstream ss;

	Utils::putVal<uint8_t>(ss, 0x10); // KADEMLIA_HELLO_REQ

	Utils::putVal<Contact>(ss, m_myself);

	/*
	RTable::iterator itor = m_route.begin();

	for(; itor != m_route.end(); ++itor) {
		Kad::KBucket<Contact>::iterator jtor = itor->second.begin();

		for(; jtor != itor->second.end(); ++jtor) {
			sendData(*jtor, ss.str());
		}
	}
	*/

	// Reschedule this timed callback
	//Utils::timedCallback(boost::bind(
	//&ED2KKadListener::timedCallback, *this
	//), 1000);
#endif
}

void Listener::onSocketEvent(UDPSocket *socket, SocketEvent evt) {
	IPV4Address& from = m_srcAddr;

	if(evt == SOCK_READ) {
		char buf[4096];
		int ret = socket->recv(buf, sizeof(buf), &from);

		logTrace(TRACE_LISTENER, boost::format(
			"ED2KKad::onSocketEvent: received "
			"data: %d bytes from %s"
			) % ret % from
		);

		std::stringstream ss;
		ss.write(buf, ret);

		try {
			m_parser->parse(ss.str());
		} catch(std::runtime_error &err) {
			logMsg(boost::format(
				"Listener::onSocketEvent: error: %s"
			) % err.what());
		}
	}
}

void Listener::sendData(const Contact &to, const std::string &data) {
	std::stringstream ss;

	Utils::putVal<uint8_t>(ss, PR_KADEMLIA); // Emule kademlia header
	ss << data;

	logTrace(TRACE_LISTENER, boost::format("Sending to %s")
		% to.m_addr.getStr()
	);
	m_socket->send(ss.str(), to.m_addr);
}

std::string getStringHash(const std::string &s) {
	Md4Transform ht;
	ht.sumUp(s.c_str(), s.length());

	return Hash<MD4Hash>(ht.getHash()).toString();
}

Contact::IdType getHash(const std::string &s) {
	std::string h = getStringHash(s);

	const char *p = h.data();
	Contact::IdType id;

	for(unsigned j, i(0); i < 16; ++i) {
		for(j = 0; j < 8; ++j) {
			id[i * 8 + j] = p[i] & (1 << j);
		}
	}

	return id;
}

void Listener::onSearch(boost::shared_ptr<Search> search) {
	for(unsigned i = 0; i < search->getTermCount(); ++i) {
		logMsg(
			boost::format("Got search term '%s'")
			% search->getTerm(i)
		);
	}

	Contact::IdType sid = getHash(search->getTerm(0));

	std::list<Contact> l = m_route.findClosestToId(sid);

	std::list<Contact>::iterator itor;

	for(itor = l.begin(); itor != l.end(); ++itor) {
		logTrace(TRACE_LISTENER,
			boost::format("Sending search request to contact %s")
			% itor->getStr()
		);

		ED2KPacket::KadRequest request;

		request.m_type = ED2KPacket::KadRequest::FIND_VALUE;

		request.m_a = sid;
		request.m_b = itor->m_id;

		// Send search requests
		sendPacketTo(request, itor->m_addr);
	}
}

}
