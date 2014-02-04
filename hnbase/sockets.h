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

#ifndef __SOCKETS_H__
#define __SOCKETS_H__

/**
 * \file sockets.h Interface for Hydranode Socket API
 */

/**
 * @page netsubsys Networking Subsystem
 *
 * The Networking subsystem is a wrapper around OS-specific networking
 * (namely, Windows Sockets and BSD Sockets). The purpose is to completely
 * hide the OS-specific from users of these classes.
 *
 * Usage:
 *        Read the public interfaces of SocketClient and SocketServer. Really,
 *        thats all there's to it. Some things you should keep in mind tho:
 *
 *        Almost all public member functions of SocketServer/SocketClient
 *        classes may throw exceptions if things go wrong. Practice has shown
 *        that using return values to indicate errors doesn't get us anywhere,
 *        since everyone will simply ignore the return values. So there -
 *        now you can't ignore the errors anymore. So basically, get to writing
 *        try/catch blocks at all accesses to sockets. I know its tedious,
 *        but its safer than to have sockets around in odd/broken states and
 *        still "available" for usage.
 *
 * Internal stuff:
 *        Sockets register themselves with SocketWatcher, which performs
 *        sockets polling for events. When events are detected, the events
 *        are posted to event subsystem. Refer to SocketWatcher documentation
 *        and implementation for more information.
 *
 */

#include <hnbase/osdep.h>
#include <hnbase/event.h>
#include <hnbase/ipv4addr.h>
#include <hnbase/fwd.h>
#include <boost/function.hpp>
#include <boost/lexical_cast.hpp>
#include <list>
#include <map>
#include <set>

//! Typedef SOCKET depending on platform
#ifdef WIN32
	typedef UINT_PTR SOCKET;
#else
	typedef int SOCKET;
#endif

/**
 * Exception class
 */
class HNBASE_EXPORT SocketError : public std::runtime_error {
public:
	SocketError(const std::string &err);
	~SocketError() throw();
};

/**
 * Base class for Socket classes, all members are protected and destructor
 * is pure virtual - don't use this class directly
 */
class HNBASE_EXPORT SocketBase {
public:
	//! Socket prioritites.
	enum SocketPriority {
		PR_LOW = -1,
		PR_NORMAL = 0,
		PR_HIGH = 1
	};

	typedef SocketEvent EventType;
	typedef SocketClient AcceptType;
	typedef SocketPriority PriorityType;

	/**
	 * Retrieve the internal SOCKET object
	 */
	SOCKET getSocket() { return m_socket; }

	//! If this socket is marked for deletion
	bool toDelete() const { return m_toDelete; }

	/**
	 * Set the priority of this socket. This affects it's scheduling policy,
	 * higher-priority get handled before lower-priority sockets.
	 *
	 * @param prio     Priority
	 */
	void setPriority(SocketPriority prio) { m_priority = prio; }

	/**
	 * Get the priority of this socket
	 */
	int8_t getPriority() const { return m_priority; }

protected:
	/**
	 * Base constructor
	 */
	SocketBase();

	/**
	 * Construct from existing socket.
	 */
	SocketBase(SOCKET s);

	/**
	 * Pure virtual destructor
	 */
	virtual ~SocketBase() = 0;

	//! Derived classes must implement this method
	virtual void destroy() = 0;

	//! Internal socket object
	SOCKET m_socket;

	//! Initialized to false and set to true in destroy() method in
	//! derived classes, this indicates SocketWatcher must delete the
	//! object during removal.
	bool m_toDelete;

	//! Socket's priority
	int8_t m_priority;
};

// Event handlers
typedef boost::function<void (SocketClient*, SocketEvent)> SCEventHandler;
typedef boost::function<void (SocketServer*, SocketEvent)> SSEventHandler;
typedef boost::function<void (UDPSocket*   , SocketEvent)> UDPSEventHandler;

/**
 * Socket client is a "connection" that is used to transfer data between
 * two peers.
 */
class HNBASE_EXPORT SocketClient : public SocketBase {
public:
	//! Type of event handler used by this object
	typedef SCEventHandler HandlerType;

	/**
	 * Constructor
	 *
	 * @param ehandler   Optional handler for socket events
	 */
	SocketClient(SCEventHandler ehandler = 0);

	/**
	 * Set new event handler for this socket
	 *
	 * @param h        New event handler
	 */
	void setHandler(HandlerType h) { m_handler = h; }

	/**
	 * Destroy this socket. It is safe to call this method multiple times.
	 */
	void destroy();

	/**
	 * Write data into socket
	 *
	 * @param buffer    Pointer to buffer of data to be written.
	 * @param length    Amount of data to send.
	 * @return          Number of bytes written to socket.
	 *
	 * \throws SocketError if something goes wrong.
	 *
	 * \note The socket API does not take ownership of the pointer passed
	 *       to this function. It attempts to send out as much data as is
	 *       possible at this moment, but does not buffer anything. Client
	 *       code is responsible for freeing the memory pointed to by the
	 *       buffer pointer.
	 * \note It is recommended to use SSocket API, which implements internal
	 *       buffering and is safer to use than this raw API.
	 */
	uint32_t  write(const char *buffer, uint32_t length);

	/**
	 * Read data from socket
	 *
	 * @param buffer     Buffer into what to place the read data.
	 * @param length     Size of buffer, amount of data to be read.
	 * @return           Number of bytes actually read.
	 *
	 * \throws SocketError if something goes wrong.
	 */
	uint32_t read(void *buffer, uint32_t length);

	/**
	 * Make an outgoing connection.
	 *
	 * @param addr     Address to connect to.
	 * @param timeout   Time in milliseconds to attempt to connect. Once the
	 *                  timeout passes, SOCK_TIMEOUT event will be posted
	 *                  if the connection attempt hasn't been completed
	 *                  to that point. Defaults to 5 seconds.
	 *
	 * Preconditions: addr.m_port != 0, addr.m_ip != 0
	 * \throws SocketError if something goes wrong.
	 */
	void connect(IPV4Address addr, uint32_t timeout = 5000);

	/**
	 * Close the connection
	 *
	 * \throws SocketError if something goes wrong.
	 */
	void disconnect();

	/**
	 * @name Accessors for various internal variables
	 */
	//@{
	bool isConnected()  const { return m_connected;  }
	bool isOk()         const { return !m_erronous;  }
	bool isConnecting() const { return m_connecting; }
	bool isWritable()   const { return m_writable;   }
	//@}

	/**
	 * Set timeout; if no events happen within this timeout, SOCK_TIMEOUT
	 * is emitted.
	 *
	 * @param t      Timeout, in milliseconds
	 */
	void setTimeout(uint32_t t) {
		m_timeout = EventMain::instance().getTick() + t;
	}

	/**
	 * \returns The address this socket is connected to.
	 */
	IPV4Address getPeer() const { return m_peer; }

	/**
	 * \returns The local address this socket is bound to
	 * \note When *this is disconnected, this returns 0, however, if *this
	 *       is connected, it returns our external IP address if we are
	 *       connected to the net directly; when we are behind a gateway
	 *       though, this returns our intranet address, so don't rely on
	 *       this giving you our external IP always (altough it's worth a
	 *       shot).
	 */
	IPV4Address getAddr() const;

	/**
	 * Input operator; writes the specified data into socket
	 */
	friend SocketClient& operator<<(
		SocketClient &c, const std::string &data
	) {
		c.write(data.data(), data.size());
		return c;
	}

	/**
	 * Extractor operator; reads all current data from socket to buffer
	 */
	friend SocketClient& operator>>(SocketClient &c, std::string &buf) {
		char tmp[1024];
		int got = 0;
		while ((got = c.read(tmp, 1024))) {
			buf.append(std::string(tmp, got));
		}
		return c;
	}

	/**
	 * @name Advanced features - use only if you know what you're doing
	 */
	//!@{
	void setTcpSendBufSize(uint32_t size);
	void setTcpRecvBufSize(uint32_t size);
	uint32_t getTcpSendBufSize() const;
	uint32_t getTcpRecvBufSize() const;
	//!@}
private:
	friend class SocketWatcher;
	friend class SocketServer;

	/**
	 * Construct from existing SOCKET object
	 */
	SocketClient(SOCKET s);

	/**
	 * Destructor hidden, use destroy() method instead.
	 */
	virtual ~SocketClient();

	bool m_connected;           //!< If the socket is connected
	bool m_hasData;             //!< If there is incoming data to be read
	bool m_connecting;          //!< If a connection attempt is in progress
	bool m_erronous;            //!< If the socket is erronous
	bool m_writable;            //!< If this socket is writable
	bool m_autoConnect;         //!< Holds autoconnect flag, used with
	                            //!< connect to hostname method.
	uint64_t m_timeout;         //!< Timeout counter for connection attempts
	IPV4Address m_peer;         //!< Peer to where we are connected to

	void setPeerName();         //!< Update m_peer member
	void close();               //!< Closes the underlying socket

	HandlerType m_handler;      //!< Event handler for this socket
};

/**
 * A listening socket server which accepts incoming connections (as
 * SocketClient objects).
 */
class HNBASE_EXPORT SocketServer : public SocketBase {
public:
	//! Type of event handler used by this object
	typedef SSEventHandler HandlerType;

	/**
	 * Constructor
	 *
	 * @param ehandler    Callback function for events.
	 */
	SocketServer(SSEventHandler ehandler = 0);

	/**
	 * Set new event handler for this socket
	 *
	 * @param h        New event handler
	 */
	void setHandler(HandlerType h) { m_handler = h; }

	/**
	 * Destroy this socket. It is safe to call this method multiple times.
	 */
	void destroy();

	/**
	 * Accept an incoming connection
	 *
	 * @param ehandler     Callback function for events for the accepted
	 *                     socket.
	 * @return             Accepted connection. May be NULL if accept()
	 *                     fails.
	 */
	SocketClient* accept(SCEventHandler ehandler = 0);

	/**
	 * Puts the server into listening state.
	 *
	 * @param addr     Address to listen on.
	 *
	 * \note If addr.m_ip == 0, the listener will listen on all available
	 * networks.
	 */
	void listen(IPV4Address addr);

	/**
	 * @name Accessors for various internal variables
	 */
	//@{
	bool isListening() { return m_listening; }
	bool hasIncoming() { return m_incoming; }
	//@}

	/**
	 * @returns The local address the server.
	 */
	IPV4Address getAddr() const { return m_addr; }

	/**
	 * @name Dummy methods, to allow generic programming
	 */
	//!@{
	bool isConnected()  const { return false; }
	bool isConnecting() const { return false; }
	//!@}
private:
	/**
	 * Destructor
	 */
	virtual ~SocketServer();

	friend class SocketWatcher;
	bool m_incoming;                    //!< Incoming connection is waiting
	bool m_listening;                   //!< If the socket is listening
	bool m_erronous;                    //!< If the socket is erronous
	IPV4Address m_addr;                 //!< Address where we are listening

	void close();               //!< Closes the underlying socket

	HandlerType m_handler;      //!< Event handler for this socket
};

/**
 * UDP socket is connection-less socket, using the UDP protocol.
 * Usage:
 * Construct UDPSocket object on heap and call listen() member function to
 * start up the socket. After that, when incoming data is detected, events are
 * emitted, and data may be received using recv() member function. Where the
 * data originated will be written into the passed IPV4Address parameter. To
 * send data to a specific location, send() method may be used, where the last
 * parameter indicates where to send the data.
 *
 * \note UDP sockets are not reliable. There is no way of knowing whether the
 *       data sent out actually reached the recipient.
 */
class HNBASE_EXPORT UDPSocket : public SocketBase {
public:
	//! Type of event handler used by this object
	typedef UDPSEventHandler HandlerType;

	//! Construct new UDP socket, optionally setting handler
	UDPSocket(UDPSEventHandler handler = 0);

	/**
	 * Set new event handler for this socket
	 *
	 * @param h        New event handler
	 */
	void setHandler(HandlerType h) { m_handler = h; }

	/**
	 * Destroy this socket. It is safe to call this method multiple times.
	 */
	void destroy();

	/**
	 * Enable/disable broadcast flag
	 *
	 * @param val  True to set, false to unset
	 */
	void setBroadcast(bool val);

	/**
	 * Starting listening on a local address. If addr.ip is 0, we will
	 * listen on all available addresses. Addr.port may not be 0.
	 *
	 * @param addr       Address to listen on
	 */
	void listen(IPV4Address addr);

	//! Provided for convenience
	void listen(uint32_t addr, uint16_t port) {
		listen(IPV4Address(addr, port));
	}
	//! Provided for convenience
	void listen(const std::string &addr, uint16_t port) {
		listen(IPV4Address(addr, port));
	}

	/**
	 * Receive data from a location
	 *
	 * @param buf      Buffer to write the received data
	 * @param len      Length of the buffer
	 * @param from     Receives the data source address
	 * @return         The amount of data actually received
	 */
	uint32_t recv(char *buf, uint32_t len, IPV4Address *from);

	/**
	 * Send data to a specific location
	 *
	 * @param data     Data to be sent out
	 * @param to       Address to send the data to
	 * @return         Amount of data actually written to socket
	 */
	uint32_t send(const std::string &data, IPV4Address to);

	//! If the socket is in listening state
	bool isListening() const { return m_listening; }
	//! If the socket has pending incoming data
	bool hasData()     const { return m_hasData; }

	/**
	 * \returns The local address of this socket
	 */
	IPV4Address getAddr() const { return m_addr; }

	/**
	 * @name Dummy methods, to allow generic programming
	 */
	//!@{
	bool isConnected()  const { return false; }
	bool isConnecting() const { return false; }
	//!@}
private:
	//! Destructor
	virtual ~UDPSocket();

	friend class SocketWatcher;
	bool m_listening;             //!< Indicates whether we are listening
	bool m_hasData;               //!< Indicates the presence of data
	bool m_erronous;              //!< Indicates erronous status
	IPV4Address m_addr;           //!< Local address

	HandlerType m_handler;      //!< Event handler for this socket
};

/**
 * SocketWatcher class keeps a list of active sockets, performs checking
 * them for events (in DoPoll() member function), as well as events posting
 * to Event Subsystem when there are events in sockets.
 */
class HNBASE_EXPORT SocketWatcher : public EventTableBase {
public:
	//! Initialize socket API
	static bool initialize();

	/**
	 * @name Static public accessors for adding/removing sockets
	 */
	//@{
	template<typename T>
	static void addSocket(T* socket) {
		instance().doAddSocket(socket);
	}
	template<typename T>
	static void removeSocket(T* socket) {
		instance().doRemoveSocket(socket);
	}
	//@}

	//! Perform the actual polling
	virtual void process();

	/**
	 * Cleans up sockets, removing all sockets pending removal from internal
	 * data structures.
	 */
	void cleanupSockets();

	/**
	 * Post an event to event queue
	 *
	 * @param obj     Source socket emitting the event
	 * @param evt     Event to be posted
	 */
	template<typename T>
	void postEvent(T *obj, SocketEvent evt);

	//! Access to the Singleton object of this class
	static SocketWatcher& instance();
private:
	SocketWatcher();
	~SocketWatcher();
	SocketWatcher(const SocketWatcher&);
	SocketWatcher& operator=(const SocketWatcher&);

	/**
	 * @name Implementation of accessors
	 */
	//@{
	void doAddSocket(SocketClient *client);
	void doAddSocket(SocketServer *socket);
	void doAddSocket(UDPSocket *socket);
	void doRemoveSocket(SocketClient *client);
	void doRemoveSocket(SocketServer *server);
	void doRemoveSocket(UDPSocket *socket);
	//@}

	std::set<SOCKET> m_sockets;                    //!< Set of sockets
	std::map<SOCKET, SocketClient*> m_clients;     //!< Map of clients
	std::map<SOCKET, SocketServer*> m_servers;     //!< Map of servers
	std::map<SOCKET, UDPSocket*   > m_udpSockets;  //!< Map of UDP sockets

	typedef std::set<SOCKET>::iterator SIter;
	typedef std::map<SOCKET, SocketClient*>::iterator SCIter;
	typedef std::map<SOCKET, SocketServer*>::iterator SSIter;
	typedef std::map<SOCKET, UDPSocket*   >::iterator SUIter;

	/**
	 * @name Internal helper functions for posting events.
	 */
	//@{
	void handleReadableSocket(SOCKET sock);
	void handleWritableSocket(SOCKET sock);
	void handleErronousSocket(SOCKET sock);
	//@}

	/**
	 * Temporary containers for sockets pending removal. These are cleared,
	 * and removed from real containers in cleanupSockets() method.
	 */
	//@{
	std::set<SocketClient*> m_clientsToRemove;
	std::set<SocketServer*> m_serversToRemove;
	std::set<UDPSocket*   > m_udpToRemove;
	//@}

	//! Event queue
	std::list<boost::function<void()> > m_events;
};

//! Useful things related to sockets
namespace Socket {
/**
 * Line ending object, similar to std::endl. When dealing with plain-text
 * protocols over sockets, \r\n is used to indicate newline.
 * Usage: <pre>
 * SocketClient *c;
 * *c << "Message" << Socket::Endl;
 * </pre>
 */
class HNBASE_EXPORT _Endl {
public:
	//! Output operator to Sockets
	operator std::string() {
		return "\r\n";
	}
	//! Comparison operator to check if string is line end
	inline friend bool operator==(const std::string &s, const _Endl&) {
		return s == "\r\n";
	}
};

//! output operator to any stream
inline std::ostream& operator<<(std::ostream &o, const _Endl &endl) {
	return o << "\r\n";
}

MSVC_ONLY(extern "C" {)
extern HNBASE_EXPORT _Endl Endl;
MSVC_ONLY(})

} //! Namespace Socket

#endif
