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

#include "librarylist.h"
#include "main.h"
#include "filetypes.h"
#include "customheader.h"
#include "ecomm.h"
#include <hnbase/lambda_placeholders.h>
#include <boost/lambda/construct.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/bind.hpp>
#include <QContextMenuEvent>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>

Engine::SharedFilesList* LibraryList::s_list = 0;
std::set<Engine::SharedFilePtr> LibraryList::s_items;

LibraryList::LibraryList(QWidget *parent) : HTreeWidget(parent), 
m_filterColumn(-1) {
	CustomHeader *h = new CustomHeader(
		header()->orientation(), header()->parentWidget()
	);
	connect(h, SIGNAL(restoreDefaults()), SLOT(init()));
	setHeader(h);
	h->setObjectName("library");

	MainWindow *m = &MainWindow::instance();
	connect(m, SIGNAL(engineConnection(bool)),SLOT(engineConnection(bool)));
	if (m->getEngine() && m->getEngine()->getMain()) {
		engineConnection(true);
	}
	setBackground(QPixmap(":/backgrounds/backgrounds/default.png"));
	connect(
		this, SIGNAL(itemClicked(QTreeWidgetItem*, int)),
		SLOT(onItemClicked(QTreeWidgetItem*, int))
	);
	connect(
		this, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)),
		SLOT(onItemDoubleClicked(QTreeWidgetItem*, int))
	);
	connect(
		this, SIGNAL(itemExpanded(QTreeWidgetItem*)),
		SLOT(onItemExpanded(QTreeWidgetItem*))
	);
	connect(
		this, SIGNAL(itemCollapsed(QTreeWidgetItem*)),
		SLOT(onItemCollapsed(QTreeWidgetItem*))
	);
}

void LibraryList::init() {
	logDebug("Librarylist initializing...");

	QStringList lHeaders;
	lHeaders << "Name" << "Size" << "Uploaded" << "Speed" << "Location";
	lHeaders << "Type";
	setHeaderLabels(lHeaders);
	header()->setFocusPolicy(Qt::NoFocus);
	header()->setMaximumHeight(20);
//	header()->setStretchLastSection(true);
//	header()->setSectionHidden(4, true);
//	header()->setSectionHidden(5, true);

	int w = 65;
#ifndef Q_OS_WIN32
	w += 12;
#endif

	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	header()->resizeSection(1, conf.value("/library/1", w).toInt());
	header()->resizeSection(2, conf.value("/library/2", w).toInt());
	header()->resizeSection(3, conf.value("/library/3", w).toInt());
	header()->resizeSection(4, conf.value("/library/4", w + 100).toInt());
	header()->resizeSection(5, conf.value("/library/5", w - 5).toInt());
	QString key("/library/hide/%1");
	header()->setSectionHidden(0, conf.value(key.arg(0), false).toBool());
	header()->setSectionHidden(1, conf.value(key.arg(1), false).toBool());
	header()->setSectionHidden(2, conf.value(key.arg(2), false).toBool());
	header()->setSectionHidden(3, conf.value(key.arg(3), false).toBool());
	header()->setSectionHidden(4, conf.value(key.arg(4), true ).toBool());
	header()->setSectionHidden(5, conf.value(key.arg(5), true ).toBool());
	header()->setObjectName("library");
	conf.setValue("/library/1", w);
	conf.setValue("/library/2", w);
	conf.setValue("/library/3", w);
	conf.setValue("/library/4", w + 100);
	conf.setValue("/library/5", w - 5);
}

void LibraryList::engineConnection(bool up) {
	if (up && !s_list) {
		s_list = new Engine::SharedFilesList(
			MainWindow::instance().getEngine()->getMain()
		);
	} else if (!up) {
		delete s_list;
		clear();
		s_list = 0;
	}
	if (s_list) {
		s_list->onAdded.connect(
			boost::lambda::bind(
				boost::lambda::new_ptr<LibraryListItem>(),
				this, __1
			)
		);
		s_list->onUpdatedList.connect(
			boost::bind(&LibraryList::resortList, this)
		);
		s_list->onAddedList.connect(
			boost::bind(&LibraryList::resortList, this)
		);
		s_list->getList();
		QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
		s_list->monitor(
			conf.value("/library/UpdateRate", 1000).toInt()
		);
		if (!topLevelItemCount()) {
			Q_FOREACH(Engine::SharedFilePtr p, s_items) {
				new LibraryListItem(this, p);
			}
		}
		conf.beginGroup("category");
		QStringList keys(conf.childKeys());
		Q_FOREACH(QString key, keys) {
			if (!m_catList.contains(key)) {
				m_catList[key] = new LibraryListItem(this, key);
			}
		}
	}
}

void LibraryList::contextMenuEvent(QContextMenuEvent *e) {
/*	QTreeWidgetItem *it = itemAt(e->pos());
	if (it) {
		LibraryListItem *l = dynamic_cast<LibraryListItem>(it);
		Q_ASSERT(l);
		QMenu menu("Shared File", this);
	}
*/
}

void LibraryList::filterByText(const QString &text) {
	updateFilter(text, 0);
}

void LibraryList::filterByDir(const QString &dir) {
	updateFilter(dir, 4);
}

void LibraryList::filterByType(const QString &type) {
	updateFilter(type, 5);
}

void LibraryList::updateFilter(const QString &text, int col) {
	setUpdatesEnabled(false);
	QStringList words = text.trimmed().split(" ");
	for (int i = 0; i < topLevelItemCount(); ++i) {
		QTreeWidgetItem *it = topLevelItem(i);
		bool hide = false;
		int hiddenChildren = 0;
		for (int j = 0; j < it->childCount(); ++j) {
			hide = false;
			Q_FOREACH(QString word, words) {
				if (!it->child(j)->text(col).contains(
					word, Qt::CaseInsensitive
				)) {
					hide = true;
					break;
				}
			}
			hiddenChildren += hide;
			setItemHidden(it->child(j), hide);
		}
		if (it->childCount()) {
			setItemHidden(it, hiddenChildren == it->childCount());
			if (!isItemHidden(it) && !isItemExpanded(it)) {
				setItemExpanded(it, true);
			}
		} else {
			hide = false;
			Q_FOREACH(QString word, words) {
				if (!it->text(col).contains(
					word, Qt::CaseInsensitive
				)) {
					hide = true;
					break;
				}
			}
			setItemHidden(it, hide);
		}
	}
	if (text.size()) {
		m_filterColumn = col;
	} else {
		m_filterColumn = -1;
	}
	setUpdatesEnabled(true);
}

void LibraryList::resortList() {
	sortItems(sortColumn(), header()->sortIndicatorOrder());
}

void LibraryList::mousePressEvent(QMouseEvent *e) {
	m_mousePos = e->pos();
	QTreeWidget::mousePressEvent(e);
}

void LibraryList::onItemClicked(QTreeWidgetItem *item, int column) {
	if (!item->childCount() || column != 0) {
		return;
	}
	QRect rect(columnViewportPosition(0), visualItemRect(item).y(), 20, 16);
	if (rect.contains(m_mousePos)) {
		setItemExpanded(item, !isItemExpanded(item));
	}
}

void LibraryList::onItemExpanded(QTreeWidgetItem *item) {
	item->setIcon(0, QIcon(":/types/icons/miinus16"));
}
void LibraryList::onItemCollapsed(QTreeWidgetItem *item) {
	item->setIcon(0, QIcon(":/types/icons/pluss16"));
}

void LibraryList::onItemDoubleClicked(QTreeWidgetItem *item, int column) {
	LibraryListItem *it = dynamic_cast<LibraryListItem*>(item);
	if (!it) {
		logDebug("Non-librarylistitem in librarylist???");
		return; 
	} else if (it->childCount()) {
		logDebug("Cannot run a category.");
		return;
	}
	
	openExternally(QString::fromStdString(it->getData()->getLocation()));
}

void LibraryList::checkAssignCat(LibraryListItem *it) {
	Q_FOREACH(LibraryListItem *cat, m_catList.values()) {
		if (cat->hasChild(it)) {
			cat->assign(it);
			break;
		}
	}
}

// LibraryListItem class
////////////////////////

LibraryListItem::LibraryListItem(QTreeWidget *parent, const QString &catName)
: QTreeWidgetItem(parent) {
	setTextAlignment(0, Qt::AlignVCenter);
	setText(0, catName);
	setText(5, "Category");
	setColumnAlignments();
	onUpdated();

	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	QList<QVariant> items = conf.value("/category/" + catName, "").toList();
	Q_FOREACH(QVariant val, items) {
		m_childIds.insert(val.toUInt());
	}
	logDebug(QString("Loading category `%1': %2 items").arg(catName).arg(m_childIds.size()));
	LibraryListItem *it = 0;
	for (int i = 0; i < parent->topLevelItemCount(); ++i) {
		it = dynamic_cast<LibraryListItem*>(parent->topLevelItem(i));
		Q_ASSERT(it);
		if (it->getData()) {
			if (m_childIds.contains(it->getData()->getId())) {
				addChild(parent->takeTopLevelItem(i));
				--i;
			}
		}
	}
	// Note: don't call saveSettings() here, it'll write empty list
	//       during startup (but fix it when files are added), however this
	//       breaks Downloadlist category settings (shared)
}

LibraryListItem::LibraryListItem(QTreeWidget *parent,Engine::SharedFilePtr data)
: QTreeWidgetItem(parent), m_data(data) {
	m_c1 = data->onUpdated.connect(
		boost::bind(&LibraryListItem::onUpdated, this)
	);
	m_c2 = data->onDeleted.connect(
		boost::lambda::bind(boost::lambda::delete_ptr(), this)
	);
	onUpdated();
	setColumnAlignments();
	qobject_cast<LibraryList*>(parent)->s_items.insert(data);
	if (m_data->hasChildren()) {
		// find all children and reparent to this object
		parent->window()->setUpdatesEnabled(false);
		LibraryListItem *it;
		for (int i = 0; i < parent->topLevelItemCount(); ++i) {
			it = dynamic_cast<LibraryListItem*>(
				parent->topLevelItem(i)
			);
			Q_ASSERT(it);
			if (m_data->hasChild(it->getData())) {
				addChild(parent->takeTopLevelItem(i));
				--i;
			}
		if (treeWidget()->isItemExpanded(this)) {
			setIcon(0, QIcon(":/types/icons/miinus16"));
		} else {
			setIcon(0, QIcon(":/types/icons/pluss16"));
		}
		setText(5, "Category");
		}
		parent->window()->setUpdatesEnabled(true);
	}
	qobject_cast<LibraryList*>(parent)->checkAssignCat(this);
}

void LibraryListItem::setColumnAlignments() {
	setTextAlignment(0, Qt::AlignVCenter);
	setTextAlignment(1, Qt::AlignRight  | Qt::AlignVCenter);
	setTextAlignment(2, Qt::AlignRight  | Qt::AlignVCenter);
	setTextAlignment(3, Qt::AlignRight  | Qt::AlignVCenter);
	setTextAlignment(4, Qt::AlignLeft   | Qt::AlignVCenter);
	setTextAlignment(5, Qt::AlignCenter | Qt::AlignVCenter);
}

void LibraryListItem::onUpdated() {
	if (!m_data) {
		onUpdatedCat();
	} else {
		onUpdatedData();
	}

	if (isPartial()) {
		for (int i = 0; i < columnCount(); ++i) {
			setTextColor(i, Qt::darkGray);
		}
	} else {
		for (int i = 0; i < columnCount(); ++i) {
			setTextColor(i, Qt::black);
		}
	}

	if (QTreeWidgetItem::parent()) {
		LibraryListItem *it = 0;
		it = dynamic_cast<LibraryListItem*>(QTreeWidgetItem::parent());
		if (it) {
			it->onUpdated();
		}
	}
}

void LibraryListItem::onUpdatedData() {
	if (!m_data) {
		return;
	}

	using namespace Engine;

	setText(0, QString::fromStdString(m_data->getName()).toAscii());
	setText(1, QString::number(m_data->getSize()));
	setText(2, QString::number(m_data->getUploaded()));
	setText(3, QString::number(m_data->getSpeed()));
	setText(4, QString::fromStdString(m_data->getLocation()));

	m_fileType = getFileType(m_data->getName());
	switch (m_fileType) {
	case FT_AUDIO:   
			setIcon(0, QIcon(":/types/icons/soundfile16")); 
			setText(5, "Audio");
			break;
	case FT_VIDEO:   
			setIcon(0, QIcon(":/types/icons/movie16"));
			setText(5, "Video");
			break;
	case FT_IMAGE:   
			setIcon(0, QIcon(":/types/icons/image16"));
			setText(5, "Image");
			break;
	case FT_ARCHIVE: 
			setIcon(0, QIcon(":/types/icons/archive16"));
			setText(5, "Archive");
			break;
	case FT_CDDVD:   
			setIcon(0, QIcon(":/types/icons/cd-dvd16"));
			setText(5, "CD/DVD");
			break;
	case FT_DOC:     
			setIcon(0, QIcon(":/types/icons/scroll16"));
			setText(5, "Document");
			break;
	case FT_PROG:    
			setIcon(0, QIcon(":/types/icons/program16"));
			setText(5, "Application");
			break;
	case FT_UNKNOWN: 
		if (childCount()) {
			if (treeWidget()->isItemExpanded(this)) {
				setIcon(0, QIcon(":/types/icons/miinus16"));
			} else {
				setIcon(0, QIcon(":/types/icons/pluss16"));
			}
			setText(5, "Category");
		} else {
			setIcon(0, QIcon(":/types/icons/unknown"));
			setText(5, "Unknown");
		}
		break;
	}
}

void LibraryListItem::onUpdatedCat() {
	if (childCount()) {
		if (treeWidget()->isItemExpanded(this)) {
			setIcon(0, QIcon(":/types/icons/miinus16"));
		} else {
			setIcon(0, QIcon(":/types/icons/pluss16"));
		}
	} else {
		setIcon(0, QIcon(":/types/icons/unknown"));
	}
}

QVariant LibraryListItem::data(int column, int role) const {
	if (!m_data) {
		return categoryData(column, role);
	}

	if (column > 0 && column < 4 && role == -1) {
		return QTreeWidgetItem::data(column, Qt::DisplayRole);
	} else if (role != Qt::DisplayRole) {
		return QTreeWidgetItem::data(column, role);
	}

	switch (column) {
		case 0: return QString::fromStdString(m_data->getName());
		case 1: return bytesToString(m_data->getSize());
		case 2: return m_data->getUploaded() ?
				bytesToString(m_data->getUploaded()) : "";
		case 3: return m_data->getSpeed() ?
				bytesToString(m_data->getSpeed()) + "/s" : "";
		case 4: return QString::fromStdString(m_data->getLocation());
		default: return QTreeWidgetItem::data(column, role);
	}
}

QVariant LibraryListItem::categoryData(int column, int role) const {
	if (column > 0 && column < 4 && role == -1) {
		return QTreeWidgetItem::data(column, Qt::DisplayRole);
	} else if (role != Qt::DisplayRole) {
		return QTreeWidgetItem::data(column, role);
	}

	quint64 sz = 0, uploaded = 0, speed = 0;
	for (int i = 0; i < childCount(); ++i) {
		QTreeWidgetItem *it = child(i);
		sz         += it->data(1, -1).toULongLong(); 
		uploaded   += it->data(2, -1).toULongLong();
		speed      += it->data(3, -1).toULongLong();
	}
	if (role == -1) {
		switch (column) {
			case 1: return sz;
			case 2: return uploaded;
			case 3: return speed;
			default: break;
		}
	} else {
		switch (column) {
			case 1: return bytesToString(sz);
			case 2: return bytesToString(uploaded);
			case 3: return speed ? bytesToString(speed) + "/s" : "";
			default: break;
		}
	}
	return QTreeWidgetItem::data(column, role);
}

bool LibraryListItem::operator<(const QTreeWidgetItem &other) const {
	int col = treeWidget()->header()->sortIndicatorSection();
	if (col > 0 && col < 4) {
		quint64 d1 = data(col, -1).toULongLong();
		quint64 d2 = other.data(col, -1).toULongLong();
		if (d1 == d2) {
			return text(0).toLower() < other.text(0).toLower();
		}
		return d1 < d2;
	} else {
		return text(col).toLower() < other.text(col).toLower();
	}
}

void LibraryListItem::assign(QTreeWidgetItem *i) {
	if (i == this) { 
		return;
	}
	if (i->parent()) {
		i->parent()->takeChild(i->parent()->indexOfChild(i));
	} else {
		treeWidget()->takeTopLevelItem(
			treeWidget()->indexOfTopLevelItem(i)
		);
	}
	LibraryListItem *it = dynamic_cast<LibraryListItem*>(i);
	if (it && it->getData()) {
		m_childIds.insert(it->getData()->getId());
	}

	addChild(i);
	onUpdated();
	saveSettings();
}


bool LibraryListItem::hasChild(LibraryListItem *it) {
	if (it->getData()) {
		return m_childIds.contains(it->getData()->getId());
	}
	return false;
}

void LibraryListItem::removeCategory() {
	if (!isCategory()) {
		return;
	}

	while (childCount()) {
		QTreeWidgetItem *it = takeChild(0);
		treeWidget()->addTopLevelItem(it);
	}
	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	conf.remove("/category/" + text(0));
	delete this;
}

void LibraryListItem::saveSettings() {
	if (isCategory()) {
		QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
		QList<QVariant> tmp;
		LibraryListItem *it = 0;
		for (int i = 0; i < childCount(); ++i) {
			it = dynamic_cast<LibraryListItem*>(child(i));
			if (it && it->getData()) {
				tmp.push_back(it->getData()->getId());
			}
		}
		conf.setValue("/category/" + text(0), tmp);
	}
}

bool LibraryListItem::isPartial() const {
	if (m_data) {
		return m_data->getPartDataId();
	} else {
		bool ret = true;
		for (int i = 0; i < childCount(); ++i) {
			ret &= dynamic_cast<LibraryListItem*>(
				child(i)
			)->isPartial();
		}
		return ret;
	}
}
