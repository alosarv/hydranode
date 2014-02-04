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
 * \file zutils.cpp Implementation of Zlib-related utility methods
 */

#include <hncore/ed2k/zutils.h>
#include <hnbase/log.h>
#include <hnbase/utils.h>
#include <zlib.h>       // for zlib compression

namespace Zlib {

// Pack data using zlib
std::string compress(const std::string &input) {
	uLongf destLen = input.size() + 300;
	boost::scoped_array<Bytef> dest(new Bytef[destLen]);
	int ret = compress2(
		dest.get(), &destLen,
		reinterpret_cast<const Bytef*>(input.data()), input.size(),
		Z_BEST_COMPRESSION
	);
	std::string tmp(reinterpret_cast<const char*>(dest.get()), destLen);

	if (ret == Z_OK) {
		return tmp;
	} else {
		switch (ret) {
		case Z_MEM_ERROR:
			logDebug(
				"ZError: Not enough memory to compress packet."
			);
			break;
		case Z_BUF_ERROR:
			logDebug(
				"ZError: Not enough buffer to compress packet."
			);
			break;
		case Z_STREAM_ERROR:
			logError(
				"ZError: Invalid compression level "
				"for compression."
			);
			break;
		default:
			break;
		}
		return std::string();
	}
}

// unpack data using zlib
std::string decompress(const std::string &input, uint32_t bufsize /* = 0 */) {
	uLongf destLen = bufsize;
	if (destLen == 0) {
		destLen = input.size() * 10 + 300;
	}
	boost::scoped_array<Bytef> dest(new Bytef[destLen]);
	memset(dest.get(), 0, destLen);
	int ret = uncompress(
		dest.get(), &destLen,
		reinterpret_cast<const Bytef*>(input.data()), input.size()
	);
	std::string tmp(reinterpret_cast<const char*>(dest.get()), destLen);
	if (ret == Z_OK) {
		return tmp;
	}
	// Else
	boost::format fmt(
		"Unpacking packet: inputSize=%s buffersize=%s: Error: `%s' %s"
	);
	fmt % Utils::hexDump(input.size()) % Utils::hexDump(destLen);
	switch (ret) {
		case Z_MEM_ERROR:
			fmt % "Not enough memory to decompress packet.";
			break;
		case Z_BUF_ERROR: {
			fmt % "Not enough buffer to decompress packet.";
			// call ourself again, increase buffer 10x
			bool ok = false;
			while (!ok) {
				try {
					tmp = decompress(input, destLen*10);
					ok = true;
				} catch (std::runtime_error&) {
					if (destLen > 1024*1024) {
						// don't go over 1 MB bufsize
						break;
					}
				}
			}
			if (ok) {
				return tmp;
			} // else falls through and throws exception
			break;
		}
		case Z_DATA_ERROR:
			fmt % "Input corrupt or incomplete.";
			break;
		case Z_ERRNO:
			fmt % "Z_ERRNO";
			break;
		case Z_VERSION_ERROR:
			fmt % "Z_VERSION_ERROR";
			break;
		case Z_STREAM_ERROR:
			fmt % "Z_STREAM_ERROR";
			break;
		default:
			fmt % "Unknown error.";
			break;
	}
	fmt % Utils::hexDump(input);
	throw std::runtime_error(fmt.str());
}

}
