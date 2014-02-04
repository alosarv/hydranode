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

#ifndef __ED2K_KAD_PACKETS_H__
#define __ED2K_KAD_PACKETS_H__

#include <hncore/ed2k/opcodes.h>
#include <hncore/ed2k/packets.h>
#include <hncore/ed2k_kad/kademlia.h>

namespace Donkey {
namespace ED2KPacket {

/**
 * Base type for all the Kad packets
 */
template<unsigned opcode>
struct KadPacket : public Donkey::ED2KPacket::Packet {
	typedef KadPacket KadBase;

	enum { OPCODE = opcode };
};

/**
 * Bootstrap request. Packet format:
 *
 * <ED2Kad::Contact>
 */
class KadBootstrapRequest
: public KadPacket<0> {
	//! Requesting client contact
	ED2KKad::Contact m_contact;

public:
	//! Serialize
	operator std::string() const {
		std::stringstream ss;

		Utils::putVal<ED2KKad::Contact>(ss, m_contact);

		return ss.str();
	}

	//! Constructor
	KadBootstrapRequest(ED2KKad::Contact& contact)
	: m_contact(contact)
	{ }

	//! Deserialize from a stream
	KadBootstrapRequest(std::istream& i)
	: m_contact(Utils::getVal<ED2KKad::Contact>(i))
	{ }
};

/**
 * Bootstrap response. Packet data:
 *
 * <(uint16_t) number of contacts><ED2KKad::Contact>*number of contacts
 */
class KadBootstrapResponse
: public KadPacket<0x8> {
private:
	std::vector<ED2KKad::Contact> m_contacts;

public:
	//! Getter/setter methods
	//!@{
	void setContacts(
		const std::vector<ED2KKad::Contact> &contacts
	) {
		m_contacts = contacts;
	}

	const std::vector<ED2KKad::Contact> &getContacts() const {
		return m_contacts;
	}
	//!@}

	//! Serialize to string
	operator std::string() const;

	//! Constructor
	KadBootstrapResponse() { }

	//! Deserialize from a stream
	KadBootstrapResponse(std::istream& i);
};

/**
 * Hello request. Packet data:
 *
 * <ED2KKad::Contact>
 */
class KadHelloRequest
: public KadPacket<0x10> {
	//! Requesting client contact
	ED2KKad::Contact m_contact;

public:
	//! Serialize to string
	operator std::string() const {
		std::stringstream ss;

		Utils::putVal<ED2KKad::Contact>(ss, m_contact);

		return ss.str();
	}

	//! Constructor
	KadHelloRequest(ED2KKad::Contact& contact)
	: m_contact(contact)
	{ }

	//! Deserialize from a stream
	KadHelloRequest(std::istream& i)
	: m_contact(Utils::getVal<ED2KKad::Contact>(i))
	{ }
};

/**
 * Hello response. Packet data:
 *
 * <ED2KKad::Contact>
 */
class KadHelloResponse
: public KadPacket<0x18> {
	//! Requesting client contact
	ED2KKad::Contact m_contact;

public:
	//! Serialize to string
	operator std::string() const {
		std::stringstream ss;

		Utils::putVal<ED2KKad::Contact>(ss, m_contact);

		return ss.str();
	}

	//! Constructor
	KadHelloResponse(ED2KKad::Contact& contact)
	: m_contact(contact)
	{ }

	//! Deserialize from a stream
	KadHelloResponse(std::istream& i)
	: m_contact(Utils::getVal<ED2KKad::Contact>(i))
	{ }
};

/**
 * Firewalled request. No packet data.
 */
class KadFirewalledRequest
: public KadPacket<0x50> {
public:
	operator std::string() const { return ""; }

	//! Constructor
	KadFirewalledRequest(ED2KKad::Contact& contact)
	{ }

	//! Deserialize from a stream
	KadFirewalledRequest(std::istream& i)
	{ }
};

/**
 * Firewalled response. Data packet:
 *
 * <uint32_t(firewalled request ip)>
 */
class KadFirewalledResponse
: public KadPacket<0x58> {
	IPV4Address m_addr;

public:
	//! Returns address
	IPV4Address getAddr() const {
		return m_addr;
	}

	//! Serialize to string
	operator std::string() const {
		std::stringstream ss;

		Utils::putVal<uint32_t>(ss, SWAP32_ON_LE(m_addr.getAddr()));

		return ss.str();
	}

	//! Constructor
	KadFirewalledResponse(const IPV4Address& addr)
	: m_addr(addr)
	{ }

	//! Deserialize from a stream
	KadFirewalledResponse(std::istream& i) {
		uint32_t addr = Utils::getVal<uint32_t>(i);

		m_addr.setAddr(SWAP32_ON_LE(addr));
	}
};

/**
 * Kademlia request. Data packet:
 *
 * <uint8_t(type)>
 * <Id (what)>
 * <Id (source)>
 */
class KadRequest
: public KadPacket<0x20> {
public: // FIXME SHOULD BE PRIVATE
	uint8_t m_type;

	// Types
	enum {
		FIND_NODE = 0x02,
		FIND_VALUE = 0x04,
		STORE = 0x0b
	};

	ED2KKad::Id m_a, m_b;

public:

	//! Serialize to string
	operator std::string() const;

	//! Constructor
	KadRequest() {
	};

	//! Deserialize from a stream
	KadRequest(std::istream& i);
};

/**
 * Kademlia response. Data packet:
 * HASH (target) [16]> <CNT> <PEER [25]>*(CNT)
 *
 */
class KadResponse
: public KadPacket<0x28> {
private:
	ED2KKad::Id m_target;

	std::vector<ED2KKad::Contact> m_contacts;

public:
	//! Getter/setter methods
	//!@{
	void setTarget(const ED2KKad::Id &target) {
		m_target = target;
	}

	const ED2KKad::Id &getTarget() const {
		return m_target;
	}

	void setContacts(const std::vector<ED2KKad::Contact>& contacts) {
		m_contacts = contacts;
	}

	const std::vector<ED2KKad::Contact>& getContacts() const {
		return m_contacts;
	}
	//!@}

	//! Serialize
	operator std::string() const;

	//! Constructor
	KadResponse() {
	};

	//! Deserialize from a stream
	KadResponse(std::istream& i);
};

}
} // end namespace Donkey

#endif
