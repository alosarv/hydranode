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

#ifndef __DONKEYPAGE_H__
#define __DONKEYPAGE_H__

#include <QWidget>
#include <QTreeWidgetItem>
#include <QMap>
#include <hncgcomm/fwd.h>
#include "htreewidget.h"

namespace Ui {
	class DonkeyPage;
}

class ServerList : public HTreeWidget {
	Q_OBJECT
public:
	ServerList(QWidget *parent);
public Q_SLOTS:
	void init();
};

class DonkeyStatusBar : public QFrame {
public:
	DonkeyStatusBar(QWidget *parent);
protected:
	void paintEvent(QPaintEvent *evt);
};

class ServerListItem : public QTreeWidgetItem {
public:
	ServerListItem(QTreeWidget *parent);
	bool operator<(const QTreeWidgetItem &o) const;
	Engine::ObjectPtr m_data;
};

class DonkeyPage : public QWidget {
	Q_OBJECT
public:
	DonkeyPage(QWidget *parent, Engine::Modules *p);
public Q_SLOTS:
	void connectToServer(QTreeWidgetItem *it, int col);
	void removeServer(QTreeWidgetItem *it);
protected:
	bool eventFilter(QObject *obj, QEvent *evt);
private:
	void gotServers(Engine::ObjectPtr obj);
	void updateObject(Engine::ObjectPtr obj);
	void addObject(Engine::ObjectPtr obj);
	void delObject(Engine::ObjectPtr obj);

	QString formatNumber(quint32 n);
	void updateNetworkInfo();

	Ui::DonkeyPage  *m_ui;
	Engine::Modules *m_modules;
	QMap<quint32, ServerListItem*> m_list;
	Engine::ObjectPtr m_serverList;
	ServerListItem *m_currentServer;
	quint32 m_curServerId;
	bool m_disableSorting;
};

#endif
