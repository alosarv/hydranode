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

/** @file test-utils.cpp Test app for utility functions in Util:: namespace */
#ifndef DOXYGEN_SHOULD_SKIP_THIS

#include <hnbase/utils.h>
#include <hnbase/log.h>
#include <boost/test/unit_test.hpp>

using boost::unit_test_framework::test_suite;
using namespace boost::unit_test_framework;

// Test data
static const char TestData1[16] = {
	0x09, 0x40, 0x46, 0x8a, 0x42, 0xc0, 0x8f, 0x01,
	0xc4, 0x3b, 0x07, 0xc7, 0xa6, 0x2c, 0x2d, 0xae
};
static const unsigned char TestData2[16] = {
	0x09, 0x40, 0x46, 0x8a, 0x42, 0xc0, 0x8f, 0x01,
	0xc4, 0x3b, 0x07, 0xc7, 0xa6, 0x2c, 0x2d, 0xae
};
static const std::string TestDecoded("0940468a42c08f01c43b07c7a62c2dae");

void test_utils() {
	BOOST_CHECK_NO_THROW(Utils::decode(TestData1, 16));
	BOOST_CHECK_NO_THROW(Utils::encode(TestDecoded.c_str(), 32));
	BOOST_CHECK(Utils::decode(TestData1, 16) == TestDecoded);
	BOOST_CHECK(Utils::decode(Utils::encode(TestDecoded.c_str(), 32).c_str(), 16) == TestDecoded);
	BOOST_CHECK(!memcmp(Utils::encode(TestDecoded.c_str(), 32).c_str(), TestData2, 16));
}

void test_copy() {
	char *src = "Hello world";
	char *dest = 0;
	Utils::copyString(src, dest);
	BOOST_CHECK(!strcmp(src, dest));
	Utils::copyString(src, dest);
	BOOST_CHECK(!strcmp(src, dest));
}
void test_hexdump() {
//	std::string data(10024, '\0');
//	std::cerr << Utils::hexDump(data);
//	Log::instance().addTraceMask(0, "test-utils");
//	logTrace(0, boost::format("Dump: %s") % Utils::hexDump(data));
}

// Test-suite entry point
test_suite* init_unit_test_suite(int /*argc*/, char **/*argv*/) {
	test_suite *testutils = BOOST_TEST_SUITE("Utils Test");
	testutils->add(BOOST_TEST_CASE(&test_utils));
	testutils->add(BOOST_TEST_CASE(&test_copy));
	testutils->add(BOOST_TEST_CASE(&test_hexdump));
	return testutils;
}

#endif
