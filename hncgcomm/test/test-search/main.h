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
#include <hncgcomm/fwd.h>
#include <vector>

class QListViewItem;
class QPoint;

class TestSearch : public QObject {
	Q_OBJECT
public:
	TestSearch();
public slots:
	void newSearch();
	void enableGui();
	void disableGui();
	void newResults(const std::vector<Engine::SearchResultPtr> &results);
	void downloadResult(QListViewItem *res, const QPoint&, int);
private:
	void setGuiEnabled(bool state);
	Engine::Search *m_search;
};
