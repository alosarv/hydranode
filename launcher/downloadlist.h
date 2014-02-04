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

#ifndef __DOWNLOADLIST_H__
#define __DOWNLOADLIST_H__

#include <QTreeWidget>
#include <hncgcomm/fwd.h>
#include <boost/scoped_ptr.hpp>

class DownloadListItem;

class DownloadList : public QTreeWidget {
public:
	DownloadList(QWidget *parent);
	void init();
	void setSource(Engine::DownloadList *source);
	void updateList(
		const std::vector<Engine::DownloadInfoPtr> &list, bool complete
	);
protected:
	void resizeEvent(QResizeEvent *event);
	boost::scoped_ptr<Engine::DownloadList> m_source;
private:
	friend class DownloadListItem;
	QMap<qint32, DownloadListItem*> m_entries;
	typedef QMap<qint32, DownloadListItem*>::iterator Iter;
};

class DownloadListItem : public QTreeWidgetItem {
public:
	DownloadListItem(QTreeWidget *parent, Engine::DownloadInfoPtr data);
	~DownloadListItem();
	void update(Engine::DownloadInfoPtr data);
private:
	Engine::DownloadInfoPtr m_data;
};

#endif
