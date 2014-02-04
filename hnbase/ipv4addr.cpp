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
 * \file ipv4addr.cpp Implementation of IPV4Addr class
 */

#include <hnbase/pch.h>
#include <hnbase/ipv4addr.h>
#include <boost/lexical_cast.hpp>

#ifdef WIN32
	#include <winsock2.h>
#else
	#include <netinet/in.h>
	#include <arpa/inet.h>
#endif

namespace Socket {
	//! make internet address
	uint32_t makeAddr(const std::string &addr) {
		return inet_addr(addr.c_str());
	}
	std::string getAddr(uint32_t ip) {
		return inet_ntoa(*(in_addr*)&ip);
	}
}

// IPV4Address class
// -----------------

IPV4Address::IPV4Address() : m_addr(), m_port() {}
IPV4Address::IPV4Address(const std::string &addr, uint16_t port /* = 0 */)
: m_addr(Socket::makeAddr(addr)), m_port(port) {}

IPV4Address::IPV4Address(uint32_t addr, uint16_t port /* = 0 */)
: m_addr(addr), m_port(port) {}

std::ostream& operator<<(std::ostream &o, const IPV4Address &a) {
	return o << a.getStr();
}

std::string IPV4Address::getStr() const {
	std::string ret(Socket::getAddr(m_addr));
	if (m_port) {
		ret += ":";
		ret += boost::lexical_cast<std::string>(m_port);
	}
	return ret;
}

std::string IPV4Address::getAddrStr() const {
	return Socket::getAddr(m_addr);
}

void IPV4Address::setAddr(const std::string &addr) {
	m_addr = Socket::makeAddr(addr);
}
