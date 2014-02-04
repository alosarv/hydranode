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
 * \file downloadlist.cpp Implementation of Download and DownloadList classes
 */

#include <hncore/ed2k/downloadlist.h>
#include <hncore/ed2k/clients.h>
#include <hncore/ed2k/clientext.h>
#include <hncore/ed2k/clientlist.h>        // used for dangling pointer checks
#include <hncore/ed2k/tag.h>               // needed by importer
#include <hncore/fileslist.h>
#include <hncore/sharedfile.h>
#include <hncore/metadata.h>
#include <hncore/partdata.h>
#include <boost/lambda/bind.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/filesystem/operations.hpp>      // needed by importer
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>

namespace Donkey {

const std::string TRACE_SRCEXCH("SourceExchange");

// Download class
// --------------
Download::Download(PartData *pd, const Hash<ED2KHash> &hash) : m_partData(pd),
m_hash(hash), m_lastSrcExch(), m_sourceLimit() {
	pd->getSourceCnt.connect(boost::bind(&Download::getSourceCount, this));
	pd->getLinks.connect(boost::bind(&Download::getLink, this, _1, _2));
}

Download::~Download() {}

void Download::getLink(PartData *file, std::vector<std::string>& links) {
	CHECK_RET(file == m_partData);
	boost::format fmt("ed2k://|file|%s|%d|%s|/");
	fmt % m_partData->getName() % m_partData->getSize() % m_hash.decode();
	std::string link = fmt.str();
	boost::algorithm::replace_all(link, " ", ".");
	links.push_back(link);
}

bool Download::isSourceReqAllowed(Client *c) const {
	if (c->getSrcExchVer() < 1) {
		return false; // no source-exchange support
	}
	if (m_sourceLimit && m_sources.size() >= m_sourceLimit) {
		return false; // got enough sources already
	}
	if (!m_partData->isRunning()) {
		return false;
	}
	uint64_t curTick = Utils::getTick();
	Detail::SourceInfoPtr src = c->getSourceInfo();

	// too early for this client
	if (src->getLastSrcExch() + SRCEXCH_CTIME > curTick) {
		return false;
	}

	// very rare file is asked from each client, once per 40 min
	if (getSourceCount() < 10) {
		return true;
	}

	// rare file asked once per 5 minutes
	if (m_lastSrcExch + SRCEXCH_FTIME > curTick) {
		return false;
	}

	// rare files asked once per 5 minutes from one client
	if (getSourceCount() < 50) {
		return true;
	}

	// not rare file - one query per 20 minutes per file
	if (m_lastSrcExch + SRCEXCH_FTIME * 4 > curTick) {
		return false;
	} else {
		return true;
	}

	return false;
}

std::vector<Download::Source> Download::getSources() const {
	std::vector<Source> tmp;
	for (CIter i = m_sources.begin(); i != m_sources.end(); ++i) {
		Source src(
			(*i)->getId(), (*i)->getTcpPort(),
			(*i)->getServerAddr().getIp(),
			(*i)->getServerAddr().getPort()
		);
		tmp.push_back(src);
	}
	return tmp;
}

uint32_t Download::getSize() const { return m_partData->getSize(); }

void Download::destroy() {
	CIter i = m_sources.begin();
	while (i != m_sources.end()) {
		CIter tmp(i++);
		if (ClientList::instance().valid(*tmp)) {
			(*tmp)->remOffered(this, false);
		} else {
			logDebug(
				"*** CORRUPTION: Dangling Client pointer "
				"found in Download::destroy()!"
			);
		}
	}
}

// DownloadList class
// ------------------

namespace Detail {
	struct MIDownloadList : boost::multi_index_container<
		Download*,
		boost::multi_index::indexed_by<
			boost::multi_index::ordered_unique<
				boost::multi_index::const_mem_fun<
					Download, PartData*,
					&Download::getPartData
				>
			>,
			boost::multi_index::ordered_unique<
				boost::multi_index::identity<
					Download*
				>
			>,
			boost::multi_index::ordered_unique<
				boost::multi_index::const_mem_fun<
					Download, Hash<ED2KHash>,
					&Download::getHash
				>
			>
		>
	> {};
	enum { ID_PD, ID_Download, ID_Hash, ID_UdpQTime };
	typedef MIDownloadList::nth_index<ID_PD>::type::iterator Iter;
	typedef MIDownloadList::nth_index<ID_Download>::type::iterator DIter;
	typedef MIDownloadList::nth_index<ID_Hash>::type::iterator HashIter;
	struct MIDownloadListIterator : public Iter {
		template<typename T>
		MIDownloadListIterator(T t) : Iter(t) {}
	};
}

DownloadList::Iter::Iter(Impl impl) : m_impl(impl) {}
Download& DownloadList::Iter::operator*() { return *(*(*m_impl)); }
Download& DownloadList::Iter::operator->() { return *(*(*m_impl)); }
void DownloadList::Iter::operator++() { ++*m_impl; }
void DownloadList::Iter::operator--() { --*m_impl; }
bool DownloadList::Iter::operator==(const DownloadList::Iter& x) const {
	return *m_impl == *x.m_impl;
}
bool DownloadList::Iter::operator!=(const DownloadList::Iter& x) const {
	return *m_impl != *x.m_impl;
}

DownloadList *s_downloadList = 0;
DownloadList::DownloadList() : m_list(new Detail::MIDownloadList) {}
DownloadList::~DownloadList() { s_downloadList = 0; }
DownloadList::Iter DownloadList::begin() {
	Iter::Impl tmp(new Detail::MIDownloadListIterator(m_list->begin()));
	return Iter(tmp);
}
DownloadList::Iter DownloadList::end() {
	Iter::Impl tmp(new Detail::MIDownloadListIterator(m_list->end()));
	return Iter(tmp);
}

DownloadList& DownloadList::instance() {
	if (!s_downloadList) {
		s_downloadList = new DownloadList;
	}
	return *s_downloadList;
}

void DownloadList::init() {
	FilesList::SFIter it = FilesList::instance().begin();
	for (; it != FilesList::instance().end(); ++it) {
		if ((*it)->isPartial() && !(*it)->getPartData()->isStopped()) {
			tryAddFile((*it)->getPartData());
		}
	}
	PartData::getEventTable().addHandler(0, this, &DownloadList::onPDEvent);
	SharedFile::getEventTable().addHandler(
		0, this, &DownloadList::onSFEvent
	);

	// we can import ed2k downloads
	FilesList::instance().import.connect(
		boost::bind(&DownloadList::import, this, _1)
	);

	logDebug(
		boost::format(
			"DownloadList initialized, %d downloads are known."
		) % m_list->size()
	);
}

void DownloadList::exit() {
	for (Detail::Iter it = m_list->begin(); it != m_list->end(); ++it) {
		delete *it;
	}
}

void DownloadList::tryAddFile(PartData *pd) {
	using namespace Detail;
	typedef const Hash<ED2KHash>& HashType;

	if (!pd->getMetaData()) {
		return;
	}
	MIDownloadList::nth_index<ID_PD>::type& list = m_list->get<ID_PD>();
	Detail::Iter it = list.find(pd);
	if (it != list.end() && pd->isRunning()) {
		onAdded(**it);
		return;
	}
	MetaData *md = pd->getMetaData();
	if (md->getSize() > std::numeric_limits<uint32_t>::max()) {
		return;
	}
	for (uint32_t j = 0; j < md->getHashSetCount(); ++j) {
		HashSetBase *hs = md->getHashSet(j);
		if (hs->getFileHashTypeId() != CGComm::OP_HT_ED2K) {
			continue;
		}
		HashType h(dynamic_cast<HashType>(hs->getFileHash()));
		Download *d = new Download(pd, h);
		m_list->insert(d);

		if (pd->isRunning()) {
			onAdded(*d);
		}
	}
}

void DownloadList::onPDEvent(PartData *pd, int evt) {
	if (evt == PD_STOPPED || evt == PD_DESTROY) {
		Detail::Iter it = m_list->find(pd);
		if (it != m_list->end()) {
			onRemoved(*(*it));
			(*it)->destroy();
			delete *it;
			m_list->erase(it);
		}
	} else if (evt == PD_RESUMED) {
		tryAddFile(pd);
	}
}

void DownloadList::onSFEvent(SharedFile *sf, int evt) {
	if (evt == SF_ADDED && sf->isPartial()) {
		if (m_list->find(sf->getPartData()) == m_list->end()) {
			tryAddFile(sf->getPartData());
		}
	}
}

Download* DownloadList::find(const Hash<ED2KHash> &hash) const {
	Detail::HashIter it = m_list->get<Detail::ID_Hash>().find(hash);
	return it == m_list->get<Detail::ID_Hash>().end() ? 0 : *it;
}

bool DownloadList::valid(Download *ptr) const {
	Detail::DIter it = m_list->get<Detail::ID_Download>().find(ptr);
	return it != m_list->get<Detail::ID_Download>().end();
}

void DownloadList::import(boost::filesystem::path p) {
	using namespace boost::filesystem;

	IOThread::Pauser pauser(IOThread::instance());

	for (directory_iterator it(p); it != directory_iterator(); ++it) {
		if (!boost::algorithm::iends_with((*it).string(), ".met")) {
			continue;
		}
		try {
			doImport(*it);
		} catch (std::exception &e) {
			logError(
				boost::format("Importing %s: %s") %
				(*it).native_file_string() % e.what()
			);
		}
	}
}

void DownloadList::doImport(const boost::filesystem::path &p) {
	using Utils::getVal;
	using Utils::getFileSize;
	using namespace boost::filesystem;

	std::string pName(p.native_file_string());
	std::ifstream ifs(pName.c_str(), std::ios::binary);

	uint8_t ver = getVal<uint8_t>(ifs);
	if (ver != 0xe0) {
		logWarning(
			boost::format(
				"Importing %s: wrong PartFile version %s"
			) % pName % Utils::hexDump(ver)
		);
		return;
	}

	(void)getVal<uint32_t>(ifs);

	Hash<ED2KHash> fHash(getVal<std::string>(ifs, 16));
	uint16_t partCnt = getVal<uint16_t>(ifs);
	std::vector<Hash<MD4Hash> > partHashes;
	while (ifs && partCnt--) {
		partHashes.push_back(
			Hash<MD4Hash>(getVal<std::string>(ifs, 16))
		);
	}

	uint32_t fSize = 0;
	std::string fName;

	uint32_t tagCount = getVal<uint32_t>(ifs);
	while (ifs && tagCount--) {
		Tag t(ifs);
		switch (t.getOpcode()) {
			case 0x01: // filename
				fName = t.getStr();
				break;
			case 0x02: // filesize
				fSize = t.getInt();
				break;
			default:
				break;
		}
	}

	MetaData *md = new MetaData(fSize);
	ED2KHashSet *hs = new ED2KHashSet(fHash, partHashes);

	md->addHashSet(hs);
	md->addFileName(fName);

	boost::algorithm::replace_last(pName, ".met", "");
	PartData *pd = new PartData(path(pName, native), md);
	SharedFile *sf = new SharedFile(pd, md);
	FilesList::instance().push(sf);

	logMsg(
		boost::format(
			"Successfully imported: " COL_GREEN "%s" COL_NONE
		) % sf->getName()
	);

	FilesList::instance().addTempDir(p.branch_path().string(), false);
}

} // end namespace Donkey
