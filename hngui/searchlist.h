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

#ifndef __SEARCHLIST_H__
#define __SEARCHLIST_H__

#include <QWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include "htreewidget.h"
#include <hncgcomm/cgcomm.h>

/**
 * Lists search results; subclassed to automagically resize columns to viewport
 * width, and more.
 *
 * init() must be called prior to usage to initialize column headers.
 */
class SearchList : public HTreeWidget {
	Q_OBJECT
public:
	SearchList(QWidget *parent);
protected:
	virtual void contextMenuEvent(QContextMenuEvent *evt);
	virtual void drawRow(
		QPainter *p, const QStyleOptionViewItem &option, 
		const QModelIndex &index
	) const;
	virtual void paintEvent(QPaintEvent *evt);
private Q_SLOTS:
	void init();

	void startDownload(QTreeWidgetItem *item, int column);
	void downloadSelected();
	void downloadSelectedTo();
};

/**
 * Item in search list
 */
class SearchListItem : public QObject, public QTreeWidgetItem {
	Q_OBJECT
public:
	SearchListItem(QTreeWidget *parent, Engine::SearchResultPtr data);
	QString getDest() const;
	QString getName() const;
	void setDest(const QString &newDest);
	void setName(const QString &newName);
	Engine::SearchResultPtr getData() const { return m_data; }
public Q_SLOTS:
	void download();
	void download(const QString &dest);
protected:
	virtual QVariant data(int column, int role) const;
	virtual bool operator<(const QTreeWidgetItem &other) const;
private:
	void onUpdated();
	Engine::SearchResultPtr m_data;
	QString m_customName, m_customDest;
}; 

#endif
