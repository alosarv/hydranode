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
 * \file torrentinfo.cpp       Implementation of TorrentInfo class
 */
#define BOOST_SPIRIT_DEBUG_PRINT_SOME 40

#include <hnbase/osdep.h>
#include <hnbase/utils.h>
#include <hnbase/log.h>
#include <hnbase/sha1transform.h>
#include <hncore/bt/torrentinfo.h>
#include <hncore/bt/protocol.h>
#include <boost/spirit.hpp>
#include <boost/spirit/dynamic/for.hpp>
#include <boost/spirit/actor/clear_actor.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace Bt {

using namespace boost::spirit;
using namespace boost::lambda;

TorrentInfo::TorrentInfo(const std::string &data)
: m_creationDate(), m_length(), m_chunkSize() {
	load(data);
}

TorrentInfo::TorrentInfo(std::istream &i)
: m_creationDate(), m_length(), m_chunkSize() {
	std::string data;
	char buf[10240];
	while (i) {
		i.read(buf, 10240);
		data.append(std::string(buf, i.gcount()));
	}
	load(data);
}

// the problem with this thing here is that since it doesn't correctly use
// closure variables, 'unknown' rule starts failing when nesting more than 2
// levels deep, resulting in complete parse failure in most cases, partial
// failure in other cases. What we actually need here is using phoenix closures,
// but that requires correct grammar specification instead of this macro magic
// we'r doing right now. On the other hand, as soon as Spirit 2.0 (or similar)
// is released, it should have proper closures support itself (via BLL IIRC),
// so perhaps it's wiser to wait for that and THEN update this code.
void TorrentInfo::load(const std::string &data) {
	INIT_PARSER();

	std::string pieces, tmpUrl; // chunk hashes, tmp storage for urls
	TorrentFile tmpFile;
	std::string infoDict, sha1Hash, ed2kHash, md4Hash, md5Hash, nodeAddr;
	uint16_t nodePort;
	rule<> fileDict =
		'd' >> *(  ("6:length" >> BINT(tmpFile.m_length))
			|  ("4:path" >> ch_p('l') >> +(BSTR(tmpFile.m_name))
				[push_back_a(tmpFile.m_name, '/')] >> 'e'
			   )
			|  ("10:path.utf-8" >> ch_p('l')
				>> *(BSTR(__dummy)) >> 'e')
			|  ("4:ed2k" >> BSTR(ed2kHash))
				[assign_a(tmpFile.m_ed2kHash, ed2kHash)]
				[clear_a(ed2kHash)]
			|  ("4:sha1" >> BSTR(sha1Hash))
				[assign_a(tmpFile.m_sha1Hash, sha1Hash)]
				[clear_a(sha1Hash)]
			|  ("3:md4" >> BSTR(md4Hash))
				[assign_a(tmpFile.m_md4Hash, md4Hash)]
				[clear_a(md4Hash)]
			|  ("3:md5" >> BSTR(md5Hash))
				[assign_a(tmpFile.m_md5Hash, md5Hash)]
				[clear_a(md5Hash)]
			|  unknown
			)
		>> ch_p('e')[push_back_a(m_files, tmpFile)][clear_a(tmpFile)]
	;
	rule<> infod =
		('d' >> *(  ("6:length"        >> BINT(m_length))
			 |  ("12:piece length" >> BINT(m_chunkSize))
			 |  ("4:name"          >> BSTR(m_name))
			 |  ("6:pieces"        >> BSTR(pieces))
			 |  ("5:files"         >> ch_p('l') >> +fileDict >> 'e')
			 |  unknown
			 )
		>> 'e')[assign_a(infoDict)]
	;
	rule<> node = 'l' >> BSTR(nodeAddr) >> BINT(nodePort) >> 'e';
	rule<> btorrent =
		'd' >> *(  ("8:announce"       >> BSTR(m_announceUrl))
			|  ("13:announce-list" >> ch_p('l')
				>>  +('l' >> +(BSTR(tmpUrl))
					[push_back_a(m_announceList, tmpUrl)]
					[clear_a(tmpUrl)]
				>> 'e') >> 'e')
			|  ("18:announce-list-orig"
				>> ch_p('d') >> BINT(__dummy) >> 'l'
				>> +BSTR(__dummy) >> 'e' >> 'e'
			   )
			|  ("13:creation date" >> BINT(m_creationDate))
			|  ("4:info"           >> infod)
			|  ("7:comment"        >> BSTR(m_comment))
			|  ("10:created by"    >> BSTR(m_createdBy))
			|  ("5:nodes"          >> ch_p('l')
				>> +node[push_back_a(
					m_nodes,
					IPV4Address(nodeAddr, nodePort)
				)][clear_a(nodeAddr)] >> 'e')
			|  ("11:modified-by"  >>
				ch_p('l') >> *BSTR(__dummy2) >> 'e'
			   )
			|  unknown
			)
		>> 'e' >> end_p
	;

	BOOST_SPIRIT_DEBUG_RULE(fileDict);
	BOOST_SPIRIT_DEBUG_RULE(infod);
	BOOST_SPIRIT_DEBUG_RULE(btorrent);
	BOOST_SPIRIT_DEBUG_RULE(dummyList);
	BOOST_SPIRIT_DEBUG_RULE(dummyDict);
	BOOST_SPIRIT_DEBUG_RULE(unknown);

	parse_info<> info = parse(
		data.data(), data.data() + data.size(), btorrent
	);
	if (!info.full) {
		logError("Torrent file parsing failed at:");
		logMsg("-------------------------");
		logMsg(info.stop);
//		std::ostringstream tmp; tmp << info.stop;
//		logMsg(Utils::hexDump(tmp.str()));
		logMsg("-------------------------");
	}

	Sha1Transform t;
	t.sumUp(infoDict.data(), infoDict.size());
	m_infoHash = t.getHash();

	// post-processing - calculate total length (if not known yet), and
	// trim tailing slashes from filenames (inherent from parser)
	if (!m_files.size()) {
		m_files.push_back(TorrentFile(m_name, m_length));
	} else {
		bool calcLength = !m_length;
		for (uint32_t i = 0; i < m_files.size(); ++i) {
			boost::algorithm::trim_if(m_files[i].m_name, __1 =='/');
			if (calcLength) {
				m_length += m_files[i].m_length;
			}
		}
	}

	m_hashes = HashSet<SHA1Hash>(getChunkSize());
	std::string tmpHash;
	for (uint32_t i = 0; i < pieces.size(); ++i) {
		tmpHash.push_back(pieces[i]);
		if (tmpHash.size() == 20) {
			m_hashes.addChunkHash(tmpHash);
			tmpHash.clear();
		}
	}

	bool found = false;
	for (size_t i = 0; i < m_announceList.size(); ++i) {
		if (m_announceList[i] == m_announceUrl) {
			found = true;
			break;
		}
	}
	if (!found) {
		m_announceList.push_back(m_announceUrl);
	}
}

void TorrentInfo::print() {
	logMsg(boost::format("Announce URL:  %s") % m_announceUrl);
	logMsg(
		boost::format("Creation date: %s")
		% boost::posix_time::from_time_t(m_creationDate)
	);
	logMsg(
		boost::format("Length:        %s (%d bytes)")
		% Utils::bytesToString(m_length) % m_length
	);
	logMsg(boost::format("Name:          %s") % m_name);
	logMsg(
		boost::format("Chunk size:    %s (%d bytes)")
		% Utils::bytesToString(m_chunkSize) % m_chunkSize
	);
	logMsg(boost::format("Info Hash:     %s") % m_infoHash.decode());
	logMsg(boost::format("Found %d chunk hashes.") %m_hashes.getChunkCnt());
	logMsg(boost::format("Found %d files:") % m_files.size());
	for (uint32_t i = 0; i < m_files.size(); ++i) {
		logMsg(
			boost::format("[%2d] %10d %s")
			% i % m_files[i].m_length % m_files[i].m_name
		);
		if (m_files[i].m_ed2kHash) {
			logMsg("\t\tED2K: " + m_files[i].m_ed2kHash.decode());
		}
		if (m_files[i].m_sha1Hash) {
			logMsg("\t\tSHA1: " + m_files[i].m_sha1Hash.decode());
		}
		if (m_files[i].m_md4Hash) {
			logMsg("\t\tMD4:  " + m_files[i].m_md4Hash.decode());
		}
		if (m_files[i].m_md5Hash) {
			logMsg("\t\tMD5:  " + m_files[i].m_md5Hash.decode());
		}
	}
	if (m_comment.size()) {
		logMsg("Comment:");
		logMsg("-----------------------------");
		logMsg(m_comment);
		logMsg("-----------------------------");
	}
	if (m_createdBy.size()) {
		logMsg(boost::format("Created by:    %s") % m_createdBy);
	}
}

TorrentInfo::TorrentInfo() : m_creationDate(), m_length(), m_chunkSize() {}

TorrentInfo::TorrentFile::TorrentFile() : m_length() {}

TorrentInfo::TorrentFile::TorrentFile(const std::string &name, uint64_t length)
: m_length(length), m_name(name) {}

void TorrentInfo::TorrentFile::clear() {
	m_name.clear();
	m_length = 0;
}

bool operator==(const TorrentInfo &x, const TorrentInfo &y) {
	return  x.m_announceUrl  == y.m_announceUrl  &&
		x.m_creationDate == y.m_creationDate &&
		x.m_length       == y.m_length       &&
		x.m_name         == y.m_name         &&
		x.m_chunkSize    == y.m_chunkSize    &&
		x.m_hashes       == y.m_hashes       &&
		x.m_files        == y.m_files        &&
		x.m_comment      == y.m_comment      &&
		x.m_createdBy    == y.m_createdBy    &&
		x.m_infoHash     == y.m_infoHash     &&
		x.m_announceList == y.m_announceList
	;
}

bool operator==(
	const TorrentInfo::TorrentFile &x, const TorrentInfo::TorrentFile &y
) {
	return  x.m_length   == y.m_length   &&
		x.m_name     == y.m_name     &&
		x.m_sha1Hash == y.m_sha1Hash &&
		x.m_ed2kHash == y.m_ed2kHash &&
		x.m_md4Hash  == y.m_md4Hash  &&
		x.m_md5Hash  == y.m_md5Hash
	;
}

std::ostream& operator<<(std::ostream &o, const TorrentInfo::TorrentFile &f) {
	return o << "   " << f.m_length << " | " << f.m_name<<std::endl;
}

} // end namespace Bt
