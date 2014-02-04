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

#include <hnbase/event.h>
#include <iostream>
#include <boost/shared_ptr.hpp>
#include <boost/intrusive_ptr.hpp>

// intrusive-ptr requirements
// --------------------------

// experiments ... technically, this would give us back the nice free-function
// syntax, w/o the stupid getEventTable() calls; however, the problems is
// aquiring the exact pointer to the right event table - no probs on linux, but
// osx and win32 dynamic loaders have trouble resolving these things over
// modules, so duplicate eventtable singletons are instanciated, and modules
// access different tables than core.
namespace EHS {
	template<typename Source, typename Event>
	void postEvent(Source src, Event evt) {
		typedef typename boost::remove_pointer<Source>::type Src;
		Src::getEventTable().postEvent(src, evt);
	}
	template<typename Source, typename Event>
	void postEvent(boost::shared_ptr<Source> src, Event evt) {
		Source::getEventTable().postEvent(src, evt);
	}
	template<typename Source, typename Event>
	void postEvent(boost::intrusive_ptr<Source> src, Event evt) {
		Source::getEventTable().postEvent(src, evt);
	}
	template<typename Source, typename Event, typename Handler>
	boost::signals::connection addHandler(
		Source src, Handler *obj,
		void (Handler::*func)(Source, Event)
	) {
		typedef typename boost::remove_pointer<Source>::type Src;
		return Src::getEventTable().addHandler(src, obj, func);
	}
	template<typename Source, typename Event, typename Handler>
	boost::signals::connection addHandler(
		boost::shared_ptr<Source> src, Handler *obj,
		void (Handler::*func)(Source, Event)
	) {
		return Source::getEventTable().addHandler(src, obj, func);
	}
	template<typename Source, typename Event, typename Handler>
	boost::signals::connection addHandler(
		boost::intrusive_ptr<Source> src, Handler *obj,
		void (Handler::*func)(Source, Event)
	) {
		return Source::getEventTable().addHandler(src, obj, func);
	}
}

// Event object. This is the event being passed from MySource to MyHandler
// through event tables.
class MyEvent {
public:
	MyEvent(const std::string &msg) : m_data(msg) {}
	friend std::ostream& operator<<(std::ostream &o, const MyEvent &evt) {
		return o << evt.m_data;
	}
	std::string m_data;
};

// Event source class. Notice how there is no references to event tables in
// this class.
class MySource {
public:
	// required by EHSv1 and EHSv2, but locks our source to single event,
	// single-source type. EHSv2 was supposed to get rid of it, but due to
	// cross-platform issues, it didn't ...
	DECLARE_EVENT_TABLE(MySource*, MyEvent);

	MySource() {
		// Post an event during construction to test the usage of `this'
		EHS::postEvent(this, MyEvent("Constructing MySource"));
	}
	~MySource() {
		// Post an event in destructor to verify we don't crash this way
		EHS::postEvent(this, MyEvent("Destroying MySource"));
	}
};
IMPLEMENT_EVENT_TABLE(MySource, MySource*, MyEvent);

// Event handler class
class MyHandler {
public:
	MyHandler() {}
	// Overloaded event handler, used for different ways MySource posts
	// events.
	void handler(MySource *, MyEvent evt) {
		std::cerr << "Received event: " << evt << std::endl;
	}
	void handler(boost::intrusive_ptr<MySource>, MyEvent evt) {
		std::cerr << "Received (intrusive) event: " << evt << std::endl;
	}
	void handler(boost::shared_ptr<MySource>, MyEvent evt) {
		std::cerr << "Received (shared) event: " << evt << std::endl;
	}
};

void test2(); // second test

// Driver
int main() {
	// Initialize objects.
	MyHandler *h = new MyHandler;
	MySource *s = new MySource;

	// Set up handler, and post an event "externally"
	EHS::addHandler(s, h, &MyHandler::handler);
	EHS::postEvent(s, MyEvent("Hello world!"));

	delete s; // delete the source

	EventMain::instance().process();

// doesn't work with EHSv2
// 	{ // event source is in intrusive_ptr wrapper
// 		boost::intrusive_ptr<MySource> src(new MySource);
// 		EHS::addHandler(src, h, &MyHandler::handler);
// 		EHS::addHandler(src.get(), h, &MyHandler::handler);
// 		EHS::postEvent(src, MyEvent("Intrusive hello!"));
// 		EventMain::instance().process();
// 	}
//
// 	{ // event source is in shared_ptr wrapper
// 		boost::shared_ptr<MySource> src(new MySource);
// 		EHS::addHandler(src, h, &MyHandler::handler);
// 		EHS::addHandler(src.get(), h, &MyHandler::handler);
// 		EHS::postEvent(src, MyEvent("Shared hello!"));
// 		EventMain::instance().process();
// 	}


	delete h;

	test2();

#ifdef WIN32
	// (just don't close the console window yet (win32-specific))
	std::cin.get();
#endif
}

// tests automatic signals disconnection
struct Y;
struct X : public Trackable {
	void onAllEvent(Y*, int) { logFatalError("onAllEvent!"); }
	void onEvent(Y*, int) { logFatalError("onEvent!"); }
};
struct Y : public Trackable { DECLARE_EVENT_TABLE(Y*, int); };
IMPLEMENT_EVENT_TABLE(Y, Y*, int);
void test2() {
	X *x = new X;
	Y y;
	boost::signals::connection c1 = Y::getEventTable().addAllHandler(x, &X::onAllEvent);
	boost::signals::connection c2 = Y::getEventTable().addHandler(&y, x, &X::onEvent);
	delete x;
	Y::getEventTable().postEvent(&y, 0);
	Y::getEventTable().process();
}
