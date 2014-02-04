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

#include <hncgcomm/utils.h>
#include <hncgcomm/osdep.h>
#include <hncgcomm/cgcomm.h>
#include <hncgcomm/tag.h>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/format.hpp>

using namespace CGComm;

namespace Engine {
namespace Detail {
	struct OCExtractor {
		typedef uint8_t result_type;
		result_type operator()(const SubSysBase *const s) const {
			return s->getSubCode();
		}
	};
	struct MainSubMap : public boost::multi_index_container<
		SubSysBase*,
		boost::multi_index::indexed_by<
			boost::multi_index::ordered_unique<
				boost::multi_index::identity<SubSysBase*>
			>,
			boost::multi_index::ordered_non_unique<OCExtractor>
		>
	> {};
	typedef MainSubMap::nth_index<0>::type::iterator Iter;
	typedef MainSubMap::nth_index<1>::type::iterator OIter;
}

boost::signal<void (const std::string&)> debugMsg;
void logDebug(const boost::format &fmt) { debugMsg(fmt.str()); }
void logDebug(const std::string &msg)   { debugMsg(msg);       }

// Main class
// ----------
Main::Main(SendFunc func) : sendData(func), m_list(new Detail::MainSubMap) {
}

Main::~Main() {
	delete m_list;
}

void Main::parse(const std::string &data) {
	m_buffer.append(data);
	while (m_buffer.size() >= 6) {
		std::istringstream tmp(m_buffer);
		uint8_t subsys = Utils::getVal<uint8_t>(tmp);
		uint32_t size = Utils::getVal<uint32_t>(tmp);
		if (m_buffer.size() < size + 5u) {
			return;
		}
		Detail::OIter it = m_list->get<1>().find(subsys);
		if (it != m_list->get<1>().end()) {
			std::istringstream packet(m_buffer.substr(5, size));
			try {
				(*it)->handle(packet);
			} catch (std::exception &e) {
				logDebug(boost::format(
					"Unhandled exception in CGComm: %s"
				) % e.what());
				logDebug(boost::format(
					"Packet: subsys=%s size=%s contents:\n%s"
				) % Utils::hexDump(subsys) 
				% Utils::hexDump(size) % Utils::hexDump(
					packet.str()
				));
			}
		} else {
			logDebug(
				boost::format(
					"Received %d bytes data from "
					"unknown subsys %s"
				) % size % Utils::hexDump(subsys)
			);
		}
		m_buffer.erase(0, size + 5);
	}
}

void Main::addSubSys(SubSysBase *sys) {
	m_list->insert(sys);
}

void Main::delSubSys(SubSysBase *sys) {
	m_list->erase(sys);
}

void Main::shutdownEngine() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, 0x00);
	Utils::putVal<uint32_t>(tmp, 0x01);
	Utils::putVal<uint8_t>(tmp, 0x01); // shutdown command
	sendData(tmp.str());
}

// SubSysBase class
// ----------------
SubSysBase::SubSysBase(Main *parent, uint8_t subCode)
: m_parent(parent), m_subCode(subCode) {
	m_parent->addSubSys(this);
}

SubSysBase::~SubSysBase() {
	m_parent->delSubSys(this);
}

uint8_t SubSysBase::getSubCode() const {
	return m_subCode;
}
void SubSysBase::sendPacket(const std::string &data) {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, m_subCode);
	Utils::putVal<uint32_t>(tmp, data.size());
	Utils::putVal(tmp, data.data(), data.size());
	m_parent->sendData(tmp.str());
}

/////////////////////////
// Searching Subsystem //
/////////////////////////

// SearchResult class
// ------------------
SearchResult::SearchResult(Search *parent)
: m_parent(parent), m_size(), m_sources(), m_fullSources(), m_num(), 
m_bitrate(), m_length() {}

void SearchResult::download() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OC_DOWNLOAD);
	Utils::putVal<uint32_t>(tmp, m_num);
	m_parent->sendPacket(tmp.str());
}

void SearchResult::download(const std::string &dest) {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OC_DOWNLOAD);
	Utils::putVal<uint32_t>(tmp, m_num);
	Utils::putVal<uint16_t>(tmp, dest.size());
	Utils::putVal(tmp, dest.data(), dest.size());
	m_parent->sendPacket(tmp.str());
}

void SearchResult::update(SearchResultPtr res) {
	m_name = res->getName();
	m_size = res->getSize();
	m_sources = res->getSources();
	m_fullSources = res->getFullSources();
	onUpdated();
}

// Search class
// ------------
Search::Search(
	Main *parent, ResultHandler handler,
	const std::string &keywords, FileType ft
) : SubSysBase(parent, SUB_SEARCH), m_sigResults(handler), m_keywords(keywords),
m_minSize(), m_maxSize(), m_fileType(ft), m_lastNum() {
}

void Search::addKeywords(const std::string &keywords) {
	m_keywords.append(" " + keywords);
}

void Search::setType(FileType ft) {
	m_fileType = ft;
}

void Search::setMinSize(uint64_t size) {
	m_minSize = size;
}

void Search::setMaxSize(uint64_t size) {
	m_maxSize = size;
}

void Search::run() {
	m_lastNum = 0;

	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OC_GET);
	std::ostringstream tmp2;
	uint8_t tc = 2;
	tmp2 << makeTag(TAG_KEYWORDS, m_keywords);
	tmp2 << makeTag<uint32_t>(TAG_FILETYPE, m_fileType);

	if (m_minSize) {
		tmp2 << makeTag(TAG_MINSIZE, m_minSize);
		++tc;
	}
	if (m_maxSize) {
		tmp2 << makeTag(TAG_MAXSIZE, m_maxSize);
		++tc;
	}
	Utils::putVal<uint16_t>(tmp, tc);
	Utils::putVal(tmp, tmp2.str().data(), tmp2.str().size());
	sendPacket(tmp.str());
}

void Search::handle(std::istream &i) {
	uint8_t opcode = Utils::getVal<uint8_t>(i);
	if (opcode != OC_LIST) {
		return;
	}
	std::vector<SearchResultPtr> list;
	uint32_t cnt = Utils::getVal<uint32_t>(i);
	while (i && cnt--) {
		uint32_t id = Utils::getVal<uint32_t>(i);
		uint8_t tc = Utils::getVal<uint8_t>(i);
		SearchResultPtr res(new SearchResult(this));
		res->m_num = id;
		while (i && tc--) {
			uint8_t toc = Utils::getVal<uint8_t>(i);
			uint16_t tsz = Utils::getVal<uint16_t>(i);
			switch (toc) {
			case TAG_FILENAME:
				res->m_name =Utils::getVal<std::string>(i, tsz);
				break;
			case TAG_FILESIZE:
				res->m_size = Utils::getVal<uint64_t>(i);
				break;
			case TAG_SRCCNT:
				res->m_sources = Utils::getVal<uint32_t>(i);
				break;
			case TAG_FULLSRCCNT:
				res->m_fullSources = Utils::getVal<uint32_t>(i);
				break;
			case TAG_BITRATE:
				res->m_bitrate = Utils::getVal<uint32_t>(i);
				break;
			case TAG_CODEC:
				res->m_codec = Utils::getVal<std::string>(i, tsz);
				break;
			case TAG_LENGTH:
				res->m_length = Utils::getVal<uint32_t>(i);
				break;
			default:
				logDebug(boost::format("Unknown tag %d (len=%d)") % (int)tc % tsz);
				i.seekg(tsz, std::ios::cur);
				break;
			}
		}
		assert(res->m_name.size());
//		if (!res->m_name.size()) {
//			continue;
//		}
		if (m_results.find(id) == m_results.end()) {
			list.push_back(res);
			m_results[id] = res;
		} else {
			m_results[id]->update(res);
		}
	}
	if (list.size()) {
		m_sigResults(list);
	}
}

///////////////////////////
// Downloading Subsystem //
///////////////////////////

// DownloadInfo class
// ------------------
DownloadInfo::DownloadInfo(DownloadList *parent, uint32_t id)
: m_size(), m_completed(), m_srcCnt(), m_fullSrcCnt(), m_avail(), m_id(id),
m_parent(parent) {}

DownloadInfo::~DownloadInfo() {
	onDeleted();
}

void DownloadInfo::update(const DownloadInfo &o) {
	assert(m_id == o.m_id);
	m_name = o.m_name;
	m_size = o.m_size;
	m_completed = o.m_completed;
	m_destDir = o.m_destDir;
	m_location = o.m_location;
	m_srcCnt = o.m_srcCnt;
	m_fullSrcCnt = o.m_fullSrcCnt;
	m_speed = o.m_speed;
	m_state = o.m_state;
	m_avail = o.m_avail;
	onUpdated();
}

void DownloadInfo::pause() {
	m_parent->pause(shared_from_this());
}

void DownloadInfo::stop() {
	m_parent->stop(shared_from_this());
}

void DownloadInfo::resume() {
	m_parent->resume(shared_from_this());
}

void DownloadInfo::cancel() {
	m_parent->cancel(shared_from_this());
}

void DownloadInfo::getNames() {
	m_parent->getNames(shared_from_this());
}

void DownloadInfo::getComments() {
	m_parent->getComments(shared_from_this());
}

void DownloadInfo::getLinks() {
	m_parent->getLinks(shared_from_this());
}

void DownloadInfo::setName(const std::string &newName) {
	if (getName() != newName) {
		m_parent->setName(shared_from_this(), newName);
	}
}

void DownloadInfo::setDest(const std::string &newDest) {
	if (getDestDir() != newDest) {
		m_parent->setDest(shared_from_this(), newDest);
	}
}

// same as in engine partdata.h
enum { OP_PARTDATA = 0x90 };

// DownloadList class
// ------------------
DownloadList::DownloadList(Main *parent) : SubSysBase(parent, SUB_DOWNLOAD) {
}

void DownloadList::getList() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OC_GET);
	sendPacket(tmp.str());
}

void DownloadList::monitor(uint32_t interval) {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OC_MONITOR);
	Utils::putVal<uint32_t>(tmp, interval);
	sendPacket(tmp.str());
}

void DownloadList::downloadFromLink(const std::string &link) {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OC_GETLINK);
	Utils::putVal<uint16_t>(tmp, link.size());
	Utils::putVal(tmp, link, link.size());
	sendPacket(tmp.str());
}

void DownloadList::downloadFromFile(const std::string &contents) {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OC_GETFILE);
	Utils::putVal<uint32_t>(tmp, contents.size()); // Notice: 32bit!
	Utils::putVal(tmp, contents, contents.size());
	sendPacket(tmp.str());
}

void DownloadList::importDownloads(const std::string &dir) {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OC_IMPORT);
	Utils::putVal<uint16_t>(tmp, dir.size());
	Utils::putVal(tmp, dir, dir.size());
	sendPacket(tmp.str());
}

void DownloadList::handle(std::istream &packet) {
	uint8_t oc = Utils::getVal<uint8_t>(packet);
	switch (oc) {
		case OC_NAMES:    foundNames(packet);    return;
		case OC_LINKS:    foundLinks(packet);    return;
		case OC_COMMENTS: foundComments(packet); return;
		case OC_LIST:
		case OC_UPDATE: break;
		default:
			logDebug(
				boost::format("downloadlist: unknown opcode %d") 
				% oc
			);
			return;
	}

	uint32_t cnt = Utils::getVal<uint32_t>(packet);
	std::vector<DownloadInfoPtr> list;
	while (packet && cnt--) {
		DownloadInfoPtr d;
		try {
			d = readDownload(packet);
		} catch (std::exception &e) {
			logDebug(
				boost::format("while parsing download: %s")
				% e.what()
			);
			continue;
		}

		Iter it = m_list.find(d->getId());
		if (it != m_list.end()) {
			(*it).second->update(*d);
			onUpdated((*it).second);
		} else {
			m_list[d->getId()] = d;
			onAdded(d);
		}
	}
	if (oc == OC_LIST) {
		onAddedList();
	} else if (oc == OC_UPDATE) {
		onUpdatedList();
	}
}

DownloadInfoPtr DownloadList::readDownload(std::istream &i) {
	uint8_t objCode = Utils::getVal<uint8_t>(i);
	uint16_t objSize = Utils::getVal<uint16_t>(i);
	if (objCode != OP_PARTDATA) {
		i.seekg(objSize, std::ios::cur);
		throw std::runtime_error(
			"Invalid ObjCode in DownloadList::readDownload!"
		);
	}

	uint32_t id = Utils::getVal<uint32_t>(i);
	DownloadInfoPtr obj(new DownloadInfo(this, id));

	uint16_t tc = Utils::getVal<uint16_t>(i); // tagcount
	while (i && tc--) {
		uint8_t toc = Utils::getVal<uint8_t>(i);
		uint16_t sz = Utils::getVal<uint16_t>(i);
		switch (toc) {
			case TAG_FILENAME:
				obj->m_name = Utils::getVal<std::string>(i, sz);
				break;
			case TAG_FILESIZE:
				obj->m_size = Utils::getVal<uint64_t>(i);
				break;
			case TAG_DESTDIR:
				obj->m_destDir = Utils::getVal<std::string>(
					i, sz
				);
				break;
			case TAG_LOCATION:
				obj->m_location = Utils::getVal<std::string>(
					i, sz
				);
				break;
			case TAG_SRCCNT:
				obj->m_srcCnt = Utils::getVal<uint32_t>(i);
				break;
			case TAG_FULLSRCCNT:
				obj->m_fullSrcCnt = Utils::getVal<uint32_t>(i);
				break;
			case TAG_COMPLETED:
				obj->m_completed = Utils::getVal<uint64_t>(i);
				break;
			case TAG_DOWNSPEED:
				obj->m_speed = Utils::getVal<uint32_t>(i);
				break;
			case TAG_STATE:
				obj->m_state = static_cast<DownloadState>(
					Utils::getVal<uint32_t>(i)
				);
				break;
			case TAG_AVAIL:
				obj->m_avail = Utils::getVal<uint8_t>(i);
				break;
			case TAG_CHILD: {
				Iter j = m_list.find(Utils::getVal<uint32_t>(i));
				if (j != m_list.end()) {
					obj->m_children.insert((*j).second);
				}
				break;
			}
			//! TODO: handle TAG_COMPLETEDCHUNKS (RangeList64)
			default:
				i.seekg(sz, std::ios::cur);
				break;
		}
	}

	return obj;
}

void DownloadList::foundNames(std::istream &packet) {
	uint32_t id = Utils::getVal<uint32_t>(packet);
	Iter it = m_list.find(id);
	if (it == m_list.end()) {
		return;
	}
	uint16_t nameCount = Utils::getVal<uint16_t>(packet);
	it->second->m_names.clear();
	while (packet && nameCount--) {
		uint16_t nameLen = Utils::getVal<uint16_t>(packet);
		std::string name = Utils::getVal<std::string>(packet, nameLen);
		uint32_t nameFreq = Utils::getVal<uint32_t>(packet);
		it->second->m_names[name] = nameFreq;
	}
	it->second->onUpdated();
	onUpdated(it->second);
	onUpdatedNames(it->second);
}

void DownloadList::foundComments(std::istream &packet) {
	uint32_t id = Utils::getVal<uint32_t>(packet);
	Iter it = m_list.find(id);
	if (it == m_list.end()) {
		return;
	}
	uint16_t commentCount = Utils::getVal<uint16_t>(packet);
	it->second->m_comments.clear();
	while (packet && commentCount--) {
		uint16_t cLen = Utils::getVal<uint16_t>(packet);
		std::string comment = Utils::getVal<std::string>(packet, cLen);
		it->second->m_comments.insert(comment);
	}
	it->second->onUpdated();
	onUpdated(it->second);
	onUpdatedComments(it->second);
}

void DownloadList::foundLinks(std::istream &packet) {
	uint32_t id = Utils::getVal<uint32_t>(packet);
	std::map<uint32_t, DownloadInfoPtr>::iterator it = m_list.find(id);
	if (it == m_list.end()) {
		return;
	}
	uint16_t linkCount = Utils::getVal<uint16_t>(packet);
	it->second->m_links.clear();
	while (packet && linkCount--) {
		uint16_t lLen = Utils::getVal<uint16_t>(packet);
		std::string link = Utils::getVal<std::string>(packet, lLen);
		it->second->m_links.insert(link);
	}
	it->second->onUpdated();
	onUpdated(it->second);
	onUpdatedLinks(it->second);
}

void DownloadList::sendRequest(uint8_t oc, uint32_t id) {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, oc);
	Utils::putVal<uint32_t>(tmp, id);
	sendPacket(tmp.str());
}

void DownloadList::pause(DownloadInfoPtr d) {
	sendRequest(OC_PAUSE, d->m_id);
}

void DownloadList::stop(DownloadInfoPtr d) {
	sendRequest(OC_STOP, d->m_id);
}

void DownloadList::resume(DownloadInfoPtr d) {
	sendRequest(OC_RESUME, d->m_id);
}

void DownloadList::cancel(DownloadInfoPtr d) {
	sendRequest(OC_CANCEL, d->m_id);
}

void DownloadList::getNames(DownloadInfoPtr d) {
	sendRequest(OC_NAMES, d->m_id);
}

void DownloadList::getComments(DownloadInfoPtr d) {
	sendRequest(OC_COMMENTS, d->m_id);
}

void DownloadList::getLinks(DownloadInfoPtr d) {
	sendRequest(OC_NAMES, d->m_id);
}

void DownloadList::setName(DownloadInfoPtr d, const std::string &newName) {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OC_SETNAME);
	Utils::putVal<uint32_t>(tmp, d->m_id);
	Utils::putVal<uint16_t>(tmp, newName.size());
	Utils::putVal(tmp, newName, newName.size());
	sendPacket(tmp.str());
}

void DownloadList::setDest(DownloadInfoPtr d, const std::string &newDest) {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OC_SETDEST);
	Utils::putVal<uint32_t>(tmp, d->m_id);
	Utils::putVal<uint16_t>(tmp, newDest.size());
	Utils::putVal(tmp, newDest, newDest.size());
	sendPacket(tmp.str());
}

//////////////////////////
// Shared Files listing //
//////////////////////////
SharedFile::SharedFile(Engine::SharedFilesList *parent, uint32_t id)
: m_size(), m_uploaded(), m_speed(), m_id(id), m_partDataId(), m_parent(parent) 
{}

SharedFile::~SharedFile() {
}

void SharedFile::update(const Engine::SharedFile &o) {
	assert(m_id == o.m_id);
	m_name       = o.m_name;
	m_location   = o.m_location;
	m_size       = o.m_size;
	m_uploaded   = o.m_uploaded;
	m_speed      = o.m_speed;
	m_partDataId = o.m_partDataId;

	onUpdated();
}

SharedFilesList::SharedFilesList(Main *parent) : SubSysBase(parent, SUB_SHARED){
}

void SharedFilesList::getList() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OC_GET);
	sendPacket(tmp.str());
}

void SharedFilesList::monitor(uint32_t interval) {
	std::ostringstream o;
	Utils::putVal<uint8_t>(o, OC_MONITOR);
	Utils::putVal<uint32_t>(o, interval);
	sendPacket(o.str());
}

void SharedFilesList::handle(std::istream &packet) {
	uint8_t oc = Utils::getVal<uint8_t>(packet);
	if (oc == OC_REMOVE) {
		uint32_t id = Utils::getVal<uint32_t>(packet);
		Iter i = m_list.find(id);
		if (i != m_list.end()) {
			i->second->onDeleted();
			onRemoved(i->second);
			m_list.erase(i);
		}
		return;
	} else if (oc == OC_CHANGEID) {
		uint32_t oldId = Utils::getVal<uint32_t>(packet);
		uint32_t newId = Utils::getVal<uint32_t>(packet);
		Iter i = m_list.find(oldId);
		if (i != m_list.end()) {
			SharedFilePtr obj = i->second;
			m_list.erase(i);
			obj->m_id = newId;
			m_list[newId] = obj;
		}
		return;
	} else if (oc != OC_LIST && oc != OC_UPDATE) {
		logDebug(boost::format("sharedlist: unknown opcode %d") % oc);
		return; // others not implemented yet
	}

	uint32_t cnt = Utils::getVal<uint32_t>(packet);
	std::vector<SharedFilePtr> list;
	while (packet && cnt--) {
		SharedFilePtr d;
		try {
			d = readFile(packet);
		} catch (std::exception &e) {
			logDebug(
				boost::format("while parsing sharedfile: %s")
				% e.what()
			);
			continue;
		}

		std::map<uint32_t, SharedFilePtr>::iterator it;
		it = m_list.find(d->getId());
		if (it != m_list.end()) {
			(*it).second->update(*d);
			onUpdated((*it).second);
		} else {
			m_list[d->getId()] = d;
			onAdded(d);
		}
	}
	if (oc == OC_LIST) {
		onAddedList();
	} else if (oc == OC_UPDATE) {
		onUpdatedList();
	}
}

SharedFilePtr SharedFilesList::readFile(std::istream &i) {
	uint8_t objCode = Utils::getVal<uint8_t>(i);
	uint16_t objSize = Utils::getVal<uint16_t>(i);
	if (objCode != OP_SHAREDFILE) {
		i.seekg(objSize, std::ios::cur);
		throw std::runtime_error(
			"Invalid ObjCode in DownloadList::readDownload!"
		);
	}

	uint32_t id = Utils::getVal<uint32_t>(i);
	SharedFilePtr obj(new SharedFile(this, id));

	uint16_t tc = Utils::getVal<uint16_t>(i); // tagcount
	while (i && tc--) {
		uint8_t toc = Utils::getVal<uint8_t>(i);
		uint16_t sz = Utils::getVal<uint16_t>(i);
		switch (toc) {
			case TAG_FILENAME:
				obj->m_name = Utils::getVal<std::string>(i, sz);
				break;
			case TAG_FILESIZE:
				obj->m_size = Utils::getVal<uint64_t>(i);
				break;
			case TAG_LOCATION:
				obj->m_location = Utils::getVal<std::string>(
					i, sz
				);
				break;
			case TAG_TOTALUP:
				obj->m_uploaded = Utils::getVal<uint64_t>(i);
				break;
			case TAG_UPSPEED:
				obj->m_speed = Utils::getVal<uint32_t>(i);
				break;
			case TAG_CHILD: {
				Iter j = m_list.find(Utils::getVal<uint32_t>(i));
				if (j != m_list.end()) {
					obj->m_children.insert((*j).second);
				}
				break;
			}
			case TAG_PDPOINTER:
				obj->m_partDataId = Utils::getVal<uint32_t>(i);
				break;
			default:
				i.seekg(sz, std::ios::cur);
				break;
		}
	}
	return obj;
}

void SharedFilesList::addShared(const std::string &dir) {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OC_ADD);
	Utils::putVal<uint16_t>(tmp, dir.size());
	Utils::putVal(tmp, dir, dir.size());
	sendPacket(tmp.str());
}

void SharedFilesList::remShared(const std::string &dir) {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OC_REMOVE);
	Utils::putVal<uint16_t>(tmp, dir.size());
	Utils::putVal(tmp, dir, dir.size());
	sendPacket(tmp.str());
}

/////////////////////////////
// Configuration Subsystem //
/////////////////////////////

Config::Config(Main *parent, ListHandler handler)
: SubSysBase(parent, SUB_CONFIG), m_handler(handler) {}

void Config::handle(std::istream &i) {
	uint8_t oc = Utils::getVal<uint8_t>(i);
	if (oc == OC_LIST) {
		uint16_t cnt = Utils::getVal<uint16_t>(i);
		while (i && cnt--) {
			std::string key = Utils::getVal<std::string>(i);
			std::string val = Utils::getVal<std::string>(i);
			m_list[key] = val;
		}
		m_handler(m_list);
	} else if (oc == OC_DATA) {
		std::string key = Utils::getVal<std::string>(i);
		std::string val = Utils::getVal<std::string>(i);
		m_list[key] = val;
		m_handler(m_list);
	}
}

void Config::getValue(const std::string &key) {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OC_GET);
	Utils::putVal<std::string>(tmp, key);
	sendPacket(tmp.str());
}

void Config::setValue(const std::string &key, const std::string &value) {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OC_SET);
	Utils::putVal<std::string>(tmp, key);
	Utils::putVal<std::string>(tmp, value);
	sendPacket(tmp.str());
}

void Config::getList() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OC_LIST);
	sendPacket(tmp.str());
}

void Config::monitor() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OC_MONITOR);
	sendPacket(tmp.str());
}

///////////////////////
// Network subsystem //
///////////////////////
Network::Network(Main *parent, UpdateHandler handler) 
: SubSysBase(parent, SUB_NETWORK), m_upSpeed(), m_downSpeed(), m_connCnt(),
m_connectingCnt(), m_totalUp(), m_totalDown(), m_sessionUp(), m_sessionDown(),
m_upPackets(), m_downPackets(), m_upLimit(), m_downLimit(), m_sessLength(),
m_totalRuntime(), m_handler(handler) {
}

void Network::getList() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OC_GET);
	sendPacket(tmp.str());
}

void Network::monitor(uint32_t interval) {
	std::ostringstream o;
	Utils::putVal<uint8_t>(o, OC_MONITOR);
	Utils::putVal<uint32_t>(o, interval);
	sendPacket(o.str());
}

void Network::handle(std::istream &i) {
	using namespace Utils;
	if (Utils::getVal<uint8_t>(i) != OC_LIST) {
		return;
	}
	uint32_t tagCount = getVal<uint32_t>(i);
	while (i && tagCount--) {
		uint8_t oc = getVal<uint8_t>(i);
		uint16_t sz = getVal<uint16_t>(i);
		switch (oc) {
			case TAG_UPSPEED:
				m_upSpeed       = getVal<uint32_t>(i); break;
			case TAG_DOWNSPEED:
				m_downSpeed     = getVal<uint32_t>(i); break;
			case TAG_CONNCNT:
				m_connCnt       = getVal<uint32_t>(i); break;
			case TAG_CONNECTINGCNT:
				m_connectingCnt = getVal<uint32_t>(i); break;
			case TAG_TOTALUP:
				m_totalUp       = getVal<uint64_t>(i); break;
			case TAG_TOTALDOWN:
				m_totalDown     = getVal<uint64_t>(i); break;
			case TAG_SESSUP:
				m_sessionUp     = getVal<uint64_t>(i); break;
			case TAG_SESSDOWN:
				m_sessionDown   = getVal<uint64_t>(i); break;
			case TAG_DOWNPACKETS:
				m_downPackets   = getVal<uint64_t>(i); break;
			case TAG_UPPACKETS:
				m_upPackets     = getVal<uint64_t>(i); break;
			case TAG_UPLIMIT:
				m_upLimit       = getVal<uint32_t>(i); break;
			case TAG_DOWNLIMIT:
				m_downLimit     = getVal<uint32_t>(i); break;
			case TAG_RUNTIMESESS:
				m_sessLength    = getVal<uint64_t>(i); break;
			case TAG_RUNTIMETOTAL:
				m_totalRuntime  = getVal<uint64_t>(i); break;
			default:
				i.seekg(sz, std::ios::cur); break;
		}
	}
	m_handler();
}

///////////////////////
// Modules subsystem //
///////////////////////

Module::Module() : m_sessUp(), m_sessDown(), m_totalUp(), m_totalDown(),
m_upSpeed(), m_downSpeed(), m_id() {}

void Module::update(ModulePtr mod) {
	assert(m_id == mod->m_id);
	*this = *mod;
}

Object::Object(Modules *parent) : m_parentList(parent), m_id() {}

void Object::doOper(
	const std::string &opName,
	const std::map<std::string, std::string> &args
) {
	m_parentList->doObjectOper(shared_from_this(), opName, args);
}

void Object::setData(uint8_t num, const std::string &newValue) {
	m_parentList->setObjectData(shared_from_this(), num, newValue);
}

void Object::update(ObjectPtr obj) {
	m_data     = obj->m_data;
	m_childIds = obj->m_childIds;
	m_name     = obj->m_name;
	m_id       = obj->m_id;
}

void Object::findChildren() {
	m_children.clear();
	std::set<uint32_t>::iterator i = m_childIds.begin();
	while (i != m_childIds.end()) {
		ObjectPtr tmp = m_parentList->findObject(*i);
		if (tmp) {
			m_children[*i] = tmp;
			tmp->m_parent = shared_from_this();
			tmp->findChildren();
		}
		++i;
	}
}

void Object::destroy() {
	for (CIter i = begin(); i != end(); ++i) {
		i->second->destroy();
		m_children.erase(i->second->getId());
		m_childIds.erase(i->second->getId());
	}
	onDestroyed(shared_from_this());
}

Modules::Modules(Main *parent) : SubSysBase(parent, SUB_MODULES) {}

void Modules::handle(std::istream &i) {
	uint8_t oc = Utils::getVal<uint8_t>(i);
	switch (oc) {
		case OC_LIST:
		case OC_UPDATE: {
			m_list.clear();
			uint32_t cnt = Utils::getVal<uint32_t>(i);
			while (i && cnt--) {
				ModulePtr mod = readModule(i);
				m_list[mod->getId()] = mod;
			}
			onUpdated();
			break;
		}
		case OC_ADD: {
			ModulePtr mod = readModule(i);
			Iter j = m_list.find(mod->getId());
			if (j == m_list.end()) {
				m_list[mod->getId()] = mod;
				onAdded(mod);
			} else {
				(*j).second->update(mod);
			}
			onUpdated();
			break;
		}
		case OC_REMOVE: {
			ModulePtr mod = readModule(i);
			Iter j = m_list.find(mod->getId());
			if (j != m_list.end()) {
				onRemoved((*j).second);
				m_list.erase(j);
			}
			onUpdated();
			break;
		}
		case OC_NOTFOUND: {
			uint32_t id = Utils::getVal<uint32_t>(i);
			logDebug(boost::format("Object not found: %d") % id);
			break;
		}
		case OC_OBJLIST: {
			uint32_t cnt = Utils::getVal<uint32_t>(i);
			ObjectPtr lastObject = ObjectPtr();
			while (i && cnt--) {
				ObjectPtr obj = readObject(i);
				ObjectPtr found = findObject(obj->getId());
				if (found) {
					found->update(obj);
					updatedObject(found);
					obj = found;
				} else {
					m_objects[obj->getId()] = obj;
				}
				lastObject = obj;
			}
			if (lastObject) {
				lastObject->findChildren();
				receivedObject(lastObject);
			}
			break;
		}
		case OC_CADDED: {
			uint32_t id = Utils::getVal<uint32_t>(i);
			ObjectPtr parent = findObject(id);
			ObjectPtr child = readObject(i);
			ObjectPtr found = findObject(child->getId());
			if (found) {
				found->update(child);
				updatedObject(found);
				child = found;
			} else {
				m_objects[child->getId()] = child;
				if (parent) {
					parent->m_childIds.insert(id);
					parent->m_children[id] = child;
					parent->childAdded(parent, child);
					child->m_parent = parent;
				}
			}
			addedObject(child);
			break;
		}
		case OC_CREMOVED: {
			uint32_t id = Utils::getVal<uint32_t>(i);
			uint32_t cid = Utils::getVal<uint32_t>(i);
			ObjectPtr parent = findObject(id);
			ObjectPtr child = findObject(cid);
			if (parent && child) {
				parent->m_childIds.erase(cid);
				parent->m_children.erase(cid);
				parent->childRemoved(parent, child);
			}
			if (child) {
				removedObject(child);
			}
			break;
		}
		case OC_DESTROY: {
			uint32_t id = Utils::getVal<uint32_t>(i);
			ObjectPtr obj = findObject(id);
			if (obj) {
				obj->destroy();
				m_objects.erase(id);
			}
			break;
		}
	}
}

ObjectPtr Modules::readObject(std::istream &i) {
	if (Utils::getVal<uint8_t>(i) != OC_OBJECT) {
		throw std::runtime_error("Invalid object.");
	}

	ObjectPtr obj(new Object(this));
	obj->m_id = Utils::getVal<uint32_t>(i);
	uint16_t nLen = Utils::getVal<uint16_t>(i);
	obj->m_name = Utils::getVal<std::string>(i, nLen);
	uint32_t dataCount = Utils::getVal<uint32_t>(i);
	while (i && dataCount--) {
		uint16_t dLen = Utils::getVal<uint16_t>(i);
		std::string data = Utils::getVal<std::string>(i, dLen);
		obj->m_data.push_back(data);
	}
	uint32_t childCount = Utils::getVal<uint32_t>(i);
	while (i && childCount--) {
		obj->m_childIds.insert(Utils::getVal<uint32_t>(i));
	}

	return obj;
}

ModulePtr Modules::readModule(std::istream &i) {
	ModulePtr mod(new Module);
	uint8_t oc = Utils::getVal<uint8_t>(i);
	if (oc != OC_MODULE) {
		throw std::runtime_error("invalid module opcode");
	}
	mod->m_id = Utils::getVal<uint32_t>(i);
	uint16_t tagCount = Utils::getVal<uint16_t>(i);
	while (i && tagCount--) {
		uint8_t tc = Utils::getVal<uint8_t>(i);
		uint16_t len = Utils::getVal<uint16_t>(i);
		switch (tc) {
			case TAG_NAME: 
				mod->m_name = Utils::getVal<std::string>(i,len);
				break;
			case TAG_DESC:
				mod->m_desc = Utils::getVal<std::string>(i,len);
				break;
			case TAG_SESSUP:
				mod->m_sessUp = Utils::getVal<uint64_t>(i);
				break;
			case TAG_SESSDOWN:
				mod->m_sessDown = Utils::getVal<uint64_t>(i);
				break;
			case TAG_TOTALUP:
				mod->m_totalUp = Utils::getVal<uint64_t>(i);
				break;
			case TAG_TOTALDOWN:
				mod->m_totalDown = Utils::getVal<uint64_t>(i);
				break;
			case TAG_UPSPEED:
				mod->m_upSpeed = Utils::getVal<uint32_t>(i);
				break;
			case TAG_DOWNSPEED:
				mod->m_downSpeed = Utils::getVal<uint32_t>(i);
				break;
			default: break;
		}
	}
	return mod;
}

void Modules::getList() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OC_LIST);
	sendPacket(tmp.str());
}

void Modules::monitor(uint32_t interval) {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OC_MONITOR);
	Utils::putVal<uint32_t>(tmp, interval);
	sendPacket(tmp.str());
}

void Modules::getObject(
	ModulePtr mod, const std::string &name, bool recurse, uint32_t timer
) {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OC_GET);
	Utils::putVal<uint32_t>(tmp, mod->getId());
	Utils::putVal<uint16_t>(tmp, name.size());
	Utils::putVal(tmp, name, name.size());
	Utils::putVal<uint8_t>(tmp, recurse);
	Utils::putVal<uint32_t>(tmp, timer);
	sendPacket(tmp.str());
}

void Modules::monitorObject(ObjectPtr obj, uint32_t interval) {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OC_MONITOR);
	Utils::putVal<uint32_t>(tmp, obj->getId());
	Utils::putVal<uint32_t>(tmp, interval);
	sendPacket(tmp.str());
}

void Modules::doObjectOper(
	ObjectPtr obj, const std::string &opName,
	const std::map<std::string, std::string> &args
) {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OC_DOOPER);
	Utils::putVal<uint32_t>(tmp, obj->getId());
	Utils::putVal<uint16_t>(tmp, opName.size());
	Utils::putVal(tmp, opName, opName.size());
	Utils::putVal<uint16_t>(tmp, args.size());
	std::map<std::string, std::string>::const_iterator i(args.begin());
	while (i != args.end()) {
		Utils::putVal<uint16_t>(tmp, i->first.size());
		Utils::putVal(tmp, i->first, i->first.size());
		Utils::putVal<uint16_t>(tmp, i->second.size());
		Utils::putVal(tmp, i->second, i->second.size());
		++i;
	}
	sendPacket(tmp.str());
}

void Modules::setObjectData(ObjectPtr obj, uint8_t dNum, const std::string &v) {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OC_SET);
	Utils::putVal<uint32_t>(tmp, obj->getId());
	Utils::putVal<uint8_t>(tmp, dNum);
	Utils::putVal<uint16_t>(tmp, v.size());
	Utils::putVal(tmp, v, v.size());
	sendPacket(tmp.str());
}

} // end namespace Engine
