/*
 *  Copyright (C) 2006 Alo Sarv <madcat_@users.sourceforge.net>
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

#ifndef __BT_TRACKER_H__
#define __BT_TRACKER_H__

#include <hnbase/ssocket.h>
#include <hnbase/hostinfo.h>
#include <hncore/bt/types.h>
#include <hncore/bt/torrentinfo.h>
#include <boost/utility.hpp>

class PartData;

namespace Bt {

class Tracker : public boost::noncopyable, public Trackable {
public:
	typedef std::vector<IPV4Address>::const_iterator CAIter;
	typedef std::vector<IPV4Address>::iterator AIter;

	Tracker(
		const std::string &host, const std::string &url,
		uint16_t port, TorrentInfo info
	);

	boost::signal<void (IPV4Address)> foundPeer;
	boost::signal<uint64_t ()>        getUploaded;
	boost::signal<uint64_t ()>        getDownloaded;
	boost::signal<PartData* ()>       getPartData;

	std::string getName()        const { return m_info.getName(); }
	std::string getHost()        const { return m_host;           }
	std::string getUrl()         const { return m_url;            }
	uint16_t    getPort()        const { return m_port;           }
	CAIter      addrBegin()      const { return m_addrs.begin();  }
	CAIter      addrEnd()        const { return m_addrs.end();    }
	uint32_t    getInterval()    const { return m_interval;       }
	uint32_t    getMinInterval() const { return m_minInterval;    }
	std::string getId()          const { return m_id;             }
	uint32_t    getCompleteSrc() const { return m_completeSrc;    }
	uint32_t    getPartialSrc()  const { return m_partialSrc;     }
private:
	TorrentInfo m_info;

	std::string m_host; //!< Tracker host part, e.g. www.tracker.com
	std::string m_url;  //!< Tracker URL part, e.g. /announce
	uint16_t    m_port; //!< Tracker port, e.g. 80
	std::vector<IPV4Address> m_addrs;  //!< Tracker IP addresses
	std::vector<IPV4Address>::iterator m_curAddr; //!< Active addr

	uint32_t    m_interval;    //!< Tracker reask interval
	uint32_t    m_minInterval; //!< Minimum tracker reask interval
	std::string m_id;          //!< ID sent by tracker
	uint32_t    m_completeSrc; //!< Number of complete sources
	uint32_t    m_partialSrc;  //!< Number of partial sources

	boost::scoped_ptr<TcpSocket> m_socket; //!< Tracker connection
	std::string  m_inBuffer;               //!< Socket input buffer

	// networking related functions
	void hostLookup();
	void hostResolved(HostInfo info);
	void connect(IPV4Address addr);
	void onSocketEvent(TcpSocket *sock, SocketEvent event);
	void parseBuffer();
	void parseContent(const std::string &content);
	void sendGetRequest();
};

} // end namespace Bt

#endif
