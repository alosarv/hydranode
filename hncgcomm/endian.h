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

#ifndef __ENDIAN_H__
#define __ENDIAN_H__

/**
 * Defines global macros for byte-swapping. Whenever available, headers provided
 * by host platform are used, but falling back to generic code when none are
 * available.
 */

#include <hncgcomm/osdep.h>

#ifdef HAVE_LIBKERN_OSBYTEORDER_H
	#include <libkern/OSByteOrder.h>
	#define SWAP16_ALWAYS(x) OSSwapInt16(x)
	#define SWAP32_ALWAYS(x) OSSwapInt32(x)
	#define SWAP64_ALWAYS(x) OSSwapInt64(x)
#elif defined(HAVE_BYTESWAP_H)
	#include <byteswap.h>
	#define SWAP16_ALWAYS(x) bswap_16(x)
	#define SWAP32_ALWAYS(x) bswap_32(x)
	#define SWAP64_ALWAYS(x) bswap_64(x)
#else
	#define SWAP16_ALWAYS(val) (                              \
		(((uint16_t) (val) & (uint16_t) 0x00ffU) << 8)  | \
		(((uint16_t) (val) & (uint16_t) 0xff00U) >> 8))

	#define SWAP32_ALWAYS(val) (                                  \
		(((uint32_t) (val) & (uint32_t) 0x000000ffU) << 24) | \
		(((uint32_t) (val) & (uint32_t) 0x0000ff00U) <<  8) | \
		(((uint32_t) (val) & (uint32_t) 0x00ff0000U) >>  8) | \
		(((uint32_t) (val) & (uint32_t) 0xff000000U) >> 24))

	#define SWAP64_ALWAYS(val) (                                           \
		(((uint64_t) (val) & (uint64_t) 0x00000000000000ffULL) << 56) |\
		(((uint64_t) (val) & (uint64_t) 0x000000000000ff00ULL) << 40) |\
		(((uint64_t) (val) & (uint64_t) 0x0000000000ff0000ULL) << 24) |\
		(((uint64_t) (val) & (uint64_t) 0x00000000ff000000ULL) <<  8) |\
		(((uint64_t) (val) & (uint64_t) 0x000000ff00000000ULL) >>  8) |\
		(((uint64_t) (val) & (uint64_t) 0x0000ff0000000000ULL) >> 24) |\
		(((uint64_t) (val) & (uint64_t) 0x00ff000000000000ULL) >> 40) |\
		(((uint64_t) (val) & (uint64_t) 0xff00000000000000ULL) >> 56))
#endif

#if defined(__BIG_ENDIAN__)
	#define SWAP16_ON_BE(val) SWAP16_ALWAYS(val)
	#define SWAP32_ON_BE(val) SWAP32_ALWAYS(val)
	#define SWAP64_ON_BE(val) SWAP64_ALWAYS(val)
	#define SWAP16_ON_LE(val) (val)
	#define SWAP32_ON_LE(val) (val)
	#define SWAP64_ON_LE(val) (val)
#else
	#define SWAP16_ON_BE(val) (val)
	#define SWAP32_ON_BE(val) (val)
	#define SWAP64_ON_BE(val) (val)
	#define SWAP16_ON_LE(val) SWAP16_ALWAYS(val)
	#define SWAP32_ON_LE(val) SWAP32_ALWAYS(val)
	#define SWAP64_ON_LE(val) SWAP64_ALWAYS(val)
#endif

#endif /* __ENDIAN_H__ */
