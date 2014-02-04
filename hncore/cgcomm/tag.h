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

#ifndef __TAG_H__
#define __TAG_H__

#include <hnbase/utils.h>

template<typename T>
struct Tag {
	Tag(uint8_t opcode, const T &data) : m_opcode(opcode), m_data(data) {}

	template<typename Type>
	friend std::ostream& operator<<(std::ostream &o, const Tag<Type> &t);

	uint8_t m_opcode;
	T m_data;
};

template<typename T>
inline std::ostream& operator<<(std::ostream &o, const Tag<T> &t) {
	Utils::putVal<uint8_t>(o, t.m_opcode);
	Utils::putVal<uint16_t>(o, sizeof(t.m_data));
	Utils::putVal<T>(o, t.m_data);
	return o;
}

template<>
inline std::ostream& operator<<(std::ostream &o, const Tag<std::string> &t) {
	Utils::putVal<uint8_t>(o, t.m_opcode);
	Utils::putVal<uint16_t>(o, t.m_data.size());
	Utils::putVal<std::string>(o, t.m_data.data(), t.m_data.size());
	return o;
}

template<typename T>
inline Tag<T> makeTag(uint8_t opcode, const T &data) {
	return Tag<T>(opcode, data);
}

#endif
