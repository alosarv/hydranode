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

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <QString>
#ifndef _MSC_VER
	#include <stdint.h>
#else
	typedef quint8 uint8_t;
	typedef quint16 uint16_t;
	typedef quint32 uint32_t;
	typedef quint64 uint64_t;
	typedef qint8 int8_t;
	typedef qint16 int16_t;
	typedef qint32 int32_t;
	typedef qint64 int64_t;
#endif

QString hexDump(const QString &input) {
	uint32_t pos = 0;
	int lpos = 0;
	std::ostringstream o;
	std::string data(input.toStdString());

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
	return QString(o.str().c_str());
}
