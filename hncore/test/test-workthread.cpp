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
 * \file test-workthread.cpp WorkThread API regress test
 */

#include <hncore/workthread.h>
#include <hnbase/event.h>
#include <boost/enable_shared_from_this.hpp>

//! Custom work to be performed
class MyWork : public ThreadWork, public boost::enable_shared_from_this<MyWork>{
public:
	DECLARE_EVENT_TABLE(boost::shared_ptr<MyWork>, int);
	MyWork(int id) : m_id(id), m_count(0) {}
	virtual bool process();
	int getId() const { return m_id; }
private:
	int m_id;
	uint32_t m_count;
};
bool MyWork::process() {
	logMsg(boost::format("Processing work %d") % m_id);
	if (++m_count > 5) {
		setComplete();
		getEventTable().postEvent(shared_from_this(), 0);
		return true;
	} else {
		return false;
	}
}
IMPLEMENT_EVENT_TABLE(MyWork, boost::shared_ptr<MyWork>, int);

//! Counts number of jobs completed successfully
int handled = 0;

//! Event handler for jobs. Notice that we must dynamically upcast the work
//! object pointer.
void handler(ThreadWorkPtr wrk, int event) {
	if (event == 0) {
		++handled;
		logMsg(
			boost::format("Work %d complete.")
			% boost::dynamic_pointer_cast<MyWork>(wrk)->getId()
		);
	}
}

//! Submit number of jobs and let the WorkThread to process them
int main() {
	MyWork::getEventTable().addAllHandler(&handler);
	for (uint32_t i = 0; i < 5; ++i) {
		ThreadWorkPtr wrk(new MyWork(i));
		logMsg(boost::format("Posting work %d") % i);
		WorkThread::instance().postWork(wrk);
		EventMain::instance().process();
	}
	while (handled < 5) {
		EventMain::instance().process();
	}
	logMsg("All jobs processed successfully.");
}
