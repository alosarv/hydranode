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

/**
 * \file clientmanager.cpp     Implementation of ClientManager class
 */

#include <hncore/clientmanager.h>

ClientManager *s_mgr = 0;

ClientManager::ClientManager() : m_clientIndex(m_clients.get<0>()),
m_moduleIndex(m_clients.get<1>()), m_connectedIndex(m_clients.get<2>()),
m_sourceIndex(m_clients.get<3>()), m_queueIndex(m_clients.get<4>()),
m_uploadingIndex(m_clients.get<5>()), m_downloadingIndex(m_clients.get<6>())
{}

ClientManager& ClientManager::instance() {
	return s_mgr ? *s_mgr : *(s_mgr = new ClientManager);
}

ClientManager::~ClientManager() {
	s_mgr = 0;
}
