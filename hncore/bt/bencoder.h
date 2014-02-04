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

/**
 * \file bencoder.h       Interface for Bencoder class
 */

#ifndef __BT_BENCODER_H__
#define __BT_BENCODER_H__

#include <hnbase/osdep.h>
#include <string>
#include <boost/shared_ptr.hpp>
#include <map>
#include <vector>

namespace Bt {

class BenCoder;

//! A bencoded dictionary
typedef std::map<std::string, BenCoder> BDict;
//! A bencoded list
typedef std::vector<BenCoder> BList;

/**
 * Bencoder encodes input data, using `bencoding' system, as defined by
 * BitTorrent protocol specification found at
 * http://wiki.theory.org/BitTorrentSpecification
 *
 * Summary:
 * \code
 * Integers:      i[value]e
 * Strings:       [length]:[value]
 * Lists:         l[value]:[value]:[value]e
 * Dictionaries:  d[key]:[value]:[key]:[value]e
 * \endcode
 * \note Lists and Maps may contain other lists and maps as well.
 *
 * \example
 * \code
 * BDict map;
 * map["hello"] = 10;
 * map["world"] = "doh";
 * std::cerr << map << std::endl; // outputs "d5:helloi10e5:world3:dohe"
 * \endcode
 */
class BenCoder {
public:
	/**
	 * Default constructor
	 */
	BenCoder();

	/**
	 * Bencode an integer value
	 */
	BenCoder(const int32_t &data);

	/**
	 * Bencode a string value
	 */
	BenCoder(const std::string &data);

	/**
	 * Same as above, for null-terminated strings
	 */
	BenCoder(const char *data);

	/**
	 * Bencode a list of data
	 */
	BenCoder(const BList &data);

	/**
	 * Bencode a map of data
	 */
	BenCoder(const BDict &data);

	/**
	 * Outputs the resulting bencoded stream to designated stream
	 *
	 * @param o         Stream to write to
	 * @param b         Bencoder to write
	 */
	friend std::ostream& operator<<(std::ostream &o, const BenCoder &b);
private:
	//! Keeps a reference to the bencoded stream
	boost::shared_ptr<std::ostringstream> m_data;
};

} // end namespace Bt

#endif
