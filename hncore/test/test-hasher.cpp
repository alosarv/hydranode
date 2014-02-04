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
 * \file test-hasher.cpp Regress-test for Files Identification Subsystem
 */

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#include <hncore/hasher.h>
#include <hnbase/log.h>
#include <hncore/metadata.h>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#define _F(x) boost::format(x)

class HashWorkTester {
public:
	HashWorkTester(const std::string &path);
	~HashWorkTester();
	void onHashEvent(boost::shared_ptr<HashWork> hw, HashEvent evt);
	void onHashComplete(MetaData *md, const boost::filesystem::path &path);
private:
	uint32_t workCount;
	uint32_t rangeCheckCount;
};
// Constructor
HashWorkTester::HashWorkTester(const std::string &path)
: workCount(0), rangeCheckCount(0) {
	logMsg(_F("Enumerating files in directory %s") % path);
	try {
		boost::filesystem::directory_iterator iter(path);
		boost::filesystem::directory_iterator end_iter;
		while (iter != end_iter && boost::filesystem::exists(*iter)) {
			if (boost::filesystem::is_directory(*iter)) {
				++iter;
				continue;
			}
			logMsg(_F("Hashing file %s") % iter->string());
			boost::shared_ptr<HashWork> hw(
				new HashWork((*iter).string())
			);
			HashWork::getEventTable().addHandler(
				hw, this, &HashWorkTester::onHashEvent
			);
			WorkThread::instance().postWork(hw);
			++iter;
			++workCount;
			if (workCount >= 10) {
				break;
			}
		}
		logMsg(boost::format("%d jobs scheduled.") % workCount);
		if (workCount == 0) {
			logMsg("Nothing to do, exiting.");
			exit(0);
		}
		logMsg("Waiting for Hasher to complete the jobs.");
	} catch (std::exception &e) {
		logError(boost::format("Error: %s") % e.what());
		logError(
			"Exception was thrown in HashWorkTester. "
			"Most likely the "
		);
		logError(
			"passed path was not legal. "
			"Please re-try with other path."
		);
		exit(1);
	}
}

// Destructor
HashWorkTester::~HashWorkTester() {
	if (workCount > 0) {
		logError(
			_F("+++ There are still %d pending jobs to do! +++")
			% workCount
		);
	} else {
		logMsg("All jobs have been performed successfully.");
	}
	if (rangeCheckCount > 0) {
		logError(
			boost::format(" +++ %d range checks failed. +++ ")
			% rangeCheckCount
		);
	} else {
		logMsg("All Range verifications succeeded.");
	}
}
void HashWorkTester::onHashComplete(
	MetaData *md, const boost::filesystem::path &path
) {
	logMsg(
		boost::format("Hash complete: %s")
		% path.native_directory_string()
	);
	logMsg(
		boost::format("File Size: %d bytes (%s)")
		% md->getSize() % Utils::bytesToString(md->getSize())
	);
	logMsg(boost::format("Number of HashSets: %d") % md->getHashSetCount());
	for (uint32_t i = 0; i < md->getHashSetCount(); ++i) {
		boost::format fmt("Type: %s ChunkSize: %d FileHash: %s");
		HashSetBase *hs = md->getHashSet(i);
		logMsg(
			fmt % hs->getFileHashType() % hs->getChunkSize()
			% hs->getFileHash().decode()
		);
		// Schedule all ranges in all generated hashsets for
		// verification against the range hashes already generated.
		uint32_t pos = 0;
		for (uint32_t j = 0; j < hs->getChunkCnt(); ++j) {
			boost::format fmt("ChunkHash(%s)[%d]: %s");
			fmt % hs->getChunkHashType() % j;
			fmt % hs->getChunkHash(j).decode();
			logMsg(fmt);
			boost::shared_ptr<HashWork> hw(
				new HashWork(
					path, pos,
					pos + hs->getChunkSize() - 1,
					&hs->getChunkHash(j)
				)
			);
			pos = hs->getChunkSize()*(j+1);
			HashWork::getEventTable().addHandler(
				hw, this, &HashWorkTester::onHashEvent
			);
			WorkThread::instance().postWork(hw);
			++rangeCheckCount;
			++workCount;
		}
	}
}

void HashWorkTester::onHashEvent(boost::shared_ptr<HashWork> hw, HashEvent evt){
	boost::format fmt("Range %d..%d verification against %d %s.");
	--workCount;
	if (evt == HASH_COMPLETE) {
		if (hw->getMetaData()) {
			onHashComplete(hw->getMetaData(), hw->getFileName());
		} else {
			logError(
				"HashWorkTester: No MetaData generated in work."
			);
		}
	} else if (evt == HASH_VERIFIED) {
		--rangeCheckCount;
		fmt % hw->begin() % hw->end() % hw->getRef()->decode();
		logDebug(fmt % "succeeded");
	} else if (evt == HASH_FAILED) {
		fmt % hw->begin() % hw->end() % hw->getRef()->decode();
		logError(fmt % "FAILED!");
	} else if (evt == HASH_FATAL_ERROR) {
		logError(
			boost::format("Fatal error hashing file %s.")
			% hw->getFileName().native_directory_string()
		);
	}
	if (!workCount) {
		logMsg("All work completed.");
		EventMain::instance().exitMainLoop();
	} else {
		logMsg(boost::format("%d jobs still pending.") % workCount);
	}
}

#ifdef __linux__
	#include <signal.h>
	void onSignal(int signum) {
		if (signum != SIGQUIT && signum != SIGTERM && signum != SIGINT){
			return;
		}
		EventMain::instance().exitMainLoop();
	}
	void initSignalHandlers() {
		struct sigaction sa;
		memset(&sa, 0, sizeof(struct sigaction));
		sa.sa_handler = &onSignal;
		sigaction(SIGQUIT, &sa, 0);
		sigaction(SIGTERM, &sa, 0);
		sigaction(SIGINT,  &sa, 0);
	}
#else
	void initSignalHandlers() {}
#endif
int main(int argc, char *argv[]) {
	initSignalHandlers();
	boost::filesystem::path::default_name_check(boost::filesystem::native);

	std::string path;
	if (argc == 1) {
		logMsg("Please provide path to files as argument.");
		return -1;
	}
	HashWorkTester ht(argv[1]);
	EventMain::instance().mainLoop();
	logMsg(
		_F("Hasher: %s hashed in %fs (%s/s)")
		% Utils::bytesToString(HashWork::getHashed())
		% HashWork::getTime()
		% Utils::bytesToString(
			static_cast<uint64_t>(
				HashWork::getHashed()/HashWork::getTime()
			)
		)
	);
	return 0;
}

#endif
