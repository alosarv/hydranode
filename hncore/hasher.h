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
 * \file hasher.h Interface for Files Checksumming Subsystem
 */

#ifndef __HASHER_H__
#define __HASHER_H__

#include <hnbase/osdep.h>
#include <hnbase/event.h>
#include <hnbase/hash.h>

#include <hncore/fwd.h>
#include <hncore/iothread.h>

#include <boost/enable_shared_from_this.hpp>
#include <boost/filesystem/path.hpp>

/**
 * Represents a job entry to be performed by WorkThread. Always wrap this class
 * into boost::shared_ptr<>, because it is a 'loose' object - it is not
 * contained anywhere, however needs to stay alive through various different
 * containers/classes. As such, there is no specific 'destroyer' assigned
 * to this class - it will need to be cleaned up once nobody really needs
 * it anymore. The path through which this object goes is generally this:
 *
 * - Client code creates an object, and submits it to WorkThread.
 * - WorkThread inserts it into its local queue of pending jobs.
 * - When time comes, hashing thread takes it out from the queue and performs
 *   the work.
 * - When the work is completed, an event is emitted from the work.
 * - If client code handles it, it can retrieve the results, after which the
 *   object gets auto-destructed, since its not contained anywhere, and the
 *   last copy of the object was sent to the client.
 * - If the client does not handle the event, the object gets autodestructed,
 *   since the last place it was stored was in event table queue, and after
 *   calling all handlers for the object, the object is removed from the queue.
 */
class HNCORE_EXPORT HashWork : public ThreadWork {
public:
	DECLARE_EVENT_TABLE(HashWorkPtr, HashEvent);

	/**
	 * Constructor for full hash work.
	 *
	 * @param filename      Full path to file to be hashed.
	 *
	 * Submitting this job to Hasher will result in HASH_COMPLETE or
	 * HASH_FATAL_ERROR events to be submitted when the job is completed,
	 * as well as the resulting data being submitted to MetaDb.
	 */
	HashWork(const boost::filesystem::path &filename);

	/**
	 * Construct a range hash work.
	 *
	 * @param filename      Full path to file to be hashed.
	 * @param begin         Begin location to begin hashing.
	 * @param end           End location until what to hash.
	 * @param ref           Reference hash to check against.
	 *
	 * Submitting this job to Hasher will result in HASH_VERIFIED or
	 * HASH_FAILED event being submitted when the job is completed.
	 */
	HashWork(
		const boost::filesystem::path &filename,
		uint64_t begin, uint64_t end, const HashBase *ref
	);

	//! Destructor
	~HashWork();

	//! @name Accessors
	//! \note We can't have these as const since scoped_lock is non-const
	//@{
	//! Whether this job is a full job.
	bool isFull() {
		boost::mutex::scoped_lock l(m_lock);
		return m_full;
	}
	//! In case of partial job, retrieves job range begin
	uint64_t begin() {
		boost::mutex::scoped_lock l(m_lock);
		return m_begin;
	}
	//! In case of partial job, retrieves job range end
	uint64_t end() {
		boost::mutex::scoped_lock l(m_lock);
		return m_end;
	}
	//! Retrieves file name to be hashed.
	boost::filesystem::path getFileName() {
		boost::mutex::scoped_lock l(m_lock);
		return m_fileName;
	}
	//! Retrieves type of hash to be generated in case of range verification
	CGComm::HashTypeId getType() {
		boost::mutex::scoped_lock l(m_lock);
		CHECK_THROW(m_ref);
		return m_ref->getTypeId();
	}
	//! In case of range hash work, retrieves reference/control hash
	const HashBase* getRef() {
		boost::mutex::scoped_lock l(m_lock);
		return m_ref;
	}
	//! Retrieves metaData pointer (filled after full hash job)
	MetaData* getMetaData() {
		boost::mutex::scoped_lock l(m_lock);
		return m_md;
	}
	//! Check if this job is valid, e.g. still needed to be performed. This
	//! is needed to make sure that while the job was waiting in the queue,
	//! it hasn't become invalid.
	bool isValid() {
		boost::mutex::scoped_lock l(m_lock);
		return m_valid;
	}
	//! This method should be called by the original job poster to abort
	//! this job, and remove from pending jobs queue. If the work is in
	//! progress already, it will also be aborted, and no results posted.
	void invalidate() {
		boost::mutex::scoped_lock l(m_lock);
		m_valid = false;
	}
	//@}

	//! For implementation use only - set metadata
	void setMetaData(MetaData *md) {
		boost::mutex::scoped_lock l(m_lock);
		m_md = md;
	}

	static uint64_t getHashed();
	static double   getTime();
	static uint32_t getBufSize();

	//! Process this job
	virtual bool process();

protected:
	/**
	 * Open file for hashing
	 */
	virtual void openFile();

	/**
	 * Read next bytes into m_buf member, starting at position
	 *
	 * @param pos      Position to start reading at
	 * @return         Number of bytes read
	 */
	virtual uint64_t readNext(uint64_t pos);

	/**
	 * This method is called when all the hashing is done. The default
	 * implementation emits events as neccesery (e.g. HASH_COMPLETE,
	 * HASH_VERIFIED at al).
	 */
	virtual void finish();

	//! File to be hashed. Must include full path to the file.
	boost::filesystem::path m_fileName;

	//! In case of range hash, this specifies range begin bytes
	uint64_t m_begin;

	//! In case of range hash, this specifies range end bytes
	uint64_t m_end;
private:
	//! After completing full hash job, contains full metadata about the
	//! file.
	MetaData *m_md;

	//! In case of range hash, this specifies reference/control hash
	const HashBase *m_ref;

	/**
	 * \short Indicates valditiy of this job
	 *
	 * This variable is set to true as default, and can be set to false
	 * using invalidate() member function. The purpose of this is to
	 * provide a mechanism to abort hashing jobs which are no longer
	 * wanted by the original poster.
	 */
	bool m_valid;

	//! Whether this job is a "full" hash job
	bool m_full;

	/**
	 * Protects all members of this object. This lock must be aquired
	 * before touching anything in this object.
	 */
	boost::mutex m_lock;

	void initState();
	void doProcess();

	int m_file;
	boost::scoped_array<char> m_buf;
	std::vector<boost::shared_ptr<HashSetMaker> > m_makers;
	bool m_inProgress;
	static uint64_t s_dataCnt;
	static double   s_timeCnt;
	static boost::mutex s_statsLock;
	static uint32_t s_bufSize;
};

#endif
