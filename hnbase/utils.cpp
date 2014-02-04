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
 * \file utils.cpp Implementation of various useful utility functions
 */

#include <hnbase/pch.h>
#include <hnbase/utils.h>
#include <sstream>
#include <boost/regex.hpp>
#include <boost/algorithm/string/erase.hpp>
#include <algorithm>

#if defined(__linux) || defined(HURD)
	#include <execinfo.h>
#endif
#ifdef __GNUC__
	#include <cxxabi.h>
#endif

namespace Utils {

// Global random number generator; this is seeded from
// Hydranode::init()
boost::mt19937 getRandom;

// Used by the base64Encode and base64Decode functions:
const std::string Base64Table = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			  	"abcdefghijklmnopqrstuvwxyz"
			  	"0123456789+/";

#ifdef __GNUC__
struct StackFrame {
	StackFrame(const std::string &line);
	std::string m_module, m_func, m_offset, m_address, m_orig;
};

StackFrame::StackFrame(const std::string &line) {
	boost::regex reg("(\\S+)\\((\\S+)\\+(\\S+)\\) \\[(\\S+)\\]");
	boost::match_results<const char*> m;
	int ret = boost::regex_match(line.c_str(), m, reg);
	if (!ret) {
		m_orig = line;
		return;
	}
	m_module = m[1];
	m_func = m[2];
	char *tmp = __cxxabiv1::__cxa_demangle(m_func.c_str(), 0, 0, &ret);
	if (ret == 0 && tmp) {
		m_func = tmp;
	}
	m_offset = m[3];
	m_address = m[4];
}
std::ostream& operator<<(std::ostream &o, const StackFrame &f) {
	if (f.m_orig.size()) {
		return o << f.m_orig;
	} else {
		boost::format fmt("%s %|20t|%6s[%10s] %s");
		fmt % f.m_module % f.m_offset % f.m_address % f.m_func;
		return o << fmt.str();
	}
}
#endif

// prints stack trace
void stackTrace(uint32_t skipFrames) {
#if defined(__linux) || defined(HURD)
	void *bt_array[200];
	int num_entries = backtrace(bt_array, 200);
	if (num_entries < 0) {
		std::cerr << "* Could not generate backtrace.\n";
		return;
	}
	char **bt_strings = backtrace_symbols(bt_array, num_entries);
	if (!bt_strings) {
		std::cerr << "* Could not get symbol ";
		std::cerr << "names for backtrace.\n";
		return;
	}
	std::vector<StackFrame> stack;
	for (int i = 0; i < num_entries; ++i) {
		stack.push_back(StackFrame(bt_strings[i]));
	}
	for (size_t i = 0; i < stack.size(); ++i) {
		if (i < skipFrames) {
			continue;
		}
		std::cerr << " [" << i << "] " << stack[i] << std::endl;
	}
	free(bt_strings);
#endif
}

/**
* Converts passed character to decimal notation.
*
* @param c         Hexadecimal-compatible unsigned character
* @return          Decimal notation of given input
*/
inline unsigned char hex2dec(unsigned char c) {
	if (c >= '0' && c <= '9') c-='0';     // numbers
	if (c >= 'A' && c <= 'G') c-='A'-10;  // upper-case letters
	if (c >= 'a' && c <= 'g') c-='a'-10;  // lower-case letters
	if (c > 15) {
		throw std::runtime_error(
			"Hash::hex2dec: c > 15. Possibly input is not"
			" well-formed hexadecimal character."
		);
	}
	return c;
}

// Decode data
std::string decode(const char *data, uint32_t length) {
	std::ostringstream o;
	for (uint32_t i = 0; i < length; i++) {
		uint16_t c = (data[i] < 0) ? data[i]+256 : data[i];
		if (c < 16) {
			o << "0";
		}
		o << std::hex << c << std::dec;
	}
	CHECK_THROW(o.str().size() == length*2);
	return o.str();
}

// Encode data
std::string encode(const char *data, uint32_t length) {
	std::string s;
	for (uint32_t i = 0, j = 0; i < length; i++, j++) {
		s    += hex2dec(data[  i]) * 16;
		s[j] += hex2dec(data[++i]);
	}
	CHECK_THROW(s.size() == length/2);
	return s;
}

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

std::string encode64(const std::string &data) {
	std::string ret, tmp;
	tmp = data;

	// converting...
	for (uint64_t i = 0; i < tmp.size(); i += 3) {
		ret += Base64Table[  tmp[i]           >> 2                    ];
		ret += Base64Table[((tmp[i]     & 3 ) << 4) | (tmp[i + 1] >>4)];
		ret += Base64Table[((tmp[i + 1] & 15) << 2) | (tmp[i + 2] >>6)];
		ret += Base64Table[  tmp[i + 2] & 63                          ];
		if (i % 57 == 54) {
			ret += '\n';
		}
	}

	// finish padding...
	if (data.size() % 3 == 1) {
		ret[ret.size() - 1] = ret[ret.size() - 2] = '=';
	} else if (data.size() % 3 == 2) {
		ret[ret.size() - 1] = '=';
	}

	return ret;
}

std::string decode64(const std::string &data) {
	std::string ret, tmp;
	tmp = data;

	std::map<char, uint32_t> tab;
	for (uint32_t j = 0; j < Base64Table.size(); j++) {
		tab[Base64Table[j]] = j;
	}

	boost::algorithm::erase_all(tmp, std::string(1, '\r'));
	boost::algorithm::erase_all(tmp, std::string(1, '\n'));

	for (uint64_t i = 0; i < tmp.size(); i += 4) {
		ret +=   (tab[tmp[i]] << 2)           | (tab[tmp[i + 1]] >> 4);
		ret += (((tab[tmp[i + 1]] & 15) << 4) | (tab[tmp[i + 2]] >> 2));
		ret += (((tab[tmp[i + 2]] & 3) << 6)  | (tab[tmp[i + 3]]));
	}

	// remove padding '=' signs...
	uint8_t remove = std::count(data.begin(), data.end(), '=');
	ret = ret.substr(0, ret.size() - remove);

	return ret;
}

std::string urlEncode(const std::string &input, bool encodeDelims) {
	std::ostringstream tmp;
	for (uint32_t i = 0; i < input.size(); ++i) {
		if (
			(input[i] >= 'a' && input[i] <= 'z') ||
			(input[i] >= 'A' && input[i] <= 'Z') ||
			(input[i] >= '0' && input[i] <= '9')
		) {
			tmp << input[i];
		} else if (
			!encodeDelims && 
			(input[i] == '$' || input[i] == '-' || input[i] == '_' ||
			input[i] == '.' || input[i] == '+' || input[i] == '!' ||
			input[i] == '*' || input[i] == '\''|| input[i] == '(' ||
			input[i] == ')')
		) {
			tmp << input[i];
		} else {
			tmp << '%';
			int c = input[i];
			if (c < 0) {
				c += 256;
			}
			if (c < 16) {
				tmp << '0';
			}
			tmp << std::hex << c << std::dec;
		}
	}
	return tmp.str();
}

std::string secondsToString(uint64_t sec, uint8_t trunc) {
	std::string speedStr;
	uint32_t min = 0, hour = 0, day = 0, mon = 0, year = 0;
	if (trunc && sec >= (60*60*24*30*12)) {
		year = sec / (60*60*24*30*12);
		sec -= (year * 60*60*24*30*12);
		speedStr += (boost::format("%dy ") % year).str();
		--trunc;
	}
	if (trunc && sec >= (60*60*24*30)) {
		mon  = sec / (60*60*24*30);
		sec -= (mon * 60*60*24*30);
		speedStr += (boost::format("%dmo ") % mon).str();
		--trunc;
	}
	if (trunc && sec >= (60*60*24)) {
		day  = sec / (60*60*24);
		sec -= (day * 60*60*24);
		speedStr += (boost::format("%dd ") % day).str();
		--trunc;
	}
	if (trunc && sec >= 3600) {
		hour = sec / 3600;
		sec -= hour * 3600;
		speedStr += (boost::format("%dh ") % hour).str();
		--trunc;
	}
	if (trunc && sec >= 60) {
		min = sec / 60;
		sec -= min * 60;
		speedStr += (boost::format("%dm ") % min).str();
		--trunc;
	}
	if (trunc && sec) {
		speedStr += (boost::format("%ds ") % sec).str();
	}
	return speedStr;
}

} // end Utils namespace
