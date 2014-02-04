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

#include "donkeypage.h"
#include "donkeypage_ui.h"
#include "main.h"
#include "customheader.h"
#include <hncgcomm/cgcomm.h>
#include <boost/bind.hpp>
#include <QSettings>
#include <limits>
#include <QPainter>
#include <QMenu>
#include <QContextMenuEvent>
#include <boost/lexical_cast.hpp>

// DonkeyStatusBar class
DonkeyStatusBar::DonkeyStatusBar(QWidget *parent) : QFrame(parent) {}

void DonkeyStatusBar::paintEvent(QPaintEvent *evt) {
//	QFrame::paintEvent(evt);
	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	if (conf.value("EnableBars").toBool()) {
		QPainter p(this);
		QPixmap img(imgDir() + "plugins/donkeyserverbar.png");
		if (!img.isNull()) {
			p.drawPixmap(
				0, 0, img.scaled(
					width(), height(), 
					Qt::IgnoreAspectRatio, 
					Qt::SmoothTransformation
				)
			);
		} else {
			logDebug("failed to load image donkeyserverbar.png");
		}
	} else {
		logDebug("Skin is disabled.");
	}
}

// ServerList class
ServerList::ServerList(QWidget *parent) : HTreeWidget(parent) {
	CustomHeader *h = new CustomHeader(
		header()->orientation(), 
		header()->parentWidget()
	);
	connect(h, SIGNAL(restoreDefaults()), SLOT(init()));
	h->setObjectName("ed2k-serverlist");
	setHeader(h);
	h->setFocusPolicy(Qt::NoFocus);
	setBackground(QPixmap(":/backgrounds/backgrounds/default.png"));
}

void ServerList::init() {
	QStringList headers;
	headers << "Name" << "Address" << "Description" << "Users" << "Files";
	headers << "Failed count" << "Preference" << "Ping"  << "Max users";
	headers << "Soft file limit" << "Hard file limit" << "Low ID users";
	setHeaderLabels(headers);

	int w = 65;
#ifndef Q_OS_WIN32
	w += 12;
#endif

	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	conf.beginGroup("ed2k-serverlist");
	header()->resizeSection( 1, conf.value( "1", w + 50).toInt());
	header()->resizeSection( 2, conf.value( "2", w + 100).toInt());
	header()->resizeSection( 3, conf.value( "3", w - 10).toInt());
	header()->resizeSection( 4, conf.value( "4", w - 5).toInt());
	header()->resizeSection( 5, conf.value( "5", w - 5).toInt());
	header()->resizeSection( 6, conf.value( "6", w).toInt());
	header()->resizeSection( 7, conf.value( "7", w - 20).toInt());
	header()->resizeSection( 8, conf.value( "8", w).toInt());
	header()->resizeSection( 9, conf.value( "9", w).toInt());
	header()->resizeSection(10, conf.value("10", w).toInt());
	header()->resizeSection(11, conf.value("11", w).toInt());
	QString key("hide/%1");
	header()->setSectionHidden( 0, conf.value(key.arg( 0), false).toBool());
	header()->setSectionHidden( 1, conf.value(key.arg( 1), false).toBool());
	header()->setSectionHidden( 2, conf.value(key.arg( 2), false).toBool());
	header()->setSectionHidden( 3, conf.value(key.arg( 3), false).toBool());
	header()->setSectionHidden( 4, conf.value(key.arg( 4), false ).toBool());
	header()->setSectionHidden( 5, conf.value(key.arg( 5), true ).toBool());
	header()->setSectionHidden( 6, conf.value(key.arg( 6), true ).toBool());
	header()->setSectionHidden( 7, conf.value(key.arg( 7), true ).toBool());
	header()->setSectionHidden( 8, conf.value(key.arg( 8), true ).toBool());
	header()->setSectionHidden( 9, conf.value(key.arg( 9), true ).toBool());
	header()->setSectionHidden(10, conf.value(key.arg(10), true ).toBool());
	header()->setSectionHidden(11, conf.value(key.arg(11), true ).toBool());
	conf.setValue( "1", w + 50);
	conf.setValue( "2", w + 100);
	conf.setValue( "3", w - 10);
	conf.setValue( "4", w - 5);
	conf.setValue( "5", w - 5);
	conf.setValue( "6", w);
	conf.setValue( "7", w - 20);
	conf.setValue( "8", w);
	conf.setValue( "9", w);
	conf.setValue("10", w);
	conf.setValue("11", w);
	conf.endGroup();
}

// ServerListItem class
ServerListItem::ServerListItem(QTreeWidget *parent) : QTreeWidgetItem(parent) {}

bool ServerListItem::operator<(const QTreeWidgetItem &o) const {
	int col = treeWidget()->header()->sortIndicatorSection();
//	Qt::SortOrder order = treeWidget()->header()->sortIndicatorOrder();
	if (col == 3 || col == 4 || col >= 7) {
		int one = text(col).toInt();
		int two = o.text(col).toInt();
//		if (order = Qt::DescendingOrder) {
//			if (!one && col == 7) {
//				one = std::numeric_limits<int>::max();
//			}
//			if (!two && col == 7) {
//				two = std::numeric_limits<int>::max();
//			}
//		}
		return one < two;
	} else {
		return text(col) < o.text(col);
	}
}

// DonkeyPage class
DonkeyPage::DonkeyPage(QWidget *parent, Engine::Modules *p) 
: QWidget(parent), m_currentServer(), m_curServerId(), m_disableSorting() {
	m_ui = new Ui::DonkeyPage;
	m_ui->setupUi(this);
	m_ui->serverList->init();

	m_modules = p;
	Engine::Modules::CIter i = p->begin();
	while (i != p->end()) {
		if (i->second->getName() == "ed2k") {
			p->getObject(i->second, "serverlist", true, 1500);
			break;
		}
		++i;
	}
	p->receivedObject.connect(
		boost::bind(&DonkeyPage::gotServers, this, _1)
	);
	p->updatedObject.connect(
		boost::bind(&DonkeyPage::updateObject, this, _1)
	);
	p->addedObject.connect(
		boost::bind(&DonkeyPage::addObject, this, _1)
	);
	p->removedObject.connect(
		boost::bind(&DonkeyPage::delObject, this, _1)
	);
	connect(
		m_ui->serverList, 
		SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), 
		SLOT(connectToServer(QTreeWidgetItem*, int))
	);
	m_ui->serverList->installEventFilter(this);
}

void DonkeyPage::gotServers(Engine::ObjectPtr obj) {
	if (obj->getName() != "serverlist") {
		return;
	}

	Engine::Object::CIter i = obj->begin();
	m_disableSorting = true;
	while (i != obj->end()) {
		if (!m_list.contains(i->second->getId())) {
			addObject(i->second);
		}
		++i;
	}
	m_disableSorting = false;
	m_serverList = obj;
	m_ui->serverList->sortItems(3, Qt::DescendingOrder);
	updateObject(m_serverList);
	updateNetworkInfo();
}

void DonkeyPage::updateObject(Engine::ObjectPtr obj) {
	if (obj == m_serverList) {
		QString status = QString::fromStdString(obj->getData(0));
		QString curServer = QString::fromStdString(obj->getData(1));
		QString id = QString::fromStdString(obj->getData(2));
		ServerListItem *cur = m_list.value(curServer.toUInt());
		if (m_currentServer) {
			for (int i = 0; i < m_currentServer->columnCount();++i){
				QFont f(m_currentServer->font(i));
				f.setBold(false);
				m_currentServer->setFont(i, f);
			}
		}
		if (cur) {
			for (int i = 0; i < cur->columnCount(); ++i) {
				QFont f(cur->font(i));
				f.setBold(true);
				cur->setFont(i, f);
			}
			m_currentServer = cur;
			m_curServerId = cur->m_data->getId();
			QString statusText(status + " to " + cur->text(0));
//			statusText += " (" +formatNumber(cur->text(3).toUInt());
//			statusText += " users and ";
//			statusText += formatNumber(cur->text(4).toUInt());
//			statusText += " files)";
			if (status != "Connecting" && status != "Logging in") {
				statusText += " with ";
				if (id.toUInt() <= 0x00ffffff) {
					statusText += "Low Id";
				} else {
					statusText += "High Id";
				}
			}
			m_ui->statusText->setText(statusText);
		} else {
			logDebug("Status: " + status);
			logDebug("CurServer: " + curServer);
			logDebug("ID: " + id);
			QString statusText(status + " to ");
			statusText += "<i>unknown server</i> ";
			if (status != "Connecting" && status != "Logging in") {
				statusText += " with ";
				if (id.toUInt() <= 0x00ffffff) {
					statusText += "Low Id";
				} else {
					statusText += "High Id";
				}
			}
			m_ui->statusText->setText(statusText);
			m_curServerId = curServer.toUInt();
			m_currentServer = 0;
		}
		return;
	}

	QMap<quint32, ServerListItem*>::iterator it =m_list.find(obj->getId());
	if (it != m_list.end()) {
		int n = 0;
		Engine::Object::DIter j = obj->dbegin();
		while (j != obj->dend()) {
			it.value()->setText(n++, QString::fromStdString(*j));
			++j;
		}
	}
	m_ui->serverList->sortItems(
		m_ui->serverList->sortColumn(),
		m_ui->serverList->header()->sortIndicatorOrder()
	);
	updateNetworkInfo();
	if (m_currentServer && obj == m_currentServer->m_data) {
		updateObject(m_serverList);
	}
}

void DonkeyPage::addObject(Engine::ObjectPtr obj) {
	if (obj->getParent() && obj->getParent()->getName() == "serverlist") {
		ServerListItem *it = new ServerListItem(m_ui->serverList);
		it->m_data = obj;
		for (int j = 3; j < m_ui->serverList->columnCount(); ++j) {
			it->setTextAlignment(j, Qt::AlignRight);
		}
		it->setIcon(0, QIcon(":/transfer/icons/clear16"));

		Engine::Object::DIter j = obj->dbegin();
		int n = 0;
		while (j != obj->dend()) {
			it->setText(n++, QString::fromStdString(*j));
			++j;
		}
		m_list[obj->getId()] = it;
		if (!m_currentServer && obj->getId() == m_curServerId) {
			updateObject(m_serverList);
		}
		if (!m_disableSorting) {
			m_ui->serverList->sortItems(
				m_ui->serverList->sortColumn(),
				m_ui->serverList->header()->sortIndicatorOrder()
			);
		}
	} else {
		logDebug("New object, but not ours.");
	}
}

void DonkeyPage::delObject(Engine::ObjectPtr obj) {
	QMap<quint32, ServerListItem*>::iterator it =m_list.find(obj->getId());
	if (it != m_list.end()) {
		if (m_currentServer == it.value()) {
			m_currentServer = 0;
		}
		delete it.value();
		m_list.erase(it);
	}
}

QString DonkeyPage::formatNumber(quint32 n) {
	QString num(QString::number(n));
	int c = 0;
	for (int i = num.size() - 1; i >= 0; --i) {
		if (++c == 3 && i) {
			num.insert(i, "'");
			c = 0;
		}
	}
	return num;
}

void DonkeyPage::updateNetworkInfo() {
	quint32 users = 0;
	quint32 files = 0;
	Q_FOREACH(ServerListItem *it, m_list.values()) {
		users += it->text(3).toUInt();
		files += it->text(4).toUInt();
	}
	m_ui->userCount->setText("Users: " + formatNumber(users));
	m_ui->fileCount->setText("Files: " + formatNumber(files));
}

void DonkeyPage::connectToServer(QTreeWidgetItem *it, int) {
	ServerListItem *server = dynamic_cast<ServerListItem*>(it);
	if (!server) {
		return;
	}
	std::map<std::string, std::string> args;
	args["id"] = boost::lexical_cast<std::string>(server->m_data->getId());
	m_serverList->doOper("connectId", args);
}

void DonkeyPage::removeServer(QTreeWidgetItem *it) {
	ServerListItem *server = dynamic_cast<ServerListItem*>(it);
	if (!server) {
		return;
	}
	std::map<std::string, std::string> args;
	args["id"] = boost::lexical_cast<std::string>(server->m_data->getId());
	m_serverList->doOper("removeId", args);
}

bool DonkeyPage::eventFilter(QObject *obj, QEvent *evt) {
	if (obj == m_ui->serverList && evt->type() == QEvent::ContextMenu) {
		QContextMenuEvent *e = dynamic_cast<QContextMenuEvent*>(evt);
		if (!e) {
			return false;
		}
		QTreeWidgetItem *it = m_ui->serverList->itemAt(
			m_ui->serverList->viewport()->mapFrom((QWidget*)obj, e->pos())
		);
		if (!it) {
			return false;
		}
		if (!m_ui->serverList->isItemSelected(it)) {
			if (e->modifiers() == Qt::NoModifier) {
				QList<QTreeWidgetItem*> items(
					m_ui->serverList->selectedItems()
				);
				Q_FOREACH(QTreeWidgetItem *i, items) {
					m_ui->serverList->setItemSelected(
						i, false
					);
				}
			}
			m_ui->serverList->setItemSelected(it, true);
		}

		QMenu menu(this);
		QAction *con = menu.addAction("Connect");
		QAction *rem = menu.addAction(
			QIcon("/transfer/icons/cancel16"), "Remove"
		);
		QAction *ret = menu.exec(e->globalPos());
		if (ret && ret == rem) {
			QList<QTreeWidgetItem*> items(
				m_ui->serverList->selectedItems()
			);
			Q_FOREACH(QTreeWidgetItem *s, items) {
				removeServer(s);
			}
		} else if (ret && ret == con) {
			connectToServer(it, 0);
		}
	}
	return false;
}

