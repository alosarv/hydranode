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
 * \file pch.h Precompiled header file
 */

#ifdef HAVE_PCH
	#ifdef WIN32
		#pragma once
		#define _WINSOCKAPI_
		#include <windows.h>
	#endif
	#include <hn/osdep.h>
	#include <hn/gettickcount.h>
	#include <map>
	#include <set>
	#include <list>
	#include <vector>
	#include <deque>
	#include <string>
	#include <iostream>
	#include <fstream>
	#include <boost/format.hpp>
	#include <boost/thread.hpp>
	#include <boost/signal.hpp>
	#include <boost/multi_index_container.hpp>
	#include <boost/multi_index/key_extractors.hpp>
	#include <boost/lambda/lambda.hpp>
	#include <boost/lambda/if.hpp>
	#include <boost/lambda/bind.hpp>
#endif
