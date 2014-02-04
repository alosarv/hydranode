/*
 *  Copyright (C) 2005-2006 Alo Sarv <madcat_@users.sourceforge.net>
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
 * \file files.cpp     Implementation of TorrentFile and PartialTorrent classes
 */

#include <hncore/bt/files.h>
#include <hnbase/timed_callback.h>
#include <hncore/metadata.h>
#include <boost/filesystem/operations.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <fcntl.h>
#include <bitset>
#include <errno.h>

namespace Bt {

// TorrentFile class
// -----------------
TorrentFile::InternalFile::InternalFile(
	uint64_t begin, uint64_t size, SharedFile *f
) : Range64(begin, begin + size - 1), m_file(f) {}

TorrentFile::TorrentFile(
	const std::map<uint64_t, SharedFile*> &files, TorrentInfo ti,
	PartialTorrent *pt
) {
	CHECK_THROW(files.size());
	typedef std::map<uint64_t, SharedFile*>::const_iterator FIter;

	if (pt) {
		setPartData(pt);
	}

	for (FIter it = files.begin(); it != files.end(); ++it) {
		if (!(*it).second->getSize()) {
			(*it).second->destroy();
			continue;
		}

		InternalFile f(
			(*it).first, (*it).second->getSize(), (*it).second
		);
		m_children.push(f);
		if (pt && (*it).second->isComplete()) {
			logDebug(
				boost::format(
					"Marking range %d..%d as complete."
				) % f.begin() % f.end()
			);
			pt->setComplete(f);
		}
		m_childrenReverse[(*it).second] = f.begin();
		SharedFile::getEventTable().addHandler(
			(*it).second, this, &TorrentFile::onSharedFileEvent
		);
		getUpSpeed.connect((*it).second->getUpSpeed);
		(*it).second->setParent(this);
	}
	m_size = ti.getSize();
	getEventTable().postEvent(this, SF_ADDED);
}

// since we use custom MetaData, which is not recorded in MetaDb, we delete it
// ourselves
TorrentFile::~TorrentFile() {
	delete getMetaData();

	// reparent all children to null, otherwise Object deletes them
	// note that the Object::CIter's become invalid after setParent()
	// call, thus we have to re-initialize it every time
	for (Object::CIter i = begin(); i != end(); i = begin()) {
		(*i).second->setParent(0);
	}
}

std::string TorrentFile::read(uint64_t begin, uint64_t end) {
	RangeList<InternalFile>::CIter i = m_children.getContains(begin, end);
	CHECK_THROW(i != m_children.end());

	std::string data;
	uint64_t tmp = begin - (*i).begin();      // first relative offset
	if ((*i).containsFull(begin, end)) {
		data = i->m_file->read(tmp, end - (*i).begin());
	} else while (i != m_children.end() && (*i).contains(begin, end)) {
		if (end <= (*i).end()) {          // data ends in this file
			data += i->m_file->read(tmp, end - (*i).begin());
		} else {                          // data crosses this file
			data += i->m_file->read(tmp, i->m_file->getSize() - 1);
		}
		tmp = 0;
		++i;
	}

	CHECK_THROW(data.size() == end - begin + 1);

	return data;
}

void TorrentFile::onSharedFileEvent(SharedFile *file, int evt) {
	if (evt == SF_DESTROY && m_children.size()) {
		RIter i = m_childrenReverse.find(file);
		CHECK_RET(i != m_childrenReverse.end());
		RangeList<InternalFile>::CIter j = m_children.getContains(
			(*i).second, (*i).second + file->getSize() - 1
		);
		CHECK_RET(j != m_children.end());
		m_childrenReverse.erase(i);
		m_children.erase(*j);
		if (!m_children.size()) {
			destroy();
		}
	}
}

SharedFile* TorrentFile::getContains(Range64 range) const {
	RangeList<InternalFile>::CIter it = m_children.getContains(range);
	CHECK_THROW(it != m_children.end());
	return (*it).m_file;
}

void TorrentFile::finishDownload() {
	PartData::getEventTable().postEvent(getPartData(), PD_DL_FINISHED);
}

// PartialTorrent class
// --------------------
PartialTorrent::CacheImpl::CacheImpl(
	uint64_t begin, uint64_t end, const boost::filesystem::path &p
) : Range64(begin, end), m_loc(p) {
	if (!boost::filesystem::exists(p)) {
		std::ofstream o(p.native_file_string().c_str());
		o.put('\0');
	}
}

// this expects relative offset, inside this cachefile
//
// the buffer bookeeping is similar as is done inside PartData - buffered data
// is in a map, keyed by begin offset, so disk writes occour in a begin->end
// fashion, avoiding backwards seeking.
void PartialTorrent::CacheImpl::write(uint64_t begin, const std::string &data) {
	m_buffer[begin] = data;
}

// again, very similar to what is done inside PartData::save method, we flush
// the data to disk. The usage of fcntl API instead of C++ iostreams is since
// GCC versions 3.2 through 3.4 lack 64bit IO support.
void PartialTorrent::CacheImpl::save() {
	if (!m_buffer.size()) {
		return;
	}

	int fd = open(m_loc.native_file_string().c_str(), O_RDWR|O_BINARY);
	if (!fd) {
		logError(
			boost::format("Failed to open cache file '%s': %s")
			% m_loc.native_file_string() % strerror(errno)
		);
		return;
	}

	for (BIter i = m_buffer.begin(); i != m_buffer.end(); ++i) try {
		uint64_t ret = lseek64(fd, (*i).first, SEEK_SET);
		CHECK_THROW(ret == static_cast<uint64_t>((*i).first));
		int c = ::write(fd, (*i).second.data(), (*i).second.size());
		CHECK_THROW(c == static_cast<int>((*i).second.size()));
	} catch (...) {
		close(fd);
		throw std::runtime_error(
			"no space left on drive (torrent cache failure)"
		);
	}
	fsync(fd);
	close(fd);
	m_buffer.clear();
}

PartialTorrent::InternalFile::InternalFile(
	uint64_t begin, uint64_t size, PartData *file
) : Range64(begin, begin + size - 1), m_file(file) {}

// constructs a PartialTorrent; input must contain ALL files within this torrent
//
// the InternalFile and m_children / m_childrenReverse machinery should be
// rather self-explanatory. However, where it gets interesting is the cache
// files mechanism.
//
// Each chunk that crosses file boundaries must be cached. Each file within a
// cached chunk must be a separate physical file, in order to allow our custom
// hasher to work on cache or real files w/o any changes in code. Hence, the
// base logic is for each file beginning and ending, we have a cache-file which
// contains the two halves of the crossing chunk. It gets more complex though -
// we don't cache if the chunks end exactly at file boundaries; we don't want
// cache for the very first (and very last) chunks, since those can never be
// crossing files. On the other hand, they can cross chunks, if the first (or
// last) files are smaller than chunksize. Furthermore, a chunk can cross
// multiple files, that are smaller than the chunk size.
PartialTorrent::PartialTorrent(
	const std::map<uint64_t, PartData*> &files,
	const boost::filesystem::path &loc,
	uint64_t size
) : m_writing() {
	using namespace boost::filesystem;
	typedef std::map<uint64_t, PartData*>::const_iterator FIter;
	CHECK_THROW(files.size());
	m_loc = loc;

	uint64_t i = 0;
	for (FIter it = files.begin(); it != files.end(); ++it, ++i) {
		if (!(*it).second->getSize()) { // file with size zero
			continue;
		}
		uint64_t offset = (*it).first;
		if (it == files.begin()) {
			if (offset > 0) {
				dontDownload(Range64(0, offset - 1));
			}
		} else if (m_children.back().end() + 1 != offset) {
			Range64 r(m_children.back().end() + 1, offset - 1);
			dontDownload(r);
		}
		InternalFile f(offset, (*it).second->getSize(), (*it).second);
		m_children.push(f);
		m_childrenReverse[(*it).second] = offset;
		RangeList64 cr = (*it).second->getCompletedRanges();
		for (RangeList64::CIter j = cr.begin(); j != cr.end(); ++j) {
			Range64 tmp(offset + (*j).begin(), offset + (*j).end());
			setComplete(tmp);
		}
		RangeList64 vr = (*it).second->getVerifiedRanges();
		for (RangeList64::CIter j = vr.begin(); j != vr.end(); ++j) {
			Range64 tmp(offset + (*j).begin(), offset + (*j).end());
			setVerified(tmp);
		}

		// set up inter-object communication
		(*it).second->dataAdded.connect(
			boost::bind(
				&PartialTorrent::childDataAdded,
				this, _1, _2, _3
			)
		);
		(*it).second->onCorruption.connect(
			boost::bind(
				&PartialTorrent::childCorruption, this, _1, _2
			)
		);
		(*it).second->canComplete.connect(
			boost::bind(&PartialTorrent::childCanComplete, this, _1)
		);
		(*it).second->onAllocDone.connect(
			boost::bind(&PartialTorrent::childAllocDone, this, _1)
		);
		(*it).second->onPaused.connect(
			boost::bind(&PartialTorrent::childPaused, this, _1)
		);
		(*it).second->onStopped.connect(
			boost::bind(&PartialTorrent::childPaused, this, _1)
		);
		(*it).second->onCanceled.connect(
			boost::bind(&PartialTorrent::childPaused, this, _1)
		);
		(*it).second->onResumed.connect(
			boost::bind(&PartialTorrent::childResumed, this, _1)
		);
		(*it).second->onDestroyed.connect(
			boost::bind(&PartialTorrent::childDestroyed, this, _1)
		);
		(*it).second->onVerified.connect(
			boost::bind(
				&PartialTorrent::childChunkVerified,
				this, _1, _2, _3
			)
		);
		getDownSpeed.connect((*it).second->getDownSpeed);
		(*it).second->setParent(this);
	}
	if (m_children.back().end() < m_size - 1) {
		dontDownload(Range64(m_children.back().end() + 1, m_size - 1));
	}

	m_size = size;
	onVerified.connect(
		boost::bind(
			&PartialTorrent::parentChunkVerified, this, _1, _2, _3
		)
	);
	getEventTable().postEvent(this, PD_ADDED);
}

PartialTorrent::~PartialTorrent() {
	// reparent all children to null, otherwise Object deletes them
	// note that the Object::CIter's become invalid after setParent()
	// call, thus we have to re-initialize it every time
	for (Object::CIter i = begin(); i != end(); i = begin()) {
		(*i).second->setParent(0);
	}
}

// this machinery creates cache for all chunks that cross file
// boundaries
// note: chunk variable must also be 64bit, otherwise compiler will do some
//       calculations at 32bit precision only.
void PartialTorrent::initCache(const TorrentInfo &info) {
	using namespace boost::filesystem;

	std::vector<TorrentInfo::TorrentFile> files = info.getFiles();
	uint64_t offset = 0;
	uint64_t cacheSize = 0;
	uint64_t chunkSize = info.getChunkSize();
	boost::format cname("%s.cache.%d.%d");
	std::map<uint64_t, uint32_t> chunkNumbers;

	for (uint32_t i = 0; i < files.size(); ++i) {
		if (!files[i].getSize()) {
			continue;
		}

		// file beginning
		uint64_t chunk = offset / chunkSize;
		Range64 f(offset, offset + files[i].getSize() - 1);
		Range64 cr(chunk * chunkSize, (chunk + 1) * chunkSize - 1);
		offset += files[i].getSize();

		if (m_children.contains(cr) && !f.containsFull(cr)) {
			cname % getLocation().native_file_string() % chunk;
			cname % chunkNumbers[chunk]++;
			path cacheFile(cname.str(), native);

			if (f.contains(cr.begin())) {      // chunk begins here
				CacheFile cf(cr.begin(), f.end(), cacheFile);
				m_cache.push(cf);
				doDownload(cf);
				cacheSize += cf.length();
			} else if (f.contains(cr.end())) {  // chunk ends here
				CacheFile cf(f.begin(), cr.end(), cacheFile);
				m_cache.push(cf);
				doDownload(cf);
				cacheSize += cf.length();
			} else {                            // chunk passes this
				CacheFile cf(f.begin(), f.end(), cacheFile);
				m_cache.push(cf);
				doDownload(cf);
				cacheSize += cf.length();
			}
		}
		// file ending
		chunk = (offset - 1) / chunkSize;
		cr = Range64(chunk * chunkSize, (chunk + 1) * chunkSize - 1);
		if (m_children.contains(cr) && !f.containsFull(cr)) {
			cname % getLocation().native_file_string() % chunk;
			cname % chunkNumbers[chunk]++;
			path cacheFile(cname.str(), native);

			if (f.contains(cr.begin())) {      // chunk begins here
				CacheFile cf(cr.begin(), f.end(), cacheFile);
				m_cache.push(cf);
				doDownload(cf);
				cacheSize += cf.length();
			} else if (f.contains(cr.end())) {  // chunk ends here
				CacheFile cf(f.begin(), cr.end(), cacheFile);
				m_cache.push(cf);
				doDownload(cf);
				cacheSize += cf.length();
			} else {                            // chunk passes this
				CacheFile cf(f.begin(), f.end(), cacheFile);
				m_cache.push(cf);
				doDownload(cf);
				cacheSize += cf.length();
			}
		}
	}
	if (!m_name.size()) {
		m_name = info.getName();
	}

	logTrace("bt.files",
		boost::format("Torrent size: %s Cache size: %s (%.2f%%)")
		% Utils::bytesToString(m_size) % Utils::bytesToString(cacheSize)
		% (cacheSize * 100 / m_size)
	);
}

PartData* PartialTorrent::getContains(Range64 range) const {
	RangeList<InternalFile>::CIter it = m_children.getContains(range);
	CHECK_THROW(it != m_children.end());
	return (*it).m_file;
}

// Forwards the data one or more concrete sub-objects
//
// The most tricky part here is figuring out the relative offset (within the
// file where the data has to be written to), from the global offset that is
// given as input BEGIN parameter. We do that by substracting the containing
// file's global begin offset from the input begin offset, and thus get the
// relative offset.
//
// We must consider that we don't have physical files within the written data
// region, either beginning, middle or end; this can happen when downloads were
// canceled and such. Thus, we must calculatate the position and offset for
// EACH file separately, since we might need to skip over few files in the
// middle.
//
// Note that we must update cache BEFORE starting normal write operations, since
// normal write operations can trigger chunk verification, leading to
// verifyRange being called, but there we already need cache to be written.
void PartialTorrent::doWrite(uint64_t begin, const std::string &data) {
	updateCache(begin, data);

	Range64 toWrite(begin, begin + data.size() - 1);
	RangeList<InternalFile>::CIter i = m_children.getContains(toWrite);

	m_writing = true;
	while (i != m_children.end() && (*i).contains(toWrite)) {
		uint64_t pos = 0;
		uint64_t offset = 0;
		if ((*i).begin() < begin) {
			offset = begin - (*i).begin();
		} else if ((*i).begin() > begin) {
			pos += (*i).begin() - begin;
		}

		uint64_t tmp = (*i).m_file->getSize() - offset;
		if (data.size() - pos < (*i).m_file->getSize() - offset) {
			tmp = data.size() - pos;
		}
		(*i++).m_file->write(offset, data.substr(pos, tmp));
	}
	m_writing = false;

	setComplete(Range64(begin, begin + data.size() - 1));
}

// Updates the cache
//
// the tricky part here is that CacheImpl::write expects RELATIVE offset inside
// the cache file, while the input BEGIN variable for this function is the
// global offset inside the entire torrent. Furthermore, we must consider the
// situation where only part of the data needs to be written into the cache.
//
// The first relative offset can be zero (if begin == cachefile beginning), but
// usually is non-zero, the difference between input begin offset and the
// cachefile's begin offset (just as in doWrite() method).All following offsets
// are zeroes, since the data is continuous.
//
// We must also keep track of the position within the data to be written. We
// default to zero, however if the cachefile global begin offset is larger than
// the input BEGIN offset, we adjust the position in the data accordingly, and
// start writing from a later position in the input data.
//
// Inside the while loop, we also must keep track of how long each of the cache
// files is, since the input data may cross multiple file boundaries. So, in
// order to write to each cache file only as much was supposed to be written
// there, we check if the to-be-written length of the data (variable named TMP)
// would flow over the end of the the cachefile. If that's the case, the TMP
// variable is adjusted to only write until the end of that specific file, and
// the loop continues to next file.
//
// We can also do early return in this function if none of the data needs to
// be cached.
void PartialTorrent::updateCache(uint64_t begin, const std::string &data) {
	Range64 toWrite(begin, begin + data.size() - 1);
	if (!m_cache.contains(toWrite)) {
		return; // region is not cached
	}
	RangeList<CacheFile>::CIter i = m_cache.getContains(toWrite);
	CHECK_RET(i != m_cache.end());

	uint64_t pos = 0;
	uint64_t offset = 0;
	if ((*i).begin() < begin) {
		offset = begin - (*i).begin();
	} else if ((*i).begin() > begin) {
		pos += (*i).begin() - begin + 1;
	}
	while (i != m_cache.end() && (*i).contains(toWrite)) {
		uint64_t tmp = (*i).length() - offset;
		if (data.size() - pos < (*i).length() - offset) {
			tmp = data.size() - pos;
		}
		(*i++).m_impl->write(offset, data.substr(pos, tmp));
		pos += tmp;
		offset = 0;
	}
}

void PartialTorrent::childDataAdded(
	PartData *file, uint64_t offset, uint32_t amount
) {
	if (!m_writing) {
		RIter i = m_childrenReverse.find(file);
		CHECK_RET(i != m_childrenReverse.end());
		CHECK_RET((*i).first == file);

		setComplete(
			Range64(
				offset + (*i).second,
				offset + (*i).second + amount - 1
			)
		);
	}
}

HashWorkPtr PartialTorrent::verifyRange(
	Range64 range, const HashBase *ref, bool doSave
) {
	boost::intrusive_ptr<TorrentHasher> c, cc;
	std::set<PartData*> waitAlloc;
	std::vector<boost::filesystem::path> files;
	std::pair<uint64_t, uint64_t> relativeOffsets;
	relativeOffsets = std::make_pair(range.begin(), range.end());

	RangeList<InternalFile>::CIter i = m_children.getContains(range);

	// can happen on some circumstances; technically, we could still try
	// to verify cache here, but what's the point really?
	if (i == m_children.end()) {
		return HashWorkPtr();
	}

	if ((*i).begin() > range.begin()) {
		relativeOffsets.first = 0;
	} else {
		relativeOffsets.first = range.begin() - (*i).begin();
	}

	// use copy of passed range, since we modify it later, but need to pass
	// the original to TorrentHasher, otherwise we break in PartData later
	Range64 tmpRange(range);
	uint64_t lastEnd = (*i).end();

	while (i != m_children.end() && (*i).contains(tmpRange)) {
		if ((*i).begin() > tmpRange.begin()) {
			// chunk beginning/middle phys file missing -
			// use cache instead
			logDebug("using cache for hashing");
			RangeList<CacheFile>::CIter j;
			j = m_cache.getContains(tmpRange);
			CHECK_FAIL(j != m_cache.end());
			(*j).m_impl->save();
			files.push_back((*j).m_impl->getLocation());
			relativeOffsets.second = (*j).length() - 1;
			if ((*j).end() < tmpRange.end()) {
				tmpRange.begin((*j).end() + 1);
			} else {
				break;
			}
		} else {
			if (doSave) {
				(*i).m_file->save();
			}
			if ((*i).m_file->allocInProgress()) {
				waitAlloc.insert((*i).m_file);
			}
			files.push_back((*i).m_file->getLocation());
			CHECK_FAIL(tmpRange.end() >= (*i).begin());
			relativeOffsets.second = tmpRange.end() - (*i).begin();
			lastEnd = (*i).end();
			if ((*i).end() < tmpRange.end()) {
				tmpRange.begin((*i).end() + 1);
			} else {
				break;
			}
		}
		i = m_children.getContains(tmpRange);
	}
	// chunk end physical file missing - use cache instead
	if (lastEnd < range.end()) {
		RangeList<CacheFile>::CIter j = m_cache.getContains(tmpRange);
		while (j != m_cache.end()) {
			if (doSave) {
				(*j).m_impl->save();
			}
			files.push_back((*j).m_impl->getLocation());
			relativeOffsets.second = (*j).length() - 1;
			if ((*j).end() < tmpRange.end()) {
				tmpRange.begin((*j).end() + 1);
			} else {
				break;
			}
			j = m_cache.getContains(tmpRange);
		}
	}

	c = new TorrentHasher(range, files, relativeOffsets, ref);
	c->waitAlloc(waitAlloc);
	if (c->canRun()) {
		IOThread::instance().postWork(c);
	} else {
		m_pendingChecks.push_back(c);
	}

	// also verify cache
	files.clear();
	RangeList<CacheFile>::CIter j = m_cache.getContains(range);
	if (j == m_cache.end()) {
		return c;
	}
	relativeOffsets.first = 0;
	while (j != m_cache.end() && (*j).contains(range)) {
		if (doSave) {
			(*j).m_impl->save();
		}
		files.push_back((*j).m_impl->getLocation());
		relativeOffsets.second = (*j++).length() - 1;
	}
	cc = new TorrentHasher(range, files, relativeOffsets, ref);
	cc->getEventTable().addHandler(c, this, &PartialTorrent::onCacheVerify);
	IOThread::instance().postWork(cc);
	return c;
}

// note: don't save children here (they are saved from fileslist, or explicitly
// as needed from verifyRange)
void PartialTorrent::save() {
	RangeList<CacheFile>::CIter i = m_cache.begin();
	while (i != m_cache.end()) try {
		(*i++).m_impl->save();
	} catch (std::exception &e) {
		logError(boost::format("Saving torrent cache: %s") % e.what());
		logMsg(
			boost::format(
				"Info: Auto-pausing '%s' for the above reason."
			) % getName()
		);
		autoPause();
		return;
	}
	if (isAutoPaused()) {
		logMsg(
			boost::format("Info: Auto-resuming file '%s'.")
			% getName()
		);
		resume();
	}
}

void PartialTorrent::childAllocDone(PartData *file) {
	std::list<boost::intrusive_ptr<TorrentHasher> >::iterator i, j;
	i = m_pendingChecks.begin();
	while (i != m_pendingChecks.end()) {
		(*i)->allocDone(file);
		if ((*i)->canRun()) {
			IOThread::instance().postWork(*i);
			j = i++;
			m_pendingChecks.erase(j);
		} else {
			++i;
		}
	}
}

void PartialTorrent::onCacheVerify(HashWorkPtr wrk, HashEvent evt) {
	if (evt == HASH_VERIFIED) {
		logDebug(
			COL_BGREEN " === Cache verification suceeded ==="
			COL_NONE
		);
	} else if (evt == HASH_FAILED) {
		logDebug(
			COL_BMAGENTA " !!! Cache verification failed !!!"
			COL_NONE
		);
	}
}

// base class gets updated from childCorruption() signal handler, there's no
// need to call base class's corrupt() method here.
void PartialTorrent::corruption(Range64 r) {
	RangeList<InternalFile>::CIter i = m_children.getContains(r);

	if (i == m_children.end() || (*i).begin() > r.end()) {
		return;
	}

	while (i != m_children.end() && (*i).contains(r)) {
		if ((*i).containsFull(r)) {
			Range64 k(r.begin()-(*i).begin(), r.end()-(*i).begin());
			CHECK_FAIL(k.length() == r.length());
			(*i).m_file->corruption(k);
		} else if ((*i).contains(r.begin())) {
			Range64 k(r.begin() - (*i).begin(), (*i).length() - 1);
			CHECK_FAIL(k.length() < r.length());
			(*i).m_file->corruption(k);
		} else if ((*i).contains(r.end())) {
			Range64 k(0, r.end() - (*i).begin());
			CHECK_FAIL(k.length() < r.length());
			(*i).m_file->corruption(k);
		} else {
			CHECK_FAIL((*i).length() < r.length());
			CHECK_FAIL((*i).length());
			(*i).m_file->corruption(Range64(0, (*i).length() - 1));
		}
		++i;
	}
}

void PartialTorrent::childCorruption(PartData *file, Range64 r) {
	RIter i = m_childrenReverse.find(file);
	CHECK_RET(i != m_childrenReverse.end());
	CHECK_RET((*i).first == file);

	Range64 tmpR(
		r.begin() + (*i).second,
		r.begin() + (*i).second + r.length() - 1
	);
	CHECK(tmpR.length() == r.length());
	setCorrupt(tmpR);
}

bool PartialTorrent::childCanComplete(PartData *file) {
	RIter i = m_childrenReverse.find(file);
	CHECK_RETVAL(i != m_childrenReverse.end(), true);
	CHECK_RETVAL((*i).first == file,           true);

	if (file->getMetaData() && file->getMetaData()->getHashSetCount()) {
		return true;
	} else {
		return isVerified(
			Range64((*i).second, (*i).second + file->getSize() - 1)
		);
	}
}

void PartialTorrent::childChunkVerified(
	PartData *file, uint64_t chunkSize, uint64_t chunk
) {
	RIter i = m_childrenReverse.find(file);
	CHECK_RET(i != m_childrenReverse.end());
	CHECK_RET((*i).first == file);
	Range64 tmp(
		(*i).second + (chunk * chunkSize),
		(*i).second + (chunk + 1) * chunkSize
	);
	setVerified(tmp);
}

void PartialTorrent::childPaused(PartData *file) {
	RIter i = m_childrenReverse.find(file);
	CHECK_RET(i != m_childrenReverse.end());
	CHECK_RET((*i).first == file);

	Range64 toRemove((*i).second, (*i).second + file->getSize() - 1);
	RangeList<CacheFile>::CIter one = m_cache.getContains(toRemove.begin());
	if (one != m_cache.end() && toRemove.contains((*one).end() + 1)) {
		toRemove.begin((*one).end() + 1);
	}
	RangeList<CacheFile>::CIter two = m_cache.getContains(toRemove.end());
	if (two != m_cache.end() && toRemove.contains((*two).begin() - 1)) {
		toRemove.end((*two).begin() - 1);
	}
	if (!m_cache.contains(toRemove)) {
		dontDownload(toRemove);
	}

	// when removing multiple files next to each other, remove the bordering
	// caches as well, since we don't need them anymore.
	if (one != m_cache.end() && !canDownloadSome(*one)) {
		m_cache.erase(*one);
		dontDownload(*one);
	}
	if (one != two && two != m_cache.end() && !canDownloadSome(*two)) {
		m_cache.erase(*two);
		dontDownload(*two);
	}
}

// todo: re-add missing cache entries if needed
void PartialTorrent::childResumed(PartData *file) {
	RIter i = m_childrenReverse.find(file);
	CHECK_RET(i != m_childrenReverse.end());
	CHECK_RET((*i).first == file);

	Range64 toAdd((*i).second, (*i).second + file->getSize() - 1);
	doDownload(toAdd);
}

void PartialTorrent::childDestroyed(PartData *file) {
	RIter i = m_childrenReverse.find(file);
	CHECK_RET(i != m_childrenReverse.end());
	CHECK_RET((*i).first == file);

	Range64 r((*i).second, (*i).second + file->getSize() - 1);
	CHECK_RET(m_children.getContains(r) != m_children.end());
	CHECK_RET(*m_children.getContains(r) == r);
	m_children.erase(r);
	m_childrenReverse.erase(file);
	file->setParent(reinterpret_cast<Object*>(0));

	if (!m_children.size()) {
		if (!file->isComplete()) {
			PartData::cancel();
			cleanCache(true);
		} else {
			getEventTable().postEvent(this, PD_COMPLETE);
			cleanCache(false);
		}
		destroy();
	}
}

void PartialTorrent::parentChunkVerified(
	PartData *file, uint64_t chunkSize, uint64_t chunk
) {
	CHECK_RET(file == this);

	Range64 c(chunk * chunkSize, ((chunk + 1) * chunkSize) - 1);
	RangeList<InternalFile>::CIter i = m_children.getContains(c);

	CHECK_RET(i != m_children.end());

	while (i != m_children.end() && (*i).contains(c)) {
		uint64_t offset = (*i).begin();
		if (c.containsFull(*i)) {           // all of file was verified
			(*i).m_file->setVerified(Range64(0, (*i).length() - 1));
		} else if ((*i).containsFull(c)) {  // entire chunk is in this
			Range64 tmp(c.begin() - offset, c.end() - offset);
			(*i).m_file->setVerified(tmp);
		} else if ((*i).contains(c.begin())) { // begins here
			Range64 tmp(c.begin() - offset, (*i).length() - 1);
			(*i).m_file->setVerified(tmp);
		} else if ((*i).contains(c.end())) { // ends here
			Range64 tmp(0, c.end() - offset);
			(*i).m_file->setVerified(tmp);
		}
		(*i++).m_file->tryComplete();
	}
}

void PartialTorrent::pause() {
	RIter i = m_childrenReverse.begin();
	while (i != m_childrenReverse.end()) {
		(*i++).first->pause();
	}
	PartData::pause();
}

void PartialTorrent::stop() {
	RIter i = m_childrenReverse.begin();
	while (i != m_childrenReverse.end()) {
		(*i++).first->stop();
	}
	PartData::stop();
}

void PartialTorrent::resume() {
	RIter i = m_childrenReverse.begin();
	while (i != m_childrenReverse.end()) {
		(*i++).first->resume();
	}
	PartData::resume();
}

// don't call PartData::cancel() here, since that's done indirectly from
// onChildDestroyed
void PartialTorrent::cancel() {
	RIter i = m_childrenReverse.begin();
	while (i != m_childrenReverse.end()) {
		(*i++).first->cancel();
	}
}

void PartialTorrent::allocDiskSpace() {
	RIter i = m_childrenReverse.begin();
	while (i != m_childrenReverse.end()) {
		(*i++).first->allocDiskSpace();
	}
}

void PartialTorrent::cleanCache(bool delTorrent) {
	std::string tmp(getLocation().leaf());
	boost::filesystem::directory_iterator it(getLocation().branch_path());
	boost::filesystem::directory_iterator end;
	while (it != end) {
		if (!delTorrent && (*it).leaf() == tmp) {
			++it;
			continue;
		}
		if (boost::algorithm::starts_with((*it).leaf(), tmp)) try {
			logDebug("Removing " + (*it).native_file_string());
			boost::filesystem::remove(*it);
		} catch (std::exception &e) {}
		++it;
	}
}

// TorrentHasher class
// -------------------
TorrentHasher::TorrentHasher(
	Range64 globalOffsets,
	const std::vector<boost::filesystem::path> &files,
	std::pair<uint64_t, uint64_t> relativeOffsets, const HashBase *ref
) : HashWork(files[0], relativeOffsets.first, relativeOffsets.second, ref),
m_files(files), m_curFile(m_files.begin()), m_globalOffsets(globalOffsets) {
	CHECK(files.size());
	CHECK(ref);
}

TorrentHasher::~TorrentHasher() {
}

// while the actual reading is done by base class, here we "swap out" the
// underlying file when crossing file boundaries. Basically, if we reach the end
// of current file, and we still need data, we switch (via openFile() call) to
// the next file. openFile() opens the file pointed to by m_fileName, which we
// adjust before calling it.
uint64_t TorrentHasher::readNext(uint64_t pos) {
	if (m_files.size() == 1) {
		return HashWork::readNext(pos);
	} else {
		Iter i = m_curFile;
		if (++i == m_files.end()) {
			return HashWork::readNext(pos);
		} else {
			uint64_t tmpEnd = m_end;
			m_end = std::numeric_limits<uint64_t>::max();
			uint64_t ret = HashWork::readNext(pos);
			m_end = tmpEnd;
			if (ret < getBufSize() && ++m_curFile != m_files.end()){
				m_fileName = *m_curFile;
				openFile();
			}
			return ret;
		}
	}
}

// before sending the hash results back to the calling code, re-adjust the
// offsets to global, since internally we used relative offsets within the
// first and last files of this hash job (in case the job crossed multiple
// file boundaries)
void TorrentHasher::finish() {
	if (!isFull()) {
		m_begin = m_globalOffsets.begin();
		m_end   = m_globalOffsets.end();
	}
	HashWork::finish();
}

} // end Bt namespace
