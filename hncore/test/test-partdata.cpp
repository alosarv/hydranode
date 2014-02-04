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
 * \file test-partdata.cpp Simulation and regress-test for PartData class
 */

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define TEST_PARTDATA // enables some friend declarations in partdata.h

#include <hncore/partdata.h>
#include <hncore/hasher.h>
#include <hnbase/log.h>
#include <hncore/metadata.h>

uint32_t file_size(50*1024*1024);
uint32_t chunk_size(9500*1024);

void writeRange(Detail::UsedRangePtr ur) {
	logDebug(boost::format("Writing range %d..%d") % ur->begin() % ur->end());
	while (!ur->isComplete()) try {
		Detail::LockedRangePtr lr = ur->getLock(chunk_size/4);
		lr->write(lr->begin(), std::string(lr->length(), '#'));
	} catch (std::runtime_error &e) {
		logError(boost::format("what(): %s") % e.what());
	}
}

void onFileEvent(PartData *, int evt) {
	if (evt == PD_COMPLETE) {
		EventMain::instance().exitMainLoop();
	}
}

void onHashEvent(HashWorkPtr p, HashEvent) {
	PartData *pd = new PartData(file_size, "test.tmp", "final.file");
	pd->setMetaData(p->getMetaData());
	for (uint32_t i = 0; i < p->getMetaData()->getHashSetCount(); ++i) {
		HashSetBase *hs = p->getMetaData()->getHashSet(i);
		logMsg(boost::format(
			" -> HashSet: FileHash: %s ChunkHash: %s ChunkSize: %s"
		) % hs->getFileHashType() % hs->getChunkHashType()
		% hs->getChunkSize());
		logMsg(boost::format("FileHash: %s") % hs->getFileHash().decode());
		boost::format fmt("ChunkHash[%d]: %s");
		for (uint32_t j = 0; j < hs->getChunkCnt(); ++j) {
			logMsg(fmt % j % (*hs)[j].decode());
		}
	}
	PartData::getEventTable().addHandler(pd, &onFileEvent);
	std::vector<bool> mask(6);
	mask[0]=1;mask[1]=0;mask[2]=0;mask[3]=1;mask[4]=1;mask[5]=0;
	pd->addSourceMask(chunk_size, mask);
	mask[0]=0;mask[1]=0;mask[2]=1;mask[3]=1;mask[4]=0;mask[5]=1;
	pd->addSourceMask(chunk_size, mask);
	mask[0]=0;mask[1]=1;mask[2]=1;mask[3]=1;mask[4]=0;mask[5]=1;
	pd->addSourceMask(chunk_size, mask);

	mask[0]=0;mask[1]=1;mask[2]=0;mask[3]=1;mask[4]=0;mask[5]=0;
	CHECK_THROW(pd->getRange(chunk_size, mask)->begin() == 9728000);
	pd->write(1235, std::string(100, '#'));
	mask[0]=1;
	CHECK_THROW(pd->getRange(chunk_size, mask)->begin() == 0);

	while (!pd->isComplete()) {
		writeRange(pd->getRange(chunk_size));
		pd->printCompleted();
	}
}

int main(int, char*[]) {
	std::ofstream ofs("test.ref", std::ios::out);
	std::string s(file_size/10, '#');
	for(uint32_t i = 0; i < 10; ++i) {
		ofs.write(s.c_str(), s.size());
	}
	ofs.flush();
	ofs.close();
	HashWorkPtr p(new HashWork("test.ref"));
	HashWork::getEventTable().addHandler(p, &onHashEvent);
	WorkThread::instance().postWork(p);
	EventMain::instance().mainLoop();
	return 0;
}

#endif
