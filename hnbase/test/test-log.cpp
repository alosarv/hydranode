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
 * @file test-log.cpp Test App for Logging Subsystem
 */


#ifndef DOXYGEN_SHOULD_SKIP_THIS

#include <iostream>
#include <string>
#include <boost/format.hpp>
#include <hnbase/log.h>

int main() {
	static const int LOGTRACE = 1;
	logMsg(boost::format("Hello %s, This is logtest#%d") % "Madcat" % 10);
	logDebug("This is a debug message.");
	logDebug(boost::format("This is %dnd debug message.") % 2);

	// Trace logs - with int mask
	logTrace(LOGTRACE, "This will not be seen.");
	Log::instance().enableTraceMask(LOGTRACE, "LogTrace");
	logTrace(LOGTRACE, "This is a Log Trace.");
	Log::instance().disableTraceMask(LOGTRACE);
	logTrace(LOGTRACE, "This will not be seen.");

	// Trace logs - with string mask
	Log::instance().addTraceMask("Trace");
	Log::instance().enableTraceMask("Trace");
	logTrace("Trace", "This is seen.");
	Log::instance().disableTraceMask("Trace");
	logTrace("Trace", "This should not be seen.");
	Log::instance().enableTraceMask("Trace");
	Log::instance().removeTraceMask("Trace");
	logTrace("Trace", "This should not be seen.");
}

#endif
