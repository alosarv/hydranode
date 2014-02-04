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

/** \file test-range.cpp Regress-test for Range API */

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#include <hnbase/range.h>
#include <hnbase/rangelist.h>
#include <boost/test/unit_test.hpp>
#include <iostream>

void test_base() {
	Range16 r(0, 5);

	BOOST_CHECK(r.length() == 6);
	BOOST_CHECK(r.begin() == 0);
	BOOST_CHECK(r.end() == 5);

	BOOST_CHECK(r.contains(0));
	BOOST_CHECK(r.contains(3));
	BOOST_CHECK(r.contains(5));
	BOOST_CHECK(!r.contains(6));
	BOOST_CHECK(!r.contains(100));

	BOOST_CHECK_THROW(r.begin(6), std::exception);
	BOOST_CHECK_NO_THROW(r.begin(5));
	BOOST_CHECK(r.length() == 1);
	BOOST_CHECK_NO_THROW(r.end(10));
	BOOST_CHECK(r.begin() == 5);
	BOOST_CHECK(r.end() == 10);
	BOOST_CHECK(r.length() == 6);

	Range16 r1(0, 5);
	Range16 r2(0, 2);
	BOOST_CHECK(r1.contains(r2));
	BOOST_CHECK(r1.contains(0, 0));
	BOOST_CHECK(r1.contains(Range16(5, 5)));
	BOOST_CHECK(r1.contains(5, 6));
	BOOST_CHECK(!r1.contains(6, 6));

	BOOST_CHECK(r1.containsFull(r2));
	BOOST_CHECK(r1.containsFull(0, 4));
	BOOST_CHECK(r1.containsFull(0, 5));
	BOOST_CHECK(!r1.containsFull(0, 6));
	BOOST_CHECK(r1.containsFull(5, 5));
	BOOST_CHECK(r1.containsFull(Range16(1, 5)));
	BOOST_CHECK(!r1.containsFull(Range16(6, 6)));
	BOOST_CHECK(!r1.containsFull(5, 10));

	Range16 r3(20, 30);
	Range16 r4( 3, 10);
	BOOST_CHECK(!r3.contains(r4));
	BOOST_CHECK(!r3.borders(r4));

	// test comparison operators
	BOOST_CHECK(r1 != r2);
	BOOST_CHECK(r2 != r1);
	BOOST_CHECK(r3 != r4);
//	BOOST_CHECK(r3 > r4);
//	BOOST_CHECK(r4 < r3);
}

void test_merge() {
	RangeList16 rl;
	rl.push(0, 5);
	BOOST_CHECK(rl.size() == 1);
	// duplicates are allowed
	BOOST_CHECK(rl.push(0, 1));
	BOOST_CHECK(rl.push(5, 5));
	BOOST_CHECK(rl.push(0, 100));
	BOOST_CHECK(rl.size() == 4);
	rl.clear();
	rl.merge(0, 5);
	BOOST_CHECK(rl.size() == 1);
	rl.merge(5, 6);
	BOOST_CHECK(rl.size() == 1);
	rl.merge(7, 10);
	BOOST_CHECK(rl.size() == 1);
	rl.erase(5, 6);
	BOOST_CHECK(rl.size() == 2);
	rl.erase(3, 6);
	BOOST_CHECK(rl.size() == 2);
	rl.erase(0, 10);
	BOOST_CHECK(rl.size() == 0);
	rl.merge(0, 4);
	rl.merge(5, 10);
	rl.merge(4, 4);
	BOOST_CHECK(rl.size() == 1);
}

void test_more() {
	RangeList16 r;
	typedef Range16 RR;
	typedef RangeList16::CIter CIter;

	CIter i;

	/**************************** PART I: Adding *************************/
	BOOST_CHECK_NO_THROW(r.merge(RR(5, 10)));
	i = r.begin();
	BOOST_CHECK((*i).begin() == 5);
	BOOST_CHECK((*i).end() == 10);

	BOOST_CHECK_NO_THROW(r.merge(RR(20, 30)));
	i++;
	BOOST_CHECK((*i).begin() == 20);
	BOOST_CHECK((*i).end() == 30);
	BOOST_CHECK(r.size() == 2);

	// RC_CONTAINS_END
	BOOST_CHECK_NO_THROW(r.merge(RR(3, 6)));
	i = r.begin();
	BOOST_CHECK((*i).begin() == 3);
	BOOST_CHECK((*i).end() == 10);
	i++;
	BOOST_CHECK((*i).begin() == 20);
	BOOST_CHECK((*i).end() == 30);
	BOOST_CHECK(r.size() == 2);

	// RC_CONTAINS_FULL - list shouldn't be changed
	BOOST_CHECK_NO_THROW(r.merge(RR(4, 8)));
	BOOST_CHECK_NO_THROW(r.merge(RR(3, 4)));
	i = r.begin();
	BOOST_CHECK((*i).begin() == 3);
	BOOST_CHECK((*i).end() == 10);
	i++;
	BOOST_CHECK((*i).begin() == 20);
	BOOST_CHECK((*i).end() == 30);
	BOOST_CHECK(r.size() == 2);

	// RC_CONTAINS_BEGIN
	BOOST_CHECK_NO_THROW(r.merge(RR(8, 12)));
	i = r.begin();
	BOOST_CHECK((*i).begin() == 3);
	BOOST_CHECK((*i).end() == 12);
	i++;
	BOOST_CHECK((*i).begin() == 20);
	BOOST_CHECK((*i).end() == 30);
	BOOST_CHECK(r.size() == 2);

	// RC_BORDER_BEGIN
	BOOST_CHECK_NO_THROW(r.merge(RR(15, 19)));
	i = r.begin();
	BOOST_CHECK((*i).begin() == 3);
	BOOST_CHECK((*i).end() == 12);
	i++;
	BOOST_CHECK((*i).begin() == 15);
	BOOST_CHECK((*i).end() == 30);
	BOOST_CHECK(r.size() == 2);

	// RC_BORDER_END
	BOOST_CHECK_NO_THROW(r.merge(RR(31, 35)));
	i = r.begin();
	BOOST_CHECK((*i).begin() == 3);
	BOOST_CHECK((*i).end() == 12);
	i++;
	BOOST_CHECK((*i).begin() == 15);
	BOOST_CHECK((*i).end() == 35);
	BOOST_CHECK(r.size() == 2);

	// RC_BORDER_END
	BOOST_CHECK_NO_THROW(r.merge(RR(35, 37)));
	i = r.begin();
	BOOST_CHECK((*i).begin() == 3);
	BOOST_CHECK((*i).end() == 12);
	i++;
	BOOST_CHECK((*i).begin() == 15);
	BOOST_CHECK((*i).end() == 37);
	BOOST_CHECK(r.size() == 2);

	// Insert between the two existing ranges
	BOOST_CHECK_NO_THROW(r.merge(RR(13, 14)));
	i = r.begin();
	BOOST_CHECK((*i).begin() == 3);
	BOOST_CHECK((*i).end() == 37);
	BOOST_CHECK(r.size() == 1);

	/************************** PART II: Erasing *************************/
	// splitting existing range into two
	BOOST_CHECK_NO_THROW(r.erase(RR(10, 12)));

	i = r.begin();
	BOOST_CHECK((*i).begin() == 3);
	BOOST_CHECK((*i).end() == 9);
	i++;
	BOOST_CHECK((*i).begin() == 13);
	BOOST_CHECK((*i).end() == 37);
	BOOST_CHECK(r.size() == 2);

	// removing range that doesnt exist
	BOOST_CHECK_NO_THROW(r.erase(RR(10, 12)));

	i = r.begin();
	BOOST_CHECK((*i).begin() == 3);
	BOOST_CHECK((*i).end() == 9);
	i++;
	BOOST_CHECK((*i).begin() == 13);
	BOOST_CHECK((*i).end() == 37);
	BOOST_CHECK(r.size() == 2);

	// RC_CONTAINS_END
	BOOST_CHECK_NO_THROW(r.erase(RR(9, 12)));
	i = r.begin();
	BOOST_CHECK((*i).begin() == 3);
	BOOST_CHECK((*i).end() == 8);
	i++;
	BOOST_CHECK((*i).begin() == 13);
	BOOST_CHECK((*i).end() == 37);
	BOOST_CHECK(r.size() == 2);

	BOOST_CHECK_NO_THROW(r.erase(RR(30, 40)));
	i = r.begin();
	BOOST_CHECK((*i).begin() == 3);
	BOOST_CHECK((*i).end() == 8);
	i++;
	BOOST_CHECK((*i).begin() == 13);
	BOOST_CHECK((*i).end() == 29);
	BOOST_CHECK(r.size() == 2);

	// RC_CONTAINS_END
	BOOST_CHECK_NO_THROW(r.erase(RR(10, 15)));
	i = r.begin();
	BOOST_CHECK((*i).begin() == 3);
	BOOST_CHECK((*i).end() == 8);
	i++;
	BOOST_CHECK((*i).begin() == 16);
	BOOST_CHECK((*i).end() == 29);
	BOOST_CHECK(r.size() == 2);

	// RC_AROUND
	BOOST_CHECK_NO_THROW(r.erase(RR(1, 10)));
	i = r.begin();
	BOOST_CHECK((*i).begin() == 16);
	BOOST_CHECK((*i).end() == 29);
	BOOST_CHECK(r.size() == 1);

	// Split again
	BOOST_CHECK_NO_THROW(r.erase(RR(20, 21)));
	i = r.begin();
	BOOST_CHECK((*i).begin() == 16);
	BOOST_CHECK((*i).end() == 19);
	i++;
	BOOST_CHECK((*i).begin() == 22);
	BOOST_CHECK((*i).end() == 29);
	BOOST_CHECK(r.size() == 2);

	// Complex erasing - cuts into two existing ranges
	BOOST_CHECK_NO_THROW(r.erase(RR(18, 25)));
	i = r.begin();
	BOOST_CHECK((*i).begin() == 16);
	BOOST_CHECK((*i).end() == 17);
	i++;
	BOOST_CHECK((*i).begin() == 26);
	BOOST_CHECK((*i).end() == 29);
	BOOST_CHECK(r.size() == 2);

	BOOST_CHECK_NO_THROW(r.merge(RR(16, 20)));
	// Complex merging - overlap with two existing ranges
	BOOST_CHECK_NO_THROW(r.merge(RR(18, 28)));
	i = r.begin();
	BOOST_CHECK((*i).begin() == 16);
	BOOST_CHECK((*i).end() == 29);
	BOOST_CHECK(r.size() == 1);

	// Range of size 1
	BOOST_CHECK_NO_THROW(r.merge(RR(30, 30)));
	i = r.begin();
	BOOST_CHECK(r.size() == 1);
	BOOST_CHECK((*i).begin() == 16);
	BOOST_CHECK((*i).end() == 30);

	// Another range of size 1
	BOOST_CHECK_NO_THROW(r.merge(RR(15, 15)));
	i = r.begin();
	BOOST_CHECK(r.size() == 1);
	BOOST_CHECK((*i).begin() == 15);
	BOOST_CHECK((*i).end() == 30);

	// Another range of size 1, this time inserting INTO existing range
	BOOST_CHECK_NO_THROW(r.merge(RR(20, 20)));
	// List shouldn't have been changed
	i = r.begin();
	BOOST_CHECK(r.size() == 1);
	BOOST_CHECK((*i).begin() == 15);
	BOOST_CHECK((*i).end() == 30);

	// Add range of size 1 into random place
	BOOST_CHECK_NO_THROW(r.merge(RR(10, 10)));
	i = r.begin();
	BOOST_CHECK(r.size() == 2);
	BOOST_CHECK((*i).begin() == 10);
	BOOST_CHECK((*i).end() == 10);
	++i;
	BOOST_CHECK((*i).begin() == 15);
	BOOST_CHECK((*i).end() == 30);
}

void test_getfree() {
	RangeList16 rl;
	rl.merge(0, 5);
	Range16 ret(rl.getFirstFree(5));
	BOOST_CHECK(ret.begin() == 6);
	BOOST_CHECK(ret.end() == 10);
	rl.merge(ret);
	ret = rl.getFirstFree(10);
	BOOST_CHECK(ret.begin() == 11);
	BOOST_CHECK(ret.end() == 20);
	rl.erase(0, 100);
	BOOST_CHECK(rl.empty());
	ret = rl.getFirstFree(10);
	BOOST_CHECK(ret.begin() == 0);
	BOOST_CHECK(ret.end() == 9);
	rl.merge(ret);
	rl.merge(11, 14);
	ret = rl.getFirstFree(10);
	BOOST_CHECK(ret.begin() == 10);
	BOOST_CHECK(ret.end() == 10);
	rl.merge(ret);
	BOOST_CHECK(rl.size() == 1);
}

void test_contains() {
	RangeList16 rl;
	rl.merge(0, 5);
	rl.merge(6, 10);
	rl.merge(12, 15);
	rl.merge(21, 26);
	BOOST_CHECK(rl.size() == 3);
	BOOST_CHECK(rl.contains(Range16(0)));
	BOOST_CHECK(rl.contains(Range16(4)));
	BOOST_CHECK(rl.contains(Range16(6)));
	BOOST_CHECK(rl.contains(Range16(10)));
	BOOST_CHECK(!rl.contains(Range16(11)));
	BOOST_CHECK(rl.contains(Range16(12)));
	BOOST_CHECK(!rl.contains(Range16(16)));
	BOOST_CHECK(rl.contains(Range16(21)));
	BOOST_CHECK(rl.contains(Range16(26)));
	BOOST_CHECK(!rl.contains(Range16(27)));

	BOOST_CHECK(rl.getContains(0, 0) != rl.end());
	BOOST_CHECK(rl.getContains(5, 5) != rl.end());
	BOOST_CHECK(rl.getContains(12, 15) != rl.end());
	BOOST_CHECK(rl.getContains(0, 0)->begin() == 0);
	BOOST_CHECK(rl.getContains(0, 0)->end() == 10);
	BOOST_CHECK(rl.getContains(0, 5)->begin() == 0);
	BOOST_CHECK(rl.getContains(0, 5)->end() == 10);
	BOOST_CHECK(rl.getContains(12, 13)->begin() == 12);
	BOOST_CHECK(rl.getContains(12, 13)->end() == 15);
	BOOST_CHECK(rl.getContains(16, 16) == rl.end());
	BOOST_CHECK(rl.getContains(12, 20) != rl.end());
	BOOST_CHECK(rl.getContains(12, 20)->begin() == 12);
	BOOST_CHECK(rl.getContains(12, 20)->end() == 15);

	rl.push(0, 15);
	BOOST_CHECK(rl.contains(Range16(0)));
	BOOST_CHECK(rl.contains(Range16(5)));
	BOOST_CHECK(rl.contains(Range16(11)));
	BOOST_CHECK(rl.contains(Range16(12)));
	BOOST_CHECK(rl.contains(Range16(13)));

	rl.clear();
	rl.merge(0, 5);
	rl.merge(10, 15);
	rl.merge(20, 25);
	BOOST_CHECK(rl.contains(0, 2));
	BOOST_CHECK(rl.contains(3, 4));
	BOOST_CHECK(rl.contains(0, 5));
	BOOST_CHECK(rl.contains(5, 6));
	BOOST_CHECK(!rl.contains(6, 9));
	BOOST_CHECK(rl.contains(6, 10));
	BOOST_CHECK(rl.contains(Range16(10)));
	BOOST_CHECK(rl.contains(10, 10));
	BOOST_CHECK(rl.contains(10, 13));
	BOOST_CHECK(rl.contains(0, 20));
	BOOST_CHECK(!rl.contains(26, 28));
}

void test_even_more() {
	RangeList16 rl;
	rl.merge(0, 100);
	rl.erase(0, 50);
	BOOST_CHECK(rl.size() == 1);
	BOOST_CHECK(rl.front().begin() == 51);
	BOOST_CHECK(rl.front().end() == 100);
	rl.clear();
	rl.merge(0, 29);
	rl.merge(34, 50);
	rl.merge(53, 100);
	BOOST_CHECK(rl.getContains(30, 33) == rl.end());
	BOOST_CHECK(rl.getContains(30, 60)->begin() == 34);
	BOOST_CHECK(rl.getContains(30, 60)->end() == 50);
	BOOST_CHECK(rl.getContains(30, 100) != rl.end());
	rl.erase(rl.getContains(30, 60));
	BOOST_CHECK(rl.getContains(30, 60)->begin() == 53);
	BOOST_CHECK(rl.getContains(30, 60)->end() == 100);
	rl.erase(rl.getContains(55, 55));
	BOOST_CHECK(rl.size() == 1);
	BOOST_CHECK(rl.begin()->begin() == 0);
	BOOST_CHECK(rl.begin()->end() == 29);

	RangeList32 rl2;
	rl2.merge(0ul, 1044496385ul);
	rl2.merge(1044496387ul, 4294967290ul);
	BOOST_CHECK(rl2.contains(Range32(123567ul)));
	BOOST_CHECK(rl2.contains(Range32(1044496385ul)));
	BOOST_CHECK(rl2.contains(Range32(123123245ul)));
	BOOST_CHECK(rl2.contains(Range32(1044496384ul)));
	BOOST_CHECK(rl2.contains(Range32(1044496387ul)));
	BOOST_CHECK(rl2.contains(Range32(2669731838ul)));
	BOOST_CHECK(rl2.contains(Range32(2669731840ul)));
	BOOST_CHECK(rl2.contains(Range32(2669731700ul)));
	BOOST_CHECK(!rl2.contains(Range32(1044496386ul)));
}

boost::unit_test::test_suite* init_unit_test_suite(int, char*[]) {
	std::cerr << "Range Management Subsystem: ";
	boost::unit_test::test_suite *test = 0;
	test = BOOST_TEST_SUITE("Range Management Subsystem");
	test->add(BOOST_TEST_CASE(&test_base));
	test->add(BOOST_TEST_CASE(&test_merge));
	test->add(BOOST_TEST_CASE(&test_more));
	test->add(BOOST_TEST_CASE(&test_getfree));
	test->add(BOOST_TEST_CASE(&test_contains));
	test->add(BOOST_TEST_CASE(&test_even_more));
	return test;
}

#endif
