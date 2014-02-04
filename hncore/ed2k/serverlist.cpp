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
 * \file serverlist.cpp Implementation of ServerList and Server classes
 */

#include <hncore/ed2k/serverlist.h>        // Class interface
#include <hncore/ed2k/server.h>            // Server class
#include <hncore/ed2k/ed2k.h>              // ED2K class>
#include <hncore/ed2k/clientlist.h>        // needed during callback requests
#include <hncore/ed2k/parser.h>            // ed2kparser
#include <hncore/ed2k/downloadlist.h>      // Download / DownloadList
#include <hnbase/utils.h>                  // Utils::hexDump
#include <hnbase/log.h>                    // log functions
#include <hnbase/ssocket.h>                // Sockets ...
#include <hnbase/timed_callback.h>         // Timed callbacks
#include <hnbase/prefs.h>                  // Settings
#include <hncore/fileslist.h>              // FilesList et al
#include <hncore/sharedfile.h>             // SharedFile
#include <hncore/partdata.h>               // PartData
#include <hncore/metadb.h>                 // for debug selfcheck
#include <boost/lambda/bind.hpp>           // bind
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <boost/multi_index/ordered_index.hpp>

namespace Donkey {

/**
 * Constants used internally by ServerList
 */
enum ED2K_ServerCommConstants {
	SOURCEREASKTIME = 20*60*1000, //!< UDP queries time, 20 minutes
	SERVERPINGTIME  = 20*60*1000, //!< Server ping time, 20 minutes
	SERVERTIMEOUT   = 20000       //!< Server connection timeout
};

const std::string TRACE = "ed2k.serverlist";
const std::string TRACE_GLOBSRC = "ed2k.globsrc";

namespace Detail {

//! ServerName extractor functor
struct ServerNameExtractor {
	typedef std::string result_type;
	result_type operator()(const Server *const s) const {
		return s->getName();
	}
};

//! ServerAddr extractor functor
struct ServerAddrExtractor {
	typedef IPV4Address result_type;
	result_type operator()(const Server *const s) const {
		return s->getAddr();
	}
};

//! Server UDP Addr extractor functor
struct ServerUAddrExtractor {
	typedef IPV4Address result_type;
	result_type operator()(const Server *const s) const {
		return IPV4Address(s->getAddr().getIp(), s->getUdpPort());
	}
};

//! last udp query time extractor functor
struct QueryTimeExtractor {
	typedef uint64_t result_type;
	result_type operator()(const Server *const s) const {
		return s->getLastUdpQuery();
	}
};

//! Server list structure, sorted by IP address and name
struct ServerListIndices : boost::multi_index::indexed_by<
	boost::multi_index::ordered_unique<
		boost::multi_index::identity<Server*>
	>,
	boost::multi_index::ordered_non_unique<ServerAddrExtractor>,
	boost::multi_index::ordered_non_unique<ServerNameExtractor>,
	boost::multi_index::ordered_non_unique<QueryTimeExtractor>,
	boost::multi_index::ordered_non_unique<ServerUAddrExtractor>
> {};
struct MIServerList : boost::multi_index_container<
	Server*, ServerListIndices
> {};
typedef MIServerList::nth_index<0>::type::iterator ServIter;
typedef MIServerList::nth_index<1>::type::iterator AddrIter;
typedef MIServerList::nth_index<2>::type::iterator NameIter;
struct QTIter : MIServerList::nth_index<3>::type::iterator {
	template<typename T>
	QTIter(const T &t) : MIServerList::nth_index<3>::type::iterator(t) {}
};
typedef MIServerList::nth_index<4>::type::iterator UAddrIter;

} // namespace Detail

using namespace Detail;

// ServerList class
// ----------------
IMPLEMENT_EVENT_TABLE(ServerList, ServerList*, ServerList::ServerListEvent);
DECLARE_PACKET_HANDLER(ServerList, ServerMessage);
DECLARE_PACKET_HANDLER(ServerList, IdChange     );
DECLARE_PACKET_HANDLER(ServerList, ServerStatus );
DECLARE_PACKET_HANDLER(ServerList, ServerList   );
DECLARE_PACKET_HANDLER(ServerList, ServerIdent  );
DECLARE_PACKET_HANDLER(ServerList, SearchResult );
DECLARE_PACKET_HANDLER(ServerList, CallbackReq  );
DECLARE_PACKET_HANDLER(ServerList, FoundSources );

ServerList::ServerList() : Object(&ED2K::instance(), "serverlist")
, m_serverSocket(), m_currentServer(),
m_parser(new ED2KParser<ServerList>(this)), m_status(),
m_list(new MIServerList) {
	// Annouce we are capable of performing searches too
	Search::addQueryHandler(
		boost::bind(&ServerList::performSearch, this, _1)
	);

	// Set up handlers for ALL SharedFile's events
	SharedFile::getEventTable().addAllHandler(
		this, &ServerList::onSharedFileEvent
	);

	// used for delayed callback events
	getEventTable().addHandler(this, this, &ServerList::onServerListEvent);
	Server::getEventTable().addHandler(0, this, &ServerList::onServerEvent);

	// register our trace masks
	Log::instance().addTraceMask(TRACE);
	Log::instance().addTraceMask(TRACE_GLOBSRC);
}

void ServerList::init() {
	bool fServers = Prefs::instance().read<bool>("/ed2k/FindServers", true);
	Prefs::instance().write("/ed2k/FindServers", fServers);
	if (fServers) {
		m_foundServerConn = Detail::foundServer.connect(
			boost::bind(&ServerList::addServer, this, _1)
		);
	}
	DownloadList::instance().onAdded.connect(
		boost::bind(&ServerList::reqSources, this, _1)
	);
	Prefs::instance().valueChanged.connect(
		boost::bind(&ServerList::configChanged, this, _1, _2)
	);

	connect();
	queryNextServer();
}

void ServerList::exit() {
	delete m_serverSocket;
	m_serverSocket = 0;
	for (ServIter i = m_list->begin(); i != m_list->end(); ++i) {
		delete *i;
	}
}

ServerList::~ServerList() {}

void ServerList::load(const std::string &file) {
	std::ifstream ifs(file.c_str(), std::ios::binary);
	if (!ifs) {
		// Can't open ...
		if (m_list->size() == 0) {
			// No servers in list.. add hardcoded ones.
			addDefaultServers();
			return;
		}
	}

	uint8_t ver = Utils::getVal<uint8_t>(ifs);
	if (ver != ST_METVERSION && ver != ST_METHEADER) {
		logError(
			boost::format(
				"Corruption found in server list "
				"(invalid versiontag %s)"
			) % Utils::hexDump(ver)
		);
		return;
	}

	uint32_t count = Utils::getVal<uint32_t>(ifs);
	Utils::StopWatch t;
	while (count--) {
		try {
			Server *s = new Server(ifs);
			AddrIter it = m_list->get<1>().find(s->getAddr());
			if (it == m_list->get<1>().end()) {
				m_list->insert(s);
			} else {
				logTrace(TRACE,
					"Ignoring duplicate server.met entry"
				);
				delete s;
			}
		} catch (std::exception &err) {
			logWarning(
				boost::format(
					"Corruption found in server.met: %s"
				) % err.what()
			);
			break;
		}
	}

	if (m_list->size() == 0) {
		addDefaultServers();
	}

	logMsg(
		boost::format(
			"ServerList loaded, %d servers are known (%dms)"
		) % m_list->size() % t
	);
	// save every 17 minutes
	Utils::timedCallback(
		boost::bind(&ServerList::save, this, file), 60*1000*17
	);
}

void ServerList::save(const std::string &file) const {
	std::ofstream ofs(file.c_str(), std::ios::binary);
	if (!ofs) {
		logError(
			boost::format(
				"Unable to save server list (opening "
				"file %s failed)"
			) % file
		);
		return;
	}

	Utils::putVal<uint8_t>(ofs, ST_METVERSION);
	Utils::putVal<uint32_t>(ofs, m_list->size());
	Utils::StopWatch t;

	for (ServIter i = m_list->begin(); i != m_list->end(); ++i) {
		ofs << *(*i);
	}

	logMsg(
		boost::format("ServerList saved, %d servers written (%dms)")
		% m_list->size() % t
	);
	// save every 17 minutes
	Utils::timedCallback(
		boost::bind(&ServerList::save, this, file), 60*1000*17
	);
}

void ServerList::addDefaultServers() {
	std::vector<IPV4Address> defaults;
	defaults.push_back(IPV4Address("63.217.27.11",    4661));
	defaults.push_back(IPV4Address("66.172.60.139",   4661));
	defaults.push_back(IPV4Address("62.241.53.2",     4242));
	defaults.push_back(IPV4Address("64.34.165.203",   5306));
	defaults.push_back(IPV4Address("199.249.181.11",  5306));
	defaults.push_back(IPV4Address("204.11.19.22",    4661));
	defaults.push_back(IPV4Address("199.249.181.7",   4242));
	defaults.push_back(IPV4Address("80.239.200.110",  3000));
	defaults.push_back(IPV4Address("64.34.176.139",   5821));
	defaults.push_back(IPV4Address("193.138.230.251", 4242));

	for (size_t i = 0; i < defaults.size(); ++i) {
		IPV4Address ip = defaults[i];
		if (m_list->get<1>().find(ip) == m_list->get<1>().end()) {
			m_list->insert(new Server(ip));
		}
	}
}

void ServerList::connect() {
	if (!m_list->size()) {
		addDefaultServers();
	}
	std::vector<Server*> tmp;
	std::copy(m_list->begin(), m_list->end(), std::back_inserter(tmp));
	std::random_shuffle(tmp.begin(), tmp.end());

	if (!tmp.size()) {
		connect();
	} else if (tmp[0]->getFailedCount() > 2) {
		logTrace(
			TRACE, boost::format("Removing dead server %s (%s)")
			% tmp[0]->getAddr() % tmp[0]->getName()
		);
		m_list->erase(tmp[0]);
		connect();
	} else {
		connect(tmp[0]);
	}

}

void ServerList::connect(Server *s) {
	if (m_serverSocket) {
		m_serverSocket->disconnect();
		delete m_serverSocket;
	}
	m_status = ST_CONNECTING;
	m_currentServer = s;

	m_serverSocket = new ED2KClientSocket();
	m_serverSocket->setHandler(this, &ServerList::onServerSocketEvent);

	IPV4Address addr = m_currentServer->getAddr();
	m_serverSocket->connect(addr, 3000);
	m_lastConnAttempt = Utils::getTick();

	logMsg(boost::format("eDonkey2000: Connecting to %s") % addr);
	m_status = ST_CONNECTING;
	notify();
}

void ServerList::onServerSocketEvent(ED2KClientSocket *c, SocketEvent evt) {
	CHECK_RET(m_currentServer);
	CHECK_THROW(c == m_serverSocket);

	switch (evt) {
		case SOCK_CONNECTED:
			logMsg(
				"eDonkey2000: Server connection established, "
				"sending login request."
			);
			m_status = ST_LOGGINGIN;
			notify();
			sendLoginRequest();
			break;
		case SOCK_READ: {
			try {
				m_parser->parse(c->getData());
			} catch (std::exception &er) {
				logDebug(
					boost::format(
						"Error parsing Server "
						"stream: %s"
					) % er.what()
				);
				connect(); // Next server
				break;
			}

			break;
		}
		case SOCK_ERR:
		case SOCK_LOST:
		case SOCK_TIMEOUT:
		case SOCK_CONNFAILED: {
			boost::format fmt(
				"eDonkey2000: Server connection lost (%s)"
			);
			if (evt == SOCK_ERR) {
				logMsg(fmt % "socket error");
			} else if (evt == SOCK_LOST) {
				logMsg(fmt % "remote host closed connection");
			} else if (evt == SOCK_TIMEOUT) {
				logMsg(fmt % "socket timed out");
			} else if (evt == SOCK_CONNFAILED) {
				logMsg(fmt % "connection attempt failed");
			}
			m_currentServer->addFailedCount();
			delete m_serverSocket;
			m_serverSocket = 0;
			m_currentServer = 0;

			// try again after a while
			int tDiff = Utils::getTick() - m_lastConnAttempt;
			if (tDiff >= 3000) {
				connect();
			} else {
				getEventTable().postEvent(
					this, EVT_CONNECT, 3000 - tDiff
				);
				logMsg(
					boost::format(
						"eDonkey2000: Reconnecting "
						"in %s seconds."
					) % (tDiff / 1000.0)
				);
			}
			break;
		}
		case SOCK_WRITE:
			break; // ignored
		default:
			logDebug(
				boost::format(
					"Unknown socket event %p in ServerList."
				) % evt
			);
			break;
	}
}

void ServerList::sendLoginRequest() {
	CHECK_THROW(m_serverSocket);
	CHECK_THROW(m_serverSocket->isConnected());
	CHECK_THROW(m_currentServer);

	// not sure if this is the best place to reset it ...
	m_currentServer->setFailedCount(0);

	*m_serverSocket << ED2KPacket::LoginRequest();

	// Login timeout
	Server::getEventTable().postEvent(
		m_currentServer, EVT_LOGINTIMEOUT, SERVERTIMEOUT
	);
}

void ServerList::publishFiles(bool useZlib) {
	CHECK_THROW(m_serverSocket);
	CHECK_THROW(m_serverSocket->isConnected());

	ED2KPacket::OfferFiles packet(useZlib ? PR_ZLIB : PR_ED2K);

	uint32_t cnt = 0;         // Don't publish over 300 files
	FilesList::CSFIter i = FilesList::instance().begin();
	for (; i != FilesList::instance().end() && cnt <= 300; ++i, ++cnt) {
		SharedFile *sf = *i;

		// Only a partial solution, but avoids publishing files which
		// have less than 9500kb downloaded. To be 100% correct, we
		// should check here if the file has at least one complete chunk
		if (sf->isPartial()) {
			if (sf->getPartData()->getCompleted() < ED2K_PARTSIZE) {
				continue;
			}
		}

		MetaData *md = sf->getMetaData();
		if (md == 0) {
			continue; // No metadata - can't do anything
		}
		if (sf->getSize() > std::numeric_limits<uint32_t>::max()) {
			// It's larger than we can support on
			// ed2k (32-bit integer - 4gb) :(
			continue;
		}

		for (uint32_t j = 0; j < md->getHashSetCount(); ++j) {
			HashSetBase* hs = md->getHashSet(j);
			if (hs->getFileHashTypeId() == CGComm::OP_HT_ED2K) {
				logTrace(TRACE, 
					boost::format("Publishing file %s") 
					% sf->getName()
				);
				packet.push(makeED2KFile(sf, md, hs, useZlib));
			}
		}
	}

	if (cnt) { // only send if we have smth to offer
		*m_serverSocket << packet;
	}
}

void ServerList::performSearch(SearchPtr search) {
	logTrace(TRACE, "[ed2k] ServerList::performSearch");

	// local server search only if we'r currently connected
	if (m_serverSocket && m_status == ST_CONNECTED) {
		*m_serverSocket << ED2KPacket::Search(search);
		m_curSearch = search;
	}

	// global search
	std::vector<Server*> tmp;
	std::copy(m_list->begin(), m_list->end(), std::back_inserter(tmp));
	std::random_shuffle(tmp.begin(), tmp.end());
	boost::shared_ptr<std::list<Server*> > list(new std::list<Server*>);
	std::copy(tmp.begin(), tmp.end(), std::back_inserter(*list));
	Utils::timedCallback(
		boost::bind(&ServerList::doGlobSearch, this, search, list), 0
	);
	std::string keywords;
	for (uint32_t i = 0; i < search->getTermCount(); ++i) {
		keywords += search->getTerm(i);
		if (i + 1 < search->getTermCount()) {
			keywords += " ";
		}
	}
	logMsg(
		boost::format(
			"eDonkey2000: Search started with keywords '%s'."
		) % keywords
	);
}

// Implement packet handlers
void ServerList::onPacket(const ED2KPacket::ServerMessage &p) {
	std::string msg = p.getMsg();
	while (msg.size() && *--msg.end() == '\n') {
		msg.erase(--msg.end()); // erase trailing empty lines
	}

	if (msg.substr(0, 8) == "ERROR : ") {
		logError("[Ed2kServer] " + msg.substr(8));
		connect();
	} else if (msg.substr(0, 10) == "WARNING : ") {
		logWarning("[Ed2kServer] " + msg.substr(10));
	} else {
		logMsg("[Ed2kServer] " + msg);
	}
}

void ServerList::onPacket(const ED2KPacket::IdChange &p) {
	CHECK_THROW_MSG(m_currentServer, "Received IdChange, but from where?");

	logMsg(
		boost::format("eDonkey2000: New ID: %d (%s)") % p.getId()
		% (p.getId() > 0x00ffffff
			? COL_BGREEN "high" COL_NONE
			: COL_BYELLOW "low" COL_NONE
		)
	);

	ED2K::instance().setId(p.getId());
	notify();
	m_currentServer->setTcpFlags(p.getFlags());

	if (m_status != ST_CONNECTED) {
		m_status = ST_CONNECTED;
		notify();
		logMsg(
			"eDonkey2000: We are now connected "
			"to eDonkey2000 network."
		);
		logMsg(
			boost::format("eDonkey2000: Server: %s(%s)")
			% m_currentServer->getName()
			% m_currentServer->getAddr()
		);
		// Publish our shared files
		publishFiles(m_currentServer->getTcpFlags() & FL_ZLIB);
		m_lastSourceRequest = 0;
		reqSources();
		getEventTable().postEvent(
			this, EVT_PINGSERVER, SERVERPINGTIME
		);
		bool fServers = Prefs::instance().read<bool>(
			"/ed2k/FindServers", true
		);
		if (fServers) {
			*m_serverSocket << ED2KPacket::GetServerList();
		}
	}
}

void ServerList::onPacket(const ED2KPacket::ServerStatus &p) {
	boost::format fmt(
		"eDonkey2000: Received server status update: "
		"Users: " COL_BCYAN "%d"
		COL_NONE " Files: " COL_BGREEN "%d" COL_NONE
	);
	logMsg(fmt % p.getUsers() % p.getFiles());
	CHECK_THROW(m_currentServer);

	m_currentServer->setUsers(p.getUsers());
	m_currentServer->setFiles(p.getFiles());
}

void ServerList::onPacket(const ED2KPacket::ServerIdent &p) {
	logMsg("eDonkey2000: Received server ident:");
	logMsg(boost::format("eDonkey2000: Addr: %s") % p.getAddr().getStr());
	logMsg(boost::format("eDonkey2000: Name: %s") % p.getName());
	logMsg(boost::format("eDonkey2000: Desc: %s") % p.getDesc());

	CHECK_THROW(m_currentServer);
	ServIter it = m_list->find(m_currentServer);
	CHECK_THROW(it != m_list->end());

	m_list->modify(it, bind(&Server::setName, __1, p.getName()));
	m_list->modify(it, bind(&Server::setDesc, __1, p.getDesc()));
}

void ServerList::onPacket(const ED2KPacket::ServerList &p) {
	bool fServers = Prefs::instance().read<bool>("/ed2k/FindServers", true);
	if (!fServers) {
		return;
	}

	uint32_t added = 0;

	for (uint32_t i = 0; i < p.getCount(); ++i) {
		AddrIter it = m_list->get<1>().find(p.getServer(i));
		if (it == m_list->get<1>().end()) {
			Server *s = new Server(p.getServer(i));
			m_list->insert(s);
			++added;
		}
	}

	logTrace(TRACE,
		boost::format("Received %d new servers (and %d duplicates)")
		% added % (p.getCount() - added)
	);
}

void ServerList::onPacket(const ED2KPacket::SearchResult &p) {
	logMsg(
		boost::format("Received %d search results from server.")
		% p.getCount()
	);
	CHECK_THROW_MSG(m_curSearch, "Search results, but no search pending?");

	for (uint32_t i = 0; i < p.getCount(); ++i) {
		m_curSearch->addResult(p.getResult(i));
	}

	// notify results even if no results were added
	m_curSearch->notifyResults();
}

void ServerList::onPacket(const ED2KPacket::CallbackReq &p) {
	logTrace(TRACE,
		boost::format("Received callback request to %s") % p.getAddr()
	);
	ClientList::instance().addClient(p.getAddr());
}

void ServerList::onPacket(const ED2KPacket::FoundSources &p) {
	CHECK_THROW(m_currentServer);

	uint32_t connLimit = SchedBase::instance().getConnLimit();
	uint32_t connCnt = SchedBase::instance().getConnCount();
	connCnt += SchedBase::instance().getConnReqCount();
	bool doConn = false;
	uint32_t cnt = 0;

	for (uint32_t i = 0; i < p.getCount(); ++i) {
		doConn = connCnt + p.getLowCount() < connLimit;
		cnt += Detail::foundSource(
			p.getHash(), p.getSource(i),
			m_currentServer->getAddr(), doConn
		);
		connCnt += doConn;
	}

	logDebug(
		boost::format(
			"Received %d new sources from server "
			"(and %d duplicates)"
		) % cnt % (p.getCount() - cnt)
	);
}

void ServerList::reqCallback(uint32_t id) {
	if (ED2K::instance().getId() < 0x00ffffff) {
		throw std::runtime_error("Cannot do lowid <-> lowid callback!");
	}

	CHECK_THROW(m_serverSocket);
	CHECK_THROW(m_serverSocket->isConnected());

	*m_serverSocket << ED2KPacket::ReqCallback(id);
}

void ServerList::onSharedFileEvent(SharedFile *sf, int evt) {
	if (!m_serverSocket) {
		return;
	} else if (!m_serverSocket->isConnected()) {
		return;
	}

	if (evt == SF_METADATA_ADDED) {
		if (!sf->getMetaData()) {
			return; // can't publish w/o metadata
		}

		if (!sf->getMetaData()->getHashSetCount()) {
			return; // can't publish w/o hashes
		}

		// Only a partial solution, but avoids publishing files which
		// have less than 9500kb downloaded. To be 100% correct, we
		// should check here if the file has at least one complete chunk
		if (sf->isPartial()) {
			if (sf->getPartData()->getCompleted() < ED2K_PARTSIZE) {
				return;
			}
		}

		publishFile(sf);
	}
}

void ServerList::publishFile(SharedFile *sf) {
	CHECK_THROW(m_serverSocket);
	CHECK_THROW(m_serverSocket->isConnected());
	CHECK_THROW(sf->getMetaData());
	CHECK_THROW(sf->getMetaData()->getHashSetCount());

	logTrace(TRACE, boost::format("Publishing file %s") % sf->getName());

	MetaData *md = sf->getMetaData();
	for (uint32_t i = 0; i < md->getHashSetCount(); ++i) {
		HashSetBase *hs = md->getHashSet(i);
		if (hs->getFileHashTypeId() == CGComm::OP_HT_ED2K) {
			bool useZlib = m_currentServer->getTcpFlags() & FL_ZLIB;
			*m_serverSocket << ED2KPacket::OfferFiles(
				makeED2KFile(sf, md, hs, useZlib),
				useZlib ? PR_ZLIB : PR_ED2K
			);
			break;
		}
	}
}

void ServerList::reqSources(Download &d) {
	assert(d.getPartData()->isRunning());
	if (m_serverSocket && m_serverSocket->isConnected()) {
		*m_serverSocket << ED2KPacket::ReqSources(
			d.getHash(), d.getSize()
		);
		logTrace(TRACE,
			boost::format("Requesting sources for %s")
			% d.getPartData()->getDestination().leaf()
		);
	}
}

/**
 * Scan files list, skipping all entries that are not partial, do not have
 * MetaData or do not have hashes, and request sources for the rest of them.
 */
void ServerList::reqSources() {
	CHECK_THROW(m_serverSocket);
	CHECK_THROW(m_serverSocket->isConnected());

	if (m_lastSourceRequest + SOURCEREASKTIME > Utils::getTick()) {
		logTrace(TRACE, "Delaying Source Reask.");
		return; // don't reask  not just yet
	}

	std::vector<Download*> downloads;
	DownloadList& list = DownloadList::instance();
	// can't use std::copy since Iter isn't fully standard-conformant
	for (DownloadList::Iter i = list.begin(); i != list.end(); ++i) {
		downloads.push_back(&*i);
	}
	std::sort(
		downloads.begin(), downloads.end(),
		boost::bind(&Download::getSourceCount, _1) <
		boost::bind(&Download::getSourceCount, _2)
	);
	std::string packet;
	uint32_t written = 0, packets = 0;
	for (size_t i = 0; i < downloads.size(); ++i) {
		if (!downloads[i]->getPartData()->isRunning()) {
			continue;
		}
		packet += ED2KPacket::ReqSources(
			downloads[i]->getHash(), downloads[i]->getSize()
		);
		if (++written == 15 && i <= 15) {
			// first 15 downloads - request now
			*m_serverSocket << packet;
			packet.clear();
			written = 0;
			++packets;
		} else if (written == 15 && packets < 5) {
			// next 15 downloads, up to a maximum of 4 additional
			// packets scheduled over the course of next 16 minutes
			Utils::timedCallback(
				boost::bind(
					&ServerList::reqMoreSources, this, 
					m_serverSocket, packet
//					&ED2KClientSocket::write, m_serverSocket, 
//					packet
				), packets * 245 * 1000
			);
			packet.clear();
			written = 0;
			++packets;
		}
	}
	if (written && packets == 0) {
		*m_serverSocket << packet;
	} else if (written && packets < 5) {
		Utils::timedCallback(
			boost::bind(
				&ServerList::reqMoreSources, this, 
				m_serverSocket, packet
//					&ED2KClientSocket::write, m_serverSocket, 
//					packet
			), packets * 245 * 1000
		);
	}

	// schedule next loop
	m_lastSourceRequest = Utils::getTick();
	getEventTable().postEvent(this, EVT_REQSOURCES, SOURCEREASKTIME);
}

void ServerList::onServerListEvent(ServerList *sl, ServerListEvent evt) {
	CHECK_THROW(sl == this);

	if (evt == EVT_REQSOURCES) {
		if (m_serverSocket && m_serverSocket->isConnected()) {
			reqSources();
		} else {
			m_lastSourceRequest = Utils::getTick();
			getEventTable().postEvent(
				this, EVT_REQSOURCES, SOURCEREASKTIME
			);
		}
	} else if (evt == EVT_PINGSERVER) {
		if (m_serverSocket && m_serverSocket->isConnected()) {
			logTrace(TRACE, "Pinging server with OfferFiles...");
			*m_serverSocket << ED2KPacket::OfferFiles();
		}
	} else if (evt == EVT_CONNECT) {
		if (!m_serverSocket) {
			connect();
		}
	} else if (evt == EVT_QUERYSERVER) {
		queryNextServer();
	} else {
		logDebug("Unknown ServerList event.");
	}
}

void ServerList::handleGlobSources(std::istringstream &i, IPV4Address from) try{
	ED2KPacket::GlobFoundSources p(i);
	uint32_t total = p.size();
	uint32_t added = 0;
	(void)total; // suppress warning when trace is turned off

	ED2KPacket::GlobFoundSources::Iter it = p.begin();
	while (it != p.end()) {
		added += Detail::foundSource(
			p.getHash(), *it++, IPV4Address(), true
		);
	}

	logTrace(TRACE_GLOBSRC,
		boost::format(
			"GlobGetSources: Received %d sources for hash "
			"%s from %s (and %d duplicates)."
		) % added % p.getHash().decode() % from
		% (total - added)
	);
	if (static_cast<size_t>(i.tellg()) < i.str().size() - 1) {
		uint8_t prot = Utils::getVal<uint8_t>(i);
		uint8_t opcode = Utils::getVal<uint8_t>(i);
		if (prot == PR_ED2K && opcode == OP_GLOBFOUNDSOURCES) {
			handleGlobSources(i, from);
		} else {
			logDebug(
				boost::format(
					"Extra data in GlobFoundSources "
					"frame: %s"
				) % Utils::hexDump(i.str().substr(i.tellg()))
			);
		}
	}
} catch (Utils::ReadError&) {
	logDebug("Unexpected EOF found while parsing GlobFoundSources packet.");
} catch (std::exception &e) {
	logDebug(
		boost::format(
			"Unexpected exception while handling GlobFoundSources "
			"packet: %s"
		) % e.what()
	);
} MSVC_ONLY(;)

void ServerList::handleGlobStatRes(std::istringstream &i, IPV4Address from) try{
	ED2KPacket::GlobStatRes packet(i);
	from.setPort(from.getPort() - 4); // to get the TCP port

	AddrIter it = m_list->get<1>().find(from);
	if (it == m_list->get<1>().end()) {
		logTrace(TRACE,
			boost::format("Passivly adding server %s.")
			% from
		);
		Server *s = new Server(from);
		it = m_list->get<1>().insert(s).first;
	} else if (packet.getChallenge() != (*it)->getChallenge()) {
		// ignore packets with wrong challenge
		logDebug(
			boost::format(
				"[%s] UDP Server responded with "
				"invalid challenge %d (expected %d)"
			) % from % packet.getChallenge() % (*it)->getChallenge()
		);
		return;
	}

	Object::disableNotify(); // optimization
	// prevent's ping-time showing enormously large pings
	if (!(*it)->pingInProgress()) {
		(*it)->setPing(0);
	} else {
		(*it)->setPingInProgress(false);
		uint64_t pingTime = Utils::getTick() - (*it)->getLastUdpQuery();
		(*it)->setPing(pingTime);
		(*it)->setLastPing(pingTime);
	}

	(*it)->setUsers(packet.getUsers());
	(*it)->setFiles(packet.getFiles());
	(*it)->setMaxUsers(packet.getMaxUsers());
	(*it)->setSoftLimit(packet.getSoftLimit());
	(*it)->setHardLimit(packet.getHardLimit());
	(*it)->setUdpFlags(packet.getUdpFlags());
	(*it)->setLowIdUsers(packet.getLowIdUsers());
	(*it)->setFailedCount(0);
	Object::enableNotify();
	(*it)->notify();

	boost::format fmt(
		"[%s] Received GlobStatRes: Users=%d Files=%d "
		"LowUsers=%d MaxUsers=%d SoftLimit=%d HardLimit=%d "
		"UdpFlags=%s Ping: %dms"
	);
	fmt % (*it)->getAddr() % (*it)->getUsers() % (*it)->getFiles();
	fmt % (*it)->getLowIdUsers() % (*it)->getMaxUsers();
	fmt % (*it)->getSoftLimit() % (*it)->getHardLimit();
	fmt % Utils::hexDump((*it)->getUdpFlags()) % (*it)->getPing();
	logTrace(TRACE, fmt);

	QTIter tmp(m_list->project<3>(it));
	udpGetSources(tmp);
} catch (Utils::ReadError&) {
	logDebug("Unexpected EOF while parsing GlobStatRes packet.");
} catch (std::exception &e) {
	logDebug(
		boost::format(
			"Unexpected exception while handling "
			"GlobStatRes packet: %s"
		) % e.what()
	);
} MSVC_ONLY(;)

void ServerList::handleGlobSearchRes(std::istringstream &i, IPV4Address from)
try {
	logTrace(TRACE,
		boost::format("Received GlobSearchRes from %s") % from
	);

	ED2KPacket::GlobSearchRes p(i);
	UAddrIter it(m_list->get<4>().find(from));

	if (it != m_list->get<4>().end() && (*it)->currentSearch()) {
		// search stopped - ignore these results
		if (!(*it)->currentSearch()->isRunning()) {
			return;
		}
		for (size_t i = 0; i < p.getCount(); ++i) {
			(*it)->currentSearch()->addResult(p.getResult(i));
		}
		(*it)->currentSearch()->notifyResults();
		if ((*it)->currentSearch()->getResultCount() > 30) {
			Utils::timedCallback(
				boost::bind(
					&Search::stop, 
					(*it)->currentSearch().get()
				), 200
			);
		}
		// Server::m_globSearch is reset from timed callback
	} else {
		logDebug(
			boost::format(
				"Received search results from unknown "
				"server %s"
			) % from
		);
	}
} catch (std::exception &e) {
	logError(
		boost::format("Handling GlobSearchRes packet from %s: %s") 
		% from % e.what()
	);
	logError("Packet contents: " + Utils::hexDump(i.str()));
} MSVC_ONLY(;)

void ServerList::queryNextServer() {
	// no servers
	if (!m_list->size()) {
		return;
	}

	MIServerList::nth_index<3>::type &list = m_list->get<3>();
	QTIter it = list.lower_bound(0);

	while (it != list.end()) {
		if (!(*it)->getAddr().getIp() || !(*it)->getAddr().getPort()) {
			++it; // null ip or port
		} else {
			break; // found valid one
		}
	}
	CHECK_RET(it != list.end());

	uint64_t lastQuery = (*it)->getLastUdpQuery();
	uint64_t curTick = Utils::getTick();

	if (lastQuery + SOURCEREASKTIME > curTick) {
		uint64_t timeDiff = lastQuery + SOURCEREASKTIME - curTick;
		getEventTable().postEvent(this, EVT_QUERYSERVER, timeDiff);
		logTrace(TRACE,
			"Delaying UDPServerQuery: lastQuery was less "
			"than 20 minutes ago."
		);
		return; // query is delayed
	}

	QTIter tmp(it);
	pingServer(tmp);

	m_list->get<3>().modify(
		it, bind(&Server::setLastUdpQuery, __1, curTick)
	);

	// schedule next query
	it = list.lower_bound(0);
	uint64_t nextQuery = (*it)->getLastUdpQuery() + SOURCEREASKTIME;
	if (nextQuery < curTick) {
		nextQuery = curTick + SOURCEREASKTIME / m_list->size();
	}
	getEventTable().postEvent(this, EVT_QUERYSERVER, nextQuery-curTick);
	logTrace(TRACE_GLOBSRC,
		boost::format("Next GlobGetSources scheduled to %.2fs")
		% ((nextQuery - curTick) / 1000.0)
	);
}

void ServerList::pingServer(QTIter &it) try {
	CHECK_THROW(it != m_list->get<3>().end());

	IPV4Address to((*it)->getAddr().getIp(), (*it)->getUdpPort());
	ED2KPacket::GlobStatReq packet;
	Client::getUdpSocket()->send(packet, to);

	(*it)->setPingInProgress(true);
	(*it)->setChallenge(packet.getChallenge());

	// 10sec ping timeout
	Server::getEventTable().postEvent(*it, EVT_PINGTIMEOUT, 10*1000);

	logTrace(TRACE, boost::format("[%s] Sending GlobStatReq.") % to);
} catch (SocketError &e) {
	(void)e;
	logTrace(TRACE,
		boost::format("[%s] Fatal error sending UDP Ping: %s")
		% (*it)->getAddr() % e.what()
	);
}
MSVC_ONLY(;)

void ServerList::udpGetSources(QTIter &it) try {
	CHECK_THROW(it != m_list->get<3>().end());

	uint32_t limit = 1; // how many requests to do
	IPV4Address to((*it)->getAddr().getIp(), (*it)->getUdpPort());
	bool sendSize = (*it)->getUdpFlags() & Server::FL_GETSOURCES2;

	if ((*it)->getUdpFlags() & Server::FL_GETSOURCES) {
		// Server accepts 512-byte UDP packets, so when sending size,
		// max count is 25 (25*20+2), w/o size it's 31 (31*16+2)
		limit = sendSize ? 25 : 31;
	}

	std::vector<Download*> downloads;
	DownloadList& list = DownloadList::instance();
	// can't use std::copy since Iter isn't fully standard-conformant
	for (DownloadList::Iter i = list.begin(); i != list.end(); ++i) {
		downloads.push_back(&*i);
	}
	std::sort(
		downloads.begin(), downloads.end(),
		boost::bind(&Download::getSourceCount, _1) <
		boost::bind(&Download::getSourceCount, _2)
	);

	uint32_t cnt = 0;
	ED2KPacket::GlobGetSources packet(sendSize);
	if (downloads.size() < limit) {
		limit = downloads.size();
	}
	for (size_t i = 0; i < limit; ++i) {
		Download *d = downloads[i];
		if (d->getPartData()->isRunning()) {
			packet.addHash(d->getHash(), d->getSize());
			++cnt;
		} else if (limit < downloads.size()) {
			++limit;
		}
	}

	if (!cnt) {
		return; // no files queried ...
	}

	boost::format fmt("[%s] Sending GlobGetSources %s");
	fmt % to;
	if ((*it)->getUdpFlags() & Server::FL_GETSOURCES2) {
		fmt % "(NewFormat)";
	} else if ((*it)->getUdpFlags() & Server::FL_GETSOURCES) {
		fmt % "(ManyFiles)";
	} else {
		fmt % "";
	}
	logTrace(TRACE_GLOBSRC, fmt);

	Client::getUdpSocket()->send(packet, to);
} catch (SocketError &) {
	logTrace(TRACE,
		boost::format("[%s] Fatal error sending UDPGetSources: %s")
		% (*it)->getAddr()
	);
}
MSVC_ONLY(;)

void ServerList::onServerEvent(Server *s, int evt) {
	if (evt == EVT_LOGINTIMEOUT) {
		if (m_status != ST_CONNECTED) {
			if (s == m_currentServer) {
				s->addFailedCount();
			}
			if (s == m_currentServer || !m_currentServer) {
				logMsg(
					"eDonkey2000: Server login "
					"attempt timed out."
				);
				connect();
			}
		}
	} else if (evt != EVT_PINGTIMEOUT) {
		logDebug("Unknown event in ServerList::onServerEvent");
		return;
	}

	if (m_list->find(s) == m_list->end()) {
		return; // invalid, outdated pointer
	} else if (s == m_currentServer) {
		return; // ping timeouts/failcounts not applied to curserver
	}

	if (s->pingInProgress()) {
		logTrace(TRACE,
			boost::format("[%s] GlobStatReq: Ping timed out.")
			% s->getAddr()
		);
		s->setPingInProgress(false);
		s->addFailedCount();
		if (s->getFailedCount() > 2) {
			logTrace(TRACE,
				boost::format(
					"[%s] GlobStatReq: Removing dead server"
					" (3 ping timeouts)."
				) % s->getAddr()
			);
			m_list->erase(s);
			delete s;
		}
	}
}

IPV4Address ServerList::getCurServerAddr() const {
	if (m_currentServer == 0) {
		throw std::runtime_error("Not connected.");
	}
	return m_currentServer->getAddr();
}

void ServerList::addServer(IPV4Address srv) {
	AddrIter it = m_list->get<1>().find(srv);
	if (it == m_list->get<1>().end()) {
		Server *s = new Server(srv);
		m_list->insert(s);
	}
}

uint32_t ServerList::getOperCount() const { 
#ifndef NDEBUG
	return 7; 
#else
	return 6;
#endif
}

Object::Operation ServerList::getOper(uint32_t n) const {
	if (n == 0) {
		Operation op("connect", true);
		op.addArg(Operation::Argument("server", true, ODT_STRING));
		return op;
	} else if (n == 1) {
		Operation op("add", true);
		op.addArg(Operation::Argument("ip", true, ODT_STRING));
		op.addArg(Operation::Argument("port", true, ODT_INT));
		return op;
	} else if (n == 2) {
		return Operation("status", false);
	} else if (n == 3) {
		return Operation("list", false);
	} else if (n == 4) {
		Operation op("connectId", true);
		op.addArg(Operation::Argument("id", true, ODT_INT));
		return op;
	} else if (n == 5) {
		Operation op("removeId", true);
		op.addArg(Operation::Argument("id", true, ODT_INT));
		return op;
#ifndef NDEBUG
	} else if (n == 5) {
		return Operation("deadclients", false);
#endif
	} else {
		throw std::runtime_error("Unknown command.");
	}
}

//! Predicate for sorting server objects by user count
struct UserCountPred {
	bool operator()(const Server *const one, const Server *const two) const{
		return one->getUsers() < two->getUsers();
	}
};

void ServerList::doOper(const Object::Operation &op) {
	if (op.getName() == "add") {
		std::string ip = op.getArg("ip").getValue();
		uint16_t port = boost::lexical_cast<uint16_t>(
			op.getArg("port").getValue()
		);
		IPV4Address addr(ip, port);
		if (m_list->get<1>().find(addr) == m_list->get<1>().end()) {
			m_list->insert(new Server(addr));
			logMsg(boost::format("Added server %s") % addr);
		} else {
			throw std::runtime_error(
				"This server is already listed."
			);
		}
		return;
	} else if (op.getName() == "status") {
		uint64_t users = 0;
		uint64_t files = 0;
		Detail::ServIter it = m_list->begin();
		while (it != m_list->end()) {
			users += (*it)->getUsers();
			files += (*it++)->getFiles();
		}
		logMsg(
			boost::format(
				"eDonkey2000 network: %d users "
				"online, sharing %d files."
			) % users % files
		);
		if (m_currentServer) {
			boost::format fmt1(
				"Connected to server: [%s] %s with %s ID (%d)"
			);
			fmt1 % m_currentServer->getAddr();
			fmt1 % m_currentServer->getName();
			if (ED2K::instance().isHighId()) {
				fmt1 % "High" % ED2K::instance().getId();
			} else {
				fmt1 % "Low" % ED2K::instance().getId();
			}
			logMsg(fmt1);
			boost::format fmt2(
				"%d users and %d files online "
				"on current server."
			);
			fmt2 % m_currentServer->getUsers();
			fmt2 % m_currentServer->getFiles();
			logMsg(fmt2);
		}
		return;
	} else if (op.getName() == "connectId") {
		uint32_t id = boost::lexical_cast<uint32_t>(
			op.getArg("id").getValue()
		);
		if (id) {
			Object *obj = findChild(id);
			if (obj) {
				Server *s = dynamic_cast<Server*>(obj);
				if (s) {
					connect(s);
				}
			}
		}
		return;
	} else if (op.getName() == "removeId") {
		uint32_t id = boost::lexical_cast<uint32_t>(
			op.getArg("id").getValue()
		);
		if (id) {
			Object *obj = findChild(id);
			if (obj) {
				Server *s = dynamic_cast<Server*>(obj);
				if (s) {
					m_list->erase(s);
					delete s;
				}
			}
		}
		return;
	} else if (op.getName() == "list") {
		std::vector<Server*> tmp;
		tmp.resize(m_list->size());
		std::copy(m_list->begin(), m_list->end(), tmp.begin());
		std::sort(tmp.begin(), tmp.end(), UserCountPred());
		for (uint32_t i = 0; i < tmp.size(); ++i) {
			boost::format fmt("[%21s]%20s Users: %d Files: %d");
			Server *srv = tmp[i];
			fmt % srv->getAddr();
			if (srv->getAddr().getAddrStr() != srv->getName()) {
				std::string tmp = srv->getName();
				if (tmp.size() > 19) {
					fmt % (" " + tmp.substr(0, 19));
				} else {
					fmt % tmp;
				}
			} else {
				fmt % "";
			}
			fmt % srv->getUsers() % srv->getFiles();
			logMsg(fmt);
		}
		return;
	} else if (op.getName() == "deadclients") {
		ClientList::instance().printDeadClients();
		return;
	} else if (op.getName() == "connect") {
		std::string name = op.getArg("server").getValue();
		std::pair<NameIter, NameIter> i = m_list->get<2>().equal_range(
			name
		);
		if (std::distance(i.first, i.second) > 1) {
			throw std::runtime_error("Ambiguous server name.");
		} else if (i.first == m_list->get<2>().end()) {
			throw std::runtime_error("No such server.");
		} else {
			connect(*i.first);
		}
	} else {
		throw std::runtime_error("Unknown operation.");
	}
}

uint32_t ServerList::getDataCount() const { return 3; }
std::string ServerList::getFieldName(uint32_t n) const {
	switch (n) {
		case 0:  return "Status";
		case 1:  return "CurrentServerId";
		case 2:  return "ClientId";
		default: return "";
	}
}

std::string ServerList::getData(uint32_t n) const {
	switch (n) {
		case 0: if (m_status == ST_CONNECTING) {
				return "Connecting";
			} else if (m_status == ST_LOGGINGIN) {
				return "Logging in";
			} else {
				return "Connected";
			}
		case 1: if (m_currentServer) {
				return boost::lexical_cast<std::string>(
					m_currentServer->getId()
				);
			} else {
				return "0";
			}
		case 2: return boost::lexical_cast<std::string>(
				ED2K::instance().getId()
			);
		default: return "";
	}
}

void ServerList::doGlobSearch(
	SearchPtr search, boost::shared_ptr<std::list<Server*> > srv
) {
	CHECK_RET(srv);
	if (!search->isRunning() || !m_list->size() || !srv->size()) {
		return;
	}
	uint32_t callback = 500; // default 500ms delay
	size_t loopCnt = srv->size();

	while (srv->size() && loopCnt--) {
		if (m_list->find(srv->front()) == m_list->end()) {
			srv->pop_front();
		} else if (srv->front()->currentSearch()) {
			srv->push_back(srv->front());
			srv->pop_front();
		} else if (!srv->front()->supportsUdpSearch()) {
			srv->pop_front();
		}
	}
	if (srv->size() && srv->front()->currentSearch()) {
		logTrace(TRACE,
			"No valid servers found for global search, "
			"retrying in 5 seconds."
		);
		callback = 5000;
	} else if (srv->size()) {
		ED2KPacket::GlobSearchReq packet(search);
		IPV4Address to(
			srv->front()->getAddr().getIp(), 
			srv->front()->getUdpPort()
		);
		Client::getUdpSocket()->send(packet, to);
		srv->front()->setCurrentSearch(search);
		Utils::timedCallback(
			boost::bind(
				&Server::setCurrentSearch,
				srv->front(), SearchPtr()
			), 20000
		);
		srv->pop_front();
		logTrace(TRACE,
			boost::format("Sent global search request to %s") % to
		);
	}
	if (srv->size()) {
		Utils::timedCallback(
			boost::bind(
				&ServerList::doGlobSearch, this,
				search, srv
			), callback
		);
	} else {
		std::string keywords;
		for (uint32_t i = 0; i < search->getTermCount(); ++i) {
			keywords += search->getTerm(i);
			if (i + 1 < search->getTermCount()) {
				keywords += " ";
			}
		}
		logMsg(
			boost::format(
				"eDonkey2000: Global search "
				"with keywords '%s' ended."
			) % keywords
		);
	}
}

void ServerList::reqMoreSources(ED2KClientSocket *sock, std::string data) {
	if (m_serverSocket && sock == m_serverSocket && sock->isConnected()) {
		logDebug("Sending source request to server.");
		CHECK(data.size());
		*m_serverSocket << data;
	} else {
		logDebug(
			"Source request callback, but server "
			"has changed since then."
		);
	}
}

void ServerList::configChanged(const std::string &key,const std::string &value){ 
	if (key == "ed2k/FindServers") {
		bool fServers = boost::lexical_cast<bool>(value);
		if (fServers && !m_foundServerConn.connected()) {
			m_foundServerConn = Detail::foundServer.connect(
				boost::bind(&ServerList::addServer, this, _1)
			);
		} else if (!fServers && m_foundServerConn.connected()) {
			m_foundServerConn.disconnect();
		}
	}
}

} // end namespace Donkey
