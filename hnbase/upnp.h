/*
 *  Copyright (c) 2005-2006 Alo Sarv <madcat_@users.sourceforge.net>
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
 * \file upnp.h Interface for UPnP port forwarding API
 */

#include <hnbase/fwd.h>
#include <hnbase/ipv4addr.h>
#include <boost/enable_shared_from_this.hpp>
#include <map>

/**
 * Contains UPnP-related classes, types and functions
 */
namespace UPnP {
	class Manager;
	class Router;
	enum Protocol { TCP, UDP };

	/**
	 * Attempts to locate all routers in this subnet via UDP broadcast.
	 *
	 * @param handler       Function which will be called every
	 *                      time a Router is found.
	 *
	 * \note This function over-writes the previous handler internally,
	 *       thus calling it multiple times with different handlers will
	 *       cause old handler(s) to be lost / overwritten.
	 */
	extern HNBASE_EXPORT void findRouters(
		boost::function<void (boost::shared_ptr<Router>)> handler
	);

	/**
	 * Router object is auto-detected via UDP broadcast by implementation,
	 * and then passed to handler function that was passed to findRouters
	 * method. Routers are always contained within boost::shared_ptr.
	 *
	 * Routers may allow configuration of themselves, namely, adding and
	 * removing port mappings (forwardings).
	 */
	class HNBASE_EXPORT Router : public boost::enable_shared_from_this<Router> {
	public:
		/**
		 * \returns human-readable name of the router
		 */
		std::string getName() const { return m_name; }

		/**
		 * \returns the control URL for this router
		 */
		std::string getCurl() const { return m_curl; }

		/**
		 * \returns the control IP for this router
		 */
		IPV4Address getIp() const { return m_ip; }

		/**
		 * \returns true if it supports port forwarding/mapping
		 */
		bool supportsForwarding() const { return m_curl.size(); }

		/**
		 * Add a port mapping
		 *
		 * @param prot      Protocol, either TCP or UDP
		 * @param ePort     External port
		 * @param iPort     Internal port
		 * @param reason    Description/reason for the mapping
		 * @param leaseTime Automatic un-mapping timeout in seconds
		 */
		void addForwarding(
			Protocol prot, uint16_t ePort, uint16_t iPort = 0,
			const std::string &reason = "", uint32_t leaseTime = 0
		);

		/**
		 * Remove a port mapping
		 *
		 * @param prot     Protocol, either TCP or UDP
		 * @param port     Port to be removed
		 */
		void removeForwarding(Protocol prot, uint16_t port);
	private:
		friend class Manager;

		/**
		 * Connection wraps together buffer input/output of a
		 * SocketClient, which itself lacks buffering support.
		 */
		struct Connection {
			Connection(SocketClient *s);
			~Connection();

			SocketClient *m_sock;        //!< Socket object
			std::string m_outBuffer;     //!< Outgoing buffer
			std::string m_inBuffer;      //!< Incoming buffer

			//! Incoming buffer in lowercase (for parsing)
			std::string m_inBufferLower;
		};

		/**
		 * Only allowed constructor (allowed by Manager)
		 *
		 * @param ip        Control IP address, including port
		 * @param loc       Description URL
		 */
		Router(IPV4Address ip, const std::string &loc);

		/**
		 * @name Disabled members
		 *
		 * Default constructor, as well as copy-constructor and
		 * assignment operator are not allowed for Routers.
		 */
		//@{
		Router();
		Router(const Router&);
		Router& operator=(const Router&);
		//@}

		/**
		 * Socket event handler, called from event tables.
		 */
		void onSocketEvent(SocketClient *sock, SocketEvent event);

		/**
		 * Sends a single request, in a new Connection, to the router.
		 *
		 * @param data      The request data
		 */
		void sendRequest(const std::string &data);

		/**
		 * Sends buffered data to sockets.
		 */
		void sendData();

		/**
		 * Parses the input buffer of a specified connection.
		 *
		 * @param c        Connection which buffer to parse
		 */
		void parseBuffer(Connection *c);

		//! Map of current open connections, keyed by the socket
		std::map<SocketClient*, Connection*> m_sockets;

		//! Iterator for the above map
		typedef std::map<SocketClient*, Connection*>::iterator Iter;

		IPV4Address m_ip;     //!< IP address for this router
		IPV4Address m_internalIp; //!< Internal ip address
		std::string m_name;   //!< Human-readable name
		std::string m_curl;   //!< Control URL / location
	};
}
