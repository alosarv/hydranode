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
 * \file publickey.h Interface for PublicKey class
 */

#ifndef __ED2K_PUBLICKEY_H__
#define __ED2K_PUBLICKEY_H__

#include <hnbase/osdep.h>
#include <boost/shared_array.hpp>

namespace Donkey {

/**
 * PublicKey indicates an entities public RSA key. PublicKey objects share
 * the underlying data and are thus inexpensive to copy. PublicKey object's
 * contents can never be modified.
 */
class PublicKey {
public:
	//! Dummy default constructor
	PublicKey() : m_keyLen() {}

	//! Construct from string data
	PublicKey(const std::string &key)
	: m_key(new uint8_t[key.size()]), m_keyLen(key.size()) {
		memcpy(m_key.get(), key.c_str(), key.size());
	}

	uint8_t* c_str() const { return m_key.get(); }
	size_t   size()  const { return m_keyLen;    }

	//! implicit conversion to bool checks for contained data
	operator bool()  const { return m_key; }

	//! Clears the contents of this key
	void clear() { m_key.reset(); m_keyLen = 0; }

	//! inequality operator to bool
	friend bool operator!(const PublicKey &p) { return !p.m_key; }

	friend bool operator<(const PublicKey &x, const PublicKey &y) {
		if ((x.m_key && !y.m_key) || (!x.m_key && y.m_key)) {
			return false;
		}
		if (x.m_keyLen != y.m_keyLen) {
			return x.m_keyLen < y.m_keyLen;
		}
		return memcmp(x.m_key.get(), y.m_key.get(), x.m_keyLen) < 0;
	}
	friend bool operator==(const PublicKey &x, const PublicKey &y) {
		if (x.m_keyLen != y.m_keyLen) {
			return false;
		}
		return !memcmp(x.m_key.get(), y.m_key.get(), x.m_keyLen);
	}
	friend bool operator!=(const PublicKey &x, const PublicKey &y) {
		return !(x == y);
	}
private:
	boost::shared_array<uint8_t> m_key;
	uint8_t m_keyLen;
};

} // end namespace Donkey

#endif
