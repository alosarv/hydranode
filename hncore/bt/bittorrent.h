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
 * \file bittorrent.h       Interface for BitTorrent main module class
 */

#ifndef __BT_BITTORRENT_H__
#define __BT_BITTORRENT_H__

#include <hncore/fwd.h>
#include <hncore/modules.h>
#include <hncore/bt/types.h>
#include <hnbase/hash.h>

namespace Bt {

/**
 * BitTorrent class is in charge of owning Torrent objects, as well as listener
 * socket. When remote clients connect, this class constructs the Client object,
 * and sets up signal handler for the Client. Once the client has sent it's
 * handshake, BitTorrent attaches the client to the corresponding Torrent
 * object. If no corresponding Torrent object is found, the client is destroyed.
 *
 * BitTorrent owns incoming client objects until they send handshake; when it
 * attaches the Client to an existing Torrent, it gives away ownership of the
 * object, and thus also removes it from it's internal containers.
 */
class BTEXPORT BitTorrent : public ModuleBase {
	DECLARE_MODULE(BitTorrent, "bt");
public:
	virtual bool onInit();
	virtual int onExit();
	virtual std::string getDesc() const { return "BitTorrent"; }

	/**
	 * @returns PeerId - unique identifier for this client/runtime.
	 *          Generated randomly on startup, and includes client name/
	 *          version information. This is sent to other peers in
	 *          handshake packet.
	 */
	std::string getId() const { return m_id; }

	/**
	 * @returns Unique (per session) key that is only sent to tracker via
	 *          'key' parameter. This should never be shared with other
	 *           peers.
	 */
	std::string getKey() const { return m_key; }

	/**
	 * @returns Location where chunk / .torrent files are stored.
	 */
	boost::filesystem::path getCacheDir() const { return m_cacheDir; }

	/**
	 * @returns The TCP port where we are listening for connections.
	 */
	uint16_t getPort() const { return m_port; }

	/**
	 * \brief Creates a new torrent to be downloaded.
	 *
 	 * @param ifs         Data stream containing the .torrent metainfo
 	 * @param dest        Destination location for started downloads
	 *
	 * It takes care of all the gory details of constructing sub-objects as
	 * well as TorrentFile, PartialTorrent and Torrent objects.
	 *
	 * Each of the created objects has MetaData object attached to it, and
	 * in the MetaData::customData fields, an entry with the following
	 * format:
	 * \code
	 * btorrent:HASH:OFFSET
	 * \endcode
	 *  Where:
	 *  - HASH is SHA1 checksum of the metainfo file
	 *  - OFFSET indicates zero-based offset of the file within the torrent
	 *
	 * \note This method doesn't return the created torrent for the same
	 *       reason why FilesList::createDownload() doesn't - in order to
	 *       enforce user code to find the newly-created files using other
	 *       means.
	 */
	void createTorrent(std::istream &ifs, boost::filesystem::path dest);

	/**
	 * Provided for convenience, this method reads metainfo data directly
	 * from the passed file location.
	 *
	 * @param file       Location of the .torrent file
	 * @param dest       Destination location for started downloads
	 */
	void createTorrent(
		const boost::filesystem::path &file,
		boost::filesystem::path dest
	);
protected:
	virtual void openUploadSlot();
private:
	//! ClientID sent to tracker and other clients
	std::string m_id, m_key;

	//! Location of cached .torrent files
	boost::filesystem::path m_cacheDir;
	//! Incoming connections listener
	boost::scoped_ptr<TcpListener> m_listener;

	/**
	 * Map of torrents, keyed by SHA1 checksum of the corresponding
	 * metainfo file
	 */
	std::map<Hash<SHA1Hash>, Torrent*> m_torrents;

	/**
	 * New incoming clients that haven't been attached to any Torrent
	 * object yet.
	 */
	std::set<Client*> m_newClients;

	//! Iterator for the above set
	typedef std::map<Hash<SHA1Hash>, Torrent*>::iterator Iter;

	/**
	 * \name Signal / Event handlers
	 */
	//!@{
	void onHandshakeReceived(Client *client);
	void onConnectionLost(Client *client);
	void onIncoming(TcpListener *server, SocketEvent evt);
	bool downloadLink(const std::string &link);
	bool downloadFile(const std::string &contents);
	void onSFEvent(SharedFile *sf, int evt);
	void onTorrentEvent(Torrent *tor, int evt);
	void configChanged(const std::string &key, const std::string &val);
	void adjustLimits();
	//!@}

	std::map<Hash<SHA1Hash>, boost::filesystem::path> m_torrentDb;
	typedef std::map<
		Hash<SHA1Hash>, boost::filesystem::path
	>::iterator DBIter;

	void initTorrentDb(const boost::filesystem::path &path);
	void initKnownTorrents(const boost::filesystem::path &confDir);
	void initFiles();
	void saveKnownTorrents(const boost::filesystem::path &confDir);

	//! Known torrents and their data locations, .torrent -> path
	std::map<std::string, boost::filesystem::path> m_known;

	/**
	 * Workaround for clients who send handshake so fast that we parse it
	 * right in Client constructor, emitting handshakeReceived signal,
	 * before we can connect our signal handler or insert the client to
	 * m_newClients map.
	 */
	Client *m_curClient;

	//! Listening port
	uint16_t m_port;
};

}

#endif
