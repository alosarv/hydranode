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

#ifndef __OSDEP_H__
#define __OSDEP_H__

// Exports/imports
#ifdef _WIN32
	#define EXPORT __declspec(dllexport)
	#define IMPORT __declspec(dllimport)
	#ifdef __MODULE__
		#define DLLEXPORT IMPORT
		#define DLLIMPORT EXPORT
	#else
		#define DLLEXPORT EXPORT
		#define DLLIMPORT IMPORT
	#endif
#else
	#define EXPORT
	#define IMPORT
	#define DLLEXPORT
	#define DLLIMPORT
#endif

// Useless MSVC warnings
#ifdef _MSC_VER
	// boost::thread exceptions don't compile with this enabled
	#pragma warning(disable:4275)
	// Another warning from boost::threads - we don't care about it
	#pragma warning(disable:4251)
	// Forcing int to bool
	#pragma warning(disable:4800)
	// conversion warnings - just too many of them
	#pragma warning(disable:4244)
	#pragma warning(disable:4267)
	// 'this' used in base member initializer list
	#pragma warning(disable:4355)
#endif

// integer types
#ifdef _MSC_VER
	typedef unsigned char      uint8_t;
	typedef unsigned short     uint16_t;
	typedef unsigned int       uint32_t;
	typedef unsigned long long uint64_t;
	typedef signed char        int8_t;
	typedef signed short       int16_t;
	typedef signed int         int32_t;
	typedef signed long long   int64_t;
#else
	#include <stdint.h>
#endif

#endif
