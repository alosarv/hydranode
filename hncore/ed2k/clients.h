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
 * \file clients.h Interface for Client classe
 */

#ifndef __ED2K_CLIENTS_H__
#define __ED2K_CLIENTS_H__

#include <hncore/ed2k/ed2ktypes.h>
#include <hncore/ed2k/publickey.h>
#include <hncore/ed2k/clientext.h>
#include <hncore/ed2k/creditsdb.h>
#include <hncore/baseclient.h>
#include <hnbase/object.h>
#include <hnbase/ipv4addr.h>
#include <hnbase/hash.h>
#include <hnbase/ssocket.h>
#include <boost/tuple/tuple.hpp>

namespace Donkey {

//! Events emitted from Client objects
enum ClientEvent {
	EVT_DESTROY,               //!< Client will be destroyed
	EVT_UPLOADREQ,             //!< Indicates we want to upload
	EVT_IDCHANGE,              //!< Client ID changed
	EVT_CANCEL_UPLOADREQ,      //!< Indicate we no longer want to send data
	EVT_CALLBACK_T,            //!< LowID callback timeout event
	EVT_REASKFILEPING,         //!< Delayed event for UDP reasks
	EVT_REASKTIMEOUT           //!< Delayed event for UDP reasks timeouts
};

namespace Detail {
	/**
	 * \name Signals emitted from Client object
	 *
	 * These are declared here, as global, because we cannot use static data
	 * in modules.
	 */
	//!@{
	/**
	 * Emitted when a client changes it's ID
	 *
	 * @param Client*   Client that changed it's ID
	 * @param uint32_t  The new ID of the client
	 */
	extern boost::signal<void (Client*, uint32_t)> changeId;

	/**
	 * Emitted when a new source is discovered by a client
	 *
	 * @param Hash<ED2KHash>&  Hash of the file the source offers
	 * @param IPV4Address      Source IP/port
	 * @param IPV4Address      Server IP/port where the source is
	 * @param bool             If true, attempt to connect to the client now
	 * @return True if the source was added, false otherwise
	 */
	extern boost::signal<
		bool (const Hash<ED2KHash>&, IPV4Address, IPV4Address, bool)
	> foundSource;

	/**
	 * Emitted when a new server is discovered
	 *
	 * @param IPV4Address   The server's IP/port
	 */
	extern boost::signal<void (IPV4Address)> foundServer;
	//!@}

	/**
	 * Check if a message should be filtered
	 *
	 * @param std::string&   Message to be checked
	 * @returns true if the message should be filtered, false otherwise
	 */
	extern boost::signal<bool (const std::string&)> checkMsgFilter;
}

/**
 * Client object encapsulates a single remote client that we are communicating
 * with. The reasons for communication may be either because it wants something
 * from us, because we want something from it, or even both. The exact purpose
 * of the client is not defined by this object. Instead, the purpose is defined
 * by member objects DownloadClient and/or UploadClient. As long as the client
 * has at least a single purpose, it must be alive, however when it has
 * fulfulled it's purpose, and no longer contains neither UploadClient nor
 * DownloadClient, it must return to the source.
 */
class Client : public BaseClient, public Trackable {
public:
	DECLARE_EVENT_TABLE(Client*, ClientEvent);

	//! @name get-to-know-you-chit-chat
	//@{
	void onPacket(const ED2KPacket::Hello &p);
	void onPacket(const ED2KPacket::HelloAnswer &p);
	void onPacket(const ED2KPacket::MuleInfo &p);
	void onPacket(const ED2KPacket::MuleInfoAnswer &p);
	//@}
	//! @name Uploading
	//@{
	void onPacket(const ED2KPacket::ReqFile &p);
	void onPacket(const ED2KPacket::SetReqFileId &p);
	void onPacket(const ED2KPacket::ReqHashSet &p);
	void onPacket(const ED2KPacket::StartUploadReq &p);
	void onPacket(const ED2KPacket::ReqChunks &p);
	void onPacket(const ED2KPacket::CancelTransfer &);
	//@}
	//! @name Downloading
	//@{
	void onPacket(const ED2KPacket::FileName &p);
	void onPacket(const ED2KPacket::FileDesc &p);
	void onPacket(const ED2KPacket::NoFile &p);
	void onPacket(const ED2KPacket::FileStatus &p);
	void onPacket(const ED2KPacket::HashSet &p);
	void onPacket(const ED2KPacket::AcceptUploadReq &);
	void onPacket(const ED2KPacket::QueueRanking &p);
	void onPacket(const ED2KPacket::MuleQueueRank &p);
	void onPacket(const ED2KPacket::DataChunk &p);
	void onPacket(const ED2KPacket::PackedChunk &p);
	//@}
	//! @name Source Exchange
	//@{
	void onPacket(const ED2KPacket::SourceExchReq &p);
	void onPacket(const ED2KPacket::AnswerSources &p);
	//@}
	//! @name Miscellanous
	//@{
	void onPacket(const ED2KPacket::Message &p);
	void onPacket(const ED2KPacket::ChangeId &p);
	//@}
	//! @name SecIdent
	//@{
	void onPacket(const ED2KPacket::SecIdentState &p);
	void onPacket(const ED2KPacket::PublicKey &p);
	void onPacket(const ED2KPacket::Signature &p);
	//@}
	//! @name UDP packets
	//@{
	void onPacket(const ED2KPacket::ReaskFilePing &p);
	void onPacket(const ED2KPacket::FileNotFound &p);
	void onPacket(const ED2KPacket::ReaskAck &p);
	void onPacket(const ED2KPacket::QueueFull &p);
	//@}

	//! @name Accessors for this client's information
	//@{
	Hash<MD4Hash> getHash()       const { return m_hash;                   }
	uint32_t      getId()         const { return m_id;                     }
	uint16_t      getTcpPort()    const { return m_tcpPort;                }
	uint16_t      getUdpPort()    const { return m_udpPort;                }
	IPV4Address   getServerAddr() const { return m_serverAddr;             }
	bool    supportsPreview()     const { return m_features       & 0x01;  }
	bool    supportsMultiPacket() const { return m_features       & 0x02;  }
	bool    supportsViewShared()  const { return m_features       &~0x04;  }
	bool    supportsPeerCache()   const { return m_features       & 0x08;  }
	uint8_t getCommentVer()       const { return m_features >>  4 & 0x0f;  }
	uint8_t getExtReqVer()        const { return m_features >>  8 & 0x0f;  }
	uint8_t getSrcExchVer()       const { return m_features >> 12 & 0x0f;  }
	uint8_t getSecIdentVer()      const { return m_features >> 16 & 0x0f;  }
	uint8_t getComprVer()         const { return m_features >> 20 & 0x0f;  }
	uint8_t getUdpVer()           const { return m_features >> 24 & 0x0f;  }
	bool    supportsUnicode()     const { return m_features >> 28 & 0x01;  }
	uint8_t getAICHVer()          const { return m_features >> 29 & 0x07;  }
	bool    isLowId()             const { return m_id < 0x00ffffff;        }
	bool    isHighId()            const { return m_id > 0x00ffffff;        }
	uint8_t getClientSoft()       const { return m_clientSoft >> 24;       }
	uint32_t getVerMjr()          const { return m_clientSoft >> 17 & 0x7f;}
	uint32_t getVerMin()          const { return m_clientSoft >> 10 & 0x7f;}
	uint32_t getVerPch()          const { return m_clientSoft >>  7 & 0x07;}
	uint32_t getVerBld()          const { return m_clientSoft       & 0x7f;}
	bool isMule() const; // returns true if this is an eMule-compat client
	//@}

	//! @name Other generic accessors
	//@{
	ED2KClientSocket* getSocket() const { return m_socket; }
	void setSocket(ED2KClientSocket *s) {
		CHECK_THROW(m_socket);// don't allow overwriting existing socket
		m_socket = s;
	}
	void setServerAddr(IPV4Address addr) { m_serverAddr = addr; }
	uint64_t getLastReaskTime() const { return m_lastReaskTime; }
	//@}

	//! @name Override base class virtuals
	virtual IPV4Address getAddr()              const;
	virtual std::string getNick()              const;
	virtual std::string getSoft()              const;
	virtual std::string getSoftVersion()       const;
	virtual uint64_t    getSessionUploaded()   const;
	virtual uint64_t    getSessionDownloaded() const;
	virtual uint64_t    getTotalUploaded()     const;
	virtual uint64_t    getTotalDownloaded()   const;
	virtual uint32_t    getUploadSpeed()       const;
	virtual uint32_t    getDownloadSpeed()     const;
	virtual uint32_t    getQueueRanking()      const;
	virtual uint32_t    getRemoteQR()          const;

	/**
	 * Start uploading to this client.
	 *
	 * \pre m_uploadClient member exists
	 */
	void startUpload();

	/**
	 * Request to download current requested file from this client
	 * (e.g. sends ReqFile packet).
	 */
	void reqDownload();

	/**
	 * Send queue ranking to the remote client.
	 *
	 * \pre m_socket exists and is in connected state
	 * \pre m_uploadClient exists
	 */
	void sendQR();

	/**
	 * @brief Queue Score calculation.
	 *
	 * This should be implemented similarly to how emule does it to be
	 * compatible. We start out with waiting time (in seconds), and modify
	 * it by:
	 * - Client's credits modifier (if present)
	 * - \todo Requested file priority
	 * - \todo Friend status (friends get increased score)
 	 */
	uint64_t getScore() const {
		assert(m_queueInfo);
		// Score calculation: Start with waiting time amount
		uint64_t tmp = EventMain::instance().getTick();
		if (tmp < m_queueInfo->getWaitStartTime()) {
			tmp = m_queueInfo->getWaitStartTime();
		}
		tmp = (tmp - m_queueInfo->getWaitStartTime()) / 1000;

		// Modify it (if possible) with credit score
		if (m_credits) {
			tmp *= static_cast<uint64_t>(m_credits->getScore());
		}
		return tmp;
	}

	/**
	 * Add an offered file to this client's offered files list.
	 *
	 * @param file    Offered file
	 * @param doConn  Whether we are allowed to establish connection too
	 */
	void addOffered(Download *file, bool doConn = true);

	/**
	 * Remove an offered file
	 *
	 * @param file    File this client is no longer offering
	 * @param cleanUp If true, also cleanup our things from PartData
	 */
	void remOffered(Download *file, bool cleanUp = true);

	/**
	 * Check if the connection is currently established with this client.
	 *
	 * @returns true if yes, false otherwise
	 */
	bool isConnected() const { return m_socket && m_socket->isConnected(); }

	/**
	 * Check whether there's LowID callback in progress
	 *
	 * @return true if there's a LowID callback in progress, false otherwise
	 */
	bool callbackInProgress() const { return m_callbackInProgress; }

	/**
	 * Check whether we are currently in process of reasking the source.
	 *
	 * @return true if reasking is in progress, false otherwise
	 */
	bool reaskInProgress() const { return m_reaskInProgress; }

	/**
	 * Set the current client state to indicate that we are on remote
	 * client's queue, waiting for an upload. This implies we have
	 * m_sourceInfo member alive, and no m_downloadInfo.
	 *
	 * Don't confuse this with similar concept where the remote client is
	 * queued on our upload queue.
	 *
	 * @param qr        Remote queue rank
	 */
	void setOnQueue(uint32_t qr);

	/**
	 * Resets all queue-related data and emits EVT_CANCEL_UPLOADREQ to
	 * indicate that this client no longer wants anything from us.
	 */
	void removeFromQueue();

	/**
	 * Attempt to verify this client's identity
	 */
	void verifyIdent();

	/**
	 * Small helper function for retrieving client IP/ID/port combination
	 * in printable format.
	 */
	std::string getIpPort() const;

	/**
	 * \name Accessors for extensions
	 */
	//!@{
	Detail::SourceInfoPtr   getSourceInfo()   const { return m_sourceInfo; }
	Detail::QueueInfoPtr    getQueueInfo()    const { return m_queueInfo;  }
	Detail::UploadInfoPtr   getUploadInfo()   const { return m_uploadInfo; }
	Detail::DownloadInfoPtr getDownloadInfo() const {return m_downloadInfo;}
	//!@}

	/**
	 * Returns pointer to shared ClientUDPSocket.
	 */
	static ED2KUDPSocket* getUdpSocket();
private:
	friend class ClientList;
	Client();                                    //!< Forbidden
	~Client();                                   //!< Allowed by ClientList
	Client(const Client&);                       //!< Copying forbidden
	const Client& operator=(const Client&);      //!< Copying forbidden

	/**
	 * Only allowed constructor. The socket is required to initialize the
	 * client and perform initial handshaking with the client.
	 *
	 * @param c         Socket connecting to this client.
	 *
	 * \pre  The socket must be in connected state, waiting for data.
	 * \post The socket object ownership is transfered to this class.
	 */
	Client(ED2KClientSocket *c);

	/**
	 * Construct a "source" type client, which has a file to offer for us.
	 *
	 * @param addr     Address where the client is
	 * @param file     File the client is offering
	 */
	Client(IPV4Address addr, Download *file);

	/**
	 * Event handler for socket events. This is called from event loop.
	 *
	 * @param c      Socket causing the event. Must match m_socket member
	 * @param evt    The event that happened
	 */
	void onSocketEvent(ED2KClientSocket *c, SocketEvent evt);

	/**
	 * Scheduler the object for destruction. Note that the actual
	 * destruction is performed by the owner of this object (ClientList).
	 */
	void destroy();

	/**
	 * Copy client information from the hello packet to internal variables.
	 *
	 * @param p        Packet to copy the data from
	 *
	 * \note This overwrites any existing information we might have stored
	 *       for this client.
	 */
	void storeInfo(const ED2KPacket::Hello &p);

	/**
	 * Processes, extracts and stores all useful information from this
	 * extinct packet, used by some older mule-based clients.
	 *
	 * @param p       MuleInfo packet to be processed
	 */
	void processMuleInfo(const ED2KPacket::MuleInfo &p);

	/**
	 * Merge sockets and parser from other client to this client, taking
	 * ownership of those two.
	 *
	 * @param c     Other client to merge data from
	 */
	void merge(Client *c);

	/**
	 * Generalized version of upload requests - constructs the neccesery
	 * members and emits the neccesery events, indicating the client wishes
	 * to download something from us.
	 *
	 * @param hash      Hash of the file the client is interested in.
	 *
	 * \note Passed by value to allow simpler fallback implementation
	 *       when empty hash is passed.
	 */
	void onUploadReq(Hash<ED2KHash> hash);

	/**
	 * Sends next three chunk requests to current downloadclient.
	 *
	 * @param onlyNew If set true, only new requests are sent
	 */
	void sendChunkReqs(bool onlyNew = false);

	/**
	 * Sends next chunk to socket (when uploading)
	 */
	void sendNextChunk();

	/**
	 * Establish connection with the remote client either by directly
	 * connecting to it, or performing a low-id callback operation through
	 * server.
	 */
	void establishConnection();

	/**
	 * Performs UDP reask for download.
	 */
	void reaskForDownload();

	/**
	 * Checks if this client is useful to us at all, and if not, emits
	 * EVT_DESTROY.
	 */
	void checkDestroy();

	/**
	 * Send our signature to this client.
	 *
	 * \pre m_reqChallenge must be set to nonzero challenge value
	 * \pre m_pubKey must exist and be valid
	 * \post m_reqChallenge is set to 0
	 */
	void sendSignature();

	//! Send our public key to this client
	void sendPublicKey();

	//! Called after successful handshake (and optionally, SecIdent), starts
	//! actual data transfer.
	void initTransfer();

	/**
	 * Called by ClientList after received IDChange event from us, this
	 * indicates that merging (if any) with existing client has been
	 * completed, and we are ready to proceed, depending on context.
	 *
	 * If supported, the identity of the client will be verified (SecIdent),
	 * and after that, depending on which extensions are present, operations
	 * performed, e.g. start uploading/downloading.
	 */
	void handshakeCompleted();

	/**
	 * Helper function, for usage when connection with the client is lost.
	 */
	void onLostConnection();

	/**
	 * Verify the contents of the passed hashset by re-calculating the file-
	 * hash from the chunkhashes.
	 */
	void verifyHashSet(boost::shared_ptr<ED2KHashSet> hs);

	/**
	 * Disconnects this client's socket and resets upload/downloadinfo
	 * members. It's safe to call this method on already-disconnected
	 * client.
	 */
	void disconnect();

	/**
	 * Sets the shared ClientUdp socket pointer, which is allocated in
	 * clients.cpp file. This should only be called by ClientList class.
	 *
	 * @param sock      Socket to be set.
	 *
	 * \throws std::runtime_error if the socket is already set and new
	 *         setting is not null.
	 */
	static void setUdpSocket(ED2KUDPSocket *sock);

	/**
	 * @name Information we have on this client
	 */
	//@{
	uint32_t      m_id;             //!< Client ID ( <= 0x00fffff == LowID )
	uint16_t      m_tcpPort;        //!< TCP port
	uint16_t      m_udpPort;        //!< UDP port
	uint32_t      m_features;       //!< Supported features
	Hash<MD4Hash> m_hash;           //!< Userhash
	PublicKey     m_pubKey;         //!< Client's public key
	IPV4Address   m_serverAddr;     //!< Server the client is connected to
	std::string   m_nick;           //!< User nickname
	uint32_t      m_clientSoft;     //!< Client soft and version
	uint32_t      m_sessionUp;      //!< Needed by BaseClient
	uint32_t      m_sessionDown;    //!< Needed by BaseClient
	//@}

	/**
	 * @name Internal things
	 */
	//@{
	//! Stream parser
	boost::shared_ptr<ED2KParser<Client, ED2KNetProtocolTCP> > m_parser;
	ED2KClientSocket*  m_socket;    //!< Socket
	Credits*           m_credits;   //!< May be 0
	//@}

	/**
	 * Extensions to Client object (also called purposes). If none of these
	 * exists, the client should be destroyed.
	 */
	//@{
	Detail::QueueInfoPtr    m_queueInfo;
	Detail::UploadInfoPtr   m_uploadInfo;
	Detail::SourceInfoPtr   m_sourceInfo;
	Detail::DownloadInfoPtr m_downloadInfo;
	//@}

	// these are related to sources only, so should probably be moved to
	// SourceInfo class
	bool m_callbackInProgress; //!< TCP callback is in progress
	bool m_reaskInProgress;    //!< (UDP) reask is in progress
	uint8_t m_failedUdpReasks; //!< Number of failed UDP reasks (in row)
	uint64_t m_lastReaskTime;  //!< When was last source reask done
	uint32_t m_lastReaskId;    //!< Our ID during last reask

	// miscellanous stuff

	//! During identity verification, contains challenge sent TO the client
	uint32_t m_sentChallenge;
	//! During identity verification, contains challenge sent BY the client
	uint32_t m_reqChallenge;

	bool m_upReqInProgress; //!< State: Upload request is in progress
	bool m_dnReqInProgress; //!< State: Download request is in progress


	struct SessionState {
		SessionState() : m_sentHello(), m_gotHello(), m_sentReq() {}
		bool m_sentHello;
		bool m_gotHello;
		bool m_sentReq;
	};
	//! Keeps track of current session state
	boost::shared_ptr<SessionState> m_sessionState;
};

} // end namespace Donkey

#endif
