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

#ifndef __ED2K_PARSER_H__
#define __ED2K_PARSER_H__

/**
 * \file parser.h
 * Interface for ED2KParser class which performs ED2K network stream parsing.
 *
 * This is the main header for ED2KParser system, and should be included by
 * user code. The support headers, "packets.h" and "opcodes.h" should generally
 * not be included by user code.
 */

#include <hncore/ed2k/opcodes.h>         // Protocol opcodes
#include <hncore/ed2k/packets.h>         // Packet objects header
#include <hncore/ed2k/zutils.h>          // for decompress
#include <hnbase/log.h>                  // For debug/trace logging
#include <hnbase/utils.h>                // for Utils::getVal

namespace Donkey {

/**
 * ED2KNetProtocolTCP can be used as second template parameter of ED2KParser. It
 * allows ED2KParser to parse correctly TCP packets.
 */
struct ED2KNetProtocolTCP {
	enum {
		HEADER_LENGTH = 5 // PROTO, LEN
	};

	//! Parse header of a TCP Packet from a stream
	template<class Stream>
	static void parseHeader(
		Stream&   i,
		uint8_t&  proto,
		uint32_t& length,
		uint8_t&  opcode
	) {
		proto = Utils::getVal<uint8_t>(i);
		length = Utils::getVal<uint32_t>(i);
		opcode = Utils::getVal<uint8_t>(i);
	}

private:
	ED2KNetProtocolTCP();
};

/**
 * ED2KNetProtocolUDP can be used as second template parameter of ED2KParser. It
 * allows ED2KParser to parse correctly UDP packets.
 */
struct ED2KNetProtocolUDP {
	enum {
		HEADER_LENGTH = 1 // PROTO
	};

	/**
	 * Parse header of an UDP Packet from a stream
	 */
	template<class Stream>
	static void parseHeader(
		Stream&   i,
		uint8_t&  proto,
		uint32_t& length,
		uint8_t&  opcode
	) {
		proto = Utils::getVal<uint8_t>(i);
		opcode = Utils::getVal<uint8_t>(i);
		length = i.str().size() - HEADER_LENGTH;
	}

private:
	ED2KNetProtocolUDP();
};

/**
 * ED2KParser template class provides a generic interface for parsing ED2K
 * network stream. The input data is sent to the parser object through parse()
 * member functions, which performs the data parsing. When a packet has been
 * detected in stream, ED2KParser first locates the correct packet-factory
 * to handle the packet, and passes the packet data to the factory. The
 * specific packet factories in turn pass the data to the actual packet object
 * which then performs the final packet parsing and packet object construction.
 * Once the packet has been constructed, the packet-factory in question calls
 * back to the parser client (specified through Parent template argument and
 * parent argument in ED2KParser constructor), calling onPacket() member
 * function and passing the newly created packet object (by reference) to the
 * function. This allows client code to implement overloaded versions of
 * onPacket to perform event-driven packet handling.
 *
 * @param Parent         Parent class which will receive packet events
 */
template<typename Parent, typename NetProtocol = ED2KNetProtocolTCP>
class ED2KParser  {
public:
	/**
	 * Declare a type for the protocol.
	 */
	typedef NetProtocol NetProtocolType;

	/**
	 * The one and only constructor, this initializes the packet parser
	 * to parse a stream.
	 *
	 * @param parent   Pointer to object to which notifications should be
	 *                 sent. Must not be null.
	 */
	ED2KParser(Parent *parent) : m_parent(parent), m_need() {
		CHECK_THROW(parent);
	}

	/**
	 * @name Accessors and modifiers for internal data
	 */
	//@{
	void    setParent(Parent *p) { CHECK_THROW(p); m_parent = p; }
	Parent* getParent()    const { return m_parent;              }
	bool    hasBuffered()  const { return m_buffer.size();       }
	void    clearBuffer()        { m_buffer.clear();             }
	//@}

	/**
	 * Continue stream parsing, passing additional data. The data is
	 * buffered internally, so @param data may be freed after passing
	 * to this method. Note that this function triggers a chain-reaction
	 * of events when a new packet is detected, which leads back to
	 * client code, into the relevant packet handler function. When this
	 * function returns, all found packets in stream have been parsed, and
	 * all remaining data has been buffered for next parsing sequence.
	 *
	 * @param data     Data buffer to be parsed.
	 */
	void parse(const std::string &data) {
		m_buffer += data;
		if(
			m_buffer.size() < NetProtocolType::HEADER_LENGTH ||
			m_buffer.size() < m_need
		) {
			return; // not enough data yet
		}
		uint32_t lastPacket = 0;
		std::istringstream i(m_buffer);
		while (i.good()) {
			lastPacket = i.tellg();
			try {
				if (!readPacket(i, m_packet)) {
					break;
				}
			} catch (Utils::ReadError&) {
				break;
			}

			// Locate the right factory for this packet.
			Iter iter = factories()[m_packet.m_proto].find(
				m_packet.m_opcode
			);
			if (iter == factories()[m_packet.m_proto].end()) {
				logTrace("ed2k.parser",
					boost::format(
						COL_GREEN "Received unknown "
						"packet: %s" COL_NONE
					) % m_packet
				);
				continue;
			}
			uint32_t curPos = i.tellg();
			if (curPos == m_buffer.size()) {
				m_buffer.clear();
			}

			// found the handler
			std::istringstream packet(m_packet.m_data);
			(*iter).second->create(m_parent, packet);
		}
		m_buffer = i.str().substr(lastPacket);
	}

	/**
	 * PacketFactory is an abstract base class for specific packet
	 * factories, which handle specific packet construction and user
	 * callbacks after packet construction. This class is implemented as
	 * public here for specific derived factories to be able to access it.
	 * It should not be used by user code.
	 *
	 * Specific factory must pass it's supported opcode (based on which
	 * ED2KParser chooses that factory to pass data to) to the base class's
	 * constructor. Specific factory must also override pure virtual
	 * create() method to perform the packet construction.
	 */
	class PacketFactory {
	public:
		/**
		 * Creates a packet and calls back to packet handler.
		 *
		 * @param parent        Pointer to packet handler object
		 * @param i             Stream containing packet data
		 * @param proto         Protocol used for this packet
		 */
		virtual void create(Parent *parent, std::istringstream &i) = 0;
	protected:
		/**
		 * Base class constructor registers the factory with ED2KParser
		 * factories list, making it available to receive packets for
		 * construction.
		 *
		 * @param proto        Protocol, into which the packet belongs
		 * @param opcode       Opcode, upon which to call this factory
		 */
		PacketFactory(uint8_t proto, uint8_t opcode) {
			FIter endIter = ED2KParser::factories().end();
			if (ED2KParser::factories().find(proto) == endIter) {
				ED2KParser::factories().insert(
					std::make_pair(proto, FactoryMap())
				);
			}
			ED2KParser::factories()[proto].insert(
				std::make_pair(opcode, this)
			);
		}

		//! Dummy destructor
		virtual ~PacketFactory() {}
	};
private:
	typedef std::map<uint8_t, PacketFactory*> FactoryMap;
	typedef typename FactoryMap::iterator Iter;
	typedef typename std::map<uint8_t, FactoryMap>::iterator FIter;

	/**
	 * While it would be syntactically possible to implement the static
	 * data as member of the ED2KParser class, it seems to cause SIGSEGV
	 * upon module loading during static data initialization. While similar
	 * approach may work within the main application, it's a no-go within
	 * a module, and thus we wrap it inside a member function which returns
	 * the object by reference.
	 *
	 * @return  Static map of supported packet factories. The outer map
	 *          is really small, and contains only two entries - PR_ED2K
	 *          and PR_EMULE, since those two protocol's are used in ed2k
	 *          network. The inner map lists all packets in the given
	 *          protocol. When a new packet is found in stream, the inner
	 *          map corresponding to the protocol is searched for the packet
	 *          opcode, and the relevant factory's create() method called,
	 *          passing the packet data.
	 */
	static std::map<uint8_t, FactoryMap>& factories() {
		static std::map<uint8_t, FactoryMap> s_factories;
		return s_factories;
	}

	/**
	 * InternalPacket structure is a temporary storage for a single packet
	 * data.
	 */
	struct InternalPacket {
		uint8_t     m_proto;       //!< protocol
		uint32_t    m_len;         //!< data + opcode length
		uint8_t     m_opcode;      //!< opcode
		std::string m_data;        //!< data

		//! Output operator into streams
		friend std::ostream& operator<<(
			std::ostream &o, const InternalPacket &i
		) {
			o << "protocol=" << Utils::hexDump(i.m_proto)  << " ";
			o << "length="   << i.m_len    << " ";
			o << "opcode="   << Utils::hexDump(i.m_opcode) << " ";
			if (i.m_data.size() < 1024) {
				o << Utils::hexDump(i.m_data);
			} else {
				o << "\nData omitted (length >= 1024)";
			}
			return o;
		}
	};

	/**
	 * readPacket() method attempts to read a single packet from the
	 * designated stream.
	 *
	 * @param i       Stream to read the packet from
	 * @param p       Packet object to store the packet data in
	 * @return        True if packet was found and read; false otherwise
	 *
	 * \throws std::out_of_range if only part of the packet could be read.
	 *         This generally indicates you should try again later when more
	 *         data is available.
	 * \throws std::runtime_error on fatal errors. If this is thrown, the
	 *         stream should be marked unusable, and exception propagated
	 *         up to client code.
	 */
	bool readPacket(std::istringstream &i, InternalPacket &p) {
		NetProtocol::parseHeader(i, p.m_proto, p.m_len, p.m_opcode);

		switch (p.m_proto) {
			case PR_EMULE: case PR_ED2K: 
			case PR_KADEMLIA: case PR_ZLIB:
				if (p.m_len == 0) { return true; }
				break;
			default:
				throw std::runtime_error(
					"Invalid protocol %s" 
					+ Utils::hexDump(p.m_proto)
				);
		}

		if (i.str().size() < p.m_len + NetProtocol::HEADER_LENGTH) {
			m_need = p.m_len +
				NetProtocol::HEADER_LENGTH - i.str().size();

			return false;
		}

		CHECK_THROW(p.m_len > 0);
		p.m_data = Utils::getVal<std::string>(i, p.m_len - 1);
		if (p.m_data.size() != p.m_len - 1) {
			throw Utils::ReadError(
				(boost::format(
					"Internal parser error error: "
					"%s != %s; packet: %s"
				) % p.m_data.size() % p.m_len % p).str()
			);
		}
		m_need = 0;

		if (p.m_proto == PR_ZLIB || p.m_proto == PR_KADEMLIA_ZLIB) {
			p.m_data = Zlib::decompress(p.m_data);
			CHECK_THROW_MSG(p.m_data.size(), "unpacking failed");

			if (p.m_proto == PR_ZLIB) {
				p.m_proto = PR_ED2K;
			} else {
				p.m_proto = PR_KADEMLIA;
			}
		} else if (
			p.m_proto != PR_EMULE &&
			p.m_proto != PR_ED2K &&
			p.m_proto != PR_KADEMLIA
		) {
			throw std::runtime_error("invalid protocol");
		}

		// used for statistics gathering
		if (
			p.m_opcode == OP_SENDINGCHUNK ||
			p.m_opcode == OP_PACKEDCHUNK
		) {
			ED2KPacket::addOverheadDn(6 + 24);
		} else {
			ED2KPacket::addOverheadDn(6 + p.m_data.size());
		}
#ifdef HEXDUMPS
		logDebug(boost::format("Received packet: %s") % p);
#endif
		return true;
	}

	std::string m_buffer;       //!< Internal data buffer
	Parent *m_parent;           //!< Pointer to packets handler class
	InternalPacket m_packet;    //!< Packet currently being parsed

	/**
	 * When parsing long packets (e.g. data packets - 10k long), this
	 * indicates how much more data is needed before completing the packet.
	 * This is used for optimizing, on order to reduce the amount of
	 * "failed" packet parse attempts.
	 */
	uint32_t m_need;
};

} // end namespace Donkey

/**
 * Declares a new packet handler within this parser.
 *
 * @param Target               Class which will handle this packet type.
 * @param Packet               Packet type to be handled.
 *
 * \note The class must also declare a member function with the following
 * prototype in order to receive the notifiations:
 * void onPacket(const Packet &);
 * The name of the function is hardcoded. Failure to implement this function
 * will result in compile-time errors. If the function is private, additional
 * macro must be used in the class interface in order to make the function
 * accessible to the parser. See below.
 */
#define DECLARE_PACKET_HANDLER(Target, Packet)                              \
	static Factory_##Packet < Target > s_packetFactory##Packet

/**
 * Special version, allowing Parent and Target to differ.
 *
 * @param Parent         Parent parser
 * @param Target         Target which will receive the events
 */
#define DECLARE_PACKET_HANDLER2(Parent, Target, Packet)                     \
	static Factory_##Packet < Parent, Target > s_packetFactory##Packet

/**
 * Special version, allow different Parent and Target and a non-default
 * protocol parser (eg UDP).
 *
 * @param Parent         Parent parser.
 * @param Target         Target which will receive the events.
 * @param Protocol       Parser protocol type, eg ED2KNetProtocolUDP.
 * @param Packet         Packet type to be handled.
 */
#define DECLARE_PACKET_HANDLER3(Parent, Target, Protocol, Packet) \
        static Factory_##Packet < Parent, Target, Protocol >      \
	s_packetFactory##Packet

/**
 * Use this macro in your class's interface to allow parser factories to
 * access the packet handler functions, if they are declared private.
 */
#define FRIEND_PARSER(Class, Packet) \
	friend class Factory_##Packet<Class>

/**
 * This macro is used by the implementation to declare a new packet parser
 * factory. This should not be used by client code.
 *
 * @param Proto          Packet protocol.
 * @param PacketType     Type of packet this parser supports. Must be
 *                       fully-qualified type name.
 * @param Opcode         The opcode of the packet this factory is capable of
 *                       creating. This factory will be called when this opcode
 *                       is encountered in stream. Overlapping opcodes go
 *                       against the logic and thus are not allowed.
 */
#define DECLARE_PACKET_FACTORY(Proto, PacketType, Opcode)                     \
template<                                                                     \
	class Parent,                                                         \
	class Target = Parent,                                                \
	class NetProtocol = typename ED2KParser<Parent>::NetProtocolType      \
> class Factory_##PacketType                                                  \
: public ED2KParser<Parent, NetProtocol>::PacketFactory {                     \
public:                                                                       \
	Factory_##PacketType()                                                \
	: ED2KParser<Parent, NetProtocol>::PacketFactory(Proto, Opcode)       \
	{ }                                                                   \
	virtual void create(Parent *parent, std::istringstream &i) {          \
		parent->onPacket(ED2KPacket::PacketType(i));                  \
	}                                                                     \
}

#include "factories.h"            // Packet factories

#endif
