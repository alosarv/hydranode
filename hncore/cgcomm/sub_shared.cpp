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

#include <hncore/cgcomm/sub_shared.h>
#include <hncore/cgcomm/sub_download.h> // for getId(PartData*)
#include <hncore/cgcomm/opcodes.h>
#include <hncore/cgcomm/tag.h>
#include <hncore/fileslist.h>
#include <hncore/sharedfile.h>
#include <hncore/partdata.h>
#include <hnbase/utils.h>
#include <hnbase/timed_callback.h>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/lambda/construct.hpp>
#include <boost/filesystem/operations.hpp>

namespace CGComm {
namespace Subsystem {
using namespace boost::lambda;
const std::string TRACE = "cgcomm.shared";
uint32_t getFId(SharedFile *f) {
	if (f->getMetaData()) {
		return getStableId(f->getMetaData());
	} else {
		return f->getId();
	}
}

// Shared::CacheEntry class
// ------------------------
Shared::CacheEntry::CacheEntry(SharedFile *file) : m_file(), m_id(), m_size(),
m_upSpeed(), m_uploaded(), m_partDataId(), m_dirty(true), m_zombie() {
	CHECK_THROW(file);
	update(file);
}

void Shared::CacheEntry::update(SharedFile *file) {
	if (file && m_id) {
		CHECK_THROW(getFId(file) == m_id);
	}
	if (file != 0) {
		m_file = file;
	}
	if (isZombie()) {
		return;
	}
	CHECK_THROW(m_file);
	if (m_id != getFId(m_file)) {
		m_id = getFId(m_file);
		m_dirty = true;
	}
	if (m_name != m_file->getName()) {
		m_name = m_file->getName();
		m_dirty = true;
	}
	if (m_size != m_file->getSize()) {
		m_size = m_file->getSize();
		m_dirty = true;
	}
	if (m_file->getPath().native_file_string() != m_location) {
		m_location = boost::filesystem::system_complete(
			m_file->getPath()
		).native_file_string();
		m_dirty = true;
	}
	uint32_t upSpeed = m_file->getUpSpeed();
	if (upSpeed != m_upSpeed) {
		m_upSpeed = upSpeed;
		m_dirty = true;
	}
	if (m_file->getUploaded() != m_uploaded) {
		m_uploaded = m_file->getUploaded();
		m_dirty = true;
	}
	if (m_file->getPartData()) {
		if (getFId(m_file->getPartData()) != m_partDataId) {
			m_partDataId = getFId(m_file->getPartData());
			m_dirty = true;
		}
	} else if (m_partDataId) {
		m_partDataId = 0;
		m_dirty = true;
	}
	if (m_file->getChildCount() != m_children.size()) {
		m_children.clear();
		Object::CIter j = m_file->begin();
		while (j != m_file->end()) {
			SharedFile *f = dynamic_cast<SharedFile*>(
				(*j).second
			);
			if (f) {
				m_children.push_back(getFId(f));
			}
			++j;
		}
		m_dirty = true;
	}
}

std::ostream& operator<<(std::ostream &o, const Shared::CacheEntry &c) {
	std::ostringstream tmp;
	uint16_t tagCount = 0;

	tmp << makeTag(TAG_FILENAME,  c.m_name),     ++tagCount;
	tmp << makeTag(TAG_FILESIZE,  c.m_size),     ++tagCount;
	tmp << makeTag(TAG_LOCATION,  c.m_location), ++tagCount;
	tmp << makeTag(TAG_UPSPEED,   c.m_upSpeed),  ++tagCount;
	tmp << makeTag(TAG_TOTALUP,   c.m_uploaded), ++tagCount;
	tmp << makeTag(TAG_PDPOINTER, c.m_partDataId), ++tagCount;

	for (size_t i = 0; i < c.m_children.size(); ++i) {
		tmp << makeTag(TAG_CHILD, c.m_children[i]), ++tagCount;
	}

	Utils::putVal<uint8_t>(o, OP_SHAREDFILE);
	Utils::putVal<uint16_t>(o, tmp.str().size());
	Utils::putVal<uint32_t>(o, c.m_id);
	Utils::putVal<uint16_t>(o, tagCount);
	Utils::putVal<std::string>(o, tmp.str(), tmp.str().size());

	logTrace(
		TRACE, boost::format("Sent shared file %s to GUI.") % c.m_name
	);

	return o;
}

// Shared class
// ------------
Shared::Shared(
	boost::function<void (const std::string&)> sendFunc
) : SubSysBase(SUB_SHARED, sendFunc), m_updateTimer() {
	SharedFile::getEventTable().addAllHandler(this, &Shared::onEvent);
	rebuildCache();
	Log::instance().addTraceMask(TRACE);
}

void Shared::handle(std::istream &i) try {
	uint8_t oc = Utils::getVal<uint8_t>(i);
	switch (oc) {
		case OC_GET:     sendList();   break;
		case OC_MONITOR: monitor(i);   break;
		case OC_ADD:     addShared(i); break;
		case OC_REMOVE:  remShared(i); break;
		default:
			logDebug(
				boost::format(
					"CGComm::Shared: Received unknown "
					"opcode from GUI: %s"
				) % Utils::hexDump(oc)
			);
			break;
	}
} catch (std::exception &e) {
	logError(boost::format("CGComm.Shared: %s") % e.what());
} MSVC_ONLY(;)

void Shared::rebuildCache() {
	for_each(m_cache.begin(), m_cache.end(), bind(delete_ptr(), __1));
	m_cache.clear();

	FilesList::SFIter it = FilesList::instance().begin();
	while (it != FilesList::instance().end()) {
		m_cache.insert(new CacheEntry(*it)), ++it;
	}
}

void Shared::sendList() {
	rebuildCache();
	if (m_cache.empty()) {
		return;
	}

	std::ostringstream tmp;
	uint32_t cnt = 0;
	for (CIter i = m_cache.begin(); i != m_cache.end(); ++i) {
		if (!(*i)->isDirty()) {
			continue;
		}
		SharedFile *f = (*i)->m_file;
		for (Object::CIter j = f->begin(); j != f->end(); ++j) {
			SharedFile *c = dynamic_cast<SharedFile*>((*j).second);
			if (!c) {
				continue;
			}
			FIter k = m_cache.get<2>().find(c);
			if (k == m_cache.get<2>().end()) {
				continue;
			}
			tmp << **k, ++cnt;
			(*k)->setDirty(false);
		}
		tmp << **i, ++cnt;
		(*i)->setDirty(false);
	}

	std::ostringstream packet;
	Utils::putVal<uint8_t>(packet, OC_LIST);
	Utils::putVal<uint32_t>(packet, cnt);
	Utils::putVal<std::string>(packet, tmp.str(), tmp.str().size());
	sendPacket(packet.str());
}

void Shared::onEvent(SharedFile *file, int event) try {
	FIter it = m_cache.get<2>().find(file);
	if (it == m_cache.get<2>().end() && event == SF_ADDED) {
		m_cache.insert(new CacheEntry(file));
	} else if (it != m_cache.get<2>().end() && event == SF_DESTROY) {
		(*it)->setZombie();
	} else if (it != m_cache.get<2>().end()) {
		if (event == SF_METADATA_ADDED) {
			changeId(it);
			it = m_cache.get<2>().find(file);
			CHECK_THROW(it != m_cache.get<2>().end());
		}
		(*it)->update(file);
	}
} catch (std::exception &e) {
	logError(
		boost::format("Unhandled exception in %s: %s")
		% __PRETTY_FUNCTION__ % e.what()
	);
} MSVC_ONLY(;)

void Shared::changeId(FIter c) {
	if ((*c)->getId() == getFId((*c)->m_file)) {
		return;
	}
	uint32_t oldId = (*c)->getId();
	uint32_t newId = getFId((*c)->m_file);
	CacheEntry *cc = *c;

	// preconditions
	CHECK_THROW(m_cache.get<1>().find(newId) == m_cache.get<1>().end());

	m_cache.get<2>().modify(
		c, boost::lambda::bind(&CacheEntry::m_id, __1) = newId
	);

	// postconditions
	CHECK_THROW(cc->getId() == newId);
	CHECK_THROW(m_cache.get<1>().find(newId) != m_cache.get<1>().end());

	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OC_CHANGEID);
	Utils::putVal<uint32_t>(tmp, oldId);
	Utils::putVal<uint32_t>(tmp, newId);
	sendPacket(tmp.str());
}

void Shared::onMonitorTimer() try {
	if (m_updateTimer) {
		Utils::timedCallback(
			this, &Shared::onMonitorTimer, m_updateTimer
		);
	}

	std::ostringstream tmp;
	uint32_t cnt = 0;
	std::vector<CIter> toRemove;
	std::vector<CacheEntry*> toRemoveObj;
	for (CIter i = m_cache.begin(); i != m_cache.end(); ++i) {
		if (!(*i)->isZombie()) {
			(*i)->update(0);
		}
		if ((*i)->isDirty() && !(*i)->isZombie()) {
			SharedFile *f = (*i)->m_file;
			for (Object::CIter j = f->begin(); j != f->end(); ++j) {
				SharedFile *c = dynamic_cast<SharedFile*>(
					(*j).second
				);
				if (!c) {
					continue;
				}
				FIter k = m_cache.get<2>().find(c);
				if (k == m_cache.get<2>().end()) {
					continue;
				} 
				if (!(*k)->isZombie()) {
					(*k)->update(0);
				}
				if ((*k)->isDirty()) {
					tmp << **k, ++cnt;
					(*k)->setDirty(false);
				}
			}
		}
		if ((*i)->isDirty()) {
			tmp << **i, ++cnt;
			(*i)->setDirty(false);
		}
		if ((*i)->isZombie()) {
			toRemove.push_back(i);
			toRemoveObj.push_back(*i);
		}
	}
	if (cnt || toRemove.size()) {
		logTrace(
			TRACE, boost::format(
				"Monitor: %d in cache, %d out-of-date, "
				"%d zombie"
			) % m_cache.size() % cnt % toRemove.size()
		);
	}
	if (cnt) {
		std::ostringstream final;
		Utils::putVal<uint8_t>(final, OC_UPDATE);
		Utils::putVal<uint32_t>(final, cnt);
		Utils::putVal<std::string>(final, tmp.str(), tmp.str().size());
		sendPacket(final.str());
	}

	while (toRemove.size()) {
		std::ostringstream remove;
		Utils::putVal<uint8_t>(remove, OC_REMOVE);
		Utils::putVal<uint32_t>(remove, (*toRemove.back())->getId());
		sendPacket(remove.str());

		m_cache.erase(toRemove.back());
		toRemove.pop_back();
	}
	for_each(
		toRemoveObj.begin(), toRemoveObj.end(), bind(delete_ptr(),__1)
	);
	toRemoveObj.clear();

} catch (std::exception &e) {
	logError(
		boost::format("Unhandled exception in %s: %s")
		% __PRETTY_FUNCTION__ % e.what()
	);
	logDebug(boost::format("Cache state:"));
	boost::format fmt("%10d => %s");
	for (CIter i = m_cache.begin(); i != m_cache.end(); ++i) {
		logDebug(fmt % (*i)->getId() % (*i)->getName());
	}
} MSVC_ONLY(;)

void Shared::monitor(std::istream &i) {
	uint32_t timer = Utils::getVal<uint32_t>(i);
	if (!m_updateTimer && timer) {
		Utils::timedCallback(this, &Shared::onMonitorTimer, timer);
		logDebug(
			boost::format(
				"CGComm: Monitoring shared files list "
				"(update rate %dms)"
			) % timer
		);
	}
	m_updateTimer = timer;
}

void Shared::addShared(std::istream &i) {
	uint16_t len = Utils::getVal<uint16_t>(i);
	std::string dir = Utils::getVal<std::string>(i, len);
	logDebug("Adding shared directory " + dir);
	try {
		FilesList::instance().addSharedDir(dir, true);
	} catch (std::exception &e) {
		logError(
			boost::format("Adding shared directory %s: %s") 
			% dir % e.what()
		);
	}
}

void Shared::remShared(std::istream &i) {
	uint16_t len = Utils::getVal<uint16_t>(i);
	std::string dir = Utils::getVal<std::string>(i, len);
	logDebug("Removing shared directory " + dir);
	try {
		FilesList::instance().remSharedDir(dir, true);
	} catch (std::exception &e) {
		logError(
			boost::format("Removing shared directory %s: %s")
			% dir % e.what()
		);
	}
}

} // end namespace Subsystem
} // end namespace CGComm
