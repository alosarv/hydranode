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

/** @file test-hash.cpp Test app for Hash classes */

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#include <hnbase/hash.h>
#include <boost/progress.hpp>
#include <boost/test/minimal.hpp>

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

// performance-testing functions
#include <vector>
void test_hash_construct();
void test_hash_destruct();
void test_hash_copy();
void test_hash_type();
std::vector<HashBase*> hashes;

void test_hash() {
	Hash<MD4Hash> h(TestData1);
	BOOST_CHECK(h.size() == 16);
	BOOST_CHECK(!memcmp(h.getData().get(), TestData1, 16));
	BOOST_CHECK(TestDecoded == Utils::decode(h.getData(), h.size()));

	std::string htmd4("MD4Hash");
	BOOST_CHECK(h.getType() == "MD4Hash");

	Hash<ED2KHash> hh(TestData1);
	BOOST_CHECK(hh.getType() == "ED2KHash");

	HashSet<MD4Hash> hs("0940468a42c08f01c43b07c7a62c2dae");
	hs.addChunkHash(Hash<MD4Hash>("0940468a42c08f01c43b07c7a62c2dae"));
	hs.setFileHash(Hash<MD4Hash>("1234468a42c08f01c43b07c7a62c2dae"));

	std::vector<HashSetBase*> hashes;
	hashes.push_back(&hs);

	BOOST_CHECK(Hash<MD4Hash>("1234468a42c08f01c43b07c7a62c2dae") == hashes[0]->getFileHash());
	BOOST_CHECK(Hash<MD4Hash>("0940468a42c08f01c43b07c7a62c2dae") == hashes[0]->getChunkHash(0));
	BOOST_CHECK(hashes[0]->getFileHashTypeId() == CGComm::OP_HT_MD4);
	BOOST_CHECK(hashes[0]->getChunkCnt() == 1);
	BOOST_CHECK(hashes[0]->getChunkHashTypeId() == CGComm::OP_HT_MD4);

	HashSet<MD4Hash, ED2KHash, 9728000> ed2kh("0940468a42c08f01c43b07c7a62c2dae");
	BOOST_CHECK(ed2kh.getFileHashType() == "ED2KHash");
	BOOST_CHECK(ed2kh.getChunkHashType() == "MD4Hash");
}
static const unsigned int HASHCOUNT = 1000000;
const char h1[16] = {
	0x09, 0x40, 0x46, 0x8a, 0x42, 0xc0, 0x8f, 0x01,
	0xc4, 0x3b, 0x07, 0xc7, 0xa6, 0x2c, 0x2d, 0xae
};

void test_hash_construct() {
	std::cerr << "Constructing 1'000'000 hashes: ";
	boost::progress_timer t;
	for (unsigned int i = 0; i < HASHCOUNT; i++) {
		hashes.push_back(
			new Hash<ED2KHash>("0940468a42c08f01c43b07c7a62c2dae")
		);
	}
}
void test_hash_destruct() {
	std::cerr << "Destroying 1'000'000 hashes: ";
	boost::progress_timer t;
	for (unsigned int i = 0; i < HASHCOUNT; i++) {
		delete hashes.back();
		hashes.pop_back();
	}
}
void test_hash_copy() {
	std::cerr << "Copying 1'000'000 hashes: ";
	Hash<ED2KHash> h;
	Hash<ED2KHash> g("0940468a42c08f01c43b07c7a62c2dae");
	boost::progress_timer t;
	for (unsigned int i = 0; i < HASHCOUNT; i++) {
		h = g;
	}
}
void test_hash_type() {
	std::cerr << "Calling ED2KHash::getType() 1'000'000 times: ";
	boost::progress_timer t;
	for (unsigned int i = 0; i < HASHCOUNT; i++) {
		ED2KHash::getType();
	}
}

void test_hash_cont() {
	std::set<Hash<MD4Hash> > hashList;
	hashList.insert(Hash<MD4Hash>(Utils::encode("0940468a42c08f01c43b07c7a62c2dae")));
	hashList.insert(Hash<MD4Hash>(Utils::encode("1234468a42c08f01c43b07c7a62c2dae")));
	hashList.insert(Hash<MD4Hash>(Utils::encode("128F393B0179DBB69A2BECA411E6181A")));
	std::set<Hash<MD4Hash> >::iterator i = hashList.find(Utils::encode("1234468a42c08f01c43b07c7a62c2dae"));
	BOOST_CHECK(i != hashList.end());
}

void test_hash_oper() {
	Hash<MD4Hash> h1;
	Hash<MD4Hash> h2(Utils::encode("0940468a42c08f01c43b07c7a62c2dae"));
	Hash<MD4Hash> h3;
	h3 = h2; // assign value to empty hash
	BOOST_CHECK(h3 == h2);
	h3 = h1; // assign emptyness to valued hash
	BOOST_CHECK(h3 == h1);
	h3 = h2;
	BOOST_CHECK(h3 == h2);
	h3 = h2; // assign value to valued hash
	BOOST_CHECK(h3 == h2);
}

int test_main(int, char*[]) {
	test_hash_construct();
	test_hash_destruct();
	test_hash_copy();
	test_hash_type();
	test_hash_cont();
	test_hash_oper();
	return 0;
}

#endif
