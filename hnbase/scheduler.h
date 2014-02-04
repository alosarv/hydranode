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
 * \file scheduler.h Interface for Hydranode Networking Scheduler API
 */

/**
 * \page Scheduler Networking Scheduler API
 *
 * \section intro Introduction
 *
 * Hydranode Networking Scheduler is divided into three major parts:
 * - The user frontend (SSocket class)
 * - Translation unit (Scheduler class)
 * - The backend (SchedBase class)
 *
 * \section front User frontend
 *
 * SSocket template class provides a generic typesafe frontend to users of the
 * API. Providing accessors for all possible socket operations, it forwards all
 * calls to the <b>Translation Unit</b>. No operations are performed in the
 * frontend. Due to the template design of the frontend, we can have all
 * possible socket functions declared there, but they won't get instanciated
 * until they are actually used. As such, attempts to perform operations on
 * sockets that do not have the specified operation defined will result in
 * compile-time errors.
 *
 * The underlying implementation for SSocket class is chosen based on the first
 * two template parameters by default, however can be overridden by clients
 * wishing to define their own underlying socket implementations.
 *
 * The last parameter to SSocket template class is the scheduler class to use.
 * This parameter is provided here merely for completeness, and is not allowed
 * (nor recommended) to be overridden by module developers.
 *
 * \section transl Translation Unit
 *
 * The <b>translation unit</b> abstracts away the specifics of the underlying
 * implementation of the sockets by wrapping the requests into generic objects,
 * and submits them to the backend for processing when the time is ready. The
 * Translation unit also handles data buffering and events from the underlying
 * sockets. As such, <b>it is also the driver</b> of the scheduler engine.
 * And last, but not least, <b>it also bridges the backend and the frontend</b>
 * through virtual function and callback mechanisms. This unit is responsible
 * for sending the actual I/O requests to the underlying socket implementation,
 * as specified by the backend. No I/O may occour without explicit permission
 * from the backend (implemented through virtual function mechanisms).
 *
 * \section backend The backend
 *
 * The backend works only using abstract request objects of types UploadReqBase,
 * DownloadReqBase and ConnReqBase, and performs the I/O scheduling. The backend
 * decides when a request may be fulfilled, and how much of the request may be
 * fulfilled at this moment. Note that the backend also owns the request objects
 * and is responsible for deleting those once they have been fulfilled.
 *
 * \todo The Translation unit is useless overhead, and could be dropped in
 *       future versions; requests should be constructed only once, in SSocket
 *       class, and passed to SchedBase as ref-counted pointers to avoid
 *       excessive construction/destruction overheads.
 * \bug State variables aren't working in SSocket
 *      Since SSocket doesn't keep any information of it's own, and calls to
 *      implementation are passed through Request engine, queries on socket
 *      state right after issuing a function, e.g. connect() on it, return
 *      wrong state, since the call hasn't been passed to implementation yet.
 *      Possible fixes include setState method in implementation classes, or an
 *      additional state variable in SSocket class (latter is prefered).
 */

#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include <hnbase/schedbase.h>          // scheduler base
#include <hnbase/sockets.h>            // xplatform socket api
#include <hnbase/log.h>                // for logging functions
#include <hnbase/rangelist.h>          // for ranges
#include <boost/function.hpp>          // function objects
#include <boost/scoped_array.hpp>      // scoped_array

namespace SchedEventHandler {

/**
 * Policy class used by Scheduler for events handling.
 *
 * @param Source       Source class type generating the events
 * @param Scheduler    Parenting scheduler for this socket type
 */
template<typename Source, typename Scheduler>
class ClientEventHandler {
	typedef typename Scheduler::SIter SIter;
	typedef typename Scheduler::RIter RIter;
	typedef typename Scheduler::UploadReq UploadReq;
	typedef typename Scheduler::DownloadReq DownloadReq;
	typedef typename Scheduler::ConnReq ConnReq;
	typedef typename Scheduler::SSocketWrapperPtr SSocketWrapperPtr;
public:
	/**
	 * Socket event handler. Note that the actual events handling is
	 * directed to smaller helper functions for simplicity and clarity.
	 *
	 * @param src        Source of the event
	 * @param evt        The event itself
	 *
	 * \note This function is called from main event loop, and should never
	 *       be called directly.
	 */
	static void onEvent(Source *src, SocketEvent evt) {
		SIter i = Scheduler::s_sockets.find(src);
		if (i == Scheduler::s_sockets.end()) {
			return; // ignore silently (happens sometimes)
		}
		switch (evt) {
			case SOCK_READ:
				handleRead(src, (*i).second);
				break;
			case SOCK_WRITE:
				handleWrite(src, (*i).second);
				break;
			case SOCK_CONNECTED:
				handleConnected(src, (*i).second);
				break;
			case SOCK_LOST:
			case SOCK_ERR:
			case SOCK_TIMEOUT:
			case SOCK_CONNFAILED:
				handleErr(src, evt, (*i).second);
				break;
			default:
				logWarning(boost::format(
					"Scheduler: Unhandled socket event %p"
				) % evt);
				break;
		}
	}

	//! Handles "connection established" type of events
	static void handleConnected(Source *src, SSocketWrapperPtr sw) {
		if (sw->m_outBuffer->size()) {
			RIter j = Scheduler::s_upReqs.find(src);
			if (j != Scheduler::s_upReqs.end()) {
				(*j).second->setValid(true);
			} else {
				UploadReq *req = new UploadReq(sw);
				SchedBase::instance().addUploadReq(req);
			}
		}
		if (*sw->m_connecting) {
			SchedBase::instance().delConnecting();
			*sw->m_connecting = false;
		}
		sw->notify(SOCK_CONNECTED);
	}

	//! Handles "socket became writable" type of events
	static void handleWrite(Source *src, SSocketWrapperPtr sw) {
		if (sw->m_outBuffer->size()) {
			RIter j = Scheduler::s_upReqs.find(src);
			if (j != Scheduler::s_upReqs.end()) {
				(*j).second->setValid(true);
			} else {
				UploadReq *req = new UploadReq(sw);
				SchedBase::instance().addUploadReq(req);
			}
		} else {
			sw->notify(SOCK_WRITE);
		}
	}

	//! Handles "socket became readable" type of events
	static void handleRead(Source *src, SSocketWrapperPtr sw) {
		RIter j = Scheduler::s_downReqs.find(src);
		if (j != Scheduler::s_downReqs.end()) {
			(*j).second->setValid(true);
		} else {
			DownloadReq *r = new DownloadReq(sw);
			SchedBase::instance().addDloadReq(r);
		}
	}

	/**
	 * Handles all error conditions, e.g. timeouts, connfailed etc.
	 *
	 * @param src     Event source
	 * @param evt     The error event
	 */
	static void handleErr(
		Source *src, SocketEvent evt, SSocketWrapperPtr sw
	) {
		SchedBase::instance().delConn();
		Scheduler::invalidateReqs(src);
		if (*sw->m_connecting) {
			SchedBase::instance().delConnecting();
			*sw->m_connecting = false;
		}
		sw->notify(evt);
	}
};

/**
 * Policy class for events emitted from server type sockets. This class is
 * used by Scheduler for events handling.
 *
 * @param Source       Event source object type
 * @param Scheduler    Scheduler corresponding to the source type
 */
template<typename Source, typename Scheduler>
class ServerEventHandler {
public:
	typedef typename Scheduler::SIter SIter;
	typedef typename Scheduler::AcceptReq AcceptReq;

	/**
	 * Event handler function for events emitted from servers
	 *
	 * @param Scheduler      Scheduler governing this socket
	 * @param src            Source of the event
	 * @param evt            The actual event itself
	 *
	 * \note This function is called from main event loop and should never
	 *       be called directly by user code.
	 */
	static void onEvent(Source *src, SocketEvent evt) {
		SIter i = Scheduler::s_sockets.find(src);
		if (i == Scheduler::s_sockets.end()) {
			return; // happens sometimes
		}
		switch (evt) {
			case SOCK_ACCEPT: {
				AcceptReq *ar = new AcceptReq((*i).second);
				SchedBase::instance().addConnReq(ar);
				break;
			}
			case SOCK_LOST:
				// Server losing connection ?
				break;
			case SOCK_ERR:
				// Server became erronous ?
				break;
			default:
				break;
		}
	}
};

} // !SchedEventHandler

/**
 * Traits class - selects the event handler policy class based on the event
 * source type. For example, SocketClient and UDPSocket use same event handler
 * policy, while SocketServer needs a specialized version of event handler
 * due to the radically different nature of it.
 *
 * The primary template of this class is not implemented. Instead, one of the
 * specialized versions below are chosen. If no possible specialization could
 * be chosen, this generates an undefined reference during linking, which is
 * expected.
 */
template<typename Source, typename Scheduler>
class GetEventHandler;

template<typename Scheduler>
class GetEventHandler<SocketClient, Scheduler> {
public:
	typedef SchedEventHandler::ClientEventHandler<
		SocketClient, Scheduler
	> Handler;
};

template<typename Scheduler>
class GetEventHandler<UDPSocket, Scheduler> {
public:
	typedef SchedEventHandler::ClientEventHandler<
		UDPSocket, Scheduler
	> Handler;
};

template<typename Scheduler>
class GetEventHandler<SocketServer, Scheduler> {
public:
	typedef SchedEventHandler::ServerEventHandler<
		SocketServer, Scheduler
	> Handler;
};

/**
 * Scheduler class, implementing second level of Hydranode Networking Scheduling
 * API, abstracts away modules part of the sockets by generating a priority
 * score (PS) for each of the pending requests. All requests are received from
 * the frontend, wrapped into generic containers, and buffered internally for
 * later processing. No direct action shall be taken in the functions directly
 * or indirectly called from frontend.
 *
 * @param Impl             Implemenetation class type
 * @param ImplPtr          Pointer to implementation class
 */
template<typename Impl, typename _ImplPtr = Impl*>
class Scheduler {
	//! Pointer to implementation
	typedef _ImplPtr ImplPtr;
	//! Function object that can be used to retrieve a module's score
	typedef typename boost::function<float (ImplPtr)> ScoreFunc;
	//! Accept type (saves some typing)
	typedef typename Impl::AcceptType AcceptType;
	//! Type of events emitted from Impl
	typedef typename Impl::EventType EventType;
	//! Function prototype for events emitted from Impl
	typedef boost::function<void (ImplPtr, EventType)> HandlerFunc;
	//! Event handler object
	typedef typename GetEventHandler<Impl, Scheduler>::Handler EventHandler;
public:
	class SSocketWrapper;
	typedef boost::shared_ptr<SSocketWrapper> SSocketWrapperPtr;

	/**
	 * Schedule outgoing data
	 *
	 * @param ptr   Implementation pointer where to send this data
	 * @param data  Data buffer to be sent out
	 *
	 * If there is existing outgoing data scheduled for this socket, the
	 * newly passed data must be appeneded to existing data buffer.
	 * Otherwise, a new data buffer is allocated for this socket, and the
	 * data copied into there. In the latter case, a new upload request
	 * is generated and submitted to SchedBase for processing.
	 *
	 * \note The pointed socket is not required to be in connected state
	 *       at the time of this call, since no actual I/O is performed
	 *       here. In that case, the data will be sent as soon as the
	 *       connection has been established.
	 *
	 * \pre ptr is previously added to scheduler using addSocket method
	 */
	static void write(SSocketWrapperPtr ptr, const std::string &data) {
		assert(s_sockets.find(ptr->getSocket()) != s_sockets.end());

		ptr->m_outBuffer->append(data);

		RIter i = s_upReqs.find(ptr->getSocket());
		if (i != s_upReqs.end()) {
			(*i).second->setValid(true);
		} else if (ptr->getSocket()->isConnected()) {
			SchedBase::instance().addUploadReq(new UploadReq(ptr));
		}
	}

	/**
	 * Read data from socket
	 *
	 * @param ptr       Socket to read data from
	 * @param buf       Buffer to append the retrieved data to
	 *
	 * This function is only allowed to read data from previously allocated
	 * internal buffer. No additional network I/O may be performed. The
	 * internal buffer is located using the passed pointer, and the found
	 * data (if any) appeneded to the designated buffer. The internal buffer
	 * must then be deallocated.
	 *
	 * \note It is not required that the designated socket is in connected
	 *       state at the time of this call, since no actual networking I/O
	 *       is performed.
	 */
	static void read(SSocketWrapperPtr ptr, std::string *buf) {
		assert(s_sockets.find(ptr->getSocket()) != s_sockets.end());
		buf->append(*ptr->m_inBuffer);
		ptr->m_inBuffer->clear();
	}

	/**
	 * Faster version of read(), this returns the input buffer directly,
	 * performing two std::string copy operations (cheap on most impls),
	 * avoiding (possibly costly) std::string::append() call.
	 *
	 * @param ptr     Pointer to socket to read data from
	 * @return        The current input buffer
	 *
	 * \note Just as with read() method, the scheduler's internal buffer is
	 *       cleared with this call.
	 */
	static std::string getData(SSocketWrapperPtr ptr) {
		assert(s_sockets.find(ptr->getSocket()) != s_sockets.end());
		std::string tmp = *ptr->m_inBuffer;
		ptr->m_inBuffer->clear();
		return tmp;
	}

	/**
	 * Accept a pending connection
	 *
	 * @param ptr    Socket to accept the connection from
	 * @return       A new socket, which is in connected state
	 *
	 * The function searches the internal buffer for the socket designated
	 * to by ptr parameter, and returns the dynamically allocated connection
	 * to caller. If there are no pending accepted connections, exception
	 * is thrown.
	 */
	static AcceptType* accept(SSocketWrapperPtr ptr) {
		assert(s_sockets.find(ptr->getSocket()) != s_sockets.end());
		CHECK_THROW(ptr->m_accepted->size());
		AcceptType *tmp = ptr->m_accepted->front();
		ptr->m_accepted->pop_front();
		return tmp;
	}

	/**
	 * Request an outgoing connection
	 *
	 * @param ptr       Socket requesting the connection
	 * @param addr      Address to connect to
	 * @param timeout   Milliseconds to try to connect before giving up
	 */
	static void connect(
		SSocketWrapperPtr ptr, IPV4Address addr, uint32_t timeout
	) {
		assert(s_sockets.find(ptr->getSocket()) != s_sockets.end());

		if (!SchedBase::instance().isAllowed(addr.getIp())) {
			SchedBase::instance().addBlocked();
			ptr->notify(SOCK_BLOCKED);
			return;
		}
		ConnReq *cr = new ConnReq(ptr, addr, timeout);
		SchedBase::instance().addConnReq(cr);
	}

	/**
	 * Disconnect a connected socket
	 *
	 * @param ptr       Socket to be disconnected
	 *
	 * Note that disconnecting a socket invalidates all pending requests
	 * for this socket, however does not clear already buffered data.
	 */
	static void disconnect(SSocketWrapperPtr ptr) {
		assert(s_sockets.find(ptr->getSocket()) != s_sockets.end());
		if (
			ptr->getSocket()->isConnected() ||
			ptr->getSocket()->isConnecting()
		) {
			ptr->getSocket()->disconnect();
			SchedBase::instance().delConn();
			invalidateReqs(ptr->getSocket());
		}
		if (*ptr->m_connecting) {
			SchedBase::instance().delConnecting();
			*ptr->m_connecting = false;
		}
	}

	/**
	 * @returns The current download speed of the socket
	 */
	static uint32_t getDownSpeed(SSocketWrapperPtr ptr) {
		assert(s_sockets.find(ptr->getSocket()) != s_sockets.end());
		return ptr->m_downSpeed->getSpeed();
	}

	/**
	 * @returns The current upload speed of the socket
	 */
	static uint32_t getUpSpeed(SSocketWrapperPtr ptr) {
		assert(s_sockets.find(ptr->getSocket()) != s_sockets.end());
		return ptr->m_upSpeed->getSpeed();
	}

	//! @returns Total uploaded to socket
	static uint64_t getUploaded(SSocketWrapperPtr ptr) {
		assert(s_sockets.find(ptr->getSocket()) != s_sockets.end());
		return ptr->m_upSpeed->getTotal();
	}

	//! @returns Total downloaded from socket
	static uint64_t getDownloaded(SSocketWrapperPtr ptr) {
		assert(s_sockets.find(ptr->getSocket()) != s_sockets.end());
		return ptr->m_downSpeed->getTotal();
	}

	/**
	 * Add a socket to the scheduler
	 *
	 * @param Module      Module owning this socket
	 * @param s           Socket to be added
	 * @param h           Frontend event handler for notifications
	 */
	template<typename Module>
	static SSocketWrapperPtr addSocket(ImplPtr s, HandlerFunc h) {
		logTrace(
			TRACE_SCHED,
			boost::format("Adding socket %p to scheduler.") % s
		);

		ScoreFunc sf(&SchedBase::getScore<Module, ImplPtr>);
		SSocketWrapperPtr sw(new SSocketWrapper(s, h, sf));
		HandlerFunc hfunc(&EventHandler::onEvent);
		Module::instance().getDownSpeed.connect(
			boost::bind(
				&SpeedMeter::getSpeed, sw->m_downSpeed.get(),
				1000.0
			)
		);
		Module::instance().getUpSpeed.connect(
			boost::bind(
				&SpeedMeter::getSpeed, sw->m_upSpeed.get(),
				1000.0
			)
		);
		sw->addUploaded   = boost::bind(
			&Module::addUploaded, &Module::instance(), _1
		);
		sw->addDownloaded = boost::bind(
			&Module::addDownloaded, &Module::instance(), _1
		);
		s->setHandler(hfunc);
		s_sockets.insert(std::make_pair(s, sw));
		Module::instance().addSocket();

		return sw;
	}

	/**
	 * Remove a socket from the scheduler
	 *
	 * @param Module      Module owning this socket
	 * @param s         Socket to be removed
	 *
	 * \note All pending data buffers will be cleared for this socket.
	 * \note All pending requests related to this socket will be deleted.
	 */
	template<typename Module>
	static void delSocket(SSocketWrapperPtr s) {
		logTrace(
			TRACE_SCHED,
			boost::format("Removing socket %p from scheduler.") % s
		);
		assert(s_sockets.find(s->getSocket()) != s_sockets.end());
		if (*s->m_connecting) {
			SchedBase::instance().delConnecting();
			*s->m_connecting = false;
		}

		s_sockets.erase(s->getSocket());
		ImplPtr impl = s->getSocket();
		if (impl->isConnected() || impl->isConnecting()) {
			SchedBase::instance().delConn();
		}
		// reset handler, in case we get any more events before actual
		// underlying socket deletions (happens sometimes)
		s->m_handler = HandlerFunc();
		invalidateReqs(impl);
		Module::instance().delSocket();
	}

// We need the internal classes public so the policy classes (namely,
// EventHandler) can access them. If only we could make the EventHandler a
// friend of ours, we could move this to private sector.
public:
	/**
	 * Wrapper object for scheduled socket, contains all the useful
	 * information we need, e.g. score, frontend event handler, and
	 * underlying socket object. Used by requests to keep track of which
	 * socket the request belongs to. This object is always contained in
	 * shared_ptr.
	 */
	class SSocketWrapper {
	public:
		/**
		 * Construct new socket wrapper
		 *
		 * @param s     Implementation-defined pointer to underlying
		 *              socket
		 * @param h     Frontend event handler for notifications
		 * @param f     Score function object to retrieve this socket's
		 *              priority score
		 */
		explicit SSocketWrapper(
			ImplPtr s, HandlerFunc h = 0, ScoreFunc f = 0
		) : m_socket(s), m_handler(h), m_scoreFunc(f), 
		m_connecting(new bool(false)),
		m_outBuffer(new std::string), m_inBuffer(new std::string),
		m_accepted(new std::deque<AcceptType*>),
		m_downSpeed(
			new SpeedMeter(
				SchedBase::DEFAULT_HISTSIZE, 
				SchedBase::DEFAULT_PRECISION
			)
		), m_upSpeed(
			new SpeedMeter(
				SchedBase::DEFAULT_HISTSIZE,
				SchedBase::DEFAULT_PRECISION
			)
		) {}

		//! @name Accessors
		//@{
		ImplPtr     getSocket()  const { return m_socket;              }
		float       getScore()   const { return m_scoreFunc(m_socket); }
		HandlerFunc getHandler() const { return m_handler;             }
		boost::signals::connection getConn() const { return m_conn; }
		void setConn(boost::signals::connection c) { m_conn = c; }
		//@}

		bool isWritable() const {
			return !m_outBuffer->size() && m_socket->isWritable();
		}
		bool isReadable() const { return m_inBuffer->size(); }

		/**
		 * Pass event to frontend
		 *
		 * @param evt      Event to be sent
		 *
		 * \note If no handler is set, this function does nothing
		 */
		void notify(EventType evt) const {
			if (m_handler) {
				m_handler(m_socket, evt);
			}
		}

		friend bool operator<(
			const SSocketWrapper &x, const SSocketWrapper &y
		) {
			return x.m_socket < y.m_socket;
		}

	// Since this is an internal class, we keep data members public for
	// easier access
		SSocketWrapper();         //!< Forbidden

		ImplPtr     m_socket;     //!< Underlying socket
		HandlerFunc m_handler;    //!< Frontend event handler
		boost::signals::connection m_conn; //!< Backend event connection
		ScoreFunc   m_scoreFunc;  //!< Function to retrieve the score
		//! if socket is connecting
		boost::shared_ptr<bool> m_connecting; 

		//! Outgoing data buffer
		boost::shared_ptr<std::string> m_outBuffer;
		//! Incoming data buffer
		boost::shared_ptr<std::string> m_inBuffer;
		//! Accepted connections
		boost::shared_ptr<std::deque<AcceptType*> > m_accepted;

		// speed-meters
		boost::shared_ptr<SpeedMeter> m_downSpeed;
		boost::shared_ptr<SpeedMeter> m_upSpeed;
		boost::function<void (uint32_t)> addUploaded;
		boost::function<void (uint32_t)> addDownloaded;
	};

	/**
	 * Upload request is a request that indicates we wish to send out
	 * data to a socket. The request is to be passed to SchedBase class
	 * which in turn calls us back through virtual function doSend() when
	 * we are allowed to perform the actual sending.
	 */
	class UploadReq : public SchedBase::UploadReqBase {
	public:
		/**
		 * Construct new upload request
		 *
		 * @param s       Socket where this request belongs to
		 * @param buf     Pointer to data buffer to be sent
		 */
		UploadReq(const SSocketWrapperPtr &s)
		: UploadReqBase(s->getScore()), m_obj(s) {
			bool ret = Scheduler::s_upReqs.insert(
				std::make_pair(s->getSocket(), this)
			).second;
			CHECK_FAIL(ret);
			(void)ret; // suppress warning in release build
		}

		//! Erase ourselves from s_upReqs map
		~UploadReq() {
			Scheduler::s_upReqs.erase(m_obj->getSocket());
		}

		/**
		 * Send out data
		 *
		 * @param num      Number of bytes to send out
		 * @return         Number of bytes actually sent out
		 */
		virtual uint32_t doSend(uint32_t num) {
			uint32_t peer = m_obj->getSocket()->getPeer().getIp();
			bool isLimited = SchedBase::instance().isLimited(peer);

			if (!isLimited || num > m_obj->m_outBuffer->size()) {
				num = m_obj->m_outBuffer->size();
			}

			uint32_t ret = 0;
			ret = m_obj->getSocket()->write(
				m_obj->m_outBuffer->data(), num
			);

			if (ret < m_obj->m_outBuffer->size()) {
				m_obj->m_outBuffer->erase(0, ret);
			} else {
				m_obj->m_outBuffer->clear();
				invalidate();
			}

			*m_obj->m_upSpeed += ret;
			m_obj->addUploaded(ret);
			return isLimited ? ret : 0;
		}

		//! Send notification to client code, requesting more data
		virtual void notify() const {
			m_obj->notify(SOCK_WRITE);
		}

		//! Retrieve number of pending bytes in this request
		virtual uint32_t getPending() const {
			return m_obj->m_outBuffer->size();
		}
	private:
		UploadReq();              //!< Forbidden
		SSocketWrapperPtr m_obj;  //!< Keeps reference data for socket
	};

	/**
	 * Download request is an indication that we wish to receive data from
	 * a peer. As a general rule, the Scheduler assumes that when this
	 * request is submitted, there actually is pending data in the
	 * underlying socket to be received.
	 *
	 * When the main scheduler decides we are allowed to receive data, it
	 * will call doRecv() method, where we can perform actual I/O.
	 */
	class DownloadReq : public SchedBase::DownloadReqBase {
	public:
		//! Construct
		DownloadReq(const SSocketWrapperPtr &s)
		: DownloadReqBase(s->getScore()), m_obj(s) {
			bool ret = Scheduler::s_downReqs.insert(
				std::make_pair(s->getSocket(), this)
			).second;
			CHECK_FAIL(ret);
			(void)ret; // suppress warning in release build
		}

		//! Destructor
		~DownloadReq() {
			Scheduler::s_downReqs.erase(m_obj->getSocket());
		}

		/**
		 * This method is called from SchedBase (as virtual function),
		 * and indicates that we may start performing actual data
		 * receiving on this socket. The received data is appended to
		 * sockets incoming data buffer.
		 *
		 * @param amount      Amount of data we are allowed to receive
		 * @return            Amount of data actually received
		 *
		 * If the remote peer is marked as no_limit in SchedBase, we
		 * ignore the limit here, and recevive max 100k with each block.
		 */
		virtual uint32_t doRecv(uint32_t amount) {
		 	// static input buffer
			static char buf[SchedBase::INPUT_BUFSIZE];

			uint32_t peer = m_obj->getSocket()->getPeer().getIp();
			bool isLimited = SchedBase::instance().isLimited(peer);

			if (!isLimited || amount > SchedBase::INPUT_BUFSIZE) {
				amount = SchedBase::INPUT_BUFSIZE;
			}

			int ret = m_obj->getSocket()->read(buf, amount);

			if (ret == 0) {  // Got no data - mh ?
				return 0;
			}

			*m_obj->m_downSpeed += ret;
			m_obj->addDownloaded(ret);
			m_obj->m_inBuffer->append(buf, ret);

			// if no limit is applied, don't return count either
			return isLimited ? ret : 0;
		}

		//! Notify client code
		virtual void notify() const {
			m_obj->notify(SOCK_READ);
		}
	private:
		DownloadReq();                //!< Forbidden
		SSocketWrapperPtr m_obj;      //!< Underlying socket
	};

	/**
	 * Accept request indicates we wish to accept an incoming connection
	 * from one of the servers. It is of generic type Connection Request.
	 * When SchedBase decides we may accept the connection, it will call
	 * doConn() member function, where we may perform actual connection
	 * accepting.
	 */
	class AcceptReq : public SchedBase::ConnReqBase {
	public:
		//! Construct
		AcceptReq(const SSocketWrapperPtr &s)
		: ConnReqBase(s->getScore()), m_obj(s) {
			bool ret = Scheduler::s_accReqs.insert(
				std::make_pair(s->getSocket(), this)
			).second;
			CHECK_FAIL(ret);
			(void)ret; // suppress warning in release build
		}
		//! Destroy
		~AcceptReq() {
			Scheduler::s_accReqs.erase(m_obj->getSocket());
		}

		/**
		 * Perform actual connection accepting. The accepted connection
		 * must be buffered into s_accepted map for later retrieval
		 * by client code. Note that we may NOT notify client code here.
		 * Notifications are managed by SchedBase.
		 *
		 * @return       Bitfield of ConnReqBase::ConnRet values
		 */
		virtual int doConn() {
			AcceptType *s = m_obj->getSocket()->accept();
			uint32_t ip = s->getPeer().getAddr();
			if (!SchedBase::instance().isAllowed(ip)) {
				SchedBase::instance().addBlocked();
				s->destroy();
				invalidate();
				return REMOVE;
			}
			m_obj->m_accepted->push_back(s);
			invalidate();
			return ADDCONN | REMOVE | NOTIFY;
		}

		virtual void notify() const {
			m_obj->notify(SOCK_ACCEPT);
		}
	private:
		AcceptReq();                //!< Forbidden
		SSocketWrapperPtr m_obj;    //!< Socket reference data
	};

	/**
	 * Connection request indicates we wish to perform an outgoing
	 * connection. When we are allowed to perform the actual connecting,
	 * SchedBase calls our doConn() member, where we may perform the actual
	 * connecting.
	 */
	class ConnReq : public SchedBase::ConnReqBase {
	public:
		//! Construct
		ConnReq(
			const SSocketWrapperPtr &s, IPV4Address addr,
			uint32_t timeout
		) : ConnReqBase(s->getScore()), m_obj(s), m_addr(addr),
		m_timeout(timeout) {
			m_out = true;
			bool ret = Scheduler::s_connReqs.insert(
				std::make_pair(s->getSocket(), this)
			).second;
			// throw instead of fail here, since this can also be
			// triggered by calling connect() on a socket twice in
			// row (nonfatal error that shouldn't kill the app)
			CHECK_THROW(ret);
		}

		//! Destroy
		~ConnReq() {
			Scheduler::s_connReqs.erase(m_obj->getSocket());
		}

		/**
		 * Initialize the actual connection
		 *
		 * @return      If the attempt succeeded or not
		 */
		virtual int doConn() try {
			if (!*m_obj->m_connecting) {
				*m_obj->m_connecting = true;
				SchedBase::instance().addConnecting();
			}
			m_obj->getSocket()->connect(m_addr, m_timeout);
			return REMOVE | ADDCONN;
		} catch (...) {
			if (*m_obj->m_connecting) {
				*m_obj->m_connecting = false;
				SchedBase::instance().delConnecting();
			}
			return REMOVE | NOTIFY;
		}

		/**
		 * Notify client code; only SOCK_CONNFAILED is emitted here,
		 * because SOCK_CONNECTED is received via events from underlying
		 * socket API.
		 */
		virtual void notify() const {
			m_obj->notify(SOCK_CONNFAILED);
		}
	private:
		ConnReq();                     //!< Forbidden
		SSocketWrapperPtr m_obj;       //!< Socket reference data
		IPV4Address       m_addr;      //!< Address to connect to
		uint32_t          m_timeout;   //!< Optional connect timeout
	};

	//! set of all scheduled sockets
	static std::map<ImplPtr, SSocketWrapperPtr> s_sockets;
	typedef typename std::map<ImplPtr, SSocketWrapperPtr>::iterator SIter;

	/**
	 * @name Maps to keep pointers to all pending requests for all sockets.
	 *
	 * The purpose of these maps is to keep backwards links to requests
	 * submitted to SchedBase in case we need to invalidate them or
	 * something. Note that these maps only keep weak references to the
	 * requests. Elements of these maps should never be deleted directly.
	 * The elements are owned by SchedBase class.
	 *
	 * To make things simpler and cleaner, the specific requests insert
	 * themselves to these maps on construction, and erase themselves on
	 * destruction.
	 */
	//@{
	static std::map<ImplPtr, SchedBase::ReqBase*> s_upReqs;
	static std::map<ImplPtr, SchedBase::ReqBase*> s_downReqs;
	static std::map<ImplPtr, SchedBase::ReqBase*> s_connReqs;
	static std::map<ImplPtr, SchedBase::ReqBase*> s_accReqs;
	//@}

	typedef typename std::map<
		ImplPtr, SchedBase::ReqBase*
	>::iterator RIter;

	/**
	 * Invalidate all requests related to a specific socket
	 *
	 * @param ptr     Socket for which all requests should be invalidated
	 */
	static void invalidateReqs(ImplPtr ptr) {
		RIter i = s_upReqs.find(ptr);
		if (i != s_upReqs.end()) {
			logTrace(TRACE_SCHED, "Aborting upload request.");
			(*i).second->invalidate();
		}
		i = s_downReqs.find(ptr);
		if (i != s_downReqs.end()) {
			logTrace(TRACE_SCHED, "Aborting download request.");
			(*i).second->invalidate();
		}
		i = s_accReqs.find(ptr);
		if (i != s_accReqs.end()) {
			logTrace(TRACE_SCHED, "Aborting accept request.");
			(*i).second->invalidate();
		}
		i = s_connReqs.find(ptr);
		if (i != s_connReqs.end()) {
			logTrace(TRACE_SCHED, "Aborting connection request.");
			(*i).second->invalidate();
		}
	}
};

// initialize static data
template<typename P1, typename P2>
std::map<
	typename Scheduler<P1, P2>::ImplPtr,
	typename Scheduler<P1, P2>::SSocketWrapperPtr
> Scheduler<P1, P2>::s_sockets;
template<typename P1, typename P2>
std::map<P2, SchedBase::ReqBase*>
Scheduler<P1, P2>::s_upReqs;
template<typename P1, typename P2>
std::map<P2, SchedBase::ReqBase*>
Scheduler<P1, P2>::s_downReqs;
template<typename P1, typename P2>
std::map<P2, SchedBase::ReqBase*>
Scheduler<P1, P2>::s_accReqs;
template<typename P1, typename P2>
std::map<P2, SchedBase::ReqBase*>
Scheduler<P1, P2>::s_connReqs;

#endif
