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

#include <hncore/cgcomm/subsysbase.h>
#include <hnbase/osdep.h>
#include <hnbase/utils.h>
#include <hnbase/sockets.h>
#include <hncore/metadata.h>
#include <sstream>

namespace CGComm {

std::set<uint32_t> s_knownIds;

uint32_t getStableId(MetaData *md) {
	CHECK_THROW(md);

	MetaData::CustomIter i = md->customBegin();
	while (i != md->customEnd()) {
		if ((*i).size() >= 10) {
			if ((*i).substr(0, 9) == "StableId=") {
				return boost::lexical_cast<uint32_t>(
					(*i).substr(9)
				);
			}
		}
		++i;
	}
	uint32_t newId = 0;
	do {
		newId = Utils::getRandom();
	} while (s_knownIds.find(newId) != s_knownIds.end());

	md->addCustomData((boost::format("StableId=%d") % newId).str());
	return newId;
}

SubSysBase::SubSysBase(
	uint8_t subCode, boost::function<void (const std::string&)> sendFunc
) : sendData(sendFunc), m_subCode(subCode) {}

SubSysBase::~SubSysBase() {}

void SubSysBase::sendPacket(const std::string &data) {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, m_subCode);
	CHECK(data.size() < std::numeric_limits<uint32_t>::max());
	Utils::putVal<uint32_t>(tmp, data.size()); // packet size
	Utils::putVal<std::string>(tmp, data.data(), data.size());
	sendData(tmp.str());
}

}
