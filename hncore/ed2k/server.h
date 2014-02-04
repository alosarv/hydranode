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
 * \file server.h Interface for Detail::Server class
 */

#ifndef __ED2K_SERVER_H__
#define __ED2K_SERVER_H__

#include <hnbase/fwd.h>
#include <hnbase/event.h>
#include <hnbase/ipv4addr.h>
#include <hnbase/object.h>
#include <hncore/fwd.h>
#include <hncore/ed2k/fwd.h>

namespace Donkey {
namespace Detail {

// Server class
// ------------
/**
 * Server represents one server object. A server in eDonkey2000 network
 * has a set of properties, however internally only the address of
 * the server is used - the remainder of properties are used only for
 * showing to user.
 *
 * Server object is owned by ServerList object, the destructor has been
 * protected to enforce that relationship.
 *
 * Note: TCP flags are not stored in server.met according to eMule.
 */
class Server : public Object, public Trackable {
public:
	DECLARE_EVENT_TABLE(Server*, int);

	//! Construct using IP and port
	Server(IPV4Address addr);

	//! Construct by reading data from stream
	Server(std::istream &i);

	//! Write the object to stream
	friend std::ostream& operator<<(
		std::ostream &o, const Server &s
	);

	//! @name Getters
	//@{
	IPV4Address getAddr()               const { return m_addr;           }
	std::string getName()               const { return m_name;           }
	std::string getDesc()               const { return m_desc;           }
	std::string getDynIp()              const { return m_dynip;          }
	std::string getVersion()            const { return m_version;        }
	uint32_t    getFailedCount()        const { return m_failedCount;    }
	uint32_t    getPreference()         const { return m_preference;     }
	uint32_t    getPing()               const { return m_ping;           }
	uint32_t    getLastPing()           const { return m_lastPing;       }
	uint32_t    getMaxUsers()           const { return m_maxUsers;       }
	uint32_t    getSoftLimit()          const { return m_softLimit;      }
	uint32_t    getHardLimit()          const { return m_hardLimit;      }
	uint32_t    getUdpFlags()           const { return m_udpFlags;       }
	uint32_t    getTcpFlags()           const { return m_tcpFlags;       }
	uint32_t    getUsers()              const { return m_users;          }
	uint32_t    getFiles()              const { return m_files;          }
	uint32_t    getLowIdUsers()         const { return m_lowIdUsers;     }
	std::vector<uint16_t> getAuxPorts() const { return m_auxPorts;       }
	uint64_t    getLastUdpQuery()       const { return m_lastUdpQuery;   }
	bool        pingInProgress()        const { return m_pingInProgress; }
	uint32_t    getChallenge()          const { return m_challenge;      }
	SearchPtr   currentSearch()         const { return m_globSearch;     }

	//! ED2K Server UDP port is always TCP + 4
	uint16_t    getUdpPort()  const { return m_addr.getPort() + 4;       }
	bool supportsGetSources() const { return m_udpFlags & FL_GETSOURCES; }
	bool supportsGetFiles()   const { return m_udpFlags & FL_GETFILES;   }
	bool supportsUdpSearch()  const { return m_udpFlags & FL_GETFILES;   }
	//@}

	//! @name Setters
	//@{
	void setAddr(IPV4Address addr) { m_addr = addr; notify(); }
	void setName(const std::string &name) {
		Object::setName(name);
		m_name = name;
		notify();
	}
	void setDesc(const std::string &desc)    { m_desc = desc;    notify(); }
	void setDynIp(const std::string &dyndip) { m_dynip = dyndip; notify(); }
	void setVersion(uint32_t ver)            { m_version = ver;  notify(); }
	void setFailedCount(uint32_t count) { m_failedCount = count; notify(); }
	void addFailedCount()                 { ++m_failedCount;     notify(); }
	void setPing(uint32_t ping)           { m_ping = ping;       notify(); }
	void setLastPing(uint32_t ping)       { m_lastPing = ping;   notify(); }
	void setMaxUsers(uint32_t users)      { m_maxUsers = users;  notify(); }
	void setSoftLimit(uint32_t limit)     { m_softLimit = limit; notify(); }
	void setHardLimit(uint32_t limit)     { m_hardLimit = limit; notify(); }
	void setUdpFlags(uint32_t flags)      { m_udpFlags = flags;  notify(); }
	void setTcpFlags(uint32_t flags)      { m_tcpFlags = flags;  notify(); }
	void addAuxPort(uint16_t port) { m_auxPorts.push_back(port); notify(); }
	void setUsers(uint32_t users)         { m_users = users;     notify(); }
	void setFiles(uint32_t files)         { m_files = files;     notify(); }
	void setLowIdUsers(uint32_t num)      { m_lowIdUsers = num;  notify(); }

	void setLastUdpQuery(uint64_t time)   { m_lastUdpQuery = time; }
	void setPingInProgress(bool v)        { m_pingInProgress = v;  }
	void setChallenge(uint32_t c)         { m_challenge = c;       }
	void setCurrentSearch(SearchPtr p)    { m_globSearch = p;      }
	//@}

	virtual uint32_t getDataCount() const;
	virtual std::string getData(uint32_t num) const;
	virtual std::string getFieldName(uint32_t num) const;
private:
	friend class ::Donkey::ServerList;
	friend bool operator<(const Server &x, const Server &y) {
		return x.m_addr < y.m_addr;
	}

	//! Values found in m_udpFlags bitfield
	enum ED2K_ServerUdpFlags {
		FL_GETSOURCES  = 0x01, //!< Supports UDP GetSources
		FL_GETFILES    = 0x02, //!< Supports UDP GetFiles/Search
		FL_NEWTAGS     = 0x08, //!< Supports NewTags
		FL_UNICODE     = 0x10, //!< Supports Unicode
		FL_GETSOURCES2 = 0x20  //!< Supports extended GetSources
	};

	Server();                    //!< Forbidden
	~Server();                   //!< Allowed by ServerList

	//! @name Data
	//@{
	IPV4Address m_addr;          //!< Address of the server
	std::string m_name;          //!< Name of the server
	std::string m_desc;          //!< Description
	std::string m_dynip;         //!< Dynamic ip address
	std::string m_version;       //!< Version number
	uint32_t m_failedCount;      //!< Number of times it has failed
	uint32_t m_preference;       //!< Priority
	uint32_t m_ping;             //!< Ping
	uint32_t m_lastPing;         //!< Last ping
	uint32_t m_maxUsers;         //!< Max allowed users
	uint32_t m_softLimit;        //!< Soft files limit
	uint32_t m_hardLimit;        //!< Hard files limit
	uint32_t m_udpFlags;         //!< UDP flags
	uint32_t m_tcpFlags;         //!< TCP flags
	uint32_t m_users;            //!< Current online users
	uint32_t m_files;            //!< Current online files
	uint32_t m_lowIdUsers;       //!< Current online lowId users
	std::vector<uint16_t> m_auxPorts; //!< Auxiliary ports

	uint64_t  m_lastUdpQuery;    //!< Time of last UDP query
	bool      m_pingInProgress;  //!< UDP Ping is in progress
	uint32_t  m_challenge;       //!< Last UDP query challenge
	SearchPtr m_globSearch;      //!< Current search, if any
	//@}

	//! Parses comma-separated list of ports and adds them
	void addAuxPorts(const std::string &ports);
};

} // end namespace Detail
} // end namespace Donkey

#endif
