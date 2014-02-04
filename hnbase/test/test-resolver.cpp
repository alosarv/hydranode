/*
 *  Copyright (C) 2005-2006 Alo Sarv <madcat_@users.sourceforge.net>
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
 * \file test-resolver.cpp Resolver test
 */

#include <iostream>
#include <hnbase/hostinfo.h>
#include <hnbase/timed_callback.h>

//class IMPORT HostInfo;
//void IMPORT lookup(const std::string&, HostInfo::HandlerType);

int pending = 0;
void foo(HostInfo info) {
	--pending;
	logMsg(boost::format("Name: %s") % info.getName());
	logMsg(boost::format("ErrorState: %s (%s)") % info.error() % info.errorMsg());
	for (HostInfo::Iter it = info.begin(); it != info.end(); ++it) {
		logMsg(boost::format("Address: %s") % (*it).getAddrStr());
	}
	std::vector<std::string> aliases = info.getAliases();
	for (uint32_t i = 0; i < aliases.size(); ++i) {
		logMsg(boost::format("Alias: %s") % aliases[i]);
	}
}

int main(int argc, char *argv[]) {
	Utils::TimedCallback::instance();
	for (int i = 1; i < argc; ++i) {
		++pending;
		DNS::lookup(argv[i], boost::bind(&foo, _1));
	}
	while (pending) {
		EventMain::instance().process();
	}
	return 0;
}
