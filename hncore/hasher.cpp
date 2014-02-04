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
 * \file hasher.cpp Implementation of Files Identification Subsystem
 */

#include <hncore/pch.h>

#include <hnbase/md4transform.h>
#include <hnbase/md5transform.h>
#include <hnbase/sha1transform.h>

#include <hncore/hasher.h>
#include <hncore/hashsetmaker.h>
#include <hncore/metadata.h>

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/lexical_cast.hpp>

#include <fstream>
#include <fcntl.h>

using boost::filesystem::path;
using boost::filesystem::no_check;
using namespace CGComm;

uint64_t HashWork::s_dataCnt = 0;
double   HashWork::s_timeCnt = 0.0;
uint32_t HashWork::s_bufSize = 32*1024;
boost::mutex HashWork::s_statsLock;
IMPLEMENT_EVENT_TABLE(HashWork, HashWorkPtr, HashEvent);

// Full hash job Constructor
HashWork::HashWork(const boost::filesystem::path &fileName)
: m_fileName(fileName), m_begin(), m_end(), m_md(), m_ref(), m_valid(true),
m_full(true), m_file(), m_inProgress() {}

// Range hash verification job
HashWork::HashWork(
	const boost::filesystem::path &fileName, uint64_t begin, uint64_t end,
	const HashBase* ref
) : m_fileName(fileName), m_begin(begin), m_end(end), m_md(), m_ref(ref),
m_valid(true), m_full(false), m_file(), m_inProgress() {
	CHECK_THROW(ref);
}

HashWork::~HashWork() {
	if (m_file) {
		close(m_file);
	}
}

uint64_t HashWork::getHashed() { return s_dataCnt; }
double HashWork::getTime() { return s_timeCnt; }
uint32_t HashWork::getBufSize() { return s_bufSize; }

bool HashWork::process() try {
	if (!m_inProgress) {
		initState();
	}
	CHECK_THROW(!isComplete());

	Utils::StopWatch s1;
	doProcess();
	s_timeCnt += s1.elapsed() / 1000.0;

	return isComplete();
} catch (std::exception &e) {
	logError(e.what());
	getEventTable().postEvent(HashWorkPtr(this), HASH_FATAL_ERROR);
	setComplete();
	return isComplete();
}
MSVC_ONLY(;)

void HashWork::openFile() {
	if (m_file) {
		close(m_file);
	}

	std::string fname(m_fileName.native_file_string());
	m_file = open(fname.c_str(), O_RDONLY|O_LARGEFILE|O_BINARY);

	if (m_file == -1 || !boost::filesystem::exists(m_fileName)) {
		throw std::runtime_error(
			(boost::format("Unable to open file `%s' for hashing.")
			% fname).str()
		);
	}
}

void HashWork::initState() {
	openFile();

	if (isFull()) {
		logMsg(
			boost::format("Hashing file `%s'")
			% m_fileName.native_file_string()
		);
	}

	boost::shared_ptr<HashSetMaker> t;

	if (isFull()) {
		t.reset(new ED2KHashMaker);
		m_makers.push_back(t);
		t.reset(new SHA1HashMaker);
		m_makers.push_back(t);
		t.reset(new MD4HashMaker);
		m_makers.push_back(t);
		t.reset(new MD5HashMaker);
		m_makers.push_back(t);
		m_begin = 0;
		m_end = Utils::getFileSize(m_fileName.native_file_string());
	} else {
		switch (getType()) {
			case OP_HT_MD4:
				t.reset(new MD4HashMaker);
				break;
			case OP_HT_MD5:
				t.reset(new MD5HashMaker);
				break;
			case OP_HT_ED2K:
				t.reset(new ED2KHashMaker);
				break;
			case OP_HT_SHA1:
				t.reset(new SHA1HashMaker);
				break;
			default:
				boost::format fmt(
					"Requested unknown hash of type %s"
				);
				logError(fmt % m_ref->getType());
				break;
		}
		m_makers.push_back(t);
		uint64_t ret = lseek64(m_file, m_begin, SEEK_SET);
		CHECK_THROW(ret == m_begin);
	}

	m_buf.reset(new char[s_bufSize]);
	m_inProgress = true;
}

uint64_t HashWork::readNext(uint64_t pos) {
	if (s_bufSize + pos > m_end) {
		return read(m_file, m_buf.get(), m_end - pos + 1);
	} else {
		return read(m_file, m_buf.get(), s_bufSize);
	}
}

void HashWork::doProcess() {
	uint64_t curPos = lseek64(m_file, 0L, SEEK_CUR);
	uint64_t ret = readNext(curPos);

	for (uint32_t i = 0; i < m_makers.size(); ++i) {
		m_makers[i]->sumUp(m_buf.get(), ret);
	}

	boost::mutex::scoped_lock l(s_statsLock);
	s_dataCnt += ret;
	curPos = lseek64(m_file, 0L, SEEK_CUR);

	if (!ret || m_end + 1 == curPos) {
		finish();
	}
}

void HashWork::finish() {
	if (isFull()) {
		CHECK_THROW(m_makers.size());
		std::string fname(m_fileName.native_file_string());
		uint64_t fileSize = Utils::getFileSize(fname);
		uint32_t modDate = Utils::getModDate(fname);
		m_md = new MetaData(fileSize);
		m_md->setModDate(Utils::getModDate(fname));
		m_md->addFileName(m_fileName.leaf());

		logTrace(TRACE_HASHER, "Full hash complete:");
		logTrace(TRACE_HASHER, "FileName: " + fname);
		logTrace(TRACE_HASHER, boost::format("FileSize: %d") %fileSize);
		logTrace(TRACE_HASHER, boost::format("ModDate:  %d") % modDate);
		(void)modDate; // suppress warning in release build

		for (uint32_t i = 0; i < m_makers.size(); ++i) {
			HashSetBase *hs = m_makers[i]->getHashSet();
			logTrace(TRACE_HASHER,
				boost::format("%s: %s")
				% hs->getFileHashType()
				% hs->getFileHash().decode()
			);
			m_md->addHashSet(hs);
		}

		getEventTable().postEvent(HashWorkPtr(this), HASH_COMPLETE);
	} else {
		CHECK_THROW(m_makers.size() == 1);
		const HashBase &h = m_makers[0]->getHashSet()->getFileHash();
		HashEvent evt(h == *m_ref ? HASH_VERIFIED : HASH_FAILED);
#ifndef NDEBUG
		boost::format fmt(
			"Chunk verification (%s, %d..%d): %s [%s (%s)]"
		);
		fmt % m_fileName.leaf() % m_begin % m_end;
		if (evt == HASH_VERIFIED) {
			fmt % "succeeded";
			fmt % m_ref->decode() % m_ref->getType();
			logTrace(TRACE_HASHER, fmt);
		} else {
			fmt % "failed";
			fmt % m_ref->decode() % m_ref->getType();
			logTrace(TRACE_HASHER,
				boost::format("%s != [%s (%s)]")
				% fmt.str() % h.decode() % h.getType()
			);
		}
#endif
		getEventTable().postEvent(HashWorkPtr(this), evt);
	}
	setComplete();
	close(m_file);
	m_file = 0;
	m_buf.reset();
}
