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
 * \file test-metadata.cpp Test app for MetaData Subsystem
 */

#ifndef DOXYGEN_SHOULD_SKIP_THIS


#include <hncore/metadata.h>
#include <hnbase/hash.h>
#include <hncore/metadb.h>
#include <stdexcept>
#include <fstream>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>

using boost::unit_test_framework::test_suite;
using namespace boost::unit_test_framework;
using namespace CGComm;

static bool hexdump = false;

AudioMetaData *amd;
void init_audiodata() {
	logMsg(" -> Initializing AudioMetaData.");

	amd = new AudioMetaData();
	amd->setTitle("Song Title");
	amd->setArtist("Song artist");
	amd->setAlbum("Unknown");
	// Wait - changed my mind
	amd->setAlbum("Some odd album");
	amd->setGenre("Something");
	amd->setComment("This is nice.");
	amd->setComposer("Someone nobody really knows.");
	amd->setOrigArtist("Long forgotten.");
	amd->setUrl("http://somewhere.org");
	amd->setEncoded("What's here ?");
	amd->setYear(1999);
	// Verify
	BOOST_CHECK(amd->getTitle() ==  "Song Title");
	BOOST_CHECK(amd->getArtist() == "Song artist");
	BOOST_CHECK(amd->getAlbum() == "Some odd album");
	BOOST_CHECK(amd->getGenre() == "Something");
	BOOST_CHECK(amd->getComment() == "This is nice.");
	BOOST_CHECK(amd->getComposer() == "Someone nobody really knows.");
	BOOST_CHECK(amd->getOrigArtist() == "Long forgotten.");
	BOOST_CHECK(amd->getUrl() == "http://somewhere.org");
	BOOST_CHECK(amd->getEncoded() == "What's here ?");
	BOOST_CHECK(amd->getYear() == 1999);
}
void test_audiodata() {
	logMsg(" -> Testing AudioMetaData I/O.");

	// Save
	std::ostringstream o;
	o << *amd;
	if (hexdump) Utils::hexDump(std::cerr, o.str());

	// Load
	std::istringstream i(o.str());
	uint8_t opcode = Utils::getVal<uint8_t>(i);
	BOOST_CHECK(opcode == OP_AMD);
	(void)Utils::getVal<uint16_t>(i); // length

	AudioMetaData *amd1 = new AudioMetaData(i);

	// Verify
	BOOST_CHECK(amd->getTitle() == amd1->getTitle());
	BOOST_CHECK(amd->getArtist() == amd1->getArtist());
	BOOST_CHECK(amd->getAlbum() == amd1->getAlbum());
	BOOST_CHECK(amd->getGenre() == amd1->getGenre());
	BOOST_CHECK(amd->getComment() == amd1->getComment());
	BOOST_CHECK(amd->getComposer() == amd1->getComposer());
	BOOST_CHECK(amd->getOrigArtist() == amd1->getOrigArtist());
	BOOST_CHECK(amd->getUrl() == amd1->getUrl());
	BOOST_CHECK(amd->getEncoded() == amd1->getEncoded());
	BOOST_CHECK(amd->getYear() == amd1->getYear());

	// Can't delete it - push into metadb
	MetaData *md = new MetaData();
	md->addAudioData(amd1);
	MetaDb::instance().push(md);
}

VideoMetaData *vmd;
void init_videodata() {
	logMsg(" -> Initializing VideoMetaData.");

	vmd = new VideoMetaData();
	vmd->setRunTime(1234);
	vmd->setFrameCount(12313423);
	vmd->setFrameRate(21.435);
	vmd->setSubtitleCount(5);
	vmd->setFrameSize(380, 460);
	vmd->addVideoStream("DivX5", 920000);
	vmd->addVideoStream("XVid", 12800000);
	vmd->addAudioStream("MPEG Layer 3", 128000);
	vmd->addAudioStream("AC3", 256000);
	// Verify
	BOOST_CHECK(vmd->getRunTime() == 1234);
	BOOST_CHECK(vmd->getFrameCount() == 12313423);
	BOOST_CHECK(vmd->getFrameRate() == 21.435f);
	BOOST_CHECK(vmd->getSubtitleCount() == 5);
	BOOST_CHECK(vmd->getFrameSize().first == 380);
	BOOST_CHECK(vmd->getFrameSize().second = 460);
	BOOST_CHECK(vmd->getVideoStreamCount() == 2);
	BOOST_CHECK(vmd->getVideoStream(0).getCodec() == "DivX5");
	BOOST_CHECK(vmd->getVideoStream(1).getCodec() == "XVid");
	BOOST_CHECK(vmd->getVideoStream(0).getBitrate() == 920000);
	BOOST_CHECK(vmd->getVideoStream(1).getBitrate() == 12800000);
	BOOST_CHECK(vmd->getAudioStreamCount() == 2);
	BOOST_CHECK(vmd->getAudioStream(0).getCodec() == "MPEG Layer 3");
	BOOST_CHECK(vmd->getAudioStream(1).getCodec() == "AC3");
	BOOST_CHECK(vmd->getAudioStream(0).getBitrate() == 128000);
	BOOST_CHECK(vmd->getAudioStream(1).getBitrate() == 256000);
}
void test_videodata() {
	logMsg(" -> Testing VideoMetaData I/O.");

	// Save
	std::ostringstream o;
	o << *vmd;
	if (hexdump) Utils::hexDump(std::cerr, o.str());
	// Load
	std::istringstream i(o.str());
	uint8_t opcode = Utils::getVal<uint8_t>(i);
	BOOST_CHECK(opcode == OP_VMD);
	(void)Utils::getVal<uint16_t>(i); // length

	VideoMetaData *vmd2 = new VideoMetaData(i);
	// verify
	BOOST_CHECK(vmd->getRunTime() == vmd2->getRunTime());
	BOOST_CHECK(vmd->getFrameCount() == vmd2->getFrameCount());
	BOOST_CHECK(vmd->getFrameRate() == vmd2->getFrameRate());
	BOOST_CHECK(vmd->getSubtitleCount() == vmd2->getSubtitleCount());
	BOOST_CHECK(vmd->getFrameSize() == vmd2->getFrameSize());
	BOOST_CHECK(vmd->getVideoStreamCount() == vmd2->getVideoStreamCount());
	BOOST_CHECK(vmd->getVideoStream(0).getCodec() == vmd2->getVideoStream(0).getCodec());
	BOOST_CHECK(vmd->getVideoStream(1).getCodec() == vmd2->getVideoStream(1).getCodec());
	BOOST_CHECK(vmd->getVideoStream(0).getBitrate() == vmd2->getVideoStream(0).getBitrate());
	BOOST_CHECK(vmd->getVideoStream(1).getBitrate() == vmd2->getVideoStream(1).getBitrate());
	BOOST_CHECK(vmd->getAudioStreamCount() == vmd2->getAudioStreamCount());
	BOOST_CHECK(vmd->getAudioStream(0).getCodec() == vmd2->getAudioStream(0).getCodec());
	BOOST_CHECK(vmd->getAudioStream(1).getCodec() == vmd2->getAudioStream(1).getCodec());
	BOOST_CHECK(vmd->getAudioStream(0).getBitrate() == vmd2->getAudioStream(0).getBitrate());
	BOOST_CHECK(vmd->getAudioStream(1).getBitrate() == vmd2->getAudioStream(1).getBitrate());

	// Can't delete vmd2 ... push into metadb
	MetaData *md = new MetaData();
	md->addVideoData(vmd2);
	MetaDb::instance().push(md);
}

ArchiveMetaData *armd;
void init_archivedata() {
	logMsg(" -> Initializing ArchiveMetaData.");

	armd = new ArchiveMetaData();
	armd->setFormat(1234);
	armd->setFileCount(4321);
	armd->setUnComprSize(1231231223);
	armd->setComprRatio(12.1231);
	armd->setPassword(true);
	armd->setComment("Very wierd archive.");
	// Verify
	BOOST_CHECK(armd->getFormat() == 1234);
	BOOST_CHECK(armd->getFileCount() == 4321);
	BOOST_CHECK(armd->getUnComprSize() == 1231231223);
	BOOST_CHECK(armd->getComprRatio() == 12.1231f);
	BOOST_CHECK(armd->getPassword() == true);
	BOOST_CHECK(armd->getComment() == "Very wierd archive.");
}

void test_archivedata() {
	logMsg(" -> Testing ArchiveMetaData I/O.");

	// Save
	std::ostringstream o;
	o << *armd;
	if (hexdump) Utils::hexDump(std::cerr, o.str());

	// Load
	std::istringstream i(o.str());
	uint8_t opcode = Utils::getVal<uint8_t>(i);
	BOOST_CHECK(opcode == OP_ARMD);
	(void)Utils::getVal<uint16_t>(i); // length

	ArchiveMetaData *armd2 = new ArchiveMetaData(i);
	// Verify
	BOOST_CHECK(armd->getFormat() == armd2->getFormat());
	BOOST_CHECK(armd->getFileCount() == armd2->getFileCount());
	BOOST_CHECK(armd->getUnComprSize() == armd2->getUnComprSize());
	BOOST_CHECK(armd->getComprRatio() == armd2->getComprRatio());
	BOOST_CHECK(armd->getPassword() == armd2->getPassword());
	BOOST_CHECK(armd->getComment() == armd2->getComment());

	// Can't delete it - push into metadb
	MetaData *md = new MetaData();
	md->setArchiveData(armd2);
	MetaDb::instance().push(md);
}

ImageMetaData *imd;
void init_imagedata() {
	logMsg(" -> Initializing ImageMetaData.");

	imd = new ImageMetaData();
	imd->setFormat(6543);
	imd->setWidth(1024);
	imd->setHeight(1280);
	imd->setCreated(5432);
	imd->setComment("This is an odd picture.");

	// Verify
	BOOST_CHECK(imd->getFormat() == 6543);
	BOOST_CHECK(imd->getWidth() == 1024);
	BOOST_CHECK(imd->getHeight() == 1280);
	BOOST_CHECK(imd->getCreated() == 5432);
	BOOST_CHECK(imd->getComment() == "This is an odd picture.");
}
void test_imagedata() {
	logMsg(" -> Testing ImageMetaData I/O.");

	// Save
	std::ostringstream o;
	o << *imd;
	if (hexdump) Utils::hexDump(std::cerr, o.str());

	// Load
	std::istringstream i(o.str());
	uint8_t opcode = Utils::getVal<uint8_t>(i);
	BOOST_CHECK(opcode == OP_IMD);
	(void)Utils::getVal<uint16_t>(i); // length

	ImageMetaData *imd2 = new ImageMetaData(i);

	// verify
	BOOST_CHECK(imd->getFormat() == imd2->getFormat());
	BOOST_CHECK(imd->getWidth() == imd2->getWidth());
	BOOST_CHECK(imd->getHeight() == imd2->getHeight());
	BOOST_CHECK(imd->getCreated() == imd2->getCreated());
	BOOST_CHECK(imd->getComment() == imd2->getComment());

	// Can't delete it - push into metadb
	MetaData *md = new MetaData();
	md->setImageData(imd2);
	MetaDb::instance().push(md);
}

void test_hash() {
	logMsg(" -> Testing Hash and HashSet objects I/O.");

	const char TestData1[16] = {
		0x09, 0x40, 0x46, 0x8a, 0x42, 0xc0, 0x8f, 0x01,
		0xc4, 0x3b, 0x07, 0xc7, 0xa6, 0x2c, 0x2d, 0xae
	};
	const std::string TestDecoded("0940468a42c08f01c43b07c7a62c2dae");

	Hash<MD4Hash> h(TestData1);
	BOOST_CHECK_EQUAL(h.size(), 16);
	BOOST_CHECK(!memcmp(h.getData().get(), TestData1, 16));
	BOOST_CHECK(TestDecoded == Utils::decode(h.getData(), h.size()));

	{
		std::ostringstream o;
		o << h;
		std::istringstream i(o.str());
		uint8_t opcode = Utils::getVal<uint8_t>(i);
		BOOST_CHECK(opcode == OP_HASH);
		uint16_t len = Utils::getVal<uint16_t>(i);
		BOOST_CHECK(len == 17);
		uint8_t hashtype = Utils::getVal<uint8_t>(i);
		BOOST_CHECK(hashtype == OP_HT_MD4);
		Hash<MD4Hash> h1(i);
		BOOST_CHECK(h.size() == 16);
		BOOST_CHECK(!memcmp(h.getData().get(), h1.getData().get(), 16));
		BOOST_CHECK(TestDecoded == Utils::decode(h1.getData(), h1.size()));
	}

	std::string htmd4("MD4Hash");
	BOOST_CHECK(h.getType() == "MD4Hash");

	Hash<ED2KHash> hh(TestData1);
	BOOST_CHECK(hh.getType() == "ED2KHash");

	{
		std::ostringstream o;
		o << hh;
		std::istringstream i(o.str());
		uint8_t opcode = Utils::getVal<uint8_t>(i);
		BOOST_CHECK(opcode == OP_HASH);
		uint16_t len = Utils::getVal<uint16_t>(i);
		BOOST_CHECK(len == 17);
		uint8_t hashtype = Utils::getVal<uint8_t>(i);
		BOOST_CHECK(hashtype == OP_HT_ED2K);
		Hash<ED2KHash> h1(i);
		BOOST_CHECK(h.size() == 16);
		BOOST_CHECK(!memcmp(h.getData().get(), hh.getData().get(), 16));
	}

	HashSet<MD4Hash> hs(Utils::encode("0940468a42c08f01c43b07c7a62c2dae"));
	hs.addChunkHash(Hash<MD4Hash>(Utils::encode("0940468a42c08f01c43b07c7a62c2dae")));
	hs.setFileHash(Hash<MD4Hash>(Utils::encode("1234468a42c08f01c43b07c7a62c2dae")));
	std::vector<HashSetBase*> hashes;
        hashes.push_back(&hs);

	BOOST_CHECK(Hash<MD4Hash>(Utils::encode("1234468a42c08f01c43b07c7a62c2dae")) == hashes[0]->getFileHash());
	BOOST_CHECK(Hash<MD4Hash>(Utils::encode("0940468a42c08f01c43b07c7a62c2dae")) == hashes[0]->getChunkHash(0));
	BOOST_CHECK(hashes[0]->getFileHashTypeId() == OP_HT_MD4);
	BOOST_CHECK(hashes[0]->getChunkCnt() == 1);
	BOOST_CHECK(hashes[0]->getChunkHashTypeId() == OP_HT_MD4);

	{
		std::ostringstream o;
		o << hs;
		if (hexdump) Utils::hexDump(std::cerr, o.str());
		std::istringstream i(o.str());
		uint8_t opcode = Utils::getVal<uint8_t>(i);
		BOOST_CHECK(opcode == OP_HASHSET);
		(void)Utils::getVal<uint16_t>(i); // length
		uint8_t fhtype = Utils::getVal<uint8_t>(i);
		BOOST_CHECK(fhtype == OP_HT_MD4);
		uint8_t phtype = Utils::getVal<uint8_t>(i);
		BOOST_CHECK(phtype == OP_HT_MD4);
		HashSet<MD4Hash> hs1(i);
		BOOST_CHECK(Hash<MD4Hash>(Utils::encode("1234468a42c08f01c43b07c7a62c2dae")) == hs1.getFileHash());
		BOOST_CHECK(hs1.getChunkCnt() == 1);
		BOOST_CHECK(Hash<MD4Hash>(Utils::encode("0940468a42c08f01c43b07c7a62c2dae")) == hs1.getChunkHash(0));
		BOOST_CHECK(hs1 == *hashes[0]);
	}


	HashSet<MD4Hash, ED2KHash, 9728000> ed2kh(Utils::encode("0940468a42c08f01c43b07c7a62c2dae"));
	ed2kh.addChunkHash(Utils::encode("A768A98215B4BEAA44956F31F5C539DA"));
	ed2kh.addChunkHash(Utils::encode("C9C6B4612E93C4AF094C67F02EE55F54"));
	ed2kh.addChunkHash(Utils::encode("0940468a42c08f01c43b07c7a62c2dae"));
	ed2kh.addChunkHash(Utils::encode("1234468a42c08f01c43b07c7a62c2dae"));
	BOOST_CHECK(ed2kh.getFileHashType() == "ED2KHash");
	BOOST_CHECK(ed2kh.getChunkHashType() == "MD4Hash");
	BOOST_CHECK(ed2kh.getFileHash().decode() == "0940468a42c08f01c43b07c7a62c2dae");
	BOOST_CHECK(ed2kh.getChunkCnt() == 4);
	BOOST_CHECK(ed2kh.getChunkHash(0).decode() == "a768a98215b4beaa44956f31f5c539da");
	BOOST_CHECK(ed2kh.getChunkHash(1).decode() == "c9c6b4612e93c4af094c67f02ee55f54");
	BOOST_CHECK(ed2kh.getChunkHash(2).decode() == "0940468a42c08f01c43b07c7a62c2dae");
	BOOST_CHECK(ed2kh.getChunkHash(3).decode() == "1234468a42c08f01c43b07c7a62c2dae");
	BOOST_CHECK_THROW(ed2kh.getChunkHash(4).decode(), std::out_of_range);

	{
		std::ostringstream o;
		o << ed2kh;
		if (hexdump) Utils::hexDump(std::cerr, o.str());
		std::istringstream i(o.str());
		HashSetBase *hsb = loadHashSet(i);
		BOOST_CHECK(hsb->getFileHashType() == "ED2KHash");
		BOOST_CHECK(hsb->getChunkHashType() == "MD4Hash");
		BOOST_CHECK(hsb->getFileHash().decode() == "0940468a42c08f01c43b07c7a62c2dae");
		BOOST_CHECK(hsb->getChunkCnt() == 4);
		BOOST_CHECK(hsb->getChunkHash(0).decode() == "a768a98215b4beaa44956f31f5c539da");
		BOOST_CHECK(hsb->getChunkHash(1).decode() == "c9c6b4612e93c4af094c67f02ee55f54");
		BOOST_CHECK(hsb->getChunkHash(2).decode() == "0940468a42c08f01c43b07c7a62c2dae");
		BOOST_CHECK(hsb->getChunkHash(3).decode() == "1234468a42c08f01c43b07c7a62c2dae");
		delete hsb;
	}
}

MetaData *md;
void init_metadata() {
	logMsg(" -> Initializing MetaData object.");

	md = new MetaData();
	md->addVideoData(vmd);
	md->addAudioData(amd);
	BOOST_CHECK_THROW(md->getArchiveData(), std::runtime_error);
	md->setArchiveData(armd);
	BOOST_CHECK_NO_THROW(md->getArchiveData());
	BOOST_CHECK_THROW(md->getImageData(), std::runtime_error);
	md->setImageData(imd);
	BOOST_CHECK_NO_THROW(md->getImageData());
	md->setSize(123123123);
	md->setModDate(123123);
	md->setFileType(FT_UNKNOWN);
	md->setTypeGuessed(true);
	md->addFileName("Boo!");
	md->addFileName("Hello world.avi");
	md->addFileName("somefile.zip");
	md->addCustomData("Hello World");
	md->addCustomData("Doh");

	BOOST_CHECK(md->getVideoDataCount() == 1);
	BOOST_CHECK(md->getVideoData(0)->getRunTime() == 1234);
	BOOST_CHECK_THROW(md->getVideoData(1)->getRunTime(), std::out_of_range);
	BOOST_CHECK(md->getAudioDataCount() == 1);
	BOOST_CHECK(md->getAudioData(0)->getTitle() == "Song Title");
	BOOST_CHECK_THROW(md->getAudioData(1)->getTitle(), std::out_of_range);
	BOOST_CHECK(md->getArchiveData()->getComment() == "Very wierd archive.");
	BOOST_CHECK(md->getImageData()->getComment() == "This is an odd picture.");
	BOOST_CHECK(md->getSize() == 123123123);
	BOOST_CHECK(md->getModDate() == 123123);
	BOOST_CHECK(md->getFileType() == FT_UNKNOWN);
	BOOST_CHECK(md->getTypeGuessed() == true);
	BOOST_CHECK(md->getFileNameCount() == 3);
	BOOST_CHECK(md->getFileName(0) == "Boo!");
	BOOST_CHECK(md->getFileName(1) == "Hello world.avi");
	BOOST_CHECK(md->getFileName(2) == "somefile.zip");
	BOOST_CHECK(md->getCustomCount() == 2);
	BOOST_CHECK(md->getCustomData(0) == "Hello World");
	BOOST_CHECK(md->getCustomData(1) == "Doh");
	BOOST_CHECK_THROW(md->getFileName(3), std::out_of_range);

	HashSet<MD4Hash, ED2KHash, 9728000> *ed2kh;
	ed2kh = new HashSet<MD4Hash, ED2KHash, 9728000>(
		Utils::encode("0940468a42c08f01c43b07c7a62c2dae")
	);
	ed2kh->addChunkHash(Utils::encode("A768A98215B4BEAA44956F31F5C539DA"));
	ed2kh->addChunkHash(Utils::encode("C9C6B4612E93C4AF094C67F02EE55F54"));
	ed2kh->addChunkHash(Utils::encode("0940468a42c08f01c43b07c7a62c2dae"));
	ed2kh->addChunkHash(Utils::encode("1234468a42c08f01c43b07c7a62c2dae"));
	md->addHashSet(ed2kh);
	MetaDb::instance().push(md);
}

void test_metadata() {
	logMsg(" -> Testing MetaData I/O.");

	std::ostringstream o;
	o << *md;
	if (hexdump) Utils::hexDump(std::cerr, o.str());
	std::istringstream i(o.str());
	uint8_t opcode = Utils::getVal<uint8_t>(i);
	BOOST_CHECK(opcode == OP_METADATA);
	(void)Utils::getVal<uint16_t>(i); // length

	MetaData *md1 = new MetaData(i);

	BOOST_CHECK(md1->getVideoDataCount() == 1);
	BOOST_CHECK(md1->getVideoData(0)->getRunTime() == 1234);
	BOOST_CHECK_THROW(md->getVideoData(1)->getRunTime(), std::out_of_range);
	BOOST_CHECK(md1->getAudioDataCount() == 1);
	BOOST_CHECK(md1->getAudioData(0)->getTitle() == "Song Title");
	BOOST_CHECK_THROW(md1->getAudioData(1)->getTitle(), std::out_of_range);
	BOOST_CHECK(md1->getArchiveData()->getComment() == "Very wierd archive.");
	BOOST_CHECK(md1->getImageData()->getComment() == "This is an odd picture.");
	BOOST_CHECK(md1->getSize() == 123123123);
	BOOST_CHECK(md1->getModDate() == 123123);
	BOOST_CHECK(md1->getFileType() == FT_UNKNOWN);
	BOOST_CHECK(md1->getTypeGuessed() == true);
	BOOST_CHECK(md1->getFileNameCount() == 3);
	BOOST_CHECK(md1->getFileName(0) == "Boo!");
	BOOST_CHECK(md1->getFileName(1) == "Hello world.avi");
	BOOST_CHECK(md1->getFileName(2) == "somefile.zip");
	BOOST_CHECK(md->getCustomCount() == 2);
	BOOST_CHECK(md->getCustomData(0) == "Hello World");
	BOOST_CHECK(md->getCustomData(1) == "Doh");
	BOOST_CHECK_THROW(md1->getFileName(3), std::out_of_range);

	MetaDb::instance().push(md1);
}

void test_metadb() {
	logMsg(" -> Testing MetaDb I/O.");
	std::ostringstream o;
	MetaDb::instance().save(o);
	if (hexdump) {
		logMsg("-> MetaDb Dump:");
		Utils::hexDump(std::cerr, o.str());
	}
	MetaDb::instance().clear();
	std::istringstream i(o.str());
	MetaDb::instance().load(i);

	MetaData *md = MetaDb::instance().find(
		Hash<ED2KHash>(
			Utils::encode("0940468a42c08f01c43b07c7a62c2dae")
		)
	);
	BOOST_CHECK(md != 0);
	BOOST_CHECK(md->getHashSetCount() == 1);
	BOOST_CHECK(md->getHashSet(0)->getFileHash().decode() == "0940468a42c08f01c43b07c7a62c2dae");
	BOOST_CHECK(md->getHashSet(0)->getChunkCnt() == 4);
	BOOST_CHECK(md->getHashSet(0)->getChunkHash(0).decode() == "a768a98215b4beaa44956f31f5c539da");
	BOOST_CHECK(md->getHashSet(0)->getChunkHash(1).decode() == "c9c6b4612e93c4af094c67f02ee55f54");
	BOOST_CHECK(md->getHashSet(0)->getChunkHash(2).decode() == "0940468a42c08f01c43b07c7a62c2dae");
	BOOST_CHECK(md->getHashSet(0)->getChunkHash(3).decode() == "1234468a42c08f01c43b07c7a62c2dae");

	std::vector<MetaData*> ret = MetaDb::instance().find("Boo!");
	BOOST_CHECK(ret.size() == 2);
	BOOST_CHECK(ret[0]->getFileName(0) == "Boo!");
	BOOST_CHECK(ret[0]->getFileName(1) == "Hello world.avi");
	BOOST_CHECK(ret[0]->getFileName(2) == "somefile.zip");
	BOOST_CHECK(ret[1]->getFileName(0) == "Boo!");
	BOOST_CHECK(ret[1]->getFileName(1) == "Hello world.avi");
	BOOST_CHECK(ret[1]->getFileName(2) == "somefile.zip");
}

boost::unit_test_framework::test_suite* init_unit_test_suite(
        int argc, char* argv[]
) {
	if (argc > 1 && !strcmp(argv[1], "-v")) {
		Log::instance().enableTraceMask(TRACE_MD, "MetaData");
		Log::instance().enableTraceMask(TRACE_HASH, "Hash");
		Log::instance().enableTraceMask(METADB, "MetaDb");
		hexdump = true;
	}
	logMsg("MetaData Structures I/O Test.");
	logMsg("Run with `-v` argument for verbose trace output.");

	unit_test_log.set_threshold_level(log_all_errors);
        test_suite *rms = BOOST_TEST_SUITE("MetaData Structures I/O Test");

	rms->add(BOOST_TEST_CASE(&init_audiodata));
	rms->add(BOOST_TEST_CASE(&test_audiodata));
	rms->add(BOOST_TEST_CASE(&init_videodata));
	rms->add(BOOST_TEST_CASE(&test_videodata));
	rms->add(BOOST_TEST_CASE(&init_archivedata));
	rms->add(BOOST_TEST_CASE(&test_archivedata));
	rms->add(BOOST_TEST_CASE(&init_imagedata));
	rms->add(BOOST_TEST_CASE(&test_imagedata));
	rms->add(BOOST_TEST_CASE(&test_hash));
	rms->add(BOOST_TEST_CASE(&init_metadata));
	rms->add(BOOST_TEST_CASE(&test_metadata));
	rms->add(BOOST_TEST_CASE(&test_metadb));
	return rms;
}


#endif
