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

#ifndef __ED2K_KAD_H__
#define __ED2K_KAD_H__

#include <hncore/modules.h>
#include <hnbase/sockets.h>
#include <hncore/kademlia.h>
#include <hncore/search.h>
#include <hncore/ed2k/parser.h>
#include <hncore/ed2k_kad/kademlia.h>
#include <hncore/ed2k_kad/packets.h>

#include <boost/noncopyable.hpp>

#include <functional>

namespace ED2KKad {
	/**
	 * UDP Listener for ED2KKad
	 */
	class Listener
	: public boost::noncopyable {
	public:
		//! Private constructor FIXME FIXME
		Listener();

		//! Private destructor
		~Listener() { };

		//! Getters
		//@{
		uint16_t getPort() const { return m_port; }
		//@}

		//! Routing table
		typedef Kad::RoutingTable<
			Kad::KBucket<Contact>,
			boost::function<void (void)>
		> RTable;

		RTable  m_route;

		//! Ourself contact
		Contact m_myself;

		//! Timed callback
		void timedCallback();

		//! Instance
		static Listener &instance();

		//! @name Packet handlers
		//@{
		void onPacket(const Donkey::ED2KPacket::KadBootstrapRequest&);
		void onPacket(const Donkey::ED2KPacket::KadBootstrapResponse&);

		void onPacket(const Donkey::ED2KPacket::KadHelloRequest&);
		void onPacket(const Donkey::ED2KPacket::KadHelloResponse&);

		void onPacket(const Donkey::ED2KPacket::KadFirewalledRequest&);
		void onPacket(const Donkey::ED2KPacket::KadFirewalledResponse&);

		void onPacket(const Donkey::ED2KPacket::KadRequest&);
		void onPacket(const Donkey::ED2KPacket::KadResponse&);
		//@}

		//! Handler for searches
		void onSearch(boost::shared_ptr<Search>);

	public: // FIXME
		//! Instance
		static Listener *s_instance;

		//! Handler for socket events
		void onSocketEvent(UDPSocket*, SocketEvent evt);

		//! Send packet
		void sendData(const Contact& to, const std::string& data);

		//! Kademlia RPC request
		void RPC();

		//! Listener port
		uint16_t                        m_port;

		//! Listening socket
		UDPSocket                       *m_socket;

		typedef Donkey::ED2KParser<
			Listener, Donkey::ED2KNetProtocolUDP
		> Parser;

		//! Packet parser
		Parser                          *m_parser;

		//! Current received address source
		IPV4Address                     m_srcAddr;

		//! Send out a packet
		template<typename Packet>
		void sendPacketTo(const Packet& packet, const IPV4Address& to) {
			std::stringstream ss;

			// UDP packet header
			Utils::putVal<uint8_t>(ss, Donkey::PR_KADEMLIA);
			Utils::putVal<uint8_t>(ss, Packet::OPCODE);

			// Data
			ss << (std::string)packet;

			m_socket->send(ss.str(), to);
		}

	private:
		//Listener(const Listener &);
	};

	/**
	 * ED2KKad module
	 */
	class Module : public ModuleBase {
		DECLARE_MODULE(Module, "ed2k_kad");
	public:
		virtual bool onInit();
		virtual int onExit();

		virtual std::string getDesc() {
			return "eDonkey2000 Kademlia Module.";
		}

	private:
		//! Generates new userhash
		std::string createUserHash() const;
	};
}

#endif
