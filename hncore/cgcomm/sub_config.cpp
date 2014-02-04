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

#include <hncore/cgcomm/sub_config.h>
#include <hncore/cgcomm/opcodes.h>
#include <hnbase/utils.h>
#include <hnbase/prefs.h>
#include <hnbase/log.h>

namespace CGComm {
namespace Subsystem {

Config::Config(
	boost::function<void (const std::string&)> sendFunc
) : SubSysBase(SUB_CONFIG, sendFunc), m_monitor() {}

void Config::handle(std::istream &i) try {
	uint8_t oc = Utils::getVal<uint8_t>(i);
	switch (oc) {
		case OC_GET:
			getValue(i);
			break;
		case OC_SET:
			setValue(i);
			break;
		case OC_LIST:
			getList();
			break;
		case OC_MONITOR:
			monitor();
			break;
		default:
			break;
	}
} catch (std::exception &e) {
	logError(boost::format("CGComm.Config: %s") % e.what());
} MSVC_ONLY(;)

void Config::getValue(std::istream &i) {
	uint16_t sz = Utils::getVal<uint16_t>(i);
	std::string key = Utils::getVal<std::string>(i, sz);
	if (key.size() && key[0] != '/') {
		key.insert(0, "/");
	}
	std::string val = Prefs::instance().read<std::string>(key, "");

	sendValue(key.substr(1), val);
}

void Config::sendValue(const std::string &key, const std::string &val) {
	std::ostringstream tmp;

	Utils::putVal<uint8_t>(tmp, OC_DATA);
	Utils::putVal<std::string>(tmp, key);
	Utils::putVal<std::string>(tmp, val);
	sendPacket(tmp.str());
}

// note: all keys set or get here are assumed to be absolute paths
void Config::setValue(std::istream &i) {
	std::string key = Utils::getVal<std::string>(i);
	std::string val = Utils::getVal<std::string>(i);

	if (key.size() && key[0] != '/') {
		key.insert(0, "/");
	}

	if (!Prefs::instance().write<std::string>(key, val)) {
		logError(
			boost::format("CGComm: Invalid value %s for key %s.")
			% val % key
		);
	}
}

// note: we send the values here without the preceding "/" in keys
void Config::getList() {
	std::ostringstream tmp;
	Prefs::CIter it(Prefs::instance().begin());
	uint32_t cnt = 0;

	for (; it != Prefs::instance().end(); ++it, ++cnt) {
		Utils::putVal<std::string>(tmp, it->first.substr(1));
		Utils::putVal<std::string>(tmp, it->second);
	}

	std::ostringstream tmp2;
	Utils::putVal<uint8_t>(tmp2, OC_LIST);
	Utils::putVal<uint16_t>(tmp2, cnt);
	Utils::putVal<std::string>(tmp2, tmp.str(), tmp.str().size());
	sendPacket(tmp2.str());
}

void Config::monitor() {
	m_monitor = true;

	Prefs::instance().valueChanged.connect(
		boost::bind(&Config::valueChanged, this, _1, _2)
	);
}

void Config::valueChanged(const std::string &key, const std::string &val) {
	if (m_monitor) {
		sendValue(key, val);
	}
}

} // end namespace SubSystem
} // end namespace CGComm

