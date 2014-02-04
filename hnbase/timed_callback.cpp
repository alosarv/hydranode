/*
 *  Copyright (C) 2005 Andrea Leofreddi <andrea.leofreddi@libero.it>
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
 * \file timed_callback.h Timed callbacks implementation
 */

#include <hnbase/timed_callback.h>

namespace Utils {

TimedCallback *TimedCallback::s_instance;

IMPLEMENT_EVENT_TABLE(TimedCallback, TimedCallback*, TimedCallback::EventType);

TimedCallback::TimedCallback() {
	getEventTable().addHandler(this, this, &TimedCallback::onEvent);
}

TimedCallback::~TimedCallback() {
	getEventTable().delHandlers(this);
}

TimedCallback& TimedCallback::instance() {
	if(!s_instance) {
		s_instance = new Utils::TimedCallback;
	}

	return *s_instance;
}

}
