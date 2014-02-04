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
 * \file config.h Interface for Config class.
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <map>
#include <string>
#include <hnbase/osdep.h>
#include <hnbase/utils.h>
#include <boost/lexical_cast.hpp>
#include <boost/signal.hpp>

/**
 * Generic configuration storage class
 *
 * Config class is capable of storing key/value pairs of arbitary type in a
 * structured hierarchy. Values can be stored at either top-level, or at
 * sub-dirs. Accessing keys can be done either using full path of the key,
 * e.g. prepending "/" character before the key location, or using combination
 * of setPath() and read() calls.
 *
 * Internally the data is stored as strings, and converted to requested types
 * on request. Thus, it is possible to write apples and read back oranges, as
 * long as:
 * - both types can be converted to std::string using boost::lexical_cast
 * - the stored type can be converted from std::string to destination type using
 *   boost::lexical_cast.
 * .
 *
 * Config class is also capable of storing the stored values in physical file
 * between sessions, using save() and load() methods. The file format is not
 * 100% compatible with standard configuration files, but attempts to be as
 * close as possible. The actual format of the file stored on disk:
 *
 * \code
 * [Boo]
 * Boolean=1
 * [Floating/Yeah]
 * Pi=3.141
 * [Test]
 * HelloWorld=Good Morning
 * []
 * Integer Value=10
 * Doh=0
 * \endcode
 *
 * \note Using multi-word names for keys is allowed.
 */
class HNBASE_EXPORT Config {
public:
	/**
	 * Default constructor.
	 */
	Config();

	/**
	 * Constructs and loads from file.
	 *
	 * @param filename    File to read data from.
	 *
	 */
	Config(const std::string &filename);

	/**
	 * Virtual destructor.
	 */
	virtual ~Config();

	/**
	 * Loads data from file.
	 *
	 * @param filename    File to read data from.
	 */
	virtual void load(const std::string &filename);

	/**
	 * Saves values into same file as used during loading.
	 */
	virtual void save() const { save(m_configFile); }

	/**
	 * Saves values into specified file.
	 *
	 * @param filename   File to save values to.
	 */
	virtual void save(const std::string &filename) const;

	/**
	 * Converts a type into the name of the type
	 */
	template<typename T>
	std::string typeToStr() const;

	/**
	 * Reads stored value into variable, or default value if not found.
	 *
	 * @param key    String key of value searched for.
	 * @param val    Pointer to variable to store found data in.
	 * @param def    Default value, used if no value was found in map.
	 *
	 * The two template classes here are neccesery, because we can get
	 * input like float + double, and compilers don't seem to like that.
	 */
	template<typename C, typename D>
	void read(std::string key, C *val, const D &def) const {
		CHECK_THROW(val);
		CHECK_THROW(key.size());

		if (key.at(0) != '/') { // convert relative to absolute
			key = m_curPath + key;
		}

		CIter i = m_values.find(key);
		if (i != m_values.end()) try {
			*val = boost::lexical_cast<C>((*i).second);
		} catch (boost::bad_lexical_cast &e) {
			logWarning(
				boost::format(
					"Invalid configuration value for key "
					"'%s' (expected %s)"
				) % key % typeToStr<C>()
			);
			*val = def;
		} else {
			*val = def;
		}
	}

	/**
	 * Overloaded version of Read function returning the value read.
	 *
	 * @param key   Key to search for.
	 * @param def   Default value to use if key is not found.
	 *
	 * @return      Value of found key, or default value if not found.
	 */
	template<typename C>
	C read(const std::string &key, const C &def) const {
		C tmp;
		read(key, &tmp, def);
		return tmp;
	}

	/**
	 * Stores key/value pair.
	 *
	 * @param key    String key for later retrieval.
	 * @param val    Value to be stored.
	 */
	template<typename T>
	bool write(const std::string &key, const T &val) {
		CHECK_THROW(key.size());

		std::string rKey = key;
		std::string rValue = boost::lexical_cast<std::string>(val);

		// insert full path (if needed)
		if (rKey.at(0) != '/') {
			rKey.insert(0, m_curPath);
		}

		if (!valueChanging(rKey.substr(1), rValue)) {
			return false;
		}

		m_values[rKey] = rValue;

		valueChanged(rKey.substr(1), rValue);

		return true;
	}

	/**
	 * Erases a key from the configuration file.
	 *
	 * @param key        Key to be erased
	 */
	void erase(const std::string &key) {
		CHECK_THROW(key.size());
		if (key.at(0) != '/') {
			m_values.erase(m_curPath + key);
		} else {
			m_values.erase(key);
		}
	}

	/**
	 * Signal which is emitted PRIOR to any value changes in the settings;
	 *
	 * @param key    Full path to the key
	 * @param value  The new value, cast to std::string
	 * @returns      False to indicate the value should NOT be changed
	 */
	boost::signal<
		bool (const std::string&, const std::string&), Utils::BoolCheck
	> valueChanging;

	/**
	 * This signal is emitted whenever a value has been changed; while
	 * valueChanging is called PRIOR to actual value change, and allows
	 * intercepting, and dis-allowing the change, this signal is called
	 * when the value change has been completed.
	 *
	 * @param key   Full path to the key
	 * @param val   The new value of the key
	 */
	boost::signal<
		void (const std::string&, const std::string&)
	> valueChanged;

	/**
	 * Sets current directory.
	 *
	 * @param dir    Directory to change to.
	 */
	void setPath(const std::string &dir);

	/**
	 * Returns number of elements stored by this instance.
	 *
	 * @return    Number of values stored by this instance.
	 */
	size_t size() const { return m_values.size(); }

	/**
	 * Dump the entire stored contents to std::cerr (for debugging)
	 */
	void dump() const;

	//! Constant iterator for internal data storage
	typedef std::map<std::string, std::string>::const_iterator CIter;
	//! Access to begin iterator
	CIter begin() const { return m_values.begin(); }
	//! Access to end iterator
	CIter end() const { return m_values.end(); }
private:
	// copying is not allowed
	Config(const Config&);
	Config& operator=(const Config&);

	std::string m_configFile;           //!< File name used during loading
	std::string m_curPath;                       //!< Current directory
	std::map<std::string, std::string> m_values; //!< Actual data storage

	typedef std::map<std::string, std::string>::iterator Iter;
};

/**
 * \name Specializations of typeToStr template
 */
//!@{
template<>
inline std::string Config::typeToStr<uint8_t>() const {
	return "unsigned integer";
}
template<>
inline std::string Config::typeToStr<uint16_t>() const {
	return "unsigned integer";
}
template<>
inline std::string Config::typeToStr<uint32_t>() const {
	return "unsigned integer";
}
template<>
inline std::string Config::typeToStr<uint64_t>() const {
	return "unsigned integer";
}
template<>
inline std::string Config::typeToStr<int8_t>() const { return "integer"; }
template<>
inline std::string Config::typeToStr<int16_t>() const { return "integer"; }
template<>
inline std::string Config::typeToStr<int32_t>() const { return "integer"; }
template<>
inline std::string Config::typeToStr<int64_t>() const { return "integer"; }
#ifdef _MSC_VER
template<>
inline std::string Config::typeToStr<int>() const { return "integer"; }
#endif
template<>
inline std::string Config::typeToStr<std::string>() const { return "string"; }
template<>
inline std::string Config::typeToStr<char*>() const { return "string"; }
template<>
inline std::string Config::typeToStr<const char*>() const { return "string"; }
template<>
inline std::string Config::typeToStr<const char* const>() const {
	return "string";
}
template<>
inline std::string Config::typeToStr<bool>() const {
	return "boolean (0 or 1)";
}
template<>
inline std::string Config::typeToStr<float>() const { return "float"; }
template<>
inline std::string Config::typeToStr<double>() const { return "double"; }
//!@}

#endif
