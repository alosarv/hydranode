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
 * \file utils.h Various useful utility functions.
 */

#ifndef __UTILS_H__
#define __UTILS_H__

#include <hnbase/osdep.h>
#include <hnbase/endian.h>
#include <hnbase/gettickcount.h>
#include <hnbase/lambda_placeholders.h>
#include <boost/static_assert.hpp>
#include <boost/scoped_array.hpp>
#include <boost/shared_array.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/type_traits.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <sys/stat.h>
#include <stdexcept>
#include <iostream>
#include <sstream>

/**
 * Various utility functions
 */
namespace Utils {
	MSVC_ONLY(extern "C" {)
	/**
	 * Global randomness generator; prefer this one instead of
	 * local variants to achieve best randomness over the
	 * application.
	 */
	extern HNBASE_EXPORT boost::mt19937 getRandom;
	MSVC_ONLY(})

	/**
	 * A type to carry stream endianess
	 *
	 * @param Endianess	Utils::LITTLE_ENDIAN or Utils::BIG_ENDIAN
	 */
	template<bool Endianess>
	class EndianType { };

	/**
	 * An object who's carrying endianess information.
	 *
	 * @param T		Stream type
	 * @param Endianess	Utils::LITTLE_ENDIAN or Utils::BIG_ENDIAN
	 */
	template<typename T, bool Endianess>
	class EndianStream : public T, public EndianType<Endianess> {
	public:
		EndianStream() {}
		template<typename Arg1>
		EndianStream(Arg1 a1) : T(a1) {}
		template<typename Arg1, typename Arg2>
		EndianStream(Arg1 a1, Arg2 a2) : T(a1, a2) {}
		template<typename Arg1, typename Arg2, typename Arg3>
		EndianStream(Arg1 a1, Arg2 a2, Arg3 a3) : T(a1, a2, a3) {}
	};

	/**
	 * Endianess test. GetEndianInfo<T>::value returns the endianess (if no
	 * endianess is available, Utils::LITTLE_ENDIAN is assumed)
	 */
	template<typename T>
	struct GetEndianInfo {
		enum { value =
			::boost::is_base_and_derived<
				EndianType<BIG_ENDIAN>, T
			>::value
		};
	};

	/**
	 * Swaps endianess if swap is true for various type of data T via the
	 * static function swap.
	 */
	template<typename T, bool Swap>
	struct SwapData;

	//! @name Specializations for swapping various datas
	//@{
	// If host endianess is equal to stream's one, then do not swap
	template<typename T>
	struct SwapData<T, false> {
		static T swap(T t) { return t; };
	};

	// uint8_t never get swapped
	template<>
	struct SwapData<uint8_t, true> {
		static uint8_t swap(uint8_t t) { return t; };
	};
	// uint16_t
	template<>
	struct SwapData<uint16_t, true> {
		static uint16_t swap(uint16_t t) { return SWAP16_ALWAYS(t); }
	};
	// uint32_t
	template<>
	struct SwapData<uint32_t, true> {
		static uint32_t swap(uint32_t t) { return SWAP32_ALWAYS(t); }
	};
	// uint64_t
	template<>
	struct SwapData<uint64_t, true> {
		static uint64_t swap(uint64_t t) { return SWAP64_ALWAYS(t); }
	};
	// float
	template<>
	struct SwapData<float, true> {
		static float swap(float t) {
			return (float)SWAP32_ALWAYS((uint32_t)t);
		}
	};
	// unimplemented
	template<typename T>
	struct SwapData<T, true> {
		static T swap(T t) {
			BOOST_STATIC_ASSERT(sizeof(T::__type_not_supported__));
		}
	};

	//@}

	/**
	 * This function takes care of swapping data if streams need it.
	 */
	template<typename T, typename Stream>
	inline T swapHostToStream(T t) {
		// since values are boolean, !! avoids this warning on gcc4.0.0:
		// comparison between `enum Utils::GetEndianInfo<...>
		// ::<anonymous>` and `enum Utils::<anonymous>`

		return SwapData<
			T, !!GetEndianInfo<Stream>::value != !!HOST_ENDIAN
		>::swap(t);
	}

	/**
	 * Convert passed data into hexadecimal notation.
	 *
	 * @param data       Data to be decoded.
	 * @param length     Length of data.
	 * @return           Hexadecimal notation of the data.
	 */
	std::string HNBASE_EXPORT decode(const char *data, uint32_t length);

	/**
	 * Convert hexadecimal character string into binary format.
	 *
	 * @param data      Data to be encoded.
	 * @param length    Length of data.
	 * @return          Binary representation of the passed data.
	 *
	 * \note Throws std::runtime_error from hex2dec if passed data is not
	 *       well-formed hexadecimal stream.
	 */
	std::string HNBASE_EXPORT encode(const char *data, uint32_t length);

	inline std::string decode(const std::string &data) {
		return decode(data.data(), data.size());
	}
	inline std::string decode(const std::string &data, uint32_t length) {
		return decode(data.data(), length);
	}
	inline std::string decode(boost::shared_array<char> data, uint32_t len){
		return decode(data.get(), len);
	}
	inline std::string encode(const std::string &data) {
		return encode(data.data(), data.size());
	}
	inline std::string encode(const std::string &data, uint32_t length) {
		return encode(data.data(), length);
	}
	inline std::string encode(boost::shared_array<char> data, uint32_t len){
		return encode(data.get(), len);
	}

	/**
	 * Encodes the passed string as per RFC 1738 specification
	 *
	 * @param input        Input data to be encoded
	 * @param encodeDelims Whether to also encode reserved delims 
	 *                     (can break full urls, but useful for 
	 *                      components of the url which can 
	 *                      potentially contain reserved characters, 
	 *                      such as hashes)
	 * @return             URL-encoded version of input
	 */
	std::string HNBASE_EXPORT urlEncode(
		const std::string &input, bool encodeDelims = false
	);

	/**
	 * Copy string from source to destination, overwriting destination
	 * memory. This function assumes source string is null-terminated,
	 * and copies until the terminating zero (including it). The destination
	 * pointer is passed by reference and modified as neccesery during
	 * memory reallocations to point to newly allocated memory region.
	 *
	 * @param src     Source, null-terminated string
	 * @param dest    Destination
	 */
	inline void copyString(const char *src, char *&dest) {
		uint32_t len = strlen(src)+1;
		if (dest == 0) {
			dest = reinterpret_cast<char*>(malloc(len));
		} else {
			dest = reinterpret_cast<char*>(realloc(dest, len));
		}
		memcpy(dest, src, len);
	}

	/**
	 * Retrieve the size of a file.
	 *
	 * @param path     Full path to the file being interested in.
	 * @return         Size of the file.
	 */
	inline uint64_t getFileSize(const boost::filesystem::path &loc) {
#ifdef _MSC_VER
		struct ::__stat64 results;
		::_stat64(loc.native_file_string().c_str(), &results);
#elif defined(WIN32) // other compilers, e.g. mingw
		struct ::_stati64 results;
		::_stati64(loc.native_file_string().c_str(), &results);
#else
		struct stat results;
		stat(loc.native_file_string().c_str(), &results);
#endif
		return results.st_size;
	}

	/**
	 * Retrieve the last modification date of a file.
	 *
	 * @param path     Full path to the file being interested in.
	 * @return         Last modification time as reported by the OS.
	 */
	inline uint32_t getModDate(const boost::filesystem::path &loc) {
		struct stat results;
		stat(loc.native_file_string().c_str(), &results);
		return results.st_mtime;
	}

	/**
	 * Prints stack trace to std::cerr; useful for debugging mainly
	 *
	 * @param skipFrames       Number of frames to skip printing from top
	 */
	void stackTrace(uint32_t skipFrames = 1);

	/**
	 * Exception class, thrown when read methods detect attempt to read
	 * past end of stream. We use this exception instead of any of
	 * pre-defined standard exceptions to make explicit differenciating
	 * between stream I/O errors and other generic errors thrown by STL.
	 */
	class ReadError : public std::runtime_error {
	public:
		ReadError(const std::string &msg) : std::runtime_error(msg) {}
	};

	/**
	 * Generic getVal functor
	 */
	template<typename T = std::string>
	struct getVal {
		//! Conversion to T
		operator T() const { return m_value; }

		//! Explicit access to read value
		T value() const { return m_value; }

		//! Generic constructor
		template<typename Stream>
		getVal(Stream &s) {
			s.read(reinterpret_cast<char *>(&m_value), sizeof(T));

			if(!s.good()) {
				throw ReadError("unexpected end of stream");
			}
			m_value = swapHostToStream<T, Stream>(m_value);
		}
	private:
		T m_value;
	};

	/**
	 * std::string specialization of getVal
	 */
	template<>
	struct getVal<std::string> {
		//! Conversion to T
		operator std::string() const { return m_value; }

		//! Explicit access to read value
		std::string value() const { return m_value; }

		//! @name std::string getVal constructors
		//@{
		template<typename Stream>
		getVal(Stream &i) {
			uint16_t len = getVal<uint16_t>(i);
			boost::scoped_array<char> buf(new char[len]);
			i.read(buf.get(), len);

			if (!i.good()) {
				throw ReadError("unexpected end of stream");
			}

			m_value = std::string(buf.get(), len);
		}
		getVal(std::istream &i, uint32_t len) {
			boost::scoped_array<char> tmp(new char[len]);
			i.read(tmp.get(), len);

			if (!i.good()) {
                        	throw ReadError("unexpected end of stream");
			}

			m_value = std::string(tmp.get(), len);
		}
		//@}
	private:
		std::string m_value;
	};

	/**
	 * Generic putVal functor
	 */
	template<typename T = std::string>
	struct putVal {
		//! Generic constructor
		template<typename Stream>
		putVal(Stream &s, T t) {
			T tmp = swapHostToStream<T, Stream>(t);

			s.write(reinterpret_cast<char *>(&tmp), sizeof(T));
		}
	};

	/**
	 * std::string specialization of putVal
	 */
	template<>
	struct putVal<std::string> {
		//! @name std::string putVal constructors
		//@{
		template<typename Stream>
		putVal(Stream &o, const std::string &str) {
			putVal<uint16_t>(o, str.size());
			o.write(str.data(), str.size());
		}

		template<typename Stream>
		putVal(Stream &o, const std::string &str, uint32_t len) {
			o.write(str.data(), len);
		}

		template<typename Stream>
		putVal(Stream &o, const char *const str, uint32_t len) {
			CHECK_THROW(str);
			o.write(str, len);
		}

		template<typename Stream>
		putVal(Stream &o, const uint8_t *const str, uint32_t len) {
			CHECK_THROW(str);
			o.write(reinterpret_cast<const char*>(str), len);
		}

		template<typename Stream>
		putVal(
			Stream &o,
			const boost::shared_array<char> &str,
			uint32_t len
		) {
			CHECK_THROW(str);
			o.write(str.get(), len);
		}
		//@}
	};

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
	void HNBASE_EXPORT hexDump(std::ostream &o, const std::string &data);

	/**
	 * Convert passed string into formatted hexadecimal string.
	 *
	 * @param data      Data to convert
	 * @return          Formatted hex-dump of the data.
	 */
	std::string HNBASE_EXPORT hexDump(const std::string &data);

	/**
	 * Encodes a string using the Base64 algorithm.
	 *
	 * @param data       String that is going to be encoded
	 * @return           The Base64 encoded string
	 */
	std::string HNBASE_EXPORT encode64(const std::string &data);

	/**
	 * Decodes a Base64-encoded string.
	 *
	 * @param data       The Base64 encoded string
	 * @return           String that is going to be decoded
	 */
	std::string HNBASE_EXPORT decode64(const std::string &data);

	/**
	* Simple time measuring class, wrapped around getTick() method.
	*/
	class HNBASE_EXPORT StopWatch {
	public:
		//! Constructs and starts the timer
		StopWatch() : m_start(getTick()) {}

		/**
		 * Retrieves the elapsed time since the construction of the
		 * object.
		 *
		 * @return Time elapsed in milliseconds
		 */
		uint64_t elapsed() const { return getTick() - m_start; }

		/**
		 * Reset the stopper, starting at zero again.
		 */
		void reset() { m_start = getTick(); }

		//! Output operator to streams
		friend std::ostream& operator<<(
			std::ostream &o, const StopWatch &s
		) {
			return o << s.elapsed();
		}
	private:
		uint64_t m_start;      //!< Timer start time
	};

	/**
	 * Convert bytes to string representation
	 *
	 * @param bytes      Amount of bytes to check
	 * @return           Human-readable string, e.g. "4.56 GB"
	 */
	inline std::string bytesToString(uint64_t bytes) {
		boost::format fmt("%.2f %s");
		if (bytes >= 1024ll*1024ll*1024ll*1024ll) {
			fmt % (bytes/1024.0/1024.0/1024.0/1024.0) % "TB";
		} else if (bytes >= 1024*1024*1024) {
			fmt % (bytes/1024.0/1024.0/1024.0) % "GB";
		} else if (bytes >= 1024*1024) {
			fmt % (bytes/1024.0/1024.0) % "MB";
		} else if (bytes >= 1024) {
			fmt % (bytes/1024.0) % "KB";
		} else if (bytes == 1) {
			return "1 byte";
		} else {
			return (boost::format("%d bytes") % bytes).str();
		}
		return fmt.str();
	}

	/**
	 * Converts seconds to human-readable y/mo/d/h/m/s string.
	 *
	 * @param sec         Seconds to be converted
	 * @param trunc       Truncate precision to so many values from the left
	 * @returns           String like '12y 3mo 5d 2h 15m 36s'
	 */
	std::string HNBASE_EXPORT secondsToString(
		uint64_t sec, uint8_t trunc = 6
	);

	/**
	 * Simple generic function object for usage in standard containers,
	 * where pointer types are stored, but actual object comparisons are
	 * needed.
	 * Usage:
	 * \code
	 * std::set<MyType*, PtrLess<MyType> > myset;
	 * \endcode
	 */
	template<typename T>
	struct PtrLess {
		bool operator()(const T *const &x, const T *const &y) const {
			return *x < *y;
		}
		bool operator()(const T &x, const T &y) const {
			return *x < *y;
		}
	};

	/**
	 * Binary function object for usage with boost::signals as combiner, or
	 * any other standard containers.
	 *
	 * @param first    Start iterator
	 * @param last     One-past-end iterator
	 * @returns        False if dereferencing any of the iterators
	 *                 evaluates to false, true otherwise.
	 *
	 * \note Returns true when first == last as well.
	 */
	struct BoolCheck {
		typedef bool result_type;
		template<typename InputIterator>
		bool operator()(InputIterator first, InputIterator last) const {
			while (first != last && *first) {
				++first;
			}
			return first == last;
		}
	};

	/**
	 * Binary function object for usage with boost::signals as combiner or
	 * other standard containers.
	 *
	 * @param first    Start iterator
	 * @param last     One-past-end iterator
	 * @returns        The sum of the values aquired by de-referencing the
	 *                 iterators
	 */
	template<typename T>
	struct Sum {
		typedef T result_type;
		template<typename InputIterator>
		T operator()(InputIterator first, InputIterator last) const {
			T tmp = 0;
			for_each(first, last, tmp += __1);
			return tmp;
		}
	};

	/**
	 * Integer concept check, based on boost/concept_check.hpp, original
	 * authors Jeremy Siek and Andrew Lumsdaine. This version adds 64-bit
	 * integers, which are missing (as of Boost 1.33 release) in
	 * concept_check.hpp.
	 */
	template <typename T>
	struct IntegerConcept {
		void constraints() {
	#if !defined BOOST_NO_TEMPLATE_PARTIAL_SPECIALIZATION
			x.error_type_must_be_an_integer_type();
	#endif
		}
		T x;
	};
	#if !defined BOOST_NO_TEMPLATE_PARTIAL_SPECIALIZATION
	template<> struct IntegerConcept<int8_t  > { void constraints() {} };
	template<> struct IntegerConcept<int16_t > { void constraints() {} };
	template<> struct IntegerConcept<int32_t > { void constraints() {} };
	template<> struct IntegerConcept<int64_t > { void constraints() {} };
	template<> struct IntegerConcept<uint8_t > { void constraints() {} };
	template<> struct IntegerConcept<uint16_t> { void constraints() {} };
	template<> struct IntegerConcept<uint32_t> { void constraints() {} };
	template<> struct IntegerConcept<uint64_t> { void constraints() {} };
	#endif

} //! namespace Utils

#endif
