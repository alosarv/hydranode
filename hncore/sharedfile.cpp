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
 * \file sharedfile.cpp Implementation of SharedFile class
 */

#include <hncore/pch.h>
#include <hnbase/prefs.h>
#include <hncore/sharedfile.h>
#include <hncore/partdata.h>
#include <hncore/metadata.h>
#include <hncore/metadb.h>
#include <hncore/fileslist.h>             // Needed for Object() constructor
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/exception.hpp>
#include <fcntl.h>
#ifndef WIN32
	#include <errno.h>
#endif

/**
 * MoveWork class is a job object submitted to IOThread for processing;
 * MoveWork performs file moving from source directory to destination directory,
 * commonly used when completing a file.
 *
 * MoveWork emits EVT_MOVE_OK when moving succeeds, or EVT_MOVE_FAILED if it
 * fails.
 *
 * \note Event constants are defined inside MoveWork class scope
 */
class MoveWork : public ThreadWork {
public:
	DECLARE_EVENT_TABLE(MoveWorkPtr, int);
	enum MoveEvent { EVT_MOVE_OK, EVT_MOVE_FAILED };

	/**
	 * Constructor
	 *
	 * @param src    Source location
	 * @param dest   Destination location
	 */
	MoveWork(
		const boost::filesystem::path &src,
		const boost::filesystem::path &dest
	) : m_src(src), m_dest(dest) {}

	/**
	 * Processes the moving. This may take some time.
	 *
	 * When both source and destination are located on same partition, the
	 * file is simply renamed, which takes nearly no time. However, when the
	 * source and destination are located on different partitions, entire
	 * data needs to be copied. Furthermore, some implementations do not
	 * support renaming accross partitions, so we fall back to explicit
	 * copy + remove in those cases.
	 *
	 * Upon successful completition, EVT_MOVE_OK is emitted. On failure,
	 * EVT_MOVE_FAILEd is emitted.
	 */
	bool process() try {
		while (boost::filesystem::exists(m_dest)) {
			m_dest = m_dest.branch_path() / ("_" + m_dest.leaf());
		}
		boost::filesystem::path bpath(m_dest.branch_path());
		boost::filesystem::path::iterator it(bpath.begin());
		boost::filesystem::path tmp;
		while (it != bpath.end()) {
			tmp /= boost::filesystem::path(
				*it++, boost::filesystem::native
			);
			if (!boost::filesystem::exists(tmp)) try {
				boost::filesystem::create_directory(tmp);
			} catch (std::exception &e) {
				logError(e.what());
				break;
			}
		}
		logMsg(
			boost::format("Moving file %s -> %s")
			% m_src.native_file_string()
			% m_dest.native_file_string()
		);
		try {
			boost::filesystem::rename(m_src, m_dest);
		} catch (const boost::filesystem::filesystem_error&) {
			boost::filesystem::copy_file(m_src, m_dest);
			boost::filesystem::remove(m_src);
		}
		getEventTable().postEvent(MoveWorkPtr(this), EVT_MOVE_OK);
		setComplete();
		return true;
	} catch (std::exception &e) {
		logError(boost::format("Moving completed file: %s") % e.what());
		getEventTable().postEvent(MoveWorkPtr(this), EVT_MOVE_FAILED);
		setComplete();
		return true;
	}

	//! \returns source location
	boost::filesystem::path getSrc()  const { return m_src; }
	//! \returns destination location
	boost::filesystem::path getDest() const { return m_dest; }
private:
	boost::filesystem::path m_src;  //!< Source path
	boost::filesystem::path m_dest; //!< Destination path
};

IMPLEMENT_EVENT_TABLE(MoveWork, MoveWorkPtr, int);

// SharedFile Class
// ----------------
IMPLEMENT_EVENT_TABLE(SharedFile, SharedFile*, int);

// ReadError exception class
SharedFile::ReadError::ReadError(const std::string &msg, ReadErrorReason reason)
: std::runtime_error(msg), m_reason(reason) {}

// Constructor for existing Shared File object (NOT partial file)
SharedFile::SharedFile(boost::filesystem::path location, MetaData *md /* =0 */)
: Object(&FilesList::instance(), location.leaf()),
m_location(location), m_size(Utils::getFileSize(m_location)), m_metaData(md),
m_partData() {
	using namespace boost::filesystem;

	// explicit sanity checking
	if (!exists(location)) {
		std::string msg = (boost::format(
			"Specified file '%s' does not exist."
		) % location.native_file_string()).str();
		throw std::runtime_error(msg);
	}
	if (is_directory(location)) {
		std::string msg = (boost::format(
			"Specified file '%s' is a directory."
		) % location.native_file_string()).str();
		throw std::runtime_error(msg);
	}
	if (is_empty(location)) {
		std::string msg = (boost::format(
			"Attempt to create empty shared file from '%s'."
		) % location.native_file_string()).str();
		throw std::runtime_error(msg);
	}
	setName(getName());

	verify(); // That function performs all the verification we need

	if (m_metaData) {
		getEventTable().postEvent(this, SF_METADATA_ADDED);
	}

	logTrace(TRACE_SHAREDFILE, boost::format(
		"New sharedfile: %s %|60t| Size: %.2fMB"
		) % location.native_file_string() % (m_size/1024.0/1024.0)
	);
	getEventTable().postEvent(this, SF_ADDED);
}

// Construct using PartData and (optional) MetaData pointers. This constructor
// creates a 'partial' shared file. Note that we do not require filename/path
// to be passed here - we can retrieve it ourselves from PartData (which does
// require m_destination string).
// Preconditions:  partData->getDestination() returns valid destination path
//                 partData->getFileSize() returns a correct file size
SharedFile::SharedFile(PartData *partData, MetaData *metaData /* = 0 */)
: Object(&FilesList::instance(), "sharedFile"), m_size(), m_metaData(metaData),
m_partData(partData) {
	CHECK_THROW(partData);

	setPartData(partData);

	// We need file name ...
	using namespace boost::filesystem;
	m_size = partData->getSize();
	setName(getName());                   // Also for Object hierarchy

	verify();

	// don't even continue if we'r a duplicate ... and if we'r a temp file,
	// delete our temp files too to avoid further errors during next loads
	if (m_metaData && isDuplicate()) {
		boost::format fmt("Duplicate %s: %s");
		if (m_partData) {
			fmt % "incomplete download and shared file";
			m_partData->deleteFiles();
		} else {
			fmt % "shared file";
		}
		fmt % getName();
		throw std::runtime_error(fmt.str());
	}

	if (m_metaData && m_metaData != m_partData->getMetaData()) {
		// both have metadata - trust the one from metadb
		delete m_partData->getMetaData();
		m_partData->setMetaData(m_metaData);
	} else if (!m_metaData && m_partData->getMetaData()) {
		// we'r missing metadata, partdata has it
		logDebug(
			boost::format(
				"%s: No MetaData found in database; "
				"using from PartData."
			) % getName()
		);
		m_metaData = m_partData->getMetaData();
	}

	m_metaData->getEventTable().addHandler(
		m_metaData, this, &SharedFile::onMetaDataEvent
	);
	getEventTable().postEvent(this, SF_ADDED);
}

// protected default constructor
SharedFile::SharedFile() : Object(&FilesList::instance(), "sharedfile"),
m_size(), m_metaData(), m_partData() {}

// Destructor
SharedFile::~SharedFile() {
	if (m_pendingJob) {
		m_pendingJob->invalidate();
	}
	if (m_partData) {
		PartData::getEventTable().safeDelete(m_partData);
	}
	getEventTable().delHandlers(this);
}

// Locate MetaData
MetaData* SharedFile::findMetaData(
	boost::filesystem::path loc, const std::string &name, PartData *pd
) {
	// Retrieve file size and mod date
	uint64_t fileSize = 0;
	if (pd) {
		fileSize = pd->getSize();
	} else {
		fileSize = Utils::getFileSize(loc);
	}
	uint32_t fileDate = Utils::getModDate(loc);

	logTrace(
		TRACE_SHAREDFILE,
		boost::format("Searching for MetaData for file %s (%s)")
		% loc.native_file_string() % name
	);
	logTrace(
		TRACE_SHAREDFILE,
		boost::format("FileName is %s, size is %d") % name
		% fileSize
	);

	std::vector<MetaData*> md = MetaDb::instance().find(name);
	logTrace(
		TRACE_SHAREDFILE,
		boost::format("%d possible candidates found.") % md.size()
	);
	for (
		std::vector<MetaData*>::iterator i = md.begin();
		i != md.end(); ++i
	) {
		// Verify size
		if ((*i)->getSize() != fileSize) {
			logTrace(TRACE_SHAREDFILE, boost::format(
				"Filesize doesn't match (%d != %d)")
				% (*i)->getSize() % fileSize
			);
			continue;
		}
		// Verify modification date
		if ((*i)->getModDate() != fileDate) {
			logTrace(TRACE_SHAREDFILE, boost::format(
				"Modification date doesn't match (%d != %d)"
				) % (*i)->getModDate() % fileDate
			);
			continue;
		}
		return *i;
	}
	logTrace(TRACE_SHAREDFILE, "Not found.");
	return 0;
}

//! Handle PartData events
void SharedFile::onPartEvent(PartData *pd, int evt) {
	CHECK_RET(pd == m_partData);

	if (evt == PD_DATA_FLUSHED) {
		updateModDate();
	} else if (evt == PD_DESTROY) {
		m_pdSigHandler.disconnect();
		// set partdata 0 here, otherwise destroy() calls
		// partdata->destroy() again when canceling downloads
		m_partData = 0; // deleted by EventTable safeDelete()
		if (!pd->isComplete()) {
			// download was canceled - destroy SharedFile also
			destroy();
		}
	} else if (evt == PD_COMPLETE) {
		finishDownload();
	}
}

void SharedFile::finishDownload() {
	CHECK_RET(m_partData);

	m_moveWork = MoveWorkPtr(
		new MoveWork(
			m_partData->getLocation(), m_partData->getDestination()
		)
	);
	MoveWork::getEventTable().addHandler(
		m_moveWork, this, &SharedFile::onMoveEvent
	);
	logDebug(
		boost::format("Requesting MoveWork(%s -> %s)")
		% m_moveWork->getSrc().native_file_string()
		% m_moveWork->getDest().native_file_string()
	);
	IOThread::instance().postWork(m_moveWork);
	PartData::getEventTable().postEvent(m_partData, PD_MOVING);
}

void SharedFile::onMoveEvent(MoveWorkPtr wrk, int evt) {
	using boost::filesystem::path;

	if (evt == MoveWork::EVT_MOVE_OK) {
		// just in case movework used fallback destination
		m_partData->m_dest = wrk->getDest();

		getEventTable().postEvent(this, SF_DL_COMPLETE);
		m_partData->getEventTable().postEvent(
			m_partData, PD_DL_FINISHED
		);
		m_partData->deleteFiles();
		m_partData->destroy();
		setLocation(wrk->getDest());

		m_moveWork = MoveWorkPtr();
		updateModDate();

		logMsg(
			boost::format(
				COL_BBLUE "Download complete: "
				COL_BGREEN "%s" COL_NONE
			) % getName()
		);
	} else if (evt == MoveWork::EVT_MOVE_FAILED) {
		// change file's incoming dir to global incoming dir,
		// but only if it differs
		std::string incDir = Prefs::instance().read<std::string>(
			"/Incoming", ""
		);
		if (!incDir.size()) {
			return;
		}
		if (incDir == wrk->getDest().branch_path().string()) {
			return;
		}
		m_partData->setDestination(
			path(incDir) / m_partData->getDestination().leaf()
		);
		m_moveWork = MoveWorkPtr();
		onPartEvent(m_partData, PD_COMPLETE);
	}
}

void SharedFile::setPartData(PartData *pd) {
	CHECK_THROW(pd);

	m_pdSigHandler = PartData::getEventTable().addHandler(
		pd, this, &SharedFile::onPartEvent
	);
	m_partData = pd;
	m_location = m_partData->getLocation();
}

// Destroy a shared file. Notice that we remove ourselves as event handler from
// our partdata before also signalling partdata destruction (in case we have
// partdata), because otherwise we'd end up getting called back ourselves from
// event table, but since we are also heading for destruction, that would be
// really bad karma ...
void SharedFile::destroy() {
	getEventTable().postEvent(this, SF_DESTROY);
	if (m_partData) {
		m_pdSigHandler.disconnect();
		m_partData->destroy();
	}
}

void SharedFile::verify() {
	if (!m_metaData) {
		logTrace(
			TRACE_SHAREDFILE,
			boost::format("verify(): location=`%s' filename=`%s'")
			% getLocation() % getName()
		);
		m_metaData = findMetaData(m_location, getName(), m_partData);
		if (m_metaData && !isPartial()) {
			uint64_t realSz = Utils::getFileSize(m_location);
			if (m_metaData->getSize() != realSz) {
				m_metaData = 0; // wrong filesize ??
			}
		} // can't do such check for temp files
	}

	if (!m_metaData && !m_partData) {
		// Too bad -> rehash the thing
		HashWorkPtr hw(new HashWork(m_location));
		HashWork::getEventTable().addHandler(
			hw, this, &SharedFile::onHashEvent
		);

		// Keep a weak reference to the job in case we want to abort it
		m_pendingJob = hw;
		IOThread::instance().postWork(hw);
	} else if (m_metaData) {
		CHECK_THROW(m_metaData->getSize() == getSize());

		// Associate MetaData with this SharedFile
		MetaDb::instance().push(m_metaData, this);
	}
}

void SharedFile::onHashEvent(HashWorkPtr hw, HashEvent evt) {
	// Clear all event handlers for this job
	HashWork::getEventTable().delHandlers(hw);
	m_pendingJob = HashWorkPtr();

	if (evt == HASH_FATAL_ERROR) {
		logError(boost::format(
			"Hasher reported fatal error. Removing shared file %s"
		) % hw->getFileName().native_file_string());
		destroy();
	} else {
		CHECK_THROW(hw->getMetaData());
		CHECK_THROW(hw->getMetaData()->getHashSetCount());
		MetaData *nmd = hw->getMetaData();
		MetaData *found = 0;
		for (uint32_t i = 0; i < nmd->getHashSetCount(); ++i) {
			found = MetaDb::instance().find(
				nmd->getHashSet(i)->getFileHash()
			);
			if (found) {
				break;
			}
		}
		if (found) {
			logDebug(
				boost::format(
					"Found existing MetaData for file "
					"%s, merging custom data."
				) % getName()
			);
			nmd->mergeCustom(found);
			// TODO: remove the found entry from MetaDb
		}

		m_metaData = hw->getMetaData();
		m_metaData->setName(getName());

		if (!isDuplicate()) {
			MetaDb::instance().push(hw->getMetaData(), this);
			getEventTable().postEvent(this, SF_METADATA_ADDED);
		}
	}
}

// check if there are any other SharedFiles with identical hash as this one.
// if that's the case, and the found file is partial, cancel the download,
// otherwise, simply record this location also in the existing file and
// destroy(). Return true if duplicate was found, false otherwise.
bool SharedFile::isDuplicate() {
	CHECK_THROW(m_metaData);
	SharedFile *sf = 0;
	for (uint32_t i = 0; i < m_metaData->getHashSetCount(); ++i) {
		sf = MetaDb::instance().findSharedFile(
			m_metaData->getHashSet(i)->getFileHash()
		);
		if (sf == this) {
			sf = 0;
		}
		if (sf) {
			break;
		}
	}
	if (sf) {
		if (sf->isPartial()) {
			logMsg(
				boost::format(
					"Currently downloaded file `%s' found "
					"shared at `%s'.\nAborting download - "
					"you already have this file."
				) % sf->getName()
				% m_location.native_file_string()
			);
			sf->getPartData()->destroy();
			sf->setLocation(m_location);
			sf->setMetaData(m_metaData);
		} else {
			sf->addLocation(m_location);
			delete m_metaData;
			m_metaData = 0;
		}
		destroy();
		return true;
	}
	return false;
}

// read data from disk and return it
// note that we ALWAYS check modification date for file before reading data from
// it. While this comes with a performance penalty, this is a needed security
// feature here, to avoid EVER sending out unverified data.
//
// when the main location fails, either due to modification, or due to non-
// existance, we attempt to switch to alternative locations, if present, and
// recurse into this function again, until we either run out of alternative
// locations, or find a location where the file has correct modification date
// and is readable.
std::string SharedFile::read(uint64_t begin, uint64_t end) {
	if (m_moveWork) {
		throw ReadError(
			"Moving in progress; try again later", ETRY_AGAIN_LATER
		);
	}

	if (m_partData && !m_partData->isComplete(begin, end)) {
		boost::format fmt("%s: Requested incomplete range %d..%d");
		fmt % getName() % begin % end;
		throw ReadError(fmt.str(), EINVALID_RANGE);
	}

	// first try to open, 'cos if the file doesn't exist, there's no point
	// in the below modification date checks either
	int fd = open(
		getPath().native_file_string().c_str(),
		O_RDONLY|O_LARGEFILE|O_BINARY
	);

	if (fd == -1) {
		if (m_locations.size()) {
			setLocation(m_locations.back().second);
			m_locations.pop_back();
			return read(begin, end);
		}
		boost::format fmt(
			"Unable to open shared file %s%s (reason: %s)"
		);
		fmt % getName();
		if (isPartial()) {
			fmt % (" (loc=" + getPath().native_file_string() + ")");
		} else {
			fmt % "";
		}
#ifdef WIN32
		fmt % "unable to open file";
#else
		fmt % strerror(errno);
#endif
		destroy(); // sorry, but if our file died, so shall we
		throw ReadError(fmt.str(), EFILE_NOT_FOUND);
	}

	// check if modification date matches what we have on record
	// if not, and we'r partial file, just rehash completed parts;
	// otherwise, drop current metadata and rehash the file
	if (m_metaData) try {
		uint32_t actual = Utils::getModDate(m_location);
		uint32_t stored = m_metaData->getModDate();

		// on fat32 partition, we seem to have 1ms-problem: after
		// saving it, we get 1ms LOWER modification date, so this
		// check here should avoid complete rehash in that case;
		// it is highly unlikely a file was modified externally
		// within 1ms timeframe of the saving time, so it's generally
		// safe to do it here.
		if (actual != stored && actual + 1 != stored) {
			boost::format fmt(
				"Error reading file %s: "
				"modification date %d != %d"
			);
			fmt % getName() % actual % stored;
			if (isPartial()) {
				m_partData->rehashCompleted();
			} else {
				MetaDb::instance().remSharedFile(this);
				m_metaData = 0;
				verify();
			}
			throw ReadError(fmt.str(), ETRY_AGAIN_LATER);
		}
	} catch (...) {
		close(fd);
		throw;
	}

	boost::scoped_array<char> buf(new char[end - begin + 1]);

	try {
		uint64_t ret = lseek64(fd, begin, SEEK_SET);
		CHECK_THROW(ret == begin);
		int check = ::read(fd, buf.get(), end - begin + 1);
		CHECK_THROW(check == static_cast<int>(end - begin + 1));
	} catch (std::exception &e) {
		close(fd);
		throw ReadError(e.what(), E_OTHER);
	}
	close(fd);

	std::string ret(buf.get(), end - begin + 1);

	return ret;
}

std::string SharedFile::getName() const {
	if (m_partData) {
		return m_partData->getDestination().leaf();
	} else {
		return m_location.leaf();
	}
}

void SharedFile::updateModDate() {
	if (m_metaData) {
		logTrace(TRACE_SHAREDFILE, "Updating modification date.");
		m_metaData->setModDate(Utils::getModDate(m_location));
	}
}

bool SharedFile::isPartial() const {
	return m_partData;
}
bool SharedFile::isComplete() const {
	return !m_partData;
}

// change the metadata for this shared file.
void SharedFile::setMetaData(MetaData *md) {
	CHECK_THROW(md);
	m_metaData = md;
}

void SharedFile::addLocation(const boost::filesystem::path &loc) {
	m_locations.push_back(std::make_pair(Utils::getModDate(loc), loc));
}

void SharedFile::setLocation(const boost::filesystem::path &loc) {
	m_location = loc;
	if (m_metaData) {
		m_metaData->setModDate(Utils::getModDate(loc));
	}
}

void SharedFile::onMetaDataEvent(MetaData *md, int evt) {
	CHECK_RET(md == m_metaData);
	if (evt == MD_SIZE_CHANGED) {
		m_size = md->getSize();
	}
}

void SharedFile::addUploaded(uint64_t sum) {
	if (m_metaData) {
		m_metaData->addUploaded(sum);
	}
}

uint64_t SharedFile::getUploaded() const {
	return m_metaData ? m_metaData->getUploaded() : 0;
}
