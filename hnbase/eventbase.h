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
 * \file eventbase.h Interface for Event Handling Subsystem
 */

#ifndef __EVENTBASE_H__
#define __EVENTBASE_H__

#include <hnbase/osdep.h>
#include <boost/thread.hpp>
#include <boost/noncopyable.hpp>
#include <list>

/**
 * \page ehs Event Handling Subsystem Overview
 *
 * EHS provides functionality to perform Event-driven programming. EventTable's
 * are used to post/store/handle events. EventTable is derived from
 * EventTableBase, which interfaces all EventTable objects with main loop,
 * latter of which is located in EventMain class. All EventTable objects
 * are registred with EventMain on construction and de-registred on
 * destruction. Whenever there are events, EventTableBase::HandleEvents()
 * function is called on each and every registred EventTableBase objects.
 *
 * Since one of the most logical way to use this functionality is in
 * multi-threaded environment, where threads submit events and main thread
 * handles them, everything in here must be 100% thread-safe.
 *
 * Customizability:
 *   New classes can be derived from EventTableBase (or even from EventTable)
 *   to further customize this functionality.
 */

class EventTableBase;

/**
 * This Singleton class is the application main event system. Users should never
 * need to access this class directly - it is accessed from EventTableBase
 * constructor/destructors.
 *
 * The purpose of this class is to provide a central list of event tables,
 * and wait for events. When event occours, mainLoop() makes a virtual function
 * call HandleEvents() on each registred event table, handling all pending
 * events.
 *
 * \note Everything here must be thread-safe and protected through mutexes.
 */
class HNBASE_EXPORT EventMain {
public:
	//! Can be used to retrieve the only instance of this class
	static EventMain& instance();

	//! Initialize this object
	static void initialize() {
		instance();
	}
	//! Add event table
	void addEventTable(EventTableBase *et) {
		boost::mutex::scoped_lock l(m_listLock);
		m_list.push_back(et);
	}
	//! Remove event table
	void removeEventTable(EventTableBase *et) {
		boost::mutex::scoped_lock l(m_listLock);
		m_list.remove(et);
	}
	//! Enter main event loop. This runs until someone calls ExitMainLoop()
	void mainLoop();

	//! Handle all pending events in current queues.
	void process();

	//! Exit main loop. Notice that this function returns immediately, but
	//! application exit will be delayed until next event loop.
	void exitMainLoop();

	//! @returns Current timer tick
	uint64_t getTick() const { return m_curTick; }
private:
	std::list<EventTableBase*> m_list;    //!< List of event tables
	boost::mutex m_listLock;
	typedef std::list<EventTableBase*>::iterator LIter;
	bool m_running;                       //!< Runs until this is true
	boost::mutex m_runningLock;
	uint64_t m_curTick;                   //!< Current tick

	EventMain();                          //!< Constructor
	EventMain(const EventMain&);          //!< Copying forbidden
	EventMain& operator=(EventMain&);     //!< Copying forbidden
	~EventMain() {}                       //!< Destructor
};

/**
 * Abstract base class for event engine. Provides two common features for all
 * event tables:
 *
 * Notify() function, which need to be used whenever there is new event posted
 * to event queue. This is needed by main event loop to "wake up" and call
 * handlers.
 *
 * Pure virtual function prototype HandleEvents(), which needs to be overridden
 * by derived classes. This function will be called once per each main event
 * loop and must handle all events in the queue.
 *
 * EventTableBase class registers itself in central list (main event loop).
 */
class HNBASE_EXPORT EventTableBase : boost::noncopyable {
	friend class EventMain;
public:
	//! Constructor - Add to central event table list
	EventTableBase();

	//! Destructor - Remove from central event table list
	virtual ~EventTableBase();

protected:
	//! Notify central event system to wake up
	static void notify();

	/**
	 * Must be overridden by derived classes. This is called from central
	 * event loop for each of the registred objects. Should handle all
	 * pending events (if existing).
	 */
	virtual void process() = 0;
private:
	/**
	 * For implementation use only - wait until there is a pending event
	 * somewhere.
	 */
	static void waitForEvent();

	/**
	 * Check if there are any pending events anywhere.
	 */
	static bool hasPending();

	/**
	 * Toggle pending event flag to false.
	 */
	static void setNoPending();
private:
	//! Used to broadcast new events
	static boost::condition s_eventNotify;
	//! Protects the above condition
	static boost::mutex s_notifyLock;
	//! Whether there are any pending events
	static bool s_hasPending;
};

#endif
