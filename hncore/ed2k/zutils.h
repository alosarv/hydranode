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
 * \file zutils.h
 * C++ wrapper functions/classes for compression-related functionality
 */

#ifndef __ED2K_ZUTILS_H__
#define __ED2K_ZUTILS_H__

#include <hnbase/osdep.h>

//! @name ZLIB compression/decompression methods
namespace Zlib {

/**
 * Compresses the passed data, returning compressed data.
 *
 * @param input         Data to be compressed
 * @return              Compressed data. May be empty if compression fails.
 */
extern std::string compress(const std::string &input);

/**
 * Decompresses passed data.
 *
 * @param input         Data to be uncompressed
 * @param bufsize       Suggested buffer size. If left at default value, the
 *                      method attempts to guess the internal buffer size for
 *                      unpacking itself.
 * @return              Unpacked data, or empty string if something goes wrong
 *
 * \throws std::runtime_error if decompression fails
 *
 * \note This function may call itself recursivly if the destination buffer size
 *       was not big enough to unpack the data (thus the second parameter also).
 *       The recursion ends either when the unpacking succeeds, or when
 *       destination buffer size exceeds 1MB. If you want to decompress data
 *       resulting in larger amount of unpacket data than 1MB, explicitly
 *       specify the buffer size as parameter.
 */
extern std::string decompress(const std::string &input, uint32_t bufsize = 0);

}

#endif
