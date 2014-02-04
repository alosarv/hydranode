/*
 *  Copyright (C) 2004-2005 Alo Sarv <madcat_@users.sourceforge.net>
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
 * \file sockets.cpp Implementation of Hydranode Sockets API
 */

#include <hnbase/pch.h>
#include <hnbase/osdep.h>
#include <hnbase/sockets.h>
#include <hnbase/log.h>
#include <stdexcept>

#ifdef WIN32
	#include <winsock2.h>
	typedef SOCKADDR sockaddr;
	typedef int socklen_t;
#else
	#include <sys/socket.h>
	#include <sys/un.h>
	#include <fcntl.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <errno.h>
#endif

#ifndef MSG_NOSIGNAL
	#define MSG_NOSIGNAL 0
#endif

/**
 * Socket error codes
 * Define our internal socket error codes from platform-specific codes for
 * simplicity. If platform misses some of the errors - no prob, just assign
 * some negative value to it.
 */
#ifdef WIN32
	enum Socket_Errors {
		SOCK_EUNKNOWN       = 0,
		SOCK_EINETDOWN      = WSAENETDOWN,
		SOCK_ECONNREFUSED   = WSAECONNREFUSED,
		SOCK_EWOULDBLOCK    = WSAEWOULDBLOCK,
		SOCK_EWINSOCK       = -1, // WSANOTINITIALIZED
		SOCK_EINVALIDSOCKET = WSAENOTSOCK,
		SOCK_ECONNRESET     = WSAECONNRESET,
		SOCK_ECONNFAILED    = WSAECONNREFUSED,
		SOCK_EACCESS        = WSAEACCES,
		SOCK_EPERM          = -2,
		SOCK_EADDRINUSE     = WSAEADDRINUSE,
		SOCK_EADDRNOTAVAIL  = WSAEADDRNOTAVAIL,
		SOCK_EINVALIDPARAM  = WSAEFAULT,
		SOCK_EINPROGRESS    = WSAEINPROGRESS,
		SOCK_EALREADYBOUND  = WSAEINVAL,
		SOCK_ETOOMANYCONN   = WSAENOBUFS,
		SOCK_ENOTSOCK       = WSAENOTSOCK,
		SOCK_EAGAIN         = WSAEWOULDBLOCK
	};
#else
	enum Socket_Errors {
		SOCK_EUNKNOWN       = 0,
		SOCK_EINETDOWN      = -1,
		SOCK_ECONNREFUSED   = ECONNREFUSED,
		SOCK_EWOULDBLOCK    = EWOULDBLOCK,
		SOCK_EWINSOCK       = -2,
		SOCK_EINVALIDSOCKET = ENOTSOCK,
		SOCK_ECONNRESET     = ECONNRESET,
		SOCK_ECONNFAILED    = ECONNREFUSED,
		SOCK_EACCESS        = EACCES,
		SOCK_EPERM          = EPERM,
		SOCK_EADDRINUSE     = EADDRINUSE,
		SOCK_EADDRNOTAVAIL  = EADDRNOTAVAIL,
		SOCK_EINVALIDPARAM  = EINVAL,
		SOCK_EINPROGRESS    = EINPROGRESS,
		SOCK_EALREADYBOUND  = SOCK_EADDRINUSE,
		SOCK_ETOOMANYCONN   = EMFILE,
		SOCK_ENOTSOCK       = ENOTSOCK,
		SOCK_EAGAIN         = EAGAIN
	};
#endif

// Exception class constructor
SocketError::SocketError(const std::string &err) : std::runtime_error(err) {
}
// Exception class destructor
SocketError::~SocketError() throw() {
}

static void initSockets() {
#ifdef WIN32
	static bool initialized = false;
	if (!initialized) {
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2,2), &wsaData) != NO_ERROR) {
			throw SocketError("WinSock Init failed.");
		}
	}
#endif
}

static void setNonBlocking(SOCKET s) {
#ifdef WIN32
	int iMode = 1;
	ioctlsocket(s, FIONBIO, (u_long FAR*) &iMode);
#else
	fcntl(s, F_SETFL, O_NONBLOCK);
#endif
}

static void setReUsable(SOCKET s) {
#ifndef WIN32
	// Allow reusing addresses to avoid ::bind causing ADDR_IN_USE errors
	int yes = 1;
	int ret = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	if (ret == -1) {
		throw SocketError("SocketServer::SocketServer: setsockopt()");
	}
#else
	u_long tmp = 0;
	ioctlsocket(s, 0x9800000C, &tmp);
#endif
}

static std::string getErrorStr(int err) {
#ifdef WIN32
	switch (err) {
		case SOCK_EWINSOCK:
			return "WinSock error";
			break;
		case SOCK_EINETDOWN:
			return "Network connection is down";
			break;
		case SOCK_EACCESS:
			return "Access denied";
			break;
		case SOCK_EADDRINUSE:
			return "Address already in use";
			break;
		case SOCK_EADDRNOTAVAIL:
			return "Address not available";
			break;
		case SOCK_EINVALIDPARAM:
			return "Invalid parameter";
			break;
		case SOCK_EINPROGRESS:
			return "Operation in progress";
			break;
		case SOCK_EALREADYBOUND:
			return "Already bound";
			break;
		case SOCK_ETOOMANYCONN:
			return "Too many connections";
			break;
		case SOCK_ENOTSOCK:
			return "Not a socket";
			break;
		case SOCK_ECONNREFUSED:
			return "Connection refused";
			break;
		case SOCK_EWOULDBLOCK:
			return "Operation would block";
			break;
		case SOCK_EUNKNOWN:
			return "Unknown error";
			break;
		default:
			return "Unknown error";
			break;
	}
	return "";
#else
	return strerror(err);
#endif
}

static int getLastError() {
#ifdef WIN32
	return WSAGetLastError();
#else
	return errno;
#endif
}

static std::string socketError(const std::string &msg) {
	std::string ret = getErrorStr(getLastError());
	logDebug(boost::format("%s%s") % msg % ret);
	return ret;
}

#ifndef WIN32
	#define closesocket(socket) ::close(socket)
#endif

static struct Counter {
	Counter() : m_cnt() {}
	~Counter();
	void operator++() { ++m_cnt; ++m_created; }
	void operator--() { --m_cnt; ++m_destroyed; }
	uint32_t m_cnt;
	uint32_t m_created;
	uint32_t m_destroyed;
} s_counter;
Counter::~Counter() {
#ifndef NDEBUG
	if (m_cnt) {
		std::cerr << "*** " << m_cnt << " SocketBase objects leaked ";
		std::cerr << " (" << m_created << " created, ";
		std::cerr << m_destroyed << " destroyed) ***" << std::endl;
	}
#endif
}

// SocketBase class
// ----------------
SocketBase::SocketBase() : m_socket(0), m_toDelete(false), m_priority(PR_NORMAL)
{
	++s_counter;
}
SocketBase::SocketBase(SOCKET s) : m_socket(s), m_toDelete(false),
m_priority(PR_NORMAL) {
	++s_counter;
}
SocketBase::~SocketBase() {
	--s_counter;
}

// SocketClient class
// ------------------
#ifndef WIN32
static const int INVALID_SOCKET = -1;
static const int SOCKET_ERROR   = -1;
#endif

// Construct and initialize
SocketClient::SocketClient(SCEventHandler ehandler)
: m_connected(false), m_hasData(false), m_connecting(false), m_erronous(false),
m_writable(false), m_timeout(), m_handler(ehandler) {
	initSockets();

	logTrace(TRACE_SOCKET, "Creating SocketClient");
}

SocketClient::SocketClient(SOCKET s) : SocketBase(s), m_connected(false),
m_hasData(false), m_connecting(false), m_erronous(false), m_writable(false),
m_timeout() {}

// Destructor. Do only cleanup here
SocketClient::~SocketClient() {
}

void SocketClient::destroy() {
	close();
	m_toDelete = true;
}

void SocketClient::connect(IPV4Address addr, uint32_t timeout) {
	if (m_connected) {
		throw SocketError("connect(): Already connected.");
	} else if (m_erronous) {
		throw SocketError("connect(): Socket is erronous.");
	} else if (m_connecting) {
		throw SocketError("connect(): Already connecting.");
	} else if (addr.getPort() == 0) {
		throw SocketError("connect(): Cannot connect to port 0.");
	}

	logTrace(
		TRACE_SOCKET, 
		boost::format("SocketClient::connect(%s) timeout=%dms") 
		% addr % timeout
	);

	if (m_socket) {
		close();
	}

	m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_socket == INVALID_SOCKET) {
		throw SocketError(
			"SocketClient::SocketClient Error: Invalid socket."
		);
	}

	setNonBlocking(m_socket);
	setReUsable(m_socket);

	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = addr.getAddr();
	sin.sin_port = htons(addr.getPort());
	int ret = ::connect(m_socket, (sockaddr*)&sin, sizeof(sin));

	// Filter out non-fatal errors (e.g. related to nonblocking sockets
	// usage)
	if (
		ret == SOCKET_ERROR
		&& getLastError() != SOCK_EINPROGRESS // not fatal
		&& getLastError() != SOCK_EWOULDBLOCK // not fatal
	) {
		logTrace(TRACE_SOCKET,
			boost::format("Closing socket %s") % m_socket
		);
		boost::format fmt("connect(%s): ");
		fmt % addr;
		std::string err = socketError(fmt.str());

		if (closesocket(m_socket) != 0) {
			logWarning(socketError("Error closing socket: "));
		}
		throw SocketError(fmt.str() + err);
	}
	m_connecting = true;
	SocketWatcher::addSocket(this);

	if (timeout) {
		setTimeout(timeout);
	}

	m_peer = addr;
}

void SocketClient::close() {
	if (m_connecting || m_connected) {
		logTrace(TRACE_SOCKET,
			boost::format("Closing socket %s") % m_socket
		);
		if (m_socket && closesocket(m_socket) != 0) {
			logWarning(socketError("Error closing socket: "));
		}
	}
	SocketWatcher::removeSocket(this);
	m_connecting = false;
	m_connected = false;
	m_timeout = 0;
}

// Disconnect
void SocketClient::disconnect() {
	logTrace(TRACE_SOCKET, "Disconnecting SocketClient.");
	close();
	m_peer = IPV4Address();     //!< Reset peer
}

// Read data from socket
uint32_t SocketClient::read(void *buffer, uint32_t length) {
	if (!m_connected) {
		throw SocketError(
			"Attempt to read from a disconnected socket."
		);
	} else if (m_connecting) {
		throw SocketError("Attempt to read from a connecting socket.");
	} else if (m_erronous) {
		throw SocketError("Attempt to read from an erronous socket.");
	}
	int ret = ::recv(
		m_socket, reinterpret_cast<char*>(buffer), length, MSG_NOSIGNAL
	);
	m_hasData = false;
	if (ret == SOCKET_ERROR) {
		ret = 0;
		if (getLastError() != SOCK_EAGAIN) {
			close();
			m_erronous = true;
			m_connected = false;
			m_connecting = false;
			SocketWatcher::instance().postEvent(this, SOCK_LOST);
		}
	}
	return ret;
}

// Write data to socket
uint32_t SocketClient::write(const char *buffer, uint32_t length) {
	if (!m_connected) {
		throw SocketError("Attempt to write to a disconnected socket.");
	} else if (m_connecting) {
		throw SocketError("Attempt to write to a connecting socket.");
	} else if (m_erronous) {
		throw SocketError("Attempt to write to an erronous socket.");
	}
	int ret = ::send(m_socket, buffer, length, MSG_NOSIGNAL);
	m_writable = false;
	if (ret == SOCKET_ERROR) {
		ret = 0;
		if (getLastError() != SOCK_EAGAIN) {
			close();
			m_erronous = true;
			m_connected = false;
			m_connecting = false;
			SocketWatcher::instance().postEvent(this, SOCK_LOST);
		}
	}
	return ret;
}

IPV4Address SocketClient::getAddr() const {
	sockaddr_in name;
	socklen_t sz = sizeof(name);

	int rv = getsockname(m_socket, reinterpret_cast<sockaddr*>(&name), &sz);

	if (rv != SOCKET_ERROR) {
		return IPV4Address(name.sin_addr.s_addr, name.sin_port);
	} else {
		return IPV4Address();
	}
}

void SocketClient::setTcpRecvBufSize(uint32_t size) {
	CHECK_THROW(m_socket > 0);
	int ret = setsockopt(
		m_socket, SOL_SOCKET, SO_RCVBUF, (char*)&size, 4
	);
	CHECK_THROW(ret == 0);
}

void SocketClient::setTcpSendBufSize(uint32_t size) {
	CHECK_THROW(m_socket > 0);
	int ret = setsockopt(
		m_socket, SOL_SOCKET, SO_SNDBUF, (char*)&size, 4
	);
	CHECK_THROW(ret == 0);
}

uint32_t SocketClient::getTcpRecvBufSize() const {
	CHECK_THROW(m_socket > 0);
	uint32_t size = 0;
	socklen_t len = sizeof(size);
	int ret = getsockopt(
		m_socket, SOL_SOCKET, SO_RCVBUF, (char*)&size, &len
	);
	CHECK_THROW(ret == 0);
	return size;
}

uint32_t SocketClient::getTcpSendBufSize() const {
	CHECK_THROW(m_socket > 0);
	uint32_t size = 0;
	socklen_t len = sizeof(size);
	int ret = getsockopt(
		m_socket, SOL_SOCKET, SO_RCVBUF, (char*)&size, &len
	);
	CHECK_THROW(ret == 0);
	return size;
}

// SocketServer class
// ------------------

// Constructor
SocketServer::SocketServer(SSEventHandler ehandler) : m_incoming(false),
m_listening(false), m_erronous(false), m_handler(ehandler) {
	initSockets();

	logTrace(TRACE_SOCKET, "Constructing SocketServer");
}

// Destructor
SocketServer::~SocketServer() {
	logTrace(TRACE_SOCKET, "Destroying SocketServer");
}

void SocketServer::destroy() {
	if (m_listening) {
		close();
		m_toDelete = true;
	} else if (!m_toDelete) {
		delete this;
	}
}

void SocketServer::close() {
	if (m_listening) {
		logTrace(TRACE_SOCKET,
			boost::format("Closing socket %s") % m_socket
		);
		if (closesocket(m_socket) != 0) {
			logWarning(socketError("Error closing socket: "));
		}
		SocketWatcher::removeSocket(this);
	}
	m_listening = false;
}

// Set socket to listening state.
void SocketServer::listen(IPV4Address addr) {
	if (m_listening) {
		throw SocketError("listen(): Already listening.");
	} else if (m_erronous) {
		throw SocketError("listen(): Socket is erronous.");
	} else if (addr.getPort() == 0) {
		throw SocketError("listen(): Cannot listen on port 0.");
	}
	logTrace(TRACE_SOCKET,
		boost::format("Starting listener on port %d") % addr.getPort()
	);

	m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_socket == INVALID_SOCKET) {
		throw SocketError(
			"SocketServer::SocketServer Error: Invalid socket."
		);
	}

	setNonBlocking(m_socket);
	setReUsable(m_socket);

	sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	if (addr.getAddr()) {
		sin.sin_addr.s_addr = addr.getAddr();
	} else {
		sin.sin_addr.s_addr = INADDR_ANY;
	}
	sin.sin_port = htons(addr.getPort());
	int retval = ::bind(
		m_socket, reinterpret_cast<sockaddr*>(&sin), sizeof(sin)
	);
	if (retval == SOCKET_ERROR) {
		throw SocketError(socketError("bind(): "));
	}

	if (::listen(m_socket, SOMAXCONN) == SOCKET_ERROR) {
		throw SocketError(socketError("listen(): "));
	}
	m_listening = true;
	SocketWatcher::addSocket(this);

	// Set m_addr
	m_addr = addr;
	if (m_addr.getAddr() == 0) {
		// This means we are listening on any available networks.
		struct sockaddr name;
		socklen_t sz = sizeof(name);
		if (getsockname(m_socket, &name, &sz) == SOCKET_ERROR) {
			// Ok, not fatal, just log an error
			logDebug("getsockname() failed!");
		} else {
			m_addr.setAddr(name.sa_data);
		}
	}
}

// accept an incoming connection, using handler to assign to new connections
// events
SocketClient* SocketServer::accept(SCEventHandler ehandler) {
	if (!m_listening) {
		throw SocketError("accept(): Not listening.");
	} else if (!m_incoming) {
		throw SocketError("accept(): No incoming connections.");
	} else if (m_erronous) {
		throw SocketError("accept(): Socket is erronous.");
	}

	struct sockaddr_in addr;
	socklen_t sz = sizeof(addr);
	SOCKET sock = ::accept(
		m_socket, reinterpret_cast<sockaddr*>(&addr), &sz
	);

	if (sock == INVALID_SOCKET) {
		throw SocketError(socketError("accept(): "));
	}

	SocketClient *client = new SocketClient(sock);
	client->m_peer = IPV4Address(addr.sin_addr.s_addr, 0);
	client->m_connected = true;
	setNonBlocking(client->m_socket);
	SocketWatcher::addSocket(client);

	client->setHandler(ehandler);
	m_incoming = false;

	return client;
}

// UDPSocket class
// ---------------
UDPSocket::UDPSocket(UDPSEventHandler handler) : m_listening(false),
m_hasData(false), m_erronous(false), m_handler(handler) {
	logTrace(TRACE_SOCKET, "Constructing UDPSocket.");

	initSockets();

	m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (m_socket == INVALID_SOCKET) {
		throw SocketError("UDPSocket::UDPSocket: Invalid socket.");
	}

	setNonBlocking(m_socket);
	setReUsable(m_socket);
}

UDPSocket::~UDPSocket() {
}

void UDPSocket::destroy() {
	if (m_listening) {
		if (closesocket(m_socket) != 0) {
			logDebug(socketError("Error closing socket: "));
		}
		SocketWatcher::removeSocket(this);
		m_listening = false;
		m_toDelete = true;
	} else if (!m_toDelete) {
		delete this;
	}
}

void UDPSocket::setBroadcast(bool val) {
#ifdef _WIN32
	char newVal = val ? 1 : 0;
#else
	int newVal = val ? 1 : 0;
#endif
	int ret = setsockopt(
		m_socket, SOL_SOCKET, SO_BROADCAST, &newVal, sizeof(newVal)
	);
	if (ret == SOCKET_ERROR) {
		logDebug(socketError("setsockopt"));
	}
}

void UDPSocket::listen(IPV4Address addr) {
	CHECK_THROW_MSG(addr.getPort(), "Listening port may not be NULL");
	logTrace(TRACE_SOCKET,
		boost::format("Starting UDP listener at port %d")
		% addr.getPort()
	);

	sockaddr_in sin;
	sin.sin_family = AF_INET;
	if (addr.getAddr()) {
		sin.sin_addr.s_addr = addr.getAddr();
	} else {
		sin.sin_addr.s_addr = INADDR_ANY;
	}
	sin.sin_port = htons(addr.getPort());

	int retval = ::bind(m_socket, (sockaddr*)&sin, sizeof(sin));

	// Set m_addr
	m_addr = addr;
	if (m_addr.getAddr() == 0) {
		// This means we are listening on any available networks.
		struct sockaddr name;
		socklen_t sz = sizeof(name);
		if (getsockname(m_socket, &name, &sz) == SOCKET_ERROR) {
			// Ok, not fatal, just log an error
			logDebug("getsockname() failed!");
		} else {
			m_addr.setAddr(name.sa_data);
		}
	}

	if (retval == SOCKET_ERROR) {
		m_erronous = true;
		throw SocketError(socketError("bind(): "));
	}
	m_listening = true;
	SocketWatcher::addSocket(this);
}

uint32_t UDPSocket::recv(char *buf, uint32_t len, IPV4Address *from) {
	CHECK_THROW_MSG(m_listening, "Socket must listen before receiving.");
	CHECK_THROW_MSG(m_hasData, "Socket must have data before receiving.");
	CHECK_THROW_MSG(buf, "Cowardly refusing to recv into NULL buffer.");
	CHECK_THROW_MSG(len, "Cowardly refusing to receive NULL bytes.");
	CHECK_THROW_MSG(from,"Cowardly refusing to receive into NULL address.");

	sockaddr_in sin;
	socklen_t fromlen = sizeof(sin);

	memset(&sin, 0, sizeof(sin));

	int ret = recvfrom(
		m_socket, buf, len, MSG_NOSIGNAL,
		reinterpret_cast<sockaddr*>(&sin), &fromlen
	);

	if (ret == SOCKET_ERROR) {
		m_hasData = false;
		if (getLastError() != SOCK_EAGAIN) {
			throw SocketError(socketError("recvfrom(): "));
		} else {
			return 0;
		}
	}

	from->setPort(ntohs(sin.sin_port));
	from->setAddr(sin.sin_addr.s_addr);

	CHECK(ret >= 0);

	int ret2 = recvfrom(
		m_socket, reinterpret_cast<char*>(0), 0, MSG_PEEK,
		reinterpret_cast<sockaddr*>(0), 0
	);
	if (ret2 <= 0) {
		m_hasData = false;
	}

	return ret;
}

uint32_t UDPSocket::send(const std::string &data, IPV4Address to) {
	CHECK_THROW_MSG(to.getPort(), "Can't send to port NULL!");
	CHECK_THROW_MSG(to.getAddr(), "Can't send to NULL!");
	CHECK_THROW_MSG(data.size(), "Cowardly refusing to send NULL buffer.");

	sockaddr_in sin;
	memset(&sin, 0, sizeof(sin));

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = to.getAddr();
	sin.sin_port = htons(to.getPort());

	logTrace(TRACE_SOCKET,
		boost::format("UDPSocket: to.Addr=%s sin.addr=%d sin.port=%d")
		% to % sin.sin_addr.s_addr % sin.sin_port
	);

	int ret = sendto(
		m_socket, data.data(), data.size(), MSG_NOSIGNAL,
		reinterpret_cast<sockaddr*>(&sin), sizeof(sin)
	);

	if (ret == SOCKET_ERROR) {
		switch (getLastError()) {
			case SOCK_EINVALIDPARAM:
			case SOCK_EACCESS:
			case SOCK_EPERM: {
				boost::format f("UDPSocket(%s): sendto(%s): ");
				f % getAddr() % to;
				socketError(f.str());
				break;
			}
			default:
				throw SocketError(socketError("sendto(): "));
				break;
		}
	}

	return ret;
}

// SocketWatcher class - Performs sockets polling and events dispatching.
// -------------------
// constructors/destructors
SocketWatcher::SocketWatcher() {
//	Log::instance().enableTraceMask(TRACE_SOCKET, "socket");
}
SocketWatcher::~SocketWatcher() {}

// Initialize socket API in platform-dependant way
// This function is called during each socket construction. The only platforms
// that are known to require explicit socket API initialization is win32.
bool SocketWatcher::initialize() {
#ifdef WIN32
	static bool initialized = false;
	if (initialized) {
		return true;
	}
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != NO_ERROR) {
		return false;
	}
	return initialized = true;
#else
	return true;
#endif
}

SocketWatcher& SocketWatcher::instance() {
	static SocketWatcher s;
	return s;
}

// Add a socket for polling
void SocketWatcher::doAddSocket(SocketServer *socket) {
	CHECK_THROW(socket != 0);
	logTrace(
		TRACE_SOCKET, boost::format("SocketWatcher: Adding socket %d")
		% socket->getSocket()
	);
	m_servers[socket->getSocket()] = socket;
	m_sockets.insert(socket->getSocket());
}

// Add a socket for polling
void SocketWatcher::doAddSocket(SocketClient *socket) {
	CHECK_THROW(socket != 0);
	logTrace(
		TRACE_SOCKET, boost::format("SocketWatcher: Adding socket %d")
		% socket->getSocket()
	);
	m_clients[socket->getSocket()] = socket;
	m_sockets.insert(socket->getSocket());
}

void SocketWatcher::doAddSocket(UDPSocket *socket) {
	CHECK_THROW(socket != 0);
	logTrace(
		TRACE_SOCKET,
		boost::format("SocketWatcher: Adding UDP socket %d")
		% socket->getSocket()
	);
	m_udpSockets[socket->getSocket()] = socket;
	m_sockets.insert(socket->getSocket());
}

// Remove a socket from polled sockets list
void SocketWatcher::doRemoveSocket(SocketServer *socket) {
	CHECK_THROW(socket);
	m_serversToRemove.insert(socket);
}
void SocketWatcher::doRemoveSocket(SocketClient *socket) {
	CHECK_THROW(socket);
	m_clientsToRemove.insert(socket);
}
void SocketWatcher::doRemoveSocket(UDPSocket *socket) {
	CHECK_THROW(socket);
	m_udpToRemove.insert(socket);
}
void SocketWatcher::cleanupSockets() {
	uint32_t serversRemoved = 0;
	uint32_t clientsRemoved = 0;
	uint32_t udpRemoved = 0;
	std::set<SocketServer*>::iterator it(m_serversToRemove.begin());
	while (it != m_serversToRemove.end()) {
		SocketServer *toRemove = *it;
		SSIter i = m_servers.find(toRemove->getSocket());
		if (i != m_servers.end() && (*i).second == toRemove) {
			m_servers.erase(i);
			m_sockets.erase(toRemove->getSocket());
		}
		if (toRemove->toDelete()) {
			delete toRemove;
		}
		++it;
		++serversRemoved;
	}
	std::set<SocketClient*>::iterator it2(m_clientsToRemove.begin());
	while (it2 != m_clientsToRemove.end()) {
		SocketClient *toRemove = *it2;
		SCIter i = m_clients.find(toRemove->getSocket());
		if (i != m_clients.end() && (*i).second == toRemove) {
			m_clients.erase(i);
			m_sockets.erase(toRemove->getSocket());
		}
		if (toRemove->toDelete()) {
			delete toRemove;
		}
		++it2;
		++clientsRemoved;
	}
	std::set<UDPSocket*>::iterator it3(m_udpToRemove.begin());
	while (it3 != m_udpToRemove.end()) {
		UDPSocket *toRemove = *it3;
		SUIter i = m_udpSockets.find(toRemove->getSocket());
		if (i != m_udpSockets.end() && (*i).second == toRemove) {
			m_udpSockets.erase(i);
			m_sockets.erase(toRemove->getSocket());
		}
		if (toRemove->toDelete()) {
			delete toRemove;
		}
		++it3;
		++udpRemoved;
	}
	if (serversRemoved || clientsRemoved || udpRemoved) {
		logTrace(TRACE_SOCKET, boost::format(
			"SocketWatcher: Cleaning sockets. Removed %d TCP "
			"Clients, %d TCP Servers and %d UDP sockets."
		) % clientsRemoved % serversRemoved % udpRemoved);
	}
	m_clientsToRemove.clear();
	m_serversToRemove.clear();
	m_udpToRemove.clear();
}

// Poll listed sockets
//
// We only add sockets to the sets for which we are certain that the select()
// operation wouldn't return immediately, e.g. if a socket already has incoming
// data, we don't add it to the sets, since that would cause select() to return
// instantly. The main reason for this is safety - if client doesn't read the
// data out of a readable socket, or accept an incoming connection, we would
// detect the socket readable again in next select(), re-post the event etc.
// Uff. Better safe than sorry - once the data has been read out, the
// SocketClient/SocketServer re-enable the flags so we'll start polling it
// again.
void SocketWatcher::process() {
	fd_set rfds;
	fd_set wfds;
	fd_set efds;
	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_ZERO(&efds);
	uint64_t curTick = EventMain::instance().getTick();

	cleanupSockets();

	// Temporary variable to keep track of highest-numbered socket,
	// needed later for passing to select().
	SOCKET highest = 0;

	// Add all servers. Note: Servers can't become writable.
	for (SSIter i = m_servers.begin(); i != m_servers.end(); ++i) {
		// Only add the ones which don't have any incoming pending
		if (!(*i).second->m_incoming) {
			FD_SET((*i).first, &rfds);
		}
		// Only add those which are not erronous
		if (!(*i).second->m_erronous) {
			FD_SET((*i).first, &efds);
		}
		if ((*i).first > highest) {
			highest = (*i).first;
		}
	}
	// Add all clients
	for (SCIter i = m_clients.begin(); i != m_clients.end(); ++i) {
		SocketClient *c = (*i).second;
		if (c->m_timeout && c->m_timeout < curTick) {
			// Timeout is over
			postEvent(c, SOCK_TIMEOUT);
			c->close();
			c->m_timeout = 0;
			continue;
		}

		// Only add the ones which don't have incoming data and are
		// connected
		if (!c->m_hasData && c->m_connected) {
			FD_SET((*i).first, &rfds);
		}
		// Only add the ones which are in connecting/connected
		// state
		if ((c->m_connecting || c->m_connected) && !c->m_writable) {
			FD_SET((*i).first, &wfds);
		}
		// Only add the ones that aren't erronous already
		if (!c->m_erronous && c->m_connected) {
			FD_SET((*i).first, &efds);
		}
		if ((*i).first > highest) {
			highest = (*i).first;
		}
	}

	// Add all UDP sockets
	for (SUIter i = m_udpSockets.begin(); i != m_udpSockets.end(); ++i) {
		if (!(*i).second->m_hasData) {
			FD_SET((*i).first, &rfds);
		}
		if (!(*i).second->m_erronous) {
			FD_SET((*i).first, &efds);
		}
		if ((*i).first > highest) {
			highest = (*i).first;
		}
	}

	cleanupSockets();

	timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 50000;

#ifdef WIN32
	Sleep(50);
#else
	usleep(50000);
#endif

	int ret = select(highest + 1, &rfds, &wfds, &efds, &tv);
	if (ret > 0) {
#ifdef WIN32
		for (unsigned int i = 0; i < rfds.fd_count; i++) {
			handleReadableSocket(rfds.fd_array[i]);
		}
		for (unsigned int i = 0; i < wfds.fd_count; i++) {
			handleWritableSocket(wfds.fd_array[i]);
		}
		for (unsigned int i = 0; i < efds.fd_count; i++) {
			handleErronousSocket(efds.fd_array[i]);
		}
#else
		for (SIter i = m_sockets.begin(); i != m_sockets.end(); i++) {
			if (FD_ISSET(*i, &rfds)) {
				handleReadableSocket(*i);
			} else if (FD_ISSET(*i, &wfds)) {
				handleWritableSocket(*i);
			} else if (FD_ISSET(*i, &efds)) {
				handleErronousSocket(*i);
			}
		}
#endif
	} else if (ret == SOCKET_ERROR) {
		socketError("select(): ");
	}

	// emit events
	while (m_events.size()) {
		m_events.front()();
		m_events.pop_front();
	}

	cleanupSockets();
}

// Socket being readable may mean a number of things:
// * If its a server, there might be an incoming connection
// * If its a connected stream socket, there may be incoming data
// * If its a connected stream socket, the connection may have been closed
void SocketWatcher::handleReadableSocket(SOCKET sock) {
	logTrace(TRACE_SOCKET, boost::format("Socket %d is readable.") % sock);

	// First determine who governs the socket. Need two map lookups here.
	SCIter i = m_clients.find(sock);
	if (i == m_clients.end()) {
		SSIter j = m_servers.find(sock);
		if (j == m_servers.end()) {
			SUIter k = m_udpSockets.find(sock);
			if (k == m_udpSockets.end()) {
				// This should never happen - we have a SOCKET
				// on record w/o governing SocketBase object!
				logDebug("Ungoverned SOCKET in SocketWatcher!");
				return;
			}
			UDPSocket *s = (*k).second;
			s->m_hasData = true;
			logTrace(TRACE_SOCKET, "UDP socket is readable.");
			postEvent(k->second, SOCK_READ);
			return;
		}
		// Here we have determined that its a server and we have
		// the pointer to the SocketServer object available
		SocketServer *server = j->second;
		server->m_incoming = true;
		logTrace(TRACE_SOCKET, "TCP Server is readable.");
		postEvent(server, SOCK_ACCEPT);
		return;
	}
	// We get here if the SOCKET was found in clients list - thus we
	// have pointer to the SocketClient object governing the SOCKET
	SocketClient *client = i->second;
	client->m_timeout = 0;

	// Peek at the data to determine if its simply readable or the
	// connection has been closed
	char buf[1];
	int retval = ::recv(sock, buf, 1, MSG_PEEK|MSG_NOSIGNAL);
	if (retval == 0) {
		// Connection has been closed
		client->close();
		client->m_connected = false;
		client->m_connecting = false;
		logTrace(TRACE_SOCKET,
			boost::format("%p: TCP Client is lost.") % client
		);
		postEvent(client, SOCK_LOST);
	} else if (
		retval == SOCKET_ERROR
		&& getLastError() != SOCK_EINPROGRESS // not fatal
		&& getLastError() != SOCK_EWOULDBLOCK // not fatal
	) {
		// Socket error has occoured
		client->m_erronous = true;
		client->close();
		logTrace(TRACE_SOCKET,
			boost::format("%p: TCP Client is erroneous: %s")
			% client % getErrorStr(getLastError())
		);
		postEvent(client, SOCK_ERR);
	} else {
		// There's data available for reading
		client->m_hasData = true;
		logTrace(TRACE_SOCKET,
			boost::format("%p: TCP Client has data.") % client
		);
		postEvent(client, SOCK_READ);
	}
}

// Socket becoming writable means a connect() call has completed or a socket
// with queued data became (again) writable
void SocketWatcher::handleWritableSocket(SOCKET sock) {
	logTrace(TRACE_SOCKET, boost::format("Socket %d is writable.") % sock);

	SCIter i = m_clients.find(sock);
	if (i == m_clients.end()) {
		// This should never happen - we have ungoverned SOCKET
		// listed.
		logDebug("Ungoverned SOCKET in SocketWatcher!");
		return;
	}
	SocketClient *client = (*i).second;
	client->m_timeout = 0;
	if (client->m_connecting && !client->m_connected) {
#ifndef WIN32
		// Check if the connection attempt succeeded or failed. On win32
		// this notificaiton is done via exceptfds, however on posix
		// we must check the flags ourselves.
		int val = 0;
		socklen_t sz = sizeof(int);
		int ret = getsockopt(sock, SOL_SOCKET, SO_ERROR, &val, &sz);
		if (ret == SOCKET_ERROR) {
			// Doh ?
			perror("getsockopt()");
		}
		if (val == 0) {
			client->m_connecting = false;
			client->m_connected = true;
			postEvent(client, SOCK_CONNECTED);
		} else {
			logTrace(TRACE_SOCKET,
				boost::format("Socket error: %d") %strerror(val)
			);
			client->close();
			client->m_connecting = false;
			client->m_connected = false;
			postEvent(client, SOCK_CONNFAILED);
		}
#else
		client->m_connecting = false;
		client->m_connected = true;
		postEvent(client, SOCK_CONNECTED);
#endif
	} else if (client->m_connected && !client->m_writable) {
		client->m_writable = true;
		postEvent(client, SOCK_WRITE);
	}
	return;
}

// Actually, these are not (as the rumors say), erronous sockets. These are
// sockets which have OOB data in them. refer to `man 2 select_tut` for more
// information.
void SocketWatcher::handleErronousSocket(SOCKET sock) {
	logTrace(TRACE_SOCKET, boost::format("Socket %d is erronous.") % sock);

	SCIter i = m_clients.find(sock);
	if (i == m_clients.end()) {
		SSIter j = m_servers.find(sock);
		if (j == m_servers.end()) {
			// This is a serious error - see similar places in
			// previous functions
			logDebug("Ungoverned SOCKET in SocketWatcher!");
			return;
		}
		// Server has become erronous
		SocketServer *server = j->second;
		server->close();
		server->m_listening = false;
		server->m_erronous = true;
		postEvent(server, SOCK_LOST);
	} else {
		// Client has become erronous
		SocketClient *client = i->second;
		logTrace(TRACE_SOCKET,
			boost::format("%p: TCP socket became erronous") % client
		);
		client->m_timeout = 0;
		client->close();
		client->m_connected = false;
		client->m_connecting = false;
		client->m_erronous = true;
		postEvent(client, SOCK_LOST);
	}
}

template<typename T>
void SocketWatcher::postEvent(T *src, SocketEvent evt) {
	if (src->m_handler) {
		m_events.push_back(boost::bind(src->m_handler, src, evt));
	}
}

namespace Socket {
	//! Implement EOL symbol
	_Endl Endl;
}
