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

#ifndef __UTILS_H__
#define __UTILS_H__

#include <hncgcomm/osdep.h>
#include <hncgcomm/endian.h>
#include <boost/static_assert.hpp>
#include <boost/scoped_array.hpp>
#include <boost/shared_array.hpp>
#include <stdexcept>
#include <iostream>
#include <sstream>

namespace Utils {
	/**
	 * Exception class, thrown when read methods detect attempt to read
	 * past end of stream. We use this exception instead of any of
	 * pre-defined standard exceptions to make explicit differentiating
	 * between stream I/O errors and other generic errors thrown by STL.
	 */
	class ReadError : public std::runtime_error {
	public:
		ReadError(const std::string &msg) : std::runtime_error(msg) {}
	};

	//! Primary template is not implemented, and causes compile-time
	//! error if instanciated.
	template<class T> inline T getVal(std::istream &i) {
		BOOST_STATIC_ASSERT(sizeof(T::__type_not_supported__));
	}

	//! @name Specializations for reading various datas
	//@{
	template<>
	inline uint8_t getVal(std::istream &i) {
		uint8_t tmp;
		i.read(reinterpret_cast<char*>(&tmp), 1);
		if (!i.good()) {
			throw ReadError("reading from stream");
		}
		return tmp;
	}
	template<>
	inline uint16_t getVal(std::istream &i) {
		uint16_t tmp;
		i.read(reinterpret_cast<char*>(&tmp), 2);
		tmp = SWAP16_ON_BE(tmp);
		if (!i.good()) {
			throw ReadError("reading from stream");
		}
		return tmp;
	}
	template<>
	inline uint32_t getVal(std::istream &i) {
		uint32_t tmp;
		i.read(reinterpret_cast<char*>(&tmp), 4);
		tmp = SWAP32_ON_BE(tmp);
		if (!i.good()) {
			throw ReadError("reading from stream");
		}
		return tmp;
	}
	template<>
	inline uint64_t getVal(std::istream &i) {
		uint64_t tmp;
		i.read(reinterpret_cast<char*>(&tmp), 8);
		tmp = SWAP64_ON_BE(tmp);
		if (!i.good()) {
			throw ReadError("reading from stream");
		}
		return tmp;
	}
	template<>
	inline float getVal(std::istream &i) {
		float tmp;
		i.read(reinterpret_cast<char*>(&tmp), 4);
		tmp = (float)SWAP32_ON_BE(*(uint32_t*)(&tmp));
		if (!i.good()) {
			throw ReadError("reading from stream");
		}
		return tmp;
	}
	template<>
	inline std::string getVal(std::istream &i) {
		uint16_t len = getVal<uint16_t>(i);
		boost::scoped_array<char> buf(new char[len]);
		i.read(buf.get(), len);
		if (!i.good()) {
			throw ReadError("reading from stream");
		}
		return std::string(buf.get(), len);
	}
	template<class T>
	inline T getVal(std::istream &i, uint32_t len) {
		T tmp;
		i.read(&tmp, len);
		if (!i.good()) {
			throw ReadError("reading from stream");
		}
		return tmp;
	}
	template<>
	inline std::string getVal(std::istream &i, uint32_t len) {
		boost::scoped_array<char> tmp(new char[len]);
		i.read(tmp.get(), len);
		if (!i.good()) {
			throw ReadError("reading from stream");
		}
		return std::string(tmp.get(), len);
	}
	//@}

	//! Write. Primary template not implemented and causes compile-time
	//! failure if instanciated.
	template<class T> inline void putVal(std::ostream &o, T val) {
		BOOST_STATIC_ASSERT(sizeof(T::__undefined_type__));
	}

	//! @name Writing function template specializations.
	//@{
	template<>
	inline void putVal(std::ostream &o, uint8_t val) {
		o.put(val);
	}
	template<>
	inline void putVal(std::ostream &o, uint16_t val) {
		val = SWAP16_ON_BE(val);
		o.write(reinterpret_cast<char*>(&val), 2);
	}
	template<>
	inline void putVal(std::ostream &o, uint32_t val) {
		val = SWAP32_ON_BE(val);
		o.write(reinterpret_cast<char*>(&val), 4);
	}
	template<>
	inline void putVal(std::ostream &o, uint64_t val) {
		val = SWAP64_ON_BE(val);
		o.write(reinterpret_cast<char*>(&val), 8);
	}
	template<>
	inline void putVal(std::ostream &o, float val) {
		val = (float)SWAP32_ON_BE(*(uint32_t*)&val);
		o.write(reinterpret_cast<char*>(&val), 4);
	}
	template<>
	inline void putVal(std::ostream &o, const std::string &str) {
		putVal<uint16_t>(o, str.size());
		o.write(str.data(), str.size());
	}
	template<>
	inline void putVal(std::ostream &o, std::string str) {
		putVal<uint16_t>(o, str.size());
		o.write(str.data(), str.size());
	}
	inline void putVal(
		std::ostream &o, const std::string &str, uint32_t len
	) {
		o.write(str.data(), len);
	}
	inline void putVal(
		std::ostream &o, const char *const str, uint32_t len
	) {
		assert(str);
		o.write(str, len);
	}
	inline void putVal(
		std::ostream &o, const uint8_t *const str, uint32_t len
	) {
		assert(str);
		o.write(reinterpret_cast<const char*>(str), len);
	}
	inline void putVal(
		std::ostream &o, const boost::shared_array<char> &str,
		uint32_t len
	) {
		assert(str);
		o.write(str.get(), len);
	}
	//@}

	/**
	 * Dumps the hexadecimal representation of a value to stream.
	 *
	 * @param o      Stream to write to
	 * @param val    Value to write
	 * @return       The original stream
	 */
	template<class T> std::ostream& hexDump(std::ostream &o, T val) {
		o << std::hex << "0x";
		if (static_cast<int>(val)< 16) {
			o << "0";
		}
		return o << static_cast<int>(val) << std::dec;
	}

	/**
	 * Creates hexadecimal dump of value into a string.
	 *
	 * @param val    Value to convert to hex
	 * @return       String containing the hex representation of the value
	 */
	template<typename T> std::string hexDump(T val) {
		std::ostringstream o;
		o << std::hex << "0x";
		if (static_cast<int>(val) < 16) {
			o << "0";
		}
		o << static_cast<int>(val) << std::dec;
		return o.str();
	}
	/**
	 * Specalization - for dumping pointer types. Note: using %p argument
	 * at boost::format generally works better and is more readable for this
	 * purpose...
	 *
	 * @param val    Value to dump
	 * @return       Hexadeciman representation of value
	 */
	template<typename T> std::string hexDump(T *val) {
		std::ostringstream o;
		o << std::hex << "0x" << val << std::dec;
		return o.str();
	}

	/**
	 * Print hexadecimal dump of specified data into specified stream.
	 *
	 * @param o         Stream to write to
	 * @param data      Data to be written
	 */
	void hexDump(std::ostream &o, const std::string &data);

	/**
	 * Convert passed string into formatted hexadecimal string.
	 *
	 * @param data      Data to convert
	 * @return          Formatted hex-dump of the data.
	 */
	std::string hexDump(const std::string &data);

}


#endif
