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

#include <qobject.h>
#include <cgcomm/cgcomm.h>
#include <vector>
#include <ecomm.h>

class TestDownload : public QObject {
	Q_OBJECT
public:
	TestDownload();
public slots:
	void onConnected();
	void onDisconnected();
private:
	void displayList(const std::vector<Engine::DownloadInfoPtr> &list);

	EngineComm m_eComm;
	Engine::DownloadList m_dList;
};
