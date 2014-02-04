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

/**
 * \file bittorrent.cpp        Implementation of BitTorrent class
 */

#include <hncore/bt/bittorrent.h>
#include <hncore/hydranode.h>
#include <hncore/fileslist.h>
#include <hncore/sharedfile.h>
#include <hncore/partdata.h>
#include <hncore/metadata.h>
#include <hncore/metadb.h>
#include <hncore/search.h>
#include <hnbase/prefs.h>
#include <hnbase/sha1transform.h>
#include <hnbase/timed_callback.h>
#include <hncore/bt/torrent.h>
#include <hncore/bt/files.h>
#include <hncore/bt/torrentinfo.h>
#include <hncore/bt/client.h>
#include <hncore/bt/bittorrent.h>
#include <boost/spirit.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/lambda/construct.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/algorithm/string/predicate.hpp>

namespace Bt {

IMPLEMENT_MODULE(BitTorrent);
const std::string TRACE("bt.bittorrent");

/**
 * Helper functor for connecting to PartData::getLinks signal, returns location
 * of the corresponding .torrent file.
 */
struct LinkReturner {
	LinkReturner(const boost::filesystem::path &p) : m_path(p) {}
	void operator()(PartData *, std::vector<std::string> &links) const {
		links.push_back(m_path.native_file_string());
	}
private:
	boost::filesystem::path m_path;
};

bool BitTorrent::onInit() {
	boost::format peerId("-HN0%d%d%d-");
	peerId % APPVER_MAJOR % APPVER_MINOR % APPVER_PATCH;
	m_id.append(peerId.str());
	std::ostringstream tmp;
	while (tmp.str().size() < 12) {
		tmp << Utils::getRandom();
	}
	m_id.append(tmp.str().substr(0, 12));
	CHECK(m_id.size() == 20);
	std::ostringstream tmpKey;
	while (tmpKey.str().size() < 20) {
		tmpKey << Utils::getRandom();
	}
	m_key = tmp.str().substr(0, 20);

	m_port = Prefs::instance().read<uint16_t>("/bt/TCPPort", 6882);
	Prefs::instance().write<uint16_t>("/bt/TCPPort", m_port);

	m_listener.reset(new TcpListener(this, &BitTorrent::onIncoming));
	uint16_t minPort = m_port - 10;
	do {
		try {
			m_listener->listen(0, m_port);
			break;
		} catch (std::exception &e) {
			logError(boost::format(
				"Unable to start BT TCP listener on port %d: %s"
			) % m_port % e.what());
			--m_port;
		}
	} while (m_port > minPort);
	if (!m_listener->isListening()) {
		logError("Giving up - unable to start TCP listener.");
	}

	bool autoStart = Prefs::instance().read<bool>(
		"/bt/AutoStartTorrents", true
	);
	Prefs::instance().write<bool>("/bt/AutoStartTorrents", autoStart);

	logMsg(
		boost::format(
			COL_BGREEN "Bittorrent TCP listener" COL_NONE
			" started on port " COL_BCYAN "%d" COL_NONE
		) % m_port
	);
	Search::addLinkHandler(boost::bind(&BitTorrent::downloadLink, this,_1));
	Search::addFileHandler(boost::bind(&BitTorrent::downloadFile, this,_1));
	SharedFile::getEventTable().addHandler(0, this, &BitTorrent::onSFEvent);

	Log::instance().addTraceMask("bt.client");
	Log::instance().addTraceMask("bt.torrent");
	Log::instance().addTraceMask("bt.bittorrent");
	Log::instance().addTraceMask("bt.files");
//	Log::instance().enableTraceMask("bt.client");

	m_cacheDir = Hydranode::instance().getConfigDir() / "bt";
	if (!boost::filesystem::exists(m_cacheDir)) try {
		boost::filesystem::create_directory(m_cacheDir);
	} catch (std::exception &e) {
		logError(
			boost::format("Unable to create %s: %s")
			% m_cacheDir.native_directory_string() % e.what()
		);
	}

	Torrent::getEventTable().addAllHandler(
		this, &BitTorrent::onTorrentEvent
	);

	try {
		initKnownTorrents(m_cacheDir);
		initTorrentDb(m_cacheDir);
	} catch (std::exception &e) {
		logError(
			boost::format("Failed to init TorrentDb: %s") % e.what()
		);
	}

	initFiles();
	adjustLimits();
	Prefs::instance().valueChanged.connect(
		boost::bind(&BitTorrent::configChanged, this, _1, _2)
	);

	return true;
}

int BitTorrent::onExit() {
	using namespace boost::lambda;
	m_listener.reset();
	for_each(
		m_newClients.begin(), m_newClients.end(),
		bind(delete_ptr(), __1)
	);
	std::map<Hash<SHA1Hash>, Torrent*>::iterator iter(m_torrents.begin());
	while (iter != m_torrents.end()) {
		delete (*iter++).second;
	}
	saveKnownTorrents(m_cacheDir);
	return 0;
}

void BitTorrent::initFiles() {
	using namespace boost::filesystem;
	typedef std::map<
		Hash<SHA1Hash>,
		std::pair<
			std::map<uint64_t, SharedFile*>,
			std::map<uint64_t, PartData*>
		>
	> ValidFiles;
	typedef ValidFiles::const_iterator FIter;
	ValidFiles files;
	uint32_t found = 0;

	FilesList::SFIter it = FilesList::instance().begin();
	while (it != FilesList::instance().end()) {
		SharedFile *sf = *it++;
		MetaData *md = sf->getMetaData();
		if (!md) {
			continue;
		}
		MetaData::CustomIter it2 = md->customBegin();
		while (it2 != md->customEnd()) {
			using namespace boost::spirit;
			int_parser<long long> lint_p; // 64bit integers parser
			std::string data = *it2++;
			std::string hash;
			uint64_t offset = 0;
			parse_info<> nfo = parse(
				data.c_str(), data.c_str() + data.size(),
				"btorrent:" >> *alnum_p[push_back_a(hash)]
				>> ':' >> lint_p[assign_a(offset)]
			);
			if (!nfo.full) {
				continue;
			}
			// these maps require encoded hashes
			hash = Utils::encode(hash);

			files[hash].first[offset] = sf;
			if (sf->getPartData()) {
				files[hash].second[offset] = sf->getPartData();
			}

			++found;
		}
	}

	std::map<Hash<SHA1Hash>, boost::filesystem::path> toCleanup(m_torrentDb);
	for (FIter i = files.begin(); i != files.end(); ++i) {
		DBIter tit = m_torrentDb.find((*i).first);
		if (tit == m_torrentDb.end()) {
			continue;
		}
		CHECK(boost::filesystem::exists((*tit).second));
		std::ifstream ifs(
			(*tit).second.native_file_string().c_str(),
			std::ios::binary
		);
		if (!ifs) {
			logError(
				boost::format(
					"Failed to open file %s for reading "
					"(check permissions?)"
				) % (*tit).second.native_file_string()
			);
			continue;
		}
		TorrentInfo ti(ifs);
		SharedFile *sf = 0;
		if (ti.getFiles().size() > 1) {
			PartialTorrent *pt = 0;
			MetaData *md = new MetaData(ti.getSize());
			md->addFileName(ti.getName());
			HashSetBase *hs = new HashSet<SHA1Hash>(ti.getHashes());
			md->addHashSet(hs);
			if ((*i).second.second.size()) {
				pt = new PartialTorrent(
					(*i).second.second,
					getCacheDir()
					/ path((*tit).second.leaf(), native),
					ti.getSize()
				);
				pt->initCache(ti);
				pt->setMetaData(md);
			}
			sf = new TorrentFile(
				(*i).second.first, ti, pt
			);
			sf->setMetaData(md);
			FilesList::instance().push(sf);
		} else {
			CHECK((*i).second.first.size());
			sf = (*i).second.first.begin()->second;
			PartData *pd = sf->getPartData();
			if (pd) {
				pd->addHashSet(
					new HashSet<SHA1Hash>(ti.getHashes())
				);
			}
		}
		if (sf->getPartData()) {
			sf->getPartData()->getLinks.connect(
				LinkReturner((*tit).second)
			);
		}

		assert(m_torrents.find(ti.getInfoHash()) == m_torrents.end());
		Torrent *tor = new Torrent(sf, ti);
		m_torrents[ti.getInfoHash()] = tor;
		logMsg(
			boost::format("BitTorrent: Resumed torrent %d")
			% ti.getName()
		);
		toCleanup.erase(ti.getInfoHash());
	}

	if (toCleanup.size()) {
		logMsg("Performing cache folder cleanup...");
	}
	for (DBIter it = toCleanup.begin(); it != toCleanup.end(); ++it) {
		std::string tmp((*it).second.leaf());
		boost::filesystem::directory_iterator it2(
			(*it).second.branch_path()
		);
		boost::filesystem::directory_iterator end;
		while (it2 != end) {
			if (boost::algorithm::starts_with((*it2).leaf(), tmp)) {
				logDebug(
					"Removing "
					+ (*it2).native_file_string()
				);
				boost::filesystem::remove(*it2);
			}
			++it2;
		}
	}

	logDebug(
		boost::format("Found %d files for %d torrents.")
		% found % files.size()
	);
}

void BitTorrent::onIncoming(TcpListener *server, SocketEvent evt) {
	if (evt != SOCK_ACCEPT) {
		return;
	}
	TcpSocket *sock = server->accept();
	m_curClient = new Client(sock);
	m_curClient->handshakeReceived.connect(
		boost::bind(&BitTorrent::onHandshakeReceived, this, _1)
	);
	m_curClient->connectionLost.connect(
		boost::bind(&BitTorrent::onConnectionLost, this, _1)
	);
	m_newClients.insert(m_curClient);
	m_curClient = 0;
}

void BitTorrent::onHandshakeReceived(Client *c) {
	using namespace boost::lambda;

	if (m_newClients.find(c) == m_newClients.end() && c != m_curClient) {
		logTrace(TRACE,
			boost::format(
				"[%s] Client received handshake, but not"
				"found in newClients map - ignoring."
			) % c->getAddr()
		);
		return;
	}
	Iter i = m_torrents.find(c->getInfoHash());
	if (i == m_torrents.end()) {
		logTrace(TRACE,
			boost::format(
				"[%s] Client sent info_hash %s, which we don't"
				"know about - deleting."
			) % c->getAddr() % c->getInfoHash().decode()
		);
		Utils::timedCallback(bind(delete_ptr(), c), 1);
		m_newClients.erase(c);
	} else {
		logTrace(TRACE,
			boost::format(
				"[%s] Attaching client to torrent %s."
			) % c->getAddr() % (*i).second->getName()
		);
		m_newClients.erase(c);
		(*i).second->addClient(c);
	}
}

void BitTorrent::onConnectionLost(Client *c) {
	if (m_newClients.find(c) == m_newClients.end() && c != m_curClient) {
		return;
	}
	m_newClients.erase(c);
	delete c;
}

void BitTorrent::createTorrent(
	const boost::filesystem::path &file, boost::filesystem::path dest
) {
	CHECK_THROW(boost::filesystem::exists(file));
	CHECK_THROW(!boost::filesystem::is_directory(file));

	std::ifstream ifs(file.native_file_string().c_str(), std::ios::binary);
	createTorrent(ifs, dest);
}

void BitTorrent::createTorrent(std::istream &ifs, boost::filesystem::path dest){
	using namespace boost::filesystem;

	std::ostringstream ofs;
	char buf[1024];
	while (ifs) {
		ifs.read(buf, 1024);
		ofs.write(buf, ifs.gcount());
	}
	std::istringstream ifs3(ofs.str());
	TorrentInfo ti(ifs3);

	if (!ti.getSize()) { // most likely parse failed, return quietly
		return;
	}

	if (m_torrents.find(ti.getInfoHash()) != m_torrents.end()) {
		logMsg(
			boost::format("You are already downloading %s")
			% ti.getName()
		);
		return;
	}

	ti.print();

	Hash<SHA1Hash> tHash = ti.getInfoHash();

	boost::format customData("btorrent:%s:%d");

	logMsg("Creating files...");

	// write the torrent info to cache folder before doing anything
	// else, since this is needed for us to later reload the torrent
	boost::filesystem::path cacheLoc(
		getCacheDir() / path(ti.getName() + ".torrent", native)
	);
	std::ofstream ofs2(
		cacheLoc.native_file_string().c_str(), std::ios::binary
	);
	CHECK_THROW(ofs2);
	std::istringstream ifs2(ofs.str());
	while (ifs2) {
		ifs2.read(buf, 1024);
		ofs2.write(buf, ifs2.gcount());
	}
	ofs2.close();

	// ok, now that we have the info, create the neccesery objects
	// first create the normal downloads, but add infohash key to them,
	// so that we can find them later on
	std::vector<TorrentInfo::TorrentFile> files = ti.getFiles();
	if (files.size() > 1) {
		if (dest.empty()) {
			dest = path(
				Prefs::instance().read<std::string>(
					"/Incoming", ""
				), no_check
			);
		}
		dest /= path(ti.getName(), native);
	}
	uint64_t pos = 0;
	for (uint32_t i = 0; i < files.size(); ++i) {
		if (!files[i].getSize()) { // file of size zero
			std::string incDir;
			incDir = Prefs::instance().read<std::string>(
				"/Incoming", ""
			);
			CHECK(incDir.size());
			path p(incDir, native);
			p /= files[i].getName();
			std::ofstream ofs(p.native_file_string().c_str());
			ofs.flush();
			continue;
		}
		MetaData *md = new MetaData(files[i].getSize());
		md->addFileName(files[i].getName());
		md->addCustomData((customData % tHash.decode() % pos).str());
		if (files[i].getSha1Hash()) {
			md->addHashSet(
				new HashSet<SHA1Hash>(files[i].getSha1Hash())
			);
		}
		if (files[i].getMd4Hash()) {
			md->addHashSet(
				new HashSet<MD4Hash>(files[i].getMd4Hash())
			);
		}
		if (files[i].getMd5Hash()) {
			md->addHashSet(
				new HashSet<MD5Hash>(files[i].getMd5Hash())
			);
		}
		if (files[i].getEd2kHash()) {
			md->addHashSet(new ED2KHashSet(files[i].getEd2kHash()));
		}
		MetaDb::instance().push(md);
		FilesList::instance().createDownload(
			files[i].getName(), md, dest
		);
		pos += files[i].getSize();

	}

	// locate our newly-created download objects
	std::map<uint64_t, SharedFile*> sharedFiles;
	std::map<uint64_t, PartData*> tempFiles;
	FilesList::SFIter it = FilesList::instance().begin();
	while (it != FilesList::instance().end()) {
		SharedFile *sf = *it++;
		if (!sf->getMetaData()) {
			continue;
		}
		MetaData *md = sf->getMetaData();
		MetaData::CustomIter it2 = md->customBegin();
		while (it2 != md->customEnd()) {
			using namespace boost::spirit;
			int_parser<long long> lint_p; // 64bit int parser

			std::string data = *it2++;
			std::string hash;
			uint64_t offset = 0;
			parse_info<> nfo = parse(
				data.c_str(), data.c_str() + data.size(),
				"btorrent:" >> *alnum_p[push_back_a(hash)]
				>> ':' >> lint_p[assign_a(offset)]
			);
			if (nfo.full && hash == tHash.decode()) {
				sharedFiles[offset] = sf;
				if (sf->getPartData()) {
					tempFiles[offset] = sf->getPartData();
				}
				break;
			}
		}
	}

	SharedFile *sf = 0;
	HashSetBase *hs = new HashSet<SHA1Hash>(ti.getHashes());
	if (files.size() > 1) {
		MetaData *md = new MetaData(ti.getSize());
		md->addFileName(ti.getName());
		md->addHashSet(hs);
		PartialTorrent *pt = new PartialTorrent(
			tempFiles, cacheLoc, ti.getSize()
		);
		pt->initCache(ti);
		pt->setMetaData(md);
		sf = new TorrentFile(sharedFiles, ti, pt);
		sf->setMetaData(md);
		FilesList::instance().push(sf);
		m_known[cacheLoc.leaf()] = dest;
	} else {
		CHECK(tempFiles.size() == 1);
		CHECK(sharedFiles.size() == 1);
		tempFiles.begin()->second->addHashSet(hs);
		sf = sharedFiles.begin()->second;
	}
	CHECK(sf->getPartData());
	sf->getPartData()->getLinks.connect(LinkReturner(cacheLoc));

	Torrent *tor = new Torrent(sf, ti);
	m_torrents[ti.getInfoHash()] = tor;
	saveKnownTorrents(m_cacheDir);
}

bool BitTorrent::downloadLink(const std::string &link) try {
	if (!boost::algorithm::iends_with(link, ".torrent")) {
		return false;
	}
	boost::filesystem::path p(link, boost::filesystem::native);
	if (!boost::filesystem::exists(p)) {
		return false;
	}
	createTorrent(p, boost::filesystem::path());
	return true;
} catch (std::exception &e) { // path constructor and createTorrent may throw
	logError(boost::format("Bittorrent: %s") % e.what());
	return false;
} MSVC_ONLY(;)

bool BitTorrent::downloadFile(const std::string &contents) try {
	logDebug(
		boost::format(
			"Starting torrent download "
			"from file contents (%d bytes)."
		) % contents.size()
	);
	std::istringstream tmp(contents);
	createTorrent(tmp, boost::filesystem::path());
	return true;
} catch (std::exception &e) { // createTorrent may throw
	logError(boost::format("Bittorrent: %s") % e.what());
	return false;
} MSVC_ONLY(;)

void BitTorrent::initTorrentDb(const boost::filesystem::path &path) {
	CHECK_THROW(boost::filesystem::exists(path));
	CHECK_THROW(boost::filesystem::is_directory(path));
	using namespace boost::algorithm;

	logMsg(
		boost::format("Scanning torrent directory at %s...")
		% path.native_directory_string()
	);

	boost::filesystem::directory_iterator it(path);
	boost::filesystem::directory_iterator end;
	for (; it != end; ++it) {
		if (!iends_with((*it).native_file_string(), ".torrent")) {
			continue;
		}
		std::ifstream ifs(
			(*it).native_file_string().c_str(), std::ios::binary
		);
		TorrentInfo nfo(ifs);
		DBIter i = m_torrentDb.find(nfo.getInfoHash());
		if (i == m_torrentDb.end()) {
			m_torrentDb[nfo.getInfoHash()] = *it;
		}
	}
	logMsg(boost::format("Found %d torrent files.") % m_torrentDb.size());
}

void BitTorrent::initKnownTorrents(const boost::filesystem::path &confDir) {
	CHECK_THROW(boost::filesystem::exists(confDir));
	CHECK_THROW(boost::filesystem::is_directory(confDir));
	using namespace boost::filesystem;

	std::ifstream ifs((confDir / "known.dat").native_file_string().c_str());
	std::string tName, dName;
	while (ifs) {
		dName.clear(), tName.clear();
		while (!tName.size() && ifs) {
			getline(ifs, tName);
		}
		while (!dName.size() && ifs) {
			getline(ifs, dName);
		}
		if (!dName.size() || !tName.size()) {
			break;
		}
		path tNamePath = confDir / tName;
		if (!exists(tNamePath)) {
			logMsg(
				"Torrent file " + tNamePath.native_file_string()
				+ " does not exist."
			);
			continue;
		}
		
		// Only scan the incoming directory if it exists; but add to
		// the known map always, since we might not have created the
		// dir yet if we just started downloading this torrent.
		if (exists(path(dName, native))) {
			FilesList::instance().addSharedDir(dName, true);
		}
		m_known[tName] = path(dName, native);
	}
}

void BitTorrent::saveKnownTorrents(const boost::filesystem::path &confDir) {
	CHECK_THROW(boost::filesystem::exists(confDir));
	CHECK_THROW(boost::filesystem::is_directory(confDir));
	using namespace boost::filesystem;

	std::ofstream ofs((confDir / "known.dat").native_file_string().c_str());
	std::map<std::string, path>::iterator it(m_known.begin());
	while (it != m_known.end()) {
		ofs << (*it).first << std::endl;
		ofs << (*it).second.native_directory_string() << std::endl;
		++it;
	}
}

void BitTorrent::openUploadSlot() {
	std::map<Hash<SHA1Hash>, Torrent*>::iterator it = m_torrents.begin();
	while (it != m_torrents.end()) {
		if ((*it++).second->tryUnchoke()) {
			break;
		}
	}
}

void BitTorrent::onSFEvent(SharedFile *sf, int evt) {
	bool autoStart = Prefs::instance().read<bool>(
		"/bt/AutoStartTorrents", false
	);
	if (evt == SF_DL_COMPLETE && autoStart) {
		if (boost::algorithm::iends_with(sf->getName(), ".torrent")) {
			downloadLink(sf->getPath().native_file_string());
		}
	}
}

void BitTorrent::onTorrentEvent(Torrent *tor, int evt) {
	if (evt == EVT_DESTROY) {
		std::map<Hash<SHA1Hash>, Torrent*>::iterator it;
		it = m_torrents.find(tor->getInfoHash());
		CHECK_RET(it != m_torrents.end());
		m_torrents.erase(it);
		delete tor;
	}
}

void BitTorrent::configChanged(
	const std::string &key, const std::string &value
) {
	if (key == "UpSpeedLimit" || key == "DownSpeedLimit") {
		adjustLimits();
	} else if (key == "bt/TCPPort") try {
		uint16_t newPort = boost::lexical_cast<uint16_t>(value);
		if (newPort != m_listener->getAddr().getPort()) {
			std::auto_ptr<TcpListener> newListener(new TcpListener);
			newListener->listen(IPV4Address(0, newPort));
			newListener->setHandler(this, &BitTorrent::onIncoming);
			m_listener.reset(newListener.get());
			m_port = newPort;
			newListener.release();
			logMsg(
				boost::format(
					"Bittorrent: TCP listener "
					"restarted on port %d"
				) % newPort
			);
		}
	} catch (std::exception &e) {
		logError(
			boost::format(
				"Bittorrent: Failed to restart TCP "
				"listener on port %d: %s"
			) % value % e.what()
		);
		Prefs::instance().write("/bt/TCPPort", m_port);
	}
}

void BitTorrent::adjustLimits() {
	boost::format fmt(
		"Lowering download speed limit to %s/s due "
		"to BitTorrent network netiquette."
	);
	uint32_t uLimit = Prefs::instance().read("/UpSpeedLimit", 0);
	uint32_t dLimit = Prefs::instance().read("/DownSpeedLimit", 0);
	if (!dLimit) {
		dLimit = std::numeric_limits<uint32_t>::max();
	} 
	if (!uLimit) {
		uLimit = std::numeric_limits<uint32_t>::max();
	}

	if (uLimit < 5 * 1024 && dLimit > uLimit * 2) {
		logMsg(fmt % Utils::bytesToString(uLimit * 2));
		Prefs::instance().write<uint32_t>("/DownSpeedLimit", uLimit*2);
	}
}

} // end Bt namespace
