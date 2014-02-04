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
 * \file packets.cpp Implementation of Packet objects input/output
 *
 * This file basically includes the entire eDonkey2000 protocol packets reading
 * writing. As such, this file grows rather big over time. In addition to that,
 * this file also needs to include large amount of ed2k module headers, since
 * we want the packet objects to be as self-contained as possible, requiring
 * minimum amount of construction arguments for easier usage. If this file
 * grows beyond our management capabilities, it could be broken up into
 * several smaller files, based on some criteria.
 */

#include <hncore/ed2k/packets.h>
#include <hncore/ed2k/ed2k.h>
#include <hncore/ed2k/serverlist.h>
#include <hncore/ed2k/tag.h>
#include <hncore/ed2k/ed2kfile.h>
#include <hncore/ed2k/zutils.h>
#include <hnbase/osdep.h>
#include <hncore/partdata.h>
#include <boost/tuple/tuple.hpp>
#include <bitset>

namespace Donkey {

// All packets are declared in ED2KPacket namespace to avoid name clashes
namespace ED2KPacket {

// Utility functions
// -----------------

uint64_t s_overheadUpSize = 0;
uint64_t s_overheadDnSize = 0;
uint64_t getOverheadUp() { return s_overheadUpSize; }
uint64_t getOverheadDn() { return s_overheadDnSize; }
void addOverheadDn(uint32_t amount) { s_overheadDnSize += amount; }

/**
 * Generates chunk map, as specified by eDonkey2000 protocol. This means
 * the vector is filled with bitfields, where each 'true' bit indicates
 * we have the 9.28MB part, and each 'false' bit indicates we do not
 * have that part. Empty partmap indicates the entire file is available.
 * Leftover bits are padded with zeros.
 *
 * Note that PartData API now supports this internally, so when possible, we
 * attempt to aquire this data from PartData. However, if that fails, we still
 * generate it ourselves here.
 */
std::vector<bool> makePartMap(const PartData *pd) {
	if (!pd) {
		return std::vector<bool>();
	}
	try {
		return pd->getPartStatus(ED2K_PARTSIZE);
	} catch (std::exception &) {}

	std::vector<bool> partMap;
	for (uint32_t i = 0; i < pd->getSize(); i += ED2K_PARTSIZE + 1) {
		if (i + ED2K_PARTSIZE > pd->getSize()) {
			partMap.push_back(pd->isComplete(i, pd->getSize()));
		} else {
			partMap.push_back(pd->isComplete(i, i + ED2K_PARTSIZE));
		}
	}
	return partMap;
}

// Write part map into stream
void writePartMap(std::ostream &o, const std::vector<bool> &partMap) {
	Utils::putVal<uint16_t>(o, partMap.size());
	std::vector<bool>::const_iterator iter = partMap.begin();
	while (iter != partMap.end()) {
		uint8_t tmp = 0;
		for (uint8_t i = 0; i < 8; ++i, ++iter) {
			if (iter == partMap.end()) {
				Utils::putVal<uint8_t>(o, tmp);
				break;
			} else {
				tmp |= *iter << i;
			}
		}
		Utils::putVal<uint8_t>(o, tmp);
	}
}

// Read part map from stream and store in passed container
void readPartMap(std::istream &i, std::vector<bool> *partMap) {
	CHECK_THROW(partMap);
	uint16_t cnt = Utils::getVal<uint16_t>(i);
	while (cnt && i) {
		std::bitset<8> tmp(Utils::getVal<uint8_t>(i).value());
		for (uint8_t i = 0; i < (cnt >= 8 ? 8 : cnt); ++i) {
			partMap->push_back(tmp[i]);
		}
		cnt -= cnt > 8 ? 8 : cnt;
	}
}

// Exception class
// ---------------
InvalidPacket::InvalidPacket(const std::string &what) :
std::runtime_error(what) {}

// Packet class
// ------------
Packet::Packet(uint8_t proto) : m_proto(proto) {}
Packet::~Packet() {}

// Construct the final packet. This method is called during every outgoing
// packet sending, thus should be as fast as possible.
//
// If we are required to build a compressed packet, we attempt to compress the
// packet data using zlib. If we end up with more data than originally after
// compression, we drop the compressed data.
//
// Note: We do NOT compress the opcode of the packet - that is left outside
//       compressed area.
//
// The packet length is the total amount of data in the packet (including
// opcode. When sending compressed packet, the packet length is the amount
// of compressed data (not uncompressed size).
std::string Packet::makePacket(const std::string &data, bool hexDump) {
	using namespace Zlib;

	std::string packet(data);
	if (m_proto == PR_ZLIB) {
		std::string ret = compress(data.substr(1));
		if (ret.size() >= data.size()) {
			m_proto = PR_ED2K; // revert to non-compressed
		} else {
#ifndef NDEBUG // Verify compressiong/decompression
			std::string check(decompress(ret));
			assert(check == data.substr(1));
#endif
			ret.insert(ret.begin(), data.at(0));
			packet = ret;
		}
	}
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, m_proto);
	Utils::putVal<uint32_t>(tmp, packet.size());
	Utils::putVal<std::string>(tmp, packet, packet.size());
#ifndef HEXDUMPS
	if (hexDump)
#endif
	{
		logDebug(boost::format(
			COL_SEND
			"Sending packet: protocol=%s opcode=%s size=%s %s %s"
			COL_NONE
			) % Utils::hexDump(m_proto) % Utils::hexDump(data.at(0))
			% Utils::hexDump(data.size())
			% (m_proto == PR_ZLIB ?
				COL_COMP "(compressed)" COL_SEND
				: ""
			) % (data.size() > 1024
				? "Data size > 1024 - omitted."
				: Utils::hexDump(data.substr(1))
			)
		);
	}

	if (tmp.str()[5] == OP_SENDINGCHUNK || tmp.str()[5] == OP_PACKEDCHUNK) {
		s_overheadUpSize += 30;
	} else {
		s_overheadUpSize += tmp.str().size();
	}

	return tmp.str();
}

                        /***********************/
                        /*  Client <-> Server  */
                        /***********************/

// LoginRequest class
// ------------------
LoginRequest::LoginRequest(uint8_t proto) : Packet(proto) {}
LoginRequest::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OP_LOGINREQUEST);
	Utils::putVal<std::string>(
		tmp, ED2K::instance().getHash().getData(), 16
	);
	Utils::putVal<uint32_t>(tmp, 0); // clientid
	Utils::putVal<uint16_t>(tmp, ED2K::instance().getTcpPort());
	Utils::putVal<uint32_t>(tmp, 5); // tagcount
	tmp << Tag(CT_NICK, ED2K::instance().getNick());
	tmp << Tag(CT_VERSION, VER_EDONKEY);
	tmp << Tag(CT_PORT, ED2K::instance().getTcpPort());
	tmp << Tag(CT_MULEVERSION, VER_OWN);
	tmp << Tag(CT_FLAGS, FL_ZLIB|FL_NEWTAGS);
	return makePacket(tmp.str());
}

// ServerMessage class
// -------------------
ServerMessage::ServerMessage(std::istream &i) {
	uint16_t len = Utils::getVal<uint16_t>(i);
	m_msg = Utils::getVal<std::string>(i, len);
}

// ServerStatus class
// ------------------
ServerStatus::ServerStatus(std::istream &i) {
	m_users = Utils::getVal<uint32_t>(i);
	m_files = Utils::getVal<uint32_t>(i);
}

// IdChange class
// --------------
IdChange::IdChange(std::istream &i) : m_id(Utils::getVal<uint32_t>(i)),
m_flags() {
	// Peek at next byte to see if we have TCP flags that newer servers send
	try {
		m_flags = Utils::getVal<uint32_t>(i);
	} catch (Utils::ReadError &) {
		// No worries - probably server doesn't support it
	}
}

// GetServerList class
// -------------------
GetServerList::GetServerList(uint8_t proto) : Packet(proto) {}
GetServerList::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OP_GETSERVERLIST);
	return makePacket(tmp.str());
}

// ServerIdent class
// -----------------
ServerIdent::ServerIdent(std::istream &i) {
	m_hash = Utils::getVal<std::string>(i, 16).value();
	m_addr.setAddr(Utils::getVal<uint32_t>(i));
	m_addr.setPort(Utils::getVal<uint16_t>(i));
	uint32_t tagcount = Utils::getVal<uint32_t>(i);
	while (tagcount-- && i) {
		Tag t(i);
		switch (t.getOpcode()) {
			case CT_SERVERNAME:
				m_name = t.getStr();
				break;
			case CT_SERVERDESC:
				m_desc = t.getStr();
				break;
			default:
				warnUnHandled("ServerIdent packet", t);
				break;
		}
	}
}

ServerList::ServerList(std::istream &i) {
	uint8_t count = Utils::getVal<uint8_t>(i);
	while (count--) {
		IPV4Address addr;
		addr.setAddr(Utils::getVal<uint32_t>(i));
		addr.setPort(Utils::getVal<uint16_t>(i));
		m_servers.push_back(addr);
	}
}

// OfferFiles class
// ----------------
OfferFiles::OfferFiles(uint8_t proto) : Packet(proto) {}
OfferFiles::OfferFiles(boost::shared_ptr<ED2KFile> f, uint8_t proto)
: Packet(proto) {
	push(f);
}

// Construct the OFFERFILES packet.
OfferFiles::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OP_OFFERFILES);
	Utils::putVal<uint32_t>(tmp, m_toOffer.size());
	for (Iter i = m_toOffer.begin(); i != m_toOffer.end(); ++i) {
		tmp << *(*i);
	}
	return makePacket(tmp.str());
}

// Search class
// ------------
/*
Search request data:
00000000  00 00 01 04 00 62 6c 61  68 02 03 00 41 6e 79 01  |.....blah...Any.|
00000010  00 03                                             |..|
*/
/*
Search request data:
00000000  00 00 00 00 01 17 00 6f  6e 65 20 74 77 6f 20 74  |.......one.two.t|
00000010  68 72 65 65 20 66 6f 75  72 20 66 69 76 65 02 09  |hree.four.five..|
00000020  00 43 44 2d 49 6d 61 67  65 73 01 00 03 03 00 00  |.CD-Images......|
00000030  10 00 01 01 00 02                                 |......|
*/
// The tricky part here is that we FIRST need to write the number of
// andParameters, and then the same number of actual parameters.
Search::Search(SearchPtr data, uint8_t proto) : Packet(proto), m_data(data) {}
// Generate search packet
Search::operator std::string() {
	// Static data used in search packet creation
	static const uint8_t  stringParameter   = 0x01;
	static const uint8_t  typeParameter     = 0x02;
	static const uint8_t  numericParameter  = 0x03;
	static const uint16_t andParameter      = 0x0000;
	static const uint32_t typeNemonic       = 0x030001;    // !! 24-bit !!
//	static const uint32_t extensionNemonic  = 0x040001;    // !! 24-bit !!
	static const uint32_t minNemonic        = 0x02000101;
	static const uint32_t maxNemonic        = 0x02000102;
//	static const uint32_t avaibilityNemonic = 0x15000101;

	std::ostringstream tmp;
	uint32_t paramCount = 0; // count parameters

	// First - search terms. in ed2k, we just send a big string containing
	// all search terms, separated by spaces (is this right?)
	std::string terms;
	for (uint32_t i = 0; i < m_data->getTermCount(); ++i) {
		terms += m_data->getTerm(i) + " ";
	}
	terms.erase(--terms.end());
	Utils::putVal<uint8_t>(tmp, stringParameter);
	Utils::putVal<uint16_t>(tmp, terms.size());
	Utils::putVal<std::string>(tmp, terms, terms.size());
	++paramCount;

	// Type is always required
	std::string type(ED2KFile::HNType2ED2KType(m_data->getType()));
	if (!type.size()) {
		type = "Any";
	}
	Utils::putVal<uint8_t>(tmp, typeParameter);
	Utils::putVal<uint16_t>(tmp, type.size());
	Utils::putVal<std::string>(tmp, type, type.size());
	// Who on earth came up with the idea of a 3-byte field ? *DOH*
	uint32_t _tmp(typeNemonic);
	_tmp = SWAP32_ON_BE(_tmp);
	tmp.write(reinterpret_cast<const char*>(&_tmp), 3);

	// Next check additional things we might have.
	if (m_data->getMinSize() > 0) {
		Utils::putVal<uint8_t>(tmp, numericParameter);
		Utils::putVal<uint32_t>(tmp, m_data->getMinSize());
		Utils::putVal<uint32_t>(tmp, minNemonic);
		++paramCount;
	}
	if (m_data->getMaxSize() < std::numeric_limits<uint32_t>::max()) {
		Utils::putVal<uint8_t>(tmp, numericParameter);
		Utils::putVal<uint32_t>(tmp, m_data->getMaxSize());
		Utils::putVal<uint32_t>(tmp, maxNemonic);
		++paramCount;
	}

	// TODO: More search parameters support
	// Finalize the packet
	std::ostringstream tmp2;
	for (uint16_t i = 0; i < paramCount; ++i) {
		Utils::putVal<uint16_t>(tmp2, andParameter);
	}
	Utils::putVal<std::string>(tmp2, tmp.str(), tmp.str().size());

	std::ostringstream packet;
	Utils::putVal<uint8_t>(packet, OP_SEARCH);
	Utils::putVal<std::string>(packet, tmp2.str(), tmp2.str().size());
	return makePacket(packet.str());
}

// SearchResult class
//! TODO: Actually, the ID and Port here contain some useful
//! TODO: information, but there were some problems with that...
//! TODO: Look into it.
SearchResult::SearchResult(std::istream &i) {
	uint32_t count = Utils::getVal<uint32_t>(i);
	Utils::StopWatch stopwatch;
	uint32_t cnt2 = count; // used for later debug msg

	while (count-- && i) {
		readResult(i);
	}

	logDebug(
		boost::format(
			"Parsing " COL_BCYAN "%d"COL_NONE" search results took "
			COL_BGREEN "%dms" COL_NONE "."
		) % cnt2 % stopwatch
	);
}

// protected default constructor for usage by GlobSearchRes class
SearchResult::SearchResult() {}

void SearchResult::readResult(std::istream &i) {
	Hash<ED2KHash> h(Utils::getVal<std::string>(i, 16));
	uint32_t id = Utils::getVal<uint32_t>(i); // id - ignored
	uint16_t port = Utils::getVal<uint16_t>(i); // port - ignored
	uint32_t tagCount = Utils::getVal<uint32_t>(i);
	std::string name;
	uint32_t size = 0;
	uint32_t sources = 0;
	uint32_t completesrc = 0;
	std::string codec;
	uint32_t bitrate = 0;
	uint32_t len = 0;
	uint32_t lastSeen = 0;
	uint8_t  rating = 0;
	while (tagCount-- && i) {
		Tag t(i);
		switch (t.getOpcode()) {
			case CT_FILENAME:
				name = t.getStr();
				break;
			case CT_FILESIZE:
				size = t.getInt();
				break;
			case CT_SOURCES:
				sources = t.getInt();
				break;
			case CT_COMPLSRC:
				completesrc = t.getInt();
				break;
			case CT_MEDIA_CODEC:
				codec = t.getStr();
				break;
			case CT_MEDIA_BITRATE:
				bitrate = t.getInt();
				break;
			case CT_MEDIA_LENGTH:
				len = t.getInt();
				break;
			case CT_LASTSEENCOMPL:
				lastSeen = t.getInt();
				break;
			case CT_FILERATING:
				rating = t.getInt();
				break;
			default:
				if (t.getName() == "bitrate") {
					bitrate = t.getInt();
				} else if (t.getName() == "length") {
					/**
					 * TODO: Convert str-tag LEN
					 *       into integer-len
					 */
				}
				warnUnHandled("SearchResult", t);
				break;
		}
	}
	boost::shared_ptr<ED2KSearchResult> f;

	// Note - ignoring ID/Port values.
	f.reset(new ED2KSearchResult(h, name, size));
	f->addSources(sources);
	f->addComplete(completesrc);
	f->addRating(rating);
	f->addSource(IPV4Address(id, port));
	if (codec.size() || bitrate || len) {
		f->getStrd().reset(new StreamData(codec, bitrate, len));
	}

	m_results.push_back(f);
}

// ReqCallback class
// -----------------
ReqCallback::ReqCallback(uint32_t id) : m_id(id) {}
ReqCallback::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OP_REQCALLBACK);
	Utils::putVal<uint32_t>(tmp, m_id);
	return makePacket(tmp.str());
}

// CallbackReq class
// -----------------
CallbackReq::CallbackReq(std::istream &i) {
	m_addr.setAddr(Utils::getVal<uint32_t>(i));
	m_addr.setPort(Utils::getVal<uint16_t>(i));
}

// ReqSources class
// ----------------
ReqSources::ReqSources(const Hash<ED2KHash> &h, uint32_t size)
: m_hash(h), m_size(size) {}
ReqSources::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OP_GETSOURCES);
	Utils::putVal<std::string>(tmp, m_hash.getData(), 16);
	Utils::putVal<uint32_t>(tmp, m_size);
	return makePacket(tmp.str());
}

// FoundSources class
// ------------------
FoundSources::FoundSources(std::istream &i) : m_lowCount() {
	m_hash = Utils::getVal<std::string>(i, 16).value();
	uint8_t cnt = Utils::getVal<uint8_t>(i);
	while (cnt--) {
		IPV4Address addr;
		addr.setAddr(Utils::getVal<uint32_t>(i));
		addr.setPort(Utils::getVal<uint16_t>(i));
		addr.setAddr(SWAP32_ON_BE(addr.getAddr()));
		m_sources.push_back(addr);
		m_lowCount += isLowId(addr.getIp());
	}
}

// GlobGetSources class
// --------------------
GlobGetSources::GlobGetSources(bool sendSize) : m_sendSize(sendSize) {}
GlobGetSources::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, PR_ED2K);
	if (m_sendSize) {
		Utils::putVal<uint8_t>(tmp, OP_GLOBGETSOURCES2);
	} else {
		Utils::putVal<uint8_t>(tmp, OP_GLOBGETSOURCES);
	}
	for (uint32_t i = 0; i < m_hashList.size(); ++i) {
		Utils::putVal<
			std::string
		>(tmp, m_hashList[i].first.getData(), 16);
		if (m_sendSize) {
			Utils::putVal<uint32_t>(tmp, m_hashList[i].second);
		}
	}
	return tmp.str();
}

// GlobFoundSources class
// ----------------------
GlobFoundSources::GlobFoundSources(std::istream &i) {
	m_hash = Utils::getVal<std::string>(i, 16).value();
	uint8_t cnt = Utils::getVal<uint8_t>(i);
	while (i && cnt--) {
		uint32_t id = Utils::getVal<uint32_t>(i);
		uint16_t port = Utils::getVal<uint16_t>(i);
		id = SWAP32_ON_BE(id);
		m_sources.push_back(IPV4Address(id, port));
	}
}

// GlobStatReq class
// -----------------
GlobStatReq::GlobStatReq() {
	m_challenge = 0x55aa0000 + static_cast<uint16_t>(Utils::getRandom());
}
GlobStatReq::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, PR_ED2K);
	Utils::putVal<uint8_t>(tmp, OP_GLOBSTATREQ);
	Utils::putVal<uint32_t>(tmp, m_challenge);
	return tmp.str();
}

// GlobStatRes class
// -----------------
GlobStatRes::GlobStatRes(std::istream &i) : m_challenge(), m_users(), m_files(),
m_maxUsers(), m_softLimit(), m_hardLimit(), m_udpFlags(), m_lowIdUsers() {
	try {
		m_challenge  = Utils::getVal<uint32_t>(i);
		m_users      = Utils::getVal<uint32_t>(i);
		m_files      = Utils::getVal<uint32_t>(i);
		m_maxUsers   = Utils::getVal<uint32_t>(i);
		m_softLimit  = Utils::getVal<uint32_t>(i);
		m_hardLimit  = Utils::getVal<uint32_t>(i);
		m_udpFlags   = Utils::getVal<uint32_t>(i);
		m_lowIdUsers = Utils::getVal<uint32_t>(i);
	} catch (std::exception &) {} // safe to ignore
}

// GlobSearchReq class
// -------------------
GlobSearchReq::GlobSearchReq(SearchPtr data, uint8_t proto)
: Search(data, proto) {}

GlobSearchReq::operator std::string() {
	std::string tmp(*static_cast<Search*>(this));
	tmp.erase(1, 4); // no size field in udp packets
	tmp[1] = OP_GLOBSEARCHREQ;
	return tmp; // makePacket is called by Search class already
}

// GlobSearchRes class
// -------------------
GlobSearchRes::GlobSearchRes(std::istream &i) : SearchResult() {
	while (i) {
		readResult(i);
		if (i.peek() == PR_ED2K) {
			(void)Utils::getVal<uint8_t>(i);
		} else {
			i.seekg(-1, std::ios::cur);
			break;
		}
		if (i.peek() == OP_GLOBSEARCHRES) {
			(void)Utils::getVal<uint8_t>(i);
		} else {
			i.seekg(-2, std::ios::cur);
			break;
		}
	}
	if (!i.eof()) {
		logDebug(boost::format(
			"Extra bytes at the end of GlobSearchRes packet: %s"
		) % Utils::hexDump(
			dynamic_cast<std::istringstream&>(i).str().substr(
				i.tellg()
			)
		));
	}
}


                      /*************************/
                      /*  Client <-> Client    */
                      /*************************/

/**
 * InterClient supported features bitset (sent in Hello/HelloAnswer packets).
 *
 * The byte-order here is little-endian (as it should be sent over net), but
 * since low-level IO methods byte-swap integers on-the-fly, this must be
 * byte-swapped prior to writing to stream on big-endian platforms for the
 * correct final output.
 */
const unsigned long SupportedFeatures = (
	(0 << 29) |        // AICH version (3 bits)
	(0 << 28) |        // Unicode support (1 bit)
	(4 << 24) |        // UDP version (4 bits)
	(0 << 20) |        // Compression version (4 bits)
	(3 << 16) |        // Secure Ident version (4 bits)
	(3 << 12) |        // Source Exchange version (4 bits)
	(2 <<  8) |        // Extended Request version (4 bits)
	(1 <<  4) |        // Comment version (1 bit)
	(0 <<  3) |        // PeerCache support (1 bit)
	(1 <<  2) |        // No view shared files allowed (1 bit)
	(0 <<  1) |        // MultiPacket support (1 bit)
	(0      )          // Preview support (1 bit)
);

// Hello class
// -----------
Hello::Hello(uint8_t proto , const std::string &modStr /* = "" */)
: Packet(proto), m_modStr(modStr) {}

Hello::Hello(std::istream &i, bool hashLen /* = true */)
: m_version(), m_muleVer(), m_udpPort(), m_features() {
	load(i, hashLen);
}
void Hello::load(std::istream &i, bool hashLen) {
	if (hashLen) {
		uint8_t hashSize = Utils::getVal<uint8_t>(i);
		if (hashSize != 16) {
			// Sanity checking
			throw InvalidPacket("Hello: hashSize != 16");
		}
	}
	m_hash = Utils::getVal<std::string>(i, 16).value();
	m_clientAddr.setAddr(Utils::getVal<uint32_t>(i));
	m_clientAddr.setPort(Utils::getVal<uint16_t>(i));
	uint32_t tagcount = Utils::getVal<uint32_t>(i);
	while (tagcount--) {
		Tag t(i);
		try {
			switch (t.getOpcode()) {
			case CT_NICK:         m_nick = t.getStr();     break;
			case CT_VERSION:      m_version = t.getInt();  break;
			case CT_PORT:         /* ignored */            break;
			case CT_MODSTR:       m_modStr = t.getStr();   break;
			case CT_MULEVERSION:  m_muleVer = t.getInt();  break;
			case CT_UDPPORTS:     m_udpPort = t.getInt();  break;
			case CT_MISCFEATURES: m_features = t.getInt(); break;
			default:
				if (t.getName() == "pr") {
					m_version |= (CS_HYBRID << 24);
				}
				break; // ignore unknown/useless tags
			}
		} catch (boost::bad_any_cast &) {
			logWarning(
				boost::format(
					"Invalid Hello Packet tag: %s"
				) % t.dump()
			);
		}
	}
	// Now comes server ip/port. NB: Servers don't send ip/port!
	try {
		m_serverAddr.setAddr(Utils::getVal<uint32_t>(i));
		m_serverAddr.setPort(Utils::getVal<uint16_t>(i));
		uint32_t tmp = Utils::getVal<uint32_t>(i);
		if (tmp == 1262767181) { // 'KDLM'
			m_version &= (CS_MLDONKEY_NEW << 24);
		} else {
			m_version &= (CS_HYBRID << 24);
		}
	} catch (Utils::ReadError&) {}

	// Post-parsing checks
	CHECK_THROW(m_clientAddr.getPort() != 0);
}
Hello::operator std::string() {
	return save(OP_HELLO, true);
}
std::string Hello::save(uint8_t opcode, bool hashLen /* = true */) {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, opcode);                // opcode
	if (hashLen) {
		Utils::putVal<uint8_t>(tmp, 16);            // hash size
	}

	// Get our own userhash from ED2K class
	Utils::putVal<std::string>(
		tmp, ED2K::instance().getHash().getData(), 16
	);

	// Get our own ip/port from ED2K class
	Utils::putVal<uint32_t>(tmp, ED2K::instance().getId());
	Utils::putVal<uint16_t>(tmp, ED2K::instance().getTcpPort());

	uint32_t tagcount = 5;

	// Note: We omit PORT tag since it is not required by ed2k specification

	// Only write modVersion if it was passed to constructor
	if (m_modStr.size()) {
		tagcount++;                                 // add modversion
	}

	Utils::putVal<uint32_t>(tmp, tagcount);             // tagcount

	if (m_modStr.size()) {
		tmp << Tag(CT_MODSTR, m_modStr);
	}

	tmp << Tag(CT_NICK, ED2K::instance().getNick());    // username
	// Mules send 0x3c, edonkey2000 clients send smth like 1000+ ...
	// and then there are some guys who send 53? Old edonkeys? anyway.
	// we pretend we are a pure-hearted mule.
	tmp << Tag(CT_VERSION, 0x3c);
	tmp << Tag(CT_MULEVERSION, VER_OWN);                // client version
	tmp << Tag(CT_UDPPORTS, ED2K::instance().getUdpPort()); // UDP port

	uint32_t features = SWAP32_ON_BE(SupportedFeatures);
	tmp << Tag(CT_MISCFEATURES, features);

	IPV4Address addr;
	try {
		addr = ::Donkey::ServerList::instance().getCurServerAddr();
	} catch (...) {
		// Nothing to do - we are probably not connected. Send 0/0
		// TODO: Is it allowed to sent 0/0 as server addr in Hello
		// TODO: packet if we are not connected?
	}
	Utils::putVal<uint32_t>(tmp, addr.getAddr());   // server ip
	Utils::putVal<uint16_t>(tmp, addr.getPort());   // server port

	return makePacket(tmp.str());
}

// HelloAnswer class
// -----------------
HelloAnswer::HelloAnswer(uint8_t proto , const std::string &modStr /* = "" */)
: Hello(proto, modStr) {}
HelloAnswer::HelloAnswer(std::istream &i) : Hello(i, false) {}
HelloAnswer::operator std::string() { return save(OP_HELLOANSWER, false); }

// MuleInfo class
// --------------
MuleInfo::MuleInfo(std::istream &i) : Packet(PR_EMULE), m_protocol(),
m_version(), m_comprVer(), m_udpVer(), m_commentVer(), m_extReqVer(),
m_srcExchVer(), m_compatCliID(), m_udpPort(), m_features(), m_opcode() {
	m_version = Utils::getVal<uint8_t>(i);
	m_protocol = Utils::getVal<uint8_t>(i);
	uint32_t tagCount = Utils::getVal<uint32_t>(i);
	while (i && tagCount--) {
		Tag t(i);
		switch (t.getOpcode()) {
			case CT_COMPRESSION:  m_comprVer    = t.getInt(); break;
			case CT_UDPVER:       m_udpVer      = t.getInt(); break;
			case CT_UDPPORT:      m_udpPort     = t.getInt(); break;
			case CT_SOURCEEXCH:   m_srcExchVer  = t.getInt(); break;
			case CT_COMMENTS:     m_commentVer  = t.getInt(); break;
			case CT_EXTREQ:       m_extReqVer   = t.getInt(); break;
			case CT_FEATURES:     m_features    = t.getInt(); break;
			case CT_COMPATCLIENT: m_compatCliID = t.getInt(); break;
			case CT_MODVERSION: {
				try {
					m_modStr = t.getStr();
					break;
				} catch (boost::bad_any_cast&) {
					try {
						boost::format fmt("ModID: %s");
						fmt % t.getInt();
						m_modStr = fmt.str();
						break;
					} catch (boost::bad_any_cast&) {
						m_modStr = "ModID: <unknown>";
						break;
					}
				}
			}
			default: break; // suppress warnings
		}
	}
}
MuleInfo::MuleInfo() : Packet(PR_EMULE), m_opcode(OP_MULEINFO) {}
MuleInfo::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, m_opcode);
	Utils::putVal<uint8_t>(tmp, 0x44);                   // Software version
	Utils::putVal<uint8_t>(tmp, 0x01);                   // Protocol version
	Utils::putVal<uint32_t>(tmp, 8);                     // Tagcount
	tmp << Tag(CT_COMPRESSION, 0x00);                    // compression
	tmp << Tag(CT_UDPVER,      0x04);                    // udp version
	tmp << Tag(CT_UDPPORT, ED2K::instance().getUdpPort()); // UDP port
	tmp << Tag(CT_SOURCEEXCH,  0x03);                    // src exchannge
	tmp << Tag(CT_COMMENTS,    0x01);                    // mh ?
	tmp << Tag(CT_EXTREQ,      0x02);                    // Extended request
	tmp << Tag(CT_FEATURES,    0x03);                    // secident only
	tmp << Tag(CT_COMPATCLIENT, CS_HYDRANODE);           // compat client
	return makePacket(tmp.str());
}


// MuleInfoAnswer class
// --------------------
MuleInfoAnswer::MuleInfoAnswer() {}
MuleInfoAnswer::MuleInfoAnswer(std::istream &i) : MuleInfo(i) {}
MuleInfoAnswer::operator std::string() {
	m_opcode = OP_MULEINFOANSWER;
	return *dynamic_cast<MuleInfo*>(this);
}

// ReqFile class
// -----------------
ReqFile::ReqFile(const Hash<ED2KHash> &h, const PartData *pd, uint16_t srcCnt)
: m_hash(h), m_srcCnt(srcCnt) {
	CHECK_THROW(!h.isEmpty());
	CHECK_THROW(pd);
	m_partMap = makePartMap(pd);
}
ReqFile::ReqFile(std::istream &i) {
	try {
		m_hash = Utils::getVal<std::string>(i, 16).value();
		readPartMap(i, &m_partMap);
		m_srcCnt = Utils::getVal<uint16_t>(i);
	} catch (Utils::ReadError&) {}
}

ReqFile::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OP_REQFILE);
	Utils::putVal<std::string>(tmp, m_hash.getData(), 16);
	writePartMap(tmp, m_partMap);
	Utils::putVal<uint16_t>(tmp, m_srcCnt);
	return makePacket(tmp.str());
}

// FileName class
// -----------------------
FileName::FileName(const Hash<ED2KHash> &h, const std::string &filename)
: m_hash(h), m_name(filename) {}
FileName::FileName(std::istream &i) {
	m_hash = Utils::getVal<std::string>(i, 16).value();
	uint16_t len = Utils::getVal<uint16_t>(i);
	m_name = Utils::getVal<std::string>(i, len);
}
FileName::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OP_FILENAME);
	Utils::putVal<std::string>(tmp, m_hash.getData(), 16);
	Utils::putVal<uint16_t>(tmp, m_name.size());
	Utils::putVal<std::string>(tmp, m_name, m_name.size());
	return makePacket(tmp.str());
}

// FileDesc class
// --------------
FileDesc::FileDesc(ED2KFile::Rating rating, const std::string &comment)
: m_rating(rating), m_comment(comment) {}
FileDesc::FileDesc(std::istream &i) {
	m_rating = static_cast<ED2KFile::Rating>(
		Utils::getVal<uint8_t>(i).value()
	);
	uint32_t len = Utils::getVal<uint32_t>(i);
	m_comment = Utils::getVal<std::string>(i, len);
}
FileDesc::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OP_FILEDESC);
	Utils::putVal<uint8_t>(tmp, m_rating);
	Utils::putVal<uint32_t>(tmp, m_comment.size());
	Utils::putVal<std::string>(tmp, m_comment, m_comment.size());
	return makePacket(tmp.str());
}

// SetReqFileId class
// ------------------
SetReqFileId::SetReqFileId(const Hash<ED2KHash> &hash) : m_hash(hash) {}
SetReqFileId::SetReqFileId(std::istream &i) {
	 m_hash = Utils::getVal<std::string>(i, 16).value();
}
SetReqFileId::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OP_SETREQFILEID);
	Utils::putVal<std::string>(tmp, m_hash.getData(), 16);
	return makePacket(tmp.str());
}

// NoFile class
// ------------
NoFile::NoFile(const Hash<ED2KHash> &hash) : m_hash(hash) {}
NoFile::NoFile(std::istream &i) {
	m_hash = Utils::getVal<std::string>(i, 16).value();
}
NoFile::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OP_REQFILE_NOFILE);
	Utils::putVal<std::string>(tmp, m_hash.getData(), 16);
	return makePacket(tmp.str());
}

// FileStatus class
// -------------------------
// Note - we can be passed 0 pointer in pd if there is no partdata - e.g.
// the file is full.
FileStatus::FileStatus(const Hash<ED2KHash> &hash, const PartData *pd)
: m_hash(hash), m_partMap(makePartMap(pd)) {
	CHECK_THROW(!hash.isEmpty());
}
FileStatus::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OP_REQFILE_STATUS);
	Utils::putVal<std::string>(tmp, m_hash.getData(), 16);
	writePartMap(tmp, m_partMap);
	return makePacket(tmp.str());
}

FileStatus::FileStatus(std::istream &i) {
	m_hash = Utils::getVal<std::string>(i, 16).value();
	readPartMap(i, &m_partMap);
}

// ReqHashSet class
// ----------------
ReqHashSet::ReqHashSet(const Hash<ED2KHash> &hash) : m_hash(hash) {}
ReqHashSet::ReqHashSet(std::istream &i)
: m_hash(Utils::getVal<std::string>(i, 16)) {}
ReqHashSet::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OP_REQHASHSET);
	Utils::putVal<std::string>(tmp, m_hash.getData(), 16);
	return makePacket(tmp.str());
}

// HashSet class
// -------------
HashSet::HashSet(ED2KHashSet *hashset) : m_tmpSet(hashset) {}
HashSet::HashSet(std::istream &i) {
	m_hashSet.reset(new ED2KHashSet());
	m_hashSet->setFileHash(Utils::getVal<std::string>(i, 16).value());
	uint16_t count = Utils::getVal<uint16_t>(i);
	while (count--) {
		m_hashSet->addChunkHash(
			Utils::getVal<std::string>(i, 16).value()
		);
	}
}
HashSet::operator std::string() {
	CHECK_THROW(m_tmpSet);
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OP_HASHSET);
	Utils::putVal<std::string>(tmp, m_tmpSet->getFileHash().getData(), 16);
	Utils::putVal<uint16_t>(tmp, m_tmpSet->getChunkCnt());
	for (uint32_t i = 0; i < m_tmpSet->getChunkCnt(); ++i) {
		Utils::putVal<std::string>(tmp, (*m_tmpSet)[i].getData(), 16);
	}
	return makePacket(tmp.str());
}

// StartUploadReq class
// --------------------
StartUploadReq::StartUploadReq() {}
StartUploadReq::StartUploadReq(const Hash<ED2KHash> &h) : m_hash(h) {}
StartUploadReq::StartUploadReq(std::istream &i) {
	// Optional
	try {
		m_hash = Utils::getVal<std::string>(i, 16).value();
	} catch (Utils::ReadError&) {}
}
StartUploadReq::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OP_STARTUPLOADREQ);
	if (!m_hash.isEmpty()) {
		Utils::putVal<std::string>(tmp, m_hash.getData(), 16);
	}
	return makePacket(tmp.str());
}

// AcceptUploadReq class
// ---------------------
AcceptUploadReq::AcceptUploadReq() {}
AcceptUploadReq::AcceptUploadReq(std::istream &) {}
AcceptUploadReq::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OP_ACCEPTUPLOADREQ);
	return makePacket(tmp.str());
}

// QueueRanking class
// ------------------
QueueRanking::QueueRanking(uint16_t rank) : m_qr(rank) {}
QueueRanking::QueueRanking(std::istream &i) {
	m_qr = Utils::getVal<uint32_t>(i);
}
QueueRanking::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OP_QUEUERANKING);
	Utils::putVal<uint32_t>(tmp, m_qr);
	return makePacket(tmp.str());
}

// MuleQueueRank class
// -------------------
MuleQueueRank::MuleQueueRank(uint16_t rank) : Packet(PR_EMULE), m_qr(rank) {}
MuleQueueRank::MuleQueueRank(std::istream &i) {
	m_qr = Utils::getVal<uint16_t>(i);
}
MuleQueueRank::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OP_MULEQUEUERANK);
	Utils::putVal<uint16_t>(tmp, m_qr);
	Utils::putVal<uint16_t>(tmp, 0); // Yes, these are needed
	Utils::putVal<uint32_t>(tmp, 0);
	Utils::putVal<uint32_t>(tmp, 0);
	return makePacket(tmp.str());
}

// ReqChunks class
// --------------
ReqChunks::ReqChunks(
	const Hash<ED2KHash> &h, const std::list<Range32> &reqparts
) : m_hash(h) {
	CHECK_THROW(!m_hash.isEmpty());
	CHECK_THROW(reqparts.size());
	CHECK_THROW(reqparts.size() < 4);
	copy(reqparts.begin(), reqparts.end(), std::back_inserter(m_reqChunks));
}
ReqChunks::ReqChunks(std::istream &i) {
	m_hash = Utils::getVal<std::string>(i, 16).value();
	boost::tuple<uint32_t, uint32_t, uint32_t> begins;
	boost::tuple<uint32_t, uint32_t, uint32_t> ends;
	begins.get<0>() = Utils::getVal<uint32_t>(i);
	begins.get<1>() = Utils::getVal<uint32_t>(i);
	begins.get<2>() = Utils::getVal<uint32_t>(i);
	ends.get<0>()   = Utils::getVal<uint32_t>(i);
	ends.get<1>()   = Utils::getVal<uint32_t>(i);
	ends.get<2>()   = Utils::getVal<uint32_t>(i);
	if (ends.get<0>()) {
		Range32 r(begins.get<0>(), ends.get<0>() - 1);
		m_reqChunks.push_back(r);
	}
	if (ends.get<1>()) {
		Range32 r(begins.get<1>(), ends.get<1>() - 1);
		m_reqChunks.push_back(r);
	}
	if (ends.get<2>()) {
		Range32 r(begins.get<2>(), ends.get<2>() - 1);
		m_reqChunks.push_back(r);
	}
}
ReqChunks::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OP_REQCHUNKS);
	Utils::putVal<std::string>(tmp, m_hash.getData(), 16);
	CHECK_THROW(m_reqChunks.size());
	Utils::putVal<uint32_t>(tmp, m_reqChunks.at(0).begin());
	if (m_reqChunks.size() > 1) {
		Utils::putVal<uint32_t>(tmp, m_reqChunks.at(1).begin());
	} else {
		Utils::putVal<uint32_t>(tmp, 0);
	}
	if (m_reqChunks.size() > 2) {
		Utils::putVal<uint32_t>(tmp, m_reqChunks.at(2).begin());
	} else {
		Utils::putVal<uint32_t>(tmp, 0);
	}
	Utils::putVal<uint32_t>(tmp, m_reqChunks.at(0).end() + 1);
	if (m_reqChunks.size() > 1) {
		Utils::putVal<uint32_t>(tmp, m_reqChunks.at(1).end() + 1);
	} else {
		Utils::putVal<uint32_t>(tmp, 0);
	}
	if (m_reqChunks.size() > 2) {
		Utils::putVal<uint32_t>(tmp, m_reqChunks.at(2).end() + 1);
	} else {
		Utils::putVal<uint32_t>(tmp, 0);
	}
	return makePacket(tmp.str());
}

// DataChunk class
// ---------------
DataChunk::DataChunk(
	Hash<ED2KHash> hash, uint32_t begin, uint32_t end,
	const std::string &data
) : m_hash(hash), m_begin(begin), m_end(end), m_data(data) {
	CHECK_THROW(m_end > m_begin);
	CHECK_THROW(m_data.size() == m_end - m_begin);
	CHECK_THROW(!hash.isEmpty());
}
DataChunk::DataChunk(std::istream &i) {
	m_hash  = Utils::getVal<std::string>(i, 16).value();
	m_begin = Utils::getVal<uint32_t>(i);
	m_end   = Utils::getVal<uint32_t>(i);
	m_data  = Utils::getVal<std::string>(i, m_end - m_begin);
}
DataChunk::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OP_SENDINGCHUNK);
	Utils::putVal<std::string>(tmp, m_hash.getData(), 16);
	Utils::putVal<uint32_t>(tmp, m_begin);
	Utils::putVal<uint32_t>(tmp, m_end);
	Utils::putVal<std::string>(tmp, m_data, m_data.size());
	return makePacket(tmp.str());
}

// PackedChunk class
// -----------------
PackedChunk::PackedChunk(
	Hash<ED2KHash> hash, uint32_t begin, uint32_t size,
	const std::string &data
) : Packet(PR_EMULE), m_hash(hash), m_begin(begin), m_size(size), m_data(data){}
// Ok, here we have a problem. Due to the packet structure implemention, we have
// no way of knowing the length of the packet at this point, and thus no way of
// knowing how much data to read. Thing is, the usual 'end' offset as used in
// DataChunk packet is used as 'total length' of the packed data, and this
// packet usually contains a roughly 10k chunk of the larger packed data part.
// So, the only way we can safely find out how much data to read from the stream
// at this point is to do some evilness. This goes against everything I believe
// in, but I see no other way - blame the protocol :(
PackedChunk::PackedChunk(std::istream &i) : Packet(PR_EMULE) {
	m_hash  = Utils::getVal<std::string>(i, 16).value();
	m_begin = Utils::getVal<uint32_t>(i);
	m_size  = Utils::getVal<uint32_t>(i);
	std::istringstream &is = dynamic_cast<std::istringstream&>(i);
	m_data  = is.str().substr(24);
}
PackedChunk::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OP_PACKEDCHUNK);
	Utils::putVal<std::string>(tmp, m_hash.getData(), 16);
	Utils::putVal<uint32_t>(tmp, m_begin);
	Utils::putVal<uint32_t>(tmp, m_size);
	Utils::putVal<std::string>(tmp, m_data, m_data.size());
	return makePacket(tmp.str());
}

// CancelTransfer class
// --------------------
CancelTransfer::CancelTransfer() {}
CancelTransfer::CancelTransfer(std::istream &) {}
CancelTransfer::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OP_CANCELTRANSFER);
	return makePacket(tmp.str());
}

// SourceExchReq class
// -------------------
SourceExchReq::SourceExchReq(const Hash<ED2KHash> &hash)
: Packet(PR_EMULE), m_hash(hash) {
	CHECK_THROW(m_hash);
}
SourceExchReq::SourceExchReq(std::istream &i) {
	m_hash = Utils::getVal<std::string>(i, 16).value();
}
SourceExchReq::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OP_REQSOURCES);
	Utils::putVal<std::string>(tmp, m_hash.getData(), 16);
	return makePacket(tmp.str());
}

// AnswerSources class
// -------------------
AnswerSources::AnswerSources(const Hash<ED2KHash> &hash, const SourceList &srcs)
: Packet(PR_EMULE), m_hash(hash), m_srcList(srcs), m_swapIds() {
	CHECK_THROW(m_hash);
}
AnswerSources::AnswerSources(std::istringstream &i) {
	m_hash = Utils::getVal<std::string>(i, 16).value();
	uint16_t cnt = Utils::getVal<uint16_t>(i);
	uint16_t itemLength = (i.str().size() - 18u) / cnt;
	CHECK_THROW(itemLength);

	while (i && cnt--) {
		Source tmp;
		if (itemLength >= 6) { // client ip/port
			tmp.get<0>() = Utils::getVal<uint32_t>(i);
			tmp.get<1>() = Utils::getVal<uint16_t>(i);
		}
		if (itemLength >= 12) { // server ip/port
			tmp.get<2>() = Utils::getVal<uint32_t>(i);
			tmp.get<3>() = Utils::getVal<uint16_t>(i);
		}
		// SrcExchv2 adds hash16 here, future things may add more
		i.seekg(itemLength - 12, std::ios::cur);
		m_srcList.push_back(tmp);
	}
}

AnswerSources::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OP_ANSWERSOURCES);
	Utils::putVal<uint16_t>(tmp, m_srcList.size());
	for (CIter i = begin(); i != end(); ++i) {
		if (m_swapIds) {
			SWAP32_ON_LE((*i).get<0>());
		}
		Utils::putVal<uint32_t>(tmp, (*i).get<0>());
		Utils::putVal<uint16_t>(tmp, (*i).get<1>());
		Utils::putVal<uint32_t>(tmp, (*i).get<2>());
		Utils::putVal<uint16_t>(tmp, (*i).get<3>());
	}
	return makePacket(tmp.str());
}

// Message class
// -------------
Message::Message(const std::string &msg) : m_message(msg) {}
Message::Message(std::istream &i) {
	uint16_t len = Utils::getVal<uint16_t>(i);
	m_message = Utils::getVal<std::string>(i, len);
}
Message::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint16_t>(tmp, m_message.size());
	Utils::putVal<std::string>(tmp, m_message, m_message.size());
	return makePacket(tmp.str());
}

// ChangeId class
// --------------
ChangeId::ChangeId(uint32_t oldId, uint32_t newId)
: m_oldId(oldId), m_newId(newId) {
	CHECK_THROW(m_oldId);
	CHECK_THROW(m_newId);
}
ChangeId::ChangeId(std::istream &i) {
	m_oldId = Utils::getVal<uint32_t>(i);
	m_newId = Utils::getVal<uint32_t>(i);

	CHECK_THROW(m_oldId);
	CHECK_THROW(m_newId);
}
ChangeId::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OP_CHANGEID);
	Utils::putVal<uint32_t>(tmp, m_oldId);
	Utils::putVal<uint32_t>(tmp, m_newId);
	return makePacket(tmp.str());
}

// SecIdentState class
// -------------------
SecIdentState::SecIdentState(::Donkey::SecIdentState s)
: Packet(PR_EMULE), m_state(s), m_challenge() {
	m_challenge = Utils::getRandom();
}
SecIdentState::SecIdentState(std::istream &i) {
	m_state = Utils::getVal<uint8_t>(i);
	m_challenge = Utils::getVal<uint32_t>(i);
}
SecIdentState::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OP_SECIDENTSTATE);
	Utils::putVal<uint8_t>(tmp, m_state);
	Utils::putVal<uint32_t>(tmp, m_challenge);
	return makePacket(tmp.str());
}

// PublicKey class
// ---------------
PublicKey::PublicKey(const ::Donkey::PublicKey &pubKey)
: Packet(PR_EMULE), m_pubKey(pubKey) {}
PublicKey::PublicKey(std::istream &i) {
	uint8_t keyLen = Utils::getVal<uint8_t>(i);
	m_pubKey = ::Donkey::PublicKey(Utils::getVal<std::string>(i, keyLen));
}
PublicKey::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OP_PUBLICKEY);
	Utils::putVal<uint8_t>(tmp, m_pubKey.size());
	Utils::putVal<std::string>(tmp, m_pubKey.c_str(), m_pubKey.size());
	return makePacket(tmp.str());
}

// Signature class
// ---------------
Signature::Signature(const std::string &sign, IpType ipType)
: Packet(PR_EMULE), m_signature(sign), m_ipType(ipType) {}

Signature::Signature(std::istream &i) : m_ipType() {
	uint8_t len = Utils::getVal<uint8_t>(i);
	m_signature = Utils::getVal<std::string>(i, len);

	// SecIdent v2
	try {
		m_ipType = Utils::getVal<uint8_t>(i);
	} catch (...) {}
}

Signature::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OP_SIGNATURE);
	Utils::putVal<uint8_t>(tmp, m_signature.size());
	Utils::putVal<std::string>(tmp, m_signature, m_signature.size());
	if (m_ipType) {
		Utils::putVal<uint8_t>(tmp, m_ipType);
	}
	return makePacket(tmp.str());
}

			/*************************
			 * Client <-> Client UDP *
			 *************************/

// ReaskFilePing class
// -------------------
ReaskFilePing::ReaskFilePing(
	const Hash<ED2KHash> &h, const PartData *pd, uint16_t srcCnt,
	uint8_t udpVersion
) : m_hash(h), m_partMap(makePartMap(pd)), m_srcCnt(srcCnt),
m_udpVersion(udpVersion) {}
ReaskFilePing::ReaskFilePing(std::istream &i) {
	m_hash = Utils::getVal<std::string>(i, 16).value();
	std::istringstream &tmp = dynamic_cast<std::istringstream&>(i);
	if (tmp.str().size() >= 20) { // UDPv4
		readPartMap(i, &m_partMap);
	}
	try { // UDPv3
		m_srcCnt = Utils::getVal<uint16_t>(i);
	} catch (...) {}
}
ReaskFilePing::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, PR_EMULE);
	Utils::putVal<uint8_t>(tmp, OP_REASKFILEPING);
	Utils::putVal<std::string>(tmp, m_hash.getData(), 16);
	if (m_udpVersion >= 4) {
		writePartMap(tmp, m_partMap);
	}
	if (m_udpVersion >= 3) {
		Utils::putVal<uint16_t>(tmp, m_srcCnt);
	}
	return tmp.str();
}

// QueueFull class
// ----------------
QueueFull::QueueFull() {}
QueueFull::QueueFull(std::istream &) {}
QueueFull::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, PR_EMULE);
	Utils::putVal<uint8_t>(tmp, OP_QUEUEFULL);
	return tmp.str();
}

// ReaskAck class
// --------------
ReaskAck::ReaskAck(const PartData *pd, uint16_t qr, uint8_t udpVersion)
: Packet(PR_EMULE), m_partMap(makePartMap(pd)), m_qr(qr),
m_udpVersion(udpVersion) {}
ReaskAck::ReaskAck(std::istream &i) {
	std::istringstream &tmp = dynamic_cast<std::istringstream&>(i);
	if (tmp.str().size() > 4) {
		readPartMap(tmp, &m_partMap);
	}
	m_qr = Utils::getVal<uint16_t>(i);
}
ReaskAck::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, PR_EMULE);
	Utils::putVal<uint8_t>(tmp, OP_REASKACK);
	if (m_udpVersion >= 4) {
		writePartMap(tmp, m_partMap);
	}
	Utils::putVal<uint16_t>(tmp, m_qr);
	return tmp.str();
}

// FileNotFound class
// ------------------
FileNotFound::FileNotFound() {}
FileNotFound::FileNotFound(std::istream &) {}
FileNotFound::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, PR_EMULE);
	Utils::putVal<uint8_t>(tmp, OP_FILENOTFOUND);
	return tmp.str();
}

// PortTest class
// --------------
PortTest::PortTest() {}
PortTest::PortTest(std::istream &) {}
PortTest::operator std::string() {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, PR_EMULE);
	Utils::putVal<uint8_t>(tmp, OP_PORTTEST);
	Utils::putVal<uint8_t>(tmp, 1);
	return tmp.str();
}

} // namespace ED2KPacket

} // end namespace Donkey
