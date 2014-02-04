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

#ifndef __SUBSYSBASE_H__
#define __SUBSYSBASE_H__

#include <hnbase/event.h>
#include <hncore/fwd.h>
#include <iosfwd>

namespace CGComm {

extern uint32_t getStableId(MetaData *md);

class SubSysBase : public Trackable {
public:
	SubSysBase(
		uint8_t subCode,
		boost::function<void (const std::string&)> sendFunc
	);

	virtual ~SubSysBase();
	virtual void handle(std::istream&) = 0;
	void sendPacket(const std::string &packet);
private:
	SubSysBase();
	SubSysBase(const SubSysBase&);
	SubSysBase& operator=(const SubSysBase&);

	// function used to send data
	boost::function<void (const std::string&)> sendData;
	uint8_t m_subCode;       // subsystem code
};

}

#endif

