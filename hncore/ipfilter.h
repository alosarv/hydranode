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
 * \file ipfilter.h Interface for IpFilterBase and IpFilter classes
 */

#ifndef __IPFILTER_H__
#define __IPFILTER_H__

#include <hnbase/osdep.h>
#include <hnbase/fwd.h>

#include <boost/function.hpp>

/**
 * Abstract base class for IpFilter engine, declares a pure virtual function
 * isAllowed(), which derived classes must override and implement checks for
 * filtering ip addresses.
 */
class HNCORE_EXPORT IpFilterBase {
public:
	IpFilterBase();
	virtual ~IpFilterBase();

	/**
	 * Check if a given ip address is allowed to be connected to / to
	 * connect us. The ip address is in host byte order.
	 */
	virtual bool isAllowed(uint32_t ip) = 0;
private:
	IpFilterBase(const IpFilterBase&);
	IpFilterBase& operator=(const IpFilterBase&);
};

class HNCORE_EXPORT IpFilter : public IpFilterBase {
public:
	IpFilter();
	~IpFilter();
	virtual bool isAllowed(uint32_t ip);
	void load(const std::string &file);
private:
	typedef std::runtime_error ParseError;
	typedef boost::function<void (const std::string&)> ParseFunc;

	void parseMldonkeyLine(const std::string &buf);
	void parseMuleLine(const std::string &buf);
	void parseGuardianLine(const std::string &buf);

	ParseFunc getParseFunc(const std::string &buf);
	std::auto_ptr<RangeList<Range<uint32_t> > > m_list;
};

#endif
