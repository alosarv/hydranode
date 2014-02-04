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


#ifndef __SUB_MODULES_H__
#define __SUB_MODULES_H__

#include <hncore/cgcomm/subsysbase.h>
#include <hncore/cgcomm/opcodes.h>
#include <hncore/fwd.h>

namespace CGComm {
namespace Subsystem {

class Modules : public SubSysBase {
public:
	Modules(boost::function<void (const std::string&)> sendFunc);
	virtual void handle(std::istream &i);
private:
	void getObject(std::istream &i);
	void monitor(std::istream &i);
	void doOper(std::istream &i);
	void setData(std::istream &i);

	uint32_t sendObject(Object *obj, bool recurse, std::ostream &tmp);
	void sendNotFound(uint32_t id);
	void objUpdated(Object *obj, ObjectEvent evt);
	void sendDirtyObjects();
	std::set<Object*> m_dirty;

	void sendList(uint8_t oc = OC_LIST);
	void onLoaded(ModuleBase *mod);
	void onUnloaded(ModuleBase *mod);
	std::string writeModule(ModuleBase *mod);

	uint32_t m_monitorTimer, m_monitorObjTimer;
};

}
}
#endif
