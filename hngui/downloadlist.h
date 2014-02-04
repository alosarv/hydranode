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

#ifndef __DOWNLOADLIST_H__
#define __DOWNLOADLIST_H__

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <hncgcomm/cgcomm.h>
#include "htreewidget.h"
#include <set>

class DownloadListItem;

class DownloadList : public HTreeWidget, public boost::signals::trackable {
	Q_OBJECT
public:
	DownloadList(QWidget *parent = 0);
	static void downloadLink(const QString &link);
	static void downloadFile(const QByteArray &file);
	static void importDownloads(const QString &dir);
	static Engine::DownloadList* getList() { return s_list; }
	void newCategory(QString name);
	void checkAssignCat(DownloadListItem *it);
Q_SIGNALS:
	void showComments();
	void commentsUpdated(Engine::DownloadInfoPtr ptr);
	void namesUpdated(Engine::DownloadInfoPtr ptr);
	void linksUpdated(Engine::DownloadInfoPtr ptr);
protected:
	void contextMenuEvent(QContextMenuEvent *evt);
	void drawRow(
		QPainter *p, const QStyleOptionViewItem &option,
		const QModelIndex &index
	) const;
	void mousePressEvent(QMouseEvent *e);
	void paintEvent(QPaintEvent *evt);
public Q_SLOTS:
	void init();

	void filterByText(const QString &text);
	void filterByStatus(const QString &text);
	void filterByType(const QString &text);

	void engineConnection(bool up);

	void clear();
	void pause();
	void stop();
	void resume();
	void cancel();
	void rename();
	void remCategory();

	void onItemClicked(QTreeWidgetItem *item, int column);
	void onItemDoubleClicked(QTreeWidgetItem *item, int column);
	void onItemExpanded(QTreeWidgetItem *item);
	void onItemCollapsed(QTreeWidgetItem *item);
private:
	void updateFilter(const QString &text, int col);
	void resortList();
	friend class DownloadListItem;
	static Engine::DownloadList *s_list;
	static std::set<Engine::DownloadInfoPtr> s_items;
	QPixmap *m_background;
	QPoint m_mousePos;
	QMap<QString, DownloadListItem*> m_catList;
};

class DownloadListItem : public QObject, public QTreeWidgetItem {
	Q_OBJECT
public:
	DownloadListItem(QTreeWidget *parent, const QString &catTitle);
	DownloadListItem(QTreeWidget *parent, Engine::DownloadInfoPtr data);
	~DownloadListItem();

	Engine::DownloadInfoPtr getData() const { return m_data;     }
	bool isCategory()                 const { return !m_data;    }
	QSet<quint16> getChildIds()       const { return m_childIds; }

	void onUpdated();
	void onUpdatedCat();
	void onUpdatedData();
	void saveSettings();
	void assign(QTreeWidgetItem *it);
	bool hasChild(DownloadListItem *it);
	void removeCategory();

	boost::signal<void()> destroyed;
public Q_SLOTS:
	void pause();
	void stop();
	void resume();
	void cancel();
	void setDest(const QString &newDest);
	void cleanName();
	void updateComments();
protected:
	QVariant data(int column, int role) const;
	bool operator<(const QTreeWidgetItem &other) const;
private:
	void setColumnAlignments();
	QVariant categoryData(int columnt, int role) const;

	Engine::DownloadInfoPtr m_data;
	boost::signals::scoped_connection m_c1, m_c2;
	bool m_canceled;
	QSet<quint16> m_childIds;

	QPixmap compBar, availBar, redBar; // cached progress bars
};

#endif
