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
 * \file sharedfile.h Interface for SharedFile class
 */

#ifndef __SHAREDFILE_H__
#define __SHAREDFILE_H__

#include <hnbase/event.h>
#include <hnbase/object.h>

#include <hncore/hasher.h>

#include <set>
#include <iosfwd>

#include <boost/filesystem/path.hpp>
#include <boost/weak_ptr.hpp>

//! Events emitted from SharedFile objects
enum SFEvent {
	//! Posted whenever SharedFile is about to be destroyed
	SF_DESTROY = 0,
	//! Posted whenever new SharedFile is constructed
	SF_ADDED,
	//! Posted whenever metadata is added to SharedFile
	SF_METADATA_ADDED,
	//! Download has been completed and moved to incoming dir
	SF_DL_COMPLETE
};

class MetaData;
class PartData;
class MoveWork;
typedef boost::intrusive_ptr<MoveWork> MoveWorkPtr;

/**
 * SharedFile object represents a file which is currently being shared. It may
 * be partial or complete or completely empty. SharedFile objects are owned by
 * FilesList class - the destructor of SharedFile has been protected to avoid
 * unwanted deletion of SharedFile objects.
 */
class HNCORE_EXPORT SharedFile : public Object, public Trackable {
public:
	DECLARE_EVENT_TABLE(SharedFile*, int);

	/**
	 * Error codes passed in ReadError exception
	 */
	enum ReadErrorReason {
		EFILE_NOT_FOUND, ETRY_AGAIN_LATER, EINVALID_RANGE, E_OTHER
	};

	/**
	 * Exception class thrown by read() method on errors
	 */
	class ReadError : public std::runtime_error {
	public:
		ReadError(const std::string &msg, ReadErrorReason reason);
		ReadErrorReason reason() const { return m_reason; }
	private:
		ReadErrorReason m_reason;
	};

	/**
	 * Construct SharedFile object.
	 *
	 * @param location    Location of the physical file.
	 * @param metaData    Optional MetaData pointer corresponding to this
	 *                    SharedFile object.
	 */
	SharedFile(boost::filesystem::path location, MetaData *metaData = 0);

	/**
	 * Construct SharedFile from existing PartData.
	 *
	 * @param partData    Pointer to PartData object.
	 * @param metaData    Optional pointer to MetaData object.
	 */
	SharedFile(PartData *partData, MetaData *metaData = 0);

	//! @name Accessors
	//@{
	std::string getLocation() const {
		return m_location.branch_path().native_directory_string();
	}
	std::string getName()             const;
	boost::filesystem::path getPath() const { return m_location; }
	MetaData* getMetaData()           const { return m_metaData; }
	PartData* getPartData()           const { return m_partData; }
	uint64_t getSize()                const { return m_size;     }
	bool hasMetaData()                const { return m_metaData; }
	bool     isComplete() const;
	bool     isPartial()  const;
	uint64_t getUploaded() const;
	//@}

	//! @name Setters
	//@{
	void setMetaData(MetaData *md);
	void setPartData(PartData *pd);
	void addUploaded(uint64_t sum);
	//@}

        //! Schedule for destruction
	void destroy();

	//! Order the SharedFile to re-check the integrity of the physical file,
	//! and up2date metadata.
	void verify();

	/**
	 * Read data from disk from location and return it.
	 *
	 * @param begin   Begin offset where to start reading
	 * @param end     End offset where to stop reading (included)
	 * @return        Data buffer of the read data
	 *
	 * \throws ReadError on failure
	 */
	virtual std::string read(uint64_t begin, uint64_t end);

	/**
	 * @returns The current upload speed of this file.
	 *
	 * Plugins should connect their socket's SpeedMeter to the currently-
	 * uploaded file while the transfer is in progress.
	 */
	boost::signal<uint32_t (), Utils::Sum<uint32_t> > getUpSpeed;
protected:
	/**
	 * Exposing default constructor in protected interface to allow maximum
	 * flexibility for derived classes
	 */
	SharedFile();

	/**
	 * Allowed by friends and derived classes
	 */
	virtual ~SharedFile();

	/**
	 * This method is called by SharedFile in response to PD_COMPLETE
	 * event from PartData object, emitted when download is completed and
	 * verified. The base class implementation requests a MoveWork to move
	 * the file to it's destination location.
	 */
	virtual void finishDownload();

	boost::filesystem::path m_location;   //!< (primary) Physical location
	uint64_t m_size;                      //!< Size of file
private:
	friend class FilesList;
	SharedFile(SharedFile &);                  //!< Forbidden
	SharedFile& operator=(const SharedFile &); //!< Forbidden

	/**
	 * Locate MetaData for a file.
	 *
	 * @param loc        Path to file to inspect.
	 * @param name       Name of the file (useful if dealing with tmp files)
	 * @param pd         If set, indicates that we'r dealing with a partial
	 *                   file, thus the size on the disk cannot be trusted,
	 *                   and filesize should be retrieved from PartData
	 *                   object instead.
	 * @return           MetaData object found. May be null if not found.
	 */
	static MetaData* findMetaData(
		boost::filesystem::path loc, const std::string &name,
		PartData *pd = 0
	);

	//! Event handler for PartData events
	void onPartEvent(PartData *pd, int evt);

	//! Event handler for file moving events
	void onMoveEvent(MoveWorkPtr ptr, int evt);

	//! Event handler for hash events
	void onHashEvent(HashWorkPtr hw, HashEvent evt);

	//! Event handler for MetaData events
	void onMetaDataEvent(MetaData *src, int evt);

	//! Updates the files modification date that is stored in MetaDb
	void updateModDate();

	/**
	 * \brief Check if there are any other SharedFiles which match the
	 *        specs of this file.
	 *
	 * @return True if a duplicate was found, and handled appropriately,
	 *         false otherwise.
	 *
	 * Note that the behaviour when duplicate is found is to destroy this
	 * one, not the duplicate.
	 */
	bool isDuplicate();

	/**
	 * \brief Change the location for this file
	 *
	 * @param loc       New location
	 *
	 * \note If m_metaData is non-zero, this also updates modification date
	 *       in m_metaData object.
	 */
	void setLocation(const boost::filesystem::path &loc);

	/**
	 * \brief Add an alternative location.
	 *
	 * @param loc       New location
	 *
	 * This can be used when we detect an identical file shared from
	 * multiple folders.
	 */
	void addLocation(const boost::filesystem::path &loc);

	MetaData   *m_metaData;     //!< May be null
	PartData   *m_partData;     //!< May be null
	HashWorkPtr m_pendingJob;   //!< Pending job (if any)
	MoveWorkPtr m_moveWork;     //!< Pending move work (if any)

	//! If we have PartData, this is connected to PartData events
	boost::signals::connection m_pdSigHandler;

	//! Vector of alternative locations, with timestamps
	std::vector<std::pair<uint32_t, boost::filesystem::path> > m_locations;
};

#endif
