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
 * \file workthread.cpp Implementation of WorkThread API
 */

#include <hnbase/workthread.h>

// ThreadWork class
// ----------------
ThreadWork::ThreadWork() : m_valid(true), m_complete(false) {}

ThreadWork::~ThreadWork() {}

// WorkThread class
// ----------------
WorkThread::WorkThread()
: m_exiting(false), m_worker(boost::bind(&WorkThread::threadLoop, this)),
m_running(true), m_pauserCount() {
}

WorkThread::~WorkThread() {
	exit();
	m_worker.join();
}

inline void WorkThread::threadLoop() {
	ThreadWorkPtr wrk;
	while (!checkExit()) {
		if (!wrk) {
			boost::mutex::scoped_lock l1(m_notifyLock);
			{
				boost::mutex::scoped_lock l2(m_queueLock);
				if (m_queue.size()) {
					wrk = m_queue.front();
					m_queue.pop();
				}
			}
			if (!wrk) {
				m_notify.wait(l1);
			}
		}
		if (wrk && wrk->isValid() && !wrk->isComplete()) try {
			if (isRunning()) {
				wrk->process();
			} else {
				boost::mutex::scoped_lock l(m_notifyLock);
				m_notify.wait(l);
			}
		} catch (std::exception &e) {
			logError(boost::format(
				"Fatal exception at WorkThread: %s"
			) % e.what());
			wrk = ThreadWorkPtr();
		} catch (...) {
			logError(boost::format(
				"Unknown fatal error at WorkThread."
			));
			wrk = ThreadWorkPtr();
		} else {
			wrk = ThreadWorkPtr();
		}
	}
}


// WorkThread::Pauser class
// ------------------------
WorkThread::Pauser::Pauser(WorkThread &parent)
: m_parent(parent) {
	if (!m_parent.m_pauserCount++) {
		m_parent.pause();
	}
}

WorkThread::Pauser::~Pauser() {
	CHECK_RET(m_parent.m_pauserCount);
	if (!--m_parent.m_pauserCount) {
		m_parent.resume();
	}
}
