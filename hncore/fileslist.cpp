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
 * \file fileslist.cpp Implementation of FilesList class
 */

#include <hncore/pch.h>
#include <hncore/fileslist.h>
#include <hncore/hydranode.h>
#include <hncore/metadata.h>
#include <hncore/metadb.h>
#include <hncore/sharedfile.h>
#include <hncore/partdata.h>
#include <hnbase/event.h>
#include <hnbase/prefs.h>
#include <boost/filesystem/operations.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <fstream>

// FilesList Class
// ---------------
// Constructor (private)
FilesList::FilesList() : Object(&Hydranode::instance(), "files") {
	import(""); // initialize the signal object from main app
	SharedFile::getEventTable().addAllHandler(
		this, &FilesList::onSharedFileEvent
	);
}

// Destructor (private)
FilesList::~FilesList() {}

void FilesList::exit() {
	for (SFIter i = m_list.begin(); i != m_list.end(); ++i) {
		delete *i;
	}

	if (HashWork::getTime() && HashWork::getHashed()) {
		boost::format fmt("Hasher: %s checksummed in %.2fs (%s/s)");
		uint64_t amount = HashWork::getHashed();
		float    time   = HashWork::getTime();
		fmt % Utils::bytesToString(amount) % time;
		fmt % Utils::bytesToString(static_cast<uint64_t>(amount/time));
		logMsg(fmt);
	}

	saveSettings();
}

FilesList& FilesList::instance() {
	static FilesList fl;
	return fl;
}

void FilesList::push(SharedFile *file) {
	CHECK_THROW(file != 0);
	SFIter i = m_list.find(file);
	if (i != m_list.end()) {
		assert(*i == file);
		throw std::runtime_error(
			"Attempt to insert a SharedFile which already exists."
		);
	}
	m_list.insert(file);
}

void FilesList::addSharedDir(const std::string &_path, bool recurse) try {
	CHECK_THROW(!_path.empty());
	std::string path = _path;
#ifdef WIN32 // workaround for boost_filesystem library issues
	boost::algorithm::replace_all(path, "\\", "/");
#endif

	verifyPath(path);

	if (m_sharedDirs.find(path) != m_sharedDirs.end()) {
		return; // already known
	}

	IOThread::Pauser pauser(IOThread::instance());

	scanSharedDir(path, recurse);
	m_sharedDirs[path] = recurse;
	saveSettings();

} catch (std::exception &e) {
	logError(boost::format("Error scanning shared directory: %s")%e.what());
} MSVC_ONLY(;)

void FilesList::addTempDir(const std::string &_path, bool scan) try {
	CHECK_RET(!_path.empty());
	std::string path = _path;
#ifdef WIN32 // workaround for boost_filesystem library issues
	boost::algorithm::replace_all(path, "\\", "/");
#endif

	verifyPath(path);

	if (m_tempDirs.find(path) != m_tempDirs.end()) {
		return; // already known
	}

	if (scan) {
		IOThread::Pauser pauser(IOThread::instance());
		scanTempDir(path);
	}
	m_tempDirs.insert(path);
	saveSettings();

} catch (std::exception &e) {
	logError(boost::format("Unable to scan temp dir: %s") % e.what());
} MSVC_ONLY(;)

// \todo remSharedDir recurse
void FilesList::remSharedDir(const std::string &path, bool /* recurse */) {
	CHECK_RET(!path.empty());
	verifyPath(path);

	std::map<std::string, bool>::iterator ret = m_sharedDirs.find(path);
	if (ret == m_sharedDirs.end()) {
		logDebug(boost::format(
			"Attempt to un-share path '%s' which was never shared."
		) % path);
		return;
	}
	m_sharedDirs.erase(ret);

	// Scan through main map and locate entries which were in this
	// directory, and destroy them.
	std::string _path = path;
#ifdef WIN32 // workaround for boost_filesystem library issues
	boost::algorithm::replace_all(_path, "/", "\\");
#endif

	for (SFIter i = m_list.begin(); i != m_list.end(); ++i) {
		if ((*i)->getLocation() == _path) {
			(*i)->destroy();
		}
	}
	saveSettings();
}

void FilesList::remTempDir(const std::string &path) {
	CHECK_RET(!path.empty());
	verifyPath(path);

	std::set<std::string>::iterator ret = m_tempDirs.find(path);
	if (ret == m_tempDirs.end()) {
		logDebug(boost::format(
			"Attempt to remove a temporary directory '%s' which is"
			" not a temporary directory."
		) % path);
		return;
	}
	m_tempDirs.erase(ret);

	// Scan through main map and locate part files which were in this
	// directory, and destroy them.
	for (SFIter i = m_list.begin(); i != m_list.end(); ++i) {
		if ((*i)->getLocation() == path && (*i)->isPartial()) {
			(*i)->destroy();
		}
	}
	saveSettings();
}

void FilesList::verifyPath(const std::string &path) {
	boost::filesystem::path p(path, boost::filesystem::no_check);
	if (!boost::filesystem::is_directory(p)) {
		std::string msg = (boost::format(
			"Specified path '%s' is not a directory."
		) % path).str();
		throw std::runtime_error(msg);
	}
	if (!boost::filesystem::exists(p)) {
		std::string msg = (boost::format(
			"Specified path '%s' does not exist."
		) % path).str();
		throw std::runtime_error(msg);
	}
}

//! This variable keeps track of recursion count in scanSharedDir (which calls
//! itself during recursive directories scanning) in order to write preferences
//! only during first entrance of the function during recursive scan, but not
//! during any subsequent recursive entrances. The variable is initialized to
//! zero, increased by one during every entrance of scanSharedDir() method, and
//! reduced by one during every exit of scanSharedDir() method. Thus, if this
//! variable is set to 1, we are dealing with first entrance of the function.
//! Once the recursion ends, the variable should be back at zero.
static int recursion = 0;

//! Directories scanning, optionally recursing.
void FilesList::scanSharedDir(const std::string &dir, bool recurse) try {
	verifyPath(dir);
	recursion++;
	using namespace boost::filesystem;

	if (recursion == 1) {
		logMsg(
			boost::format("Scanning shared directory %s %s")
			% dir % (recurse ? "recursivly" : "")
		);
		SDIter i = m_sharedDirs.find(dir);
		if (i == m_sharedDirs.end()) {
			m_sharedDirs.insert(std::make_pair(dir, recurse));
		} else {
			// We already have this dir - update it
			if ((*i).second != recurse) {
				(*i).second = recurse;
			}
		}
		saveSettings();
	}

	// Scan it
	directory_iterator end_itr; // Default constructor -> end iterator
	for (directory_iterator i(path(dir, no_check)); i != end_itr; ++i) {
		if (is_directory(*i)) {
			if (recurse) {
				scanSharedDir((*i).string(), recurse);
			}
			continue;
		}
		std::string fName = (*i).leaf();
		boost::algorithm::to_lower(fName);
		if (boost::algorithm::starts_with(fName, "albumart")) {
			continue;
		} else if (fName == "folder.jpg") {
			continue;
		} else if (fName == "thumbs.db") {
			continue;
		} else if (fName == "desktop.ini") {
			continue;
		}

		NIter j = m_list.get<1>().find((*i).string());
		if (j == m_list.get<1>().end()) {
			SharedFile *f = new SharedFile(*i);
			m_list.insert(f);
		} else {
			// Let the file re-verify its integrity
			(*j)->verify();
		}
	}
	recursion--;
} catch (std::exception &er) {
	logError(boost::format("Scanning shared directories: %s") % er.what());
	recursion--;
}
MSVC_ONLY(;)

/**
 * Scan directory for part files.
 */
void FilesList::scanTempDir(const std::string &dir) {
	using namespace boost::filesystem;
	using namespace boost::algorithm;

	logDebug(boost::format("Scanning temp directory: %s") % dir);

	verifyPath(dir);
	uint32_t found = 0;
	directory_iterator end_itr; // default constructor -> end iterator
	for (directory_iterator i(dir); i != end_itr; ++i) {
		if (!iends_with((*i).string(), ".dat")) {
			continue;
		}
		if (loadTempFile((*i).string())) {
			++found;
		}
	}
	logMsg(boost::format("Scanning Temp dir: Found %d temp files.") %found);
}

// bah ... writing exception-safe loading is crap. We really ought to consider
// moving sharedfile, partdata and metadata completely to smart-pointers, would
// certainly make things safer and cleaner (not only here, but all over the
// place). But for the time being - *ARGH*
bool FilesList::loadTempFile(const boost::filesystem::path &file) {
	using namespace boost::filesystem;
	using namespace boost::algorithm;

	std::string partName = file.string();
	replace_last(partName, ".dat.bak", ""); // in case of backups
	replace_last(partName, ".dat", "");
	PartData *pd = 0;
	SharedFile *sf = 0;
	MetaData *md = 0;
	if (exists(partName)) try {
		pd = new PartData(file);  // may throw
		md = pd->getMetaData();

		// SharedFile constructor calls findMetaData()
		sf = new SharedFile(pd); // may throw

		// as side-effect of SharedFile constructor,
		// pd->metadata may have been deleted
		// and replaced with MetaData found in MetaDb!!
		md = pd->getMetaData();

		pd->init();              // may throw

		// only do this if none of the above threw
		if (sf->getMetaData()) {
			// associate MetaData with SharedFile
			MetaDb::instance().push(sf->getMetaData(), sf);
		}

		m_list.insert(sf);
		return true;
	} catch (std::exception &e) {
		logError(boost::format(
			"Failed to load part file %s:\n  what(): %s"
		) % partName % e.what());
		// ~SharedFile calls PartData::safeDelete, so avoid that
		if (sf) {
			sf->m_partData = 0;
		}
		delete pd; delete sf; delete md;
		path bakName(file.native_file_string() + ".bak");
		if (exists(bakName)) {
			logMsg("Attempting to load backup...");
			loadTempFile(bakName);
		} else {
			logError(
				boost::format("Backup file %s does not exist.")
				% bakName.native_file_string()
			);
		}
	} else {
		// clean out any leftovers from previous downloads
		remove(file);
		path tmp(file.string() + ".bak", native);
		if (exists(tmp) && !is_directory(tmp)) {
			remove(tmp);
		}
	}
	return false;
}

void FilesList::onSharedFileEvent(SharedFile *sf, int evt) {
	if (evt == SF_DESTROY) {
		m_list.erase(sf);
		sf->getEventTable().safeDelete(sf);
	} else if (evt == SF_DL_COMPLETE) {
		SFIter it = m_list.find(sf);
		CHECK_RET(it != m_list.end());
		CHECK_RET(*it == sf);
		m_list.erase(it);
		m_list.insert(sf);
	}
}

// Customization virtual functions
uint32_t FilesList::getOperCount() const {
	return 1;
}

Object::Operation FilesList::getOper(uint32_t n) const {
	switch (n) {
		case 0: {
			Object::Operation o("adddir", true);
			o.addArg(
				Object::Operation::Argument(
					"sharedpath", true, ODT_STRING
				)
			);
			o.addArg(
				Object::Operation::Argument(
					"recurse", false, ODT_BOOL
				)
			);
			return o;
		}
		default:
			throw std::runtime_error("No such operation.");
	}
}

void FilesList::doOper(const Object::Operation &oper) {
	logDebug(
		boost::format("FilesList: Received operation %s")
		% oper.getName()
	);
	logDebug(boost::format("argcount=%d") % oper.getArgCount());
	for (uint8_t i = 0; i < oper.getArgCount(); ++i) {
		logDebug(
			boost::format("Argument %s=%s")
			% oper.getArg(i).getName()
			% oper.getArg(i).getValue()
		);
	}

	if (oper.getName() == "adddir") {
		uint8_t argc = oper.getArgCount();
		if (!argc) {
			logError("Invalid argument count to operation.");
			return;
		}
		std::string path;
		bool recurse = false;
		try {
			path = oper.getArg("sharedpath").getValue();
		} catch (...) {
			logError("Need argument `sharedpath`");
			return;
		}
		try {
			recurse = boost::lexical_cast<bool>(
				oper.getArg("recurse").getValue()
			);
		} catch (boost::bad_lexical_cast&) {
			logError("Bad argument type for argument `recurse`");
		} catch (std::runtime_error&) {
			// Ignored
		}
		scanSharedDir(path, recurse);
	}
}
// Start a new download
// --------------------
// First, we must construct a PartData, which will serve as the empty new
// download. PartData constructor needs full temp file path, so we must figure
// out a temp file name at this point. I think a random integer number would
// suffice here, altough checks must be made that there already isn't a file
// with same name. Different clients use different systems for temp file names,
// some use integers numbers starting from zero, some use real file name. Using
// real file name opens up the problem of user thinking it IS a real full file,
// oblivios of the fact that it is actually a half-complete file. So using a
// random integer value here should avoid any confusion.
//
// Thus, the next loop generates random ID's, checking for existance of the
// file with that ID, until we find a free ID. Once we'v established that,
// we can head on to constructing the actual PartData object.
//
// After that, it's just tieing things together. PartData to SharedFile,
// the newly created MetaData also to the SharedFile (and also submit to MetaDb)
// and insert the entire compound into our internal list. This completes the
// download initialization sequence.
void FilesList::createDownload(
	std::string name, MetaData *md, boost::filesystem::path dest
) {
	using boost::filesystem::path;
	using boost::filesystem::no_check;
	boost::algorithm::trim(name);

	if (dest.empty()) {
		std::string incDir = Prefs::instance().read<std::string>(
			"/Incoming", ""
		);
	#ifdef WIN32 // workaround for boost_filesystem library issues
		boost::algorithm::replace_all(incDir, "\\", "/");
	#endif
		dest = path(incDir, no_check);
	}
	dest /= path(name, no_check);

	uint32_t id = 0;

	std::string tmpdir = Prefs::instance().read<std::string>("/Temp", "");
	path p(tmpdir, no_check);

	std::string fname;
	do {
		id = Utils::getRandom();
		fname = boost::lexical_cast<std::string>(id);
		fname += ".tmp";
	} while (boost::filesystem::exists(p/fname));

	p /= fname;

	PartData *pd = new PartData(md->getSize(), p, dest);
	SharedFile *sf = new SharedFile(pd, md);

	MetaDb::instance().push(md, sf);
	push(sf);
	pd->save();
	onDownloadCreated(pd);

	logMsg(boost::format("Download started: %s") % pd->getName());
	logDebug(
		"Destination: "
		+ pd->getDestination().branch_path().native_directory_string()
	);
}

void FilesList::savePartFiles(bool saveAll) {
	uint32_t saved = 0, skipped = 0;
	Utils::StopWatch s1;
	for (SFIter i = m_list.begin(); i != m_list.end(); ++i) {
		if ((*i)->isPartial()) {
			if (saveAll || (*i)->getPartData()->hasBuffered()) {
				(*i)->getPartData()->save();
				(*i)->updateModDate();
				++saved;
			} else {
				++skipped;
			}
		}
	}
	logTrace(
		TRACE_FILESLIST,
		boost::format("%d temp files saved in %dms (skipped %d)")
		% saved % s1 % skipped
	);
}

void FilesList::saveSettings() const {
	std::string incDir(Prefs::instance().read<std::string>("/Incoming",""));
#ifdef WIN32 // workaround for boost_filesystem library issues
	boost::algorithm::replace_all(incDir, "\\", "/");
#endif
	if (m_sharedDirs.size()) {
		Prefs::instance().write(
			"/SharedDirs/Count", m_sharedDirs.size() - 1
		);
		std::map<std::string, bool>::const_iterator it(
			m_sharedDirs.begin()
		);
		uint32_t cnt = 0;
		while (it != m_sharedDirs.end()) {
			if ((*it).first == incDir) {
				++it;
				continue;
			}
			boost::format fmt("/SharedDirs/Dir_%d");
			fmt % cnt++;
			Prefs::instance().write(fmt.str(), (*it).first);
			if ((*it).second) {
				Prefs::instance().write(
					fmt.str() + "_recurse", true
				);
			}
			++it;
		}
	}

	std::string globTemp(Prefs::instance().read<std::string>("/Temp", ""));
	std::set<std::string>::const_iterator it(m_tempDirs.begin());

#ifdef WIN32 // workaround for boost_filesystem library issues
	boost::algorithm::replace_all(globTemp, "\\", "/");
#endif
	if (m_tempDirs.size()) {
		Prefs::instance().write("/TempCnt", m_tempDirs.size() - 1);
		uint32_t cnt = 0;
		while (it != m_tempDirs.end()) {
			if (*it == globTemp) {
				++it;
				continue;
			}
			boost::format fmt("/Temp_%d");
			fmt % cnt++;
			Prefs::instance().write(fmt.str(), *it++);
		}
	}
}
