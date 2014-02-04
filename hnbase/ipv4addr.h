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
 * \file ipv4addr.h Interface for IPV4Addr class
 */

#ifndef __IPV4ADDR_H__
#define __IPV4ADDR_H__

#include <hnbase/osdep.h>

/**
 * IPV4Address encapsulates an IP address, with optional port.
 */
class HNBASE_EXPORT IPV4Address {
public:
	//! @name Construction and destruction
	//@{
	explicit IPV4Address(const std::string &addr, uint16_t port = 0);
	explicit IPV4Address(uint32_t addr, uint16_t port = 0);
	IPV4Address();
	//@}

	//! @name Accessors
	//@{
	uint32_t    getAddr() const { return m_addr; }
	uint16_t    getPort() const { return m_port; }
	uint32_t    getIp() const { return m_addr; }
	void setAddr(uint32_t addr) { m_addr = addr; }
	void setPort(uint16_t port) { m_port = port; }

	// Returns "127.0.0.1:25" for example
	// Note - these are not inlined because we don't have Socket namespace
	// yet at this point in this header, and reordering things doesn't help.
	std::string getStr()   const;
	void setAddr(const std::string &addr);
	std::string getAddrStr() const;
	//@}

	//! Output operator to streams
	friend HNBASE_EXPORT std::ostream& operator<<(
		std::ostream &o, const IPV4Address &a
	);

	/**
	 * @name Comparison operators
	 */
	//@{
	friend bool operator<(const IPV4Address &x, const IPV4Address &y) {
		if (x.m_addr != y.m_addr) {
			return x.m_addr < y.m_addr;
		} else {
			return x.m_port < y.m_port;
		}
	}
	friend bool operator==(const IPV4Address &x, const IPV4Address &y) {
		return x.m_addr == y.m_addr && x.m_port == y.m_port;
		}
	friend bool operator!=(const IPV4Address &x, const IPV4Address &y) {
		return !(x == y);
	}
	operator bool() const { return m_addr && m_port; }
	//@}
private:
	uint32_t m_addr;         //!< IP address
	uint16_t m_port;         //!< Port
};

// Few useful utility functions, defined in Socket namespace
namespace Socket {
	/**
	 * Makes internet address from string. Basically this is inet_ntoa()
	 *
	 * @param data         Dot-separated IP address, e.g. "127.0.0.1"
	 * @return             Internet address, used in binary formats.
	 */
	HNBASE_EXPORT uint32_t makeAddr(const std::string &data);

	/**
	 * Creates human-readable address from ip. Basically this is inet_aton.
	 *
	 * @param ip           Ip address
	 * @return             Human-readable address, e.g. "127.0.0.1"
	 */
	HNBASE_EXPORT std::string getAddr(uint32_t ip);
}

#endif
