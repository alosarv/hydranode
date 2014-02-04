/*
 *  Copyright (C) 2005-2006 Alo Sarv <madcat_@users.sourceforge.net>
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
 * \file hostinfo.cpp Implementation of Hydranode resolver API
 */

#include <hnbase/hostinfo.h>

namespace DNS {
	ResolverThread::ResolverThread() {
		m_errors[HostInfo::HI_NOERROR]    = "No Error";
		m_errors[HostInfo::HI_NOTFOUND]   = "Not Found";
		m_errors[HostInfo::HI_NODATA]     = "No Data";
		m_errors[HostInfo::HI_NORECOVERY] = "Fatal Name Server Error";
		m_errors[HostInfo::HI_TRYAGAIN]   = "Try Again Later";
	}

	ResolverThread& ResolverThread::instance() {
		static ResolverThread rt;
		return rt;
	}
}

HostInfo::HostInfo(const std::string &name, HandlerType handler)
: m_name(name), m_error(HI_NOERROR), m_handler(handler) {}

HostInfo::HostInfo(const HostInfo &h) {
	*this = h;
}

HostInfo& HostInfo::operator=(const HostInfo &h) {
	m_error     = h.m_error;
	m_errorMsg  = h.m_errorMsg;
	m_handler   = h.m_handler;
	m_name      = h.m_name;
	m_addrList  = h.m_addrList;
	m_aliasList = h.m_aliasList;
	return *this;
}

bool HostInfo::process() {
	struct hostent *ret = gethostbyname(m_name.c_str());
	if (!ret) {
		m_error = static_cast<ErrorState>(h_errno);
	} else {
		int i = 0;
		while (char *c = ret->h_addr_list[i++]) {
			IPV4Address addr(*reinterpret_cast<uint32_t*>(c));
			m_addrList.push_back(addr);
		}
		i = 0;
		while (char *c = ret->h_aliases[i++]) {
			m_aliasList.push_back(c);
		}
	}
	m_errorMsg = DNS::ResolverThread::instance().error(m_error);
	Utils::timedCallback(boost::bind(m_handler, *this), 1);
	setComplete();
	return true;
}

