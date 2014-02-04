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
 * \file serverlist.h Interface for ServerList and Server classes
 */

#ifndef __ED2K_SERVERLIST_H__
#define __ED2K_SERVERLIST_H__

#include <hncore/ed2k/ed2ktypes.h>         // types and fwd dclrs
#include <hnbase/ipv4addr.h>               // ipv4addr
#include <hnbase/object.h>                 // object api
#include <hncore/fwd.h>                    // forward declrs
#include <hncore/search.h>                 // Search API

namespace Donkey {

namespace Detail {
	struct FileQueryList;  // Files Query Times list implementation
	struct MIServerList;   // ServerList implementation
	struct QTIter;         // QueryTime Iterator
	class Server;
}

/**
 * ServerList class encapsulates the list of known servers. The list is loaded
 * from config/ed2k/server.met on startup, and saved to same file on shutdown.
 * The file format conforms to eDonkey2000 protocol specification. This class
 * also has the added responsibility of handling server connections, as well as
 * communication with currently connected server.
 *
 * ServerList class is a singleton with lazy initialization. The only instance
 * of this class may be retrieved via static instance() member function.
 */
class ServerList : public Object, public boost::signals::trackable {
public:
	//! Return the only instance of this class
	static ServerList& instance() {
		static ServerList *s = new ServerList();
		return *s;
	}

	//! Load the list from file, adding all found items to the list
	void load(const std::string &file);

	//! Save the list to the specified file
	void save(const std::string &file) const;

	/**
	 * Initialize ServerList networking
	 */
	void init();

	/**
	 * Exit ServerList
	 */
	void exit();

	/**
	 * \brief Connect to a random server.
	 *
	 * If the connections fail, the connecting attempt to continue until the
	 * connection has been established, or the list is exhausted.
	 */
	void connect();

	/**
	 * \brief Connect to a specific server, possibly disconnecting from
	 *        current server, if connected.
	 *
	 * @param s    Server to connect to
	 */
	void connect(Detail::Server *s);

	/**
	 * \returns Address of currently connected server
	 * \throws std::runtime_error if not connected
	 */
	IPV4Address getCurServerAddr() const;

	/**
	 * Get current connection status.
	 *
	 * @return       Bitfield containing one or more of Status values.
	 */
	uint8_t getStatus() const { return m_status; }

	/**
	 * Request callback from a client via server
	 *
	 * @param id    ID if the client we wish to contact us
	 */
	void reqCallback(uint32_t id);

	/**
	 * Add a server by address
	 *
	 * @param addr      Address of the server
	 */
	void addServer(IPV4Address addr);

	// ddeml.h (included from windows.h included from gettickcount.h)
	// defines ST_CONNECTED already
	#ifdef ST_CONNECTED
		#undef ST_CONNECTED
	#endif

	//! Statuses
	enum Status {
		ST_CONNECTED  = 0x01,       //!< Connected
		ST_CONNECTING = 0x02,       //!< Not connected
		ST_LOGGINGIN  = 0x03        //!< Logging in to server
	};

private:

	// internal data
	// -------------

	ED2KClientSocket*      m_serverSocket;   //!< Socket to current server
	Detail::Server*        m_currentServer;  //!< Currently connected server
	SearchPtr              m_curSearch;      //!< Current search in progress
	uint64_t               m_lastSourceRequest; //!< Time of last src req

	//! Server stream parser
	boost::shared_ptr<ED2KParser<ServerList, ED2KNetProtocolTCP> > m_parser;
	//! Keeps track of currect connection status
	uint8_t m_status;

	//! List of servers, sorted by IP and Name
	boost::scoped_ptr<Detail::MIServerList> m_list;

	//! Keeps the time when last connection attempt was done
	uint32_t m_lastConnAttempt;

	//! Connected to Detail::foundServer signal
	boost::signals::scoped_connection m_foundServerConn;

	//! Events emitted from ServerList class (used internally only)
	enum ServerListEvent {
		EVT_PINGSERVER,    //!< Ping server with empty OfferFiles packet
		EVT_REQSOURCES,    //!< Request sources from server
		EVT_QUERYSERVER,   //!< UDP GetSources and Ping request time
		EVT_CONNECT        //!< Attempt to connect to next server
	};

	//! Events emitted from Server class (used internally only)
	enum ServerEvent {
		EVT_PINGTIMEOUT,   //!< Indicates UDP query timed out
		EVT_LOGINTIMEOUT   //!< Login attempt timeouts
	};

	// internal functions

	DECLARE_EVENT_TABLE(ServerList*, ServerListEvent);

	ServerList();                        //!< Singleton - private
	~ServerList();                       //!< Singleton - private

	//! Event handler for our own events, used for timed events
	void onServerListEvent(ServerList *sl, ServerListEvent evt);

	//! Event handler for currentserver events
	void onServerSocketEvent(ED2KClientSocket *c, SocketEvent evt);

	//! Event handler for internal Server events
	void onServerEvent(Detail::Server *serc, int event);

	//! Handles application configuration changes
	void configChanged(const std::string &key, const std::string &value);

	//! Sends login request
	void sendLoginRequest();

	/**
	 * Sends our current shared files list to server (all of them)
	 *
	 * @param useZlib        Whether to use Zlib compression (and the
	 *                       related 'special' id's during sending.
	 */
	void publishFiles(bool useZlib);

	/**
	 * Publish a single file to server.
	 *
	 * @param sf             File to publish
	 */
	void publishFile(SharedFile *sf);

	/**
	 * Perform a search on eDonkey2000 network, based on criteria described
	 * in argument.
	 *
	 * @param search         Pointer to search object describing the search
	 *                       criteria.
	 */
	void performSearch(SearchPtr search);

	/**
	 * Adds a set of hardcoded servers to the server list.
	 */
	void addDefaultServers();

	/**
	 * Event handler for events emitted from SharedFile. This function is
	 * called from event table and should never be called directly.
	 */
	void onSharedFileEvent(SharedFile *sf, int evt);

	/**
	 * Request sources for all temporary files which have the neccesery
	 * information (e.g. ED2KHash).
	 */
	void reqSources();

	/**
	 * Requests sources for download from current server (if connected).
	 * Does nothing if we are currently not connected to a server.
	 *
	 * @param d           Download to request sources for
	 */
	void reqSources(Download &d);

	/**
	 * Requests sources from current server for next 15 files; this method
	 * is called via timed callbacks and shouldn't be called directly.
	 *
	 * @param sock        Socket to write to; writing is only performed if
	 *                    it matches m_serverSocket.
	 * @param data        Data to be written to the socket.
	 */
	void reqMoreSources(ED2KClientSocket *sock, std::string data);

	/**
	 * Chooses next server to perform UDP queries with
	 */
	void queryNextServer();

	//! Ping the server pointed to by iterator
	void pingServer(Detail::QTIter &it);
	//! Get sources via UDP from server pointed to by iterator
	void udpGetSources(Detail::QTIter &it);

	/**
	 * Event handler for server udp listener
	 */
	void onUdpData(ED2KUDPSocket *sock, SocketEvent evt);

	/**
	 * Sends global UDP search query to next server in list.
	 */
	void doGlobSearch(
		SearchPtr search,
		boost::shared_ptr<std::list<Detail::Server*> > srv
	);
public:
	//! @name Packet handlers
	//@{
	void onPacket(const ED2KPacket::ServerMessage &p);
	void onPacket(const ED2KPacket::IdChange &p);
	void onPacket(const ED2KPacket::ServerStatus &p);
	void onPacket(const ED2KPacket::ServerIdent &p);
	void onPacket(const ED2KPacket::ServerList &p);
	void onPacket(const ED2KPacket::SearchResult &p);
	void onPacket(const ED2KPacket::CallbackReq &p);
	void onPacket(const ED2KPacket::FoundSources  &p);

	/**
	 * Handles GlobFoundSources data from UDP servers.
	 *
	 * @param i      Input stream to read packet(s) from
	 * @param from   The server that sent this data
	 */
	void handleGlobSources(std::istringstream &i, IPV4Address from);

	/**
	 * Handles GlobStatRes data from UDP servers
	 *
	 * @param i      Input stream to read packet from
	 * @param from   The server that sent this data
	 */
	void handleGlobStatRes(std::istringstream &i, IPV4Address from);

	/**
	 * Handles GlobSearchRes packet from UDP servers
	 *
	 * @param i      Input stream to read packet from
	 * @param from   The server that sent this data
	 */
	void handleGlobSearchRes(std::istringstream &i, IPV4Address from);
	//@}

	//! @name Various operations
	//@{
	virtual uint32_t getOperCount() const;
	virtual Object::Operation getOper(uint32_t n) const;
	virtual void doOper(const Object::Operation &op);
	virtual uint32_t getDataCount() const;
	virtual std::string getData(uint32_t n) const;
	virtual std::string getFieldName(uint32_t n) const;
	//@}
};

} // end namespace Donkey

#endif
