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
 * \file eventbase.cpp Implementation of Event Handling Subsystem
 */

#include <hnbase/pch.h>
#include <hnbase/eventbase.h>
#include <hnbase/gettickcount.h>

EventMain::EventMain() : m_running(true), m_curTick(Utils::getTick()) { }

EventMain& EventMain::instance() {
	static EventMain s_eMain;
	return s_eMain;
}

void EventMain::mainLoop() {
	while (true) {
		EventTableBase::waitForEvent();
		process();
		boost::mutex::scoped_lock l(m_runningLock);
		if (!m_running) {
			break;
		}
	}
}

void EventMain::process() {
	m_curTick = Utils::getTick();
	for (LIter i = m_list.begin(); i != m_list.end(); i++) {
		(*i)->process();
	}
}

void EventMain::exitMainLoop() {
	boost::mutex::scoped_lock l(m_runningLock);
	m_running = false;
	EventTableBase::notify();
}

// EventTableBase class
//
boost::mutex EventTableBase::s_notifyLock;
boost::condition EventTableBase::s_eventNotify;
bool EventTableBase::s_hasPending = false;

EventTableBase::EventTableBase() {
	EventMain::instance().addEventTable(this);
}

EventTableBase::~EventTableBase() {
//	EventMain::instance().removeEventTable(this);
}

void EventTableBase::notify() {
	boost::mutex::scoped_lock l(s_notifyLock);
	s_eventNotify.notify_all();
	s_hasPending = true;
}

void EventTableBase::waitForEvent() {
	boost::mutex::scoped_lock l(s_notifyLock);
	boost::xtime xt;
	boost::xtime_get(&xt, boost::TIME_UTC);
	xt.nsec += 300000000ULL; // 300ms wait delay
	s_eventNotify.timed_wait(l, xt);
}

bool EventTableBase::hasPending() {
	boost::mutex::scoped_lock l(s_notifyLock);
	return s_hasPending;
}

void EventTableBase::setNoPending() {
	boost::mutex::scoped_lock l(s_notifyLock);
	s_hasPending = false;
}
