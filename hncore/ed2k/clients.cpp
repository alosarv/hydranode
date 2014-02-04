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
 * \file clients.cpp Implementation of clients-related classes
 */

#include <hncore/ed2k/clients.h>
#include <hncore/ed2k/ed2k.h>
#include <hncore/ed2k/serverlist.h>
#include <hncore/ed2k/creditsdb.h>
#include <hncore/ed2k/parser.h>
#include <hncore/ed2k/clientext.h>
#include <hnbase/lambda_placeholders.h>
#include <hnbase/timed_callback.h>
#include <hnbase/ssocket.h>
#include <hnbase/prefs.h>
#include <hncore/metadb.h>
#include <hncore/sharedfile.h>
#include <hncore/partdata.h>
#include <hncore/fileslist.h>
#include <hncore/hydranode.h>
#include <hncore/hashsetmaker.h>        // for ed2khashmaker
#include <boost/lambda/bind.hpp>
#include <boost/lambda/construct.hpp>

namespace Donkey {

/**
 * Here's a collection of things that can be used to tweak the ed2k inter-client
 * communication procedures. Don't change these unless you know what you'r
 * doing. Incorrect values here can seriously affect the modules' networking
 * performance.
 */
enum Ed2k_ClientConstants {
	/**
	 * Time between source queue ranking reasks. Ed2K netiquette says that
	 * you shouldn't do reasks more often than once per 30 minutes. Major
	 * clients drop queued clients from uploadqueue when no reask has been
	 * done during 1 hour. UDP reasks should be used when possible, falling
	 * back to TCP only if neccesery.
	 */
	SOURCE_REASKTIME = 30*60*1000,

	/**
	 * UDP reask timeout. How long to wait for response after sending UDP
	 * ReaskFilePing. If two UDP reasks fail, we attempt TCP reask, if that
	 * also fails, drop the client as dead.
	 */
	UDP_TIMEOUT = 30000,

	/**
	 * Specifies the TCP stream socket timeout; when there's no activity
	 * in the socket for this amount of time, the socket connection is
	 * dropped.
	 */
	SOCKET_TIMEOUT = 10000,

	/**
	 * Specifices LowID callback request timeout; if the called client
	 * doesn't respond within this timeframe, the client is considered dead.
	 *
	 * lugdunum servers can delay LowId callbacks up to 12 seconds
	 * internally, in order to group several callback requests, so values
	 * lower than 15 seconds are not recommended.
	 */
	CALLBACK_TIMEOUT = 60000,

	/**
	 * Specifies the TCP connection attempt timeout.
	 */
	CONNECT_TIMEOUT = 5000
};

//! Client trace mask
const std::string TRACE_CLIENT = "ed2k.client";
const std::string TRACE_SECIDENT = "ed2k.secident";
const std::string TRACE_DEADSRC = "ed2k.deadsource";
const std::string TRACE_SRCEXCH = "ed2k.sourceexchange";

//! UDP Socket for performing Client <-> Client UDP communication
ED2KUDPSocket *s_clientUdpSocket = 0;
ED2KUDPSocket* Client::getUdpSocket() {
	return s_clientUdpSocket;
}
// we allow rewriting existing udp socket as well, e.g. during runtime config
// changes
void Client::setUdpSocket(ED2KUDPSocket *sock) {
	s_clientUdpSocket = sock;
}

namespace Detail {
	boost::signal<void (Client*, uint32_t)> changeId;
	boost::signal<
		bool (const Hash<ED2KHash>&, IPV4Address, IPV4Address, bool)
	> foundSource;
	boost::signal<void (IPV4Address)> foundServer;
	boost::signal<bool (const std::string&)> checkMsgFilter;
}

// Client class
// ------------
IMPLEMENT_EVENT_TABLE(Client, Client*, ClientEvent);

// Packet handlers declarations
// get-to-know-you chit-chat
DECLARE_PACKET_HANDLER(Client, Hello          );
DECLARE_PACKET_HANDLER(Client, HelloAnswer    );
DECLARE_PACKET_HANDLER(Client, MuleInfo       );
DECLARE_PACKET_HANDLER(Client, MuleInfoAnswer );
// uploading/downloading
DECLARE_PACKET_HANDLER(Client, ReqFile        );
DECLARE_PACKET_HANDLER(Client, SetReqFileId   );
DECLARE_PACKET_HANDLER(Client, ReqHashSet     );
DECLARE_PACKET_HANDLER(Client, StartUploadReq );
DECLARE_PACKET_HANDLER(Client, ReqChunks      );
DECLARE_PACKET_HANDLER(Client, CancelTransfer );
DECLARE_PACKET_HANDLER(Client, FileName       );
DECLARE_PACKET_HANDLER(Client, FileDesc       );
DECLARE_PACKET_HANDLER(Client, FileStatus     );
DECLARE_PACKET_HANDLER(Client, NoFile         );
DECLARE_PACKET_HANDLER(Client, HashSet        );
DECLARE_PACKET_HANDLER(Client, AcceptUploadReq);
DECLARE_PACKET_HANDLER(Client, QueueRanking   );
DECLARE_PACKET_HANDLER(Client, MuleQueueRank  );
DECLARE_PACKET_HANDLER(Client, DataChunk      );
DECLARE_PACKET_HANDLER(Client, PackedChunk    );
// source exchange
DECLARE_PACKET_HANDLER(Client, SourceExchReq  );
DECLARE_PACKET_HANDLER(Client, AnswerSources  );
DECLARE_PACKET_HANDLER(Client, AnswerSources2 );
// misc
DECLARE_PACKET_HANDLER(Client, Message        );
DECLARE_PACKET_HANDLER(Client, ChangeId       );
// secident
DECLARE_PACKET_HANDLER(Client, SecIdentState  );
DECLARE_PACKET_HANDLER(Client, PublicKey      );
DECLARE_PACKET_HANDLER(Client, Signature      );

// Constructor
Client::Client(ED2KClientSocket *c) : BaseClient(&ED2K::instance()),
m_id(), m_tcpPort(), m_udpPort(), m_features(), m_clientSoft(), m_sessionUp(),
m_sessionDown(), m_parser(new ED2KParser<Client>(this)), m_socket(c),
m_credits(), m_callbackInProgress(false),m_reaskInProgress(false),
m_failedUdpReasks(), m_lastReaskTime(), m_lastReaskId(), m_sentChallenge(),
m_reqChallenge(), m_upReqInProgress(), m_dnReqInProgress() {
	CHECK_THROW(c);
	CHECK_THROW(c->isConnected() || c->isConnecting());

	m_sessionState.reset(new SessionState);

	m_id      = c->getPeer().getAddr();
	m_tcpPort = c->getPeer().getPort();

	setConnected(true);
	c->setHandler(this, &Client::onSocketEvent);
	m_parser->parse(c->getData());
}

Client::Client(IPV4Address addr, Download *file):BaseClient(&ED2K::instance()),
m_id(addr.getAddr()), m_tcpPort(addr.getPort()), m_udpPort(), m_features(),
m_clientSoft(), m_sessionUp(), m_sessionDown(),
m_parser(new ED2KParser<Client>(this)), m_socket(),m_credits(),
m_callbackInProgress(false), m_reaskInProgress(false), m_failedUdpReasks(),
m_lastReaskTime(), m_lastReaskId(), m_sentChallenge(), m_reqChallenge(),
m_upReqInProgress(), m_dnReqInProgress() {

	addOffered(file, false); // don't connect right away
}

Client::~Client() {
	getEventTable().delHandlers(this);
	delete m_socket;
}

void Client::destroy() {
	getEventTable().postEvent(this, EVT_DESTROY);
	if (m_socket) try {
		m_socket->disconnect();
	} catch (...) {}
	delete m_socket;
	m_socket = 0;
	m_downloadInfo.reset();
	m_queueInfo.reset();
	m_sourceInfo.reset();
	m_uploadInfo.reset();
}

void Client::checkDestroy() {
	if (!m_sourceInfo && !m_downloadInfo && !m_queueInfo && !m_uploadInfo) {
		destroy();
	}
}

std::string Client::getIpPort() const {
	boost::format fmt("%s:%s");
	if (isHighId()) {
		fmt % Socket::getAddr(getId());
	} else {
		fmt % boost::lexical_cast<std::string>(getId());
	}
	fmt % getTcpPort();
	return fmt.str();
}

/**
 * Attempt to establish connection with the remote client. If the remote client
 * is LowID, we will request a callback via server (but only if we are connected
 * to a server, and the remote client is on same server as we are). If the
 * remote client is HighID, we attempt to connect it directly. If anything
 * goes wrong, std::runtime_error will be thrown.
 *
 * If this method is called and socket already exists, it silently returns. The
 * reason for not throwing exceptions on that case is that there's a lot of race
 * conditions floating around regarding re-establishing connections. While some
 * of them are internal, and could (should) be fixed, many of them are inherent
 * from remote client's bad protocol usage, and it is nearly impossible to
 * handle all cases correctly.
 */
void Client::establishConnection() try {
	if (m_socket) {
		return; // silently ignored
	}

	if (isLowId() && ED2K::instance().isLowId()) {
		logTrace(TRACE_DEADSRC,
			boost::format(
				"[%s] Unable to perform LowID <-> "
				"LowID callback."
			) % getIpPort()
		);
		destroy();
	} else if (isHighId()) { // highid
		logTrace(
			TRACE_CLIENT,
			boost::format("[%s] Connecting...") % getIpPort()
		);
		m_socket = new ED2KClientSocket();
		m_socket->setHandler(this, &Client::onSocketEvent);
		IPV4Address addr(m_id, m_tcpPort);
		m_socket->connect(addr, CONNECT_TIMEOUT);
		// avoids race condition when reaskForDownload is called
		// when socket is connected, but SOCK_CONNECTED event hasn't
		// arrived yet.
		m_sessionState.reset(new SessionState);
	} else {
		IPV4Address curServ = ServerList::instance().getCurServerAddr();
		if (m_serverAddr && m_serverAddr != curServ) {
			logTrace(TRACE_DEADSRC,
				boost::format(
					"[%s] We are on server %s, client is "
					"on server %s; dropping."
				) % getIpPort() % curServ % m_serverAddr
			);
			destroy();
			return;
		}
		logTrace(TRACE_CLIENT,
			boost::format("[%s] %p: Performing LowID callback...")
			% getIpPort() % this
		);
		ServerList::instance().reqCallback(m_id);
		getEventTable().postEvent(
			this, EVT_CALLBACK_T, CALLBACK_TIMEOUT
		);
		m_callbackInProgress = true;
	}
} catch (std::exception &e) {
	(void)e;
	logTrace(
		TRACE_DEADSRC, boost::format(
			"[%s] Error performing LowID callback: %s"
		) % getIpPort() % e.what()
	);
	destroy();
}
MSVC_ONLY(;)

// Add an offered file. This is the public accessor and really forwards the
// call to DownloadInfo sub-object (constructing it if neccesery).
// doConn variable allows delaying connection attempt for later
void Client::addOffered(Download *file, bool doConn) {
	if (!m_sourceInfo) {
		logTrace(TRACE_CLIENT,
			boost::format("[%s] %p: Creating new SourceInfo")
			% getIpPort() % this
		);

		m_sourceInfo.reset(new Detail::SourceInfo(this, file));

		if (isConnected() && !m_downloadInfo) {
			reqDownload();
		} else if (!m_socket && doConn) {
			establishConnection();
		}
	} else {
		logTrace(TRACE_CLIENT,
			boost::format("[%s] %p: Adding offered file.")
			% getIpPort() % this
		);

		m_sourceInfo->addOffered(file);
	}
}

void Client::remOffered(Download *file, bool cleanUp) {
	CHECK_THROW(file);

	if (m_sourceInfo) {
		m_sourceInfo->remOffered(file, cleanUp);
		if (!m_sourceInfo->getOffCount()) {
			logTrace(
				TRACE_CLIENT,
				boost::format(
					"[%s] Removed last offered file %s"
				) % getIpPort()
				% file->getPartData()->getDestination().leaf()
			);
			m_sourceInfo.reset();
			m_lastReaskTime = 0;
		} else {
			logTrace(
				TRACE_CLIENT,
				boost::format("[%s] Removed offered file %s")
				% getIpPort()
				% file->getPartData()->getDestination().leaf()
			);
		}
	}
	if (m_downloadInfo) {
		if (m_downloadInfo->getReqPD() == file->getPartData()) {
			m_downloadInfo.reset();
		}
	}
	checkDestroy();
}

// merge all the information that we need from the other client. Keep in mind
// that the other client will be deleted shortly after this function is called
// by clientlist, so we must take everything we need from that client.
void Client::merge(Client *c) {
	CHECK_THROW(c != this);

	if (m_socket && c->m_socket) {
		throw std::runtime_error("Client is already connected!");
	} else if (c->m_socket && !m_socket) {
		m_socket = c->m_socket;
		m_socket->setHandler(this, &Client::onSocketEvent);
		c->m_socket = 0;
		c->setConnected(false);
		m_sessionState = c->m_sessionState;
	}

	m_parser = c->m_parser;
	m_parser->setParent(this);
	if (c->m_queueInfo && !m_queueInfo) {
		m_queueInfo = c->m_queueInfo;
		m_queueInfo->setParent(this);
	}
	if (c->m_sourceInfo && !m_sourceInfo) {
		m_sourceInfo = c->m_sourceInfo;
		m_sourceInfo->setParent(this);
	}
	if (c->m_uploadInfo && !m_uploadInfo) {
		m_uploadInfo = c->m_uploadInfo;
		m_uploadInfo->setParent(this);
	}
	if (c->m_downloadInfo && !m_downloadInfo) {
		m_downloadInfo = c->m_downloadInfo;
		m_downloadInfo->setParent(this);
	}
	if (c->m_hash && !m_hash) {
		m_hash = c->m_hash;
	}
	if (c->m_udpPort && !m_udpPort) {
		m_udpPort = c->m_udpPort;
	}
	if (c->m_pubKey && !m_pubKey) {
		m_pubKey = c->m_pubKey;
	}
	if (c->m_serverAddr && !m_serverAddr) {
		m_serverAddr = c->m_serverAddr;
	}
	if (c->m_nick.size() && !m_nick.size()) {
		m_nick = c->m_nick;
	}
	if (c->m_clientSoft && !m_clientSoft) {
		m_clientSoft = c->m_clientSoft;
	}
	if (c->m_credits && !m_credits) {
		m_credits = c->m_credits;
	}
	if (c->m_lastReaskTime && !m_lastReaskTime) {
		m_lastReaskTime = c->m_lastReaskTime;
	}
	if (c->m_sentChallenge && !m_sentChallenge) {
		m_sentChallenge = c->m_sentChallenge;
	}
	if (c->m_reqChallenge && !m_reqChallenge) {
		m_reqChallenge = c->m_reqChallenge;
	}
	if (m_callbackInProgress) {
		logTrace(TRACE_CLIENT,
			boost::format("[%s] %p: LowID callback succeeded.")
			% getIpPort() % this
		);
		m_callbackInProgress = false;
	}
	if (m_socket && m_socket->isConnected()) {
		setConnected(true);
	}
}

// Event handler for socket events.
// note that due to changeId() signal handlers, we might be deleted when
// returning from parser, so do NOT do anything in this function after returning
// from parse(), since we might have been deleted.
void Client::onSocketEvent(ED2KClientSocket *c, SocketEvent evt) {
	assert(m_socket);
	assert(c == m_socket);

	// 2 minutes timeout when transfer is in progress, since emule often
	// sets clients in "stalled" transfer, however gaining an upload slot
	// in ed2k is so damn time-consuming that we really want to give the
	// remote client more chances to send stuff before we give up on it.
	// 2 minutes timeout when transfer is in progress ought to be enough.
	if (m_uploadInfo || m_downloadInfo) {
		m_socket->setTimeout(120000);
	} else {
		m_socket->setTimeout(SOCKET_TIMEOUT);
	}

	if (evt == SOCK_READ) try {
		m_parser->parse(c->getData());
	} catch (std::exception &er) {
		logTrace(TRACE_DEADSRC,
			boost::format(
				"[%s] Destroying client: error during client "
				"stream parsing/handling: %s"
			) % getIpPort() % er.what()
		);
		destroy();
	} else if (evt == SOCK_WRITE && m_uploadInfo) {
		sendNextChunk();
	} else if (evt == SOCK_CONNECTED) {
		logTrace(TRACE_CLIENT,
			boost::format(
				"[%s] Connection established, sending Hello"
			) % getIpPort()
		);
		*m_socket << ED2KPacket::Hello();
		m_failedUdpReasks = 0;
		m_upReqInProgress = false;
		m_dnReqInProgress = m_sourceInfo ? true : false;
		setConnected(true);
		m_sessionState.reset(new SessionState);
		m_sessionState->m_sentHello = true;
	} else if (evt == SOCK_CONNFAILED) {
		logTrace(
			TRACE_DEADSRC, boost::format(
				"[%s] Dropping client (unable to connect)"
			) % getIpPort()
		);
		destroy();
	} else if (evt == SOCK_TIMEOUT) {
		logTrace(TRACE_CLIENT,
			boost::format("[%s] Connection timed out.")
			% getIpPort()
		);
		onLostConnection();
	} else if (evt == SOCK_LOST) {
		logTrace(TRACE_CLIENT,
			boost::format("[%s] Connection lost.") % getIpPort()
		);
		onLostConnection();
	} else if (evt == SOCK_ERR) {
		logTrace(TRACE_CLIENT,
			boost::format(
				"[%s] Connection lost (socket error)."
			) % getIpPort()
		);
		onLostConnection();
	}
}

void Client::onLostConnection() {
	if (m_socket) {
		m_sessionUp   += m_socket->getUploaded();
		m_sessionDown += m_socket->getDownloaded();
	}
	setConnected(false);
	delete m_socket;
	m_socket = 0;
	m_sentChallenge = 0;
	m_reqChallenge = 0;

	if (!m_queueInfo && m_sourceInfo && !m_lastReaskTime) {
		logTrace(TRACE_DEADSRC,
			boost::format(
				"[%s] Destroying client: Source, but "
				"never connected."
			) % getIpPort()
		);
		destroy();
	} else if (!m_sourceInfo && !m_queueInfo) {
		logTrace(TRACE_DEADSRC,
			boost::format(
				"[%s] Destroying client: TCP connection"
				" lost/failed, and no src/queue info is"
				" available."
			) % getIpPort()
		);
		destroy();
	} else if (m_failedUdpReasks > 2) {
		logTrace(TRACE_DEADSRC,
			boost::format(
				"[%s] Destroying client: 3 UDP Reasks "
				"failed, and TCP Reask also failed."
			) % getIpPort()
		);
		destroy();
	} else if (m_downloadInfo || (m_sourceInfo && m_dnReqInProgress)) {
		// handles two cases: when socket timeouts while downloading,
		// and when download request was sent, but no answer received
		getEventTable().postEvent(
			this, EVT_REASKFILEPING, SOURCE_REASKTIME
		);
		m_lastReaskTime = EventMain::instance().getTick();
	} else if (m_uploadInfo) {
		if (!m_queueInfo) {
			m_queueInfo.reset(
				new Detail::QueueInfo(this, m_uploadInfo)
			);
		}
		getEventTable().postEvent(this, EVT_UPLOADREQ);
	} else if (
		m_sourceInfo && m_sessionState && !m_sessionState->m_sentReq
		&& !m_queueInfo
	) {
		logTrace(TRACE_DEADSRC,
			 boost::format(
				"[%s] Destroying client: is source, but file "
				"request could not be sent this session."
			) % getIpPort()
		);
		destroy();
	} else if (m_sessionState && !m_sessionState->m_gotHello) {
		logTrace(TRACE_DEADSRC,
			boost::format(
				"[%s] Destroying client: connection lost "
				"before completing handshake."
			) % getIpPort()
		);
		destroy();
	}

	m_upReqInProgress = false;
	m_dnReqInProgress = false;
	m_downloadInfo.reset();
	m_uploadInfo.reset();
	m_sessionState.reset();
}

// Get to know you chit chat
// -------------------------
// Before we can do anything other useful things with the remote client, we
// must first get to know him/her and also introduce ourselves. We do it by
// saying Hello, and expecting HelloAnswer. Alternatively, if he/she said
// Hello to us, we politely respond with HelloAnswer.
//
// There are rumors about some old mules walking around the network, using some
// odd mule-language. Newer-generation mules have learned english, and no longer
// require the usage of mule-language, however, for old-generation mules, we
// must speak their language, and thus also say MuleInfo and/or MuleInfoAnswer.
//

// Stores client info found in packet internally. This is used as helper
// method by Hello/HelloAnswer packet handler
void Client::storeInfo(const ED2KPacket::Hello &p) {
	m_tcpPort    = p.getClientAddr().getPort();
	m_udpPort    = p.getUdpPort();
	m_features   = p.getFeatures();
	m_hash       = p.getHash();
	m_serverAddr = p.getServerAddr();
	m_nick       = p.getNick();

	if (getClientSoft() != CS_MLDONKEY_NEW2) {
		// new mldonkeys send muleinfo with mldonkey info, and THEN
		// hello with ID 0x00 (emule) - this check detects it.
		m_clientSoft = p.getMuleVer();
	}
	if (!m_clientSoft) {
		m_clientSoft |= p.getVersion() << 24;
	}
	logTrace(TRACE_CLIENT,
		boost::format(
			"[%s] (Hello) ClientSoftware is " COL_BBLUE
			"%s" COL_BGREEN " %s" COL_NONE
		) % getIpPort() % getSoft() % getSoftVersion()
	);
	logTrace(TRACE_CLIENT,
		boost::format("[%s] (Hello) Nick: %s Userhash: %s UDPPort: %d")
		% getIpPort() % m_nick % m_hash.decode() % m_udpPort
	);
	std::string msg("[" + getIpPort() + "] (Hello) Features: ");
	if (supportsPreview())     msg += "Preview ";
	if (supportsMultiPacket()) msg += "MultiPacket ";
	if (supportsViewShared())  msg += "ViewShared ";
	if (supportsPeerCache())   msg += "PeerCache ";
	if (supportsUnicode())     msg += "Unicode ";
	if (getCommentVer()) {
		msg += (boost::format("Commentv%d ")
		% static_cast<int>(getCommentVer())).str();
	}
	if (getExtReqVer()) {
		msg += (boost::format("ExtReqv%d ")
		% static_cast<int>(getExtReqVer())).str();
	}
	if (getSrcExchVer()) {
		msg += (boost::format("SrcExchv%d ")
		% static_cast<int>(getSrcExchVer())).str();
	}
	if (getSecIdentVer()) {
		msg += (boost::format("SecIdentv%d ")
		% static_cast<int>(getSecIdentVer())).str();
	}
	if (getComprVer()) {
		msg += (boost::format("Comprv%d ")
		% static_cast<int>(getComprVer())).str();
	}
	if (getUdpVer()) {
		msg += (boost::format("Udpv%d ")
		% static_cast<int>(getUdpVer())).str();
	}
	if (getAICHVer()) {
		msg += (boost::format("AICHv%d ")
		% static_cast<int>(getAICHVer())).str();
	}
	logTrace(TRACE_CLIENT, msg);

	Detail::changeId(this, p.getClientAddr().getAddr());
}

std::string Client::getSoft() const {
	switch (m_clientSoft >> 24) {
		case CS_EMULE:         return "eMule";
		case CS_CDONKEY:       return "cDonkey";
		case CS_LXMULE:        return "(l/x)mule";
		case CS_AMULE:         return "aMule";
		case CS_SHAREAZA:
		case CS_SHAREAZA_NEW:  return "Shareaza";
		case CS_EMULEPLUS:     return "eMulePlus";
		case CS_HYDRANODE:     return "Hydranode";
		case CS_MLDONKEY_NEW2: return "MLDonkey";
		case CS_LPHANT:        return "lphant";
		case CS_HYBRID:        return "eDonkeyHybrid";
		case CS_DONKEY:        return "eDonkey";
		case CS_MLDONKEY:      return "OldMLDonkey";
		case CS_OLDEMULE:      return "OldeMule";
		case CS_MLDONKEY_NEW:  return "MLDonkey";
		case CS_UNKNOWN:
		default: return (
			boost::format("Unknown %s")
			% Utils::hexDump(m_clientSoft >> 24)
		).str();
	}
}

std::string Client::getSoftVersion() const {
	std::string ret;
	if (getClientSoft() == CS_EMULE) {
		ret += "0.";
		ret += boost::lexical_cast<std::string>(getVerMin());
		ret += static_cast<uint8_t>(getVerPch() + 0x61);
	} else {
		boost::format fmt("%d.%d.%d-%d");
		fmt % getVerMjr() % getVerMin() % getVerPch() % getVerBld();
		ret += fmt.str();
	}
	return ret;
}

// base class virtuals
// -------------------

IPV4Address Client::getAddr() const {
	if (m_socket) {
		return m_socket->getPeer();
	} else if (isHighId()) {
		return IPV4Address(m_id, m_tcpPort);
	} else {
		return IPV4Address();
	}
}

std::string Client::getNick() const {
	return m_nick;
}

uint64_t Client::getSessionUploaded() const {
	if (m_socket) {
		return m_sessionUp + m_socket->getUploaded();
	} else {
		return m_sessionUp;
	}
}

uint64_t Client::getSessionDownloaded() const {
	if (m_socket) {
		return m_sessionDown + m_socket->getDownloaded();
	} else {
		return m_sessionDown;
	}
}

uint64_t Client::getTotalUploaded() const {
	return m_credits ? m_credits->getUploaded() : m_sessionUp;
}
uint64_t Client::getTotalDownloaded() const {
	return m_credits ? m_credits->getDownloaded() : m_sessionDown;
}
uint32_t Client::getUploadSpeed() const {
	return m_socket ? m_socket->getUpSpeed() : 0;
}
uint32_t Client::getDownloadSpeed() const {
	return m_socket ? m_socket->getDownSpeed() : 0;
}
uint32_t Client::getQueueRanking() const {
	return m_queueInfo ? m_queueInfo->getQR() : 0;
}
uint32_t Client::getRemoteQR() const {
	return m_sourceInfo ? m_sourceInfo->getQR() : 0;
}

bool Client::isMule() const {
	if (getClientSoft() == CS_EMULE && m_hash) {
		return m_hash.getData()[5] == 14 && m_hash.getData()[14] == 111;
	} else {
		return false;
	}
}

// Hi there little one
void Client::onPacket(const ED2KPacket::Hello &p) {
	CHECK_THROW(m_socket);

	*m_socket << ED2KPacket::HelloAnswer();

	if (isMule() && getVerMin() < 43) {
		logTrace(TRACE_CLIENT, "Old eMule detected, sending MuleInfo.");
		// eMule (old) extended protocol - also send MuleInfo
		*m_socket << ED2KPacket::MuleInfo();
	}
	CHECK_THROW(m_sessionState);
	m_sessionState->m_gotHello = true;
	m_sessionState->m_sentHello = true;
	storeInfo(p);
}

// Nice to meet you too
void Client::onPacket(const ED2KPacket::HelloAnswer &p) {
	logTrace(TRACE_CLIENT,
		boost::format("[%s] Received HelloAnswer.") % getIpPort()
	);
	CHECK_THROW(m_sessionState);
	m_sessionState->m_gotHello = true;
	storeInfo(p);
}

void Client::onPacket(const ED2KPacket::MuleInfo &p) {
	logTrace(TRACE_CLIENT,
		boost::format("[%s] Received MuleInfo.") % getIpPort()
	);
	processMuleInfo(p);
	CHECK_THROW(m_socket);
	*m_socket << ED2KPacket::MuleInfoAnswer();
}

// the old extinct mule language
void Client::onPacket(const ED2KPacket::MuleInfoAnswer &p) {
	logTrace(TRACE_CLIENT,
		boost::format("[%s] Received MuleInfoAnswer") % getIpPort()
	);
	processMuleInfo(p);
}

void Client::processMuleInfo(const ED2KPacket::MuleInfo &p) {
	m_clientSoft |= p.getCompatCliID()   << 24; // compat client id
	m_clientSoft |= (p.getVersion() + 8) << 10; // minor version, in hex

	logTrace(TRACE_CLIENT,
		boost::format("[%s] %s %s using old eMule protocol.")
		% getIpPort() % getSoft() % getSoftVersion()
	);

	if (isMule() && getVerMin() < 42) {
		handshakeCompleted();
	}
}

// Upload requests
// ---------------
// Here we go again. Just as we arrived on the net, ppl start wanting something
// from us. Can't they just leave us alone and stop wanting every last bit of
// our preciousssss files? *sigh*
//
// Well, here goes.

// He/she wants a file. "Ask, and thou shall receive."
void Client::onPacket(const ED2KPacket::ReqFile &p) {
	CHECK_THROW(isConnected());
	using boost::signals::connection;

	logTrace(TRACE_CLIENT,
		boost::format("[%s] Received ReqFile for %s")
		% getIpPort() % p.getHash().decode()
	);
	SharedFile *sf = MetaDb::instance().findSharedFile(p.getHash());
	if (sf) {
		*m_socket << ED2KPacket::FileName(p.getHash(), sf->getName());
		m_upReqInProgress = true;
		if (sf->isPartial() && !m_sourceInfo) {
			logTrace(TRACE_CLIENT,
				boost::format(
					"[%s] Passivly adding source "
					"and sending ReqFile."
				) % getIpPort()
			);
			Download *file = 0;
			file = DownloadList::instance().find(p.getHash());
			CHECK_THROW(file);
			m_sourceInfo.reset(new Detail::SourceInfo(this, file));
			reqDownload();
		}
	} else {
		*m_socket << ED2KPacket::NoFile(p.getHash());
		logTrace(TRACE_CLIENT,
			boost::format(
				"[%s] Sending NoFile for hash %s"
			) % getIpPort() % p.getHash().decode()
		);
	}
}

// Seems he/she is confident in his/her wishes. Well, can't argue there.
// Confidence is a virtue :)
void Client::onPacket(const ED2KPacket::SetReqFileId &p) {
	CHECK_THROW(isConnected());

	SharedFile *sf = MetaDb::instance().findSharedFile(p.getHash());
	if (sf) {
		logTrace(TRACE_CLIENT,
			boost::format("[%s] Received SetReqFileId for %s")
			% getIpPort() % sf->getName()
		);
		*m_socket << ED2KPacket::FileStatus(
			p.getHash(), sf->getPartData()
		);
		if (m_uploadInfo) {
			if (m_uploadInfo->getReqChunkCount()) {
				logTrace(TRACE_CLIENT,
					boost::format(
						"[%s] Cannot SetReqFileId "
						"after ReqChunks!"
					) % getIpPort()
				);
				return;
			} else {
				m_uploadInfo->setReqFile(sf);
			}
		} else if (m_queueInfo) {
			m_queueInfo->setReqFile(sf, p.getHash());
		} else {
			m_queueInfo.reset(
				new Detail::QueueInfo(this, sf, p.getHash())
			);
			m_queueInfo->m_lastQueueReask = Utils::getTick();
			m_uploadInfo.reset();
			getEventTable().postEvent(this, EVT_UPLOADREQ);
		}
	} else {
		*m_socket << ED2KPacket::NoFile(p.getHash());
		logTrace(TRACE_CLIENT,
			boost::format(
				"[%s] Received request for unknown file %s"
			) % getIpPort() % p.getHash().decode()
		);
	}
}

// So, seems they'r really sure they want this file. Ohwell, let's see what
// we can do. But wait - we can't do anything yet - we need to ask permission
// from our parent first. More on this later...
//
// PS: Rumors say some mules starting with A letter want to start upload before
//     actually saying what they want in setreqfileid. Poor bastards, but
//     we must serve all equally, so try to work around it and grant the request
//     anyway, if we have enough information at this point.
void Client::onPacket(const ED2KPacket::StartUploadReq &p) {
	logTrace(TRACE_CLIENT,
		boost::format("[%s] Received StartUploadReq.") % getIpPort()
	);
	onUploadReq(p.getHash());
}

// handles upload requests
void Client::onUploadReq(Hash<ED2KHash> hash) {
	if (!hash && !m_uploadInfo && !m_queueInfo) {
		return; // not enough info to do anything with this request
	}
	SharedFile *sf = 0;
	if (!hash && m_uploadInfo) {
		hash = m_uploadInfo->getReqHash();
		sf = m_uploadInfo->getReqFile();
	} else if (!hash && m_queueInfo) {
		hash = m_queueInfo->getReqHash();
		sf = m_queueInfo->getReqFile();
	}
	if (hash && !sf) {
		sf = MetaDb::instance().findSharedFile(hash);
	}
	if (!hash || !sf) {
		logTrace(
			TRACE_CLIENT, boost::format(
				"[%s] Received upload request, but for what?"
			) % getIpPort()
		);
		return;
	}

	if (m_uploadInfo) {
		m_uploadInfo->setReqFile(sf);
		m_uploadInfo->setReqHash(hash);
		startUpload();
	} else if (m_queueInfo) {
		m_queueInfo->setReqFile(sf, hash);
		m_queueInfo->m_lastQueueReask = Utils::getTick();
		getEventTable().postEvent(this, EVT_UPLOADREQ);
	} else {
		m_queueInfo.reset(new Detail::QueueInfo(this, sf, hash));
		m_queueInfo->m_lastQueueReask = Utils::getTick();
		getEventTable().postEvent(this, EVT_UPLOADREQ);
	}
}

// So, they want to know the entire hashset of the file? Interesting concept.
// Do we have the hashset? Do WE actually know what we are sharing? Might not
// always be the case, if we don't have the file ourselves yet, or something
// else is wrong. On the other hand, if we have even few bytes of the file,
// and thus are sharing it, then we should have the hashset of it anyway, since
// we ask for a hashset ourselves first time we start a download. So we can
// be pretty sure we know the hashset of the file we'r sharing at this point.
void Client::onPacket(const ED2KPacket::ReqHashSet &p) {
	logTrace(TRACE_CLIENT,
		boost::format("[%s] Received ReqHashSet for %s")
		% getIpPort() % p.getHash().decode()
	);

	MetaData *md = MetaDb::instance().find(p.getHash());
	if (md == 0) {
		return; // ignored
	}
	HashSetBase *hs = 0;
	for (uint32_t i = 0; i < md->getHashSetCount(); ++i) {
		hs = md->getHashSet(i);
		if (hs->getFileHashTypeId() == CGComm::OP_HT_ED2K) {
			break;
		}
	}
	if (hs == 0) {
		return; // ignored
	}
	ED2KHashSet *ehs = dynamic_cast<ED2KHashSet*>(hs);
	if (!ehs) {
		logDebug("Internal type error upcasting HashSetBase.");
		return;
	}
	CHECK_THROW(m_socket);
	*m_socket << ED2KPacket::HashSet(ehs);
}

// Actual uploading
// ----------------
// Well, this is fun. We wait for chunk requests, once those arrive, we send
// those chunks, and so on and so forth, until we run out of chunks. But in
// reality, they never stop sending chunk requests, so at some point we'll have
// to pull the plug. When exactly we do it is up to us - I guess we'll just
// treat all the same and kick 'em after few MB's or so.

// Initialize upload sequence. This is done by sending AcceptUploadReq packet
// to the remote client. If we already have requested chunks list at this point,
// we can start sending data right away. However, some clients seem request
// chunks only AFTER receiving AcceptUploadReq packet, so in that case, the
// data sending is delayed until we receive the chunk request.
void Client::startUpload() {
	if (!m_uploadInfo) {
		m_uploadInfo.reset(new Detail::UploadInfo(this, m_queueInfo));
	}
	if (m_queueInfo) {
		m_queueInfo.reset();
	}
	if (isConnected()) {
		logTrace(TRACE_CLIENT,
			boost::format("[%s] Starting upload.") % getIpPort()
		);
		*m_socket << ED2KPacket::AcceptUploadReq();
		m_uploadInfo->connectSpeeder();
		if (m_uploadInfo->getReqChunkCount()) {
			sendNextChunk();
		} else {
			logTrace(TRACE_CLIENT, boost::format(
				"[%s] Waiting for chunk requests."
			) % getIpPort());
		}
	} else try {
		establishConnection();
	} catch (std::exception &e) {
		(void)e;
		logTrace(TRACE_DEADSRC,
			boost::format("[%s] Unable to connect to client: %s")
			% getIpPort() % e.what()
		);
		destroy();
	}
}

// You are #X on my queue. Please stay calm and wait your turn, it'll come
// (eventually anyway).
void Client::sendQR() {
	CHECK_THROW(isConnected());
	CHECK_THROW(m_queueInfo);
	CHECK_THROW(m_queueInfo->getQR());
	CHECK_THROW(!m_uploadInfo);
	logTrace(TRACE_CLIENT,
		boost::format("[%s] Sending QueueRanking %d.")
		% getIpPort() % m_queueInfo->getQR()
	);
	if (isMule()) {
		*m_socket << ED2KPacket::MuleQueueRank(m_queueInfo->getQR());
	} else {
		*m_socket << ED2KPacket::QueueRanking(m_queueInfo->getQR());
	}
	m_upReqInProgress = false;
}

void Client::disconnect() {
	if (m_socket) {
		m_socket->disconnect();
		delete m_socket;
		m_socket = 0;
	}
	onLostConnection();
}

// More work? work work.
void Client::onPacket(const ED2KPacket::ReqChunks &p) {
	CHECK_THROW(m_uploadInfo);
	CHECK_THROW(isConnected());
	for (uint8_t i = 0; i < p.getReqChunkCount(); ++i) {
		m_uploadInfo->addReqChunk(p.getReqChunk(i));
	}
	sendNextChunk();
}

// Send next requested chunk to client.
//
// We send data in 10k chunks, in the order of which they were requested.
void Client::sendNextChunk() try {
	CHECK_THROW(m_uploadInfo);
	CHECK_THROW(!m_queueInfo);
	CHECK_THROW(isConnected());

	if (!m_uploadInfo->getReqChunkCount()) {
		if (!m_uploadInfo->getSent()) {
			return;
		}
		logTrace(TRACE_CLIENT,
			boost::format("[%s] No more reqchunks.")
			% getIpPort()
		);
		m_uploadInfo.reset();
		getEventTable().postEvent(this, EVT_CANCEL_UPLOADREQ);
		m_queueInfo.reset();
		if (!m_downloadInfo) {
			disconnect();
		}
		if (!m_sourceInfo) {
			logTrace(TRACE_DEADSRC, boost::format(
				"[%s] Destroying client: No more requested "
				"chunks for uploading, and no source "
				"information.") % getIpPort()
			);
			destroy();
		}
		return;
	}

	if (!m_credits && m_pubKey) {
		m_credits = CreditsDb::instance().create(m_pubKey, m_hash);
	}

	logTrace(TRACE_CLIENT,
		boost::format(
			COL_SEND "[%s] Uploading file %s%s, total sent %s"
			COL_NONE
		) % getIpPort() % m_uploadInfo->getReqFile()->getName()
		% (m_uploadInfo->isCompressed()
			? COL_COMP " (compressed)" COL_SEND
			: ""
		) % Utils::bytesToString(m_uploadInfo->getSent())
	);

	if (!m_uploadInfo->hasBuffered()) try {
		m_uploadInfo->bufferData();
//		if (getComprVer()) {
//			m_uploadInfo->compress();
//		}
	} catch (SharedFile::ReadError &e) {
		if (e.reason() == SharedFile::ETRY_AGAIN_LATER) {
			Utils::timedCallback(
				boost::bind(&Client::sendNextChunk, this), 3000
			);
			return;
		} else {
			throw;
		}
	}

	boost::tuple<uint32_t, uint32_t, std::string> nextChunk;
	nextChunk = m_uploadInfo->getNext(10240);

//	if (getComprVer() && m_uploadInfo->isCompressed()) {
//		*m_socket << ED2KPacket::PackedChunk(
//			m_uploadInfo->getReqHash(), nextChunk.get<0>(),
//			nextChunk.get<1>(), nextChunk.get<2>()
//		);
//	} else {
		*m_socket << ED2KPacket::DataChunk(
			m_uploadInfo->getReqHash(), nextChunk.get<0>(),
			nextChunk.get<1>(), nextChunk.get<2>()
		);
//	}

	if (m_credits) {
		m_credits->addUploaded(nextChunk.get<2>().size());
	}
	m_uploadInfo->getReqFile()->addUploaded(nextChunk.get<2>().size());
} catch (std::exception &e) {
	logDebug(
		boost::format("[%s] Sending next chunk to ed2kclient: %s")
		% getIpPort() % e.what()
	);
	if (m_uploadInfo) {
		m_queueInfo.reset(new Detail::QueueInfo(this, m_uploadInfo));
		m_queueInfo->m_lastQueueReask = Utils::getTick();
		m_uploadInfo.reset();
		getEventTable().postEvent(this, EVT_UPLOADREQ);
	} else {
		checkDestroy();
	}

} MSVC_ONLY(;)

// No more? No less. Was a bad file anyway.
void Client::onPacket(const ED2KPacket::CancelTransfer &) {
	logTrace(TRACE_CLIENT,
		boost::format("[%s] Received CancelTransfer.") % getIpPort()
	);
	m_uploadInfo.reset();
	m_queueInfo.reset();
	if (!m_sourceInfo) {
		logTrace(TRACE_DEADSRC, boost::format(
			"[%s] Destroying client: Received CancelTransfer, and "
			"no sourceinfo is available.") % getIpPort()
		);
		destroy();
	} else {
		getEventTable().postEvent(this, EVT_CANCEL_UPLOADREQ);
	}
}

// Downloading
// -----------
// Enough of serving others. Time to get something for ourselves too, right?
// Let's get started right away. Let's see what can they tell us.

// Oh? A filename? Riiight, very useful. Stuff it somewhere and get over it.
void Client::onPacket(const ED2KPacket::FileName &p) {
	logTrace(TRACE_CLIENT,
		boost::format("[%s] Received FileName for hash %s: %s.")
		% getIpPort() % p.getHash().decode() % p.getName()
	);

	if (!m_sourceInfo) {
		Download *d = DownloadList::instance().find(p.getHash());
		if (d) {
			addOffered(d, false);
		}
	}
	if (!m_sourceInfo) {
		// happens when addOffered() discovers we don't need the file
		// from the client afterall. Just ignore it then.
		return;
	}
	CHECK_THROW(isConnected());
	CHECK_THROW(m_sourceInfo);

	Download *d = m_sourceInfo->getReqFile();
	MetaData *md = d->getPartData()->getMetaData();
	if (md) {
		m_sourceInfo->addFileName(md, p.getName());
	}

	logTrace(TRACE_CLIENT,
		boost::format("[%s] Sending SetReqFileId for hash %s")
		% getIpPort() % m_sourceInfo->getReqFile()->getHash().decode()
	);
	*m_socket << ED2KPacket::SetReqFileId(
		m_sourceInfo->getReqFile()->getHash()
	);

	if (d->isSourceReqAllowed(this)) {
		logTrace(TRACE_SRCEXCH, boost::format(
			"[%s] SourceExchange: Requesting sources for file %s."
		) % getIpPort() % d->getHash().decode());

		*m_socket << ED2KPacket::SourceExchReq(d->getHash());
		d->setLastSrcExch(Utils::getTick());
	}
}

// Description of the file ... well, actually, a comment. Rather useless, if
// you ask me, but well - some like them.
void Client::onPacket(const ED2KPacket::FileDesc &p) {
	CHECK_THROW(m_sourceInfo);
	CHECK_THROW(m_sourceInfo->getReqFile());

	boost::format fmt(
		"Received comment for file %s:\nRating: %s Comment: %s"
	);
	fmt %m_sourceInfo->getReqFile()->getPartData()->getDestination().leaf();
	fmt % ED2KFile::ratingToString(p.getRating());
	fmt % p.getComment();

	logMsg(fmt);
	MetaData *md = m_sourceInfo->getReqFile()->getPartData()->getMetaData();
	if (md) {
		boost::format fmt("%s (Rating: %s)");
		fmt % p.getComment() % ED2KFile::ratingToString(p.getRating());
		md->addComment(fmt.str());
	}
}

// Status of the file ... now we'r getting somewhere. Among other useful things,
// this contains which chunks of the file the sender has. Niice. This is useful,
// keep it around somewhere, since we might want to x-ref it with our own chunk
// lists to generate nice chunk requests.
//
// Now we can indicate that we are really really sure we really want the file
// we'v been trying to request so far, so tell it to it -> startuploadreq.
// As a sidenote though, we might want to know more of the file than we already
// know, e.g. part hashes, so locate the corresponding hashset from metadb, and
// request a hashset if we need one.
//
// Note that if the source does not have any needed parts for us (checked via
// m_sourceInfo->hasNeededParts() method), we keep the source alive for now -
// it might become useful at some later time.
void Client::onPacket(const ED2KPacket::FileStatus &p) {
	logTrace(TRACE_CLIENT,
		boost::format("[%s] Received FileStatus.") % getIpPort()
	);
	CHECK_THROW(isConnected());

	if (!m_sourceInfo) {
		Download *d = DownloadList::instance().find(p.getHash());
		if (d) {
			addOffered(d);
		}
	}

	if (!m_sourceInfo) {
		// happens when addOffered() discovers we don't need the file
		// from the client afterall. Just ignore it then.
		return;
	}

	m_sourceInfo->setPartMap(p.getPartMap());
	m_dnReqInProgress = false;

	// only send StartUploadReq if we don't have downloadinfo yet
	if (m_sourceInfo->hasNeededParts() && !m_downloadInfo) {
		logTrace(TRACE_CLIENT,
			boost::format("[%s] Sending StartUploadReq for hash %s")
			% getIpPort() 
			% m_sourceInfo->getReqFile()->getHash().decode()
		);
		ED2KPacket::StartUploadReq p(
			m_sourceInfo->getReqFile()->getHash()
		);
		*m_socket << p;

		getEventTable().postEvent(
			this, EVT_REASKFILEPING, SOURCE_REASKTIME
		);
		m_lastReaskTime = EventMain::instance().getTick();
	} else if (!m_sourceInfo->hasNeededParts()) {
		logTrace(TRACE_CLIENT,
			boost::format("[%s] Client has no needed parts.")
			% getIpPort()
		);
		// re-establish connection in one hour
		Utils::timedCallback(
			boost::bind(&Client::establishConnection, this),
			SOURCE_REASKTIME * 2
		);
	}

	if (m_sourceInfo->getReqFile()->getSize() <= ED2K_PARTSIZE) {
		return; // don't need hashset
	}

	MetaData *md = m_sourceInfo->getReqFile()->getPartData()->getMetaData();
	for (uint32_t i = 0; i < md->getHashSetCount(); ++i) {
		HashSetBase *hs = md->getHashSet(i);
		if (hs->getFileHashTypeId() != CGComm::OP_HT_ED2K) {
			continue;
		}
		if (!hs->getChunkCnt()) {
			*m_socket << ED2KPacket::ReqHashSet(
				m_sourceInfo->getReqFile()->getHash()
			);
		}
	}
}

// Oh? But ... but you said you had the file ? What did you do it ? Did you
// delete it already? But why? Was it a bad file? ....
// So many questions.... so little time.
void Client::onPacket(const ED2KPacket::NoFile &p) {
	logTrace(TRACE_CLIENT,
		boost::format("[%s] Received NoFile.") % getIpPort()
	);
	if (!m_sourceInfo) {
		logTrace(TRACE_CLIENT,
			boost::format("[%s] Got NoFile, but no sourceinfo?")
			% getIpPort()
		);
		return;
	}
 	Download *d = DownloadList::instance().find(p.getHash());
	if (d && m_sourceInfo->offers(d)) {
		remOffered(d);
	}
}

// This could be useful... any cool info is always welcome. So here ... hashes?
// hum, let's update MetaDb then, if we received this, we probably needed them.
void Client::onPacket(const ED2KPacket::HashSet &p) {
	if (!m_sourceInfo || !m_sourceInfo->getReqFile()) {
		return;
	}

	logTrace(TRACE_CLIENT,
		boost::format("[%s] Received HashSet for %s")
		% getIpPort() % p.getHashSet()->getFileHash().decode()
	);

	verifyHashSet(p.getHashSet());

	MetaData *md = m_sourceInfo->getReqFile()->getPartData()->getMetaData();
	if (!md) {
		logTrace(TRACE_CLIENT,
			boost::format(
				"[%s] Received HashSet, but for what file?"
			) % getIpPort()
		);
		return;
	}
	HashSetBase *hs = 0;
	for (uint32_t i = 0; i < md->getHashSetCount(); ++i) {
		if (md->getHashSet(i)->getFileHashTypeId()!=CGComm::OP_HT_ED2K){
			continue;
		}
		hs = md->getHashSet(i);
		break;
	}
	if (!hs) {
		logTrace(TRACE_CLIENT,
			boost::format(
				"[%s] Received HashSet, but for what file?"
			) % getIpPort()
		);
		return;
	}
	ED2KHashSet *ehs = dynamic_cast<ED2KHashSet*>(hs);
	CHECK_THROW(ehs);
	CHECK_THROW(ehs->getFileHash() == p.getHashSet()->getFileHash());

	if (ehs->getChunkCnt()) {
		return; // no chunk hashes needed
	}
	for (uint32_t i = 0; i < p.getHashSet()->getChunkCnt(); ++i) {
		ehs->addChunkHash((*p.getHashSet())[i].getData());
	}
	if (m_sourceInfo && m_sourceInfo->getReqFile()) {
		m_sourceInfo->getReqFile()->getPartData()->addHashSet(ehs);
	}
}

// verify the contents of passed hashset
void Client::verifyHashSet(boost::shared_ptr<ED2KHashSet> hs) {
	ED2KHashMaker maker;
	for (uint32_t i = 0; i < hs->getChunkCnt(); ++i) {
		maker.sumUp((*hs)[i].getData().get(), 16);
	}
	boost::scoped_ptr<HashSetBase> ret(maker.getHashSet());

	if (ret->getFileHash() != hs->getFileHash()) {
		throw std::runtime_error("Client sent invalid hashset!");
	}
}

// note: don't disconnect the client here, since he might want smth from us
// too. While this does impose up to 30-sec longer delay in keeping sockets
// open, if we disconnect here, we ruin remote client's attempt to request
// stuff from us.
//
// using our smart early-disconnection scheme, we can disconnect the client
// here, if we are sure the remote client has sent all their requests already.
void Client::setOnQueue(uint32_t qr) {
	CHECK_THROW(m_sourceInfo);
	CHECK_THROW(m_sourceInfo->getReqFile());

	logTrace(TRACE_CLIENT,
		boost::format(
			COL_GREEN "[%s] We are queued on position" COL_COMP
			" %d " COL_GREEN "for file %s" COL_NONE
		) % getIpPort() % qr
		% m_sourceInfo->getReqFile()->
			getPartData()->getDestination().leaf()
	);

	m_sourceInfo->setQR(qr);

	// when transfer was in progressn, and we got queue ranking, also
	// schedule next reask, otherwise we never reask this client.
	if (m_downloadInfo) {
		getEventTable().postEvent(
			this, EVT_REASKFILEPING, SOURCE_REASKTIME
		);
		m_lastReaskTime = EventMain::instance().getTick();
	}

	// reset download-info here, to work around the case where client sends
	// us queue-ranking while downloading is in progress (in which case, it
	// put us on queue, and is supposed to stop transfering data to us).
	// This also handles next AcceptUpload properly (which is otherwise
	// simply ignored, if DownloadInfo object is alive when it arrives).
	m_downloadInfo.reset();

	m_dnReqInProgress = false;
}

void Client::onPacket(const ED2KPacket::QueueRanking &p) {
	setOnQueue(p.getQR());
}

void Client::onPacket(const ED2KPacket::MuleQueueRank &p) {
	setOnQueue(p.getQR());
}

void Client::reqDownload() {
	CHECK_THROW(m_sourceInfo);
	CHECK_THROW(isConnected());

	m_sourceInfo->swapToLowest();
	Download *d = m_sourceInfo->getReqFile();

	CHECK_THROW(d);
	CHECK_THROW(DownloadList::instance().valid(d));

	// don't reask more often than allowed
	if (m_lastReaskTime + SOURCE_REASKTIME > Utils::getTick()) {
		// this is necessery, otherwise when we'r uploading to a client,
		// but did UDP reask earlier, we drop the source, since
		// onLostConnection thinks we failed to send req this session.
		if (m_sessionState) {
			m_sessionState->m_sentReq = true;
		}
		return;
	}

	logTrace(TRACE_CLIENT,
		boost::format("[%s] Requesting download.") % getIpPort()
	);
	*m_socket << ED2KPacket::ReqFile(d->getHash(), d->getPartData());
	m_lastReaskId = ED2K::instance().getId();

	CHECK_THROW(m_sessionState);
	m_sessionState->m_sentReq = true;

	// reset it here, to avoid requesting multiple times in row.
	// Note that we shouldn't emit REASKFILEPING event here - that's done
	// after sending StartUploadReq
	m_lastReaskTime = Utils::getTick();

	m_dnReqInProgress = true;
	m_failedUdpReasks = 0;
}

// Ok, now we'r really getting somewhere - seems we can start downloading now.
// Send out chunk requests and start waiting for data.
// We may already have m_downloadInfo alive at this point if we were already
// downloading from this client, and it simply re-sent us AcceptUploadReq
// again for some reason (mules seem to do it after every 9500kb part.
//
// It's also possible that we were called back, and the sending client sent
// Hello + Accept in straight row (mules do it), in which case we get here
// before we have handled id-change properly (parsing is done via direct
// calls, events go through main event loop, thus slower). In that case, just
// don't do anything here - after merging (or whatever happens on idchange),
// we'll start dloading anyway.
void Client::onPacket(const ED2KPacket::AcceptUploadReq &) try {
	logTrace(TRACE_CLIENT,
		boost::format("[%s] Received AcceptUploadReq.") % getIpPort()
	);
	if (m_sourceInfo && m_sourceInfo->getPartMap()) {
		Download *d = m_sourceInfo->getReqFile();
		CHECK_THROW(d);
		CHECK_THROW(DownloadList::instance().valid(d));

		if (!m_sourceInfo->getReqFile()->getPartData()->isRunning()) {
			logTrace(TRACE_CLIENT,
				boost::format(
					"[%s] Client accepted transfer, but "
					"file is already paused."
				) % getIpPort()
			);
			*m_socket << ED2KPacket::CancelTransfer();
			m_downloadInfo.reset();
			return;
		}
		if (!m_downloadInfo) {
			PartData *pd = d->getPartData();
			m_downloadInfo.reset(
				new Detail::DownloadInfo(
					this, pd, m_sourceInfo->getPartMap()
				)
			);
			sendChunkReqs();
		}
		// this avoids dropping the client with error 'is source,
		// but file req couldn't be sent this session' once the 
		// download session ends
		if (m_sessionState) {
			m_sessionState->m_sentReq = true;
		}

	} else {
		logTrace(TRACE_CLIENT,
			boost::format(
				"[%s] Client accepted download request, "
				"but what shall we download?"
			) % getIpPort()
		);
	}
} catch (std::exception &e) {
	logDebug(
		boost::format("[%s] Starting download: %s")
		% getIpPort() % e.what()
	);
	m_downloadInfo.reset();
}
MSVC_ONLY(;)

// Little helper method - we send chunk requests from several locations, so
// localize this in this helper function.
// If onlyNew is set true, we only send the back-most chunk request in the list.
// While mules and edonkeys request chunk in a rotational way, (e.g.
// [c1, c2, c3], [c2, c3, c4], [c3, c4, c5], it seems even they get confused
// sometimes with this thing, and start sending chunks twice. Or it could be
// we fail to do it exactly the way they expect it. Whatever the reason, it
// seems way too error-prone and un-neccesery, so we'r just going to fall back
// to requesting exactly the chunks we need, nothing more.
void Client::sendChunkReqs(bool onlyNew) try {
	using ED2KPacket::ReqChunks;
	typedef std::list<Range32>::iterator Iter;

	CHECK_THROW(isConnected());
	CHECK_THROW(m_downloadInfo);

	std::list<Range32> creqs = m_downloadInfo->getChunkReqs();

	if (creqs.size()) {
 		if (onlyNew) {
			while (creqs.size() > 1) {
				creqs.pop_front();
			}
		}

		for (Iter i = creqs.begin(); i != creqs.end(); ++i) {
			boost::format fmt("[%s] Requesting chunk %d..%d");
			fmt % getIpPort();
			logTrace(TRACE_CLIENT, fmt % (*i).begin() % (*i).end());
		}

		*m_socket << ReqChunks(
			m_sourceInfo->getReqFile()->getHash(), creqs
		);
	}
} catch (std::exception &e) {
	// Something went wrong internally... might be we failed to aquire more
	// locks in partdata, might be partdata was completed ... whatever the
	// reason, it's internal, and we might have previously requested chunks
	// that are still being downloaded, so don't self-destruct here just
	// yet. If PartData was completed, we get notified of it and will handle
	// it appropriately then. If we did fail to generate more locks, sooner
	// or later, the remote client will disconnect us, after it has sent
	// us everything we requested, and we'll handle it there. So don't do
	// anything here.
	logTrace(TRACE_CLIENT,
		boost::format("[%s] Exception while sending chunk reqests: %s")
		% getIpPort() % e.what()
	);
	(void)e;
}

MSVC_ONLY(;)

// Receiving data. If downloadInfo->write() returns true, it means we completed
// a chunk, and need to send more chunk requests.
void Client::onPacket(const ED2KPacket::DataChunk &p) {
	CHECK_THROW(m_downloadInfo);
	CHECK_THROW(isConnected());

	if (!m_downloadInfo->getReqPD()->isRunning()) {
		logTrace(TRACE_CLIENT,
			boost::format(
				"[%s] Canceling transfer - file "
				"was paused/stopped."
			) % getIpPort()
		);
		*m_socket << ED2KPacket::CancelTransfer();
		m_downloadInfo.reset();
		return;
	}

	logTrace(TRACE_CLIENT,
		boost::format(
			COL_RECV "[%s] Received %d bytes data for `%s'"COL_NONE
		) % getIpPort() % p.getData().size()
		% m_downloadInfo->getReqPD()->getDestination().leaf()
	);

	Range32 r(p.getBegin(), p.getEnd() - 1);
	bool ret = m_downloadInfo->write(r, p.getData());

	if (!m_credits && m_pubKey) {
		m_credits = CreditsDb::instance().create(m_pubKey, m_hash);
	}
	if (m_credits) {
		m_credits->addDownloaded(p.getData().size());
	}

	if (ret) {
		sendChunkReqs(true);
	}
}

// Packed data - handled slightly differently from normal data.
void Client::onPacket(const ED2KPacket::PackedChunk &p) {
	CHECK_THROW(m_downloadInfo);
	CHECK_THROW(isConnected());

	if (!m_downloadInfo->getReqPD()->isRunning()) {
		logTrace(TRACE_CLIENT,
			boost::format(
				"[%s] Canceling transfer - file "
				"was paused/stopped."
			) % getIpPort()
		);
		*m_socket << ED2KPacket::CancelTransfer();
		m_downloadInfo.reset();
		return;
	}

	if (!m_downloadInfo->getReqPD()->isRunning()) {
		*m_socket << ED2KPacket::CancelTransfer();
		m_downloadInfo.reset();
		return;
	}

	logTrace(TRACE_CLIENT,
		boost::format(
			COL_RECV "[%s] Received %d bytes data for `%s' "
			COL_BYELLOW "(compressed)" COL_NONE
		) % getIpPort() % p.getData().size()
		% m_downloadInfo->getReqPD()->getDestination().leaf()
	);

	bool ret = m_downloadInfo->writePacked(
		p.getBegin(), p.getSize(),p.getData()
	);

	if (!m_credits && m_pubKey) {
		m_credits = CreditsDb::instance().create(m_pubKey, m_hash);
	}
	if (m_credits) {
		m_credits->addDownloaded(p.getData().size());
	}

	if (ret) {
		sendChunkReqs(true);
	}
}

void Client::onPacket(const ED2KPacket::SourceExchReq &p) {
	CHECK_THROW(m_socket);

	Download *d = DownloadList::instance().find(p.getHash());

	if (d && d->getSourceCount() > 0 && d->getSourceCount() < 50) {
		ED2KPacket::AnswerSources packet(p.getHash(), d->getSources());
		packet.setSwapIds(getSrcExchVer() >= 3);
		*m_socket << packet;

		logTrace(TRACE_SRCEXCH,
			boost::format(
				"[%s] SourceExchange: Sending %d sources for "
				"hash %s"
			) % getIpPort() % packet.size() % p.getHash().decode()
		);
	}
}

void Client::onPacket(const ED2KPacket::AnswerSources &p) try {
	bool fServers = Prefs::instance().read<bool>("/ed2k/FindServers", true);
	uint32_t cnt = 0;
	bool swapIds = getSrcExchVer() >= 3;
	ED2KPacket::AnswerSources::CIter it = p.begin();
	while (it != p.end()) {
		IPV4Address src((*it).get<0>(), (*it).get<1>());
		IPV4Address srv((*it).get<2>(), (*it).get<3>());
		if (swapIds) {
			src.setAddr(SWAP32_ON_LE(src.getAddr()));
		}
		if (src) {
			cnt += Detail::foundSource(p.getHash(), src, srv, true);
		}
		if (srv && fServers) {
			Detail::foundServer(srv);
		}
		++it;
	}

	logTrace(TRACE_SRCEXCH,
		boost::format(
			"[%s] SourceExchange: Received %d sources for hash "
			"%s (and %d duplicates)"
		) % getIpPort() % cnt % p.getHash().decode() % (p.size() - cnt)
	);
} catch (std::exception &e) {
	logTrace(TRACE_SRCEXCH,
		boost::format("[%s] SourceExchange error: %s")
		% getIpPort() % e.what()
	);
	(void)e;
}

void Client::onPacket(const ED2KPacket::ChangeId &p) {
	boost::format fmt("[%s] ChangeId: %d -> %d");
	fmt % getIpPort();
	if (::Donkey::isHighId(p.getOldId())) {
		fmt % Socket::getAddr(p.getOldId());
	} else {
		fmt % p.getOldId();
	}
	if (::Donkey::isHighId(p.getNewId())) {
		fmt % Socket::getAddr(p.getNewId());
	} else {
		fmt % p.getNewId();
	}
	logTrace(TRACE_CLIENT, fmt);
	Detail::changeId(this, p.getNewId());
}

void Client::onPacket(const ED2KPacket::Message &p) {
	if (!Detail::checkMsgFilter(p.getMsg())) {
		logMsg(
			boost::format("Received message from %s: %s")
			% getIpPort() % p.getMsg()
		);
	}
}

void Client::onPacket(const ED2KPacket::SecIdentState &p) {
	using namespace ED2KPacket;
	boost::format fmt(
		"[%s] Received SecIdentState %s (ch=%d)"
	);
	fmt % getIpPort();
	if (p.getState() == SI_SIGNEEDED) {
		logTrace(TRACE_SECIDENT, fmt % "NeedSign" % p.getChallenge());
	} else if (p.getState() == SI_KEYANDSIGNEEDED) {
		logTrace(TRACE_SECIDENT, fmt%"NeedKeyAndSign"%p.getChallenge());
	} else {
		logTrace(TRACE_SECIDENT, fmt % "Unknown" % p.getChallenge());
	}

	m_reqChallenge = p.getChallenge();

	if (!m_pubKey && !m_sentChallenge) {
		verifyIdent();
	}
	if (p.getState() == SI_KEYANDSIGNEEDED) {
		sendPublicKey();
	}
	if (m_pubKey) {
		sendSignature();
	}
}

void Client::onPacket(const ED2KPacket::PublicKey &p) {
	logTrace(TRACE_SECIDENT,
		boost::format("[%s] Received PublicKey.") %getIpPort()
	);
	if (m_pubKey && m_pubKey != p.getKey()) {
		logDebug(
			boost::format(
				"[%s] Client sent DIFFERENT public "
				"keys! Someone is doing something bad."
			) % getIpPort()
		);
		m_pubKey.clear();
		m_credits = 0;  // no credits anymore
	} else {
		m_pubKey = p.getKey();
		if (m_reqChallenge) {
			sendSignature();
		}
	}
}

void Client::onPacket(const ED2KPacket::Signature &p) {
	CHECK_RET(m_pubKey);
	CHECK_RET(m_sentChallenge);

	boost::format fmt(
		"[%s] Received Signature (iType=%s) (ch=%d)"
	);
	fmt % getIpPort();
	if (p.getIpType() == IP_REMOTE) {
		fmt % "Remote";
	} else if (p.getIpType() == IP_LOCAL) {
		fmt % "Local";
	} else {
		fmt % "Null";
	}
	logTrace(TRACE_SECIDENT, fmt % m_sentChallenge);

	IpType iType = 0;
	uint32_t id = 0;
	if (getSecIdentVer() > 1 && p.getIpType()) {
		iType = p.getIpType();
		if (iType == IP_REMOTE) {
			id = ED2K::instance().getId();
		} else if (iType == IP_LOCAL) {
			id = m_socket->getPeer().getIp();
		}
	}

	bool ret = false;
	try {
		ret = CreditsDb::verifySignature(
			m_pubKey, m_sentChallenge, p.getSign(), iType, id
		);
	} catch (std::exception &e) {
		logTrace(TRACE_CLIENT,
			boost::format("[%s] Error verifying signature: %s")
			% getIpPort() % e.what()
		);
		(void)e;
	}

	if (ret) {
		logTrace(TRACE_SECIDENT,
			boost::format("[%s] Ident succeeded.")
			% getIpPort()
		);
		if (!m_credits) {
			m_credits = CreditsDb::instance().find(m_pubKey);
		}
		if (m_credits) {
			CHECK_THROW(m_pubKey == m_credits->getPubKey());
			// save some ram - avoid storing PubKey twice
			// (it has refcounted implementation)
			m_pubKey = m_credits->getPubKey();
			m_credits->setLastSeen(Utils::getTick() / 1000);
			logTrace(TRACE_SECIDENT,
				boost::format(
					"[%s] Found credits: %s up, %s down"
				) % getIpPort() % Utils::bytesToString(
					m_credits->getUploaded()
				) % Utils::bytesToString(
					m_credits->getDownloaded()
				)
			);
		}
		try {
			initTransfer();
		} catch (std::exception &e) {
			logDebug(
				boost::format(
					"[%s] Failed to initTransfer(): %s"
				) % getIpPort() % e.what()
			);
			destroy(); // what else can we do here ?
		}
	} else {
		logTrace(TRACE_SECIDENT,
			boost::format(
				COL_BYELLOW "[%s] SecIdent failed!" COL_NONE
			) % getIpPort()
		);
		m_credits = 0;
	}
	m_sentChallenge = 0;
}

void Client::sendPublicKey() {
	CHECK_THROW(isConnected());

	*m_socket<< ED2KPacket::PublicKey(CreditsDb::instance().getPublicKey());
}

void Client::sendSignature() {
	CHECK_THROW(m_pubKey); // need client's public key to send signature!
	CHECK_THROW(m_reqChallenge); // challenge this sig responds to
	CHECK_THROW(isConnected());

	logTrace(TRACE_SECIDENT,
		boost::format("[%s] Sending Signature (ch=%d)")
		% getIpPort() % m_reqChallenge
	);

	IpType iType = 0;
	uint32_t ip = 0;
	if (getSecIdentVer() == 2) { // only v2 uses this, not v1 and not v3
		if (::Donkey::isLowId(ED2K::instance().getId())) {
			iType = IP_REMOTE;
			ip = getId();
		} else {
			iType = IP_LOCAL;
			ip  = ED2K::instance().getId();
		}
	}

	std::string sign(
		CreditsDb::createSignature(m_pubKey, m_reqChallenge, iType, ip)
	);

	ED2KPacket::Signature packet(sign, iType);
	*m_socket << packet;
	m_reqChallenge = 0;
}

void Client::verifyIdent() {
	if (!getSecIdentVer() || m_sentChallenge) {
		return;
	}
	CHECK_THROW(isConnected());
	boost::format fmt("[%s] Requesting %s");

	SecIdentState state(SI_SIGNEEDED);
	if (!m_pubKey) {
		state = SI_KEYANDSIGNEEDED;
		fmt % getIpPort() % "KeyAndSign";
	} else {
		fmt % getIpPort() % "Sign";
	}
	logTrace(TRACE_SECIDENT, fmt);

	ED2KPacket::SecIdentState packet(state);
	m_sentChallenge = packet.getChallenge();
	*m_socket << packet;
}

void Client::handshakeCompleted() try {
	if (getSecIdentVer() && !m_sentChallenge) {
		verifyIdent();
	} else {
		initTransfer();
	}
} catch (std::exception &e) {
	logDebug(
		boost::format("[%s] Failed to initTransfer(): %s")
		% getIpPort() % e.what()
	);
	destroy(); // what else can we do here ?
}

void Client::initTransfer() {
	if (m_uploadInfo) {
		m_queueInfo.reset();
		startUpload();
	}
	if (m_sourceInfo && !m_downloadInfo) {
		reqDownload();
	}
}

// perform a reask for download
// if our ID has changed since last reask (stored in m_lastReaskId member),
// we force TCP reask, since with UDP reask, the remote client can't recognize
// us. Also, if the remote client is LowID, or doesn't support UDP reasks, we
// also perform TCP reask instead. In all other cases, we use UDP to save
// bandwidth.
// If we'r already connected to the client at the time of this function call,
// just send normal reask packet via the current socket.
void Client::reaskForDownload() {
	CHECK_THROW(m_sourceInfo);
	CHECK_THROW(!m_downloadInfo);

	// allow reask time to vary in 2-second timeframe; worst-case scenario
	// is that we do two reasks within 2-second timeframe, but this avoids
	// small variations in time calculations, where we skip a reask since
	// this handler was called 1 sec before the actual reask time.
	if (m_lastReaskTime + SOURCE_REASKTIME > Utils::getTick() + 2000) {
		logTrace(TRACE_CLIENT,
			boost::format(
				"[%s] Reask time, we already did that %.2f "
				"seconds ago. Ignoring this reask."
			) % getIpPort()
			% ((Utils::getTick() - m_lastReaskTime) / 1000)
		);
		return;
	}

	if (!isConnected() && ED2K::instance().isLowId() && isLowId()) {
		logTrace(TRACE_CLIENT,
			boost::format("[%s] Cannot do LowID<->LowID reask!")
			% getIpPort()
		);
		destroy();
		return;
	}

	if (isConnected()) {
		reqDownload();
	} else if (m_lastReaskId && m_lastReaskId != ED2K::instance().getId()) {
		logTrace(TRACE_CLIENT,
			boost::format(
				"[%s] Our ID has changed since last reask, "
				"forcing TCP reask."
			) % getIpPort()
		);
		establishConnection();
	} else if (isHighId() && getUdpPort() && getUdpVer()) try {
		Download *d = m_sourceInfo->getReqFile();
		CHECK_THROW(d);
		CHECK_THROW(DownloadList::instance().valid(d));
		const PartData *pd = d->getPartData();
		CHECK_THROW(pd);
		Hash<ED2KHash> hash = d->getHash();
		uint32_t srcCnt = d->getSourceCount();
		const IPV4Address addr(getId(), getUdpPort());

		ED2KPacket::ReaskFilePing packet(hash, pd, srcCnt, getUdpVer());
		s_clientUdpSocket->send(packet, addr);

		getEventTable().postEvent(this, EVT_REASKTIMEOUT, UDP_TIMEOUT);
		m_reaskInProgress = true;
		m_lastReaskId = ED2K::instance().getId();

		logTrace(TRACE_CLIENT,
			boost::format("[%s] UDP Reask in progress...")
			% getIpPort()
		);
	} catch (SocketError &e) {
		logDebug(
			boost::format(
				"[%s] Fatal error performing UDP reask: %s"
			) % getIpPort() % e.what()
		);
	} else {
		logTrace(TRACE_CLIENT,
			boost::format("[%s] TCP Reask in progress...")
			% getIpPort()
		);
		establishConnection();
	}
}

// UDP file reasks handler
//
// If we do not have queueinfo when this packet is received, this indicates the
// client isn't in our queue; this usually happens when we have just restarted,
// and our queue has been lost, but clients come reasking. In that case, to be
// fair to the remote clients, we passivly initiate the file upload request.
//
// However, we will send QR 0 in response in this function, because getting it
// to send the right QR here means more breaking things apart, and keeping even
// more state variables than we already have, so - sending QR0 to ppl for a
// while after restart is acceptable loss, at least for now - internally we'r
// still handling everything properly, so this only affects "visible" side on
// the remote client anyway.
void Client::onPacket(const ED2KPacket::ReaskFilePing &p) {
	// no queue-info - usually happens when we do a restart, and forget
	// our queue - add it to our queue by our normal queueing rules.
	if (!m_queueInfo) {
		logTrace(TRACE_CLIENT,
			boost::format(
				"[%s] UDP ReaskFilePing: Passivly initiating "
				"remote UploadReq."
			) % getIpPort()
		);
		onUploadReq(p.getHash());
		Download *d = DownloadList::instance().find(p.getHash());
		if (d) {
			addOffered(d);
		}
	}

	// can't send to LowId clients; or already uploading; or no queue info
	if (isLowId() || m_uploadInfo || !m_queueInfo) {
		return;
	}

	logTrace(TRACE_CLIENT,
		boost::format("[%s] Received ReaskFilePing.") % getIpPort()
	);

	IPV4Address addr(getId(), getUdpPort());

	if (p.getHash() != m_queueInfo->getReqHash()) {
		SharedFile *sf = MetaDb::instance().findSharedFile(p.getHash());
		if (sf) {
			logTrace(TRACE_CLIENT,
				boost::format(
					"[%s] UDP SwapToAnotherFile performed."
				) % getIpPort()
			);
			m_queueInfo->setReqFile(sf, p.getHash());
		} else try {
			logTrace(TRACE_CLIENT,
				boost::format(
					"[%s] UDP SwapToAnotherFile failed."
				) % getIpPort()
			);
			ED2KPacket::FileNotFound packet;
			s_clientUdpSocket->send(packet, addr);
		} catch (SocketError &e) {
			logDebug(boost::format(
				"[%s] Fatal error sending UDP packet: %s"
			) % getIpPort() % e.what());
		}
	}

	const PartData *pd = m_queueInfo->getReqFile()->getPartData();
	uint16_t queueRank = m_queueInfo->getQR();

	try {
		ED2KPacket::ReaskAck packet(pd, queueRank, getUdpVer());
		s_clientUdpSocket->send(packet, addr);

		logTrace(TRACE_CLIENT,
			boost::format(
				"[%s] ReaskFilePing: Sending QueueRank %s"
			) % getIpPort() % queueRank
		);

		m_queueInfo->m_lastQueueReask = Utils::getTick();
	} catch (SocketError &e) {
		logDebug(
			boost::format(
				"[%s] Fatal error sending ReaskFilePing: %s"
			) % getIpPort() % e.what()
		);
	}
}

void Client::onPacket(const ED2KPacket::ReaskAck &p) {
	CHECK_THROW(m_sourceInfo);
	m_reaskInProgress = false;
	m_failedUdpReasks = 0;

	logTrace(TRACE_CLIENT,
		boost::format(
			"[%s] UDP Reask: Received ReaskAck; "
			"We are queued on position %d for file %s"
		) % getIpPort() % p.getQR() % m_sourceInfo->getReqFile()->
			getPartData()->getDestination().leaf()
	);

	m_sourceInfo->setPartMap(p.getPartMap());
	m_sourceInfo->setQR(p.getQR());

	getEventTable().postEvent(this, EVT_REASKFILEPING, SOURCE_REASKTIME);
	m_lastReaskTime = EventMain::instance().getTick();
}

// We can only assume that the FileNotFound is about currently requested
// file, because the packet doesn't contain hash. There might be a race
// condition hidden here, when we swap to another file DURING the UDP callback,
// in which case we incorrectly remove the wrong offered file from m_sourceInfo.
void Client::onPacket(const ED2KPacket::FileNotFound &) {
	m_reaskInProgress = false;
	m_failedUdpReasks = 0;

	logTrace(TRACE_CLIENT,
		boost::format("[%s] UDP Reask: Received FileNotFound")
		% getIpPort()
	);

	if (m_sourceInfo) {
		remOffered(m_sourceInfo->getReqFile());
	}
	if (m_sourceInfo) {
		reaskForDownload(); // switch to another file performed
	}
}

void Client::onPacket(const ED2KPacket::QueueFull &) {
	m_reaskInProgress = false;
	m_failedUdpReasks = 0;

	logTrace(TRACE_CLIENT,
		boost::format("[%s] UDP Reask: Received QueueFull")
		% getIpPort()
	);

	if (m_sourceInfo) {
		m_sourceInfo->setQR(0);
	}

	getEventTable().postEvent(this, EVT_REASKFILEPING, SOURCE_REASKTIME);
	m_lastReaskTime = EventMain::instance().getTick();
}

void Client::removeFromQueue() {
	m_queueInfo.reset();
	getEventTable().postEvent(this, EVT_CANCEL_UPLOADREQ);
	checkDestroy();
}

} // end namespace Donkey
