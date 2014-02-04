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
 * \file metadb.cpp Implementation of MetaDb class
 */

#include <hncore/pch.h>

#include <hncore/metadb.h>
#include <hncore/sharedfile.h>
#include <hncore/metadata.h>

// Default constructor
MetaDb::MetaDb() {
}

MetaDb::~MetaDb() {
}

MetaDb& MetaDb::instance() {
	static MetaDb md;
	return md;
}

enum MetaDbOpCodes {
	OP_MDB_VERSION = 0x01    //!< version
};

// load from stream
void MetaDb::load(std::istream &i) try {
	if (!i) {
		return;
	}
	Utils::StopWatch t;

	logTrace(TRACE_MD, "Loading MetaDb from stream.");
	uint8_t ver = Utils::getVal<uint8_t>(i);
	if (ver != OP_MDB_VERSION) {
		logError("Wrong version found loading MetaDb from stream.");
		throw std::runtime_error(
			"Parse error reading MetaDb from stream."
		);
	}
	uint32_t count = Utils::getVal<uint32_t>(i);
	logTrace(TRACE_MD, boost::format("%d objects to read.") % count);
	uint32_t added = 0;
	while (count-- && i) {
		uint8_t oc = Utils::getVal<uint8_t>(i);
		(void)Utils::getVal<uint16_t>(i);
		CHECK_THROW(oc == CGComm::OP_METADATA);
		MetaData *md = new MetaData(i);
		if (!md->getName().size()) {
			delete md;
			continue;
		} else {
			push(md);
		}
		if (count) {
			logTrace(
				TRACE_MD, boost::format(
					"Loading MetaDb: %d entries to go."
				) % count
			);
		}
		++added;
	}
	if (!i) {
		logError("Unexpected end of stream reading MetaDb.");
	} else {
		logMsg(
			boost::format(
				"MetaDb loaded successfully. %d entries added. "
				"(%dms)"
			) % added % t
		);
	}
} catch (std::exception &e) {
	logError(boost::format("Unable to load MetaDb: %s") % e.what());
}
MSVC_ONLY(;)

void MetaDb::save(std::ostream &os) const {
	os << *this;
}

// we do cleanup here instead of destructor since ~MetaData calls
// EventTable::delHandlers, which can fail on shutdown, but we can't remove
// that call from there, so ...
void MetaDb::exit() {
	for (LIter i = m_list.begin(); i != m_list.end(); ++i) {
		delete *i;
	}
	m_list.clear();
}

// Write contents to designated output stream
std::ostream& operator<<(std::ostream &o, const MetaDb &md) {
	logTrace(TRACE_MD, "Writing MetaDb to stream.");
	Utils::StopWatch s1;
	Utils::putVal<uint8_t>(o, OP_MDB_VERSION);
	Utils::putVal<uint32_t>(o, md.m_list.size());
	uint16_t count = 0;
	for (MetaDb::CLIter i = md.m_list.begin(); i != md.m_list.end(); ++i) {
		o << *(*i);
		++count;
	}
	CHECK_THROW(count == md.m_list.size());
	return o;
}

// MetaDb - Adding entries
// -----------------------

// Try to add a file name to m_filenames map. If the name (and source) already
// exist, the operation will silently fail.
//
// note that previous names for the file still remain in the list, since
// we lack two-way lookups in there currently :(
void MetaDb::tryAddFileName(MetaData *source, const std::string &name) {
	CHECK_THROW(source != 0);
	logTrace(
		METADB, boost::format("void MetaDb::tryAddFileName(%s)")
		% name
	);

	std::pair<FNIter, FNIter> i = m_filenames.equal_range(name);
	if (i.first == i.second) {
		logTrace(METADB, "-> Inserting.");
		m_filenames.insert(std::make_pair(name, source));
	} else {
		bool found = false;
		for (FNIter j = i.first; j != i.second; ++j) {
			if ((*j).second == source) {
				found = true;
				break;
			}
		}
		if (found == false) {
			logTrace(METADB, "-> Inserting.");
			m_filenames.insert(std::make_pair(name, source));
		}
	}
}

// Try to add entry to m_nameToSF map
void MetaDb::tryAddFileName(SharedFile *sf, const std::string &name) {
	CHECK_THROW(sf != 0);

	logTrace(
		METADB, boost::format("void MetaDb::tryAddFileName(%s)")
		% name
	);
	std::pair<NTSFIter, NTSFIter> i = m_nameToSF.equal_range(name);
	if (i.first != i.second) {
		for (NTSFIter j = i.first; j != i.second; ++j) {
			if ((*j).second == sf) {
				return;
			}
		}
	}

	logTrace(METADB, "-> Inserting.");
	m_nameToSF.insert(std::make_pair(name, sf));

	// We need to know of its destruction events
	SharedFile::getEventTable().addHandler(
		sf, this, &MetaDb::onSharedFileEvent
	);
}

// Try to add a hashset to Hash <-> MetaData reference map
void MetaDb::tryAddHashSet(MetaData *source, const HashSetBase *hash) {
	CHECK_THROW(hash != 0);
	CHECK_THROW(source != 0);
	logTrace(
		METADB, boost::format("void MetaDb::tryAddHashSet(%s)")
		% hash->getFileHash().decode()
	);

	const HashBase &hashToAdd = hash->getFileHash();

	HTMDIter i = m_hashes.find(hashToAdd.getTypeId());
	if (i == m_hashes.end()) {
		// Already outer map key doesn't exist - add the outer map
		// entry, add inner map, and add the hash to inner map.
		std::map<HashWrapper, MetaData*> maptoAdd;
		maptoAdd.insert(
			std::make_pair(HashWrapper(&hashToAdd), source)
		);
		m_hashes.insert(
			std::make_pair(hashToAdd.getTypeId(), maptoAdd)
		);
	} else {
		// Outer map key found - search in inner map
		HWTMDIter j = (*i).second.find(HashWrapper(&hashToAdd));
		if (j == (*i).second.end()) {
			// Key not found in inner map - insert it.
			(*i).second.insert(
				std::make_pair(HashWrapper(&hashToAdd), source)
			);
		}
	}
}

// Try to add hashset to Hash <-> SharedFile reference map
void MetaDb::tryAddHashSet(SharedFile *sf, const HashSetBase *hash) {
	CHECK_THROW(hash != 0);
	CHECK_THROW(sf != 0);

	const HashBase &hashToAdd = hash->getFileHash();

	// Add to m_hashToSF map
	HTSFIter i = m_hashToSF.find(hashToAdd.getTypeId());
	if (i == m_hashToSF.end()) {
		// Already outer map key doesn't exist - add the outer map
		// entry, add inner map, and add the hash to inner map.
		std::map<HashWrapper, SharedFile*> toAdd;
		toAdd.insert(std::make_pair(HashWrapper(&hashToAdd), sf));
		m_hashToSF.insert(
			std::make_pair(hashToAdd.getTypeId(), toAdd)
		);
	} else {
		// Outer map key found - search in inner map
		HWTSFIter j = (*i).second.find(HashWrapper(&hashToAdd));
		if (j == (*i).second.end()) {
			// Key not found in inner map - insert it.
			(*i).second.insert(
				std::make_pair(HashWrapper(&hashToAdd), sf)
			);
		}
	}

	// We need to know of its destruction events
	SharedFile::getEventTable().addHandler(
		sf, this, &MetaDb::onSharedFileEvent
	);
}

// Add new MetaData object
void MetaDb::push(MetaData *md) {
	CHECK_THROW(md != 0);

	bool added = m_list.insert(md).second;

	// Add cross-reference file names
	tryAddFileName(md, md->getName());

	// Add cross-reference hashes
	for (uint32_t i = 0; i < md->getHashSetCount(); ++i) {
		tryAddHashSet(md, md->getHashSet(i));
	}

	if (added) {
		MetaData::getEventTable().addHandler(
			md, this, &MetaDb::onMetaDataEvent
		);
	}
}

// Add new MetaData object and associate it with SharedFile lookups
void MetaDb::push(MetaData *md, SharedFile *sf) {
	CHECK_THROW(md != 0);

	push(md);

	bool added = m_sfToMd.insert(std::make_pair(sf, md)).second;

	for (uint32_t i = 0; i < md->getHashSetCount(); ++i) {
		tryAddHashSet(sf, md->getHashSet(i));
	}

	tryAddFileName(sf, md->getName());

	if (added) {
		SharedFile::getEventTable().addHandler(
			sf, this, &MetaDb::onSharedFileEvent
		);
	}
}

// MetaDb - Finding entries
// ------------------------

// Locate MetaData by searching with file hash by looking at m_hashes map
MetaData* MetaDb::find(const HashBase &h) const {
	logTrace(
		METADB, boost::format("Searching for %s: %s")
		% h.getType() % h.decode()
	);

	CHTMDIter i = m_hashes.find(h.getTypeId());

	if (i == m_hashes.end()) {
		// outer map key not found
		return 0;
	}

	CHWTMDIter j = (*i).second.find(HashWrapper(&h));
	if (j == (*i).second.end()) {
		// Inner map key not found
		return 0;
	}

	logTrace(METADB, "Found.");
	return (*j).second;
}

// Locate MetaData(s) by searching with file name by looking at m_filenames map
std::vector<MetaData*> MetaDb::find(const std::string &filename) const {
	logTrace(METADB, boost::format("Searching for filename %s") % filename);

	std::vector<MetaData*> ret;
	std::pair<CFNIter, CFNIter> i = m_filenames.equal_range(filename);
	if (i.first == i.second) {
		return ret;
	}
	for (CFNIter j = i.first; j != i.second; ++j) {
		ret.push_back((*j).second);
	}
	logTrace(METADB, boost::format("%d match(es) found.") % ret.size());
	return ret;
}

// Locate MetaData by searching with SharedFile
MetaData* MetaDb::find(SharedFile *sf) const {
	CSFMDIter i = m_sfToMd.find(sf);
	if (i == m_sfToMd.end()) {
		return 0;
	}
	return (*i).second;
}

// Locate SharedFile by searching with hash
SharedFile* MetaDb::findSharedFile(const HashBase &h) const {
	CHTSFIter i = m_hashToSF.find(h.getTypeId());
	if (i == m_hashToSF.end()) {
		return 0;
	}
	HashWrapper hw(&h);
	CHWTSFIter j = (*i).second.find(hw);
	if (j == (*i).second.end()) {
		return 0;
	}
	return (*j).second;
}

// Locate SharedFiless matching given file name
std::vector<SharedFile*> MetaDb::findSharedFile(
	const std::string &filename) const
{
	std::vector<SharedFile*> ret;
	std::pair<CNTSFIter, CNTSFIter> i = m_nameToSF.equal_range(filename);
	if (i.first == i.second) {
		return ret;
	}
	for (CNTSFIter j = i.first; j != i.second; j++) {
		ret.push_back((*j).second);
	}
	return ret;
}

void MetaDb::remSharedFile(SharedFile *sf) {
	MetaData *md = 0;

	// Remove from m_sfToMD map, remembering MetaData pointer
	SFMDIter iter = m_sfToMd.find(sf);
	if (iter != m_sfToMd.end()) {
		md = (*iter).second;
		m_sfToMd.erase(iter);
	}
	if (!md) {
//		logDebug(
//			"onSharedFileEvent: SharedFile doesn't have MetaData."
//		);
		return;
	}

	// Look up all file names found in metadata and erase them from
	// m_nameToSF map
	MetaData::NameIter it = md->namesBegin();
	while (it != md->namesEnd()) {
		NTSFIter j = m_nameToSF.find((*it++).first);
		if (j == m_nameToSF.end()) {
			continue;
		}
		m_nameToSF.erase(j);
	}

	// Look up all hash sets foudn in metadata and erase them from
	// m_hashToSF map
	for (uint32_t i = 0; i < md->getHashSetCount(); ++i) {
		HTSFIter j = m_hashToSF.find(
			md->getHashSet(i)->getFileHashTypeId()
		);
		if (j == m_hashToSF.end()) {
			continue;
		}
		HWTSFIter k = (*j).second.find(
			HashWrapper(&(md->getHashSet(i)->getFileHash()))
		);
		if (k == (*j).second.end()) {
			continue;
		}
		(*j).second.erase(k);
		if ((*j).second.size() == 0) {
			m_hashToSF.erase(j);
		}
	}
}


// MetaDb - Event Handling
// -----------------------

void MetaDb::onMetaDataEvent(MetaData *md, int evt) {
	CHECK_THROW(md != 0);

	// \todo Uh, yeah, boss, but how do we update SharedFile maps ?
	switch (evt) {
		case MD_ADDED_FILENAME:
			tryAddFileName(md, md->getName());
			break;
		case MD_ADDED_HASHSET:
			for (uint32_t i = 0; i < md->getHashSetCount(); ++i) {
				tryAddHashSet(md, md->getHashSet(i));
			}
			break;
		default:
			break;
	}
}

void MetaDb::onSharedFileEvent(SharedFile *sf, int evt) {
	if (evt != SF_DESTROY) {
		return;
	}
	remSharedFile(sf);
}

// Clears the entire contents of the Db.
void MetaDb::clear() {
	for (LIter i = m_list.begin(); i != m_list.end(); ++i) {
		delete *i;
	}
	m_list.clear();
	m_sfToMd.clear();
	m_filenames.clear();
	m_nameToSF.clear();
	m_hashToSF.clear();
	m_hashes.clear();
}

// "From down here we can make the whole wall collapse!"
// "Uh, yeah, boss, but how do we get out?"
//                              -- Goblin Digging Team
