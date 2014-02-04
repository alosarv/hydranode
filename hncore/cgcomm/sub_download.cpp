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

#include <hncore/cgcomm/sub_download.h>
#include <hncore/cgcomm/opcodes.h>
#include <hncore/cgcomm/tag.h>
#include <hncore/fileslist.h>
#include <hncore/partdata.h>
#include <hncore/partdata_impl.h>
#include <hncore/search.h>
#include <hncore/metadata.h>
#include <hnbase/utils.h>
#include <hnbase/timed_callback.h>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/lambda/construct.hpp>
#include <boost/filesystem/operations.hpp>

namespace CGComm {
namespace Subsystem {
using namespace boost::lambda;
const std::string TRACE = "cgcomm.download";

// Download::CacheEntry class
// --------------------------
Download::CacheEntry::CacheEntry(PartData *file)
: m_file(), m_id(), m_state(DSTATE_RUNNING), m_size(), m_sourceCnt(),
m_fullSourceCnt(), m_completed(), m_avail(), m_dirty(true), m_zombie(false) {
	CHECK_THROW(file);
	update(file);
	if (file->isPaused()) { 
		m_state = DSTATE_PAUSED; 
	} else if (file->isStopped()) {
		m_state = DSTATE_STOPPED;
	}
}

void Download::CacheEntry::update(PartData *file, FileState newState) {
	if (file && m_id) {
		CHECK_THROW(getFId(file) == m_id)
	}

	if (m_state != newState && newState != DSTATE_KEEP) {
		m_state = newState;
		m_dirty = true;
	}
	if (newState == DSTATE_COMPLETE) {
		m_completed = m_size;
		m_sourceCnt = 0;
		m_fullSourceCnt = 0;
		m_speed = 0;
		m_dirty = true;
		m_location = boost::filesystem::system_complete(
			m_file->getDestination()
		).native_file_string();
	} else if (newState == DSTATE_CANCELED) {
		m_completed = 0;
		m_sourceCnt = 0;
		m_fullSourceCnt = 0;
		m_speed = 0;
		m_dirty = true;
	} else {
		if (file != 0) {
			m_file = file;
		}
		if (isZombie()) {
			return;
		}
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
		std::string tmp = boost::filesystem::system_complete(
			m_file->getDestination().branch_path()
		).native_directory_string();
		if (m_destination != tmp) {
			m_destination = tmp;
			m_dirty = true;
		}
		// rewrite 'location' field for complete downloads to allow
		// user interfaces to 'open completed file' on double-click
		// using only the m_location data (destination can change
		// during completing procedure)
		if (m_state == DSTATE_COMPLETE) {
			tmp = boost::filesystem::system_complete(
				m_file->getDestination()
			).native_file_string();
		} else {
			tmp = boost::filesystem::system_complete(
				m_file->getLocation()
			).native_file_string();
		}
		if (m_location != tmp) {
			m_location = tmp;
			m_dirty = true;
		}
		if (m_sourceCnt != m_file->getSourceCnt()) {
			m_sourceCnt = m_file->getSourceCnt();
			m_dirty = true;
		}
		if (m_fullSourceCnt != m_file->getFullSourceCnt()) {
			m_fullSourceCnt = m_file->getFullSourceCnt();
			m_dirty = true;
		}
		if (m_fullSourceCnt && m_avail < 100) {
			m_avail = 100;
			m_dirty = true;
		}
		if (!m_fullSourceCnt && m_file->getSize()) {
			// calculate availability
			Detail::ChunkMap &c = m_file->getChunks();
			RangeList64 r;
			Detail::CMPosIndex::iterator it = c.begin(); 
			while (it != c.end()) {
				if ((*it).getAvail()) {
					r.merge(*it);
				}
				++it;
			}
			uint64_t sum = 0;
			RangeList64::Iter i = r.begin(); 
			while (i != r.end()) {
				sum += (*i).length();
				++i;
			}
			uint8_t avail = sum * 100ull / m_file->getSize();
			if (avail != m_avail) {
				m_avail = avail;
				m_dirty = true;
			}
		}
		if (m_completed != m_file->getCompleted()) {
			m_completed = m_file->getCompleted();
			m_dirty = true;
		}
		uint32_t downSpeed = m_file->getDownSpeed();
		if (m_state != DSTATE_COMPLETE && m_state != DSTATE_CANCELED) {
			if (m_speed != downSpeed) {
				m_speed = downSpeed;
				m_dirty = true;
			}
		}
		if (m_file->getChildCount() != m_children.size()) {
			m_children.clear();
			Object::CIter j = m_file->begin();
			while (j != m_file->end()) {
				PartData *f = dynamic_cast<PartData*>(
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
}

std::ostream& operator<<(std::ostream &o, const Download::CacheEntry &c) {
	std::ostringstream tmp;
	uint16_t tagCount = 0;
	tmp << makeTag(TAG_FILENAME,   c.m_name),          ++tagCount;
	tmp << makeTag(TAG_FILESIZE,   c.m_size),          ++tagCount;
	tmp << makeTag(TAG_DESTDIR,    c.m_destination),   ++tagCount;
	tmp << makeTag(TAG_SRCCNT,     c.m_sourceCnt),     ++tagCount;
	tmp << makeTag(TAG_FULLSRCCNT, c.m_fullSourceCnt), ++tagCount;
	tmp << makeTag(TAG_COMPLETED,  c.m_completed),     ++tagCount;
	tmp << makeTag(TAG_DOWNSPEED,  c.m_speed),         ++tagCount;
	tmp << makeTag(TAG_LOCATION,   c.m_location),      ++tagCount;
	tmp << makeTag(TAG_AVAIL,      c.m_avail),         ++tagCount;
	tmp << makeTag(TAG_STATE, static_cast<uint32_t>(c.m_state)),++tagCount;
	for (size_t i = 0; i < c.m_children.size(); ++i) {
		tmp << makeTag(TAG_CHILD, c.m_children[i]), ++tagCount;
	}

	// custom tag for RangeList64 - need to hand-code this one
//	Utils::putVal<uint8_t>(tmp, TAG_COMPLETEDCHUNKS);
//	std::ostringstream tmp2;
//	tmp2 << f->getCompletedChunks();
//	Utils::putVal<uint16_t>(tmp, tmp2.str().size());
//	Utils::putVal<std::string>(tmp, tmp2.str(), tmp2.str().size());

	// finalize
	Utils::putVal<uint8_t>(o, OP_PARTDATA);
	Utils::putVal<uint16_t>(o, tmp.str().size());
	Utils::putVal<uint32_t>(o, c.m_id);
	Utils::putVal<uint16_t>(o, tagCount);
	Utils::putVal<std::string>(o, tmp.str(), tmp.str().size());

	logTrace(
		TRACE, boost::format("Sent download %s to GUI.") % c.m_name
	);
	return o;
}

// Download class
// --------------

// populate our internal list and set up event handlers
Download::Download(
	boost::function<void (const std::string&)> sendFunc
) : SubSysBase(SUB_DOWNLOAD, sendFunc), m_updateTimer() {
	PartData::getEventTable().addAllHandler(this, &Download::onEvent);
	rebuildCache();
	Log::instance().addTraceMask(TRACE);
}

void Download::handle(std::istream &i) try {
	uint8_t oc = Utils::getVal<uint8_t>(i);
	switch (oc) {
		case OC_GET:       sendList();     break;
		case OC_MONITOR:   monitor(i);     break;
		case OC_PAUSE:     pause(i);       break;
		case OC_STOP:      stop(i);        break;
		case OC_RESUME:    resume(i);      break;
		case OC_CANCEL:    cancel(i);      break;
		case OC_GETLINK:   getLink(i);     break;
		case OC_GETFILE:   getFile(i);     break;
		case OC_IMPORT:    import(i);      break;
		case OC_NAMES:     getNames(i);    break;
		case OC_COMMENTS:  getComments(i); break;
		case OC_LINKS:     getLinks(i);    break;
		case OC_SETNAME:   setName(i);     break;
		case OC_SETDEST:   setDest(i);     break;
		default:
			logDebug(
				boost::format(
					"CGComm::Download: Received "
					"unknown opcode from GUI: %s"
				) % Utils::hexDump(oc)
			);
			break;
	}
} catch (std::exception &e) {
	logError(boost::format("CGComm.Download: %s") % e.what());
} MSVC_ONLY(;)

void Download::rebuildCache() {
	for_each(m_cache.begin(), m_cache.end(), bind(delete_ptr(), __1));
	m_cache.clear();

	FilesList::SFIter it = FilesList::instance().begin();
	while (it != FilesList::instance().end()) {
		if ((*it)->getPartData()) {
			CacheEntry *c = new CacheEntry((*it)->getPartData());
			m_cache.insert(c);
		}
		++it;
	}
}

void Download::sendList() {
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
		PartData *f = (*i)->m_file;
		for (Object::CIter j = f->begin(); j != f->end(); ++j) {
			PartData *c = dynamic_cast<PartData*>((*j).second);
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

	logTrace(TRACE, boost::format("Sent %d downloads to GUI.") % cnt);
}


void Download::onEvent(PartData *file, int event) try {
	FIter i = m_cache.get<2>().find(file);
	std::string cs(i == m_cache.get<2>().end() ? "" : "(cached)");
	if (event == PD_CANCELED) {
		logDebug("CANCELED => " + file->getName() + cs);
	} else if (event == PD_DESTROY) {
		logDebug("DESTROYED => " + file->getName() + cs);
	}

	if (i == m_cache.get<2>().end() && event == PD_ADDED) {
		logTrace(TRACE,
			boost::format(
				"New temp file added that was not in cache: %s"
			) % file->getName()
		);
		CacheEntry *c = new CacheEntry(file);
		c->setDirty(true);
		m_cache.insert(c);
	} else if (i != m_cache.get<2>().end() && event == PD_DESTROY) {
		logTrace(TRACE,
			boost::format("File destroyed: %s") % file->getName()
		);
		(*i)->update(0, DSTATE_KEEP);
		(*i)->setZombie();
	} else if (i != m_cache.get<2>().end()) {
		logTrace(TRACE,
			boost::format("Received misc event from %s")
			% file->getName()
		);
		FileState newState = DSTATE_RUNNING;
		switch (event) {
			case PD_DL_FINISHED: newState = DSTATE_COMPLETE;  break;
			case PD_CANCELED:    newState = DSTATE_CANCELED;  break;
			case PD_VERIFYING:   newState = DSTATE_VERIFYING; break;
			case PD_MOVING:      newState = DSTATE_MOVING;    break;
			case PD_PAUSED:      newState = DSTATE_PAUSED;    break;
			case PD_STOPPED:     newState = DSTATE_STOPPED;   break;
			case PD_RESUMED:     newState = DSTATE_RUNNING;   break;
			default: break;
		}
		(*i)->update(file, newState);
	}
} catch (std::exception &e) {
	logError(
		boost::format("Unhandled exception in %s: %s")
		% __PRETTY_FUNCTION__ % e.what()
	);
} MSVC_ONLY(;)

void Download::onMonitorTimer() try {
	// schedule next update
	if (m_updateTimer) {
		Utils::timedCallback(
			this, &Download::onMonitorTimer, m_updateTimer
		);
	}

	std::ostringstream tmp;
	uint32_t cnt = 0;
	std::vector<CIter> toRemove;
	std::vector<CacheEntry*> toRemoveObj;
	for (CIter i = m_cache.begin(); i != m_cache.end(); ++i) {
		if (!(*i)->isZombie()) {
			(*i)->update(0, DSTATE_KEEP);
		}
		if ((*i)->isDirty() && !(*i)->isZombie()) {
			PartData *f = (*i)->m_file;
			for (Object::CIter j = f->begin(); j != f->end(); ++j) {
				PartData *c = dynamic_cast<PartData*>(
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
					(*k)->update(0, DSTATE_KEEP);
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
				"Monitor: %d in cache, "
				"%d out-of-date, %d zombie"
			) % m_cache.size() % cnt % toRemove.size()
		);
	}

	while (toRemove.size()) {
		m_cache.erase(toRemove.back());
		toRemove.pop_back();
	}
	for_each(
		toRemoveObj.begin(), toRemoveObj.end(), bind(delete_ptr(),__1)
	);
	toRemoveObj.clear();

	if (!cnt) {
		return;
	}
	std::ostringstream final;
	Utils::putVal<uint8_t>(final, OC_UPDATE);
	Utils::putVal<uint32_t>(final, cnt);
	Utils::putVal<std::string>(final, tmp.str(), tmp.str().size());

	sendPacket(final.str());
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

void Download::monitor(std::istream &i) {
	uint32_t timer = Utils::getVal<uint32_t>(i);
	if (!m_updateTimer && timer) {
		Utils::timedCallback(this, &Download::onMonitorTimer, timer);
		logDebug(
			boost::format(
				"CGComm: Monitoring DownloadList "
				"(update rate %dms)"
			) % timer
		);
	}
	m_updateTimer = timer;
}

void Download::pause(std::istream &i) {
	uint32_t num = Utils::getVal<uint32_t>(i);
	IDIter it = m_cache.get<1>().find(num);
	CHECK_THROW(it != m_cache.get<1>().end());
	(*it)->m_file->pause();
}

void Download::stop(std::istream &i) {
	uint32_t num = Utils::getVal<uint32_t>(i);
	IDIter it = m_cache.get<1>().find(num);
	CHECK_THROW(it != m_cache.get<1>().end());
	(*it)->m_file->stop();
}

void Download::resume(std::istream &i) {
	uint32_t num = Utils::getVal<uint32_t>(i);
	IDIter it = m_cache.get<1>().find(num);
	CHECK_THROW(it != m_cache.get<1>().end());
	(*it)->m_file->resume();
}

void Download::cancel(std::istream &i) {
	uint32_t num = Utils::getVal<uint32_t>(i);
	IDIter it = m_cache.get<1>().find(num);
	CHECK_THROW(it != m_cache.get<1>().end());
	(*it)->m_file->cancel();
}

void Download::getLink(std::istream &i) {
	uint16_t len = Utils::getVal<uint16_t>(i);
	std::string link = Utils::getVal<std::string>(i, len);
	Search::downloadLink(link);
}

void Download::getFile(std::istream &i) {
	uint32_t len = Utils::getVal<uint32_t>(i); // Notice: 32bit!
	std::string contents = Utils::getVal<std::string>(i, len);
	Search::downloadFile(contents);
}

void Download::import(std::istream &i) {
	uint16_t len = Utils::getVal<uint16_t>(i);
	boost::filesystem::path dir(
		Utils::getVal<std::string>(i, len),
		boost::filesystem::native
	);
	FilesList::instance().import(dir);
}

void Download::getNames(std::istream &i) {
	uint32_t id = Utils::getVal<uint32_t>(i);
	IDIter it = m_cache.get<1>().find(id);
	CHECK_THROW(it != m_cache.get<1>().end());
	MetaData *md = (*it)->m_file->getMetaData();
	CHECK_THROW(md);
	if (!md->getFileNameCount()) {
		return;
	}

	std::map<std::string, uint32_t> names;
	std::copy(
		md->namesBegin(), md->namesEnd(), 
		std::inserter(names, names.begin())
	);
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OC_NAMES);
	Utils::putVal<uint32_t>(tmp, id);
	Utils::putVal<uint16_t>(tmp, names.size());
	std::map<std::string, uint32_t>::const_iterator itor(names.begin());
	while (itor != names.end()) {
		std::string name = (*itor).first;
		Utils::putVal<uint16_t>(tmp, name.size());
		Utils::putVal<std::string>(tmp, name, name.size());
		Utils::putVal<uint32_t>(tmp, (*itor).second);
		++itor;
	}
	sendPacket(tmp.str());
}

void Download::getComments(std::istream &i) {
	uint32_t id = Utils::getVal<uint32_t>(i);
	IDIter it = m_cache.get<1>().find(id);
	CHECK_THROW(it != m_cache.get<1>().end());
	MetaData *md = (*it)->m_file->getMetaData();
	CHECK_THROW(md);
	if (!md->getCommentCount()) {
		return;
	}

	std::set<std::string> comments;
	std::copy(
		md->commentsBegin(), md->commentsEnd(), 
		std::inserter(comments, comments.begin())
	);
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OC_COMMENTS);
	Utils::putVal<uint32_t>(tmp, id);
	Utils::putVal<uint16_t>(tmp, comments.size());
	std::set<std::string>::const_iterator itor(comments.begin());
	while (itor != comments.end()) {
		Utils::putVal<uint16_t>(tmp, itor->size());
		Utils::putVal<std::string>(tmp, *itor, itor->size());
		++itor;
	}
	sendPacket(tmp.str());
}

void Download::setName(std::istream &i) {
	uint32_t id = Utils::getVal<uint32_t>(i);
	IDIter it = m_cache.get<1>().find(id);
	CHECK_THROW(it != m_cache.get<1>().end());
	uint16_t nameLen = Utils::getVal<uint16_t>(i);
	std::string newName = Utils::getVal<std::string>(i, nameLen);
	CHECK_THROW(nameLen);
	(*it)->m_file->setDestination(
		(*it)->m_file->getDestination().branch_path() / 
		boost::filesystem::path(newName, boost::filesystem::native)
	);
	(*it)->setDirty(true);
}

void Download::setDest(std::istream &i) {
	uint32_t id = Utils::getVal<uint32_t>(i);
	IDIter it = m_cache.get<1>().find(id);
	CHECK_THROW(it != m_cache.get<1>().end());
	uint16_t destLen = Utils::getVal<uint16_t>(i);
	std::string newDest = Utils::getVal<std::string>(i, destLen);
	CHECK_THROW(destLen);
	(*it)->m_file->setDestination(
		boost::filesystem::path(newDest, boost::filesystem::native) /
		(*it)->m_file->getDestination().leaf()
	);
	(*it)->setDirty(true);
}

void Download::getLinks(std::istream &i) {
	uint32_t id = Utils::getVal<uint32_t>(i);
	IDIter it = m_cache.get<1>().find(id);
	CHECK_THROW(it != m_cache.get<1>().end());
	std::vector<std::string> links;
	(*it)->m_file->getLinks((*it)->m_file, links);
	if (!links.size()) {
		return;
	}
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OC_LINKS);
	Utils::putVal<uint32_t>(tmp, id);
	Utils::putVal<uint16_t>(tmp, links.size());
	for (size_t i = 0; i < links.size(); ++i) {
		Utils::putVal<uint16_t>(tmp, links[i].size());
		Utils::putVal<std::string>(tmp, links[i], links[i].size());
	}
	sendPacket(tmp.str());
}

}
}
