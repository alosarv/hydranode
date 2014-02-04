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
 * \file main.cpp Main application entry point.
 */

#include <hncore/pch.h>
#include <hnbase/log.h>
#include <hncore/hydranode.h>

/** Self-checks to make sure all types are correctly defined */
BOOST_STATIC_ASSERT(sizeof(uint8_t) == 1);
BOOST_STATIC_ASSERT(sizeof(uint16_t) == 2);
BOOST_STATIC_ASSERT(sizeof(uint32_t) == 4);
BOOST_STATIC_ASSERT(sizeof(uint64_t) == 8);
BOOST_STATIC_ASSERT(sizeof(int8_t) == 1);
BOOST_STATIC_ASSERT(sizeof(int16_t) == 2);
BOOST_STATIC_ASSERT(sizeof(int32_t) == 4);
BOOST_STATIC_ASSERT(sizeof(int64_t) == 8);
#ifndef _WIN32
	BOOST_STATIC_ASSERT(sizeof(off_t) == 8);
#endif

extern void initSignalHandlers(boost::function<void ()>, bool);

// Application entry point
int main(int argc, char *argv[]) {
	initSignalHandlers(
		boost::bind(&Hydranode::exit, &Hydranode::instance()), 
#ifdef WIN32 // no auto-trace on win32
		false
#else
		true
#endif
	);

	return Hydranode::instance().run(argc, argv);
}
