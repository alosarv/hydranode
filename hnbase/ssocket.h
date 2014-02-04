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
 * \file ssocket.h Interface for SSocket class
 */

#ifndef __SSOCKET_H__
#define __SSOCKET_H__

#include <hnbase/fwd.h>                // forward declarations
#include <hnbase/scheduler.h>          // the scheduler itself
#include <boost/bind.hpp>              // function object binding

/**
 * SSocket template represents a socket that can be serve as communication
 * medium between two remote parties. The exact implementation is chosen
 * based on the template parameters.
 *
 * @param Module        Required, the module governing this socket.
 * @param Type          Type of the socket. Can be either Client or Server
 * @param Protocol      Protocol to be used in the socket, e.g. TCP or UDP
 *
 * \note This class acts as a Facade for the Scheduler engine. All public
 *        member function calls of this class are forwarded to the respective
 *        underlying scheduler classes.
 */
template<
	typename Module,
	typename Type,
	typename Protocol,/* Socket::TCP */
	typename Impl     /* typename Socket::Implement<Type, Protocol>::Impl */
> class SSocket : public Trackable {
public:
	typedef typename Impl::PriorityType PriorityType;
	typedef typename Impl::EventType EventType;
	typedef Impl* ImplPtr;
	typedef boost::function<void (SSocket*, EventType)> HandlerFunc;
	typedef Scheduler<Impl> _Scheduler;
	typedef typename _Scheduler::SSocketWrapperPtr SSocketWrapperPtr;

	/**
	 * Construct and initialize, optionally setting event handler
	 *
	 * @param h      Optional event handler
	 */
	SSocket(HandlerFunc h = 0) : m_impl(new Impl), m_handler(h) {
		m_ptr = _Scheduler::template addSocket<Module>(
			m_impl, boost::bind(&SSocket::onEvent, this, _1, _2)
		);
	}

	/**
	 * Convenience constructor - performs event handler functor binding
	 * internally.
	 *
	 * @param obj      Object to receive event notifications
	 * @param func     Function to receive event notifications
	 */
	template<typename T>
	SSocket(T *obj, void (T::*func)(SSocket*, EventType))
	: m_impl(new Impl), m_handler(boost::bind(func, obj, _1, _2)) {
		m_ptr = _Scheduler::template addSocket<Module>(
			m_impl, boost::bind(&SSocket::onEvent, this, _1, _2)
		);
	}

	/**
	 * Constructer used only internally during incoming connections
	 * accepting.
	 *
	 * @param s        New socket
	 */
	SSocket(typename Impl::AcceptType *s) : m_impl(s) {
		m_ptr = _Scheduler::template addSocket<Module>(
			s, boost::bind(&SSocket::onEvent, this, _1, _2)
		);
	}

	//! Destructor
	~SSocket() {
		_Scheduler::template delSocket<Module>(m_ptr);
		m_impl->destroy();
	}

	/**
	 * @name Input/Output
	 */
	//@{

	/**
	 * Write data into socket
	 *
	 * @param buf   Data to be written
	 */
	void write(const std::string &buf) {
		_Scheduler::write(m_ptr, buf);
	}

	/**
	 * Read data from socket
	 *
	 * @param buf   Buffer to read data into. The data is appended to the
	 *              specified string.
	 */
	void read(std::string *buf) {
		_Scheduler::read(m_ptr, buf);
	}

	/**
	 * A more optimized version of read() method, this returns the incoming
	 * data buffer directly. The different between getData() and read()
	 * methods is that in case of read(), the data is appended to the
	 * buffer, which means memcpy operation, however, with this method, only
	 * std::string copy-constructor is called, and majority of
	 * implementations have that optimized to avoid data copying. So this
	 * method should be prefered over read() method.
	 *
	 * @return     All data that has been received thus far.
	 *
	 * \note If you ignore the return value, the data is effectivly lost,
	 *       since scheduler removes the data from it's internal buffer.
	 */
	std::string getData() {
		return _Scheduler::getData(m_ptr);
	}

	/**
	 * Perform an outgoing connection
	 *
	 * @param addr      Address to connect to
	 * @param timeout   Optional timeout for connection attempt. Defaults
	 *                  to 5 seconds.
	 */
	void connect(IPV4Address addr, uint32_t timeout = 5000) {
		_Scheduler::connect(m_ptr, addr, timeout);
	}

	/**
	 * Perform an outgoing connection
	 *
	 * @param hostname  Hostname to connect to
	 * @param port      Port to connect to
	 * @param timeout   Optional timeout for connection attempt. Defaults
	 *                  to 5 seconds.
	 */
	void connect(
		const std::string &hostname,
		uint16_t port, uint32_t timeout = 5000
	) {
		_Scheduler::connect(m_ptr, hostname, port, timeout);
	}

	/**
	 * Disconnect a connected socket. If the socket is not connected, this
	 * function does nothing. Note that sockets are automatically
	 * disconnected when they are destroyed.
	 *
	 * @param lazy     If true, the actual disconnection is delayed until
	 *                 pending data has been sent out.
	 */
	void disconnect() {
		_Scheduler::disconnect(m_ptr);
	}

	/**
	 * Start a listener, waiting for incoming connections
	 *
	 * @param addr     Local address to listen on. If addr.ip is set to
	 *                 0, connections are accepted from all networks,
	 *                 otherwise connections are only accepted from the
	 *                 designated net. For example, if ip is 127.0.0.1,
	 *                 only loopback connections are accepted.
	 */
	void listen(IPV4Address addr) {
		m_impl->listen(addr);
	}

	/**
	 * Convenience method - construct IPV4Address internally
	 *
	 * @param ip       Ip to listen on
	 * @param port     Port to listen on
	 */
	void listen(uint32_t ip, uint16_t port) {
		listen(IPV4Address(ip, port));
	}

	/**
	 * Accept an incoming connection.
	 *
	 * @return         New socket, which is in connected state, ready to
	 *                 receive and transmit data. The return type depends
	 *                 on the underlying implementation. The returned socket
	 *                 is created in same module as the listening socket.
	 *
	 * \throws if there was no incoming connection pending at this moment.
	 */
	SSocket<Module, Socket::Client, Protocol>* accept() {
		return new SSocket<Module, Socket::Client, Protocol>(
			_Scheduler::accept(m_ptr)
		);
	}

	/**
	 * Send data to specific address. This applies only to UDP sockets.
	 *
	 * @param to       Address to send data to
	 * @param buf      Buffer containing the data to be sent
	 */
	void send(IPV4Address to, const std::string &buf) {
		_Scheduler::send(to, buf);
	}

	/**
	 * Receive data from designated address. This applies only to UDP type
	 * sockets.
	 *
	 * @param buf       Buffer to write the retrieved data to. The data is
	 *                  appended to the designated string.
	 * @param from      Will receive the data source address
	 */
	void recv(IPV4Address *from, std::string *buf) {
		_Scheduler::recv(from, buf);
	}
	//@}

	/**
	 * Set socket timeout. If no events happen before the timeout is over,
	 * the socket is closed and EVT_TIMEOUT posted.
	 *
	 * @param t   Timeout in milliseconds
	 */
	void setTimeout(uint32_t t) {
		m_impl->setTimeout(t);
	}

	/**
	 * @returns The current upload speed of the socket
	 */
	uint32_t getUpSpeed() const {
		return _Scheduler::getUpSpeed(m_ptr);
	}

	/**
	 * @returns The current download speed of the socket
	 */
	uint32_t getDownSpeed() const {
		return _Scheduler::getDownSpeed(m_ptr);
	}

	//! @returns Total bytes uploaded to this socket
	uint64_t getUploaded() const {
		return _Scheduler::getUploaded(m_ptr);
	}
	//! @returns Total bytes downloaded from this socket
	uint64_t getDownloaded() const {
		return _Scheduler::getDownloaded(m_ptr);
	}

	/**
	 * @name Event handling
	 */
	//@{
	//! Set event handler, overwriting old handler
	void        setHandler(HandlerFunc handler) { m_handler = handler; }
	//! Set the event handler, performing functor binding internally
	template<typename T>
	void setHandler(T *obj, void (T::*func)(SSocket*, EventType)) {
		m_handler = boost::bind(func, obj, _1, _2);
	}
	//! Retrieve the handler function object
	HandlerFunc getHandler() const              { return m_handler;    }
	//! Clear the existing event handler
	void        clearHandler()                  { m_handler.clear();   }
	//@}

	//! Set this sockets priority
	void setPriority(PriorityType prio) { m_impl->setPriority(prio); }

	/**
	 * @name Queries
	 */
	//@{
	bool isConnected()         const { return m_impl->isConnected();  }
	bool isOk()                const { return m_impl->isOk();         }
	bool isConnecting()        const { return m_impl->isConnecting(); }
	/**
	 * \returns true if there is no currently buffered outgoing data for
	 *          this socket.
	 * \note This has no effect on whether or not you can actually write()
	 *       data to this socket, since all data is always accepted and
	 *       buffered. This function is mostly useful when you want to know
	 *       if there currently is still outgoing data left in the socket,
	 *       and thus whether or not to expect SOCK_WRITE event in near
	 *       future (emitted when underlying socket becomes writable and
	 *       there's no more buffered data).
	 */
	bool isWritable()          const { return m_ptr->isWritable();    }
	/**
	 * \returns true if there is currently buffered incoming data in the
	 *          socket.
	 */
	bool isReadable()          const { return m_ptr->isReadable();    }
	/**
	 * \returns true if this socket is listening (only works with servers)
	 */
	bool isListening()         const { return m_impl->isListening();  }
	/**
	 * \returns true if this socket has incoming connections (only works
	 *          with tcp servers)
	 */
	bool hasIncoming()         const { return m_impl->hasIncoming();  }
	/**
	 * \returns The address of the remote peer of this socket
	 */
	IPV4Address getPeer()      const { return m_impl->getPeer();      }
	/**
	 * \returns The local address of this socket
	 */
	IPV4Address getAddr()      const { return m_impl->getAddr();      }
	PriorityType getPriority() const { return m_impl->getPriority();  }
	//@}

	/**
	 * @name TCP socket performance tuning
	 */
	//!@{
	void setTcpSendBufSize(uint32_t size) { m_impl->setTcpSendBufSize(size); }
	void setTcpRecvBufSize(uint32_t size) { m_impl->setTcpRecvBufSize(size); }
	uint32_t getTcpSendBufSize() const { return m_impl->getTcpSendBufSize(); }
	uint32_t getTcpRecvBufSize() const { return m_impl->getTcpRecvBufSize(); }
	//!@}
private:
	SSocket(const SSocket&);             //!< Forbidden
	SSocket& operator=(const SSocket&);  //!< Forbidden

	ImplPtr     m_impl;           //!< Implementation object
	HandlerFunc m_handler;        //!< External event handler function

	/**
	 * Pointer to scheduler's internal wrapper; stored here to avoid
	 * (possibly slow) lookups in Scheduler during every call made through
	 * this class.
	*/
	SSocketWrapperPtr m_ptr;

	/**
	 * Internal event handler, called from scheduler. Forwards the event
	 * to user-defined event handler (if present).
	 *
	 * @param ptr       Implementation pointer generating this event. Must
	 *                  match m_impl member.
	 * @param evt       The event itself
	 */
	void onEvent(ImplPtr ptr, EventType evt) {
		CHECK_FAIL(ptr == m_impl);
		if (m_handler) {
			m_handler(this, evt);
		}
	}

	friend SSocket& operator<<(SSocket &s, const std::string &data) {
		s.write(data);
		return s;
	}
	friend SSocket& operator>>(SSocket &s, std::string &data) {
		s.read(&data);
		return s;
	}
};

#endif // !__SSOCKET_H__
