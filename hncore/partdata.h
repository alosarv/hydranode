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
 * \file partdata.h Interface for PartData class
 */

#ifndef __PARTDATA_H__
#define __PARTDATA_H__

#include <hnbase/osdep.h>
#include <hnbase/rangelist.h>
#include <hnbase/event.h>
#include <hnbase/object.h>

#include <hncore/fwd.h>

#include <boost/filesystem/path.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <map>
#include <list>

/**
 * Events emitted from PartData object
 */
enum PDEvent {
	PD_ADDED = 1,   //!< Posted whenever PartData is constructed
	PD_DATA_ADDED,  //!< Posted whenever data is added to PartData
	PD_DATA_FLUSHED,//!< Posted whenever a partdata flushes buffers
	PD_DESTROY,     //!< Posted whenever a partdata is about to be destroyed
	PD_VERIFYING,   //!< When file is complete, but in verification phase
	PD_MOVING,      //!< When file is verified, and moving is in progress
	PD_COMPLETE,    //!< File has been completed and will be moved shortly
	PD_DL_FINISHED, //!< File has been moved to incoming and finished
	PD_CANCELED,    //!< The download was canceled
	PD_PAUSED,      //!< The download was paused
	PD_STOPPED,     //!< The download was stopped
	PD_RESUMED      //!< The download was resumed
};


namespace CGComm {
	//! Opcodes used within PartData object I/O between streams
	enum PartDataOpCodes {
		OP_PD_VER         = 0x01,  //!< uint8  File version
		OP_PARTDATA       = 0x90,  //!< uint8  PartData object
		OP_PD_DOWNLOADED  = 0x91,  //!< uint64 Downloaded data
		OP_PD_DESTINATION = 0x92,  //!< string Destination location
		OP_PD_COMPLETED   = 0x93,  //!< RangeList64 completed ranges
		OP_PD_HASHSET     = 0x94,  //!< RangeList<HashBase*> hashset
		OP_PD_STATE       = 0x95,  //!< Download state
		OP_PD_VERIFIED    = 0x96   //!< RangeList64 verified ranges
	};
	//! State of a download
	enum DownloadState {
		STATE_RUNNING = 0, //!< Active download
		STATE_PAUSED  = 1, //!< Paused (inactive) download
		STATE_STOPPED = 2  //!< Stopped (disabled) download
	};
}

namespace Detail {
	struct ChunkMap;
	struct PosIter;
	class Chunk;
	class AllocJob;
	typedef boost::intrusive_ptr<AllocJob> AllocJobPtr;
}

/**
 * PartData object encapsulates all details of download handling, including
 * keeping track of downloaded parts, availability chunk-maps, coo-ordinating
 * the chunks downloading between multiple plugins (in order to achieve highest
 * effectivness), and so on.
 *
 * PartData object should never be created/used separately. Instead, use
 * FilesList::createDownload() method to have the download created for you, and
 * then look it up from FilesList to aquire the corresponding SharedFile and
 * PartData objects.
 *
 * For each PartData object, there must be a corresponding SharedFile object.
 *
 * If MetaData object corresponding to this object (m_metaData member) includes
 * any chunk or file-hashes, PartData will recognize them, and verify all
 * downloaded data against those hashes. If those hashes fail, PartData will
 * clear the corrupt data.
 *
 * Once PartData detects that the entire file has been downloaded, and final
 * rehash (against all known file-hashes) succeeds, it will emit PD_COMPLETE
 * event, which is handled by SharedFile, which in turn moves the complete
 * file to it's destination directory.
 */
class HNCORE_EXPORT PartData : public Object, public Trackable {
public:
	DECLARE_EVENT_TABLE(PartData*, int);
private:
	/**
	 * \brief Construct a NEW temporary file
	 *
	 * Using this constructor, a new temporary file is constructed, at
	 * specified location with specified size.
	 *
	 * @param size      Size of the resulting file.
	 * @param loc       Location on disk where to store the temp file
	 * @param dest      Destination where to write the complete file
	 *
	 * \note The disk space indicated by @param size is not allocated on
	 *       actual disk right away. Instead, the size is allocated
	 *       dynamically as the file grows. This can be changed from global
	 *       application preferences though.
	 */
	PartData(
		uint64_t size,
		const boost::filesystem::path &loc,
		const boost::filesystem::path &dest
	);

	/**
	 * \brief Load a previously constructed temporary file.
	 *
	 * This method can be used to resume a previously started download, by
	 * reading the necessery data from @param loc.
	 *
	 * @param loc    Path to PartData reference file, which contains the
	 *               data required to resume the download.
	 */
	PartData(const boost::filesystem::path &loc);

	/**
	 * Used when loading temp files from disk, checks the file dates and
	 * rehashes completed parts if needed. This is not called directly from
	 * the loading constructor since SharedFile constructor can replace our
	 * MetaData object, which would result in HashBase pointers in Chunk
	 * become invalid (they refer to MetaData that would then be deleted).
	 */
	void init();
public:
	/**
	 * Used for imported downloads, this constructs a new download that
	 * has already been downloaded (possibly by another client), and
	 * imported via FilesList::import() signal.
	 *
	 * @param loc   Location of actual file data
	 * @param md    Reference MetaData for this file
	 */
	PartData(const boost::filesystem::path &loc, MetaData *md);

	/**
	 * \brief Add an availability chunk mask
	 *
	 * Allows modules to register chunk availability maps, so PartData
	 * can decide the lowest-available chunk to be returned from get*
	 * methods
	 *
	 * @param chunkSize     Size of one chunk
	 * @param chunks        Boolean vector, where each true indicates
	 *                      the source having the part, and false the source
	 *                      not having the part.
	 */
	void addSourceMask(uint64_t chunkSize, const std::vector<bool> &chunks);

	/**
	 * \brief Optimized version of addSourceMask(), adds a full source.
	 *
	 * Similar to addSourceMask(), this adds a source which has the entire
	 * file.
	 *
	 * @param chunkSize    Size of one chunk
	 */
	void addFullSource(uint64_t chunkSize);

	/**
	 * \brief Remove an availability chunk mask
	 * \see addSourceMask
	 */
	void delSourceMask(uint64_t chunkSize, const std::vector<bool> &chunks);

	/**
	 * \brief Remove a full source mask
	 * \see addFullSource
	 */
	void delFullSource(uint64_t chunkSize);

	/**
	 * \brief Locates a range that PartData considers most important.
	 *
	 * The current implementation considers partially completed ranges top
	 * priority, followed by rarest chunks, and then lowest used chunks.
	 *
	 * @param size      Optional, size of the range to be aquired.
	 * @return          Pointer to a range marked as 'used', or null
	 *
	 * \note The size of the given range my be smaller than was requested.
	 * \note Current implementation ignores the size parameter.
	 */
	Detail::UsedRangePtr getRange(uint32_t size = 0);

	/**
	 * \brief Locates a range which is also contained in passed chunkmap.
	 *
	 * This method restricts getRange() call to only chunks which are
	 * indicated by a true value in the passed chunkmap (e.g. partial
	 * sources).
	 *
	 * @param size      Size of a chunk in the chunkmap
	 * @param chunks    The chunks the source has
	 * @return          Pointer to a range marked as 'used', or null
	 */
	Detail::UsedRangePtr getRange(
		uint32_t size, const std::vector<bool> &chunks
	);

	/**
	 * \brief Simply writes data starting at specified offset.
	 *
	 * Checks will be performed to ensure the validity of the location
	 * and that it's not already complete or locked.
	 */
	void write(uint64_t beginOffset, const std::string &data);

	/**
	 * \name Check for completeness.
	 * Methods to check whether the entire file, or a part of it, is
	 * complete.
	 */
	//!@{
	bool isComplete() const {
		return m_complete.size() == 1 &&
			!(*m_complete.begin()).begin() &&
			(*m_complete.begin()).end() == m_size - 1;
	}
	bool isComplete(const Range64 &r) const {
		return m_complete.containsFull(r);
	}
	bool isComplete(uint64_t begin, uint64_t end) const {
		return m_complete.containsFull(begin, end);
	}
	//!@}

	/**
	 * \name Generic accessors
	 */
	//!@{
	uint64_t  getChunkCount(uint64_t chunkSize) const;
	MetaData* getMetaData() const { return m_md; }
	void      setMetaData(MetaData *md);
	void      setDestination(const boost::filesystem::path &p);
	uint64_t  getSize() const { return m_size; }
	uint64_t  getCompleted() const; //!< Returns number of bytes completed
	uint32_t  getFullSourceCnt() const { return m_fullSourceCnt; }
	virtual boost::filesystem::path getLocation() const;
	virtual boost::filesystem::path getDestination() const;
	virtual std::string getName() const;

	std::vector<bool> getPartStatus(uint32_t chunkSize) const {
		CHECK_THROW(m_partStatus.find(chunkSize) != m_partStatus.end());
		return (*m_partStatus.find(chunkSize)).second;
	}
	RangeList64 getCompletedRanges() const { return m_complete; }
	RangeList64 getVerifiedRanges()  const { return m_verified; }
	bool allocInProgress() const { return m_allocJob; }
	Detail::ChunkMap& getChunks() const { return *m_chunks; }
	//!@}

	/**
	 * Saves the current state of this file to m_loc.dat file.
	 */
	virtual void save();

	/**
	 * \brief Adds a hashset with chunkhashes which to test downloaded data
	 * against.
	 *
	 * @param hs      Hashset
	 *
	 * \pre hs->getChunkCount() > 0
	 * \pre hs->getPartSize() > 0
	 */
	void addHashSet(const HashSetBase *hs);

	/**
	 * Add availability for a specific chunk, indicated by chunksize and
	 * index.
	 *
	 * @param chunkSize     Chunk size of the corresponding hashset/chunkmap
	 * @param index         Index of the chunk to add availability
	 */
	void addAvail(uint32_t chunkSize, uint32_t index);

	/**
	 * Attempt to complete this file
	 *
	 * @returns true if completition process has been started, false
	 *          otherwise
	 */
	bool tryComplete();

	//! Exception class
	struct HNCORE_EXPORT LockError : public std::runtime_error {
		LockError(const std::string&);
	};
	//! Exception class
	struct HNCORE_EXPORT RangeError : public std::runtime_error {
		RangeError(const std::string&);
	};

	//! Output operator to streams
	friend std::ostream& operator<<(std::ostream &o, const PartData &p);

	//! Returns number of sources
	boost::signal<uint32_t (), Utils::Sum<uint32_t> > getSourceCnt;

	/**
	 * Emitted whenever data has been written to this file
	 *
	 * @param PartData*    "this" pointer
	 * @param uint64_t     Begin offset where data was written
	 * @param uint32_t     Length of data that was written
	 */
	boost::signal<void (PartData*, uint64_t, uint32_t)> dataAdded;

	/**
	 * Emitted whenever corruption has been found
	 *
	 * @param PartData*     "this" pointer
	 * @param Range64       Range that was corrupted
	 */
	boost::signal<void (PartData*, Range64)> onCorruption;

	/**
	 * Emitted when *this is detected to be complete; if any of the slots
	 * return false, the completition is not done, but must be done via
	 * explicit call to doComplete(). This allows plugins to delay file
	 * completititon (e.g. in BT case, where the file data may not be
	 * verified yet).
	 */
	boost::signal<bool (PartData*), Utils::BoolCheck> canComplete;

	/**
	 * Emitted when a chunk is verified.
	 *
	 * @param PartData*      This pointer
	 * @param uint64_t       Size of the specific chunkmap
	 * @param uint64_t       Index of the chunk that was verified
	 */
	boost::signal<void (PartData*, uint64_t, uint64_t)> onVerified;

	/**
	 * @returns The current download speed of this file.
	 *
	 * Plugins should connect their socket's SpeedMeter to the currently-
	 * downloaded file while the transfer is in progress.
	 */
	boost::signal<uint32_t (), Utils::Sum<uint32_t> > getDownSpeed;

	//! @name Additional signals, duplicating the events
	//!@{
	boost::signal<void (PartData*)> onPaused;
	boost::signal<void (PartData*)> onStopped;
	boost::signal<void (PartData*)> onResumed;
	boost::signal<void (PartData*)> onCompleted;
	boost::signal<void (PartData*)> onDestroyed;
	boost::signal<void (PartData*)> onCanceled;
	boost::signal<void (PartData*)> onAllocDone;
	//!@}

	/**
	 * Add a source to this file
	 *
	 * @param PartData*      File to add source to
	 * @param std::string    Source to be added
	 */
	boost::signal<void (PartData*, const std::string&)> addSource;

	/**
	 * Get all links for a specific download. The actual links are plugin-
	 * dependant text strings.
	 */
	boost::signal<void (PartData*, std::vector<std::string>&)> getLinks;

	/**
	 * Indicates that the range is corrupt, and should be marked as such.
	 *
	 * @param r            Corrupt range
	 */
	virtual void corruption(Range64 r);

	/**
	 * \brief Cancels this download, discarding ALL downloaded data.
	 */
	virtual void cancel();

	/**
	 * Pausing means all existing sources should be kept, but active
	 * searching for new sources shouldn't be done, and no data should
	 * be written to the file. std::runtime_error exception shall be
	 * generated upon attempts to write into a paused download.
	 *
	 * PD_PAUSED event shall be emitted upon call to this function.
	 *
	 * \note If the file is already paused, this function has no effect.
	 */
	virtual void pause();

	/**
	 * Stopping means all existing sources should be dropped, and no data
	 * should be written to the file from here-on. std::runtime_error
	 * exception shall be generated upon attempts to write into a stopped
	 * download.
	 *
	 * PD_STOPPED event shall be emitted upon call to this function.
	 *
	 * \note If the file is already stopped, this function has no effect.
	 */
	virtual void stop();

	/**
	 * Resume the download.
	 *
	 * PD_RESUMED event shall be emitted upon call to this function.
	 *
	 * \note If the file isn't paused/stopped, this function has no effect.
	 */
	virtual void resume();

	/**
	 * Event handler for various hasher events. This is public to allow
	 * overriding the default hashing scheme, and thus set up this function
	 * as event handler (can't take address of a private/protected method).
	 *
	 * This function should never be called directly.
	 *
	 * @param p        Hash work that generated the event
	 * @param evt      The event that was emitted
	 */
	void onHashEvent(HashWorkPtr p, HashEvent evt);

	/**
	 * @name State queries
	 */
	//!@{
	bool isPaused()  const { return m_paused; }
	bool isStopped() const { return m_stopped; }
	bool isRunning() const { return !isPaused() && !isStopped(); }
	//!@}

	/**
	 * Explicitly marks the specified range as "complete", without any
	 * further checks. This is needed to allow plugins implement some custom
	 * behaviour, such as external corruption-recovery (e.g. ed2k AICH).
	 *
	 * Use with care!
	 *
	 * @param range      Range to be marked "complete".
	 */
	void setComplete(Range64 range);

	/**
	 * Marks a range in the file to be never downloaded; useful for virtual
	 * files, e.g. torrents, to indicate some sub-files should never be
	 * downloaded.
	 *
	 * @param range       Range to be marked "dont download"
	 *
	 * \note This setting is not saved to disk, so must be re-set every
	 * startup.
	 */
	void dontDownload(Range64 range);

	/**
	 * Reverse effect than the above function, this indicates that a range
	 * that was formerly marked as "dont download" via dontDownload() method
	 * should now be downloaded again.
	 *
	 * @param range        Range to be marked as "downloadable" again.
	 */
	void doDownload(Range64 range);

	/**
	 * Check if it is allowed to download any part of the specified range
	 *
	 * @param range       Range to check
	 * @returns           True if some part of the range is "downloadable"
	 */
	bool canDownloadSome(Range64 range) {
		return !m_dontDownload.containsFull(range);
	}

	/**
	 * Check if it is allowed to download all of the specified range
	 *
	 * @param range       Range to check
	 * @returns           True if it's allowed to download all of the range
	 */
	bool canDownloadFull(Range64 range) {
		return !m_dontDownload.contains(range);
	}

	/**
	 * Check if a range has been verified already.
	 *
	 * @param range       Range to be checked
	 * @returns           True if all of the range has been verified.
	 */
	bool isVerified(Range64 range) {
		return m_verified.containsFull(range);
	}

	/**
	 * Explicitly marks a range to be corrupt. The range is also erased from
	 * m_complete map, and onCorruption() signal emitted.
	 *
	 * @param range       Range to be marked corrupt.
	 */
	void setCorrupt(Range64 range);

	/**
	 * Explicitly marks a range as "verified". The range is also merged to
	 * m_complete map, and erased from m_corrupt map, if needed.
	 *
	 * @param range      Range to be marked as verified
	 */
	void setVerified(Range64 range);

	/**
	 * Check if there is any buffered data to be flushed to disk. This can
	 * be used to skip saving files which don't have anything buffered.
	 *
	 * @returns true if there is anything buffered.
	 */
	bool hasBuffered() const { return m_buffer.size(); }

	/**
	 * @returns Amount of data currently in this file's buffers
	 */
	uint32_t amountBuffered() const;

	/**
	 * Allocates all neccesery disk space for this file. If the file already
	 * has all it's space allocated, this function doesn't do anything.
	 *
	 * Note that PartData automatically allocates disk space for itself during
	 * first buffer flush.
	 */
	virtual void allocDiskSpace();

	/**
	 * Rehashes all chunks (event incomplete ones, despite the name of the
	 * function). This is used by PartData internally when importing alien
	 * temp files, but can be called explicitly as well if needed.
	 */
	void rehashCompleted();
protected:
	/**
	 * Exposing default constructor in protected interface to allow
	 * maximum flexibility by derived classes.
	 */
	PartData();

	/**
	 * Allowed by friends and derived classes
	 */
	virtual ~PartData();

	/**
	 * Performs actual writing; can be overridden by derived classes
	 *
	 * @param begin         Begin offset to write data
	 * @param data          Data to be written
	 */
	virtual void doWrite(uint64_t begin, const std::string &data);

	/**
	 * Request verification (hashing) of a data range in the file
	 *
	 * @param range         Range to be verified
	 * @param ref           Reference hash to check against
	 * @param doSave        Whether to call save() before verification
	 *
	 * \note The last argument is used as optimization, in cases where we
	 *       want to verify a LARGE number of ranges in a row, e.g. when
	 *       rehashing all completed chunks, and in that case, save() calls
	 *       for each chunk (possibly thousands of chunks) would cause way
	 *       too high delay.
	 */
	virtual HashWorkPtr verifyRange(
		Range64 range, const HashBase *ref, bool doSave = true
	);

	void destroy(); // emits PD_DESTROY Event
	void autoPause()    { pause(); m_autoPaused = true; }
	bool isAutoPaused() { return m_autoPaused; }

	uint64_t m_size;                 //!< Size of this file
	boost::filesystem::path m_loc;   //!< Current location of this file
	boost::filesystem::path m_dest;  //!< Current destination for this file

private:
	friend class SharedFile;
	friend class FilesList;
	friend class Detail::UsedRange;
	friend class Detail::LockedRange;
	friend class Detail::Chunk;

	// these are part of test-suite and need to be friends
	#ifdef TEST_PARTDATA
	friend int test_main(int, char[]);
	friend void onHashEvent(HashWorkPtr p, HashEvent);
	#endif

	//! Copying part files is not allowed
	PartData(const PartData&);
	PartData& operator=(const PartData&);

	/**
	 * All signals must be called for the first time from the library they
	 * were created in, in this case, hncore. This method calls all signals
	 * declared in PartData API, and performs other related initialization.
	 */
	void initSignals();

	/**
	 * \short Aquire lock on a subrange of UsedRange.
	 *
	 * @param r      UsedRange to aquire lock within
	 * @param size   Upper limit on the size of the LockedRange requested.
	 * @return       Locked range, or null on failure
	 *
	 */
	Detail::LockedRangePtr getLock(Detail::UsedRangePtr r, uint32_t size);

	/**
	 * Check if a lock can be aquired within the passed range. This method
	 * essencially behaves like getLock() method, but is faster, since no
	 * LockedRange object is never constructed, and it also doesn't require
	 * passing UsedRange pointer, so it's considerably faster. Used by
	 * getRange() method while looping.
	 *
	 * @param r         Range to check
	 * @param size      Preferred size of the lock
	 * @returns         True if lock can be aquired within the passed range
	 */
	bool canLock(const Range64 &r, uint32_t size = 1) const;

	/**
	 * \name Main rangelists.
	 *
	 * These lists are exclusive, no Range may exist simultaneously in
	 * multiple of these lists. Also, none of these lists may contain
	 * overlapping ranges.
	 */
	//! @{
	RangeList64 m_complete;     //!< Complete ranges
	RangeList64 m_locked;       //!< Locked ranges
	RangeList64 m_corrupt;      //!< Corrupt ranges
	RangeList64 m_verified;     //!< Verified ranges
	RangeList64 m_dontDownload; //!< Ranges that should never be downloaded
	//! @}

	/**
	 * \name Implementation functions
	 */
	//!@{
	void checkAddChunkMap(uint64_t chunkSize);
	template<typename Predicate>
	Detail::UsedRangePtr getNextChunk(uint64_t size, Predicate &pred);
	template<typename Predicate>
	Detail::UsedRangePtr doGetRange(uint64_t size, Predicate &pred);
	void flushBuffer();
	boost::logic::tribool verifyHashSet(const HashSetBase *hs);
	void deleteFiles(); // delete physical files refering to this temp file
	void onMetaDataEvent(MetaData *src, int evt);
	void allocDone(Detail::AllocJobPtr job, bool evt);
	void updateChunks(Range64 range);
	void cleanupName();

	/**
	 * Perform file completititon, that is hashing the entire file, checking
	 * those hashes against what we have on record, if and they match, move
	 * to location stored in m_dest.
	 */
	void doComplete();
	//!@}

	/**
	 * \name Implementation data members
	 */
	//!@{
	boost::scoped_ptr<Detail::ChunkMap> m_chunks;
	std::map<uint64_t, std::string> m_buffer;
	typedef std::map<uint64_t, std::string>::iterator BIter;
	uint32_t m_toFlush;
	MetaData *m_md;
	uint16_t m_pendingHashes;
	//! Pointer to full rehash job (if any) in progress, used for canceling
	//! full rehash in case a chunkhash fails while this is in progress.
	HashWorkPtr m_fullJob;
	//! Pointer to allocation job (if any) in progress
	Detail::AllocJobPtr m_allocJob;
	uint32_t m_sourceCnt;
	uint32_t m_fullSourceCnt;
	//! Caches complete chunks boolmaps, for faster usage by modules
	std::map<uint32_t, std::vector<bool> > m_partStatus;
	//! Paused/stopped status
	bool m_paused, m_stopped, m_autoPaused;
	//! When file allocation is in progress, new completed chunk hash
	//! jobs are buffered here, and submitted to Hasher after allocation
	//! succeeds.
	std::vector<HashWorkPtr> m_chunkChecks;
	//!}
public:
	//! For testing purposes only
	void printCompleted();
	void printChunkStatus();
};

namespace Detail {
	/**
	 * \brief Range marked as "in use".
	 *
	 * UsedRange concept is similar to many thread libraries lock object
	 * concepts - you retrieve one via get() methods in PartData, and when
	 * it is destroyed, it takes care that all used/locked ranges do get
	 * freed properly. This object may only be used when wrapped in
	 * boost::shared_ptr.
	 *
	 * \note There may be multiple UsedRange's refering to same Chunk.
	 *       This is indicated by m_useCnt member of Chunk class.
	 */
	class HNCORE_EXPORT UsedRange :
		public Range64,
		public boost::enable_shared_from_this<UsedRange>
	{
	public:
		/**
		 * \brief Aquire a lock within this UsedRange
		 *
		 * @param size     Size of the requested lock
		 * @return         Locked range object, or null on failure
		 *
		 * \note The returned locked range may be smaller than requested
		 */
		boost::shared_ptr<LockedRange> getLock(uint32_t size);

		/**
		 * Check this UsedRange for completeness
		 */
		bool isComplete() const;

		//! Destructor public for boost::checked_deleter
		~UsedRange();
	private:
		friend class ::PartData;

		/**
		 * \brief Constructor
		 *
		 * Allowed only by PartData. UsedRange keeps a pointer back to
		 * its parent object, and also sets up event handers as
		 * neccesery to ensure the pointer remains valid.
		 *
		 * \note The template may be left undefined here since it's
		 *       only called from inside partdata.cpp
		 */
		template<typename IterType>
		UsedRange(PartData *parent, IterType it);

		/**
		 * Constructs new UsedRange with specified begin and end
		 * offsets, w/o associating it with any specific chunk.
		 */
		UsedRange(PartData *parent, uint64_t begin, uint64_t end);

		//! copying is not allowed
		UsedRange(const UsedRange&);
		UsedRange& operator=(const UsedRange&);

		//! Parent PartData
		PartData *m_parent;

		//! Chunk this UsedRange refers to
		boost::scoped_ptr<Detail::PosIter> m_chunk;
	};

	/**
	 * \brief LockedRange object is an exclusivly locked Range in PartData.
	 *
	 * The lock is aquired upon call to UsedRange::getLock() method, which
	 * constructs the lock object and returns to client code. The indicated
	 * range in PartData is then exclusivly locked, with this object being
	 * the only one allowed to access the locked region of the file. Upon
	 * the destruction of this object, the lock is automatically freed.
	 */
	class HNCORE_EXPORT LockedRange : public Range64 {
	public:
		//! Destructor public for boost::checked_deleter
		~LockedRange();

		/**
		 * \brief Write data to within this locked region.
		 *
		 * @param beginOffset     Begin offset where to write data
		 * @param data            Data to be written
		 *
		 * \throws LockError If attempting to write outside this lock
		 */
		void write(uint64_t beginOffset, const std::string &data);

		//! Check if this range is complete
		bool isComplete() const { return m_parent->isComplete(*this); }
	private:
		friend class ::PartData;

		/**
		 * \brief Construct new Lock
		 *
		 * @param parent      PartData object this lock belongs to
		 * @param r           Range to be locked
		 */
		LockedRange(PartData *parent, Range64 r, UsedRangePtr used);

		/**
		 * \brief Construct new lock and associate with chunk
		 *
		 * @param parent      PartData object this lock belongs to
		 * @param r           Range to be locked
		 * @param it          Iterator to chunk the lock belongs to
		 */
		LockedRange(
			PartData *parent, Range64 r, Detail::PosIter &it,
			UsedRangePtr used
		);

		//! Copying locks is forbidden
		LockedRange(const LockedRange&);
		//! Copying locks is forbidden
		LockedRange& operator=(const LockedRange&);

		//!< Parent file
		PartData *m_parent;

		//! The chunk containing this LockedRange. May be invalid.
		boost::scoped_ptr<Detail::PosIter> m_chunk;

		/**
		 * Each locked range must also keep a reference to the
		 * corresponding used range. This has the effect that client
		 * code can drop the UsedRange object after retrieving locks,
		 * however internally we still need to keep the UsedRange
		 * objects alive as long as any of the locks aquired by that
		 * UsedRange are alive.
		 */
		UsedRangePtr m_used;
	};
}

#endif
