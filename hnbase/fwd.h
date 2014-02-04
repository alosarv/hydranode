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
 * \file fwd.h Forward declarations for Hydrabase classes
 */

#ifndef __HNBASEFWD_H__
#define __HNBASEFWD_H__

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <hnbase/osdep.h>

class HashBase;
class HashSetBase;
class SocketWatcher;
class SocketClient;
class SocketServer;
class UDPSocket;
class IPV4Address;
class Object;
template<typename Impl, typename _ImplPtr> class Scheduler;
template<typename T> class Range;
template<typename T> class RangeList;

//! Socket types and protocols for easier SSocket class usage
namespace Socket {
	class TCP;        //!< Protocol:  TCP
	class UDP;        //!< Protocol:  UDP
	class Client;     //!< Semantics: Client
	class Server;     //!< Semantics: Server

	/**
	 * Traits class for choosing the underlying implementation based
	 * on socket type and protocol. The primary template of this class is
	 * not implemented. Instead, one of the specializations are expected to
	 * be chosen, which define the actual underlying implementation type. If
	 * no possible specialization could be found, a compile-time error
	 * occours.
	 */
	template<typename Type, typename Proto>
	struct Implement;
	template<>
	struct Implement<Client, TCP> {
		typedef SocketClient Impl;
	};
	template<>
	struct Implement<Server, UDP> {
		typedef UDPSocket Impl;
	};
	template<>
	struct Implement<Server, TCP> {
		typedef SocketServer Impl;
	};
}

template<
	typename Module,
	typename Type,
	typename Protocol = Socket::TCP,
	typename Impl = typename Socket::Implement<Type, Protocol>::Impl
> class SSocket;

//! Various socket events passed to event handler
enum SocketEvent {
	SOCK_CONNECTED = 1,     //!< Outgoing connection has been established
	SOCK_CONNFAILED,        //!< Outgoing connection failed
	SOCK_ACCEPT,            //!< Incoming connection is ready to be accepted
	SOCK_READ,              //!< Incoming data is ready for reading
	SOCK_WRITE,             //!< Socket became writable
	SOCK_LOST,              //!< Socket connection has been lost
	SOCK_ERR,               //!< Error has accoured.
	SOCK_TIMEOUT,           //!< The connection has timed out
	SOCK_BLOCKED            //!< The connection has been blocked by IpFilter
};

/**
 * Object events, propangated up the object hierarchy, and passed to event
 * handler
 */
enum ObjectEvent {
	OBJ_MODIFIED = 0x01, //!< This object's data has been modified
	OBJ_ADDED,           //!< An object has been added to this container
	OBJ_REMOVED,         //!< An object has been removed from this container
	OBJ_DESTROY          //!< This object has been destroyed
};

#endif
