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

#ifndef __BT_CLIENT_H__
#define __BT_CLIENT_H__

#include <hncore/bt/types.h>
#include <hncore/fwd.h>
#include <hncore/baseclient.h>
#include <hnbase/hash.h>
#include <queue>

namespace Bt {

class Client : public BaseClient, public Trackable {
public:
	Client(TcpSocket *sock);
	Client(IPV4Address addr);
	~Client();

	boost::signal<void (Client*)> handshakeReceived;
	boost::signal<void (Client*)> connectionLost;

	// outgoing packets
	void sendPing();
	void sendChoke();
	void sendInterested();
	void sendUnchoke();
	void sendUninterested();
	void sendHave(uint32_t index);
	void sendRequest(uint32_t index, uint32_t offset, uint32_t length);
	void sendCancel(uint32_t index, uint32_t offset, uint32_t length);
	void sendPiece(uint32_t index, uint32_t offset,const std::string &data);
	void sendBitfield();

	bool isChoking()    const { return m_isChoking;    }
	bool isInterested() const { return m_isInterested; }
	bool amChoking()    const { return m_amChoking;    }
	bool amInterested() const { return m_amInterested; }

	Hash<SHA1Hash> getInfoHash()  const { return m_infoHash; }
	std::string    getPeerId()    const { return m_peerId; }
	IPV4Address    getAddr()      const { return m_addr; }
	bool           isFullSource() const { return m_bitField.size(); }

	void setFile(SharedFile *file);
	void setFile(PartData *file);
	void setTorrent(Torrent *t);

	struct Request {
		Request();
		Request(uint64_t index, uint64_t offset, uint64_t length);
		Request(::Detail::LockedRangePtr r, Torrent *t);

		friend bool operator<(const Request &x, const Request &y) {
			if (x.m_index == y.m_index) {
				return x.m_offset < y.m_offset;
			}
			return x.m_index < y.m_index;
		}
		friend bool operator==(const Request &x, const Request &y) {
			return  x.m_index == y.m_index   &&
				x.m_offset == y.m_offset &&
				x.m_length == y.m_length;
		}
		friend bool operator!=(const Request &x, const Request &y) {
			return !(x == y);
		}
		uint64_t m_index;
		uint64_t m_offset;
		uint64_t m_length;

		::Detail::LockedRangePtr m_locked;
	};

	// implement BaseClient interface
	virtual uint32_t getUploadSpeed()       const;
	virtual uint32_t getDownloadSpeed()     const;
	virtual uint64_t getSessionUploaded()   const;
	virtual uint64_t getSessionDownloaded() const;
	virtual uint64_t getTotalUploaded()     const;
	virtual uint64_t getTotalDownloaded()   const;
	virtual std::string getSoft()           const { return m_clientSoft;   }
	virtual std::string getSoftVersion()    const { return m_clientVersion;}
private:
	Client(const Client&);
	Client& operator=(const Client&);

	void onSocketEvent(TcpSocket *sock, SocketEvent evt);
	void parseBuffer();
	void parsePacket(BEIStream &i, uint32_t len);
	void sendHandshake();
	void sendNextChunk();
	void checkNeedParts();
	void sendRequests();

	// incoming packets
	void onPing();
	void onChoke();
	void onUnchoke();
	void onInterested();
	void onUninterested();
	void onHave(uint32_t index);
	void onBitfield(const std::string &bits);
	void onRequest(uint32_t index, uint32_t offset, uint32_t length);
	void onPiece(uint32_t index, uint32_t offset, const std::string &data);
	void onCancel(uint32_t index, uint32_t offset, uint32_t length);

	void onVerified(PartData *file, uint32_t chunkSize, uint32_t chunk);
	void onDestroyed(PartData *file);

	void updateSignals();
	void parsePeerId(const std::string &peerId);

	boost::scoped_ptr<TcpSocket> m_socket;

	std::string    m_peerId;          //!< Remote peer ID
	Hash<SHA1Hash> m_infoHash;        //!< Torrent info hash
	IPV4Address    m_addr;            //!< IP of the client
	std::string    m_clientSoft;      //!< Client's software / program
	std::string    m_clientVersion;   //!< Version of client's software

	/**
	 * @name State variables
	 */
	//!@{
	bool              m_isChoking;     //!< The client is choking us
	bool              m_isInterested;  //!< The client is interested
	bool              m_amChoking;     //!< We are choking the client
	bool              m_amInterested;  //!< We are interested
	bool              m_handshakeSent; //!< We sent handshake
	bool              m_needParts;     //!< We need parts from client
	std::vector<bool> m_bitField;      //!< Parts the client has

	std::string          m_inBuffer;    //!< incoming data buffer
	std::list<Request>   m_requests;    //!< incoming requests
	std::deque<Request>  m_outRequests; //!< outgoing chunk requests

	::Detail::UsedRangePtr m_usedRange; //!< currently used range
	//!@}

	SharedFile     *m_file;             //!< seeded file
	PartData       *m_partData;         //!< downloaded file
	Torrent        *m_torrent;          //!< torrent the client belongs to

	bool m_sourceMaskAdded;             //!< If we added srcmask to file

	/**
	 * Client's speedmeter is connected to whichever (physical) file it's
	 * currently downloading; since that can change at any point during
	 * transfer, this connection is disconnected and re-attached to
	 * different file when crossing file boundaries.
	 */
	boost::signals::connection m_currentSpeedMeter;

	/**
	 * Client's upload speedmeter connected to whatever (physical) file
	 * it's currently uploading; since that can change at any point during
	 * transfer, this connection is disconnected and re-attached to
	 * different file when crossing file boundaries.
	 */
	boost::signals::connection m_currentUploadMeter;
};

}

#endif
