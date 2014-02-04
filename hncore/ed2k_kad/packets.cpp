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

#include <hnbase/log.h>
#include <hncore/ed2k_kad/kademlia.h>
#include <hncore/ed2k_kad/packets.h>

namespace Donkey {
namespace ED2KPacket {

KadBootstrapResponse::KadBootstrapResponse(std::istream& i) {
	uint16_t entries = Utils::getVal<uint16_t>(i);

	logDebug(
		boost::format("KadBootstrapResponse: got %i entries") % entries
	);

	m_contacts.reserve(entries);

	while(entries--) {
		ED2KKad::Contact e = Utils::getVal<ED2KKad::Contact>(i);

		logDebug(boost::format("\t%s") % e.getStr());

		m_contacts.push_back(e);
	}
}

KadBootstrapResponse::operator std::string() const {
	std::stringstream ss;

	Utils::putVal<uint16_t>(ss, m_contacts.size());

	std::vector<ED2KKad::Contact>::const_iterator iter;

	for(iter = m_contacts.begin(); iter != m_contacts.end();) {
		Utils::putVal<ED2KKad::Contact>(ss, *iter++);
	}

	return ss.str();
}

KadRequest::KadRequest(std::istream& i)
: m_type(Utils::getVal<uint8_t>(i)),
m_a(Utils::getVal<ED2KKad::Id>(i)),
m_b(Utils::getVal<ED2KKad::Id>(i)) {
}

KadRequest::operator std::string() const {
	std::stringstream ss;

	Utils::putVal<uint8_t>(ss, m_type);
	Utils::putVal<ED2KKad::Id>(ss, m_a);
	Utils::putVal<ED2KKad::Id>(ss, m_b);

	return ss.str();
}


KadResponse::KadResponse(std::istream& i)
: m_target(Utils::getVal<ED2KKad::Id>(i)) {
	uint8_t entries = Utils::getVal<uint8_t>(i);

	logDebug(boost::format("KadResponse: got %i entries") % int(entries));

	m_contacts.reserve(entries);

	while(entries--) {
		ED2KKad::Contact e = Utils::getVal<ED2KKad::Contact>(i);

		logDebug(boost::format("\t%s") % e.getStr());

		m_contacts.push_back(e);
	}
}

KadResponse::operator std::string() const {
	std::stringstream ss;

	Utils::putVal<ED2KKad::Id>(ss, m_target);

	Utils::putVal<uint8_t>(ss, m_contacts.size());

	std::vector<ED2KKad::Contact>::const_iterator iter;

	for(iter = m_contacts.begin(); iter != m_contacts.end();) {
		Utils::putVal<ED2KKad::Contact>(ss, *iter++);
	}

	return ss.str();
}

}
} // end namespace Donkey
