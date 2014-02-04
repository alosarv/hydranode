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
 * \file event.h Interface for Event subsystem
 */

#ifndef __EVENT_H__
#define __EVENT_H__

/**
 * @page ehsv2 Event Handling Subsystem, version 2
 *
 * \section intro Introduction
 * Event Handling Subsystem version 2 (EHS) provides a generic API for passing
 * arbitary objects from a <i>source</i> to one or more <i>handlers</i> without
 * coupling neither the source and handlers, nor coupling either of those with
 * the EHS.
 *
 * \section ratio Rationale
 * The original Event Handling Subsystem (version 1) had a number of issues that
 * made the subsystem inconvenient to use, errorpone and hard to debug. Namely,
 * the syntax for postingevents and/or setting handlers was so inconvenient that
 * we had to resort to preprocessor macros to compensate for that. In addition,
 * the original engine was based on CBFunctor library, which dates back to 1994,
 * and thus breaks on some modern compilers. Thus, the system was rewritten.
 *
 * \section req Requirements
 * The requirements for EHSv2 are:
 * \list
 * - Decouple the event source from event tables. This is required to allow
 *   posting events from sources that were originally not designed for events,
 *   because they come from, for example, third-party libraries.
 * - Easy-to-learn and easy-to-use usage syntax.
 * - Base on a modern function objects library that is portable.
 *
 * \section usage Usage
 * There are two common operations needed to be done when it comes to event
 * tables usage - setting up event handlers, and posting events. In order to
 * provide maximum simplicity for those operations, there are a set of utility
 * methods (defined in <b>EHS</b> namespace) which simplify the syntax. The
 * basic usage of EHS goes as follows:
 *
 * \code
 * // Initialize objects. Note: the first event (posted in MySource
 * // constructor) is not handled.
 * MyHandler *h = new MyHandler;
 * MySource *s = new MySource;
 * // Set up handler, and post an event
 * EHS::addHandler(s, h, &MyHandler::handlerfunc);
 * EHS::postEvent(s, MyEvent("Hello world!"));
 * \endcode
 */

// hydranode includes
#include <hnbase/osdep.h>
#include <hnbase/bind_placeholders.h>
#include <hnbase/eventbase.h>
#include <hnbase/gettickcount.h>
#include <hnbase/log.h>
#include <hnbase/utils.h>
#include <hnbase/tsptrs.h>

// event tables backend - Boost.Signal, Boost.Function and Boost.Bind libraries
#include <boost/signal.hpp>
#include <boost/signals/trackable.hpp>
#include <boost/signals/connection.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/type_traits.hpp>

// multi-thread safety - Boost.Thread (mutexes)
#include <boost/thread.hpp>

// std includes
#include <map>
#include <set>

/**
 * Trackable object allows tracking the object's lifetime by Event subsystem,
 * and thus avoid emitting events from already-destroyed sources. To enable
 * source object tracking, derive the event source type publically from
 * Trackable,and the rest of the machinery is done automatically by Event
 * subsystem (via compile-time type-checking).
 *
 * Trackable is also boost::signals::trackable, so connections established by
 * a Trackable object are also disconnected when Trackable object is destroyed,
 * thus providing automatic and safe event handlers removal.
 *
 * \note This does not work with smart-pointer types yet.
 */
class HNBASE_EXPORT Trackable : public boost::signals::trackable {
public:
	Trackable() : m_validator(new bool(true)) {}
	Trackable(const Trackable &copy) : m_validator(new bool(true)) { }
	~Trackable() { *m_validator = false; }

	boost::intrusive_ptr<bool> getValidator() const { return m_validator; }
private:
	boost::intrusive_ptr<bool> m_validator;
};

/**
 * EventTable template class encapsulates pending events storage, event handlers
 * storage and event handlers calling when instructed to do so from main event
 * loop. For each event source and event type, there is a specific event table.
 *
 * @param Source    Type of object the events are emitting from. This should
 *                  generally be a pointer type, e.g. MyClass*
 * @param Event     Type of event the source emits.
 */
template<typename Source, typename Event>
class EventTable : public EventTableBase {
	class InternalEvent;
	class DelayedEvent;
	//! Internal event shared pointer
	typedef boost::intrusive_ptr<InternalEvent> EventPtr;
	//! Delayed event shared pointer
	typedef boost::intrusive_ptr<DelayedEvent> DelayedPtr;
	//! Type of handler function
	typedef typename boost::function<void (Source, Event)> Handler;
	//! Type of signal used in this event table
	typedef typename boost::signal<void (Source, Event)> SigType;
	//! Helper typedef, for iterating on sigMap
	typedef typename std::map<Source, SigType*>::iterator Iter;
	//! Helper typedef, for iterating on pending events vector
	typedef typename std::vector<EventPtr>::iterator PIter;
	//! Helper typedef, for iterator in m_toDelete vector
	typedef typename std::set<Source>::iterator DeleteIter;
public:
	//! Dummy default constructor
	EventTable() {}

	EventTable(std::string source, std::string event)
	: m_source(source), m_event(event) {
#ifdef DEBUG_EVTT // for init/cleanup debugging; don't use log calls here!
		std::cerr << "Debug: " <<
			(boost::format("Initializing EventTable(%s, %s)")
			% source % event) << std::endl;
		;
#endif
	}

	//! Dummy destructor
	~EventTable() {
#ifdef DEBUG_EVTT
		std::cerr << "Debug: " <<
			(boost::format("Destroying EventTable(%s, %s)")
			% m_source % m_event) << std::endl;
		;
#endif
		typename std::map<Source, SigType*>::iterator it;
		it = m_sigMap.begin();
		while (it != m_sigMap.end()) {
			delete (*it++).second;
		}
		m_sigMap.clear();
	}

	/**
	 * Connect an event handler to a source. The handler shall be called
	 * whenever the source object emits events.
	 *
	 * \note It is recommended to use classes derived from boost::signals::
	 *       trackable for event handlers, and connect them via the
	 *       overloaded addHandler method, since that way automatic
	 *       disconnection is guaranteed when either the signal source or
	 *       the signal handler objects get destroyed.
	 *
	 * @param src     Source the handler is interested in
	 * @param ha      Event handler function object
	 * @return        The established connection between the event source
	 *                and event handler. This can later be used to
	 *                disconnect the handler from the source.
	 */
	boost::signals::connection addHandler(Source src, Handler ha) {
		if (!src) {
			return addAllHandler(ha);
		}

		boost::recursive_mutex::scoped_lock l(m_sigMapMutex);
		Iter i = m_sigMap.find(src);
		if (i == m_sigMap.end()) {
			i = m_sigMap.insert(
				std::make_pair(src, new SigType())
			).first;
		}
		return (*i).second->connect(ha, boost::signals::at_front);
	}

	/**
	 * Overloaded version of the above method, performs function object
	 * binding internally. This is the recommended version of this function,
	 * since while being also simpler to use, it also guarantees function
	 * disconnection in case the object is derived from boost::signals::
	 * trackable.
	 *
	 * \note This limitation is inherent from Boost.Signals library -
	 *       namely, trackable objects only work when bound with
	 *       boost::bind, raw Boost.Function or Boost.Lambda functors do not
	 *       implement the necesery interface for this to work.
	 *
	 * @param src   Event source to be interested in
	 * @param obj   Event handler object
	 * @param func  Event handler function
	 * @return      The established connection between the event source and
	 *              event handler, which can later be used to disconnect the
	 *              handler from the source.
	 */
	template<typename T>
	boost::signals::connection addHandler(
		Source src, T *obj, void (T::*func)(Source, Event)
	) {
		if (!src) {
			return addAllHandler(boost::bind(func, obj, _b1, _b2));
		}

		boost::recursive_mutex::scoped_lock l(m_sigMapMutex);
		Iter i = m_sigMap.find(src);
		if (i == m_sigMap.end()) {
			i = m_sigMap.insert(
				std::make_pair(src, new SigType())
			).first;
		}

		return (*i).second->connect(
			boost::bind(func, obj, _b1, _b2),
			boost::signals::at_front
		);
	}

	/**
	 * Connect a raw function to the source signal.
	 */
	template<typename T>
	boost::signals::connection addHandler(
		Source src, void (*ha)(Source, Event)
	) {
		if (!src) {
			return addAllHandler(ha);
		}

		boost::recursive_mutex::scoped_lock l(m_sigMapMutex);
		Iter i = m_sigMap.find(src);
		if (i == m_sigMap.end()) {
			i = m_sigMap.insert(
				m_sigMap.begin(),
				std::make_pair(src, new SigType)
			);
		}
		return (*i).second->connect(ha, boost::signals::at_front);
	}

	/**
	 * @name Add handler for all events from all sources of this type.
	 */
	//@{
	boost::signals::connection addAllHandler(Handler ha) {
		boost::recursive_mutex::scoped_lock l(m_sigMapMutex);
		return m_allSig.connect(ha);
	}
	template<typename T>
	boost::signals::connection addAllHandler(
		T *obj, void (T::*func)(Source, Event)
	) {
		boost::recursive_mutex::scoped_lock l(m_sigMapMutex);
		return m_allSig.connect(boost::bind(func, obj, _b1, _b2));
	}
	template<typename T>
	boost::signals::connection addAllHandler(void (*func)(Source, Event)) {
		boost::recursive_mutex::scoped_lock l(m_sigMapMutex);
		return m_allSig.connect(func);
	}
	//@}

	/**
	 * Disconnect an event handler which has previously been connected with
	 * addHandler() method.
	 *
	 * @param src     Event source
	 * @param c       Connection to be disconnected
	 */
	void delHandler(Source src, const boost::signals::connection &c) {
		boost::recursive_mutex::scoped_lock l(m_sigMapMutex);
		c.disconnect();
		Iter i = m_sigMap.find(src);
		if (i != m_sigMap.end() && (*i).second->empty()) {
			delete (*i).second;
			m_sigMap.erase(i);
		}
	}

	/**
	 * Erase all handlers refering to a specific source. This is useful to
	 * be called from either source object's destructor, or by whoever is
	 * destroying the source, to clean up the event table things.
	 *
	 * @param src    Source object
	 */
	void delHandlers(Source src) {
		boost::recursive_mutex::scoped_lock l(m_sigMapMutex);
		Iter i = m_sigMap.find(src);
		if (i != m_sigMap.end()) {
			delete (*i).second;
			m_sigMap.erase(i);
		}
	}

	/**
	 * Special version of handler removal, which works for functions that
	 * can be compared (e.g. raw free functions).
	 *
	 * @param src   Source to be disconnected from
	 * @param ha    Event handler function
	 */
	void delHandler(Source src, void (*ha)(Source, Event)) {
		boost::recursive_mutex::scoped_lock l(m_sigMapMutex);
		Iter i = m_sigMap.find(src);
		if (i != m_sigMap.end()) {
			(*i).second->disconnect(ha);
			if ((*i).second->empty()) {
				delete (*i).second;
				m_sigMap.erase(i);
			}
		}
	}

	/**
	 * Post an event to the event queue, which shall be passed to all
	 * handlers during next event loop.
	 *
	 * @param src     Event source
	 * @param evt     The event itself
	 */
	void postEvent(Source src, Event evt) {
		boost::recursive_mutex::scoped_lock l(m_pendingMutex);
		m_pending.push_back(EventPtr(new InternalEvent(src, evt)));
		notify();
	}

	/**
	 * Post a delayed event, which shall be emitted when the delay is over.
	 * Note that the actual delay until to the event emitting may vary,
	 * depending on application load, up to 100ms.
	 *
	 * @param src     Event source
	 * @param evt     The event itself
	 * @param delay   Delay, in milliseconds, until event will be passed to
	 *                handlers
	 *
	 * \note Breaks compilation on non-pointer source types
	 * \note Could be optimized to use compile-time type-checking instead
	 *       of runtime.
	 */
	void postEvent(Source src, Event evt, uint32_t delay) {
		boost::recursive_mutex::scoped_lock l(m_delayedMutex);
		DelayedPtr devt(new DelayedEvent(src, evt, delay));
		m_delayed.insert(devt);
	}

	/**
	 * Request EventTables to safely delete the source object, after all
	 * events for the source have been processed.
	 *
	 * @param src        Source object to be deleted
	 */
	void safeDelete(Source src) {
		boost::recursive_mutex::scoped_lock l(m_deleteMutex);
		m_toDelete.insert(src);
	}

	/**
	 * Emit an event from source object, and then request EventTables to
	 * safely delete the source object, after all events for the source have
	 * been processed.
	 *
	 * @param src        Source object to emit event from and to delete
	 * @param evt        Event to be emitted
	 */
	void safeDelete(Source src, Event evt) {
		postEvent(src, evt);
		safeDelete(src);
	}

	/**
	 * Call all handlers for all pending events. This method is called from
	 * main event loop.
	 */
	virtual void process() {
		checkForDelayed();

		boost::recursive_mutex::scoped_lock l1(m_pendingMutex);
		boost::recursive_mutex::scoped_lock l2(m_sigMapMutex);

		while (m_pending.size()) {
			EventPtr evt = m_pending.front();
			if (!evt->isValid()) {
				m_pending.pop_front();
				continue;
			}

			Iter j = m_sigMap.find(evt->getSource());
			if (j != m_sigMap.end()) {
				(*(*j).second)(
					evt->getSource(), evt->getEvent()
				);
			}
			m_allSig(evt->getSource(), evt->getEvent());

			m_pending.pop_front();
		}

		checkDelete();
	}

	//! Delete-function, used by safeDelete; declared public in order to
	//! create friendship with Src class
	template<typename Src, bool>
	struct Deleter {
		static void doDel(Src) {}
	};
	template<typename Src>
	struct Deleter<Src, true> {
		static void doDel(Src ptr) {
			delete ptr;
		}
	};
private:
	/**
	 * @name Copying is not allowed
	 */
	//@{
	EventTable(const EventTable&);
	EventTable& operator=(const EventTable &);
	//@}

	/**
	 * Stores all signal objects for all objects of type Source. This map
	 * is filled by addHandler() method, which adds the signal here if it's
	 * not here already. Also, addHandler() connects functions to the
	 * signals in this map. When a signal in this map no longer has any
	 * handlers, it should be removed (done in delHandler() method).
	 *
	 * \note We'r using heap-allocated signal objects here, since Signal
	 *       is not copyable.
	 */
	std::map<Source, SigType*> m_sigMap;

	/**
	 * Signal which will be fired whenever ANY source of this type emits
	 * an event. This is a way to handle events from all sources of a given
	 * type.
	 */
	SigType m_allSig;

	/**
	 * Pending events queue, filled by postEvent() method and cleared in
	 * process().
	 */
	std::deque<EventPtr> m_pending;

	/**
	 * Delayed events, which will be posted when the timeout is over.
	 * The key to this map is the actual tick when the event should be
	 * emitted.
	 */
	std::multiset<DelayedPtr, Utils::PtrLess<DelayedPtr> > m_delayed;

	/**
	 * Deletion queue, filled by safeDelete method; objects here are
	 * deleted once all events for the source have been processed, at
	 * the end of event loop.
	 */
	std::set<Source> m_toDelete;

	/**
	 * Recursive mutexes which protect the above four containers in multi-
	 * threaded environment. They are recursive since we are most likely
	 * to be called from an event handler from same thread, in which case we
	 * want to be able to access the containers.
	 */
	//@{
	boost::recursive_mutex m_sigMapMutex;
	boost::recursive_mutex m_pendingMutex;
	boost::recursive_mutex m_delayedMutex;
	boost::recursive_mutex m_deleteMutex;
	//@}

	//! Used for debugging; contains the source and event type names.
	std::string m_source, m_event;

	/**
	 * Performs compile-time type-checking to find out if source is derived
	 * from Trackable, and if it is so, the below specialization of this
	 * class template is chosen, which takes the source validator and
	 * attaches it to the event. When source is destroyed, the validator
	 * is invalidated, and thus the event won't be emitted.
	 */
	template<typename _Src, typename _Evt, bool>
	struct GetSetHandler {
		static void doSet(_Src, _Evt) {}
	};
	template<typename _Src, typename _Evt>
	struct GetSetHandler<_Src, _Evt, true> {
		static void doSet(_Src src, _Evt evt) {
			evt->setValidator(((Trackable*)src)->getValidator());
		}
	};

	/**
	 * Helper method, which automatically deducts template arguments from
	 * parameters; used to compensate against MSVC's problems with the above
	 * constructs.
	 */
	template<typename _Src, typename _Evt>
	static void getSetHandler(_Src src, _Evt evt) {
		GetSetHandler<
			_Src, _Evt, boost::is_base_and_derived<
				Trackable,
				typename boost::remove_pointer<_Src>::type
			>::value
		>::doSet(src, evt);
	}

	/**
	 * Wrapper class for temporary storing pending events and similar.
	 */
	class InternalEvent {
	public:
		//! Constructor
		InternalEvent(Source src, Event evt) : m_src(src), m_evt(evt) {
			EventTable::getSetHandler(src, this);
		}

		Source getSource() const { return m_src; }
		Event  getEvent()  const { return m_evt; }
		//! Check the validity of this event
		bool isValid() const { return m_valid ? *m_valid : true; }
		//! Set the validator for this event
		void setValidator(boost::intrusive_ptr<bool> v) { m_valid = v; }
	private:
		Source   m_src;                        //!< Event source object
		Event    m_evt;                        //!< The event itself
		boost::intrusive_ptr<bool> m_valid;    //!< Validity
	};

	/**
	 * DelayedEvent is an event that is to be emitted after specified time
	 * has passed. DelayedEvents can be invalidated at any time before
	 * their emitting via setting the shared pointer m_valid to false.
	 * Trackable-derived objects automatically set this pointer to false
	 * upon destruction.
	 */
	class DelayedEvent : public InternalEvent {
	public:
		/**
		 * Constructor
		 *
		 * @param src         Source object
		 * @param evt         Event object
		 * @param delay       Delay, in milliseconds
		 */
		DelayedEvent(Source src, Event evt, uint32_t delay)
		: InternalEvent(src, evt),
		m_postTime(EventMain::instance().getTick() + delay) {}

		//! Comparison operator, ordered by m_postTime member
		friend bool operator<(
			const DelayedEvent &x, const DelayedEvent &y
		) {
			return x.m_postTime < y.m_postTime;
		}
		//! Implicit conversion to uint64_t returns m_postTime
		operator uint64_t() const { return m_postTime; }
	private:
		uint64_t m_postTime;               //!< Posting time
	};

	/**
	 * Transfer all delayed events for which the delay is over to pending
	 * list.
	 */
	void checkForDelayed() {
		boost::recursive_mutex::scoped_lock l1(m_delayedMutex);
		boost::recursive_mutex::scoped_lock l2(m_pendingMutex);
		uint64_t tick = EventMain::instance().getTick();

		while (m_delayed.size() && (*(*m_delayed.begin()) <= tick)) {
			m_pending.push_back(*m_delayed.begin());
			m_delayed.erase(m_delayed.begin());
		}
	}

	/**
	 * Deletes all objects in m_toDelete container; to be called at the end
	 * of event loop, after all events are processed.
	 */
	void checkDelete() {
		boost::recursive_mutex::scoped_lock l(m_deleteMutex);
		DeleteIter it = m_toDelete.begin();
		while (it != m_toDelete.end()) {
			Deleter<
				Source,
				boost::is_pointer<Source>::value
			>::doDel(*it++);
		}
		m_toDelete.clear();
	}
};

/**
 * Use this macro to define an event table for a specific object. This generates
 * a static member function getEventTable(), which can be used to get access to
 * the event table.
 *
 * @param Source      Event source type. This must be a fully qualified type,
 *                    for usage within EventTables.  Example: SocketClient*
 * @param Event       Event emitted from the source. Example: SocketEvent
 *
 * \note This design was chosen because alternative approaches which would not
 *       couple the event table with the event source causes problems with win32
 *       dynamic loader and linker and thus could not be used.
 * \note Friend declaration is needed for checkDelete() to work with private
 *       destructors.
 */
#define DECLARE_EVENT_TABLE(Source, Event)                                 \
	static EventTable<Source, Event>& getEventTable();                 \
	friend struct EventTable<Source, Event >::Deleter<                 \
		Source, boost::is_pointer< Source >::value                 \
	>

#define IMPLEMENT_EVENT_TABLE(SourceClass, Source, Event)                  \
	EventTable<Source, Event>& SourceClass::getEventTable() {          \
	static EventTable<Source, Event> s_evtT(#Source, #Event);          \
		return s_evtT;                                             \
	} class SourceClass

#endif
