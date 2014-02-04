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

// Note: copied from hnbase/utils.cpp to avoid making hncgcomm library depend
//       on hnbase library.

#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <hncgcomm/osdep.h>

namespace Utils {

// Produces standard hexdump to output stream of given data
// The expected output is something like this:
/*
00000000  3c 3f 78 6d 6c 20 76 65  72 73 69 6f 6e 3d 22 31  |<?xml version="1|
00000010  2e 30 22 20 65 6e 63 6f  64 69 6e 67 3d 22 55 54  |.0" encoding="UT|
00000020  46 2d 38 22 3f 3e 0d 0a  3c 21 2d 2d 43 2b 2b 42  |F-8"?>..<!--C++B|
*/
void hexDump(std::ostream &o, const std::string &data) {
	uint32_t pos = 0;
	int lpos = 0;
	while (pos <= data.size()) try {
		lpos = 0;
		o << std::hex;
		o.fill('0');
		o.width(8);
		// this is here to trigger exception if we'r out of
		// range already at this point
		data.at(pos);
		o << pos << " ";
		for (uint8_t i = 0; i < 8; ++i, ++lpos, pos++) {
			o << " ";
			int c = static_cast<int>(data.at(pos));
			if ((c < 0 ? c += 256 : c) < 16) {
				o << "0";
			}
			o << c;
		}
		o << " ";
		for (uint8_t i = 0; i < 8; ++i, ++lpos, pos++) {
			o << " ";
			int c = static_cast<int>(data.at(pos));
			if ((c < 0 ? c += 256 : c) < 16) {
				o << "0";
			}
			o << c;
		}
		o << "  ";
		o << "|";
		pos -= 16;
		for (uint8_t i = 0; i < 16; ++i, ++pos) {
			uint8_t c = data.at(pos);
			o.put((c > 32 && c < 127) ? c : '.');
		}
		o << "|" << std::endl;
	} catch (std::out_of_range &) {
		if (!(data.size() % 16)) {
			break;
		}
		int curpos = 12+3*lpos;
		if (lpos < 8) {
			--curpos;
		}
		for (uint8_t i = 0; i < 60-curpos; ++i) {
			o << " ";
		};
		o << " |";
		pos -= lpos;
		for (uint8_t i = 0; i < lpos; ++i, ++pos) {
			uint8_t c = data.at(pos);
			if (c > 32 && c < 127) {
				o << c;
			} else {
				o << ".";
			}
		}
		o << "|" << std::endl;
		break;
	}
	o << std::dec;
}

std::string hexDump(const std::string &data) {
	std::ostringstream o;
	o << std::endl;
	hexDump(o, data);
	return o.str();
}

}
