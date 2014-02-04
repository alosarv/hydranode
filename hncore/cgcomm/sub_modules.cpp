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

#include <hncore/cgcomm/sub_modules.h>
#include <hncore/cgcomm/opcodes.h>
#include <hncore/cgcomm/tag.h>
#include <hncore/modules.h>
#include <hnbase/timed_callback.h>

namespace CGComm {
namespace Subsystem {

Modules::Modules(
	boost::function<void (const std::string&)> sendFunc
) : SubSysBase(SUB_MODULES, sendFunc), m_monitorTimer(), m_monitorObjTimer() {
	ModManager::instance().onModuleLoaded.connect(
		boost::bind(&Modules::onLoaded, this, _1)
	);
	ModManager::instance().onModuleUnloaded.connect(
		boost::bind(&Modules::onUnloaded, this, _1)
	);
}

void Modules::handle(std::istream &i) {
	uint8_t oc = Utils::getVal<uint8_t>(i);
	switch (oc) {
		case OC_LIST:      sendList();   break;
		case OC_MONITOR:   monitor(i);   break;
		case OC_GET:       getObject(i); break;
		case OC_SET:       setData(i);   break;
		case OC_DOOPER:    doOper(i);    break;
		default: break;
	}
}


void Modules::sendList(uint8_t oc) {
	std::ostringstream tmp;
	ModManager::MIter it = ModManager::instance().begin();
	uint32_t count = 0;
	while (it != ModManager::instance().end()) {
		std::string mod = writeModule((*it).second);
		Utils::putVal<std::string>(tmp, mod, mod.size());
		++count;
		++it;
	}
	std::ostringstream final;
	Utils::putVal<uint8_t>(final, oc);
	Utils::putVal<uint32_t>(final, count);
	Utils::putVal<std::string>(final, tmp.str(), tmp.str().size());
	sendPacket(final.str());

	if (m_monitorTimer && oc == OC_UPDATE) {
		Utils::timedCallback(
			boost::bind(&Modules::sendList, this, OC_UPDATE),
			m_monitorTimer
		);
	}
}

void Modules::monitor(std::istream &i) {
	uint32_t timer = Utils::getVal<uint32_t>(i);
	if (!m_monitorTimer && timer) {
		Utils::timedCallback(
			boost::bind(&Modules::sendList, this,OC_UPDATE),
			timer
		);
	}
	m_monitorTimer = timer;
}

std::string Modules::writeModule(ModuleBase *mod) {
	std::ostringstream tmp;
	uint16_t tagCount = 0;
	tmp << makeTag(TAG_NAME,      mod->getName()),              ++tagCount;
	tmp << makeTag(TAG_DESC,      mod->getDesc()),              ++tagCount;
	tmp << makeTag(TAG_SESSUP,    mod->getSessionUploaded()),   ++tagCount;
	tmp << makeTag(TAG_SESSDOWN,  mod->getSessionDownloaded()), ++tagCount;
	tmp << makeTag(TAG_TOTALUP,   mod->getTotalUploaded()),     ++tagCount;
	tmp << makeTag(TAG_TOTALDOWN, mod->getTotalDownloaded()),   ++tagCount;
	tmp << makeTag(TAG_UPSPEED,   mod->getUpSpeed()),           ++tagCount;
	tmp << makeTag(TAG_DOWNSPEED, mod->getDownSpeed()),         ++tagCount;
	std::ostringstream tmp2;
	Utils::putVal<uint8_t>(tmp2, OC_MODULE);
	Utils::putVal<uint32_t>(tmp2, mod->getId());
	Utils::putVal<uint16_t>(tmp2, tagCount);
	Utils::putVal<std::string>(tmp2, tmp.str(), tmp.str().size());
	return tmp2.str();
}

void Modules::onLoaded(ModuleBase *mod) {
	if (m_monitorTimer) {
		std::ostringstream tmp;
		Utils::putVal<uint8_t>(tmp, OC_ADD);
		std::string tmp2 = writeModule(mod);
		Utils::putVal<std::string>(tmp, tmp2, tmp2.size());
		sendPacket(tmp.str());
	}
}

void Modules::onUnloaded(ModuleBase *mod) {
	if (m_monitorTimer) {
		std::ostringstream tmp;
		Utils::putVal<uint8_t>(tmp, OC_REMOVE);
		std::string tmp2 = writeModule(mod);
		Utils::putVal<std::string>(tmp, tmp2, tmp2.size());
		sendPacket(tmp.str());
	}
}

void Modules::getObject(std::istream &i) {
	uint32_t    id      = Utils::getVal<uint32_t>(i);
	uint16_t    len     = Utils::getVal<uint16_t>(i);
	std::string name    = Utils::getVal<std::string>(i, len);
	bool        recurse = Utils::getVal<uint8_t>(i);
	uint32_t    timer   = Utils::getVal<uint32_t>(i);

	Object *obj = Object::findObject(id);
	if (!obj) {
		sendNotFound(id);
		return;
	} 
	for (Object::CIter i = obj->begin(); i != obj->end(); ++i) {
		if (i->second->getName() == name) {
			std::ostringstream tmp;
			uint32_t cnt = sendObject(i->second, recurse, tmp);
			std::ostringstream final;
			Utils::putVal<uint8_t>(final, OC_OBJLIST);
			Utils::putVal<uint32_t>(final, cnt);
			Utils::putVal<std::string>(
				final, tmp.str(), tmp.str().size()
			);
			sendPacket(final.str());
			break;
		}
	}

	if (timer) {
		obj->objectNotify.connect(
			boost::bind(&Modules::objUpdated, this, _1, _2)
		);
	}

	if (timer && !m_monitorObjTimer) {
		Utils::timedCallback(
			boost::bind(&Modules::sendDirtyObjects, this), timer
		);
	}
	if (timer) {
		m_monitorObjTimer = timer;
	}
}

uint32_t Modules::sendObject(Object *obj, bool recurse, std::ostream &tmp) {
	uint32_t cnt = 0;
	if (recurse) {
		for (Object::CIter i = obj->begin(); i != obj->end(); ++i) {
			cnt += sendObject(i->second, recurse, tmp);
		}
	}
	Utils::putVal<uint8_t>(tmp, OC_OBJECT);
	Utils::putVal<uint32_t>(tmp, obj->getId());
	Utils::putVal<uint16_t>(tmp, obj->getName().size());
	Utils::putVal<std::string>(tmp, obj->getName(), obj->getName().size());
	Utils::putVal<uint32_t>(tmp, obj->getDataCount());
	for (uint8_t i = 0; i < obj->getDataCount(); ++i) {
		std::string data(obj->getData(i));
		Utils::putVal<uint16_t>(tmp, data.size());
		Utils::putVal<std::string>(tmp, data, data.size());
	}
	Utils::putVal<uint32_t>(tmp, obj->getChildCount());
	for (Object::CIter i = obj->begin(); i != obj->end(); ++i) {
		Utils::putVal<uint32_t>(tmp, i->second->getId());
	}

	return ++cnt;
}

void Modules::doOper(std::istream &i) {
	uint32_t id = Utils::getVal<uint32_t>(i);
	Object *obj = Object::findObject(id);
	if (!obj) {
		sendNotFound(id);
	} else {
		uint16_t len = Utils::getVal<uint16_t>(i);
		std::string opName = Utils::getVal<std::string>(i, len);
		uint16_t argCount = Utils::getVal<uint16_t>(i);
		std::vector<Object::Operation::Argument> args;
		while (i && argCount--) {
			uint16_t nLen = Utils::getVal<uint16_t>(i);
			std::string argName =Utils::getVal<std::string>(i,nLen);
			uint16_t vLen = Utils::getVal<uint16_t>(i);
			std::string argVal =Utils::getVal<std::string>(i, vLen);
			args.push_back(
				Object::Operation::Argument(argName, argVal)
			);
		}
		Object::Operation op(opName, args);
		obj->doOper(op);
	}
}

void Modules::setData(std::istream &i) {
	uint32_t id = Utils::getVal<uint32_t>(i);
	Object *obj = Object::findObject(id);
	if (!obj) {
		sendNotFound(id);
	} else {
		uint8_t dNum = Utils::getVal<uint8_t>(i);
		uint16_t vLen = Utils::getVal<uint16_t>(i);
		std::string value = Utils::getVal<std::string>(i, vLen);
		obj->setData(dNum, value);
	}
}

void Modules::sendNotFound(uint32_t id) {
	std::ostringstream tmp;
	Utils::putVal<uint8_t>(tmp, OC_NOTFOUND);
	Utils::putVal<uint32_t>(tmp, id);
	sendPacket(tmp.str());
}

void Modules::objUpdated(Object *obj, ObjectEvent evt) {
	if (evt == OBJ_MODIFIED) {
		m_dirty.insert(obj);
	} else if (evt == OBJ_ADDED && obj->getParent()) {
		std::ostringstream tmp;
		Utils::putVal<uint8_t>(tmp, OC_CADDED);
		Utils::putVal<uint32_t>(tmp, obj->getParent()->getId());
		sendObject(obj, false, tmp);
		sendPacket(tmp.str());
		m_dirty.erase(obj);
	} else if (evt == OBJ_REMOVED && obj->getParent()) {
		std::ostringstream tmp;
		Utils::putVal<uint8_t>(tmp, OC_CREMOVED);
		Utils::putVal<uint32_t>(tmp, obj->getParent()->getId());
		Utils::putVal<uint32_t>(tmp, obj->getId());
		sendPacket(tmp.str());
		m_dirty.erase(obj);
	}
}

void Modules::sendDirtyObjects() {
	if (m_dirty.size()) {
		std::ostringstream tmp;
		Utils::putVal<uint8_t>(tmp, OC_OBJLIST);
		Utils::putVal<uint32_t>(tmp, m_dirty.size());
		std::set<Object*>::iterator it = m_dirty.begin();
		while (it != m_dirty.end()) {
			sendObject(*it, false, tmp);
			++it;
		}
		sendPacket(tmp.str());
		m_dirty.clear();
	}

	if (m_monitorObjTimer) {
		Utils::timedCallback(
			boost::bind(&Modules::sendDirtyObjects, this),
			m_monitorObjTimer
		);
	}
}

} // end namespace Subsystem
} // end namespace CGComm
