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

#ifndef __ED2K_TAG_H__
#define __ED2K_TAG_H__

#include <hnbase/osdep.h>
#include <boost/any.hpp>
#include <boost/format.hpp>
#include <iosfwd>
#include <stdexcept>
#include <string>

namespace Donkey {

/**
 * Exception class, thrown when Tag parsing fails.
 */
class TagError : public std::runtime_error {
public:
	TagError(const std::string &msg) : std::runtime_error(msg) {}
	TagError(boost::format fmt) : std::runtime_error(fmt.str()) {}
};

/**
 * Tag class represents one Tag in the protocol. A tag has its identifier
 * (either 8-bit opcode or string value), and a data field. Refer to
 * eDonkey2000 protocol tag system overview for detailed information on the
 * Tag parsing system.
 */
class Tag {
public:
	/**
	 * Construct and load from input stream
	 *
	 * @param i        Input stream to read the data from
	 * \throws TagError if parsing fails
	 */
	Tag(std::istream &i);

	/**
	 * @name Construct using opcode (or name) and value. Only string and
	 *       integer data types are allowed for construction.
	 */
	//@{
	Tag(uint8_t opcode, const std::string &value)
	: m_opcode(opcode), m_value(value), m_valueType(TT_STRING) {}
	Tag(uint8_t opcode, uint32_t value)
	: m_opcode(opcode), m_value(value), m_valueType(TT_UINT32) {}
	Tag(const std::string &name, const std::string &value)
	: m_opcode(0), m_name(name), m_value(value), m_valueType(TT_STRING) {}
	Tag(const std::string &name, uint32_t value)
	: m_opcode(0), m_name(name), m_value(value), m_valueType(TT_UINT32) {}
	//@}

	/**
	 * @name Accessors for preconstructed or preloaded Tag data.
	 */
	//@{
	uint32_t getInt()   const try {
		return boost::any_cast<uint32_t>(m_value);
	} catch (boost::bad_any_cast&) {
		throw TagError(
			boost::format(
				"Invalid Tag value retrieval (int). \n"
				"Tag is %s"
			) % dump()
		);
	}
	float    getFloat() const try {
		return boost::any_cast<float>(m_value);
	} catch (boost::bad_any_cast&) {
		throw TagError(
			boost::format(
				"Invalid Tag value retrieval (float). \n"
				"Tag is %s"
			) % dump()
		);
	}
	std::string getStr() const try {
		return boost::any_cast<std::string>(m_value);
	} catch (boost::bad_any_cast&) {
		throw TagError(
			boost::format(
				"Invalid Tag value retrieval (string). \n"
				"Tag is %s"
			) % dump()
		);
	}
	uint8_t     getOpcode()    const { return m_opcode;    }
	std::string getName()      const { return m_name;      }
	uint8_t     getValueType() const { return m_valueType; }
	//@}

	/**
	 * Generate a debugging string from this tag's contents.
	 *
	 * @param data    If true, also include tag data/value field
	 * @return        Human-readable (mostly) hexdump of tag's contents
	 */
	std::string dump(bool data = true) const;
private:
	Tag();                       //!< Default constructor protected

	uint8_t     m_opcode;        //!< Opcode
	std::string m_name;          //!< Name (in case of string-tag)
	boost::any  m_value;         //!< Value
	uint8_t     m_valueType;     //!< Type of value, one of ValueTypes

	//! Different tag value types used in the protocol
	enum ValueTypes {
		TT_HASH    = 0x01,    //!< unsupported, 16 bytes
		TT_STRING  = 0x02,    //!< [u16]len[len]data
		TT_UINT32  = 0x03,    //!< 4 bytes
		TT_FLOAT   = 0x04,    //!< 4 bytes
		TT_BOOL    = 0x05,    //!< unsupported, 1 byte
		TT_BOOLARR = 0x06,    //!< unsupported, [u16]len[len]data
		TT_BLOB    = 0x07,    //!< unsupported, [u16]len[len]data
		TT_UINT16  = 0x08,    //!< 2 bytes
		TT_UINT8   = 0x09,    //!< 1 byte
		TT_BSOB    = 0x0a     //!< unsupported, [u16]len[len]data
	};

	//! Writes ed2k-compatible tag structure to specified output stream.
	friend std::ostream& operator<<(std::ostream &o, const Tag &t);

	/**
	 * Small helper function to localize the warning messages on unhandled
	 * tags found while parsing.
	 *
	 * @param loc  Location where the tag was found, e.g. "server.met"
	 * @param t    The unhandled tag itself.
	 */
	friend void warnUnHandled(const std::string &loc, const Tag &t);
};

} // end namespace Donkey

/**
 * \page ed2k-tag   eDonkey2000 protocol tag system overview
 * \section Abstract
 * This document describes the Tag system used in eDonkey2000 protocol.
 * \sa \ref ed2kproto
 *
 * \section toc Table of Contents
 * \par
 * \ref overview  <br>
 * \ref tagheader <br>
 * \ref desc      <br>
 *
 * \section overview 1. Overview
 *
 * Tags are used in ed2k network to add any number of additional data fields
 * to existing data structures while keeping the protocol backwards compatible.
 * The basic concept is that a tag can be identified by its data type (and/or
 * length), and can be simply ignored if the specific tag is not supported
 * for whatever reason.
 *
 * Client's can detect and handle various tags by their opcode, which is
 * 8-byte value. Additionally, there have been some protocol extensions which
 * also allow 'named' tags, which - logically - have a name instead of simple
 * opcode.
 *
 * \section tagheader 2. Header specification
 *
 * Standard ed2k tag:
 * \code
 * +--------+--------+--------+--------+--------
 * |  type  |  length = 0x01  | opcode |   type-specific amount of data
 * +--------+--------+--------+--------+--------
 * \endcode
 * Extended protocol, string tag:
 * \code
 * +-------+-------+-------+~~~~~~~~~~~~~~~+------
 * |  type |     length    | <length>name  | type-specific amount of data
 * +-------+-------+-------+~~~~~~~~~~~~~~~+------
 * \endcode
 * Lugdunum extended protocol, special types
 * \code
 * +-------+--------+-------
 * |  type | opcode | (type & 0x7f) - 0x10 bytes of string data
 * +-------+--------+-------
 * \endcode
 *
 * \section desc 3. Description
 *
 * As can be seen, lugdunum's extended protocol allows incorporating the string
 * field length into the type bits, lowering the header overhead. This format
 * can be detected by looking at the highest-order bit of type field. If it is
 * set, the type is encoded using this extension. To retrieve the actual data
 * type, apply &0x7f to the type. If this resolves into an integer of size
 * 0x10 or more, the string data length can be retrieved by substracting 0x10
 * from the type. Otherwise, the tag should be treated as normal ed2k tag type.
 *
 * Type field in the tag may be one of the following:
 * \dontinclude tag.h
 * \skip TT_HASH
 * \until TT_BSOB
 */

#endif
