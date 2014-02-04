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
 * \file server.cpp Implementation of Detail::Server class
 */

#include <hncore/ed2k/server.h>                 // class interface
#include <hncore/ed2k/tag.h>                    // Tags for reading/writing
#include <hncore/ed2k/opcodes.h>                // opcodes
#include <hncore/ed2k/serverlist.h>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/tokenizer.hpp>

namespace Donkey {
namespace Detail {

IMPLEMENT_EVENT_TABLE(Server, Server*, int);

// Standard constructor
Server::Server(IPV4Address addr) : Object(&ServerList::instance(), "server"),
m_addr(addr), m_failedCount(), m_preference(), m_ping(), m_lastPing(),
m_maxUsers(), m_softLimit(), m_hardLimit(), m_udpFlags(), m_users(), m_files(),
m_lowIdUsers(), m_lastUdpQuery(), m_pingInProgress(), m_challenge(),
m_globSearch()  {
	// Sanity checking
	CHECK_THROW_MSG(addr.getAddr(), "Server IP may not be null.");
	CHECK_THROW_MSG(addr.getPort(), "Server Port may not be null.");

	setName(m_addr.getStr());

	// mark all servers udp-search capable initially, so when starting
	// with empty server list, it's still possible to perform searches
	m_udpFlags |= FL_GETFILES; 
}

// Destructor
Server::~Server() {}

// Construct and load from stream
Server::Server(std::istream &i) : Object(&ServerList::instance(), "server"),
m_failedCount(), m_preference(), m_ping(), m_lastPing(), m_maxUsers(),
m_softLimit(), m_hardLimit(), m_udpFlags(), m_users(), m_files(),
m_lowIdUsers(), m_lastUdpQuery(), m_pingInProgress(), m_challenge(),
m_globSearch()  {
	m_addr.setAddr(Utils::getVal<uint32_t>(i));
	m_addr.setPort(Utils::getVal<uint16_t>(i));

	// Sanity checking
	CHECK_THROW_MSG(m_addr.getAddr(), "Invalid ip NULL found for server.");
	CHECK_THROW_MSG(m_addr.getPort(),"Invalid port NULL found for server.");

	uint32_t tagcount = Utils::getVal<uint32_t>(i);
	while (tagcount-- && i) {
		try {
			Tag t(i);
			switch (t.getOpcode()) {
				case ST_NAME:
					setName(t.getStr());
					break;
				case ST_DESC:
					m_desc = t.getStr();
					break;
				case ST_DYNIP:
					m_dynip = t.getStr();
					break;
				case ST_VERSION:
					m_version = t.getStr();
					break;
				case ST_FAIL:
					m_failedCount = t.getInt();
					break;
				case ST_PREFERENCE:
					m_preference = t.getInt();
					break;
				case ST_PING:
					m_ping = t.getInt();
					break;
				case ST_LASTPING:
					m_lastPing = t.getInt();
					break;
				case ST_MAXUSERS:
					m_maxUsers = t.getInt();
					break;
				case ST_SOFTLIMIT:
					m_softLimit = t.getInt();
					break;
				case ST_HARDLIMIT:
					m_hardLimit = t.getInt();
					break;
				case ST_UDPFLAGS:
					m_udpFlags = t.getInt();
					break;
				case ST_LOWIDUSRS:
					m_lowIdUsers = t.getInt();
					break;
				case ST_AUXPORTLIST:
					addAuxPorts(t.getStr());
					break;
				default:
					if (!t.getName().compare("users")) {
						m_users = t.getInt();
						break;
					}
					if (!t.getName().compare("files")) {
						m_files = t.getInt();
						break;
					}
					if (!t.getName().compare("maxusers")) {
						m_maxUsers = t.getInt();
						break;
					}
					if (!t.getName().compare("lowusers")) {
						m_lowIdUsers = t.getInt();
						break;
					}
					warnUnHandled("server.met", t);
					break;
			}
		} catch (TagError &er) {
			logWarning(
				boost::format("Invalid tag in server.met: %s")
				% er.what()
			);
		}
	}
	if (getName() == "server") {
		setName(m_addr.getStr());
	}
}

std::ostream& operator<<(std::ostream &o, const Server &s) {
	Utils::putVal<uint32_t>(o, s.getAddr().getAddr());
	Utils::putVal<uint16_t>(o, s.getAddr().getPort());
	std::ostringstream tmp;
	uint32_t tagcount = 0;
	if (s.getName().size()) {
		tmp << Tag(ST_NAME, s.getName());
		++tagcount;
	}
	if (s.getDesc().size()) {
		tmp << Tag(ST_DESC, s.getDesc());
		++tagcount;
	}
	if (s.getDynIp().size()) {
		tmp << Tag(ST_DYNIP, s.getDynIp());
		++tagcount;
	}
	if (s.getVersion().size()) {
		tmp << Tag(ST_VERSION, s.getVersion());
		++tagcount;
	}
	if (s.getFailedCount()) {
		tmp << Tag(ST_FAIL, s.getFailedCount());
		++tagcount;
	}
	if (s.getPreference()) {
		tmp << Tag(ST_PREFERENCE, s.getPreference());
		++tagcount;
	}
	if (s.getPing()) {
		tmp << Tag(ST_PING, s.getPing());
		++tagcount;
	}
	if (s.getLastPing()) {
		tmp << Tag(ST_LASTPING, s.getLastPing());
		++tagcount;
	}
	if (s.getMaxUsers()) {
		tmp << Tag(ST_MAXUSERS, s.getMaxUsers());
		++tagcount;
	}
	if (s.getSoftLimit()) {
		tmp << Tag(ST_SOFTLIMIT, s.getSoftLimit());
		++tagcount;
	}
	if (s.getHardLimit()) {
		tmp << Tag(ST_HARDLIMIT, s.getHardLimit());
		++tagcount;
	}
	if (s.getUdpFlags()) {
		tmp << Tag(ST_UDPFLAGS, s.getUdpFlags());
		++tagcount;
	}
	if (s.getUsers()) {
		tmp << Tag("users", s.getUsers());
		++tagcount;
	}
	if (s.getFiles()) {
		tmp << Tag("files", s.getFiles());
		++tagcount;
	}
	if (s.getMaxUsers()) {
		tmp << Tag("maxusers", s.getMaxUsers());
		++tagcount;
	}
	if (s.getLowIdUsers()) {
		tmp << Tag("lowusers", s.getLowIdUsers());
		++tagcount;
	}
	if (s.getAuxPorts().size()) {
		using namespace boost::algorithm;
		std::string buf;
		std::vector<uint16_t>::const_iterator it = s.m_auxPorts.begin();
		while (it != s.m_auxPorts.end()) {
			buf += boost::lexical_cast<std::string>(*it++) + ",";
		}
		trim_right_if(buf, is_any_of(","));
		tmp << Tag(ST_AUXPORTLIST, buf);
		++tagcount;
	}
	Utils::putVal<uint32_t>(o, tagcount);
	Utils::putVal<std::string>(o, tmp.str(), tmp.str().size());
	return o;
}

void Server::addAuxPorts(const std::string &ports) try {
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

	boost::char_separator<char> sep(",");
	tokenizer tok(ports, sep);

	for (tokenizer::iterator i = tok.begin(); i != tok.end(); ++i) {
		m_auxPorts.push_back(boost::lexical_cast<uint16_t>(*i));
	}
} catch (boost::bad_lexical_cast&) {
	logWarning("Invalid AUXPORT tag in server.met (cast failed)");
}
MSVC_ONLY(;)

uint32_t Server::getDataCount() const { return 12; }

std::string Server::getData(uint32_t num) const {
	using boost::lexical_cast;
	switch (num) {
		case  0: return m_name;
		case  1: return m_addr.getStr();
		case  2: return m_desc;
		case  3: return boost::lexical_cast<std::string>(m_users);
		case  4: return boost::lexical_cast<std::string>(m_files);
		case  5: return boost::lexical_cast<std::string>(m_failedCount);
		case  6: return boost::lexical_cast<std::string>(m_preference);
		case  7: return boost::lexical_cast<std::string>(m_ping);
		case  8: return boost::lexical_cast<std::string>(m_maxUsers);
		case  9: return boost::lexical_cast<std::string>(m_softLimit);
		case 10: return boost::lexical_cast<std::string>(m_hardLimit);
		case 11: return boost::lexical_cast<std::string>(m_lowIdUsers);
		default: return std::string();
	}
}

std::string Server::getFieldName(uint32_t num) const {
	switch (num) {
		case  0: return "Name";
		case  1: return "Address";
		case  2: return "Description";
		case  3: return "Users";
		case  4: return "Files";
		case  5: return "Failed count";
		case  6: return "Preference";
		case  7: return "Ping";
		case  8: return "Max users";
		case  9: return "Soft file limit";
		case 10: return "Hard file limit";
		case 11: return "Low ID users";
		default: return std::string();
	}
}

} // namespace Detail
} // end namespace Donkey
