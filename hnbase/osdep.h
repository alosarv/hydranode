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
 * \file osdep.h Various os/compiler specific stuff
 */

#ifndef __OSDEP_H__
#define __OSDEP_H__

// do compiler version check before including anything else to avoid errors from
// boost headers early on
#ifdef __GNUC__
	#if __GNUC__ < 3 || (__GNUC__ == 3 && __GNUC_MINOR__ < 2)
		#error ==== This application requires GCC 3.2 or newer ====
	#endif
#endif
#include <boost/version.hpp>
#if BOOST_VERSION < 103301
	#error === This application requires Boost 1.33.1 (or newer) headers ===
#endif

#include <boost/format.hpp>
#include <boost/config.hpp>
#include <boost/cstdint.hpp>

#ifndef BOOST_HAS_STDINT_H
	using boost::uint8_t;
	using boost::uint16_t;
	using boost::uint32_t;
	using boost::uint64_t;
	using boost::int8_t;
	using boost::int16_t;
	using boost::int32_t;
	using boost::int64_t;
#endif

//! Ensure that both _WIN32 and WIN32 defined, if one of them is defined
//! (needed when building on mingw32, where _WIN32 is defined, but WIN32 isn't)
#if defined(_WIN32) && !defined(WIN32)
	#define WIN32
#elif defined(WIN32) && !defined(_WIN32)
	#define WIN32
#endif

//! Client version information
enum VersionInfo {
	APPVER_MAJOR = 0,
	APPVER_MINOR = 3,
	APPVER_PATCH = 0
};

// Exports/imports
#if defined(WIN32) && !defined(STATIC_BUILD)
	#define EXPORT __declspec(dllexport)
	#define IMPORT __declspec(dllimport)
	#ifdef BUILDING_HNBASE
		#define HNBASE_EXPORT EXPORT
	#else
		#define HNBASE_EXPORT IMPORT
	#endif
	#ifdef BUILDING_HNCORE
		#define HNCORE_EXPORT EXPORT
	#else
		#define HNCORE_EXPORT IMPORT
	#endif
#else
	#define EXPORT
	#define IMPORT
	#define HNBASE_EXPORT
	#define HNCORE_EXPORT
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
	// MSVC 8.0 deprecated POSIX names for open(), write() etc, but
	// since we need to use them on other platforms, disable this warning.
	#pragma warning(disable:4996)
#endif

// IO-related functions; slightly different on MSVC
#ifdef WIN32
	#include <io.h>
	#define lseek64(fd, pos, dir) _lseeki64(fd, pos, dir)
	#define O_LARGEFILE 0                 // posix compat
	#define fsync(fd) _commit(fd)         // posix compat
	#define _WINSOCKAPI_                  // use winsock2
	#define WIN32_LEAN_AND_MEAN           // disable useless headers
	#define FD_SETSIZE 512                // 512 sockets for select
	#ifndef _INTEGRAL_MAX_BITS            // vs2005 predefines it
		#define _INTEGRAL_MAX_BITS 64 // for stat64
	#endif
#elif defined(__MACH__) || defined(__FreeBSD__) || \
defined(__NetBSD__) || defined( __OpenBSD__ )
	// Mac and BSD lack lseek64, and O_LARGEFILE
	#define lseek64(fd, pos, dir) lseek(fd, pos, dir)
	#define O_LARGEFILE 0
#endif

#ifndef WIN32 // POSIX lacks O_BINARY
	#define O_BINARY 0
#endif

#ifndef __GNUC__
	#define __PRETTY_FUNCTION__ __FUNCTION__
#endif

#ifndef NDEBUG
//! Debugging macro which logs an error if condition fails.
#define CHECK(Cond)                                                           \
	if (!(Cond)) {                                                        \
		logError(                                                     \
			boost::format(                                        \
				"%1%:%2%: `%3%': Check `%4%' failed."         \
			) % __FILE__ % __LINE__ % __PRETTY_FUNCTION__ % #Cond \
		);                                                            \
	}
//! Debugging macro which logs an error and message if condition fails
#define CHECK_MSG(Cond, Message)                                         \
	if (!(Cond)) {                                                   \
		logError(                                                \
			boost::format(                                   \
				"%1%:%2%: `%3%': Check `%4%' failed: %5%"\
			) % __FILE__ % __LINE__ % __PRETTY_FUNCTION__    \
			% #Cond % Message                                \
		);                                                       \
	}
#else // NDEBUG
	#define CHECK(Cond)
	#define CHECK_MSG(Cond, Message)
#endif // NDEBUG/!NDEBUG

//! Aborts the application with message if condition fails
#define CHECK_FAIL(Cond)                                                  \
	if (!(Cond)) {                                                    \
		logFatalError(                                            \
			boost::format(                                    \
				"\nInternal logic error at %1%:%2%, in "  \
				"function `%3%': %4%"                     \
			) % __FILE__ % __LINE__ % __PRETTY_FUNCTION__     \
			% #Cond                                           \
		);                                                        \
	}

//! Throws std::runtime_error if condition is true
//! This is our replacement for assert() for situations where assert() is not
//! appropriate.
#define CHECK_THROW_MSG(Cond, Message)                                   \
	if (!(Cond)) {                                                   \
		throw std::runtime_error(                                \
			(boost::format("%1%:%2%: %3%")                   \
			% __FILE__ % __LINE__ % (Message)).str()         \
		);                                                       \
	}

#define CHECK_THROW(Cond)                                                \
	if (!(Cond)) {                                                   \
		throw std::runtime_error(                                \
			(boost::format("%1%:%2%: Check `%3%' failed")    \
			% __FILE__ % __LINE__ % #Cond).str()             \
		);                                                       \
	}

//! Checks condition and logs and error + returns from function if it fails.
//! Second version of this macro returns a specific value from the function
//! Third version does the same as the ones mentioned above, but let
//! you specify a custom error message.
//! (for usage in functions that need to return a value)
#ifndef NDEBUG
#define CHECK_RET(Cond)                                                  \
	if (!(Cond)) {                                                   \
		logError(                                                \
			boost::format("%1%:%2%: Check `%3%' failed")     \
			% __FILE__ % __LINE__ % #Cond                    \
		);                                                       \
		return;                                                  \
	}
#define CHECK_RETVAL(Cond, RetVal)                                       \
	if (!(Cond)) {                                                   \
		logError(                                                \
			boost::format("%1%:%2%: Check `%3%' failed")     \
			% __FILE__ % __LINE__ % #Cond                    \
		);                                                       \
		return (RetVal);                                         \
	}
#define CHECK_RET_MSG(Cond, Message)                                     \
	if (!(Cond)) {                                                   \
		logError(                                                \
			boost::format("%1%:%2%: %3%")                    \
			% __FILE__ % __LINE__ % Message                  \
		);                                                       \
		return;                                                  \
	}
#else
#define CHECK_RET(Cond)
#define CHECK_RETVAL(Cond, RetVal)
#define CHECK_RET_MSG(Cond, Message)
#endif

/**
 * Ansi color codes. These can be used to colourize text output to terminal
 * or socket. In the future, the check's whether to enable colors or not should
 * be done on runtime (perhaps using a commandline argument?).
 */
#define COL_NONE     "\33[0;0m"
#define COL_BLACK    "\33[0;30m"
#define COL_RED      "\33[0;31m"
#define COL_BRED     "\33[1;31m"
#define COL_GREEN    "\33[0;32m"
#define COL_BGREEN   "\33[1;32m"
#define COL_YELLOW   "\33[0;33m"
#define COL_BYELLOW  "\33[1;33m"
#define COL_BLUE     "\33[0;34m"
#define COL_BBLUE    "\33[1;34m"
#define COL_MAGENTA  "\33[0;35m"
#define COL_BMAGENTA "\33[1;35m"
#define COL_CYAN     "\33[0;36m"
#define COL_BCYAN    "\33[1;36m"
#define COL_WHITE    "\33[0;37m"
#define COL_BWHITE   "\33[1;37m"

//! Compile code only when compiling with GNU GCC
#ifdef __GNUC__
	#define GCC_ONLY(x) x
#else
	#define GCC_ONLY(x)
#endif

//! Compile code only when compiling with Microsoft Visual C++
#ifdef _MSC_VER
	#define MSVC_ONLY(x) x
#else
	#define MSVC_ONLY(x)
#endif

#endif // __OSDEP_H__

