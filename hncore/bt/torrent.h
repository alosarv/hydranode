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
 * \file torrent.h       Interface for Torrent class
 */

#ifndef __BT_TORRENT_H__
#define __BT_TORRENT_H__

#include <hnbase/ssocket.h>
#include <hnbase/hostinfo.h>
#include <hncore/bt/torrentinfo.h>
#include <hncore/bt/types.h>
#include <hncore/bt/client.h>
#include <boost/utility.hpp>

namespace Bt {

enum TorrentEvent {
	EVT_DESTROY = 1
};

class Tracker;

/**
 * Torrent class represents a single torrent, which is either being downloaded
 * or seeded currently. It wraps together three components - TorrentFile,
 * PartialTorrent (optional) and Clients (peers) set. It also is in charge of
 * tracker connections (if the torrent has tracker).
 *
 * Client objects are owned by this torrent when they are attached to it. It
 * also handles a number of signals from Client objects, namely connectionLost
 * and handshakeReceived.
 */
class Torrent : public Trackable {
public:
	DECLARE_EVENT_TABLE(Torrent*, int);

	/**
	 * Construct a Torrent from a TorrentFile and torrent info data.
	 *
	 * @param file         File this torrent represents
	 * @param info         Contains extra information about this torrent
	 */
	Torrent(SharedFile *file, const TorrentInfo &info);

	/**
	 * Destroys this torrent and all related clients.
	 */
	~Torrent();

	/**
	 * @name Generic accessors
	 */
	//!@{
	Hash<SHA1Hash> getInfoHash()   const { return m_info.getInfoHash();  }
	uint64_t       getChunkSize()  const { return m_info.getChunkSize(); }
	uint32_t       getChunkCnt()   const { return m_info.getChunkCnt();  }
	std::string    getName()       const { return m_info.getName();      }
	uint64_t       getUploaded()   const { return m_uploaded;            }
	uint64_t       getDownloaded() const { return m_downloaded;          }
	PartData*      getPartData()   const { return m_partData;            }
	//!@}

	/**
	 * Add a client to this Torrent; this class takes ownership of the
	 * object.
	 *
	 * @param c      Client to be added.
	 *
	 * \note If the client doesn't belong to this torrent (based on the
	 *       info_hash value sent in handshake), it may be immediately
	 *       destroyed.
	 */
	void addClient(Client *c);

	/**
	 * Add a request to the request queue
	 *
	 * @param r         Request to be added
	 */
	void addRequest(const Client::Request &r);

	/**
	 * Remove a specific request from the request listing
	 *
	 * @param r        Request to be removed
	 */
	void delRequest(const Client::Request &r);

	/**
	 * Get a request; returns the oldest-added request, with the logic being
	 * that if a request hasn't been answered for a long time, it's most
	 * likely a slow source, and this method will allow re-allocating the
	 * request to a faster source.
	 *
	 * @param chunks  Chunks the client has
	 * @returns       A request to be sent
	 */
	Client::Request getRequest(const std::vector<bool> &chunks);

	/**
	 * Clear all requests with the specified index
	 *
	 * @param index         Index to be cleared
	 */
	void clearRequests(uint32_t index);

	/**
	 * Try to unchoke one of the interested clients and start uploading
	 *
	 * @param c          Optionally, client that called this
	 * @param dontChoke  Set to true if @c should continue uploading
	 * @returns true if uploading was started, false otherwise
	 *
	 * The purpose of the two parameters is that Client::sendChoke
	 * method calls this method, and passes those two pointers; if
	 * this method decides that the client should continue uploading,
	 * it will set dontChoke pointer to true.
	 */
	bool tryUnchoke(Client *c = 0, bool *dontChoke = 0);

	/**
	 * @returns Number of peers connected for this torrent
	 */
	size_t getPeerCount() const { return m_clients.size(); }

	//! Increase uploaded data counter (this session)
	void addUploaded(uint32_t amount);

	//! Increase downloaded data counter (this session)
	void addDownloaded(uint32_t amount) { m_downloaded += amount; }

	/**
	 * Returns availability bitfield for this torrent
	 */
	std::string getBitfield() const { return m_bitField; }
private:
	Torrent(const Torrent&);
	Torrent& operator=(const Torrent&);

	/**
	 * @name Event handlers
	 */
	//!@{
	void onPartDataEvent(PartData *file, int evt);
	void onSharedFileEvent(SharedFile *file, int evt);
	//!@}

	/**
	 * @name Signal handlers
	 */
	//!@{
	void handshakeReceived(Client *c);
	void connectionLost(Client *c);
	void onChunkVerified(PartData *file, uint64_t csz, uint64_t chunk);
	//!@}

	/**
	 * Creates and attaches a peer to this torrent.
	 *
	 * @param addr       Peer ip/port info
	 */
	void createClient(IPV4Address addr);

	//! Initializes m_bitField member and connects onChunkVerified signal
	void initBitfield();

	SharedFile                  *m_file;      //!< Seeded file
	PartData                    *m_partData;  //!< Downloaded file(optional)
	TorrentInfo                  m_info;      //!< Info about this torrent

	std::set<Tracker*>            m_trackers;

	uint64_t    m_uploaded;    //!< How much we have downloaded this session
	uint64_t    m_downloaded;  //!< How much we have uploaded this session

	std::set<Client*> m_clients; //!< Peers attached to this torrent

	//! Availability bitfield for this torrent
	std::string m_bitField;

	//! All outgoing requests
	std::deque<boost::weak_ptr< ::Detail::LockedRange> > m_sharedReqs;
};

}

#endif
