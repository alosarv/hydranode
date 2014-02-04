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
 * \file partdata.cpp Implementation of PartData and related classes
 */

#include <hncore/pch.h>

#include <hnbase/log.h>
#include <hnbase/lambda_placeholders.h>
#include <hnbase/hash.h>
#include <hnbase/prefs.h>
#include <hnbase/timed_callback.h>

#include <hncore/partdata.h>
#include <hncore/partdata_impl.h>
#include <hncore/metadata.h>
#include <hncore/hasher.h>
#include <hncore/hydranode.h>

#include <boost/lambda/lambda.hpp>
#include <boost/lambda/if.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/algorithm/string/replace.hpp>

#include <fstream>
#include <fcntl.h>

using namespace boost::lambda;
using namespace boost::multi_index;
using namespace CGComm;

static const uint32_t BUF_SIZE_LIMIT = 512*1024; //!< 512k buffer

namespace Detail {

// UsedRange class
// -------------------------
template<typename IterType>
UsedRange::UsedRange(PartData *parent, IterType it) : Range64((*it).begin(),
(*it).end()), m_parent(parent),
m_chunk(new PosIter(project<ID_Pos>(*m_parent->m_chunks, it))) {
	CHECK_THROW(parent != 0);
	CHECK_THROW(*m_chunk != get<ID_Pos>(*parent->m_chunks).end());

	// SpamReduction(tm)
	if (length() > 1) {
		logTrace(TRACE_PARTDATA,
			boost::format("%s: Using range %d..%d")
			% m_parent->m_dest.leaf() % begin() % end()
		);
	}

	get<ID_Pos>(*m_parent->m_chunks).modify(
		*m_chunk, ++bind(&Chunk::m_useCnt, __1)
	);
}

UsedRange::UsedRange(PartData *parent, uint64_t begin, uint64_t end) :
Range64(begin, end), m_parent(parent) {
	m_chunk.reset(new PosIter(get<ID_Pos>(*m_parent->m_chunks).end()));

	if (length() > 1) {
		logTrace(TRACE_PARTDATA,
			boost::format("%s: Using range %d..%d")
			% m_parent->m_dest.leaf() % this->begin() % this->end()
		);
	}
}

UsedRange::~UsedRange() {
	if (length() > 1) {
		logTrace(TRACE_PARTDATA,
			boost::format("%s: Un-using range %d..%d")
			% m_parent->m_dest.leaf() % begin() % end()
		);
	}

	if (*m_chunk != m_parent->m_chunks->get<ID_Pos>().end()) {
		get<ID_Pos>(*m_parent->m_chunks).modify(
			*m_chunk, --bind(&Chunk::m_useCnt, __1)
		);
	}
}

LockedRangePtr UsedRange::getLock(uint32_t size) {
	return m_parent->getLock(shared_from_this(), size);
}

bool UsedRange::isComplete() const {
	return m_parent->isComplete(*this);
}

// LockedRange class
// ---------------------------
LockedRange::LockedRange(PartData *parent, Range64 r, UsedRangePtr used)
: Range64(r), m_parent(parent),
m_chunk(new PosIter(get<ID_Pos>(*m_parent->m_chunks).end())), m_used(used) {

	// SpamReduction(tm)
	if (length() > 1) {
		logTrace(TRACE_PARTDATA,
			boost::format("%s: Locking range %d..%d")
			% m_parent->m_dest.leaf() % begin() % end()
		);
	}

	m_parent->m_locked.merge(*this);
}

LockedRange::LockedRange(
	PartData *parent, Range64 r, PosIter &it, UsedRangePtr used
) : Range64(r), m_parent(parent), m_chunk(new PosIter(it)), m_used(used) {
	CHECK_THROW(parent != 0);

	if (length() > 1) {
		logTrace(TRACE_PARTDATA,
			boost::format("%s: Locking range %d..%d")
			% m_parent->m_dest.leaf() % begin() % end()
		);
	}

	m_parent->m_locked.merge(*this);
}

LockedRange::~LockedRange() {
	if (length() > 1) {
		logTrace(TRACE_PARTDATA,
			boost::format("%s: UnLocking range %d..%d")
			% m_parent->m_dest.leaf() % begin() % end()
		);
	}

	m_parent->m_locked.erase(*this);
}

void LockedRange::write(uint64_t begin, const std::string &data) {
	if (begin > end() || begin + data.size() - 1 > end()) {
		throw PartData::LockError("Writing outside lock.");
	}
	if (*m_chunk != get<ID_Pos>(*m_parent->m_chunks).end()) {
		m_parent->m_chunks->get<ID_Pos>().modify(
			*m_chunk, bind(&Chunk::write, __1, begin, data)
		);
	} else {
		m_parent->doWrite(begin, data);
		m_parent->tryComplete();
	}
}

// Chunk class
// ---------------------
Chunk::Chunk(
	PartData *parent, uint64_t begin, uint64_t end,
	uint32_t size, const HashBase *hash
) : Range64(begin, end), m_parent(parent), m_hash(hash), m_verified(
	m_parent->isVerified(*this)
), m_partial(
	m_parent->m_complete.contains(*this) &&
	!m_parent->m_complete.containsFull(*this)
), m_complete(m_parent->isComplete(*this)), m_avail(), m_useCnt(), m_size(size)
{
	if (isComplete() && !isVerified() && hash) {
		HashWorkPtr c = m_parent->verifyRange(*this, m_hash);
		if (c) {
			c->getEventTable().addHandler(
				c, this, &Chunk::onHashEvent
			);
		}
	}
}

Chunk::Chunk(
	PartData *parent, Range64 range, uint32_t size, const HashBase *hash
) : Range64(range), m_parent(parent), m_hash(hash), m_verified(
	m_parent->isVerified(*this)
), m_partial(
	m_parent->m_complete.contains(*this) &&
	!m_parent->m_complete.containsFull(*this)
), m_complete(m_parent->isComplete(*this)), m_avail(), m_useCnt(), m_size(size)
{
	if (isComplete() && !isVerified() && hash) {
		HashWorkPtr c = m_parent->verifyRange(*this, m_hash);
		if (c) {
			c->getEventTable().addHandler(
				c, this, &Chunk::onHashEvent
			);
		}
	}
}

void Chunk::write(uint64_t begin, const std::string &data) {
	m_parent->doWrite(begin, data);
	if (!m_partial) {
		CMPosIndex &idx = m_parent->m_chunks->get<ID_Pos>();
		CMPosIndex::iterator it = idx.find(*this);
		assert(it != idx.end());
		idx.modify(it, bind(&Chunk::m_partial, __1) = true);
	}
}

void Chunk::updateState() const {
	CMPosIndex &idx = m_parent->m_chunks->get<ID_Pos>();
	CMPosIndex::iterator it = idx.find(*this);
	assert(it != idx.end());
	if (m_parent->m_complete.contains(*this)) {
		if (m_parent->m_complete.containsFull(*this)) {
			idx.modify(it, bind(&Chunk::m_partial, __1) = false);
			idx.modify(it, bind(&Chunk::m_complete, __1) = true);
		} else {
			idx.modify(it, bind(&Chunk::m_partial, __1) = true);
			idx.modify(it, bind(&Chunk::m_complete, __1) = false);
		}
	} else {
		idx.modify(it, bind(&Chunk::m_partial, __1) = false);
		idx.modify(it, bind(&Chunk::m_complete, __1) = false);
	}
	idx.modify(
		it, bind(&Chunk::m_verified, __1) = m_parent->isVerified(*this)
	);
	if (isComplete() && !isVerified()) {
		const_cast<Chunk*>(this)->verify();
	}
}

void Chunk::verify(bool save) {
	logTrace(TRACE_PARTDATA,
		boost::format("%s: Completed chunk %d..%d")
		% m_parent->m_dest.leaf() % this->begin() % this->end()
	);

	CMPosIndex &idx = m_parent->m_chunks->get<ID_Pos>();
	CMPosIndex::iterator it = idx.find(*this);
	assert(it != idx.end());
	idx.modify(it, bind(&Chunk::m_partial, __1) = false);
	idx.modify(it, bind(&Chunk::m_verified, __1) = false);
	idx.modify(it, bind(&Chunk::m_complete, __1) = true);

	if (m_hash) {
		HashWorkPtr c = m_parent->verifyRange(*this, m_hash, save);
		if (c) {
			c->getEventTable().addHandler(
				c, this, &Chunk::onHashEvent
			);
		}
	} else {
		logWarning(
			boost::format(
				"%s: Chunk %s..%s has no hash - "
				"shouldn't happen..."
			) % m_parent->m_dest.leaf() %this->begin() % this->end()
		);
		m_parent->tryComplete();
	}
}

void Chunk::onHashEvent(HashWorkPtr c, HashEvent evt){
	assert(*c->getRef() == *getHash());
	c->getEventTable().delHandlers(c);

	CMPosIndex &idx = m_parent->m_chunks->get<ID_Pos>();
	CMPosIndex::iterator it = idx.find(*this);
	assert(it != idx.end());
	if (evt == HASH_FAILED) {
		m_parent->corruption(*this);
		idx.modify(it, bind(&Chunk::m_verified, __1) = false);
		idx.modify(it, bind(&Chunk::m_complete, __1) = false);
		idx.modify(it, bind(&Chunk::m_partial, __1) = false);
		if (m_parent->m_fullJob) {
			m_parent->m_fullJob->cancel();
			m_parent->m_fullJob = HashWorkPtr();
		}
		m_parent->m_partStatus[m_size][begin()/m_size] = false;
		logTrace(TRACE_CHUNKS,
			boost::format("Chunk #%d is corrupt.")
			% (begin() / m_size)
		);
		assert(!isComplete());
	} else if (evt == HASH_VERIFIED) {
		idx.modify(it, bind(&Chunk::m_complete, __1) = true);
		idx.modify(it, bind(&Chunk::m_verified, __1) = true);
		idx.modify(it, bind(&Chunk::m_partial, __1) = false);
		m_parent->m_partStatus[m_size][begin()/m_size] = true;
		m_parent->m_verified.merge(*this);
		m_parent->m_complete.merge(*this);
		m_parent->onVerified(m_parent, m_size, begin()/m_size);
		logTrace(TRACE_CHUNKS,
			boost::format("Verified chunk #%d") % (begin() / m_size)
		);
	} else if (evt == HASH_FATAL_ERROR) {
		logError(
			boost::format("Fatal error hashing file `%s'")
			% c->getFileName().native_file_string()
		);
	}
	--m_parent->m_pendingHashes;
	m_parent->tryComplete();
}

/**
 * AllocJob allocates disk space for temp file. Emits event 'true' when
 * allocation succeeds, false otherwise.
 */
class AllocJob : public ThreadWork {
public:
	DECLARE_EVENT_TABLE(AllocJobPtr, bool);
	AllocJob(const boost::filesystem::path &file, uint64_t size);
	virtual bool process();
private:
	boost::filesystem::path m_file;
	uint64_t m_size;
};
IMPLEMENT_EVENT_TABLE(AllocJob, AllocJobPtr, bool);

AllocJob::AllocJob(const boost::filesystem::path &file, uint64_t size)
: m_file(file), m_size(size) {}

// allocates disk space in 10mb increments
bool AllocJob::process() {
	int fd = open(m_file.native_file_string().c_str(), O_RDWR|O_BINARY);
	if (!fd) {
		getEventTable().postEvent(AllocJobPtr(this), false);
	} else {
		::lseek64(fd, m_size - 1, SEEK_SET);
		::write(fd, "1", 1);
		::fsync(fd);
		::close(fd);
		assert(Utils::getFileSize(m_file) <= m_size);
		bool ret = Utils::getFileSize(m_file) == m_size;
		getEventTable().postEvent(AllocJobPtr(this), ret);
	}
	setComplete();
	return true;
}
} // namespace Detail

using namespace Detail;

// PartData exception classes
// --------------------------
PartData::LockError::LockError(const std::string &msg):std::runtime_error(msg){}
PartData::RangeError::RangeError(const std::string &mg):std::runtime_error(mg){}

// PartData class
// --------------
IMPLEMENT_EVENT_TABLE(PartData, PartData*, int);

PartData::PartData(
	uint64_t size,
	const boost::filesystem::path &loc,
	const boost::filesystem::path &dest
) : Object(0), m_size(size), m_loc(loc), m_dest(dest), m_chunks(new ChunkMap),
m_toFlush(), m_md(), m_pendingHashes(), m_sourceCnt(), m_fullSourceCnt(),
m_paused(), m_stopped(), m_autoPaused() {
	initSignals();

	std::ofstream o(loc.string().c_str(), std::ios::binary);
	o.flush();
	getEventTable().postEvent(this, PD_ADDED);
	cleanupName();
}


PartData::PartData(const boost::filesystem::path &p) try : Object(0),
m_size(), m_chunks(new ChunkMap), m_toFlush(), m_md(), m_pendingHashes(),
m_sourceCnt(), m_fullSourceCnt(), m_paused(), m_stopped(), m_autoPaused() {
	initSignals();

	logTrace(TRACE_PARTDATA,
		boost::format("Loading temp file: %s") % p.string()
	);
	using namespace Utils;
	using boost::algorithm::replace_last;

	std::string tmp(p.string());
	replace_last(tmp, ".dat.bak", ""); // in case of backups
	replace_last(tmp, ".dat", "");

	m_loc = boost::filesystem::path(tmp, boost::filesystem::native);

	std::ifstream ifs(p.string().c_str(), std::ios::binary);
	CHECK_THROW(ifs);
	CHECK_THROW(Utils::getVal<uint8_t>(ifs) == OP_PARTDATA);

	(void)Utils::getVal<uint16_t>(ifs);
	if (Utils::getVal<uint8_t>(ifs) != OP_PD_VER) {
		logWarning("Unknown partdata version.");
	}
	m_size = Utils::getVal<uint64_t>(ifs);

	uint16_t tagc = Utils::getVal<uint16_t>(ifs);

	while (tagc-- && ifs) {
		uint8_t   oc = getVal<uint8_t>(ifs);
		uint16_t len = getVal<uint16_t>(ifs);
		switch (oc) {
			case OP_PD_DESTINATION:
				m_dest = getVal<std::string>(ifs).value();
				break;
			case OP_PD_COMPLETED:
				if (Utils::getVal<uint8_t>(ifs)!=OP_RANGELIST) {
					logWarning("Invalid tag.");
					ifs.seekg(len, std::ios::cur);
				} else {
					(void)Utils::getVal<uint16_t>(ifs);
					m_complete = RangeList64(ifs);
				}
				break;
			case OP_PD_VERIFIED:
				if (Utils::getVal<uint8_t>(ifs)!=OP_RANGELIST) {
					logWarning("Invalid tag.");
					ifs.seekg(len, std::ios::cur);
				} else {
					(void)Utils::getVal<uint16_t>(ifs);
					m_verified = RangeList64(ifs);
				}
				break;
			case OP_PD_STATE: {
				uint8_t state = Utils::getVal<uint8_t>(ifs);
				if (state == STATE_STOPPED) {
					m_stopped = true;
				} else if (state == STATE_PAUSED) {
					m_paused = true;
				}
				break;
			}
			default:
				logWarning("Unhandled tag in PartData.");
				ifs.seekg(len, std::ios::cur);
				break;
		}
	}

	if (ifs && Utils::getVal<uint8_t>(ifs) == OP_METADATA) {
		(void)Utils::getVal<uint16_t>(ifs);
		m_md = new MetaData(ifs);
	}

	cleanupName();
//	printCompleted();

} catch (std::runtime_error &) {
	delete m_md;
}
MSVC_ONLY(;)

PartData::PartData(
	const boost::filesystem::path &path, MetaData *md
) : Object(0), m_size(md->getSize()), m_loc(path), m_chunks(new ChunkMap),
m_toFlush(), m_md(md), m_pendingHashes(), m_sourceCnt(), m_fullSourceCnt(),
m_paused(), m_stopped(), m_autoPaused() {
	initSignals();

	for (uint32_t i = 0; i < md->getHashSetCount(); ++i) {
		HashSetBase *hs = md->getHashSet(i);
		if (hs->getChunkSize() && hs->getChunkCnt()) {
			addHashSet(hs);
		}
	}

	std::string incDir(
		Prefs::instance().read<std::string>("/Incoming", "")
	);

	setDestination(boost::filesystem::path(incDir) / md->getName());
	m_complete.merge(0, getSize() - 1);
	save();
	rehashCompleted();
	getEventTable().postEvent(this, PD_ADDED);
}

// protected default constructor
PartData::PartData() : Object(0), m_size(), m_chunks(new ChunkMap), m_toFlush(),
m_md(), m_pendingHashes(), m_sourceCnt(), m_fullSourceCnt(), m_paused(),
m_stopped(), m_autoPaused() {
	initSignals();
}

PartData::~PartData() {
	getEventTable().delHandlers(this);
}

// boost::signals breaks when the first time the signal is invoced it's done
// from different module than the one where it was originally created in. while
// the majority of these signals are only invoced from inside partdata, they
// are part of partdata's public api, so anyone can invoke them (sadly), hence
// make sure they get properly initalized here.
//
// since many of the signals duplicate events, we can make our life bit easier
// here, and connect the postevent calls to the signals, so we don't have to do
// both things everywhere (which would quickly become errorprone).
void PartData::initSignals() {
	onPaused.connect(
		boost::bind(
			&EventTable<PartData*, int>::postEvent,
			&getEventTable(), _b1, PD_PAUSED
		)
	);
	onStopped.connect(
		boost::bind(
			&EventTable<PartData*, int>::postEvent,
			&getEventTable(), _b1, PD_STOPPED
		)
	);
	onResumed.connect(
		boost::bind(
			&EventTable<PartData*, int>::postEvent,
			&getEventTable(), _b1, PD_RESUMED
		)
	);
	onCompleted.connect(
		boost::bind(
			&EventTable<PartData*, int>::postEvent,
			&getEventTable(), _b1, PD_COMPLETE
		)
	);
	onCanceled.connect(
		boost::bind(
			&EventTable<PartData*, int>::postEvent,
			&getEventTable(), _b1, PD_CANCELED
		)
	);
}

void PartData::init() {
	CHECK_RET(m_md);

	// add hashes from metadata
	for (uint32_t i = 0; i < m_md->getHashSetCount(); ++i) {
		HashSetBase *hs = m_md->getHashSet(i);
		if (hs->getChunkSize() && hs->getChunkCnt()) {
			addHashSet(hs);
		}
	}

	// It's possible we loaded a complete file - if so, re-try completing.
	if (isComplete()) {
		Utils::timedCallback(
			boost::bind(&PartData::tryComplete, this), 0
		);
	} else {
		uint32_t modDate = Utils::getModDate(m_loc);
		bool needRehash = false;
		if (m_md && modDate != m_md->getModDate()) {
			// allow 1ms errors (FAT32 causes them)
			if (modDate + 1 != m_md->getModDate()) {
				needRehash = true;
			}
		}
		if (needRehash) {
			logMsg(
				boost::format(
					"%s: Modification date changed "
					"(%d != %d), rehashing completed parts."
				) % m_dest.leaf() % modDate % m_md->getModDate()
			);
			rehashCompleted();
		}
	}

	getEventTable().postEvent(this, PD_ADDED);
}

void PartData::rehashCompleted() {
	CMPosIndex &idx = m_chunks->get<ID_Pos>();
	for (CMPosIndex::iterator i = idx.begin(); i != idx.end(); ++i) {
		if ((*i).getHash()) {
			idx.modify(i, bind(&Chunk::verify, __1, false));
		}
	}
}

void PartData::addSourceMask(
	uint64_t chunkSize, const std::vector<bool> &chunks
) {
	typedef CMLenIndex::iterator Iter;

	++m_sourceCnt;
	if (chunks.empty()) {
		addFullSource(chunkSize);
		return;
	}

	// ed2k has 1 more chunk if size == N * chunkSize
	uint64_t expected = getChunkCount(chunkSize);
	if (m_size % chunkSize == 0 && chunkSize == ED2K_PARTSIZE) {
		++expected;
	}
	if (chunks.size() != expected) {
		boost::format err(
			"Invalid number of values in chunkmap "
			"(expected %d, got %d)"
		);
		err % expected % chunks.size();
		throw std::runtime_error(err.str());
	}

	checkAddChunkMap(chunkSize);
	int i = 0;
	CMLenIndex& pi = m_chunks->get<ID_Length>();
	std::pair<Iter, Iter> ret = pi.equal_range(chunkSize);
	for (Iter j = ret.first; j != ret.second; ++j) {
		pi.modify(j, bind(&Chunk::m_avail, __1) += chunks[i++]);
	}
}

void PartData::addFullSource(uint64_t chunkSize) {
	typedef CMLenIndex::iterator Iter;

	checkAddChunkMap(chunkSize);
	CMLenIndex& pi = m_chunks->get<ID_Length>();
	std::pair<Iter, Iter> ret = pi.equal_range(chunkSize);
	for (Iter i = ret.first; i != ret.second; ++i) {
		pi.modify(i, ++bind(&Chunk::m_avail, __1));
	}
	++m_fullSourceCnt;
}
void PartData::delSourceMask(
	uint64_t chunkSize, const std::vector<bool> &chunks
) {
	typedef CMLenIndex::iterator Iter;

	CHECK_THROW(m_sourceCnt);
	--m_sourceCnt;
	if (chunks.empty()) {
		delFullSource(chunkSize);
		return;
	}

	// ed2k has 1 more chunk if size == N * chunkSize
	uint64_t expected = getChunkCount(chunkSize);
	if (m_size % chunkSize == 0 && chunkSize == ED2K_PARTSIZE) {
		++expected;
	}
	if (chunks.size() != expected) {
		boost::format err(
			"Invalid number of values in chunkmap "
			"(expected %d, got %d)"
		);
		err % expected % chunks.size();
		throw std::runtime_error(err.str());
	}

	int i = 0;
	CMLenIndex& pi = m_chunks->get<ID_Length>();
	std::pair<Iter, Iter> ret = pi.equal_range(chunkSize);
	for (Iter j = ret.first; j != ret.second; ++j) {
		if(chunks[i]) { // ensures we don't integer underflow
			CHECK_THROW((*j).m_avail);
		}
		pi.modify(j, bind(&Chunk::m_avail, __1) -= chunks[i++]);
	}
}
void PartData::delFullSource(uint64_t chunkSize) {
	typedef CMLenIndex::iterator Iter;

	CHECK_THROW(m_fullSourceCnt);
	--m_fullSourceCnt;
	CMLenIndex& pi = m_chunks->get<ID_Length>();
	std::pair<Iter, Iter> ret = pi.equal_range(chunkSize);
	for (Iter i = ret.first; i != ret.second; ++i) {
		CHECK_THROW((*i).m_avail);
		pi.modify(i, --bind(&Chunk::m_avail, __1));
	}
}

/**
 * Unary function object for usage with doGetRange() method
 *
 * @param r  Ignored
 * @returns Always true
 */
struct TruePred { bool operator()(const Range64 &) { return true; } };

/**
 * Unary function object for usage with doGetRange() method, in order to check
 * if the generated range is contained within the chunkmap contained within this
 * functor.
 *
 * @param r    Range candidate to be checked
 * @return     True if @param r is contained within m_rl, false otherwise
 */
struct CheckPred {
	CheckPred(const std::vector<bool> &chunks, uint32_t chunkSize)
	: m_chunks(chunks), m_chunkSize(chunkSize) {}

	bool operator()(const Range64 &r) {
		if (r.begin() > m_chunks.size() * m_chunkSize) {
			return false;
		}
		uint32_t chunk1 = r.begin() / m_chunkSize;
		uint32_t chunk2 = r.end() / m_chunkSize + 1;
		while (chunk1 != chunk2) {
			if (!m_chunks[chunk1++]) {
				return false;
			}
		}
		return true;
	}
	const std::vector<bool> &m_chunks;
	uint64_t m_chunkSize;
};


UsedRangePtr PartData::getRange(uint32_t size) {
	TruePred pred;
	return doGetRange(size, pred);
}

UsedRangePtr PartData::getRange(
	uint32_t size, const std::vector<bool> &chunks
) {
	if (chunks.empty()) {
		TruePred pred;
		return doGetRange(size, pred);
	}
	CheckPred pred(chunks, size);
	return doGetRange(size, pred);
}

template<typename Predicate>
UsedRangePtr PartData::getNextChunk(uint64_t size, Predicate &pred) {
	UsedRangePtr ret;
	if (m_complete.empty()) {
		ret = UsedRangePtr(new UsedRange(this, 0, size));
		if (ret->end() > m_size - 1) {
			ret->end(m_size - 1);
		}
	} else if (m_complete.front().begin() > 0) {
		uint64_t end = m_complete.front().begin() - 1;
		ret = UsedRangePtr(new UsedRange(this, 0, end));
	} else if (m_complete.front().end() < m_size) {
		uint64_t beg = m_complete.front().end() + 1;
		uint64_t end = beg + size > m_size ? m_size : beg +size;
		ret = UsedRangePtr(new UsedRange(this, beg, end));
	}
	if (!ret->getLock(1)) {
		ret.reset();
	}
	return ret;
}

template<typename Predicate>
UsedRangePtr PartData::doGetRange(uint64_t size, Predicate &pred) {
	typedef CMSelectIndex::iterator SIter;

	if (m_chunks->empty()) {
		return getNextChunk(size, pred);
	}
	CMSelectIndex &idx = m_chunks->template get<ID_Selector>();
	std::pair<SIter, SIter> r = idx.equal_range(
		boost::make_tuple(false, true)
	);
	uint32_t hopCnt = 0;
	for (SIter i = r.first; i != r.second; ++i) {
		if (pred(*i) && canLock(*i)) {
			UsedRangePtr ret(new UsedRange(this, i));
			assert(ret->getLock(1));
			logTrace(TRACE_CHUNKS,
				boost::format(
					"Selected chunk #%d (avail=%d, "
					"usecnt=%d partial=%s complete="
					"%s) in %d hops"
				) % ((*i).begin() / (*i).m_size)
				% (*i).m_avail % (*i).m_useCnt
				% ((*i).m_partial ? "yes" : "no")
				% ((*i).m_complete ? "yes" : "no")
				% hopCnt
			);
			return ret;
		}
		++hopCnt;
	}
	logTrace(TRACE_CHUNKS,
		boost::format("Failure to select chunk, did %d hops") % hopCnt
	);
	return UsedRangePtr();
}

uint64_t PartData::getChunkCount(uint64_t chunkSize) const {
	return m_size / chunkSize + (m_size % chunkSize ? 1 : 0);
}

void PartData::checkAddChunkMap(uint64_t cs) {
	typedef CMLenIndex::iterator LIter;

	logTrace(TRACE_PARTDATA, boost::format("Adding chunkmap (cs=%d)") % cs);

	if (m_partStatus.find(cs) == m_partStatus.end()) {
		m_partStatus[cs] = std::vector<bool>(getChunkCount(cs));
	}
	std::pair<LIter, LIter> ret =m_chunks->get<ID_Length>().equal_range(cs);
	if (ret.first == m_chunks->get<ID_Length>().end()) {
		for (uint64_t i = 0; i < getChunkCount(cs); ++i) {
			uint64_t beg = i * cs;
			uint64_t end = (i + 1) * cs - 1;
			if (end > m_size - 1) {
				end = m_size - 1;
			}
			Chunk c(this, beg, end, cs);
			m_chunks->insert(c);
			m_partStatus[cs][i] = isComplete(beg, end);
		}
	}
}

void PartData::addHashSet(const HashSetBase *hs) {
	if (!getSize()) {
		logDebug("Cannot add hashes to files of size 0.");
		return;
	}

	CHECK_THROW(hs->getChunkSize() > 0);
	typedef CMLenIndex::iterator LIter;

	logTrace(TRACE_PARTDATA,
		boost::format("Adding hashset (cs=%d cc=%d)")
		% hs->getChunkSize() % hs->getChunkCnt()
	);

	uint32_t cs = hs->getChunkSize();
	std::pair<LIter, LIter> ret =m_chunks->get<ID_Length>().equal_range(cs);

	if (ret.first == m_chunks->get<ID_Length>().end()) {
		checkAddChunkMap(cs);
		ret = m_chunks->get<ID_Length>().equal_range(cs);
	}

	int cc = hs->getChunkCnt();
	int check = std::distance(ret.first, ret.second);

	// file of size of multiple of chunksize has one more chunkhash in ed2k
	// TODO: is this portable across networks?
	CHECK_THROW(m_size % cs ? check == cc : check == cc - 1);

	uint32_t j = 0;
	bool saved = false;
	for (LIter i = ret.first; i != ret.second; ++i) {
		m_chunks->get<ID_Length>().modify(
			i, bind(&Chunk::m_hash, __1) = &(*hs)[j++]
		);
		if ((*i).isComplete() && !(*i).isVerified()) {
			if (!saved) {
				save();
				saved = true;
			}
			m_chunks->get<ID_Length>().modify(
				i, bind(&Chunk::verify, __1, false)
			);
		}
	}
}

bool PartData::canLock(const Range64 &r, uint32_t size) const {
	Range64 cand(r.begin(), r.begin());
	typedef RangeList64::CIter CIter;
	CIter i = m_complete.getContains(cand);
	CIter j = m_locked.getContains(cand);
	CIter k = m_dontDownload.getContains(cand);
	do {
		i = m_complete.getContains(cand);
		j = m_locked.getContains(cand);
		k = m_dontDownload.getContains(cand);
		if (i != m_complete.end()) {
			cand = Range64((*i).end() + 1, (*i).end() + 1);
		} else if (j != m_locked.end()) {
			cand = Range64((*j).end() + 1, (*j).end() + 1);
		} else if (k != m_dontDownload.end()) {
			cand = Range64((*k).end() + 1, (*k).end() + 1);
		} else {
			break;
		}
	} while (r.contains(cand));
	return r.contains(cand);
}

LockedRangePtr PartData::getLock(UsedRangePtr used, uint32_t size) {
	Range64 cand(used->begin(), used->begin());
	typedef RangeList64::CIter CIter;
	CIter i = m_complete.getContains(cand);
	CIter j = m_locked.getContains(cand);
	CIter k = m_dontDownload.getContains(cand);
	do {
		i = m_complete.getContains(cand);
		j = m_locked.getContains(cand);
		k = m_dontDownload.getContains(cand);
		if (i != m_complete.end()) {
			cand = Range64((*i).end() + 1, (*i).end() + 1);
		} else if (j != m_locked.end()) {
			cand = Range64((*j).end() + 1, (*j).end() + 1);
		} else if (k != m_dontDownload.end()) {
			cand = Range64((*k).end() + 1, (*k).end() + 1);
		} else {
			break;
		}
	} while (used->contains(cand));
	if (!used->contains(cand)) {
		return LockedRangePtr();
	}
	cand.end(cand.begin() + size - 1);
	if (cand.end() > used->end()) {
		cand.end(used->end());
	}
	i = m_complete.getContains(cand);
	if (i != m_complete.end()) {
		cand.end((*i).begin() - 1);
	}
	i = m_locked.getContains(cand);
	if (i != m_locked.end()) {
		cand.end((*i).begin() - 1);
	}
	k = m_dontDownload.getContains(cand);
	if (k != m_dontDownload.end()) {
		cand.end((*k).begin() - 1);
	}
	CHECK_THROW(cand.length() <= size);
	return LockedRangePtr(
		new LockedRange(this, cand, *used->m_chunk, used)
	);
}

// publically available write method, this allows writing to the file without
// aquiring Used/Locked ranges - useful for simple protocols.
// Basically it just forwards the call to doWrite(), however it does a bunch of
// checks prior to the call, as well as bunch of chunkmap updates after the
// call in order to keep everyone happy.
void PartData::write(uint64_t begin, const std::string &data) {
	logTrace(TRACE_PARTDATA,
		boost::format("Safe-writing at offset %d.") % begin
	);

	CHECK_THROW(!m_locked.contains(begin, begin + data.size() - 1));
	CHECK_THROW(!m_complete.contains(begin, begin + data.size() - 1));

	doWrite(begin, data);
	tryComplete();
}

void PartData::updateChunks(Range64 range) {
	CMPosIndex &pi = m_chunks->get<ID_Pos>();
	CMPosIndex::iterator i = pi.lower_bound(Chunk(this, range, 0));
	if (i != pi.end() && (*i).contains(range)) {
		(*i).updateState();
	}
	CMPosIndex::iterator j = i;
	while (i != pi.begin() && (*--i).contains(range)) {
		(*i).updateState();
	}
	while (j != pi.end() && ++j != pi.end() && (*j).contains(range)) {
		(*j).updateState();
	}
}

void PartData::setComplete(Range64 range) {
	m_complete.merge(range);
	m_dontDownload.erase(range);
	updateChunks(range);
}

void PartData::setCorrupt(Range64 range) {
	m_complete.erase(range);
	m_corrupt.merge(range);
	m_verified.erase(range);
	onCorruption(this, range);
	updateChunks(range);
}

void PartData::setVerified(Range64 range) {
	m_verified.merge(range);
	m_corrupt.erase(range);
	m_complete.merge(range);
	updateChunks(range);
}

// don't try to call doComplete() from here, since this method is called from
// Chunk::write(), prior to the chunk attempting to hash itself (if it has
// hashes). Thus, doComplete() check here would incorrectly succeed, since
// m_pendingHashes hasn't been increased, which would then lead to full rehash
// being done prior to last chunkhash, resulting in crash later on during file
// moving.
void PartData::doWrite(uint64_t begin, const std::string &data) {
	// since we allow (on certain conditions) resizing partdata on the fly,
	// this check ensures that we don't allow writing to a file of size 0
	CHECK_THROW(m_size);
	// don't allow writing to paused/stopped file
	CHECK_THROW(isRunning());

	logTrace(TRACE_PARTDATA,
		boost::format("Writing at offset %d, datasize is %d")
		% begin % data.size()
	);

	CHECK_THROW(!m_complete.contains(begin, begin + data.size() - 1));

	m_buffer[begin] = data;
	setComplete(Range64(begin, begin + data.size() - 1));
	m_toFlush += data.size();
	getEventTable().postEvent(this, PD_DATA_ADDED);
	if (m_toFlush >= BUF_SIZE_LIMIT) {
		save();
	}
	dataAdded(this, begin, data.size());
}

void PartData::flushBuffer() {
	logTrace(TRACE_PARTDATA,
		boost::format("Flushing buffers: %s") % m_dest.leaf()
	);

	// don't attempt to alloc if we'r shutting down
	if (m_allocJob || (m_toFlush && !Hydranode::instance().isRunning())) {
		return;
	} else if (m_toFlush && Utils::getFileSize(m_loc) < m_size) {
		allocDiskSpace();
		return;
	}

	int fd = open(m_loc.native_file_string().c_str(), O_RDWR|O_BINARY);
	if (fd < 1) {
		throw std::runtime_error(
			"unable to open file " + m_loc.native_file_string()
			+ " for writing."
		);
	}

	for (BIter i = m_buffer.begin(); i != m_buffer.end(); ++i) try {
		uint64_t ret = lseek64(fd, (*i).first, SEEK_SET);
		CHECK_THROW(ret == static_cast<uint64_t>((*i).first));
		int c = ::write(fd, (*i).second.data(), (*i).second.size());
		CHECK_THROW(c == static_cast<int>((*i).second.size()));
	} catch (...) {
		close(fd);
		throw std::runtime_error("no space left on drive");
	}
	fsync(fd);
	close(fd);

	m_buffer.clear();
	m_toFlush = 0;
	if (m_md) {
		m_md->setModDate(Utils::getModDate(m_loc));
	}
	getEventTable().postEvent(this, PD_DATA_FLUSHED);
//	printCompleted();
}

//! Sorts container of HashSetBase* objects based on chunkhashcount
struct ChunkCountPred {
	bool operator()(const HashSetBase *x, const HashSetBase *y) {
		return x->getChunkCnt() < y->getChunkCnt();
	}
};

// don't use CHECK_THROW here, since it's an event handler, and throw here will
// bring down the app.
void PartData::onHashEvent(HashWorkPtr p, HashEvent evt) {
	p->getEventTable().delHandlers(p);

	using boost::logic::tribool;
	if (!p->isFull()) {
		Range64 tmp(p->begin(), p->end());
		CMPosIndex::iterator it = m_chunks->find(Chunk(this, tmp, 0));
		CHECK_RET(it != m_chunks->end());
		CHECK_RET((*it).getHash() == p->getRef());
		m_chunks->modify(
			it, boost::bind(&Chunk::onHashEvent, _b1, p, evt)
		);
		return;
	}

	m_fullJob = HashWorkPtr();
	if (evt == HASH_FATAL_ERROR) {
		logError("Fatal error performing final rehash on PartData.");
	} else if (evt != HASH_COMPLETE) {
		logDebug(boost::format(
			"PartData received unknown event %d.") % evt
		);
		return;
	}
	CHECK_RET(p->getMetaData());
	MetaData *ref = p->getMetaData();
	logDebug(
		boost::format("Full metadata generated for file %s (%s)")
		% ref->getName() % m_dest.leaf()
	);

	if (ref->getSize() != m_size) {
		logError(
			boost::format(
				"Completing download %s: File size mismatch, "
				"expected %d (%s), but size on disk is %d (%s)"
			) % getName() % m_size % Utils::bytesToString(m_size)
			% ref->getSize() % Utils::bytesToString(ref->getSize())
		);
		setCorrupt(Range64(0, m_size));
		return;
	}


	//! Generated hashsets, sorted on chunk-count
	std::multiset<const HashSetBase*, ChunkCountPred> generated;
	typedef std::multiset<const HashSetBase*,ChunkCountPred>::iterator Iter;

	for (uint32_t i = 0; i < ref->getHashSetCount(); ++i) {
		generated.insert(ref->getHashSet(i));
	}

	uint16_t ok = 0; uint16_t failed = 0; uint16_t notfound = 0;
	for (Iter i = generated.begin(); i != generated.end(); ++i) {
		const HashSetBase *hs = *i;
		boost::format fmt("Generated hashset: %s %s %s: %s");
		fmt % hs->getFileHashType();
		fmt % (hs->getChunkSize() ? hs->getChunkHashType() : "");
		if (hs->getChunkSize()) {
			fmt % hs->getChunkSize();
		} else {
			fmt % "";
		}
		fmt % hs->getFileHash().decode();
		logDebug(fmt.str());
		for (uint32_t j = 0; j < hs->getChunkCnt(); ++j) {
			boost::format fmt("[%2d] %s");
			logDebug(fmt % j % (*hs)[j].decode());
		}
		boost::logic::tribool ret = verifyHashSet(hs);
		if (ret) {
			++ok;
		} else if (!ret) {
			++failed;
		} else if (boost::indeterminate(ret)) {
			++notfound;
		}
	}

	logDebug(
		boost::format("Final rehash: %d ok, %d failed, %d not found")
		% ok % failed % notfound
	);
	if (isComplete()) {
		onCompleted(this);
	}
}

boost::logic::tribool PartData::verifyHashSet(const HashSetBase *ref) {
	CHECK_THROW(m_md);
	CHECK_THROW(ref);
	for (uint32_t j = 0; j < m_md->getHashSetCount(); ++j) {
		const HashSetBase *orig = m_md->getHashSet(j);
		if (ref->getFileHashTypeId() != orig->getFileHashTypeId()) {
			continue;
		}
		if (ref->getChunkHashTypeId() != orig->getChunkHashTypeId()) {
			continue;
		}
		if (ref->getChunkCnt() != orig->getChunkCnt()) {
			return boost::indeterminate;
		}
		if (ref->getChunkCnt() == orig->getChunkCnt() == 0) {
			if (ref->getFileHash() == orig->getFileHash()) {
				setVerified(Range64(0, m_size - 1));
				return true;
			} else {
				corruption(Range64(0, m_size - 1));
				return false;
			}
		}
		bool ok = true; // if any chunks fail, this is set false
		for (uint64_t i = 0; i < ref->getChunkCnt(); ++i) {
			// last special hash (filesize == X*chunksize)
			// - no checking needed
			if (ref->getChunkSize() * i == m_size) {
				break;
			}
			uint64_t beg = ref->getChunkSize() * i;
			uint64_t end = beg + ref->getChunkSize();
			if (end > m_size - 1) {
				end = m_size - 1;
			}
			if ((*ref)[i] != ((*orig)[i])) {
				boost::format fmt(
					"Final rehash, chunkhash %d..%d failed:"
					" Generated hash %s != real hash %s"
				);
				fmt % beg % end % (*ref)[i].decode();
				fmt % (*orig)[i].decode();
				logError(fmt);
				corruption(Range64(beg, end));
				ok = false;
			} else {
				setVerified(Range64(beg, end));
			}
		}
		return ok;
	}
	return boost::indeterminate;
}

void PartData::setMetaData(MetaData *md) {
	CHECK_THROW(md);
	md->getEventTable().addHandler(md, this, &PartData::onMetaDataEvent);

	m_md = md;
	for (uint32_t i = 0; i < m_md->getHashSetCount(); ++i) {
		HashSetBase *hs = m_md->getHashSet(i);
		if (hs->getChunkSize() && hs->getChunkCnt()) {
			addHashSet(m_md->getHashSet(i));
		}
	}
}

HashWorkPtr PartData::verifyRange(
	Range64 range, const HashBase *ref, bool doSave
) {
	if (doSave) {
		save();
	}
	HashWorkPtr c(
		new HashWork(m_loc.string(), range.begin(), range.end(), ref)
	);
	if (m_allocJob) {
		m_chunkChecks.push_back(c);
	} else {
		IOThread::instance().postWork(c);
	}
	++m_pendingHashes;
	return c;
}

bool PartData::tryComplete() {
	if (isComplete() && !m_fullJob && !m_pendingHashes &&canComplete(this)){
		doComplete();
		return true;
	}
	return false;
}

void PartData::doComplete() {
	CHECK_THROW(isComplete());
	CHECK_RET(!m_fullJob);

	save();
	HashWorkPtr p(new HashWork(m_loc.string()));
	HashWork::getEventTable().addHandler(p, this, &PartData::onHashEvent);
	IOThread::instance().postWork(p);
	getEventTable().postEvent(this, PD_VERIFYING);
	m_fullJob = p;
}

// this is a bit tricky, since we must ensure that when disk is full, we don't
// corrupt anything, not even backup files; thus, we do all operations first on
// temporary files ("_" suffix), and if they were written successfully, rename
// them to the real files.
// care must also be taken to remove all our temporary files ("_" suffix ones)
// when throwing exceptions - we don't want to leave our mess behind
void PartData::save() try {
	using Utils::getFileSize;

	// flush all temporary buffers; this can throw out_of_disk_space as well
	flushBuffer();

	// write into a temporary buffer at first
	std::ostringstream tmp;
	tmp << *this;
	if (m_md) {
		tmp << *m_md;
	}

	// back up existing .dat file; we first move it to .bak_ file, and
	// verify the operation's success before writing over the real existing
	// .bak file.
	boost::filesystem::path p(m_loc.string() + ".dat");
	if (exists(p)) {
		// rename to .bak_
		boost::filesystem::path backup(p.string() + ".bak");
		if (boost::filesystem::exists(backup.string() + "_")) {
			boost::filesystem::remove(backup.string() + "_");
		}
		boost::filesystem::copy_file(p, backup.string() + "_");

		// verify the above operation succeeded
		if (getFileSize(p) != getFileSize(backup.string() + "_")) try {
			throw std::runtime_error("no space left on drive");
		} catch (...) {
			boost::filesystem::remove(backup.string() + "_");
			throw;
		}

		// now remove the real .bak file and replace with the old .dat
		if (boost::filesystem::exists(backup)) {
			boost::filesystem::remove(p.string() + ".bak");
		}
		boost::filesystem::rename(backup.string() + "_", backup);
	}

	// now write a new .dat file, but write to .dat_ first to ensure all
	// data fit on disk properly
	std::ofstream ofs((p.string() + "_").c_str(), std::ios::binary);
	Utils::putVal<std::string>(ofs, tmp.str(), tmp.str().size());
	ofs.flush();
	if (!ofs.good()) {
		boost::filesystem::remove(p.string() + "_");
		throw std::runtime_error("no space left on drive");
	}
	ofs.close();
	// and finally, if the above didn't throw, rename the .dat_ to .dat
	boost::filesystem::remove(p);
	boost::filesystem::rename((p.string() + "_"), p);

	// buffer flush and dat file save successful - check if we need to
	// auto-resume the download
	if (isAutoPaused()) {
		logMsg(
			boost::format("Info: Auto-resuming file '%s'.")
			% getName()
		);
		resume();
	}
} catch (std::exception &e) {
	logError(
		boost::format("Error saving temp file: %s") % e.what()
	);
	if (isRunning()) {
		logMsg(
			boost::format(
				"Info: Auto-pausing file '%s' due "
				"to the above error."
			) % getName()
		);
		autoPause();
	}
}
MSVC_ONLY(;)

std::ostream& operator<<(std::ostream &o, const PartData &p) {
	Utils::putVal<uint8_t>(o, OP_PARTDATA);
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OP_PD_VER);
	Utils::putVal<uint64_t>(tmp, p.m_size);

	Utils::putVal<uint16_t>(tmp, 4); // tagcount

	Utils::putVal<uint8_t>(tmp, OP_PD_DESTINATION);
	Utils::putVal<uint16_t>(tmp, p.m_dest.string().size() + 2);
	Utils::putVal<std::string>(tmp, p.m_dest.string());

	Utils::putVal<uint8_t>(tmp, OP_PD_COMPLETED);
	std::ostringstream tmp2;
	tmp2 << p.m_complete;
	Utils::putVal<std::string>(tmp, tmp2.str());

	Utils::putVal<uint8_t>(tmp, OP_PD_VERIFIED);
	std::ostringstream tmp3;
	tmp3 << p.m_verified;
	Utils::putVal<std::string>(tmp, tmp3.str());

	Utils::putVal<uint8_t>(tmp, OP_PD_STATE);
	Utils::putVal<uint16_t>(tmp, 1);
	if (p.isPaused()) {
		Utils::putVal<uint8_t>(tmp, STATE_PAUSED);
	} else if (p.isStopped()) {
		Utils::putVal<uint8_t>(tmp, STATE_STOPPED);
	} else {
		Utils::putVal<uint8_t>(tmp, STATE_RUNNING);
	}

	Utils::putVal<std::string>(o, tmp.str());

	return o;
}

void PartData::corruption(Range64 r) {
	logWarning(
		boost::format("%s: Corruption found at %d..%d (%s)")
		% getName() % r.begin() % r.end()
		% Utils::bytesToString(r.length())
	);
	setCorrupt(r);
}

uint64_t PartData::getCompleted() const {
	uint64_t tmp = 0;
	for_each(
		m_complete.begin(), m_complete.end(),
		tmp += bind(&Range64::length, __1)
	);
	return tmp;
}

// for debugging purposes only
void PartData::printCompleted() {
	float perc = getCompleted() * 100.0 / getSize();
	static const uint32_t width = 74;
	logTrace(TRACE_PARTDATA,
		boost::format("/%s\\") % std::string(width - 2, '-')
	);
	std::string buf("[          ]");
	for (uint32_t i = 0; i < 10; ++i) {
		if (perc > i * 10 && perc < (i + 1) * 10) {
			uint8_t c = static_cast<uint8_t>(perc) % 10;
			if (c > 0 && c < 4) {
				buf[i + 1] = '.';
			} else if (c >= 4 && c < 7) {
				buf[i + 1] = '-';
			} else if (c >= 7 && c < 10) {
				buf[i + 1] = '|';
			}
		} else if (perc && perc >= i * 10) {
			buf[i + 1] = '#';
		}
	}

	boost::format fmtname("| Name: " COL_GREEN "%s" COL_NONE "%|73t||");
	std::string filename = getDestination().leaf();
	if (filename.size() > width - 10u) {
		std::string beg = filename.substr(0, width - 20);
		std::string end = filename.substr(filename.size() - 4);
		filename = beg + "[...]" + end;
	}
	fmtname % filename;
	logTrace(TRACE_PARTDATA, fmtname);
	boost::format fmt("| Complete: %s %5.2f%% Size: %s / %d bytes%|73t||");
	fmt % buf % perc % Utils::bytesToString(getSize()) % getSize();
	logTrace(TRACE_PARTDATA, fmt);
	for (RangeList64::CIter i = m_complete.begin();i!=m_complete.end();++i){
		boost::format fmt("| Complete range: %d -> %d %|73t||");
		logTrace(TRACE_PARTDATA, fmt % (*i).begin() % (*i).end());
	}
	logTrace(TRACE_PARTDATA,
		boost::format("\\%s/") % std::string(width - 2, '-')
	);
}

void PartData::printChunkStatus() {
	logTrace(TRACE_PARTDATA,
		boost::format("Chunk status for " COL_BCYAN "%s" COL_NONE)
		% getName()
	);
	CMPosIndex &idx = m_chunks->get<ID_Pos>();
	CMPosIndex::iterator it = idx.begin();
	uint32_t num = 0;
	logTrace(TRACE_PARTDATA,
		" num)      begin           end *1 *2 *3  *4  *5 *6"
	);
	while (it != idx.end()) {
		boost::format fmt("%4d) %10d -> %10d %s  %s  %s %2d %2d %s");
		fmt % num % (*it).begin() % (*it).end();
		fmt % ((*it).m_partial ? 'x' : '.');
		fmt % ((*it).m_verified ? 'x' : '.');
		fmt % ((*it).m_complete ? 'x' : '.');
		fmt % (*it).m_avail % (*it).m_useCnt;
		fmt % ((*it).m_hash ? 'x' : '.');
		logTrace(TRACE_PARTDATA, fmt.str());
		++it; ++num;
	}
	logTrace(TRACE_PARTDATA, boost::format(" *1  Partial"));
	logTrace(TRACE_PARTDATA, boost::format(" *2  Verified"));
	logTrace(TRACE_PARTDATA, boost::format(" *3  Complete"));
	logTrace(TRACE_PARTDATA, boost::format(" *4  Availability"));
	logTrace(TRACE_PARTDATA, boost::format(" *5  UseCount"));
	logTrace(TRACE_PARTDATA, boost::format(" *6  Has hash?"));
}

void PartData::destroy() {
	getEventTable().safeDelete(this, PD_DESTROY);
	onDestroyed(this);
}

void PartData::cancel() {
	onCanceled(this);
	deleteFiles();
	destroy();
}

void PartData::deleteFiles() {
	using namespace boost::filesystem;

	if (!m_loc.empty()) {
		if (exists(m_loc)) try {
			remove(m_loc);
		} catch (std::exception &e) {
			logDebug(
				boost::format("Deleting file %s: %s") 
				% m_loc.native_file_string() % e.what()
			);
		}

		path tmp(m_loc.string() + ".dat", native);
		if (exists(tmp)) try {
			remove(tmp);
		} catch (std::exception &e) {
			logDebug(
				boost::format("Deleting file %s: %s") 
				% tmp.native_file_string() % e.what()
			);
		}

		tmp = path(tmp.string() + ".bak", native);
		if (exists(tmp)) try {
			remove(tmp);
		} catch (std::exception &e) {
			logDebug(
				boost::format("Deleting file %s: %s") 
				% tmp.native_file_string() % e.what()
			);
		}
	}
}

void PartData::onMetaDataEvent(MetaData *md, int evt) {
	CHECK_RET(md == m_md);
	if (evt == MD_SIZE_CHANGED) {
		m_size = md->getSize();
		if (m_chunks) {
			m_chunks->clear();
		}
	} else if (evt == MD_NAME_CHANGED) {
		setDestination(getDestination().branch_path() / md->getName());
	}
}

void PartData::addAvail(uint32_t chunkSize, uint32_t index) {
	CMPosIndex& pi = m_chunks->get<ID_Pos>();
	Range64 tmp(chunkSize * index, chunkSize * (index + 1) - 1);
	CMPosIndex::iterator it = pi.find(Chunk(this, tmp, 0));
	if (it != pi.end() && (*it).length() == chunkSize) {
		pi.modify(it, bind(&Chunk::m_avail, __1)++);
		logTrace(TRACE_PARTDATA,
			boost::format("Adding avail to chunk %d..%d")
			% (*it).begin() % (*it).end()
		);
	} else if (it != pi.end()) {
		CMPosIndex::iterator j(it);
		if ((*++j).length() == chunkSize) {
			pi.modify(it, bind(&Chunk::m_avail, __1)++);
		}
	} else if (it != pi.begin()) {
		if ((*--it).length() == chunkSize) {
			pi.modify(it, bind(&Chunk::m_avail, __1)++);
		}
	}
}

void PartData::pause() {
	if (!m_paused) {
		m_paused = true;
		m_stopped = false;
		onPaused(this);
	}
}

void PartData::stop() {
	if (!m_stopped) {
		m_paused = false;
		m_stopped = true;
		onStopped(this);
	}
}

void PartData::resume() {
	if (m_paused || m_stopped) {
		m_paused = false;
		m_stopped = false;
		m_autoPaused = false;
		onResumed(this);
	}
}

void PartData::allocDone(AllocJobPtr job, bool evt) {
	assert(m_allocJob);
	assert(job == m_allocJob);
	if (!evt) {
		logError(
			boost::format(
				"Allocating %s space for file %s failed "
				"- out of disk space?"
			) % Utils::bytesToString(m_size) % getName()
		);
		pause();
		m_autoPaused = true;
	} else if (m_paused && m_autoPaused) {
		// allocation succeeded, but file was previously auto-paused -
		// resume it now
		resume();
	}
	m_allocJob = AllocJobPtr();
	if (evt) {
		// flush buffers after finishing allocation
		save();
		for (uint32_t i = 0; i < m_chunkChecks.size(); ++i) {
			IOThread::instance().postWork(m_chunkChecks[i]);
		}
		m_chunkChecks.clear();
		onAllocDone(this);
	}
}

void PartData::dontDownload(Range64 range) {
	m_dontDownload.merge(range);
}

void PartData::doDownload(Range64 range) {
	m_dontDownload.erase(range);
}

boost::filesystem::path PartData::getLocation() const {
	return m_loc;
}
boost::filesystem::path PartData::getDestination() const {
	return m_dest;
}
std::string PartData::getName() const {
	return m_dest.leaf();
}

void PartData::setDestination(const boost::filesystem::path &p) {
	m_dest = p;
	if (m_md) {
		m_md->setName(m_dest.leaf());
	}
}

uint32_t PartData::amountBuffered() const {
	typedef std::map<uint64_t, std::string>::const_iterator CBIter;
	uint32_t ret = 0;
	for (CBIter i = m_buffer.begin(); i != m_buffer.end(); ++i) {
		ret += (*i).second.size();
	}
	return ret;
}

void PartData::cleanupName() {
	std::string fname = getDestination().leaf();
	for (size_t i = 0; i < fname.size(); ++i) {
		if (
			fname[i] == '\\' || fname[i] == '/' || 
			fname[i] == '*'  || fname[i] == ':' ||
			fname[i] == '"'  || fname[i] == '?' ||
			fname[i] == '<'  || fname[i] == '>' ||
			fname[i] == '|'
		) {
			fname.erase(i, 1);
			--i;
		}
	}
	setDestination(getDestination().branch_path() / fname);
}

void PartData::allocDiskSpace() {
	if (Utils::getFileSize(m_loc) == m_size) {
		return;
	}
	logMsg(
		boost::format("Allocating %d disk space for download %s.") 
		% Utils::bytesToString(m_size) % getName()
	);
	m_allocJob = AllocJobPtr(new AllocJob(m_loc, m_size));
	AllocJob::getEventTable().addHandler(
		m_allocJob, this, &PartData::allocDone
	);
	IOThread::instance().postWork(m_allocJob);
}
