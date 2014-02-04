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

#ifndef __ED2K_PACKETS_H__
#define __ED2K_PACKETS_H__

/**
 * \file packets.h
 * This is an implementation header for ED2K protocol parser. This file should
 * never be included directly from user code - instead include "parser.h".
 *
 * Declares all supported packet objects, both incoming and outgoing, in
 * ED2KPacket namespace to avoid name clashes.
 *
 * Implementation note: The packet objects, implemented in packets.cpp, should
 * be as smart as possible and require as few usage arguments as possible. This
 * is done to minimize the usage complexity of these packet objects. Only
 * arguments to constructors should be those that we cannot possibly detect
 * ourselves during packet generation. The side-effect of this is that the
 * implementation file will need to include a large amount of ed2k module
 * headers, however the usage simplicity is well worth it.
 */

#include <hncore/ed2k/opcodes.h>          // For opcodes
#include <hncore/ed2k/ed2kfile.h>         // For ED2KFile and ED2KFile::Rating
#include <hncore/ed2k/ed2ksearch.h>       // For ED2KSearchResult
#include <hncore/ed2k/ed2ktypes.h>        // For ED2KHashSet
#include <hncore/ed2k/publickey.h>        // for PublicKey
#include <hnbase/osdep.h>                 // For types
#include <hnbase/ipv4addr.h>              // For ipv4address
#include <hnbase/hash.h>                  // For Hash<MD4Hash>
#include <hnbase/range.h>                 // For Range32
#include <boost/tuple/tuple.hpp>

namespace Donkey {

/**
 * Client software
 */
enum ED2K_ClientSoftware {
	CS_EMULE         = 0x00,  //!< Official eMule
	CS_CDONKEY       = 0x01,  //!< CDonkey
	CS_LXMULE        = 0x02,  //!< lmule, xmule
	CS_AMULE         = 0x03,  //!< amule
	CS_SHAREAZA      = 0x04,  //!< shareaza
	CS_SHAREAZA_NEW  = 0x28,  //!< shareaza version 2.2 and newer
	CS_EMULEPLUS     = 0x05,  //!< emuleplus
	CS_HYDRANODE     = 0x06,  //!< hydranode
	CS_MLDONKEY_NEW2 = 0x0a,  //!< second new mldonkey
	CS_LPHANT        = 0x14,  //!< lphant
	CS_HYBRID        = 0x3c,  //!< eDonkey2000 Hybrid
	CS_DONKEY        = 0x3d,  //!< eDonkey2000
	CS_MLDONKEY      = 0x3e,  //!< original mldonkey
	CS_OLDEMULE      = 0x3f,  //!< old eMule
	CS_UNKNOWN       = 0x36,  //!< unknown
	CS_MLDONKEY_NEW  = 0x98   //!< first new mldonkey
};

//! Versions
enum ED2K_Versions {
	VER_EDONKEY = 0x3c,
	VER_OWN = (
		(CS_HYDRANODE << 24) | (APPVER_MAJOR << 17) |
		(APPVER_MINOR << 10) | (APPVER_PATCH << 7)
	)
};

/**
 * ED2KPacket namespace contains all packet objects supported by the parser.
 */
namespace ED2KPacket {

extern uint64_t getOverheadUp();
extern uint64_t getOverheadDn();
extern void addOverheadDn(uint32_t amount);

//! Exception class, thrown when invalid packets are found during parsing.
class InvalidPacket : public std::runtime_error {
public:
	InvalidPacket(const std::string &what);
};

/**
 * Abstract base Packet class defines a virtual destructor, and few convenience
 * methods for usage by derived classes. This class is never used directly,
 * instead one of the concrete derived classes are used.
 */
class Packet {
protected:
	/**
	 * The only allowed constructor.
	 *
	 * @param proto       Protocol based on which to create the packet.
	 *                    currently supports PR_ED2K, PR_EMULE and PR_ZLIB
	 */
	Packet(uint8_t proto = PR_ED2K);

	/**
	 * Pure virtual destructor.
	 */
	virtual ~Packet() = 0;

	//! Keeps the protocol opcode passed to the constructor for later
	//! outputting (see makePacket method).
	uint8_t m_proto;

	/**
	 * Makes packet, finalizing the packet generation. This function
	 * writes the packet header, and (optionally) compresses the packet
	 * data if m_proto == PR_ZLIB
	 *
	 * @param data     Data to make packet of. Assumes data[0] is opcode
	 * @param hexDump  If set to true, the packet data is printed to stdout
	 * @return         Packet ready for sending to target
	 */
	std::string makePacket(const std::string &data, bool hexDump = false);
};


                        /***********************/
                        /*  Client <-> Server  */
                        /***********************/

/**
 * LoginRequest packet is sent to server after establishing the connection to
 * let the server know we are there.
 *
 * Usage: Client -> Server
 */
class LoginRequest : public Packet {
public:
	LoginRequest(uint8_t proto = PR_ED2K);
	operator std::string();
};

/**
 * ServerMessage is a free-form string message sent by servers after successful
 * login. This is a kind of MOTD from servers. Can include newlines and be
 * of arbitary length (up to numeric_limits<uin16_t>)
 *
 * Usage: Server -> Client
 */
class ServerMessage : public Packet {
public:
	ServerMessage(std::istream &i);

	std::string getMsg() const { return m_msg; }
private:
	std::string m_msg;
};

/**
 * ServerStatus is a packet sent by servers after establishing a connection,
 * containing number of users and files currently on the server.
 *
 * Usage: Server -> Client
 */
class ServerStatus : public Packet {
public:
	ServerStatus(std::istream &i);

	uint32_t getUsers() const { return m_users; }
	uint32_t getFiles() const { return m_files; }
private:
	uint32_t m_users;
	uint32_t m_files;
};

/**
 * IdChange packet is sent by servers after establishing a connection with the
 * server to notify the client of their new ID. In eDonkey2000 network, ID's
 * below 0x00ffffff are considered "low-id" and penalized. The ID is actually
 * the ip address of the client, in network byte order, which the server detects
 * by making a client<->client connection to our listening socket. If the server
 * is unable to connect to our listening socket, it sends us lowID message and
 * assigns us a low (< 0x00ffffff) ID.
 *
 * New lugdunum (16.44+) servers also send additional u32 containing supported
 * features bitfield.
 *
 * Usage: Server -> Client
 */
class IdChange : public Packet {
public:
	IdChange(std::istream &i);

	uint32_t getId()    const { return m_id; }
	uint32_t getFlags() const { return m_flags; }
private:
	uint32_t m_id;
	uint32_t m_flags; //!< Lugdunum 16.44+ servers send supported features
};

/**
 * Sent to server, this requests the server to send us its current known servers
 * list.
 *
 * Usage:  Client -> Server
 */
class GetServerList : public Packet {
public:
	GetServerList(uint8_t proto = PR_ED2K);
	operator std::string();
};

/**
 * ServerIdent is a response from server to GetServerList packet, and includes
 * information about the server.
 *
 * Usage:  Server -> Client
 */
class ServerIdent : public Packet {
public:
	ServerIdent(std::istream &i);

	Hash<MD4Hash> getHash() const { return m_hash; }
	IPV4Address   getAddr() const { return m_addr; }
	std::string   getName() const { return m_name; }
	std::string   getDesc() const { return m_desc; }
private:
	Hash<MD4Hash> m_hash;               //!< Server hash
	IPV4Address m_addr;                 //!< Server ip address/port
	std::string m_name;                 //!< Server name
	std::string m_desc;                 //!< Server description
};

/**
 * ServerList packet is sent by servers in response to GetServerList packet,
 * and contains a list of known servers to the source.
 *
 * Usage:  Server -> Client
 */
class ServerList : public Packet {
public:
	ServerList(std::istream &i);

	uint32_t    getCount()            const { return m_servers.size(); }
	IPV4Address getServer(uint32_t n) const { return m_servers.at(n);  }
private:
	std::vector<IPV4Address> m_servers;
};

/**
 * OfferFiles packet is used in eDonkey2000 network to publish shared files to
 * currently connected server. The packet contains a list of files we want to
 * make available for others to download from us.
 *
 * eDonkey2000 network sets some limitations on the files that can be made
 * available. Namely, the file size must be <= numeric_limits<uint32_t>::max(),
 * because sizes are used as 32-bit integers in ed2k network. This limits file
 * size to roughly 4gb. Additionally, it is generally not recommended to publish
 * more than 300 files. Prefered files should be large (iso, avi, etc) files,
 * instead of small mp3 collections. Small files should be packet together
 * into archives for publishing. However, this is user-related issue and is not
 * related to the implementation of this class, but it is good to mention it
 * too.
 *
 * The semantics regarding this packet as as follows:
 *
 * *) A full list of all shared files should be sent when server connection has
 *    been established.
 * *) A partial list containing only added files should be sent whenever a new
 *    shared file has been added.
 * *) An empty list should be sent on regular intervals as keep-alive packet
 *    to lugdunum servers.
 *
 * Usage: Client -> Server
 */
class OfferFiles : public Packet {
public:
	OfferFiles(uint8_t proto = PR_ED2K);
	OfferFiles(boost::shared_ptr<ED2KFile> f, uint8_t proto = PR_ED2K);
	void push(boost::shared_ptr<ED2KFile> toAdd) {
		m_toOffer.push_back(toAdd);
	}
	operator std::string();
private:
	std::vector< boost::shared_ptr<ED2KFile> > m_toOffer;
	typedef std::vector< boost::shared_ptr<ED2KFile> >::iterator Iter;
};

/**
 * Search packet is used in client<->server communication to perform a search
 * on one or more servers. The search query is passed as SearchPtr to this
 * object, and handled internally as appropriate.
 *
 * Usage: Client -> Server
 */
class Search : public Packet {
public:
	Search(SearchPtr data, uint8_t proto = PR_ED2K);
	operator std::string();
private:
	SearchPtr m_data;         //!< Search query data
};

/**
 * SearchResult packet is sent by server, and contains one or more search
 * results.
 *
 * Usage: Server -> Client
 */
class SearchResult : public Packet {
public:
	SearchResult(std::istream &i);
	uint32_t getCount() const { return m_results.size(); }
	boost::shared_ptr<ED2KSearchResult> getResult(uint32_t num) const {
		return m_results.at(num);
	}
protected:
	SearchResult();

	//! Reads one result from stream
	void readResult(std::istream &i);
private:
	std::vector<boost::shared_ptr<ED2KSearchResult> > m_results;
};


/**
 * Request the callback from a LowID client. This request is relayed to the
 * client indicated by @id by the server, after which the client will
 * "call back" to us, e.g. connect us. This can only be sent if the sender
 * itself is high-id (since otherwise the callee wouldn't be able to connect
 * us).
 */
class ReqCallback : public Packet {
public:
	ReqCallback(uint32_t id);
	operator std::string();
private:
	uint32_t m_id;
};

/**
 * This packet indicates that a remote client wishes us to "call back" to it.
 * This packet is sent by server in case we are a low-id client, thus remote
 * clients cannot connect us directly.
 */
class CallbackReq : public Packet {
public:
	CallbackReq(std::istream &i);

	IPV4Address getAddr() const { return m_addr; }
private:
	IPV4Address m_addr;
};

/**
 * Request sources for a hash from server. Note: Requesting sources costs
 * credits on server, and thus should be used carefully to avoid getting
 * blacklisted.
 */
class ReqSources : public Packet {
public:
	ReqSources(const Hash<ED2KHash> &h, uint32_t size);
	operator std::string();
private:
	Hash<ED2KHash> m_hash;        //!< File to request sources for
	uint32_t       m_size;        //!< File size
};

/**
 * This is server's response to ReqSources packet, and contains the list of
 * sources for the given hash.
 */
class FoundSources : public Packet {
public:
	FoundSources(std::istream &i);
	uint32_t    getCount()            const { return m_sources.size(); }
	IPV4Address getSource(uint32_t n) const { return m_sources.at(n);  }
	Hash<ED2KHash> getHash()          const { return m_hash;           }
	uint32_t    getLowCount()         const { return m_lowCount;       }
private:
	Hash<ED2KHash> m_hash;              //!< File these sources belong to
	std::vector<IPV4Address> m_sources; //!< Sources list
	uint32_t       m_lowCount;          //!< # of LowID sources found
};

/**
 * Global source aquisition; sent via UDP, and contains one or more file hashes,
 * for which we wish to get sources for.
 */
class GlobGetSources : public Packet {
public:
	//! @param sendSize   Whether to also include filesizes
	GlobGetSources(bool sendSize);
	void addHash(const Hash<ED2KHash> &hash, uint32_t fileSize) {
		m_hashList.push_back(std::make_pair(hash, fileSize));
	}
	operator std::string();
private:
	std::vector<std::pair<Hash<ED2KHash>, uint32_t> > m_hashList;
	bool m_sendSize; //!< Whether to include filesizes in the packet
};

/**
 * Response to UDP GlobGetSources, this packet includes hash and list of sources
 * corresponding to that hash.
 */
class GlobFoundSources : public Packet {
public:
	typedef std::vector<IPV4Address>::iterator Iter;

	GlobFoundSources(std::istream &i);

	size_t size() const { return m_sources.size(); }
	Iter begin() { return m_sources.begin(); }
	Iter end() { return m_sources.end(); }
	Hash<ED2KHash> getHash() const { return m_hash; }
private:
	Hash<ED2KHash> m_hash;              //!< File these sources belong to
	std::vector<IPV4Address> m_sources; //!< Sources list
};

/**
 * Global stats request; requests server to respond it's current users/files
 * counts.
 */
class GlobStatReq : public Packet {
public:
	GlobStatReq();
	operator std::string();

	uint32_t getChallenge() const { return m_challenge; }
private:
	uint32_t m_challenge;    //!< Random challenge
};

/**
 * Global stats response - includes the servers current users/files counts.
 */
class GlobStatRes : public Packet {
public:
	GlobStatRes(std::istream &i);

	uint32_t getChallenge()  const { return m_challenge;  }
	uint32_t getUsers()      const { return m_users;      }
	uint32_t getFiles()      const { return m_files;      }
	uint32_t getMaxUsers()   const { return m_maxUsers;   }
	uint32_t getSoftLimit()  const { return m_softLimit;  }
	uint32_t getHardLimit()  const { return m_hardLimit;  }
	uint32_t getUdpFlags()   const { return m_udpFlags;   }
	uint32_t getLowIdUsers() const { return m_lowIdUsers; }
private:
	uint32_t m_challenge;   //!< Challenge; equals GlobStatReq::m_challenge
	uint32_t m_users;       //!< Current online users
	uint32_t m_files;       //!< Current online files
	uint32_t m_maxUsers;    //!< Max users seen online
	uint32_t m_softLimit;   //!< Soft files count limit
	uint32_t m_hardLimit;   //!< Hard files count limit
	uint32_t m_udpFlags;    //!< UDP flags
	uint32_t m_lowIdUsers;  //!< Number of online LowId users
};

/**
 * Global search request, sent via UDP socket to servers; format is
 * same as with Search packet, except for different opcode.
 *
 * Usage: Client -> Server (UDP)
 */
class GlobSearchReq : public Search {
public:
	GlobSearchReq(SearchPtr data, uint8_t proto = PR_ED2K);
	operator std::string();
};

/**
 * Response to GlobSearchReq packet, sent by servers via UDP; format is
 * same as SearchResult packet, except for different opcode.
 */
class GlobSearchRes : public SearchResult {
public:
	GlobSearchRes(std::istream &i);
};

                      /*************************/
                      /*  Client <-> Client    */
                      /*************************/

/**
 * Hello packet is used to initialize a communication with another client.
 * The packet contains a set of information about ourselves.
 */
class Hello : public Packet {
public:
	Hello(uint8_t proto = PR_ED2K, const std::string &modStr = "");
	Hello(std::istream &i, bool hashLen = true);
	operator std::string();

	Hash<MD4Hash> getHash()       const { return m_hash;       }
	IPV4Address   getClientAddr() const { return m_clientAddr; }
	IPV4Address   getServerAddr() const { return m_serverAddr; }
	std::string   getNick()       const { return m_nick;       }
	uint32_t      getVersion()    const { return m_version;    }
	std::string   getModStr()     const { return m_modStr;     }
	uint32_t      getMuleVer()    const { return m_muleVer;    }
	uint16_t      getUdpPort()    const { return m_udpPort;    }
	uint32_t      getFeatures()   const { return m_features;   }
protected:
	void load(std::istream &i, bool hashLen = true);
	std::string save(uint8_t opcode, bool hashLen = true);
private:
	Hash<MD4Hash> m_hash;         //!< Userhash
	IPV4Address   m_clientAddr;   //!< Client ip address/port
	IPV4Address   m_serverAddr;   //!< Server ip address/port
	std::string   m_nick;         //!< User nick
	uint32_t      m_version;      //!< User client version
	std::string   m_modStr;       //!< User client mod string
	uint32_t      m_muleVer;      //!< Mule version
	uint16_t      m_udpPort;      //!< UDP port
	uint32_t      m_features;     //!< eMule extended features bitset
};

/**
 * HelloAnswer packet is the expected response to Hello packet. This includes
 * a set of information about the client. HelloAnswer differs from Hello only
 * by one byte (hashsize is sent on Hello but not on HelloAnswer), and has
 * different opcode.
 */
class HelloAnswer : public Hello {
public:
	HelloAnswer(uint8_t proto = PR_ED2K, const std::string &modStr = "");
	HelloAnswer(std::istream &i);
	operator std::string();
};

class MuleInfo : public Packet {
public:
	MuleInfo();
	MuleInfo(std::istream &i);
	operator std::string();

	uint8_t     getProto()       const { return m_protocol;    }
	uint8_t     getVersion()     const { return m_version;     }
	uint8_t     getComprVer()    const { return m_comprVer;    }
	uint8_t     getUdpVer()      const { return m_udpVer;      }
	uint8_t     getCommentVer()  const { return m_commentVer;  }
	uint8_t     getExtReqVer()   const { return m_extReqVer;   }
	uint8_t     getSrcExchVer()  const { return m_srcExchVer;  }
	uint8_t     getCompatCliID() const { return m_compatCliID; }
	uint16_t    getUdpPort()     const { return m_udpPort;     }
	uint16_t    getFeatures()    const { return m_features;    }
	std::string getModStr()      const { return m_modStr;      }
private:
	uint8_t     m_protocol;          //!< Protocol version
	uint8_t     m_version;           //!< Client version
	uint8_t     m_comprVer;          //!< Compression version
	uint8_t     m_udpVer;            //!< UDP version
	uint8_t     m_commentVer;        //!< Comment version
	uint8_t     m_extReqVer;         //!< Ext Req version
	uint8_t     m_srcExchVer;        //!< Source Exchange version
	uint8_t     m_compatCliID;       //!< Compatible client ID
	uint16_t    m_udpPort;           //!< UDP port
	uint16_t    m_features;          //!< Supported features
	std::string m_modStr;            //!< Mod string
protected:
	uint8_t     m_opcode;            //!< For implementation use only
};

/**
 * MuleInfoAnswer packet is the response to MuleInfo packet, and contains the
 * exact same data as MuleInfo packet.
 */
class MuleInfoAnswer : public MuleInfo {
public:
	MuleInfoAnswer();
	MuleInfoAnswer(std::istream &i);
	operator std::string();
};

/**
 * ReqFile indicates that the sender wishes us to to upload the file to him.
 * Thus he sends ReqFile, and expects FileName and (optionally) FileDesc answers
 * from us before proceeding.
 *
 * eMule extends this request by adding partmap and complete source counts if
 * client supports extended requests.
 */
class ReqFile : public Packet {
public:
	ReqFile(const Hash<ED2KHash> &h, const PartData *pd, uint16_t srcCnt=0);
	ReqFile(std::istream &i);
	operator std::string();

	Hash<ED2KHash> getHash() const { return m_hash; }
private:
	Hash<ED2KHash>    m_hash;      //!< File hash
	std::vector<bool> m_partMap;   //!< ExtReqv1: part map
	uint16_t          m_srcCnt;    //!< ExtReqv2: num COMPLETE sources
};

/**
 * FileName is the expected response to ReqFile, and contains the hash, and
 * the file name corresponding to that hash. Note that this packet also implies
 * that the sender is in fact sharing the file in question.
 */
class FileName : public Packet {
public:
	FileName(const Hash<ED2KHash> &hash, const std::string &filename);
	FileName(std::istream &i);
	operator std::string();

	std::string getName()    const { return m_name; }
	Hash<ED2KHash> getHash() const { return m_hash; }
private:
	Hash<ED2KHash> m_hash;   //!< File hash
	std::string m_name;      //!< File name
};

/**
 * FileDesc packet contains the rating and the comment of the file. It is used
 * as reply to ReqFile packet, along with FileName packet, and only sent if
 * the file does have a rating/comment set, thus it is optional. Note that this
 * packet does not contain the hash of the file.
 */
class FileDesc : public Packet {
public:
	FileDesc(ED2KFile::Rating rating, const std::string &comment);
	FileDesc(std::istream &i);
	operator std::string();

	std::string      getComment() const { return m_comment; }
	ED2KFile::Rating getRating()  const { return m_rating;  }
private:
	ED2KFile::Rating m_rating;        //!< Rating
	std::string      m_comment;       //!< Comment
};

/**
 * SetReqFileId is the last request from the client, binding the requested file
 * to the hash sent in this packet. This means the client is bound to receive
 * the file corresponding to the hash from us. Note that it is allowed (by eMule
 * extended protocol) to change the requested file while waiting on the queue.
 */
class SetReqFileId : public Packet {
public:
	SetReqFileId(const Hash<ED2KHash> &hash);
	SetReqFileId(std::istream &i);
	operator std::string();

	Hash<ED2KHash> getHash() const { return m_hash; }
private:
	Hash<ED2KHash> m_hash;
};

/**
 * Answer to SetReqFileId packet, this indicates that we are not sharing the
 * file currently.
 */
class NoFile : public Packet {
public:
	NoFile(const Hash<ED2KHash> &hash);
	NoFile(std::istream &i);
	operator std::string();

	Hash<ED2KHash> getHash() const { return m_hash; }
private:
	Hash<ED2KHash> m_hash;
};

/**
 * Finalizing the upload request sequence, this packet is sent by the uploading
 * client to indicate that the file is ready to be uploaded. It also contains
 * the part map of the file (if it is partial). Note that sending the partmap
 * is optional, and if not sent, the receiver assumes that the entire file
 * is available. Thus partmap is only sent if the file is partial.
 */
class FileStatus : public Packet {
public:
	FileStatus(const Hash<ED2KHash> &hash, const PartData *pd);
	FileStatus(std::istream &i);
	operator std::string();

	std::vector<bool> getPartMap() const { return m_partMap; }
	Hash<ED2KHash>       getHash() const { return m_hash;    }
private:
	Hash<ED2KHash>    m_hash;
	std::vector<bool> m_partMap;
};

/**
 * ReqHashSet packet is used to request a hashset from remote client. The
 * expected answer to this packet is HashSet packet.
 */
class ReqHashSet : public Packet {
public:
	ReqHashSet(const Hash<ED2KHash> &hash);
	ReqHashSet(std::istream &i);
	operator std::string();

	Hash<ED2KHash> getHash() const { return m_hash; }
private:
	Hash<ED2KHash> m_hash;          //!< File hash
};

/**
 * HashSet packet is answer to ReqHashSet packet, and contains the hashset
 * corresponding to the requested filehash.
 *
 * There are two semantics here. The tmpSet pointer is used for hashsets passed
 * in the first constructor, which takes a pointer. This pointer is not deleted,
 * since we cannot take ownership of it here. The m_hashSet pointer however is
 * loaded from stream by second constructor, and is dynamically deallocated
 * when no longer needed.
 *
 * It's safe (altough similarly messy) to do it this way rather than not take
 * ownership at all for the hashsets created in stream-loading constructor,
 * because the latter would expose too large of a risc of memory leaks in case
 * of exception-situations, which is unacceptable. Using this approach, the
 * constructed HashSet is guaranteed to be destroyed in all cases.
 */
class HashSet : public Packet {
public:
	HashSet(ED2KHashSet *hashset);
	HashSet(std::istream &i);
	operator std::string();

	boost::shared_ptr<ED2KHashSet> getHashSet() const { return m_hashSet; }
private:
	boost::shared_ptr<ED2KHashSet> m_hashSet;
	ED2KHashSet *m_tmpSet;
};

/**
 * StartUploadReq finalizes the upload request sequence. It may optionally
 * contain the requested file hash. Upon receiving this packet, the uploading
 * client must insert the requesting client into it's upload queue and/or start
 * uploading. The responses to this packet are thus QueueRanking or
 * AcceptUploadReq.
 *
 * \note Sending hash in this packet is optional!
 */
class StartUploadReq : public Packet {
public:
	StartUploadReq();
	StartUploadReq(const Hash<ED2KHash> &h);
	StartUploadReq(std::istream &i);
	operator std::string();

	Hash<ED2KHash> getHash() const { return m_hash; }
private:
	Hash<ED2KHash> m_hash;       //!< Optional
};

/**
 * Empty packet, indicating an accepted upload request (e.g. the uploading can
 * start right away).
 */
class AcceptUploadReq : public Packet {
public:
	AcceptUploadReq();
	AcceptUploadReq(std::istream &i);
	operator std::string();
};

/**
 * Indicates the queue ranking of a queued client.
 */
class QueueRanking : public Packet {
public:
	QueueRanking(uint16_t rank);
	QueueRanking(std::istream &i);
	operator std::string();

	uint16_t getQR() const { return m_qr; }
private:
	uint16_t m_qr;
};

/**
 * MuleQueueRank packet is different from QueueRanking packet only from
 * implementation point of view. While QueueRanking contains 32-bit integer
 * value, this one contains 16-bit integer value, plus 10 empty bytes.
 */
class MuleQueueRank : public Packet {
public:
	MuleQueueRank(uint16_t rank);
	MuleQueueRank(std::istream &i);
	operator std::string();

	uint16_t getQR() const { return m_qr; }
private:
	uint16_t m_qr;
};

/**
 * Requests (up to) three parts, indicated by the three ranges
 *
 * Important: In ed2k network, the chunk range end offset is NOT included.
 * This is different from hydranode Range implementation, where end offset
 * is also included. The member m_reqChunks contains the ranges in hydranode
 * Range format, and conversions to ed2k format are done during packet
 * parsing.
 */
class ReqChunks : public Packet {
public:
	ReqChunks(const Hash<ED2KHash> &h, const std::list<Range32> &reqparts);
	ReqChunks(std::istream &i);
	operator std::string();

	Hash<ED2KHash> getHash()       const { return m_hash;             }
	uint8_t getReqChunkCount()     const { return m_reqChunks.size(); }
	Range32 getReqChunk(uint8_t n) const { return m_reqChunks.at(n);  }
private:
	Hash<ED2KHash>        m_hash;               //!< Requested file hash
	std::vector<Range32> m_reqChunks;          //!< Requested chunks
};

/**
 * DataChunk packet indicates a single data chunk send from one client to
 * another.
 */
class DataChunk : public Packet {
public:
	DataChunk(
		Hash<ED2KHash> hash, uint32_t begin,
		uint32_t end, const std::string &data
	);
	DataChunk(std::istream &i);
	operator std::string();

	const std::string& getData()  const { return m_data;  }
	uint32_t           getBegin() const { return m_begin; }
	uint32_t           getEnd()   const { return m_end;   }
private:
	Hash<ED2KHash> m_hash;     //!< File hash where the data belongs to
	uint32_t       m_begin;    //!< Begin offset (inclusive)
	uint32_t       m_end;      //!< End offset (exclusive)
	std::string    m_data;     //!< The data
};

/**
 * Emule extended packet, this contains packed data chunk.
 *
 * This packet contains only part of the entire packed data stream. In ed2k
 * protocol, a 180k chunk is compressed, and spread over 10k chunks and sent
 * separately. The size member of this packet indicates the total size of the
 * packed data (out of which 10kb or so was sent in this packet).
 */
class PackedChunk : public Packet {
public:
	PackedChunk (
		Hash<ED2KHash> hash, uint32_t begin,
		uint32_t size, const std::string &data
	);
	PackedChunk(std::istream &i);
	operator std::string();

	const std::string& getData()  const { return m_data;  }
	uint32_t           getBegin() const { return m_begin; }
	uint32_t           getSize()  const { return m_size;  }
private:
	Hash<ED2KHash> m_hash;     //!< File hash where the data belongs to
	uint32_t       m_begin;    //!< Data begin offset
	uint32_t       m_size;     //!< Size of entire packed data chunk
	std::string    m_data;     //!< Part of the packed data chunk
};

/**
 * CancelTransfer packet indicates that the receiver of this packet should stop
 * sending data. This packet does not contain any payload.
 */
class CancelTransfer : public Packet {
public:
	CancelTransfer();
	CancelTransfer(std::istream &i);
	operator std::string();
};

/**
 * SourceExchReq packet is sent from one client to another in order to request
 * all sources the remote client knows for a hash.
 */
class SourceExchReq : public Packet {
public:
	SourceExchReq(const Hash<ED2KHash> &hash);
	SourceExchReq(std::istream &i);
	operator std::string();

	Hash<ED2KHash> getHash() const { return m_hash; }
private:
	Hash<ED2KHash> m_hash;
};

/**
 * AnswerSources packet is the expected answer to SourceExchReq packet, and
 * contains the list of sources corresponding to a file hash. As a general
 * rule, no more than 500 sources are sent, and other limitations may be
 * present, depending on client.
 */
class AnswerSources : public Packet {
public:
	typedef boost::tuple<uint32_t, uint16_t, uint32_t, uint16_t> Source;
	typedef std::vector<Source> SourceList;
	typedef SourceList::const_iterator CIter;

	AnswerSources(const Hash<ED2KHash> &hash, const SourceList &srcs);
	AnswerSources(std::istringstream &i);
	operator std::string();

	Hash<ED2KHash> getHash() const    { return m_hash;            }
	CIter begin()            const    { return m_srcList.begin(); }
	CIter end()              const    { return m_srcList.end();   }
	size_t size()            const    { return m_srcList.size();  }

	/**
	 * If set to true, all client-ids are swapped before sending
	 */
	void setSwapIds(bool swap) { m_swapIds = swap; }
private:
	Hash<ED2KHash> m_hash;
	SourceList m_srcList;
	bool m_swapIds;
};

/**
 * This packet can occasionally be sent compressed, however our parser then
 * resets the opcode to PR_ED2K (since compressed packets from Servers should
 * be done so). However, in this case, the packet is supposed to be PR_EMULE
 * instead, which leads to the problem, solved by this typedef and duplicate
 * DECLARE_PACKET_FACTORY() macro in factories.h.
 */
typedef AnswerSources AnswerSources2;

/**
 * A text message sent from one client to another client
 */
class Message : public Packet {
public:
	Message(const std::string &msg);
	Message(std::istream &i);
	operator std::string();

	std::string getMsg() const { return m_message; }
private:
	std::string m_message;
};

/**
 * ChangeId packet indicates the sending client changed it's ID on the net from
 * oldId to newId.
 */
class ChangeId : public Packet {
public:
	ChangeId(uint32_t oldId, uint32_t newId);
	ChangeId(std::istream &i);
	operator std::string();

	uint32_t getOldId() const { return m_oldId; }
	uint32_t getNewId() const { return m_newId; }
private:
	uint32_t m_oldId;
	uint32_t m_newId;
};

/**
 * Initiate Secure Identification with remote client; requests the remote
 * client to send us signature (and if needed) also publickey. Later, when
 * we already have the client's pubkey, we can only request the signature.
 */
class SecIdentState : public Packet {
public:
	SecIdentState(::Donkey::SecIdentState s);
	SecIdentState(std::istream &i);
	operator std::string();

	uint8_t  getState()     const { return m_state;     }
	uint32_t getChallenge() const { return m_challenge; }
private:
	uint8_t  m_state;     //!< One of ::SecIdentState enum values
	uint32_t m_challenge; //!< Random 32bit challenge
};

/**
 * PublicKey packet is the expected response to SecIdentState packet which
 * requested PublicKey; it contains - the remote client's public key ofcourse.
 */
class PublicKey : public Packet {
public:
	PublicKey(const ::Donkey::PublicKey &pubKey);
	PublicKey(std::istream &i);
	operator std::string();

	::Donkey::PublicKey getKey() const { return m_pubKey; }
private:
	::Donkey::PublicKey m_pubKey;   //!< Public key
};

/**
 * Signature packet is the expected response to SecIdentState packet which
 * requested Signature; it contains - the remote client's signature ofcourse.
 */
class Signature : public Packet {
public:
	Signature(const std::string &sign, IpType ipType = 0);
	Signature(std::istream &i);
	operator std::string();

	std::string getSign()   const { return m_signature; }
	IpType      getIpType() const { return m_ipType;    }
private:
	std::string m_signature; //!< Signature
	IpType      m_ipType;    //!< Either local, remote or none
};

			/*************************
			 * Client <-> Client UDP *
			 *************************/

/**
 * Used to ping sources every 20 minutes to verify that we are still queued for
 * our requested file. By default the packet contains only filehash, but
 * UDPv3 adds complete source count, UDPv4 adds full extended info, as in
 * ReqFile packet (e.g. partmap).
 *
 * Note that if you do not send this packet via UDP to eMules at least once per
 * hour, you will be dropped from queues.
 */
class ReaskFilePing : public Packet {
public:
	ReaskFilePing(
		const Hash<ED2KHash> &h, const PartData *pd, uint16_t srcCnt,
		uint8_t udpVersion
	);
	ReaskFilePing(std::istream &i);
	operator std::string();

	Hash<ED2KHash>    getHash()    const { return m_hash;    }
	std::vector<bool> getPartMap() const { return m_partMap; }
	uint16_t          getSrcCnt()  const { return m_srcCnt;  }
private:
	Hash<ED2KHash> m_hash;       //!< Filehash
	std::vector<bool> m_partMap; //!< Availability partmap
	uint16_t m_srcCnt;           //!< full sources count
	uint8_t m_udpVersion;        //!< Client udp version
};

/**
 * QueueFull indicates that the remote client's queue is ... full. This packet
 * contains no other data.
 */
class QueueFull : public Packet {
public:
	QueueFull();
	QueueFull(std::istream &i);
	operator std::string();
};

/**
 * ReaskAck is an expected response to ReaskFilePing packet, ant includes the
 * remote queue ranking. UDPv4 also includes partmap;
 */
class ReaskAck : public Packet {
public:
	ReaskAck(const PartData *pd, uint16_t rank, uint8_t udpVersion);
	ReaskAck(std::istream &i);
	operator std::string();

	std::vector<bool> getPartMap() const { return m_partMap; }
	uint16_t          getQR()      const { return m_qr;      }
private:
	std::vector<bool> m_partMap;    //!< Availability partmap
	uint16_t m_qr;                  //!< Remote queue ranking
	uint8_t m_udpVersion;           //!< UDP version
};

/**
 * Indicates that the requested file (via last ReaskFilePing) was not found.
 * This packet contains no other data.
 */
class FileNotFound : public Packet {
public:
	FileNotFound();
	FileNotFound(std::istream &i);
	operator std::string();
};

/**
 * PortTest isn't exactly part of eDonkey2000 nor eMule extended protocol; it
 * is instead used to verify correct firewall configurations, for example, by
 * a website with an appropriate button. This packet may contain single '1'
 * value to indicate successful port test. This packet may be sent both via
 * TCP and UDP.
 */
class PortTest : public Packet {
public:
	PortTest();
	PortTest(std::istream &i);
	operator std::string();
};

} // ! namespace ED2KPacket

} // end namespace Donkey

#endif
