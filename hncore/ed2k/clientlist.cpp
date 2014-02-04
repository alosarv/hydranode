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
 * \file clientlist.cpp Implementation of ClientList class
 */

#include <hncore/ed2k/clientlist.h>
#include <hncore/ed2k/parser.h>
#include <hncore/ed2k/ed2k.h>
#include <hncore/ed2k/clientext.h>
#include <hncore/ed2k/downloadlist.h>
#include <hncore/ed2k/serverlist.h>
#include <hncore/clientmanager.h>
#include <hncore/sharedfile.h>
#include <hnbase/prefs.h>
#include <hnbase/ssocket.h>
#include <boost/lambda/bind.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <boost/filesystem/operations.hpp>

namespace Donkey {
enum Ed2k_Constants {
	/**
	 * Size of the udp listener's input buffer size; UDP packets exceeding
	 * this size will be truncated. Since the UDP buffer is reused for new
	 * packets (and not reallocated), changing this shouldn't have any
	 * effect on performance.
	 */

	UDP_BUFSIZE = 1024,
	/**
	 * How often to recalculate the queue scores and resort the queue. Since
	 * this is rather time-consuming operation if the queue is big, this is
	 * done on regular intervals.
	 */

	QUEUE_UPDATE_TIME = 10000,
	/**
	 * When previous UDP attempt timeouts, how long to wait until next try?
	 */

	UDP_REASK_INTERVAL = 10*60*1000,
	/**
	 * How long we keep clients in queue who haven't performed any kind of
	 * reask.
	 */

	QUEUE_DROPTIME = 60*60*1000,

	/**
	 * Specifies the TCP connection attempt timeout.
	 */
	CONNECT_TIMEOUT = 15000
};

IMPLEMENT_EVENT_TABLE(ClientList, ClientList*, ClientList::ClientListEvt);

const std::string TRACE_CLIENT = "ed2k.client";
const std::string TRACE_SECIDENT = "ed2k.secident";
const std::string TRACE_DEADSRC = "ed2k.deadsource";
const std::string TRACE_CLIST = "ed2k.clientlist";
const std::string TRACE_SRCEXCH = "ed2k.sourceexchange";

namespace Detail {
//! ClientList indexes
struct ClientListIndices : boost::multi_index::indexed_by<
	boost::multi_index::ordered_unique<
		boost::multi_index::identity<Client*>
	>,
	boost::multi_index::ordered_non_unique<
		boost::multi_index::const_mem_fun<
			Client, uint32_t, &Client::getId
		>
	>,
	boost::multi_index::ordered_non_unique<
		boost::multi_index::const_mem_fun<
			Client, Hash<MD4Hash>, &Client::getHash
		>
	>
> {};
//! ClientList type
struct CList : boost::multi_index_container<Client*, ClientListIndices> {};

//! Different index ID's
enum { ID_Client, ID_Id, ID_Hash };
typedef CList::nth_index<ID_Client>::type CMap;
typedef CList::nth_index<ID_Id>::type IDMap;
typedef CList::nth_index<ID_Hash>::type HashMap;
typedef CMap::iterator CIter;
typedef IDMap::iterator IIter;
typedef HashMap::iterator HashIter;

} // namespace Detail
using namespace Detail;

// dummy constructors/destructors. Don't do anything fancy here - do in init()!
ClientList::ClientList() : m_clients(new CList), m_listener(),
m_udpBuffer(new char[UDP_BUFSIZE]) {
	// regen queue every 10 seconds
	getEventTable().postEvent(this, EVT_REGEN_QUEUE, QUEUE_UPDATE_TIME);
	getEventTable().addHandler(this, this, &ClientList::onClientListEvent);
	Client::getEventTable().addAllHandler(this, &ClientList::onClientEvent);

	// register our trace masks
	Log::instance().addTraceMask(TRACE_CLIENT);
	Log::instance().addTraceMask(TRACE_CLIST);
	Log::instance().addTraceMask(TRACE_SECIDENT);
	Log::instance().addTraceMask(TRACE_DEADSRC);
	Log::instance().addTraceMask(TRACE_SRCEXCH);
}

ClientList::~ClientList() {}

void ClientList::exit() {
	logDebug("ClientList exiting.");
	Log::instance().disableTraceMask(TRACE_CLIENT);
	Log::instance().disableTraceMask(TRACE_CLIST);
	Log::instance().disableTraceMask(TRACE_SECIDENT);

	delete m_listener;
	m_listener = 0;
	Client::getUdpSocket()->destroy();
	Client::setUdpSocket(0);

	for (CIter i = m_clients->begin(); i != m_clients->end(); ++i) {
		delete *i;
	}
}

void ClientList::init() {
	if (m_listener) {
		delete m_listener;
	}
	// bring up TCP listener
	m_listener = new ED2KServerSocket(this, &ClientList::onServerEvent);
	uint16_t port = ED2K::instance().getTcpPort();
	uint16_t upperLimit = port + 10; // how many fallback ports

	while (port < upperLimit) {
		try {
			m_listener->listen(0, port);
			break;
		} catch (SocketError &e) {
			logWarning(boost::format(
				"Unable to start ED2K listener on port %d: %s"
			) % port % e.what());
			++port;
		}
	}
	m_listener->setPriority(SocketBase::PR_HIGH);

	if (!m_listener->isListening()) {
		throw std::runtime_error(
			"Unable to start ED2K listener, giving up."
		);
	}
	ED2K::instance().setTcpPort(port);
	logMsg(
		boost::format(
			COL_BGREEN "ED2K TCP listener " COL_NONE 
			"started on port " COL_BCYAN "%d" COL_NONE
		) % port
	);

	// bring up UDP listener
	Client::setUdpSocket(new ED2KUDPSocket);
	port = ED2K::instance().getUdpPort();
	upperLimit = port + 10; // how many fallback ports
	while (port < upperLimit) {
		try {
			Client::getUdpSocket()->listen(0, port);
			break;
		} catch (SocketError &e) {
			logWarning(boost::format(
				"Unable to start ED2K UDP listener "
				"on port %d: %s"
			) % port % e.what());
			++port;
		}
	}
	ED2K::instance().setUdpPort(port);
	Client::getUdpSocket()->setHandler(
		boost::bind(&ClientList::onUdpData, this, _1, _2)
	);
	logMsg(
		boost::format(
			COL_BGREEN "ED2K UDP listener " COL_NONE
			"started on port " COL_BCYAN "%d" COL_NONE
		) % port
	);

	std::string filter = Prefs::instance().read<std::string>(
		"/MessageFilter", ""
	);
	if (filter.size() && filter != "disable") try {
		boost::filesystem::path p(filter);
		CHECK_THROW(boost::filesystem::exists(p));
		std::ifstream ifs(filter.c_str());
		std::string tmp;
		uint32_t cnt = 0;
		while (getline(ifs, tmp)) {
			if (tmp.size()) {
				m_msgFilter.push_back(tmp);
				++cnt;
			}
		}
		logMsg(
			boost::format(
				"Message filter loaded, %d messages filtered."
			) % cnt
		);
	} catch (std::exception&) {
		filter.clear();
	} else if (filter != "disable") {
		// default message filter
		m_msgFilter.push_back("AUTO MESSAGE");
		m_msgFilter.push_back("L33cher Team and buy a Ketamine");
		m_msgFilter.push_back("eMule FX the BEST eMule ever!");
		m_msgFilter.push_back("ZamBoR 2");
		m_msgFilter.push_back("DI-Emule V.2.0");
	}

	Prefs::instance().write("/MessageFilter", filter);

	Detail::changeId.connect(
		boost::bind(&ClientList::onIdChange, this, _1, _2)
	);
	Detail::foundSource.connect(
		boost::bind(&ClientList::addSource, this, _1, _2, _3, _4)
	);
	Detail::checkMsgFilter.connect(
		boost::bind(&ClientList::msgFilter, this, _1)
	);
	Prefs::instance().valueChanged.connect(
		boost::bind(&ClientList::configChanged, this, _1, _2)
	);
	SharedFile::getEventTable().addAllHandler(this, &ClientList::onSFEvent);
}

void ClientList::onServerEvent(ED2KServerSocket *s, SocketEvent evt) {
	assert(s == m_listener);

	if (evt == SOCK_ACCEPT) {
		ED2KClientSocket *sock = s->accept();
		logTrace(TRACE_CLIST,
			boost::format("Client connected from %s")
			% sock->getPeer()
		);
		Client *client = new Client(sock);
		m_clients->insert(client);
	} else {
		logDebug("Unknown event in ClientList::onServerEvent");
	}
}

//! handles Client::changeId signal
//! \todo Also compare hashes; doesn't work correctly now
//! because hash-index isn't correctly updated right now.
void ClientList::onIdChange(Client *c, uint32_t newId) {
	using boost::lambda::bind;

	logTrace(TRACE_CLIST,
		boost::format("[%s] %p: Client ID changed to %d")
		% c->getIpPort() % c % newId
	);

	IDMap &idList = m_clients->get<ID_Id>();

	CIter i = m_clients->get<ID_Client>().find(c);
	CHECK_THROW(i != m_clients->get<ID_Client>().end());

	IIter idIt = m_clients->project<ID_Id>(i);
	idList.modify(idIt, bind(&Client::m_id, __1(__1)) = newId);

	std::pair<IIter, IIter> r = idList.equal_range(c->m_id);
	logTrace(TRACE_CLIST,
		boost::format("[%s] Found %d candidates")
		% c->getIpPort() % std::distance(r.first, r.second)
	);
	for (IIter j = r.first; j != r.second; ++j) {
		CHECK_FAIL((*j)->getId() == c->getId());
		boost::format fmt("[%s] Candidate: %s %s ... %s");
		fmt % c->getIpPort() % (*j)->getIpPort() % *j;
		if (*j == c) {
			logTrace(TRACE_CLIST, fmt % "Is myself ...");
			continue;
		} else if ((*j)->getTcpPort() != c->getTcpPort()) {
			logTrace(TRACE_CLIST, fmt % "Wrong TCPPort");
		} else {
			logTrace(TRACE_CLIST, fmt % "Merging...");
			(*j)->merge(c);
			c->destroy();
			c = *j;
			break;
		}
	}

	if (c->m_uploadInfo) {
		m_uploading.insert(c);
	}

	// older emules need also to do MuleInfo packet before considering
	// handshake completed - hence the check
	if (!c->isMule() || (c->isMule() && c->getVerMin() >= 42)) {
		c->handshakeCompleted();
	}
}

void ClientList::removeClient(Client *c) {
	logTrace(TRACE_CLIST, boost::format("Destroying client %p") % c);
	CHECK_RET(m_clients->find(c) != m_clients->end());
	m_clients->erase(c);
	m_uploading.erase(c);
	m_queued.erase(c);
	delete c;
	if (m_uploading.size() < getSlotCount()) {
		startNextUpload();
	}
}

// event handler for events emitted from Client object
void ClientList::onClientEvent(Client *c, ClientEvent evt) {
	switch (evt) {
		case EVT_DESTROY:
			removeClient(c);
			break;
		case EVT_UPLOADREQ: try {
			m_uploading.erase(c); // just in case

			// no uploading/queued clients -> start one
			if (m_uploading.size() < getSlotCount()) {
				if (!m_queued.size()) {
					c->startUpload();
					m_uploading.insert(c);
				}
			} else if (m_queued.find(c) == m_queued.end()) {
				// wasn't found already in queue - push back
				m_queued.insert(c);
				m_queue.push_back(c);
				CHECK_THROW(c->m_queueInfo);
				c->m_queueInfo->setQR(m_queued.size());
				if (c->isConnected()) {
					c->sendQR();
				}
			} else if (c->isConnected()) {
				// was found in queue - just send current rank
				c->sendQR();
			}
			break;
		} catch (std::exception &e) {
			logDebug(
				boost::format("[%s] OnUploadRequest: %s")
				% c->getIpPort() % e.what()
			);
			if (m_uploading.size() < getSlotCount()) {
				startNextUpload();
			}
			break;
		}
		case EVT_CANCEL_UPLOADREQ:
			m_uploading.erase(c);
			m_queued.erase(c);
			if (m_uploading.size() < getSlotCount()) {
				startNextUpload();
			}
			break;
		case EVT_CALLBACK_T:
			if (c->callbackInProgress() && !c->isConnected()) {
				logTrace(TRACE_CLIENT, boost::format(
					"[%s] %p LowID callback timed out."
				) % c->getIpPort() % c);
				removeClient(c);
			}
			break;
		case EVT_REASKFILEPING:
			if (c->m_sourceInfo && !c->m_downloadInfo) try {
				c->reaskForDownload();
			} catch (std::exception &e) {
				logDebug(
					boost::format(
						"[%s] Error reasking for "
						"download: %s"
					) % c->getIpPort() % e.what()
				);
				c->destroy();
			}
			break;
		case EVT_REASKTIMEOUT:
			if (c->reaskInProgress() && !c->isConnected()) {
				logTrace(TRACE_CLIENT, boost::format(
					"[%s] UDP Reask #%d timed out."
				) % c->getIpPort() % static_cast<int>(
					c->m_failedUdpReasks
				));

				c->m_reaskInProgress = false;

				// try UDP 2 times, if that fails, try TCP
				// callback, if that also fails, drop the client
				if (++c->m_failedUdpReasks > 2) try {
					logTrace(TRACE_CLIENT, boost::format(
						"[%s] Attempting TCP Reask..."
					) % c->getIpPort());
					c->establishConnection();
				} catch (std::exception &e) {
					logDebug(boost::format(
						"[%s] Fatal error attempting "
						"to establish connection: %s"
					) % c->getIpPort() % e.what());
					c->destroy();
				} else {
					// re-try in 10 minutes
					Client::getEventTable().postEvent(
						c, EVT_REASKFILEPING,
						UDP_REASK_INTERVAL
					);
				}
			}
			break;
		default:
			logDebug("ClientList::onClientEvent: Unknown event.");
			break;
	}
}

/**
 * Attempts to start next upload from the queue. If there are no clients in
 * queue, this method does nothing. Otherwise, it attempts to start uploading
 * to the highest-rated client in the queue (first erasing all invalid entries).
 * If something goes wrong during all that, and exception is thrown, the process
 * starts again, discarding obsolete entries and attempting to start upload to
 * next valid entry. This loop goes around until there are no more entries left
 * in the queue, or no exception is thrown anymore.
 */
void ClientList::startNextUpload() {
	logTrace(TRACE_CLIST, "Starting next upload.");

	while (m_queue.size()) {
		// remove obsolete entries
		while (m_queue.size()) {
			if (m_queued.find(m_queue.front()) == m_queued.end()) {
				m_queue.erase(m_queue.begin());
			} else {
				break;
			}
		}
		if (m_queue.size()) try {
			Client *c = m_queue.front();
			if (!c->m_queueInfo) {
				m_queue.pop_front();
				m_queued.erase(c);
				continue;
			}
			m_queued.erase(m_queue.front());
			m_queue.pop_front();
			logTrace(TRACE_CLIST,
				 boost::format(
					"[%s] Starting upload to client with "
					"highest score (%d)"
				) % c->getIpPort() % c->getScore()
			);
			c->startUpload();
			m_uploading.insert(c);
			if (m_uploading.size() >= getSlotCount()) {
				break;
			}
		} catch (std::exception &er) {
			logDebug(
				boost::format("Error starting upload: %s")
				% er.what()
			);
		}
	}
}

void ClientList::onClientListEvent(ClientList *, ClientListEvt evt) {
	if (evt == EVT_REGEN_QUEUE) {
		updateQueue();
	}
}

bool scoreComp(const Client *const x, const Client *const y) {
	return x->getScore() > y->getScore();
}


/**
 * Regenerate the upload queue, sorting and setting queue ranks as neccesery.
 * This method is called from event table every QUEUE_UPDATE_TIME interval.
 * The delayed event is also posted from this method.
 *
 * Three operations need to be performed within this function:
 * - Build up m_queue list from items in m_queued. Only use clients which have
 *   m_queueInfo member, since we need that member's presence later on. Note
 *   that the existing contents of m_queue list must be discarded prior to this
 *   operation.
 * - Sort the newly generate queue, based on the client's queue scores. The
 * - Now that we have a sorted queue, we must iterate over the entire queue once
 *   more, and set each client's queue ranking to reflect it's position in the
 *   sorted queue. The queue ranking is needed later on by the client itself
 *   to send the queue ranking to the remote client on request.
 */
void ClientList::updateQueue() {
	typedef std::set<Client*>::iterator QIter;
	typedef std::list<Client*>::iterator QLIter;

	Utils::StopWatch s;
	uint64_t curTick = Utils::getTick();   // cache current tick count
	uint32_t dropped = 0;                  // num dropped entries
	m_queue.clear();

	for (QIter i = m_queued.begin(); i != m_queued.end(); ++i) {
		Detail::QueueInfoPtr q = (*i)->m_queueInfo;
		if (q && q->getLastQueueReask() + QUEUE_DROPTIME > curTick) {
			m_queue.push_back(*i);
		} else if (q && !(*i)->m_sourceInfo) {
			// only drop clients here when they aren't sources
			(*i)->removeFromQueue();
			++dropped;
		}
	}
	m_queue.sort(&scoreComp);

	logTrace(TRACE_CLIST,
		boost::format(
			"Queue update: %d clients queued, %d dropped, took %dms"
		) % m_queue.size() % dropped % s
	);
	if (m_queue.size()) {
		logTrace(TRACE_CLIST,
			boost::format(
				"Highest score is %d (%s), "
				"lowest score is %d (%s)"
			) % m_queue.front()->getScore()
			% m_queue.front()->getIpPort()
			% m_queue.back()->getScore()
			% m_queue.back()->getIpPort()
		);
	}

	uint32_t cnt = 1;
	for (QLIter i = m_queue.begin(); i != m_queue.end(); ++i, ++cnt) {
		(*i)->m_queueInfo->setQR(cnt);
	}

	getEventTable().postEvent(this, EVT_REGEN_QUEUE, QUEUE_UPDATE_TIME);

	// See if we need more slots
	checkOpenMoreSlots();
}

/**
 * Calculates last 10 seconds' average uploadrate, and if that is less than 90%
 * of allowed limit, opens up another upload slot.
 *
 * Note that we check & verify against first call to this method using a
 * static boolean. Problem is, during first call, we cannot accurately calculate
 * last 10 second's avg speed, since lastSent isn't initialized yet. Thus this
 * hack. Alternatively, we could ofcourse move lastSent outside this method,
 * and initialize it elsewhere, but I like keeping methods self-contained :)
 */
void ClientList::checkOpenMoreSlots() {
	using Utils::bytesToString;
	static uint32_t lastSent = SchedBase::instance().getTotalUpstream();
	static uint32_t lastRecv = SchedBase::instance().getTotalDownstream();
	static uint64_t lastSentTime = Utils::getTick();
	static uint64_t lastRecvTime = Utils::getTick();
	static bool firstCall = true;
	static uint64_t lastOverheadDn = ED2KPacket::getOverheadDn();
	static uint64_t lastOverheadUp = ED2KPacket::getOverheadUp();

	if (firstCall) {
		firstCall = false;
		return;
	}

	uint64_t curTick = Utils::getTick();

	// do calculation with floating-point precision, and then convert the
	// final result to uint32_t, to get highest reasonable precision.
	uint32_t upDiff = SchedBase::instance().getTotalUpstream() - lastSent;
	uint32_t dnDiff = SchedBase::instance().getTotalDownstream() - lastRecv;
	float upTimeDiff = (curTick - lastSentTime) / 1000.0f;
	float dnTimeDiff = (curTick - lastRecvTime) / 1000.0f;
	uint32_t avgu = static_cast<uint32_t>(upDiff / upTimeDiff);
	uint32_t avgd = static_cast<uint32_t>(dnDiff / dnTimeDiff);
	uint32_t conns = SchedBase::instance().getConnCount();
	uint32_t overheadDn = (ED2KPacket::getOverheadDn()-lastOverheadDn) / 10;
	uint32_t overheadUp = (ED2KPacket::getOverheadUp()-lastOverheadUp) / 10;
	if (overheadUp > avgu) {
		overheadUp = avgu;
	}
	if (overheadDn > avgd) {
		overheadDn = avgd;
	}

	boost::format fmt(
		"[Statistics]"
		" Sources: "  COL_GREEN  "%4d"   COL_NONE
		" | Queued: " COL_YELLOW "%4d"   COL_NONE
		" | Up: "     COL_BCYAN  "%9s/s" COL_NONE
		" | Down: "   COL_BGREEN "%9s/s"  COL_NONE
	);

	fmt % SourceInfo::count() % m_queue.size();
	fmt % bytesToString(avgu) % bytesToString(avgd);

	logMsg(fmt);

	// Write to statistics log
	// 1. source count
	// 2. queue size
	// 3. upload speed
	// 4. download speed
	// 5. connection count
	// 6. overhead up
	// 7. overhead down
	// 8. num uploading clients
	// 9. num downloading clients
	boost::filesystem::path p(ED2K::instance().getConfigDir());
	p /= "../statistics.log";
	std::ofstream ofs(p.native_file_string().c_str(), std::ios::app);
	ofs << curTick << " [ED2KStatistics] ";
	ofs << SourceInfo::count() << ":" << m_queue.size() << ":";
	ofs << (avgu - overheadUp) << ":" << (avgd - overheadDn) << ":";
	ofs << conns << ":" << overheadUp << ":" << overheadDn << ":";
	ClientManager::UploadingClients uc(
		ClientManager::instance().getUploading()
	);
	ClientManager::DownloadingClients dc(
		ClientManager::instance().getDownloading()
	);
	ofs << std::distance(uc.first, uc.second) << ":";
	ofs << std::distance(dc.first, dc.second) << std::endl;

	// conditions for opening new slots
	uint32_t curUpSpeed = SchedBase::instance().getUpSpeed();
	bool openSlot = m_queue.size();
	openSlot &= curUpSpeed < SchedBase::instance().getUpLimit() * .9;
	openSlot &= avgu < SchedBase::instance().getUpLimit() * .85;

//	if (openSlot) {
//		startNextUpload();
//	}

	lastSent = SchedBase::instance().getTotalUpstream();
	lastRecv = SchedBase::instance().getTotalDownstream();
	lastSentTime = curTick;
	lastRecvTime = curTick;
	lastOverheadDn = ED2KPacket::getOverheadDn();
	lastOverheadUp = ED2KPacket::getOverheadUp();
}

bool ClientList::addSource(
	const Hash<ED2KHash> &h, IPV4Address caddr,
	IPV4Address saddr, bool doConn
) try {
	if (caddr.getIp() == ED2K::instance().getId()) {
		if (caddr.getPort() == ED2K::instance().getTcpPort()) {
			logDebug("Cowardly refusing to talk to myself.");
			return false;
		}
	}

	Client *c = findClient(caddr);
	Download *d = DownloadList::instance().find(h);
	if (c && d) {
		c->addOffered(d, doConn);
		c->setServerAddr(saddr);
		return false;
	} else if (d) {
		c = new Client(caddr, d);
		c->setServerAddr(saddr);
		m_clients->insert(c);
		if (doConn) {
			c->establishConnection();
		} else {
			// delay connection a bit
			Client::getEventTable().postEvent(
				c, EVT_REASKFILEPING, 60*1000
			);
		}
		return true;
	} else {
		return false;
	}
} catch (std::exception &e) {
	logDebug(boost::format("Error adding source: %s") % e.what());
	return false;
}
MSVC_ONLY(;)

Client* ClientList::findClient(IPV4Address addr) {
	uint32_t id = addr.getIp();
	IDMap &list = m_clients->get<ID_Id>();
	std::pair<IIter, IIter> ret = list.equal_range(id);
	for (IIter i = ret.first; i != ret.second; ++i) {
		CHECK_FAIL((*i)->getId() == addr.getIp());
		if ((*i)->getTcpPort() == addr.getPort()) {
			return *i;
		}
	}
	return 0;
}

Client* ClientList::findClientByUdp(IPV4Address addr) {
	uint32_t id = addr.getIp();
	IDMap &list = m_clients->get<ID_Id>();
	std::pair<IIter, IIter> ret = list.equal_range(id);
	for (IIter i = ret.first; i != ret.second; ++i) {
		CHECK_FAIL((*i)->getId() == addr.getIp());
		if ((*i)->getUdpPort() == addr.getPort()) {
			return *i;
		}
	}
	return 0;
}

void ClientList::addClient(IPV4Address addr) {
	if (addr.getIp() == ED2K::instance().getId()) {
		if (addr.getPort() == ED2K::instance().getTcpPort()) {
			logDebug("Cowardly refusing to talk to myself.");
			return;
		}
	}

	Client *c = findClient(addr);
	if (c) {
		c->establishConnection();
	} else {
		ED2KClientSocket *sock = new ED2KClientSocket();
		sock->connect(addr, CONNECT_TIMEOUT);
		if (sock->isConnecting()) {
			c = new Client(sock);
			m_clients->insert(c);
		} else {
			delete sock;
		}
	}
}

void ClientList::onUdpData(ED2KUDPSocket *src, SocketEvent evt) try {
	CHECK(src == Client::getUdpSocket());
	if (evt != SOCK_READ) {
		return;
	}

	IPV4Address from;
	uint32_t amount = src->recv(m_udpBuffer.get(), UDP_BUFSIZE, &from);
	std::string buf(m_udpBuffer.get(), amount);

	if (buf.size() < 2) {
		if (src->hasData()) {
			onUdpData(src, evt);
		}
		return;
	}

	Client *c = 0;
	std::istringstream packet(buf.substr(2));
	uint8_t prot = static_cast<uint8_t>(buf[0]);

	if (prot != PR_EMULE && prot != PR_ED2K) {
		logDebug(
			boost::format("Received unknown UDP packet: %s")
			% Utils::hexDump(buf)
		);
		if (src->hasData()) {
			onUdpData(src, evt);
		}
		return;
	}

	try {
	switch (static_cast<uint8_t>(buf[1])) {
		case OP_REASKFILEPING: {
			ED2KPacket::ReaskFilePing p(packet);
			if ((c = findClientByUdp(from))) {
				c->onPacket(p);
			}
			break;
		}
		case OP_QUEUEFULL:
			if ((c = findClientByUdp(from))) {
				ED2KPacket::QueueFull p(packet);
				c->onPacket(p);
			}
			break;
		case OP_FILENOTFOUND:
			if ((c = findClientByUdp(from))) {
				ED2KPacket::FileNotFound p(packet);
				c->onPacket(p);
			}
			break;
		case OP_REASKACK:
			if ((c = findClientByUdp(from))) {
				ED2KPacket::ReaskAck p(packet);
				c->onPacket(p);
			}
			break;
		case OP_GLOBFOUNDSOURCES:
			ServerList::instance().handleGlobSources(packet, from);
			break;
		case OP_GLOBSTATRES:
			ServerList::instance().handleGlobStatRes(packet, from);
			break;
		case OP_GLOBSEARCHRES:
			ServerList::instance().handleGlobSearchRes(
				packet, from
			);
			break;
		default:
			logDebug(
				boost::format(
					"Received unknown UDP packet: "
					"protocol=%s opcode=%s %s"
				) % Utils::hexDump(buf[0])
				% Utils::hexDump(buf[1]) % Utils::hexDump(buf)
			);
			break;
	} // switch
	} catch (std::exception &e) {
		logDebug(
			boost::format(
				"While parsing/handling ClientUDP packet: %s"
			) % e.what()
		);
		logDebug(
			boost::format("Packet contents: %s") 
			% Utils::hexDump(buf)
		);
	}
	if (src->hasData()) {
		onUdpData(src, evt);
	}
} catch (std::runtime_error &e) {
	logDebug(
		boost::format("While parsing/handling ClientUDP packet: %s")
		% e.what()
	);
	if (src->hasData()) {
		onUdpData(src, evt);
	}
} MSVC_ONLY(;)

bool ClientList::valid(Client *c) {
	CIter it = m_clients->get<ID_Client>().find(c);
	return it != m_clients->get<ID_Client>().end();
}

bool ClientList::msgFilter(const std::string &msg) const {
	std::vector<std::string>::const_iterator it = m_msgFilter.begin();
	while (it != m_msgFilter.end()) {
		if (msg.find(*it++) != std::string::npos) {
			return true;
		}
	}
	return false;
}

uint32_t ClientList::getSlotCount() const {
	uint32_t check = SchedBase::instance().getUpLimit();
	if (ED2K::instance().getUpLimit()) {
		check = ED2K::instance().getUpLimit();
	}
	return (check / 1024 / 4) + 1; // 4kb/s per slot + 1 slot
}

void ClientList::onSFEvent(SharedFile *sf, int evt) {
	if (evt != SF_DESTROY) {
		return;
	}
	for (CIter i = m_clients->begin(); i != m_clients->end(); ++i) {
		if ((*i)->m_queueInfo) {
			if ((*i)->m_queueInfo->getReqFile() == sf) {
				(*i)->m_queueInfo.reset();
			}
		}
		if ((*i)->m_uploadInfo) {
			if ((*i)->m_uploadInfo->getReqFile() == sf) {
				(*i)->m_uploadInfo.reset();
			}
		}
		(*i)->checkDestroy();
	}
}

struct ReaskComp {
	bool operator()(const Client *const c1, const Client *const c2) {
		return c1->getLastReaskTime() < c2->getLastReaskTime();
	}
};

void ClientList::printDeadClients() {
	std::vector<Client*> deadClients;
	uint64_t curTick = EventMain::instance().getTick();
	for (CIter i = m_clients->begin(); i != m_clients->end(); ++i) {
		if ((*i)->m_lastReaskTime) {
			if ((*i)->m_lastReaskTime + 60*60*1000 < curTick) {
				deadClients.push_back(*i);
			}
		}
	}
	std::sort(deadClients.begin(), deadClients.end(), ReaskComp());
	if (deadClients.size()) {
		logMsg(
			boost::format("There are %d potentially dead clients.") 
			% deadClients.size()
		);
	}
	size_t cnt = deadClients.size() > 20 ? 20 : deadClients.size();
	for (size_t i = 0; i < cnt; ++i) {
		logMsg(
			boost::format("[%s] Last asked %s ago.") 
			% deadClients[i]->getIpPort() 
			% Utils::secondsToString(
				(curTick - deadClients[i]->m_lastReaskTime) 
				/ 1000
			)
		);
	}
}

void ClientList::configChanged(const std::string &key, const std::string &value)
try {
	if (key == "ed2k/TCP Port") {
		uint16_t newPort = boost::lexical_cast<uint16_t>(value);
		ED2KServerSocket *newListener = 0;
		if (newPort != m_listener->getAddr().getPort()) try {
			newListener = new ED2KServerSocket;
			newListener->listen(IPV4Address(0, newPort));
			delete m_listener;
			m_listener = newListener;
			m_listener->setHandler(this,&ClientList::onServerEvent);
			ED2K::instance().setTcpPort(newPort);
			logMsg(
				boost::format(
					"eDonkey2000: TCP listener restarted "
					"on port %d."
				) % newPort
			);
			// force reconnect to check ID et al
			// this assumes connect() doesn't throw
			ServerList::instance().connect();
		} catch (std::exception&) {
			delete newListener;
			throw;
		}
	} else if (key == "ed2k/UDP Port") {
		uint16_t newPort = boost::lexical_cast<uint16_t>(value);
		ED2KUDPSocket *listener = Client::getUdpSocket();
		ED2KUDPSocket *newListener = 0;
		if (newPort != listener->getAddr().getPort()) try {
			newListener = new ED2KUDPSocket;
			newListener->listen(IPV4Address(0, newPort));
			listener->destroy();
			Client::setUdpSocket(newListener);
			ED2K::instance().setUdpPort(newPort);
			newListener->setHandler(
				boost::bind(
					&ClientList::onUdpData, this, _1, _2
				)
			);
			logMsg(
				boost::format(
					"eDonkey2000: UDP listener restarted "
					"on port %d."
				) % newPort
			);
		} catch (std::exception&) {
			if (newListener) {
				newListener->destroy();
			}
			throw;
		}
	}
} catch (std::exception &e) {
	logError(
		boost::format(
			"[eDonkey2000] Trying to change %s port to %s: %s"
		) % (key == "ed2k/TCP Port" ? "TCP" : "UDP") % value % e.what()
	);
	// we must revert config to the old (working) port here, since we can't
	// safely install a valueChanging handler to veto the port change
	// (starting listeners on same port twice within same event loop can't
	// be done due to delayed deletion of sockets)
	Prefs::instance().write(
		"/ed2k/TCP Port", m_listener->getAddr().getPort()
	);
	Prefs::instance().write(
		"/ed2k/UDP Port", Client::getUdpSocket()->getAddr().getPort()
	);
}

} // end namespace Donkey
