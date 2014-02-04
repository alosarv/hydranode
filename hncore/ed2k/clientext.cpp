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
 * \file clientext.cpp Implementation of Client class extensions
 */

#include <hncore/ed2k/clientext.h>
#include <hncore/ed2k/clients.h>
#include <hncore/ed2k/zutils.h>
#include <hncore/partdata.h>
#include <hncore/sharedfile.h>
#include <hncore/metadb.h>
#include <hncore/metadata.h>
#include <hnbase/ssocket.h>

namespace Donkey {

namespace Detail {
const std::string TRACE_CLIENT = "ed2k.client";

/**
 * How much data to send to a single client during an upload session.
 *
 * eMule sends 9500kb by default, but when "upload full chunks" setting is
 * enabled, it uploads full 180k chunks, which means slightly more, namely
 * 53 * ED2K_CHUNKSIZE (9540kb). Current Hydranode default behaviour should be
 * to upload full chunks, hence 9540kb.
 *
 * If you want faster Queue rotation, lowering this value would give the
 * desired result. It is recommended to keep this value at least in
 * n*ED2K_CHUNKSIZE, to promote full chunks sending.
 */
const uint32_t SEND_TO_ONE_CLIENT = 53 * ED2K_CHUNKSIZE;

//! \name Object counters
//!@{
size_t s_queueInfoCnt = 0;
size_t s_uploadInfoCnt = 0;
size_t s_downloadInfoCnt = 0;
size_t s_sourceInfoCnt = 0;
//!@}

// QueueInfo class
// ---------------
QueueInfo::QueueInfo(Client *parent, SharedFile *req, const Hash<ED2KHash> &h)
: ClientExtBase(parent), m_queueRanking(), m_reqFile(req), m_reqHash(h),
m_waitStartTime(Utils::getTick()), m_lastQueueReask() {
	++s_queueInfoCnt;
	m_parent->setRequest(m_reqFile);
}

QueueInfo::QueueInfo(Client *parent, UploadInfoPtr nfo) : ClientExtBase(parent),
m_queueRanking(), m_waitStartTime(Utils::getTick()), m_lastQueueReask() {
	CHECK_THROW(nfo);
	m_reqFile = nfo->getReqFile();
	m_reqHash = nfo->getReqHash();
	++s_queueInfoCnt;
	m_parent->setRequest(m_reqFile);
}

QueueInfo::~QueueInfo() {
	--s_queueInfoCnt;
	if (!m_parent->getUploadInfo()) {
		m_parent->setRequest(0);
	}
}

size_t QueueInfo::count() {
	return s_queueInfoCnt;
}

// UploadInfo class
// ------------------
UploadInfo::UploadInfo(Client *parent, QueueInfoPtr nfo)
: ClientExtBase(parent),  m_curPos(), m_endPos(), m_compressed(false),
m_sent() {
	CHECK_THROW(nfo);
	m_reqFile = nfo->getReqFile();
	m_reqHash = nfo->getReqHash();
	CHECK_THROW(m_reqFile);
	CHECK_THROW(m_reqHash);
	CHECK_THROW(MetaDb::instance().findSharedFile(m_reqHash) == m_reqFile);
	++s_uploadInfoCnt;
	m_parent->setUploading(true, m_reqFile);
}

// Dummy destructor
UploadInfo::~UploadInfo() {
	logTrace(TRACE_CLIENT,
		boost::format("[%s] UploadSessionEnd: Total sent: %d bytes")
		% m_parent->getIpPort() % m_sent
	);
	--s_uploadInfoCnt;
	m_speeder.disconnect();
	m_parent->setUploading(false, m_reqFile);
}

size_t UploadInfo::count() {
	return s_uploadInfoCnt;
}

void UploadInfo::connectSpeeder() {
	CHECK_THROW(m_parent->getSocket());
	m_speeder = m_reqFile->getUpSpeed.connect(
		boost::bind(
			&ED2KClientSocket::getUpSpeed, m_parent->getSocket()
		)
	);
}

// buffer first data chunk into memory
void UploadInfo::bufferData() {
	CHECK_THROW(m_reqFile);
	CHECK_THROW(m_reqChunks.size());

	m_curPos = m_reqChunks.front().begin();
	m_endPos = m_reqChunks.front().end();
	m_buffer = m_reqFile->read(m_curPos, m_endPos);

	logTrace(TRACE_CLIENT,
		boost::format("[%s] Buffering upload data (%d..%d, size=%d)") %
		m_parent->getIpPort() % m_curPos % m_endPos % m_buffer.size()
	);
	CHECK_THROW_MSG(m_buffer.size(), "UploadBuffering failed!");
}

// Compress the upload buffer
bool UploadInfo::compress() {
	CHECK_THROW(m_buffer.size());

	Utils::StopWatch s;
	std::string tmp = Zlib::compress(m_buffer);
	logTrace(TRACE_CLIENT, boost::format("Data compression took %dms.") %s);

	if (tmp.size() < m_buffer.size()) {
		m_buffer = tmp;
		m_compressed = true;
		m_endPos = m_buffer.size();
		return true;
	} else {
		m_compressed = false;
		return false;
	}
}

// retrieve next chunks
boost::tuple<uint32_t, uint32_t, std::string>
UploadInfo::getNext(uint32_t amount) {
	if (m_buffer.size() < amount) {
		amount = m_buffer.size();
	}

	boost::tuple<uint32_t, uint32_t, std::string> ret;
	ret.get<0>() = m_curPos;
	if (m_compressed) {
		ret.get<1>() = m_reqChunks.front().length();
	} else {
		ret.get<1>() = m_curPos + amount;
	}
	ret.get<2>() = m_buffer.substr(0, amount);

	m_buffer.erase(0, amount);
	m_curPos += amount;

	if (m_buffer.size() == 0 && m_reqChunks.size()) {
		m_reqChunks.pop_front();
	}

	m_sent += amount; // update sent count

	return ret;
}

// Add a new requested chunk (but only if we don't have this in our list
// already).
//
// Since we only want to send 9.28mb to each client at a time, requests for
// chunks that would exceed that amount will be denied. The result is that
// in reality, we, in worst-case scenario, 9710001 bytes to a client instead of
// full 9728000 chunk.
void UploadInfo::addReqChunk(Range32 r) {
	std::list<Range32>::iterator i = m_reqChunks.begin();

	uint32_t requested = 0;
	while (i != m_reqChunks.end()) {
		if ((*i) == r) {
			return; // already here
		}
		requested += (*i++).length();
	}

	if (m_sent + requested + r.length() <= SEND_TO_ONE_CLIENT) {
		logTrace(TRACE_CLIENT,
			boost::format("Adding chunk request %d..%d")
			% r.begin() % r.end()
		);
		m_reqChunks.push_back(r);
	}
}

// SourceInfo class
// ----------------
SourceInfo::SourceInfo(Client *parent, Download *file) :
ClientExtBase(parent), m_reqFile(file), m_qr(), m_needParts(), m_lastSrcExch() {
	CHECK_THROW(parent);
	CHECK_THROW(file);
	addOffered(file);
	++s_sourceInfoCnt;
}

SourceInfo::~SourceInfo() {
	typedef std::set<Download*>::iterator Iter;
	for (Iter i = m_offered.begin(); i != m_offered.end(); ++i) {
		if (!DownloadList::instance().valid(*i)) {
			continue;
		}
		PartData *pd = (*i)->getPartData();
		if (pd == m_reqFile->getPartData() && m_partMap) {
			try {
				pd->delSourceMask(ED2K_PARTSIZE, *m_partMap);
			} catch (std::exception &e) {
				logDebug(
					boost::format("[%s] ~SourceInfo: %s")
					% m_parent->getIpPort() % e.what()
				);
			}
		}
		(*i)->delSource(m_parent);
		dynamic_cast<BaseClient*>(m_parent)->remOffered(pd);
	}
	--s_sourceInfoCnt;
	std::map<MetaData*, std::string>::iterator it(m_fileNamesAdded.begin());
	while (it != m_fileNamesAdded.end()) {
		(*it).first->delFileName((*it).second);
		++it;
	}
}

size_t SourceInfo::count() {
	return s_sourceInfoCnt;
}

void SourceInfo::setPartMap(const std::vector<bool> &partMap) {
	CHECK_THROW(m_reqFile);
	CHECK_THROW(m_reqFile->getPartData());

	if (m_partMap) {
		m_reqFile->getPartData()->delSourceMask(
			ED2K_PARTSIZE, *m_partMap
		);
		*m_partMap = partMap;
	} else {
		m_partMap.reset(new std::vector<bool>(partMap));
	}

	m_reqFile->getPartData()->addSourceMask(ED2K_PARTSIZE, partMap);
#ifndef NDEBUG
	std::string tmp;
	for (uint32_t i = 0; i < m_partMap->size(); ++i) {
		tmp += ((*m_partMap)[i] ? "1" : "0");
	}
	if (!tmp.empty()) {
		logTrace(
			TRACE_CHUNKS,
			boost::format("[%s] eDonkey2000 chunkmap: %s")
			% m_parent->getIpPort() % tmp
		);
	} else {
		logTrace(
			TRACE_CHUNKS,
			boost::format("[%s] Is full source.")
			% m_parent->getIpPort()
		);
	}
#endif
	checkNeedParts();
}

void SourceInfo::swapToLowest() {
	CHECK_THROW(m_reqFile);

	Download *d = m_reqFile;
	typedef std::set<Download*>::iterator Iter;
	for (Iter i = m_offered.begin(); i != m_offered.end(); ++i) {
		if (!DownloadList::instance().valid(*i)) {
			continue;
		}
		if ((*i)->getSourceCount() + 1 < d->getSourceCount()) {
			d = *i;
		}
	}
	if (d != m_reqFile) {
		setReqFile(d);
	}
}

void SourceInfo::setReqFile(Download *file) {
	CHECK_THROW(file);
	CHECK_THROW(offers(file));

	if (m_reqFile && file != m_reqFile) {
		if (m_partMap) {
			m_reqFile->getPartData()->delSourceMask(
				ED2K_PARTSIZE, *m_partMap
			);
			m_partMap.reset();
		}
		m_reqFile->delSource(m_parent);
		file->addSource(m_parent);
		m_parent->setSource(file->getPartData());
	}
	m_reqFile = file;
	checkNeedParts();
}

void SourceInfo::addOffered(Download *file) {
	CHECK_THROW(file);

	if (m_offered.insert(file).second) {
		file->addSource(m_parent);
	}
	if (!m_reqFile) {
		setReqFile(file);
	}

	dynamic_cast<BaseClient*>(m_parent)->addOffered(file->getPartData());
}

void SourceInfo::remOffered(Download *file, bool cleanUp) {
	CHECK_THROW(file);

	typedef std::set<Download*>::iterator Iter;
	Iter it = m_offered.find(file);
	if (it == m_offered.end()) {
		return;
	}

	m_offered.erase(it);
	file->delSource(m_parent);
	dynamic_cast<BaseClient*>(m_parent)->remOffered(file->getPartData());

	if (file == m_reqFile) {
		if (m_partMap && cleanUp) {
			m_reqFile->getPartData()->delSourceMask(
				ED2K_PARTSIZE, *m_partMap
			);
		}

		m_partMap.reset();
		m_needParts = false;

		Download *switchTo = 0;
		for (it = m_offered.begin(); it != m_offered.end(); ++it) {
			if (DownloadList::instance().valid(*it)) {
				switchTo = *it;
				break;
			}
		}
		if (switchTo) {
			logTrace(TRACE_CLIENT,
				boost::format(
					"[%s] Switching source to other file."
				) % m_parent->getIpPort()
			);
			setReqFile(*it);
			swapToLowest();
		} else {
			m_reqFile = 0;
		}
	}
}

void SourceInfo::checkNeedParts() {
	if (m_partMap) {
		m_needParts = false;
		if (m_partMap->empty()) {
			m_needParts = true;
		} else try {
			m_reqFile->getPartData()->getRange(
				ED2K_PARTSIZE, *m_partMap
			);
			m_needParts = true;
		} catch (...) {}
	} else {
		// set true for now - we'll get an update once we get partmap
		m_needParts = true;
	}
}

void SourceInfo::addFileName(MetaData *md, const std::string &name) {
	if (m_fileNamesAdded[md].size()) {
		md->delFileName(m_fileNamesAdded[md]);
	}
	md->addFileName(name);
	m_fileNamesAdded[md] = name;
}

// DownloadInfo class
// ------------------
DownloadInfo::DownloadInfo(Client *parent, PartData *pd, PartMapPtr partMap)
: ClientExtBase(parent), m_reqPD(pd), m_partMap(partMap), m_packedBegin(),
m_received() {
	CHECK_THROW(pd);
	CHECK_THROW(partMap);
	++s_downloadInfoCnt;
	m_speeder = pd->getDownSpeed.connect(
		boost::bind(
			&ED2KClientSocket::getDownSpeed, m_parent->getSocket()
		)
	);
	m_parent->setDownloading(true, m_reqPD);
}

DownloadInfo::~DownloadInfo() {
	if (m_packedBuffer.size() && m_reqPD && m_reqChunks.size()) try {
		logTrace(TRACE_CLIENT,
			boost::format(
				"[%s] DownloadInfo: LastMinute packed "
				"data flushing."
			) % m_parent->getIpPort()
		);

		std::string tmp(Zlib::decompress(m_packedBuffer));
		uint32_t end = m_packedBegin + tmp.size() - 1;
		write(Range32(m_packedBegin, end), tmp);
	} catch (std::exception &) {
		logTrace(TRACE_CLIENT,
			boost::format(
				"[%s] DownloadInfo: LastMinute flushing failed."
			) % m_parent->getIpPort()
		);
	}
	logTrace(TRACE_CLIENT,
		boost::format(
			"[%s] DownloadSessionEnd: Total received: %d bytes"
		) % m_parent->getIpPort() % m_received
	);
	--s_downloadInfoCnt;
	m_speeder.disconnect();
	m_parent->setDownloading(false, m_reqPD);
}

size_t DownloadInfo::count() {
	return s_downloadInfoCnt;
}

std::list<Range32> DownloadInfo::getChunkReqs() try {
	CHECK_THROW(m_reqPD);

	if (!m_curPart) {
		m_curPart = m_reqPD->getRange(ED2K_PARTSIZE, *m_partMap);
		if (!m_curPart) {
			logTrace(TRACE_CLIENT,
				boost::format(
					"[%s] DownloadInfo: Client has no "
					"needed parts or all chunks are "
					"already locked."
				) % m_parent->getIpPort()
			);
			throw std::runtime_error("No more needed parts");
		}
	}
	while (m_reqChunks.size() < 3) {
		::Detail::LockedRangePtr lock(
			m_curPart->getLock(ED2K_CHUNKSIZE)
		);
		if (!lock) {
			m_curPart.reset();
			return getChunkReqs();
		}
		m_reqChunks.push_back(lock);
	}
	std::list<Range32> tmp;
	for (Iter i = m_reqChunks.begin(); i != m_reqChunks.end(); ++i) {
		tmp.push_back(*(*i));
	}
	return tmp;
} catch (std::exception&) {
	if (m_reqChunks.size()) {
		std::list<Range32> tmp;
		for (Iter i = m_reqChunks.begin(); i != m_reqChunks.end(); ++i){
			tmp.push_back(*(*i));
		}
		return tmp;
	} else {
		throw;
	}
}
MSVC_ONLY(;)

bool DownloadInfo::write(Range32 r, const std::string &data) try {
	CHECK_THROW(m_reqPD);
	CHECK_THROW(m_reqChunks.size());
	CHECK(data.size());

	if (data.size() == 0) {
		return false; // nothing to do
	}

	Iter i = m_reqChunks.begin();
	for (; i != m_reqChunks.end() && !(*i)->contains(r); ++i);
	CHECK_THROW(i != m_reqChunks.end());

	(*i)->write(r.begin(), data);
	m_received += data.size();

	if ((*i)->isComplete()) {
		logTrace(TRACE_CLIENT,
			boost::format("[%s] Completed chunk %d..%d")
			% m_parent->getIpPort() % (*i)->begin() % (*i)->end()
		);
		m_reqChunks.erase(i);
		return true;
	}

	return false;
} catch (std::exception &e) {
	logDebug(
		boost::format("[%s] Writing data: %s")
		% m_parent->getIpPort() % e.what()
	);
	return false;
}
MSVC_ONLY(;)

bool DownloadInfo::writePacked(
	uint32_t begin, uint32_t size, const std::string &data
) {
	CHECK_THROW(m_reqPD);
	CHECK_THROW(m_reqChunks.size());

	m_packedBegin = begin;
	m_packedBuffer.append(data);

	if (m_packedBuffer.size() == size) {
		std::string tmp(Zlib::decompress(m_packedBuffer));
		logTrace(TRACE_CLIENT,
			boost::format("[%s] Completed packed chunk %d..%d")
			% m_parent->getIpPort() % begin
			% (begin + tmp.size() - 1)
		);

		if (tmp.size() > m_packedBuffer.size()) {
			logTrace(TRACE_CLIENT, boost::format(
				"[%s] Saved %d bytes by download compression."
			) % m_parent->getIpPort() %
			(tmp.size() - m_packedBuffer.size()));
		}

		return write(Range32(begin, begin + tmp.size() - 1), tmp);
	}

	return false;
}

} // namespace detail
} // end namespace Donkey
