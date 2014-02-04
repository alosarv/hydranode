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
 * \file workthread.h Interface for WorkThread API
 */

#ifndef __WORKTHREAD_H__
#define __WORKTHREAD_H__

#include <hnbase/osdep.h>
#include <hnbase/log.h>
#include <hnbase/tsptrs.h>
#include <boost/noncopyable.hpp>
#include <boost/thread.hpp>
#include <boost/intrusive_ptr.hpp>
#include <queue>

/**
 * ThreadWork class indicates a job to be processed in separate thread. Derived
 * classes shall override pure virtual process() member function to perform
 * their work. Notice that process() method is called from secondary thread,
 * so everything done in that method must be multi-thread safe.
 *
 * After constructing ThreadWork object on free store, wrapped in shared_ptr,
 * submit the work to WorkThread class for processing. Only one work is being
 * processed at any time, altough this may be changed/increased in later
 * revisions to make better use of multi-processor systems.
 */
class HNBASE_EXPORT ThreadWork {
public:
	/**
	 * Default constructor
	 */
	ThreadWork();

	/**
	 * Dummy destructor
	 */
	virtual ~ThreadWork();

	/**
	 * Override this pure virtual method to perform your work. It is highly
	 * recommended to perform your work incrementally, returning false here
	 * after each pass, and true once all the work has been processed. There
	 * is no performance penalty for doing so, and it allows us to later add
	 * support for work_progress concept.
	 *
	 * @return true if all work has been done, false otherwise.
	 *
	 * \todo Return value is currently ignored by implementation; use
	 *       setComplete() to indicate the job is complete to WorkThread.
	 */
	virtual bool process() = 0;

	//! Cancels a pending job
	void cancel() {
		boost::mutex::scoped_lock l(m_validLock);
		m_valid = false;
	}

	//! Check whether this job is still valid for processing
	bool isValid() {
		boost::mutex::scoped_lock l(m_validLock);
		return m_valid;
	}

	//! Check whether this job is complete
	bool isComplete() {
		boost::mutex::scoped_lock l(m_completeLock);
		return m_complete;
	}
protected:
	//! Set the state of the job to /completed/
	void setComplete() {
		boost::mutex::scoped_lock l(m_completeLock);
		m_complete = true;
	}
private:
	bool m_valid;                  //!< If true, the job is still valid
	boost::mutex m_validLock;      //!< Protects m_valid member
	bool m_complete;               //!< If true, the job is complete
	boost::mutex m_completeLock;   //!< Protects m_complete member
};

typedef boost::intrusive_ptr<ThreadWork> ThreadWorkPtr;

// WorkThread class
// ----------------
class HNBASE_EXPORT WorkThread : public boost::noncopyable {
public:
	/**
	 * Constructs and starts worker thread, waiting for jobs to perform.
	 */
	WorkThread();

	/**
	 * Destroys worker thread. Current job is aborted, and the thread is
	 * joined with main thread.
	 */
	~WorkThread();

	/**
	 * Post a work for processing.
	 *
	 * @param work      Pointer to ThreadWork object to be processed
	 */
	void postWork(ThreadWorkPtr work);

	/**
	 * Check whether the thread is running or paused
	 *
	 * @return true if the thread is running
	 */
	bool isRunning();

	/**
	 * Exception-safe wrapper object for pausing/resuming WorkThread
	 */
	struct HNBASE_EXPORT Pauser {
	        Pauser(WorkThread &parent);
	        ~Pauser();
        	WorkThread &m_parent;
	};

private:
	//! Main work thread loop function
	void threadLoop();

	//! Used by secondary thread, this is used to check whether the thread
	//! should exit
	bool checkExit();

	//! Used by main thread, this signals work thread to exit ASAP
	void exit();

	//! Pause the thread execution
	void pause();

	//! Resume the thread execution
	void resume();

	//! Pending jobs queue, filled by postWork() and processed by
	//! secondary thread.
	std::queue<ThreadWorkPtr> m_queue;
	//! Protects m_queue member
	boost::mutex m_queueLock;
	//! Used to signal events to secondary thread
	boost::condition m_notify;
	//! Protects m_notify member
	boost::mutex m_notifyLock;
	//! If true, work thread should exit as soon as possible
	bool m_exiting;
	//! Protects m_exiting member
	boost::mutex m_exitLock;
	//! Worker thread object
	boost::thread m_worker;

	boost::mutex m_runningLock;  //!< Protects m_running member
	bool m_running;              //!< If the thread is running
	boost::condition m_continue; //!< Used to signal running state change
	uint32_t m_pauserCount;      //!< Number of active "Pausers".
};

inline bool WorkThread::checkExit() {
	boost::mutex::scoped_lock l(m_exitLock);
	return m_exiting;
}

inline void WorkThread::exit() {
	boost::mutex::scoped_lock l1(m_notifyLock);
	boost::mutex::scoped_lock l2(m_exitLock);
	m_exiting = true;
	m_notify.notify_all();
}

inline void WorkThread::postWork(ThreadWorkPtr work) {
	boost::mutex::scoped_lock l(m_notifyLock);
	boost::mutex::scoped_lock l1(m_queueLock);
	m_queue.push(work);
	m_notify.notify_all();
}

inline void WorkThread::pause() {
	boost::mutex::scoped_lock l(m_runningLock);
	m_running = false;
	logTrace(TRACE_HT, "Workthread paused.");
}

inline void WorkThread::resume() {
	boost::mutex::scoped_lock l(m_runningLock);
	m_running = true;
	boost::mutex::scoped_lock l2(m_notifyLock);
	m_notify.notify_all();
	logTrace(TRACE_HT, "Workthread resumed.");
}

inline bool WorkThread::isRunning() {
	boost::mutex::scoped_lock l(m_runningLock);
	return m_running;

}

#endif
