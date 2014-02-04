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

#ifndef __CGCOMM_H__
#define __CGCOMM_H__

#include <hncore/modules.h>
#include <hncore/cgcomm/client.h>
#include <hnbase/sockets.h>

namespace CGComm {

class ModMain : public ModuleBase {
	DECLARE_MODULE(ModMain, "cgcomm");
public:
	bool onInit();
	int onExit();
	std::string getDesc() const { return "Core/GUI Communication"; }
private:
	void onIncoming(SocketServer *sock, SocketEvent evt);
	void onClientEvent(Client *c, int evt);

	SocketServer *m_listener;
	typedef std::set<Client*> ClientMap;
	ClientMap m_clients;
	typedef ClientMap::iterator Iter;
};

}

#endif
