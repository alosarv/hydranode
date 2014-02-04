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

#ifndef __LIBRARYLIST_H__
#define __LIBRARYLIST_H__

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <hncgcomm/cgcomm.h>
#include "htreewidget.h"
#include <set>

class LibraryListItem;

class LibraryList : public HTreeWidget {
	Q_OBJECT
public:
	LibraryList(QWidget *parent = 0);

	int filterColumn() const { return m_filterColumn; }
	QString filterText() const { return m_filterText; }
	void checkAssignCat(LibraryListItem *it);
	static Engine::SharedFilesList* getList() { return s_list; }
protected:
	void contextMenuEvent(QContextMenuEvent *e);
	void mousePressEvent(QMouseEvent *e);
public Q_SLOTS:
	void init();

	void engineConnection(bool up);

	void filterByText(const QString &text);
	void filterByDir(const QString &dir);
	void filterByType(const QString &type);

	void onItemClicked(QTreeWidgetItem *item, int column);
	void onItemDoubleClicked(QTreeWidgetItem *item, int column);
	void onItemExpanded(QTreeWidgetItem *item);
	void onItemCollapsed(QTreeWidgetItem *item);
private:
	void updateFilter(const QString &text, int col);
	void resortList();
	friend class LibraryListItem;
	static Engine::SharedFilesList *s_list;
	static std::set<Engine::SharedFilePtr> s_items;
	QPoint m_mousePos;
	int m_filterColumn;
	QString m_filterText;
	QMap<QString, LibraryListItem*> m_catList;
};

class LibraryListItem : public QObject, public QTreeWidgetItem {
	Q_OBJECT
public:
	LibraryListItem(QTreeWidget *parent, const QString &catTitle);
	LibraryListItem(QTreeWidget *parent, Engine::SharedFilePtr data);
	Engine::SharedFilePtr getData() const { return m_data; }
	int fileType() const { return m_fileType; }
	bool isCategory() const { return !m_data; }
	bool isPartial() const;
	void saveSettings();
	void assign(QTreeWidgetItem *it);
	bool hasChild(LibraryListItem *it);
	void removeCategory();
protected:
	QVariant data(int column, int role) const;
	bool operator<(const QTreeWidgetItem &other) const;
private:
	void onUpdated();
	void onUpdatedCat();
	void onUpdatedData();
	void setColumnAlignments();
	QVariant categoryData(int columnt, int role) const;

	Engine::SharedFilePtr m_data;
	boost::signals::scoped_connection m_c1, m_c2;
	int m_fileType;
	QSet<quint16> m_childIds;
};

#endif
