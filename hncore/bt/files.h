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
 * \file files.h      Interface for TorrentFile and PartialTorrent classes
 */

#ifndef __BT_FILES_H__
#define __BT_FILES_H__

#include <hncore/sharedfile.h>
#include <hncore/partdata.h>
#include <hncore/hasher.h>
#include <hncore/bt/types.h>
#include <hncore/bt/torrentinfo.h>

namespace Bt {
class TorrentHasher;

/**
 * TorrentFile is a slightly customized SharedFile type, which keeps an internal
 * list of child objects. Since Torrents can often contain many files, but they
 * must be operated as a single big file on protocol level, this class provides
 * the required interface for reading data from the file. There is no
 * corresponding physical file for this SharedFile, it's merely a virtual
 * file.
 */
class TorrentFile : public SharedFile {
	friend class Torrent;
public:
	/**
	 * Construct TorrentFile from a number of files (and optionally, also
	 * attach a PartialTorrent object, so getPartData() works properly).
	 *
	 * @param files         Files this torrent is composed of, keyed by
	 *                      begin offsets
	 * @param size          Total size of this torrent
	 * @param pt            PartialTorrent (optional)
	 */
	TorrentFile(
		const std::map<uint64_t, SharedFile*> &files,
		TorrentInfo ti, PartialTorrent *pt = 0
	);

	/**
	 * Override base class read() method; forwards the call to the correct
	 * child object(s).
	 *
	 * @param begin     Begin offset to start reading (inclusive)
	 * @param end       End offset to end reading (inclusive)
	 * @return          The read data
	 *
	 * \throws std::exception if reading fails
	 */
	virtual std::string read(uint64_t begin, uint64_t end);

	/**
	 * Find out which sub-file contains the specified range. Note that it is
	 * not defined which file will be returned if the range crosses multiple
	 * files. It's not a big problem though, since the chunk sizes in BT are
	 * rather small, and this is called only by Client for most-recent chunk
	 * request.
	 *
	 * @param range       Range to search for
	 * @returns           Sub-file that contains the specified range
	 */
	SharedFile* getContains(Range64 range) const;

protected:
	/**
	 * Overrides base class method; does nothing, since we don't have
	 * anything to move when completing download.
	 */
	virtual void finishDownload();

private:
	/**
	 * Each sub-file in the torrent allocates a specific range in the
	 * torrent, and are thus kept in a RangeList. This customized Range
	 * object associates a range with a file.
	 */
	struct InternalFile : public Range64 {
		InternalFile(const Range64 &r) : Range64(r), m_file() {}
		InternalFile(uint64_t begin, uint64_t size, SharedFile *f);
		InternalFile(uint64_t beg, uint64_t end)
		: Range64(beg, end), m_file() {}

		SharedFile *m_file;
	};

	//! Children
	RangeList<InternalFile> m_children;

	//! file -> offset reverse lookups
	std::map<SharedFile*, uint64_t> m_childrenReverse;
	typedef std::map<SharedFile*, uint64_t>::iterator RIter;

	/**
	 * \name Event/signal handlers
	 */
	//!@{
	void onSharedFileEvent(SharedFile *file, int evt);
	//!@}

	//! Destructor is only allowed by Torrent class
	~TorrentFile();
};

/**
 * PartialTorrent is a similar object as TorrentFile, with the difference of
 * being a virtual wrapper around a number of PartData objects. It overrides
 * a number of PartData virtual functions to forward the calls to coresponding
 * sub-objects, based on offsets.
 */
class PartialTorrent : public PartData {
	friend class Torrent;
public:
	/**
	 * Construct PartialTorrent from a list of PartData objects
	 *
	 * @param files       Objects to construct from, keyed by begin offsets
	 * @param loc         Location where to store cache files
	 * @param size        Total size of this torrent
	 */
	PartialTorrent(
		const std::map<uint64_t, PartData*> &files,
		const boost::filesystem::path &loc, uint64_t size
	);

	/**
	 * Initiates Cache files and cached ranges listing.
	 *
	 * @param info        Torrent info used for reference data
	 */
	void initCache(const TorrentInfo &info);

	/**
	 * Overriding base class save() method, this omits writing the .dat file
	 * for the torrent (since it doesn't have one), and saves the cache
	 * files.
	 */
	virtual void save();

	/**
	 * \name Overide few more base class functions
	 */
	//!@{
	virtual void pause();
	virtual void stop();
	virtual void resume();
	virtual void cancel();
	virtual void allocDiskSpace();
	virtual std::string getName() const { return m_name; }
	//!@}

	/**
	 * Find out which sub-file contains the specified range. Note that it is
	 * not defined which file will be returned if the range crosses multiple
	 * files. It's not a big problem though, since the chunk sizes in BT are
	 * rather small, and this is called only by Client for most-recent chunk
	 * request.
	 *
	 * @param range       Range to search for
	 * @returns           Sub-file that contains the specified range
	 */
	PartData* getContains(Range64 range) const;

	/**
	 * Sets the name of this file. Since the torrent consists of many files,
	 * this must be set externally by the creator of the download.
	 *
	 * \note initCache() function sets this as well (if not already set),
	 *       based on the passed TorrentInfo name.
	 *
	 * @param name      Name to be set.
	 */
	void setName(const std::string &name) { m_name = name; }
protected:
	/**
	 * Write data to the file
	 *
	 * @param begin         Begin offset to start writing at
	 * @param data          Data to be written
	 *
	 * \throws std::exception if something goes wrong
	 */
	virtual void doWrite(uint64_t begin, const std::string &data);

	/**
	 * Verify data at the specified range
	 *
	 * @param range        Range to be verified
	 * @param ref          Reference hash to check against
	 * @returns            The work object posted to workthread
	 */
	virtual HashWorkPtr verifyRange(
		Range64 range, const HashBase *ref, bool doSave = true
	);

	/**
	 * Called by Detail::Chunk implementation class, indicates that the
	 * specified region of file didn't pass a hash check.
	 *
	 * @param r            Corrupt range
	 */
	virtual void corruption(Range64 r);
private:
	/**
	 * Each sub-file in the torrent allocates a specific range in the
	 * torrent, and are thus kept in a RangeList. This customized Range
	 * object associates a range with a file.
	 */
	struct InternalFile : public Range64 {
		InternalFile(const Range64 &r) : Range64(r), m_file() {}
		InternalFile(uint64_t beg, uint64_t end)
		: Range64(beg, end), m_file() {}

		InternalFile(uint64_t begin, uint64_t size, PartData *f);

		PartData *m_file;
	};

	/**
	 * Implements CacheFile; refer to CacheFile class for documentation.
	 */
	struct CacheImpl : public Range64 {
		CacheImpl(
			uint64_t begin, uint64_t end,
			const boost::filesystem::path &p
		);

		/**
		 * Writes data to this cache file
		 *
		 * @param begin    Relative offset inside this file
		 * @param data     Data to be written
		 */
		void write(uint64_t begin, const std::string &data);

		/**
		 * Flushes buffer to disk
		 */
		void save();

		//! @returns the physical location for the cache data
		boost::filesystem::path getLocation() const { return m_loc; }
	private:
		//! Physical location on disk
		boost::filesystem::path m_loc;
		//! Data buffer for non-flushed data
		std::map<uint64_t, std::string> m_buffer;
		//! Makes life simpler
		typedef std::map<uint64_t, std::string>::iterator BIter;
	};

	/**
	 * CacheFile represents a part of a chunk of a file that crosses file
	 * boundaries. Each file in the chunk has it's own CacheFile object.
	 * Each CacheFile is stored in disk as separate file, to allow
	 * TorrentHasher to work seamlessly across "real" and "cached" files.
	 *
	 * The purpose of this mechanism is to handle gaps in the torrent
	 * (files missing/canceled) cleanly - we always have the cache which
	 * we can read the missing data in order to perform cross-file hashes.
	 *
	 * The actual machinery is implemented in CacheImpl class, since this
	 * object is held on stack, and stored in m_cache RangeList.
	 */
	struct CacheFile : public Range64 {
		CacheFile(uint64_t r) : Range64(r), m_impl() {}
		CacheFile(const Range64 &r) : Range64(r), m_impl() {}
		CacheFile(uint64_t beg, uint64_t end)
		: Range64(beg, end), m_impl() {}
		CacheFile(
			uint64_t begin, uint64_t end,
			const boost::filesystem::path &p
		) : Range64(begin, end), m_impl(new CacheImpl(begin, end, p)) {}

		boost::filesystem::path getLocation() const {
			return m_impl->getLocation();
		}

		boost::shared_ptr<CacheImpl> m_impl;
	};

	//! Cache for all data chunks that cross file boundaries
	RangeList<CacheFile> m_cache;

	//! Children
	RangeList<InternalFile> m_children;

	//! Map of children, keyed by object, for reverse lookups
	std::map<PartData*, uint64_t> m_childrenReverse;

	//! Pending hash jobs, waiting for allocations to finish
	std::list<boost::intrusive_ptr<TorrentHasher> > m_pendingChecks;

	/**
	 * This is true when we'r in doWrite() call, false otherwise. The
	 * rationale is that in childDataAdded signal handler we call
	 * setComplete, but we want to call setComplete from doWrite() when
	 * writing from this file. The fundamental problem is actually the
	 * need to call setComplete from updateCache, which is needed since
	 * cache writes don't trigger the childDataAdded signal.
	 *
	 * Thus, the current behaviour is to set this variable to true while
	 * we'r in doWrite method, so childDataAdded signal handler won't call
	 * setComplete. When doWrite finishes, it calls setComplete itself.
	 */
	bool m_writing;

	//! Name of this download
	std::string m_name;

	/**
	 * @name Iterators for easier usage
	 */
	//!@{
	typedef std::map<PartData*, uint64_t>::iterator RIter;
	//!@}

	//! Destruction only allowed by Torrent class
	~PartialTorrent();

	/**
	 * @name Signal handlers
	 */
	//!@{
	void childDataAdded(PartData *file, uint64_t offset, uint32_t amount);
	void childCorruption(PartData *file, Range64 range);
	bool childCanComplete(PartData *file);
	void childChunkVerified(PartData *file, uint64_t csz, uint64_t chunk);
	void childAllocDone(PartData *file);
	void childPaused(PartData *file);
	void childResumed(PartData *file);
	void childDestroyed(PartData *file);
	void parentChunkVerified(PartData *file, uint64_t csz, uint64_t chunk);
	//!@}

	/**
	 * Updates cache of cross-file chunks
	 *
	 * @param begin      Global begin offset of input data
	 * @param data       Data that was just written
	 */
	void updateCache(uint64_t begin, const std::string &data);

	/**
	 * Destroys all temporary 'cache' files.
	 */
	void cleanCache(bool delTorrent);

	/**
	 * Event handler for cache checksum jobs
	 */
	void onCacheVerify(HashWorkPtr wrk, HashEvent evt);
};

/**
 * Customized hasher for torrent files. Since chunks can cross file boundaries
 * in torrents, we need to read data possibly from arbitary number of files,
 * and concatenate the data together.
 */
class TorrentHasher : public HashWork {
public:
	/**
	 * Construct custom hasher
	 *
	 * @param globalOffsets     Global offsets (relative to torrent file)
	 * @param files             Files from which to read data
	 * @param relativeOffsets   Relative offsets to first/last file being
	 *                          hashed
	 * @param ref               Reference hash
	 */
	TorrentHasher(
		Range64 globalOffsets,
		const std::vector<boost::filesystem::path> &files,
		std::pair<uint64_t, uint64_t> relativeOffsets,
		const HashBase *ref
	);

	//! Dummy destructor
	~TorrentHasher();

	/**
	 * Set the list of files that we'r waiting for allocation to finish
	 * before running this job.
	 *
	 * @param wait     List of files to wait on
	 */
	void waitAlloc(const std::set<PartData*> &wait) { m_waiting = wait; }

	/**
	 * Indicate that an allocation job has finished
	 *
	 * @param f        The file that finished allocating
	 */
	void allocDone(PartData *f) { m_waiting.erase(f); }

	/**
	 * @returns true if this job can be run now (no allocations pending)
	 */
	bool canRun() const { return !m_waiting.size(); }
protected:
	/**
	 * Read next data from file
	 *
	 * @param pos      Current position in file
	 * @return         Number of bytes read
	 */
	virtual uint64_t readNext(uint64_t pos);

	/**
	 * Called when hash work has been finished. Adjusts begin() / end()
	 * variables to be global offsets (from m_globalOffsets member), since
	 * DURING hashing, we need relative offsets, but PartData needs global
	 * offsets in order to find the chunk that needed this hash job, thus
	 * the modification must be done prior to the events being posted.
	 */
	virtual void finish();
private:
	//! Files for this work
	std::vector<boost::filesystem::path> m_files;

	//! Iterator for the above vector
	typedef std::vector<boost::filesystem::path>::iterator Iter;

	//! Current file being hashed
	Iter m_curFile;

	//! Keeps the global offsets (relative to torrent start)
	Range64 m_globalOffsets;

	//! File allocations we'r waiting for before we can run this job
	std::set<PartData*> m_waiting;
};

}

#endif
