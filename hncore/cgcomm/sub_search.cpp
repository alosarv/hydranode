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

#include <hncore/cgcomm/sub_search.h>
#include <hncore/search.h>
#include <hncore/cgcomm/tag.h>
#include <hncore/cgcomm/opcodes.h>
#include <hncore/partdata.h>
#include <hncore/fileslist.h>
#include <hncore/metadata.h>
#include <hnbase/utils.h>
#include <hnbase/timed_callback.h>
#include <boost/bind.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/lambda/construct.hpp>

namespace CGComm {
namespace Subsystem {
using namespace boost::lambda;

Search::CacheEntry::CacheEntry(uint32_t n, SearchResultPtr res) 
: m_id(n), m_size(), m_streamData(), m_dirty() {
	update(res);
}

void Search::CacheEntry::update(SearchResultPtr res) {
	if (m_name != res->getName()) {
		m_name = res->getName();
		m_dirty = true;
	}
	if (m_size != res->getSize()) {
		m_size = res->getSize();
		m_dirty = true;
	}
	if (m_sourceCnt != res->getSources()) {
		m_sourceCnt = res->getSources();
		m_dirty = true;
	}
	if (m_fullSourceCnt != res->getComplete()) {
		m_fullSourceCnt = res->getComplete();
		m_dirty = true;
	}
	if (res->getStrd() && !m_streamData) {
		m_streamData = new StreamData(*res->getStrd());
	} else if (m_streamData && *m_streamData != *res->getStrd()) {
		*m_streamData = *res->getStrd();
	}
}

std::ostream& operator<<(std::ostream &o, const Search::CacheEntry &c) {
	uint32_t tagCount = 0;
	std::ostringstream tags;
	tags << makeTag(TAG_FILENAME, c.m_name), ++tagCount;
	tags << makeTag(TAG_FILESIZE, c.m_size), ++tagCount;
	tags << makeTag(TAG_SRCCNT, c.m_sourceCnt), ++tagCount;
	tags << makeTag(TAG_FULLSRCCNT, c.m_fullSourceCnt), ++tagCount;
	if (c.m_streamData) {
		tags << makeTag(TAG_BITRATE, c.m_streamData->getBitrate());
		tags << makeTag(TAG_CODEC, c.m_streamData->getCodec());
		tags << makeTag(TAG_LENGTH, c.m_streamData->getLength());
		tagCount += 3;
	}
	Utils::putVal<uint32_t>(o, c.m_id);
	Utils::putVal<uint8_t>(o, tagCount);
	Utils::putVal<std::string>(o, tags.str(), tags.str().size());
	return o;
}

Search::Search(
	boost::function<void (const std::string&)> sendFunc
) : SubSysBase(SUB_SEARCH, sendFunc), m_lastResultCount() {}

Search::~Search() {
	if (m_currentSearch) {
		m_currentSearch->stop();
		for_each(
			m_cache.begin(), m_cache.end(), bind(delete_ptr(), __1)
		);
		m_cache.clear();
	}
}

void Search::handle(std::istream &i) try {
	uint8_t oc = Utils::getVal<uint8_t>(i);
	switch (oc) {
		case OC_GET:      perform(i);  break;
		case OC_DOWNLOAD: download(i); break;
		default: break;
	}
} catch (std::exception &e) {
	logError(boost::format("CGComm.Search: %s") % e.what());
} MSVC_ONLY(;)

void Search::perform(std::istream &i) try {
	using namespace ::Utils;
	std::string str; // search string
	uint64_t minSize = 0;
	uint64_t maxSize = 0;
	uint32_t filetype = FT_UNKNOWN;
	uint16_t tc = getVal<uint16_t>(i);
	while (i && tc--) {
		uint8_t oc = getVal<uint8_t>(i);
		uint16_t len = getVal<uint16_t>(i);
		switch (oc) {
			case TAG_KEYWORDS:
				str = getVal<std::string>(i, len);
				break;
			case TAG_MINSIZE:
				minSize = getVal<uint64_t>(i);
				break;
			case TAG_MAXSIZE:
				maxSize = getVal<uint64_t>(i);
				break;
			case TAG_FILETYPE:
				filetype = getVal<uint32_t>(i);
				break;
			default:
				i.seekg(len, std::ios::cur);
				break;
		}
	}

	if (m_currentSearch) {
		m_currentSearch->stop();
		m_currentSearch.reset();
		for_each(
			m_cache.begin(), m_cache.end(), bind(delete_ptr(), __1)
		);
		m_cache.clear();
		m_lastResultCount = 0;
	}

	if (!str.size()) {
		logError("Cannot search without keywords.");
		return;
	}
	logDebug("CgComm> Search.keywords=" + str);

	m_currentSearch.reset(new ::Search(str));
	m_currentSearch->setType(FileType(filetype));
	if (minSize) {
		logDebug(boost::format("CgComm> Search.minSize=%d") % minSize);
		m_currentSearch->setMinSize(minSize);
	}
	if (maxSize) {
		logDebug(boost::format("CgComm> Search.maxSize=%d") % maxSize);
		m_currentSearch->setMaxSize(maxSize);
	}
	m_currentSearch->run();
	Utils::timedCallback(boost::bind(&Search::sendUpdates, this), 700);
} catch (std::exception &e) {
	logError(boost::format("Error starting search: %s") % e.what());
	if (m_currentSearch) {
		m_currentSearch->stop();
		m_currentSearch.reset();
	}
}
MSVC_ONLY(;)

/**
 * Functor changing the destination path of temp files.
 */
struct DestChanger {
	DestChanger(const boost::filesystem::path &dest) : m_newDest(dest) {}
	void operator()(PartData *file) const {
		using namespace boost::filesystem;
		file->setDestination(m_newDest / path(file->getName(), native));
	}
	boost::filesystem::path m_newDest;
};

void Search::download(std::istream &i) {
	using namespace boost::filesystem;

	logDebug(
		boost::format("CurrentSearch has %d results in it.")
		% m_currentSearch->getResultCount()
	);

	uint32_t id = Utils::getVal<uint32_t>(i);
	boost::signals::scoped_connection c;
	try {
		uint16_t destLen = Utils::getVal<uint16_t>(i);
		std::string dest = Utils::getVal<std::string>(i, destLen);
		if (dest.size()) {
			c = FilesList::instance().onDownloadCreated.connect(
				DestChanger(path(dest, native))
			);
		}
	} catch (...) {}

	if (m_currentSearch && id <= m_currentSearch->getResultCount()) {
		logDebug(
			boost::format("Starting download from searchResult #%d")
			% id
		);
		m_currentSearch->getResult(id)->download();
	} else {
		logDebug(boost::format("No such search result (%d)") % id);
	}
}

void Search::sendUpdates() {
	if (!m_currentSearch) {
		return;
	}
	using namespace Utils;
	Utils::StopWatch s1;

	std::ostringstream tmp;
	uint32_t cnt = 0;
	for (size_t i = 0; i < m_cache.size(); ++i) {
		m_cache[i]->update(m_currentSearch->getResult(i));
		if (m_cache[i]->isDirty()) {
			tmp << *m_cache[i], ++cnt;
			m_cache[i]->setDirty(false);
		}
	}
	while (m_cache.size() < m_currentSearch->getResultCount()) {
		m_cache.push_back(
			new CacheEntry(
				m_cache.size(),
				m_currentSearch->getResult(m_cache.size())
			)
		);
		tmp << *m_cache[m_cache.size() - 1], ++cnt;
	}
	if (cnt) {
		std::ostringstream packet;
		putVal<uint8_t>(packet, OP_SEARCHRESULTS);
		putVal<uint32_t>(packet, cnt);
		putVal<std::string>(packet, tmp.str(), tmp.str().size());
		sendPacket(packet.str());
//		m_lastResultCount = ptr->getResultCount();
		logDebug(
			boost::format("Sent %d results to GUI (%dms)")
			% cnt % s1
		);
	}
	Utils::timedCallback(boost::bind(&Search::sendUpdates, this), 700);
}


}
}
