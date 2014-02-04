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

#ifndef __HOSTINFO_H__
#define __HOSTINFO_H__

/**
 * \file hostinfo.h Interface for Hydranode Resolver API
 */

#include <hnbase/osdep.h>
#include <hnbase/event.h>
#include <hnbase/ipv4addr.h>
#include <hnbase/workthread.h>
#include <hnbase/timed_callback.h>
#include <string>

#ifdef WIN32
	#include <winsock2.h>
#else
	#include <netdb.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
#endif

/**
 * Contains information about hosts, which is filled by process() [blocking]
 * method. Generally, process() will be called in a secondary thread.
 */
class HNBASE_EXPORT HostInfo : public ThreadWork {
public:
	enum ErrorState {
		HI_NOERROR    = 0,
		HI_NOTFOUND   = HOST_NOT_FOUND,
		HI_NODATA     = NO_ADDRESS,
		HI_NORECOVERY = NO_DATA,
		HI_TRYAGAIN   = TRY_AGAIN
	};
	typedef std::vector<IPV4Address>::const_iterator Iter;
	typedef boost::function<void (HostInfo)> HandlerType;

	HostInfo(const std::string &name, HandlerType);
	HostInfo(const HostInfo &h);
	HostInfo& operator=(const HostInfo &h);

	bool process();

	Iter begin()  const { return m_addrList.begin(); }
	Iter end()    const { return m_addrList.end();   }
	size_t size() const { return m_addrList.size();  }
	std::string getName() const { return m_name; }
	std::vector<std::string> getAliases()   const { return m_aliasList; }
	std::vector<IPV4Address> getAddresses() const { return m_addrList;  }
	ErrorState error() const { return m_error; }
	std::string errorMsg() const { return m_errorMsg;  }

private:
	std::vector<IPV4Address> m_addrList;
	std::vector<std::string> m_aliasList;
	std::string m_name;
	ErrorState  m_error;
	std::string m_errorMsg;
	HandlerType m_handler;
};

namespace DNS {
	/**
	 * Worker thread that performs DNS lookups using blocking API calls.
	 */
	class HNBASE_EXPORT ResolverThread : public WorkThread {
	public:
		ResolverThread();
		static ResolverThread& instance();
		std::string error(HostInfo::ErrorState err) {
			return m_errors[err];
		}
	private:
		std::map<HostInfo::ErrorState, std::string> m_errors;
	};

	/**
	 * Perform a DNS name-to-address lookup.
	 *
	 * @param name       Name to be looked up, e.g. "www.google.com"
	 * @param handler    Function that takes HostInfo argument
	 *
	 * \note handler function is called even when DNS lookup fails. Make
	 *       sure to test HostInfo::error() for error conditions.
	 */
	inline void lookup(
		const std::string &name, HostInfo::HandlerType handler
	) {
		CHECK_RET(name.size());
		CHECK_RET(handler);
		ThreadWorkPtr job(new HostInfo(name, handler));
		ResolverThread::instance().postWork(job);
	}

	/**
	 * Overloaded version of the above, provided for convenience.
	 */
	template<typename T>
	inline void lookup(
		const std::string &name, T *obj, void (T::*handler)(HostInfo)
	) {
		CHECK_RET(obj);
		CHECK_RET(handler);
		lookup(name, boost::bind(handler, obj, _1));
	}
}

#endif
