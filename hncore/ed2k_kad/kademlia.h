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

#ifndef __ED2K_KAD_KADEMLIA_H__
#define __ED2K_KAD_KADEMLIA_H__

#include <bitset>
#include <hncore/kademlia.h>

namespace ED2KKad {

/**
 * ED2K Kad id is 128bit
 */
typedef std::bitset<128> Id;

/**
 * ED2K Kad Contact: a generic 128bit-id kademlia contact with tcp port and type
 */
struct Contact
: public Kad::Contact<128> {
	//! TCP Port
	uint16_t m_tcpPort;

	//! Contact type
	uint8_t m_type;

	//! Dumps to human readable string
	std::string getStr() const {
		boost::format f(
			"%s (%s, tcp %i, type %i)"
		);

		f % Kad::KUtils::bitsetHexDump(m_id)
		  % m_addr.getStr() % m_tcpPort % int(m_type);

		return f.str();
	}

	//! Equality test operator
	bool operator==(const ED2KKad::Contact& t) {
		return Kad::Contact<128>::operator==(t);
	}

	//! Flag
	bool m_alive;

	//! Default constructor
	Contact()
	: Kad::Contact<128>(), m_tcpPort(0), m_type(0), m_alive(true)
	{ }
};

}

//! Specialization of IO functors for ED2KKadContact
namespace Utils {

/**
 * Specialization of getVal for bitsets
 */
template<>
struct getVal<std::bitset<128> > {
	//! Conversion to T
	operator std::bitset<128>() const { return m_value; }

	//! Explicit access to read value
	std::bitset<128> value() const { return m_value; }

	//! Generic constructor
	template<typename Stream>
	getVal(Stream &s) {
		// Read 128bit (in emule format, 4 uint32_t)
		uint32_t data[4];

		for(unsigned i = 0; i < 4; ++i) {
			data[3 - i] = Utils::getVal<uint32_t>(s);
		}

		for(unsigned i = 0; i < 128; ++i) {
			m_value.set(i, data[i / 32] & (1 << i % 32));
		}
	}
private:
	std::bitset<128> m_value;
};

/**
 * Specialization of putVal for bitsets
 */
template<>
struct putVal<std::bitset<128> > {
	//! Generic constructor
	template<typename Stream>
	putVal(Stream& s, const std::bitset<128>& bset) {
		// Write 128bit (in emule format, 4 uint32_t)
		uint32_t data[] = { 0, 0, 0, 0 };

		for(unsigned i = 0; i < 128; ++i) {
			data[3 - i / 32] |= (bset[i] << i % 32);
		}

		for(unsigned i = 0; i < 4; ++i) {
			Utils::putVal<uint32_t>(s, data[i]);
		}
	}
};

/**
 * Specialization of getVal for ED2KKadContact
 */
template<>
struct getVal<ED2KKad::Contact> {
	//! Conversion to T
	operator ED2KKad::Contact() const { return m_value; }

	//! Explicit access to read value
	ED2KKad::Contact value() const { return m_value; }

	//! Generic constructor
	template<typename Stream>
	getVal(Stream &s) {
		// Read 128bit (in emule format, 4 uint32_t)
		m_value.m_id = Utils::getVal<std::bitset<128> >(s);

		// Read ip address
		uint32_t addr = Utils::getVal<uint32_t>(s);
		m_value.m_addr.setAddr(SWAP32_ON_LE(addr));

		// Read udp port
		m_value.m_addr.setPort(Utils::getVal<uint16_t>(s));

		// Read tcp port
		m_value.m_tcpPort = Utils::getVal<uint16_t>(s);

		// Read type
		m_value.m_type = Utils::getVal<uint8_t>(s);
	}
private:
	ED2KKad::Contact m_value;
};

/**
 * Specialization of putVal for ED2KKadContact
 */
template<>
struct putVal<ED2KKad::Contact> {
	//! Generic constructor
	template<typename Stream>
	putVal(Stream &s, const ED2KKad::Contact &contact) {
		// Write 128bit (in emule format, 4 uint32_t)
		Utils::putVal<std::bitset<128> >(s, contact.m_id);

		// Write ip address
		Utils::putVal<uint32_t>(s, 
			SWAP32_ON_LE(contact.m_addr.getAddr())
		);

		// Write udp port
		Utils::putVal<uint16_t>(s, contact.m_addr.getPort());

		// Write tcp port
		Utils::putVal<uint16_t>(s, contact.m_tcpPort);

		// Write type
		Utils::putVal<uint8_t>(s, contact.m_type);
	}
};

}

#endif
