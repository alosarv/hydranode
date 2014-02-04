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
 * \file ipfilter.cpp Implementation of IpFilterBase and IpFilter classes
 */

#include <hncore/pch.h>

#include <hnbase/bind_placeholders.h>
#include <hnbase/log.h>
#include <hnbase/utils.h>
#include <hnbase/rangelist.h>

#include <hncore/ipfilter.h>

#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>
#include <boost/spirit.hpp>
#include <boost/algorithm/string/trim.hpp>

// IpFilterBase class
// ------------------
IpFilterBase::IpFilterBase() {}
IpFilterBase::~IpFilterBase() {}

// IpFilter class
// --------------
IpFilter::IpFilter() : m_list(new RangeList32) {}
IpFilter::~IpFilter() {}
void IpFilter::load(const std::string &file) {
	std::ifstream ifs(file.c_str());
	if (!ifs) {
		boost::format fmt(
			"Unable to open file %s for reading."
		);
		throw std::runtime_error((fmt % file).str().c_str());
	}
	std::string buf;

	getline(ifs, buf);
	if (!buf.size()) {
		return;
	}
	// remove trailing newline - needed when loading \n\rEOL files on linux
	if (*--buf.end() == 0x0a) {
		buf.erase(--buf.end());
	}
	if (*--buf.end() == 0x0d) {
		buf.erase(--buf.end());
	}


	ParseFunc parseFunc = getParseFunc(buf);
	if (!parseFunc) {
		throw std::runtime_error("unknown ipfilter format");
	}

	ifs.seekg(0);
	m_list->clear();

	uint32_t line = 0;
	uint32_t read = 0;
	Utils::StopWatch s1;
	while (getline(ifs, buf)) try {
		if (buf.empty()) {
			continue;
		}
		// remove trailing newline
		if (*--buf.end() == 0x0a) {
			buf.erase(--buf.end());
		}
		if (*--buf.end() == 0x0d) {
			buf.erase(--buf.end());
		}
		parseFunc(buf);
		++read;
		++line;
	} catch (const ParseError &e) {
		boost::format fmt("%s:%s: Parse error: %s");
		logWarning(fmt % file % line % e.what());
		throw;
	}
	boost::format fmt(
		"IpFilter loaded in %fms, %d lines read, %d ranges blocked."
	);
	logMsg(fmt % s1 % read % m_list->size());
}

IpFilter::ParseFunc IpFilter::getParseFunc(const std::string &buf) {
	try {
		CHECK_THROW(buf.size() >= 4);
		parseMuleLine(buf);
		logDebug("eMule format IPFilter detected.");
		return boost::bind(&IpFilter::parseMuleLine, this, _b1);
	} catch (...) {}

	try {
		parseMldonkeyLine(buf);
		logDebug("MLDonkey format IPFilter detected.");
		return boost::bind(&IpFilter::parseMldonkeyLine, this, _b1);
	} catch (...) {}

	try {
		parseGuardianLine(buf);
		logDebug("GuardianP2P format IPFilter detected.");
		return boost::bind(&IpFilter::parseGuardianLine, this, _b1);
	} catch (...) {}

	return 0;
}

using namespace boost::spirit;
static uint32_t one = 0, two = 0;
static uint_parser<unsigned, 10, 1, 3> uint3_p;
static rule<> parse_one =
	uint3_p[assign_a((reinterpret_cast<char*>(&one))[3])] >> '.' >>
	uint3_p[assign_a((reinterpret_cast<char*>(&one))[2])] >> '.' >>
	uint3_p[assign_a((reinterpret_cast<char*>(&one))[1])] >> '.' >>
	uint3_p[assign_a((reinterpret_cast<char*>(&one))[0])];
static rule<> parse_two =
	uint3_p[assign_a((reinterpret_cast<char*>(&two))[3])] >> '.' >>
	uint3_p[assign_a((reinterpret_cast<char*>(&two))[2])] >> '.' >>
	uint3_p[assign_a((reinterpret_cast<char*>(&two))[1])] >> '.' >>
	uint3_p[assign_a((reinterpret_cast<char*>(&two))[0])];

void IpFilter::parseMldonkeyLine(const std::string &buf) {
	size_t i = buf.find_last_of(':');
	if (i == std::string::npos) {
		throw ParseError("Expected ':' token.");
	}
	std::string tmp(buf.substr(i + 1));
	if (parse(tmp.c_str(), parse_one >> '-' >> parse_two).full) {
		m_list->merge(SWAP32_ON_BE(one), SWAP32_ON_BE(two));
	} else {
		throw ParseError("unknown error");
	}
}

void IpFilter::parseMuleLine(const std::string &buf) {
	if (parse(
		buf.substr(0, 33).c_str(), parse_one >> " - " >> parse_two
	).full) {
		m_list->merge(one, two);
	} else {
		throw ParseError("unknown error");
	}
}

void IpFilter::parseGuardianLine(const std::string &buf) {
	if (buf[0] == '#') {
		return;
	}

	size_t i = buf.find_first_of(',');
	if (i == std::string::npos) {
		throw ParseError("Expected ',' token.");
	}
	std::string tmp(buf.substr(0, i));
	boost::algorithm::trim(tmp);
	parse_info<> nfo = parse(tmp.c_str(), parse_one >> '-' >> parse_two);
	if (nfo.full) {
		m_list->merge(one, two);
	} else {
		throw ParseError("unknown error");
	}
}

bool IpFilter::isAllowed(uint32_t ip) {
	return !m_list->contains(Range32(SWAP32_ON_LE(ip)));
}
