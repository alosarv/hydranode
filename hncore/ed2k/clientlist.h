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
 * \file clientlist.h Interface for ClientList class
 */

#ifndef __ED2K_CLIENTLIST_H__
#define __ED2K_CLIENTLIST_H__

#include <hncore/ed2k/fwd.h>
#include <hncore/ed2k/clients.h>
#include <hncore/fwd.h>

namespace Donkey {
namespace Detail {
	struct CList;
}

/**
 * ClientList manages all clients currently in use. A client is considered
 * "in use" when it has DownloadClient and/or UploadClient members alive.
 * This class owns the Client objects and is responsible for deleting the
 * clients, when the client submits EVT_DESTROY event.
 *
 * Clients can be found in this list by searching with the Client IP. Multiple
 * clients from same IP are allowed, in which case the differenciating can be
 * made based on the client's ports.
 */
class ClientList {
	enum ClientListEvt {
		EVT_REGEN_QUEUE  //!< Indicates ClientList to regen queue
	};
	DECLARE_EVENT_TABLE(ClientList*, ClientListEvt);
public:
	static ClientList& instance() {
		static ClientList *c = new ClientList;
		return *c;
	}

	/**
	 * Add a client which is to be connected. This method is generally
	 * used by ServerList to notify us about callback requests.
	 *
	 * @param addr      Address the client is located at
	 */
	void addClient(IPV4Address addr);

	/**
	 * Add a client that shall act as "source" for a file.
	 *
	 * @param file       File offered by the client
	 * @param caddr      Address of the client
	 * @param saddr      Address of the server the client is on
	 * @param doConn     Whether to establish connection with the client
	 * @return           True if source was added, false otherwise
	 *
	 * \note This method returns false also when the hash was added to an
	 *       already existing source.
	 */
	bool addSource(
		const Hash<ED2KHash> &file, IPV4Address caddr,
		IPV4Address saddr, bool doConn = true
	);

	/**
	 * Initializes clientlist, setting up data structures and network
	 * connections as needed. This function should be called when the
	 * module is initially loaded, but can also be called at any later
	 * time on runtime to update/reset/restart the ClientList's internals.
	 * On reason why you might want to call this is when you modify the
	 * "TCP Port" value in ED2KConfig, in which case the main listener
	 * must be restarted (done in this function).
	 */
	void init();

	/**
	 * Called on module exit; perform cleanup
	 */
	void exit();

	/**
	 * Check if a Client pointer is valid.
	 *
	 * @param c  Pointer to check
	 * @returns  true if the pointer is valid, false otherwise
	 */
	bool valid(Client *c);

	/**
	 * Selects the next valid client from the queue (highest-ranking), and
	 * starts new upload on the client.
	 */
	void startNextUpload();

	/**
	 * Prints the number of potentially zombie clients, as well as top
	 * clients that have been zombie the longest. This is meant to aid in
	 * debugging zombie clients via 'ed2k deadclients' command in hnsh.
	 */
	void printDeadClients();
private:
	ClientList();
	~ClientList();
	ClientList(const ClientList&);
	ClientList& operator=(const ClientList&);

	//! Event handler for client events
	void onClientEvent(Client *c, ClientEvent evt);

	//! Event handler for socket server events
	void onServerEvent(ED2KServerSocket *s, SocketEvent evt);

	/**
	 * Erase a client from all known containers and delete the object.
	 *
	 * @param c      Client to be destroyed.
	 */
	void removeClient(Client *c);

	/**
	 * Handles Client::changeId() signal; changes the client's Id, and
	 * updates our internal data structures to reflect the change.
	 *
	 * @param c         Client that changes it's ID
	 * @param newId     New ID of the client
	 */
	void onIdChange(Client *c, uint32_t newId);

	/**
	 * Event handler for our own events. This is actually used for queue
	 * updating timed callback events (however, the events emitted from
	 * this class may also be intercepted by external watchers if
	 * interested). This function is called from event loop and should never
	 * be called directly.
	 *
	 * @param evt     Event that happened
	 */
	void onClientListEvent(ClientList *, ClientListEvt evt);

	/**
	 * Re-generates m_queue vector (from m_queued set), ordered by the
	 * client's scores. Sets each client's m_uploadClient member's queue
	 * ranking based on their position in the resulting m_queue vector.
	 *
	 * This function is called once per every X seconds, where X is the
	 * queue update interval. The purpose of this is to be able to send
	 * queue rankings to remote clients (thus each client's member m_qr
	 * value will indicate it's current position in the queue), as well as
	 * for picking up highest-ranking clients from the queue when we want
	 * to start sending data to them.
	 */
	void updateQueue();

	/**
	 * Checks if we are uploading ineffectivly (e.g. current global upload
	 * rate is significently below what is the limit), and then attempts
	 * to open up another upload slot.
	 */
	void checkOpenMoreSlots();

	/**
	 * Locates a specific client. by ID and TCP port
	 *
	 * @param addr     ClientID and TCP port of the searched client
	 * @return         Pointer to the client, or 0 if not found
	 */
	Client* findClient(IPV4Address addr);

	/**
	 * Locate a specific client, by ID and UDP port
	 *
	 * @param addr     ClientID and UDP port of the searched client
	 * @return         Pointer to the client, or 0 if not found
	 */
	Client* findClientByUdp(IPV4Address addr);

	/**
	 * Event handler for ClientUDP socket events.
	 */
	void onUdpData(ED2KUDPSocket *src, SocketEvent evt);

	/**
	 * Checks if the passed message should be filtered.
	 *
	 * @param msg          Message to check
	 * @returns            True if the message should be filtered
	 */
	bool msgFilter(const std::string &msg) const;

	/**
	 * @returns Recommended number of open upload slots
	 */
	uint32_t getSlotCount() const;

	/**
	 * Event handler for all shared files events, checks and destroys
	 * all clients that were requesting the file when SF_DESTROY
	 * is emitted.
	 */
	void onSFEvent(SharedFile *sf, int evt);

	/**
	 * Handles configuration changes; rebinds TCP / UDP ports if needed
	 */
	void configChanged(const std::string &key, const std::string &value);
private:
	/**
	 * List of all clients we have alive.
	 */
	boost::scoped_ptr<Detail::CList> m_clients;

	/**
	 * This is the queued clients list. The contents of this list are
	 * ordered by the clients score, generated every X seconds. Note that
	 * every X seconds, this container is cleared and re-filled from
	 * m_queuedClients set. Also note that there may be dangling pointers
	 * existing in this list, in worst-case scenario for up to X seconds
	 * (e.g. until after next full regen). As such, if you access this
	 * container, you need to make sure the pointer you just got is still
	 * valid by looking it up from m_queuedClients set. If it is not found
	 * there, the pointer is invalid and should be discarded.
	 */
	std::list<Client*> m_queue;

	/**
	 * This set contains all clients which have m_uploadClient member
	 * pointer and are not currently uploading. This set is used as base for
	 * building up the actual upload queue into m_queue list every X
	 * seconds. The purpose of this set is to have a reference container in
	 * which we can actually erase elements using fast integer-based lookups
	 * (which we couldn't do in m_queue list since that one is ordered by
	 * score).
	 *
	 * Note that we could achive the same purpose by using m_clients
	 * container, however, m_clients also contains downloadclients, so
	 * it would cause many more loops in there to find the queued clients
	 * than it is to find them here. Also, this set is a more static entity
	 * than m_queue member (which is completely regenerated often).
	 */
	std::set<Client*> m_queued;

	/**
	 * This set contains all clients that are currently in uploading state,
	 * e.g. sending data. The number of elements allowed in this list
	 * depends on various factors, however it should never go below 1.
	 */
	std::set<Client*> m_uploading;

	/**
	 * Main ED2K listener socket. This is where all incoming TCP clients
	 * connect to. By default it is set to listen on port 4662, however it
	 * may be changed in configuration, under key "TCP Port".
	 *
	 * Note that if you modify the listener address, you need to also call
	 * ClientList::init() to restart the listener.
	 */
	ED2KServerSocket *m_listener;

	/**
	 * Input buffer for UDP data; the size is defined by UDP_BUFSIZE
	 */
	boost::scoped_array<char> m_udpBuffer;

	/**
	 * Inter-client messages that should be filtered.
	 */
	std::vector<std::string> m_msgFilter;
};

} // end namespace Donkey

#endif
