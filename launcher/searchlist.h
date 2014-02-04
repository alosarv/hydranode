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

#ifndef __SEARCHLIST_H__
#define __SEARCHLIST_H__

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <hncgcomm/fwd.h>

/**
 * Lists search results; subclassed to automagically resize columns to viewport
 * width, and more.
 *
 * init() must be called prior to usage to initialize column headers.
 */
class SearchList : public QTreeWidget {
public:
	SearchList(QWidget *parent);
	void init();
protected:
	virtual void resizeEvent(QResizeEvent *event);
//	virtual void paintEvent(QPaintEvent *event);
//	virtual void scrollContentsBy(int dx, int dy);
private slots:
	void startDownload(QTreeWidgetItem *item, int column);
private:
//	QPixmap *m_img; // background image
};

/**
 * Item in search list
 */
class SearchListItem : public QTreeWidgetItem {
public:
	SearchListItem(QTreeWidget *parent, Engine::SearchResultPtr data);
	void download();
protected:
	virtual QVariant data(int column, int role) const;
	virtual bool operator<(const QTreeWidgetItem &other) const;
private:
	void dataChanged();
	Engine::SearchResultPtr m_data;
};

#endif
