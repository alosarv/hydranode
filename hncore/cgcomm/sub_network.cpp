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

#include "sub_network.h"
#include "opcodes.h"
#include "tag.h"
#include <hnbase/timed_callback.h>
#include <hnbase/schedbase.h>
#include <hncore/hydranode.h>

namespace CGComm {
namespace Subsystem {

Network::Network(boost::function<void (const std::string&)> sendFunc) 
: SubSysBase(SUB_NETWORK, sendFunc), m_updateTimer() {}

void Network::handle(std::istream &i) {
	uint8_t oc = Utils::getVal<uint8_t>(i);
	switch (oc) {
		case OC_GET:     sendList(); break;
		case OC_MONITOR: monitor(i); break;
		default:         break;
	}
}

void Network::monitor(std::istream &i) {
	uint32_t timer = Utils::getVal<uint32_t>(i);
	if (!m_updateTimer && timer) {
		Utils::timedCallback(this, &Network::onUpdateTimer, timer);
		logDebug(
			boost::format(
				"CGComm: Monitoring Networking "
				"(update rate %dms)"
			) % timer
		);
	}
	m_updateTimer = timer;
}

void Network::onUpdateTimer() {
	sendList();
	if (m_updateTimer) {
		Utils::timedCallback(
			this, &Network::onUpdateTimer, m_updateTimer
		);
	}
}

void Network::sendList() {
	std::ostringstream tmp;
	uint32_t tagCount = 0;
	SchedBase &s = SchedBase::instance();
	Hydranode &h = Hydranode::instance();

	tmp << makeTag(TAG_UPSPEED,       s.getDisplayUpSpeed()),   ++tagCount;
	tmp << makeTag(TAG_DOWNSPEED,     s.getDisplayDownSpeed()), ++tagCount;
	tmp << makeTag(TAG_CONNCNT,       s.getConnCount()),        ++tagCount;
	tmp << makeTag(TAG_CONNECTINGCNT, s.getConnectingCount()),  ++tagCount;
	tmp << makeTag(TAG_TOTALUP,       h.getTotalUploaded()),    ++tagCount;
	tmp << makeTag(TAG_TOTALDOWN,     h.getTotalDownloaded()),  ++tagCount;
	tmp << makeTag(TAG_DOWNPACKETS,   s.getDownPackets()),      ++tagCount;
	tmp << makeTag(TAG_UPPACKETS,     s.getUpPackets()),        ++tagCount;
	tmp << makeTag(TAG_UPLIMIT,       s.getUpLimit()),          ++tagCount;
	tmp << makeTag(TAG_DOWNLIMIT,     s.getDownLimit()),        ++tagCount;
	tmp << makeTag(TAG_SESSUP,        s.getTotalUpstream()),    ++tagCount;
	tmp << makeTag(TAG_SESSDOWN,      s.getTotalDownstream()),  ++tagCount;
	tmp << makeTag(TAG_RUNTIMESESS,   h.getRuntime()),          ++tagCount;
	tmp << makeTag(TAG_RUNTIMETOTAL,  h.getTotalRuntime()),     ++tagCount;

	std::ostringstream tmp2;
	Utils::putVal<uint8_t>(tmp2, OC_LIST);
	Utils::putVal<uint32_t>(tmp2, tagCount);
	Utils::putVal<std::string>(tmp2, tmp.str().data(), tmp.str().size());
	
	sendPacket(tmp2.str());
}

}
}
