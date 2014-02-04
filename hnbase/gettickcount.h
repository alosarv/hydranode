/*
 * Name:        common/timercmn.cpp
 * Purpose:     Common timer implementation
 * Author:
 *    Original version by Julian Smart
 *    Vadim Zeitlin got rid of all ifdefs (11.12.99)
 *    Sylvain Bougnoux added wxStopWatch class
 *    Guillermo Rodriguez <guille@iies.es> rewrote from scratch (Dic/99)
 * Modified by:
 * Created:     04/01/98
 * RCS-ID:      $Id: gettickcount.h 2718 2006-02-27 17:06:15Z madcat $
 * Copyright:   (c) Julian Smart and Markus Holzem
 *              (c) 1999 Guillermo Rodriguez <guille@iies.es>
 * Licence:     wxWindows license
 */

/**
 * \file gettickcount.h Portable gettimeofday clone function.
 */


#ifndef __GETTICKCOUNT_H__
#define __GETTICKCOUNT_H__

#include <hnbase/osdep.h>

#if defined(WIN32) || defined(_MSC_VER)
	// Prevents inclusion of winsock1 API when this file is included before
	// winsock2.h gets included.
	#include <windows.h>
	// No, thank you very much, we do NOT need these macros.
	#undef max
	#undef min
#elif defined(BOOST_HAS_GETTIMEOFDAY)
	#include <sys/time.h>
#elif defined(__MACH__)
	#include <Timer.h>
	#include <DriverServices.h>
	#undef check   // we don't want this macro defined
	#undef verify  // we don't want this macro defined
#elif defined(HAVE_FTIME)
	#include <sys/timeb.h>
#endif

namespace Utils {

/**
 * getTick() function copied from wxWidgets library wxGetLocalTimeMillis()
 * function (from src/common/timercmn.cpp)
 */
inline uint64_t getTick() {
#if defined(WIN32) || defined(_MSC_VER)
	// 00:00:00 Jan 1st 1970
	SYSTEMTIME thenst = { 1970, 1, 4, 1, 0, 0, 0, 0 };
	FILETIME thenft;
	SystemTimeToFileTime(&thenst, &thenft);

	// time in 100 nanoseconds
	uint64_t then = thenft.dwHighDateTime;
	then <<= 32;
	then += thenft.dwLowDateTime;

	SYSTEMTIME nowst;
	GetLocalTime(&nowst);
	FILETIME nowft;
	SystemTimeToFileTime(&nowst, &nowft);

	// time in 100 nanoseconds
	uint64_t now = nowft.dwHighDateTime;
	now <<= 32;
	now += nowft.dwLowDateTime;

	// time from 00:00:00 Jan 1st 1970 to now in milliseconds
	return (now - then) / 10000;

#elif defined(BOOST_HAS_GETTIMEOFDAY)
	uint64_t val = 1000l;
	struct timeval tp;
	if (gettimeofday(&tp, (struct timezone *)0) != -1) {
		val *= tp.tv_sec;
		return (val + (tp.tv_usec / 1000));
	} else {
		return 0;
	}
#elif defined(BOOST_HAS_FTIME)
	uint64_t val = 1000l;
	struct timeb tp;

	// ftime() is void and not int in some mingw32 headers, so don't
	// test the return code (well, it shouldn't fail anyhow...)
	(void)ftime(&tp);
	val *= tp.time;
	return (val + tp.millitm);
#elif defined(__MACH__)
	uint64_t val = 1000l;
	static UInt64 gMilliAtStart = 0;

	Nanoseconds upTime = AbsoluteToNanoseconds( UpTime() );

	if (gMilliAtStart == 0) {
	    time_t start = time(NULL);
	    gMilliAtStart = ((UInt64) start) * 1000000L;
	    gMilliAtStart -= upTime.lo / 1000 ;
	    gMilliAtStart -= ( ( (UInt64) upTime.hi ) << 32 ) / (1000 * 1000);
	}

	UInt64 millival = gMilliAtStart;
	millival += upTime.lo / (1000 * 1000);
	millival += ( ( (UInt64) upTime.hi ) << 32 ) / (1000 * 1000);
	val = millival;

	return val;
#else
	#error GetTickCount not implemented for your platform.
#endif
}

} // namespace Utils

#endif
