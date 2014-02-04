/*
 *  Copyright (C) 2006 Alo Sarv <madcat_@users.sourceforge.net>
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

#include <hnbase/utils.h>
#include <boost/test/minimal.hpp>


static const char TestData[16] = {
	0x09, 0x40, 0x46, 0x8a, 0x42, 0xc0, 0x8f, 0x01,
	0xc4, 0x3b, 0x07, 0xc7, 0xa6, 0x2c, 0x2d, 0xae
};

typedef Utils::EndianStream<std::istringstream, BIG_ENDIAN> BEIStream;
typedef Utils::EndianStream<std::ostringstream, BIG_ENDIAN> BEOStream;
typedef Utils::EndianStream<std::istringstream, LITTLE_ENDIAN> LEIStream;
typedef Utils::EndianStream<std::ostringstream, LITTLE_ENDIAN> LEOStream;

int test_main(int argc, char *argv[]) {
	// stream with no endianess setting
	std::istringstream i(std::string(TestData, 16));
	BOOST_CHECK((int)Utils::getVal<uint8_t>(i) == 9);
	BOOST_CHECK((int)Utils::getVal<uint8_t>(i) == 64);
	BOOST_CHECK(Utils::getVal<uint16_t>(i) == 35398);
	BOOST_CHECK(Utils::getVal<uint32_t>(i) == 26198082);
	BOOST_CHECK(Utils::getVal<uint64_t>(i) == 12550736831366773700ull);
	std::ostringstream o;
	Utils::putVal<uint8_t>(o, 9);
	Utils::putVal<uint8_t>(o, 64);
	Utils::putVal<uint16_t>(o, 35398);
	Utils::putVal<uint32_t>(o, 26198082);
	Utils::putVal<uint64_t>(o, 12550736831366773700ull);
	BOOST_CHECK(o.str() == std::string(TestData, 16));

	// big endian format stream
	BEIStream bi(std::string(TestData, 16));
	BOOST_CHECK((int)Utils::getVal<uint8_t>(bi) == 9);
	BOOST_CHECK((int)Utils::getVal<uint8_t>(bi) == 64);
	BOOST_CHECK(Utils::getVal<uint16_t>(bi) == 18058);
	BOOST_CHECK(Utils::getVal<uint32_t>(bi) == 1119915777);
	BOOST_CHECK(Utils::getVal<uint64_t>(bi) == 14139904009127603630ull);
	BEOStream bo;
	Utils::putVal<uint8_t>(bo, 9);
	Utils::putVal<uint8_t>(bo, 64);
	Utils::putVal<uint16_t>(bo, 18058);
	Utils::putVal<uint32_t>(bo, 1119915777);
	Utils::putVal<uint64_t>(bo, 14139904009127603630ull);
	BOOST_CHECK(bo.str() == std::string(TestData, 16));

	// little endian format stream
	LEIStream li(std::string(TestData, 16));
	BOOST_CHECK((int)Utils::getVal<uint8_t>(li) == 9);
	BOOST_CHECK((int)Utils::getVal<uint8_t>(li) == 64);
	BOOST_CHECK(Utils::getVal<uint16_t>(li) == 35398);
	BOOST_CHECK(Utils::getVal<uint32_t>(li) == 26198082);
	BOOST_CHECK(Utils::getVal<uint64_t>(li) == 12550736831366773700ull);
	LEOStream lo;
	Utils::putVal<uint8_t>(lo, 9);
	Utils::putVal<uint8_t>(lo, 64);
	Utils::putVal<uint16_t>(lo, 35398);
	Utils::putVal<uint32_t>(lo, 26198082);
	Utils::putVal<uint64_t>(lo, 12550736831366773700ull);
	BOOST_CHECK(lo.str() == std::string(TestData, 16));

	return boost::exit_success;
 }
