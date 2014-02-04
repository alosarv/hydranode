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
 * \file hnsh.h Interface for HNShell class
 */

#ifndef __HNSH_H__
#define __HNSH_H__

#include <hncore/modules.h>
#include <hncore/hnsh/shellclient.h>
#include <hnbase/ssocket.h>
#include <set>

namespace Shell {

/**
 * HNShell Module
 */
class HNShell : public ModuleBase {
	DECLARE_MODULE(HNShell, "hnsh");
public:
	virtual bool onInit();
	virtual int onExit();
	virtual std::string getDesc() const { return "Shell Interface"; }
	void removeClient(ShellClient *c);
private:
	SocketServer *m_server;
	std::set<ShellClient*> m_clients;
	void serverEventHandler(SocketServer*, SocketEvent evt);
	void onShellEvt(ShellClient *c, ShellClientEvent evt);
	void migrateSettings();
};

}

#endif
