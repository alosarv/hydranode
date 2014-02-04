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

#include "downloadlist.h"
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
#include <QScrollBar>
#include <QPainter>
#include <QSettings>
#include <QFileDialog>

Engine::DownloadList* DownloadList::s_list = 0;
std::set<Engine::DownloadInfoPtr> DownloadList::s_items;

DownloadList::DownloadList(QWidget *parent) : HTreeWidget(parent) {
	CustomHeader *h = new CustomHeader(
		header()->orientation(), header()->parentWidget()
	);
	connect(h, SIGNAL(restoreDefaults()), SLOT(init()));
	setHeader(h);
	h->setObjectName("transfer");

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
	header()->setFocusPolicy(Qt::NoFocus);
	header()->setSortIndicator(2, Qt::DescendingOrder);
	header()->setMaximumHeight(20);

	QPalette palette(header()->palette());
	palette.setColor(QPalette::Midlight, QColor(0, 0, 0, 0));
	palette.setColor(QPalette::Light, QColor(0, 0, 0, 0));
	palette.setColor(QPalette::Mid, QColor(0, 0, 0, 0));
	palette.setColor(QPalette::Shadow, QColor(0, 0, 0, 0));
	header()->setPalette(palette);
}

void DownloadList::init() {
	logDebug("DownloadList: initializing");
	QStringList dlHeaders;
	dlHeaders << "Name" << "Size" << "Progress";
	dlHeaders << "Speed" << "Sources" << "Remaining" << "Avail" << "Type";
	setHeaderLabels(dlHeaders);

	int w = 65;
#ifndef Q_OS_WIN32
	w += 12;
#endif
	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	header()->resizeSection(1, conf.value("/transfer/1", w).toInt());
	header()->resizeSection(2, conf.value("/transfer/2", w + 35).toInt());
	header()->resizeSection(3, conf.value("/transfer/3", w).toInt());
	header()->resizeSection(4, conf.value("/transfer/4", w - 15).toInt());
	header()->resizeSection(5, conf.value("/transfer/5", w - 5).toInt());
	header()->resizeSection(6, conf.value("/transfer/6", w - 30).toInt());
	header()->resizeSection(7, conf.value("/transfer/7", w - 5).toInt());
	QString key("/transfer/hide/%1");
	header()->setSectionHidden(0, conf.value(key.arg(0), false).toBool());
	header()->setSectionHidden(1, conf.value(key.arg(1), false).toBool());
	header()->setSectionHidden(2, conf.value(key.arg(2), false).toBool());
	header()->setSectionHidden(3, conf.value(key.arg(3), false).toBool());
	header()->setSectionHidden(4, conf.value(key.arg(4), true ).toBool());
	header()->setSectionHidden(5, conf.value(key.arg(5), false).toBool());
	header()->setSectionHidden(6, conf.value(key.arg(6), true ).toBool());
	header()->setSectionHidden(7, conf.value(key.arg(7), true ).toBool());
	header()->setObjectName("transfer");
	conf.setValue("/transfer/1", w);
	conf.setValue("/transfer/2", w + 35);
	conf.setValue("/transfer/3", w);
	conf.setValue("/transfer/4", w - 15);
	conf.setValue("/transfer/5", w - 5);
	conf.setValue("/transfer/6", w - 30);
	conf.setValue("/transfer/7", w - 5);
}

void DownloadList::downloadLink(const QString &link) {
	s_list->downloadFromLink(link.toStdString());
}

void DownloadList::downloadFile(const QByteArray &file) {
	s_list->downloadFromFile(std::string(file.data(), file.size()));
}

void DownloadList::importDownloads(const QString &dir) {
	s_list->importDownloads(dir.toStdString());
}

void DownloadList::newCategory(QString name) {
	m_catList[name] = new DownloadListItem(this, name);
}

void DownloadList::engineConnection(bool up) {
	if (up && !s_list) {
		s_list = new Engine::DownloadList(
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
				boost::lambda::new_ptr<DownloadListItem>(),
				this, __1
			)
		);
		s_list->onUpdatedList.connect(
			boost::bind(&DownloadList::resortList, this)
		);
		s_list->onAddedList.connect(
			boost::bind(&DownloadList::resortList, this)
		);
		s_list->onUpdatedNames.connect(
			boost::bind(&DownloadList::namesUpdated, this, _1)
		);
		s_list->onUpdatedComments.connect(
			boost::bind(&DownloadList::commentsUpdated, this, _1)
		);
		s_list->onUpdatedLinks.connect(
			boost::bind(&DownloadList::linksUpdated, this, _1)
		);
		s_list->getList();
		QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
		s_list->monitor(conf.value("/transfer/UpdateRate",700).toInt());
		if (!topLevelItemCount()) {
			Q_FOREACH(Engine::DownloadInfoPtr p, s_items) {
				new DownloadListItem(this, p);
			}
		}
		conf.beginGroup("category");
		QStringList keys(conf.childKeys());
		Q_FOREACH(QString key, keys) {
			if (!m_catList.contains(key)) {
				m_catList[key] = new DownloadListItem(this,key);
			}
		}
	}
}

void DownloadList::checkAssignCat(DownloadListItem *it) {
	Q_FOREACH(DownloadListItem *cat, m_catList.values()) {
		if (cat->hasChild(it)) {
			cat->assign(it);
			break;
		}
	}
}

void DownloadList::contextMenuEvent(QContextMenuEvent *e) {
	QTreeWidgetItem *it = itemAt(e->pos());
	if (it) {
		if (!isItemSelected(it)) {
			if (e->modifiers() == Qt::NoModifier) {
				QList<QTreeWidgetItem*> items(selectedItems());
				Q_FOREACH(QTreeWidgetItem *i, items) {
					setItemSelected(i, false);
				}
			}
			setItemSelected(it, true);
		}
		DownloadListItem *d = dynamic_cast<DownloadListItem*>(it);
		Q_ASSERT(d);
		QMenu menu("Download", this);
		menu.addAction(
			QIcon(":/transfer/icons/pause16"),
			"Pause", this, SLOT(pause())
		);
		menu.addAction(
			QIcon(":/transfer/icons/stop16"),
			"Stop", this, SLOT(stop())
		);
		menu.addAction(
			QIcon(":/transfer/icons/resume16"),
			"Resume", this, SLOT(resume())
		);
		menu.addAction(
			QIcon(":/transfer/icons/cancel16"),
			"Cancel", this, SLOT(cancel())
		);
		if (d->isCategory()) {
			menu.addAction(
				QIcon(":/transfer/icons/cancel16"),
				"Remove category", this, SLOT(remCategory())
			);
		}
		menu.addSeparator();
		QAction *destAc = menu.addAction("Set destination...");
		QAction *cleanAc = menu.addAction("Cleanup file name");
		QAction *showNames = menu.addAction("Show comments");

		if (m_catList.size() && m_catList.size() <= 5) {
			menu.addSeparator();
		}
		QMenu catMenuReal("Assign to", &menu);
		QMenu *catMenu = &menu;
		if (m_catList.size() > 5) {
			catMenu = &catMenuReal;
			menu.addMenu(catMenu);
		}
		QSet<QAction*> assignActions;
		Q_FOREACH(QString name, m_catList.keys()) {
			QAction *assign = new QAction(catMenu);
			assign->setText(name);
			QFont font(assign->font());
			font.setItalic(true);
			assign->setFont(font);
			catMenu->addAction(assign);
			assignActions.insert(assign);
		}
//		if (selectedItems().size() == 1) {
//			menu.addAction("Rename", this, SLOT(rename()));
//		}
		QAction *ret = menu.exec(e->globalPos());
		if (ret == destAc) {
			QString newDest;
			if (d->getData()) {
				newDest = QString::fromStdString(
					d->getData()->getDestDir()
				);
			};
			newDest = QFileDialog::getExistingDirectory(
				window(), "Choose destination directory",
				newDest, QFileDialog::ShowDirsOnly
			);
			if (newDest.size()) {
				QList<QTreeWidgetItem*> items(selectedItems());
				Q_FOREACH(QTreeWidgetItem *i, items) {
					dynamic_cast<DownloadListItem*>(i)
					->setDest(newDest);
				}
			}
		} else if (ret == cleanAc) {
			QList<QTreeWidgetItem*> items(selectedItems());
			Q_FOREACH(QTreeWidgetItem *i, items) {
				dynamic_cast<DownloadListItem*>(i)->cleanName();
			}
		} else if (ret == showNames) {
			showComments();
		} else if (assignActions.contains(ret)) {
			DownloadListItem *cat = m_catList[ret->text()];
			if (!cat) { return; }
			QList<QTreeWidgetItem*> items(selectedItems());
			Q_FOREACH(QTreeWidgetItem *i, items) {
				cat->assign(i);
			}
		}
	}
}

void DownloadList::clear() {
	setUpdatesEnabled(false);
	for (int i = 0; i < topLevelItemCount(); ++i) {
		QTreeWidgetItem *it = topLevelItem(i);
		if (it->text(5) == "Completed" || it->text(5) == "Canceled") {
			s_items.erase(
				dynamic_cast<DownloadListItem*>(it)->getData()
			);
			delete it;
			--i;
			continue;
		}
		for (int j = 0; j < it->childCount(); ++j) {
			QTreeWidgetItem *k = it->child(j);
			QString s = k->text(5);
			if (s == "Completed" || s == "Canceled") {
				s_items.erase(
					dynamic_cast<DownloadListItem*>(
						k
					)->getData()
				);
				--j;
				delete k;
			}
		}
	}
	setUpdatesEnabled(true);
}

void DownloadList::pause() {
	QList<QTreeWidgetItem*> items(selectedItems());
	Q_FOREACH(QTreeWidgetItem *i, items) {
		if (!isItemHidden(i)) {
			dynamic_cast<DownloadListItem*>(i)->pause();
		}
	}
}

void DownloadList::stop() {
	QList<QTreeWidgetItem*> items(selectedItems());
	Q_FOREACH(QTreeWidgetItem *i, items) {
		if (!isItemHidden(i)) {
			dynamic_cast<DownloadListItem*>(i)->stop();
		}
	}
}

void DownloadList::resume() {
	QList<QTreeWidgetItem*> items(selectedItems());
	Q_FOREACH(QTreeWidgetItem *i, items) {
		if (!isItemHidden(i)) {
			dynamic_cast<DownloadListItem*>(i)->resume();
		}
	}
}

void DownloadList::cancel() {
	QList<QTreeWidgetItem*> items(selectedItems());
	QString text(
		"Are you sure you want to cancel the "
		"following downloads:\n\n"
	);
	uint32_t cnt = 0;
	Q_FOREACH(QTreeWidgetItem *i, items) {
		if (!isItemHidden(i)) {
			QString tmp((*i).data(0, Qt::DisplayRole).toString());
			text += tmp.left(100);
			if (tmp.size() > 100) {
				text += "...";
			}
			text += "\n";
		}
		if (++cnt > 20) {
			text += "... other files being canceled not shown ...";
			break;
		}
	}
	if (QMessageBox::question(
		this, "Confirm Cancel Download", text,
		QMessageBox::Yes, QMessageBox::No
	) == QMessageBox::Yes) {
		Q_FOREACH(QTreeWidgetItem *i, items) {
			if (!isItemHidden(i)) {
				dynamic_cast<DownloadListItem*>(i)->cancel();
			}
		}
	}
}

void DownloadList::remCategory() {
	QList<QTreeWidgetItem*> items(selectedItems());
	QList<DownloadListItem*> categories;
	DownloadListItem *it = 0;
	Q_FOREACH(QTreeWidgetItem *i, items) {
		it = dynamic_cast<DownloadListItem*>(i);
		if (it && it->isCategory()) {
			categories.push_back(it);
		}
	}
	QString text(
		"Are you sure you want to remove the following categories\n"
		"(all child items will be unassigned):\n"
	);
	uint32_t cnt = 0;
	Q_FOREACH(DownloadListItem *i, categories) {
		if (!isItemHidden(i)) {
			QString tmp = i->text(0);
			text += tmp.left(100);
			if (tmp.size() > 100) {
				text += "...";
			}
			text += "\n";
		}
		if (++cnt > 20) {
			text += "... other categories being ";
			text += "removed not shown ...";
			break;
		}
	}
	if (QMessageBox::question(
		this, "Confirm Removing Categories", text,
		QMessageBox::Yes, QMessageBox::No
	) == QMessageBox::Yes) {
		Q_FOREACH(DownloadListItem *i, categories) {
			i->removeCategory();
		}
	}
}

void DownloadList::rename() {
	QList<QTreeWidgetItem*> items(selectedItems());
	if (items.size() != 1) {
		return;
	}
	openPersistentEditor(items.first(), 0);
}

void DownloadList::resortList() {
	sortItems(sortColumn(), header()->sortIndicatorOrder());
}

void DownloadList::filterByText(const QString &text) {
	updateFilter(text, 0);
}

void DownloadList::filterByStatus(const QString &text) {
	if (text == "Any Status" ) {
		updateFilter("", 5);
	} else {
		updateFilter(text, 5);
	}
}

void DownloadList::filterByType(const QString &text) {
	updateFilter(text, 7);
}

void DownloadList::updateFilter(const QString &text, int col) {
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
	setUpdatesEnabled(true);
}

void DownloadList::drawRow(
	QPainter *p, const QStyleOptionViewItem &option, 
	const QModelIndex &index
) const {
	QTreeWidget::drawRow(p, option, index);

	QPixmap compBar(":/icons/greenbar");
	QPixmap availBar(":/icons/bluebar");
	QPixmap redBar(":/icons/redbar");

	if (availBar.isNull() || compBar.isNull() || redBar.isNull()) {
		return;
	}

	// input variables
	int pos = columnViewportPosition(2);
	int y = option.rect.y() + (option.rect.height() - compBar.height()) / 2;
	quint64 fSize = index.sibling(index.row(), 1).data(-1).toULongLong();
	quint64 compSize = index.sibling(index.row(), 2).data(-1).toULongLong();
	int availPerc = index.sibling(index.row(), 6).data(-1).toInt();

	QString textS = index.sibling(index.row(), 5).data(-1).toString();
	if (textS == "Paused" || textS == "Stopped") {
		availBar = redBar = QPixmap(":/icons/silverbar");
		compBar = QPixmap(":/icons/ironbar");
	}

	if (!fSize) { 
		QPixmap cBar(":/icons/cyanbar");
		p->drawPixmap(
			pos, y, cBar.scaled(columnWidth(2), cBar.height())
		);
		return; 
	} else if (fSize == compSize) { // download completed
		QPixmap yBar(":/icons/yellowbar");
		p->drawPixmap(
			pos, y, yBar.scaled(columnWidth(2), yBar.height())
		);
		return;
	}
	
	// width of different bars
	int compLen = static_cast<int>(
		columnWidth(2) * (static_cast<double>(compSize) / fSize)
	);
	int unavLen = static_cast<int>(
		columnWidth(2) * ((100 - availPerc) / 100.0)
	);
	int avLen = columnWidth(2) - compLen;
	if (unavLen > columnWidth(2) - compLen) {
		unavLen = columnWidth(2) - compLen;
	}
	unavLen > 0 ? avLen -= unavLen : unavLen = 0;

	p->drawPixmap(pos, y, compBar.scaled(compLen, compBar.height()));
	pos += compLen;
	p->drawPixmap(pos, y, availBar.scaled(avLen, availBar.height()));
	pos += avLen;
	p->drawPixmap(pos, y, redBar.scaled(unavLen, redBar.height()));

	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	if (!compSize || !conf.value("EnablePerc").toBool()) {
		return;
	}

	// text background coloring
	QRect rect(QFontMetrics(p->font()).boundingRect("99.99%"));
	rect.setWidth(rect.width() + 4);
	pos = columnViewportPosition(2) + (columnWidth(2) - rect.width()) / 2;
	if (pos < columnViewportPosition(2)) {
		return;
	}
	QPixmap back(":/icons/percback");
	p->drawPixmap(pos, y, back.scaled(rect.width(), back.height()));
	p->save();
	p->setPen(QColor(0, 0, 0, 100));
	p->drawLine(pos, y, pos, y + back.height());
	p->drawLine(pos + rect.width(), y, pos + rect.width(), y+back.height());
	p->restore();

	QString text(QString::number(compSize * 100.0 / fSize, 'f', 2) + "%");
	rect = QFontMetrics(p->font()).boundingRect(text);
	pos = columnViewportPosition(2) + (columnWidth(2) - rect.width()) / 2;
	y = option.rect.bottom() - (option.rect.height() - rect.height()) + 1;
#ifdef Q_WS_X11
	y += 4;
#endif
	p->save();
	p->setPen(Qt::white);
	p->drawText(pos, y, text);
	p->restore();
}

void DownloadList::mousePressEvent(QMouseEvent *e) {
	m_mousePos = e->pos();
	QTreeWidget::mousePressEvent(e);
}

void DownloadList::onItemClicked(QTreeWidgetItem *item, int column) {
	if (!item->childCount() || column != 0) {
		return;
	}
	QRect rect(columnViewportPosition(0), visualItemRect(item).y(), 20, 16);
	if (rect.contains(m_mousePos)) {
		setItemExpanded(item, !isItemExpanded(item));
	}
}

void DownloadList::onItemExpanded(QTreeWidgetItem *item) {
	item->setIcon(0, QIcon(":/types/icons/miinus16"));
}
void DownloadList::onItemCollapsed(QTreeWidgetItem *item) {
	item->setIcon(0, QIcon(":/types/icons/pluss16"));
}

void DownloadList::onItemDoubleClicked(QTreeWidgetItem *item, int column) {
	DownloadListItem *it = dynamic_cast<DownloadListItem*>(item);
	if (!it) {
		logDebug("Non-downloadlistitem in downloadlist???");
		return; 
	} else if (it->childCount()) {
		logDebug("Cannot run a category.");
		return;
	} else if (it->text(5) != "Completed") {
		logDebug("Refusing to open a temp file - use preview instead.");
		return;
	}

	openExternally(QString::fromStdString(it->getData()->getLocation()));
}

void DownloadList::paintEvent(QPaintEvent *evt) {
	HTreeWidget::paintEvent(evt);
//	QPainter p(viewport());
//	QPixmap btn(imgDir() + "opendetails.png");
//	if (!btn.isNull()) {
//		int posX = (window()->width() - btn.width()) / 2;
//		int posY = window()->height() - btn.height();
//		p.drawPixmap(
//			viewport()->mapFrom(window(), QPoint(posX, posY)), btn
//		);
//	}
}

// DownloadListItem class
// ----------------------

DownloadListItem::DownloadListItem(QTreeWidget *parent, const QString &catName)
: QTreeWidgetItem(parent) {
	setTextAlignment(0, Qt::AlignVCenter);
	setText(0, catName);
	setText(7, "Category");
	setColumnAlignments();
	onUpdated();

	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	QList<QVariant> items = conf.value("/category/" + catName, "").toList();
	Q_FOREACH(QVariant val, items) {
		m_childIds.insert(val.toUInt());
	}
	logDebug(QString("Loading category `%1': %2 items").arg(catName).arg(m_childIds.size()));
	DownloadListItem *it = 0;
	for (int i = 0; i < parent->topLevelItemCount(); ++i) {
		it = dynamic_cast<DownloadListItem*>(parent->topLevelItem(i));
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
	//       breaks LibraryList category settings (shared)
}

DownloadListItem::DownloadListItem(
	QTreeWidget *parent, Engine::DownloadInfoPtr data
) : QTreeWidgetItem(parent), m_data(data), m_canceled() {
	m_c1 = data->onUpdated.connect(
		boost::bind(&DownloadListItem::onUpdated, this)
	);
	m_c2 = data->onDeleted.connect(
		boost::lambda::bind(boost::lambda::delete_ptr(), this)
	);
	onUpdated();
	setColumnAlignments();
	qobject_cast<DownloadList*>(parent)->s_items.insert(data);
	if (m_data->hasChildren()) {
		// find all children and reparent to this object
		parent->window()->setUpdatesEnabled(false);
		DownloadListItem *it;
		for (int i = 0; i < parent->topLevelItemCount(); ++i) {
			it = dynamic_cast<DownloadListItem*>(
				parent->topLevelItem(i)
			);
			Q_ASSERT(it);
			if (m_data->hasChild(it->getData())) {
				addChild(parent->takeTopLevelItem(i));
				--i;
			}
		}
		if (treeWidget()->isItemExpanded(this)) {
			setIcon(0, QIcon(":/types/icons/miinus16"));
		} else {
			setIcon(0, QIcon(":/types/icons/pluss16"));
		}
		setText(7, "Category");
		parent->window()->setUpdatesEnabled(true);
	}
	qobject_cast<DownloadList*>(parent)->checkAssignCat(this);
}

DownloadListItem::~DownloadListItem() {
	destroyed();
}

void DownloadListItem::setColumnAlignments() {
	setTextAlignment(0, Qt::AlignVCenter);
	setTextAlignment(1, Qt::AlignRight  | Qt::AlignVCenter);
	setTextAlignment(2, Qt::AlignRight  | Qt::AlignVCenter);
	setTextAlignment(3, Qt::AlignRight  | Qt::AlignVCenter);
	setTextAlignment(4, Qt::AlignRight  | Qt::AlignVCenter);
	setTextAlignment(5, Qt::AlignCenter | Qt::AlignVCenter);
	setTextAlignment(6, Qt::AlignCenter | Qt::AlignVCenter);
	setTextAlignment(7, Qt::AlignCenter | Qt::AlignVCenter);
}

void DownloadListItem::onUpdated() {
	if (!m_data) {
		onUpdatedCat();
	} else {
		onUpdatedData();
	}

	if (QTreeWidgetItem::parent()) {
		DownloadListItem *it = 0;
		it = dynamic_cast<DownloadListItem*>(QTreeWidgetItem::parent());
		if (it) {
			it->onUpdated();
		}
	}
	if (text(5) == "Paused" || text(5) == "Stopped") {
		for (int i = 0; i < columnCount(); ++i) {
			setTextColor(i, Qt::darkGray);
		}
	} else {
		for (int i = 0; i < columnCount(); ++i) {
			setTextColor(i, Qt::black);
		}
	}
}

void DownloadListItem::onUpdatedData() {
	if (!m_data) {
		return;
	}
	using namespace Engine;

	setText(0, QString::fromStdString(m_data->getName()));
	setText(1, QString::number(m_data->getSize()));
	setText(2, QString::number(m_data->getCompleted()));
	setText(3, QString::number(m_data->getSpeed()));
	setText(4, QString::number(m_data->getSourceCnt()));
	setText(6, QString::number(m_data->getAvail()));

	quint64 left = m_data->getSize() - m_data->getCompleted();
	if (left) {
		if (m_data->getSpeed()) {
			setText(5, QString::number(left / m_data->getSpeed()));
		} else {
			setText(5, QString(""));
		}
	} 
	switch (m_data->getState()) {
		case Engine::DSTATE_MOVING:    setText(5, "Moving");    break;
		case Engine::DSTATE_VERIFYING: setText(5, "Verifying"); break;
		case Engine::DSTATE_CANCELED:  setText(5, "Canceled");  break;
		case Engine::DSTATE_COMPLETED: setText(5, "Completed"); break;
		case Engine::DSTATE_PAUSED:    setText(5, "Paused");    break;
		case Engine::DSTATE_STOPPED:   setText(5, "Stopped");   break;
		case Engine::DSTATE_RUNNING:
		default:                       break;
	}

	switch (getFileType(m_data->getName())) {
	case FT_AUDIO:   
			setIcon(0, QIcon(":/types/icons/soundfile16")); 
			setText(7, "Audio");
			break;
	case FT_VIDEO:   
			setIcon(0, QIcon(":/types/icons/movie16"));
			setText(7, "Video");
			break;
	case FT_IMAGE:   
			setIcon(0, QIcon(":/types/icons/image16"));
			setText(7, "Image");
			break;
	case FT_ARCHIVE: 
			setIcon(0, QIcon(":/types/icons/archive16"));
			setText(7, "Archive");
			break;
	case FT_CDDVD:   
			setIcon(0, QIcon(":/types/icons/cd-dvd16"));
			setText(7, "CD/DVD");
			break;
	case FT_DOC:     
			setIcon(0, QIcon(":/types/icons/scroll16"));
			setText(7, "Document");
			break;
	case FT_PROG:    
			setIcon(0, QIcon(":/types/icons/program16"));
			setText(7, "Application");
			break;
	case FT_UNKNOWN: 
		if (childCount()) {
			if (treeWidget()->isItemExpanded(this)) {
				setIcon(0, QIcon(":/types/icons/miinus16"));
			} else {
				setIcon(0, QIcon(":/types/icons/pluss16"));
			}
			setText(7, "Category");
		} else {
			setIcon(0, QIcon(":/types/icons/unknown"));
			setText(7, "Unknown");
		}
		break;
	}

	if (m_data->getState() == Engine::DSTATE_CANCELED) {
		treeWidget()->setItemSelected(this, false);
		deleteLater();
	}
}

void DownloadListItem::onUpdatedCat() {
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

QVariant DownloadListItem::categoryData(int column, int role) const {
	if (column == 0) { 
		return QTreeWidgetItem::data(column, role); 
	} else if (role != Qt::DisplayRole && role != -1) {
		return QTreeWidgetItem::data(column, role);
	}

	quint64 sz = 0, comp = 0, speed = 0, srcs = 0, avail = 0, eta = 0;
	bool allPaused = true, allStopped = true, allMoving = true;
	bool allVerifying = true, allCanceled = true, allComplete = true;
	for (int i = 0; i < childCount(); ++i) {
		QTreeWidgetItem *it = child(i);
		sz    += it->data(1, -1).toULongLong(); 
		comp  += it->data(2, -1).toULongLong(); 
		speed += it->data(3, -1).toULongLong(); 
		srcs  += it->data(4, -1).toULongLong(); 
		avail += it->data(6, -1).toULongLong(); 
		allPaused    &= it->text(5) == "Paused";
		allStopped   &= it->text(5) == "Stopped";
		allMoving    &= it->text(5) == "Moving";
		allVerifying &= it->text(5) == "Verifying";
		allCanceled  &= it->text(5) == "Canceled";
		allComplete  &= it->text(5) == "Completed";
	}
	if (speed && sz > comp) {
		eta = (sz - comp) / speed;
	}
	if (role == -1) {
		switch (column) {
			case 1: return sz;
			case 2: return comp;
			case 3: return speed;
			case 4: return srcs;
			case 5: 
				if (allPaused) {
					return "Paused";
				} else if (allStopped) {
					return "Stopped";
				} else if (allMoving) {
					return "Moving";
				} else if (allVerifying) {
					return "Verifying";
				} else if (allCanceled) {
					return "Canceled";
				} else if (allComplete) {
					return "Completed";
				} else {
					return eta;
				}
			case 6: return avail;
			default: break;
		}
	} else {
		switch (column) {
			case 1: return bytesToString(sz);
			case 2: return bytesToString(comp);
			case 3: return speed ? bytesToString(speed) + "/s" : "";
			case 4: 
				if (srcs) {
					return srcs;
				} else {
					return "";
				}
			case 5: 
				if (allPaused) {
					return "Paused";
				} else if (allStopped) {
					return "Stopped";
				} else {
					return secondsToString(eta, 2);
				}
			case 6: return avail;
			default: break;
		}
	}
	return QTreeWidgetItem::data(column, role);
}

QVariant DownloadListItem::data(int column, int role) const {
	if (!m_data) {
		return categoryData(column, role);
	}

	if (column > 0 && column < 7 && role == -1) {
		return QTreeWidgetItem::data(column, Qt::DisplayRole);
	} else if (role != Qt::DisplayRole) {
		return QTreeWidgetItem::data(column, role);
	}

	switch (column) {
		case 0:  return QString::fromStdString(m_data->getName());
		case 1:  return bytesToString(m_data->getSize());
		case 2:  return m_data->getCompleted() ?
				 bytesToString(m_data->getCompleted()) : "";
		case 3:  return m_data->getSpeed() ?
				bytesToString(m_data->getSpeed()) + "/s" : "";
		case 4:  return m_data->getSourceCnt() ?
				QString::number(m_data->getSourceCnt()) : "";
		case 5: {
			quint64 left = m_data->getSize()-m_data->getCompleted();
			if (left && m_data->getSpeed()) {
				return secondsToString(
					left / m_data->getSpeed(), 2
				);
			} else {
				return QTreeWidgetItem::data(column, role);
			}
		}
		default: return QTreeWidgetItem::data(column, role);
	}
}

bool DownloadListItem::operator<(const QTreeWidgetItem &other) const {
	int col = treeWidget()->header()->sortIndicatorSection();
	if (col == 2) {
		quint64 c1 = data(col, -1).toULongLong();
		quint64 c2 = other.data(col, -1).toULongLong();
		quint64 s1 = data(1, -1).toULongLong();
		quint64 s2 = other.data(1, -1).toULongLong();
		double cp1 = s1 ? c1 * 100.0 / s1 : 0.0;
		double cp2 = s2 ? c2 * 100.0 / s2 : 0.0;
		if (cp1 == cp2) {
			return text(0).toLower() > other.text(0).toLower();
		} else {
			return cp1 < cp2;
		}
	} else if (col == 1 || col > 2) {
		quint64 d1 = data(col, -1).toULongLong();
		quint64 d2 = other.data(col, -1).toULongLong();
		if (d1 == d2) {
			return text(0).toLower() > other.text(0).toLower();
		}
		return d1 < d2;
	} else {
		QString d1 = text(col).toLower();
		QString d2 = other.text(col).toLower();
		if (d1 == d2 && col != 0) {
			return text(0).toLower() > other.text(0).toLower();
		}
		return d1 < d2;
	}
}

void DownloadListItem::setDest(const QString &newDest) {
	if (m_data) {
		m_data->setDest(newDest.toStdString());
	} else {
		for (int i = 0; i < childCount(); ++i) {
			dynamic_cast<DownloadListItem*>(child(i))->setDest(
				newDest
			);
		}
	}
}

void DownloadListItem::cleanName() {
	if (m_data) {
		m_data->setName(::cleanName(text(0)).toStdString());
	} else {
		for (int i = 0; i < childCount(); ++i) {
			dynamic_cast<DownloadListItem*>(child(i))->cleanName();
		}
	}
}

void DownloadListItem::cancel() { 
	if (m_data) {
		m_data->cancel(); 
		m_canceled = true; 
	} else {
		for (int i = 0; i < childCount(); ++i) {
			dynamic_cast<DownloadListItem*>(child(i))->cancel();
		}
	}
}

void DownloadListItem::pause() {
	if (m_data) {
		m_data->pause();
	} else {
		for (int i = 0; i < childCount(); ++i) {
			dynamic_cast<DownloadListItem*>(child(i))->pause();
		}
	}
}

void DownloadListItem::stop() {
	if (m_data) {
		m_data->stop();
	} else {
		for (int i = 0; i < childCount(); ++i) {
			dynamic_cast<DownloadListItem*>(child(i))->stop();
		}
	}
}

void DownloadListItem::resume() {
	if (m_data) {
		m_data->resume();
	} else {
		for (int i = 0; i < childCount(); ++i) {
			dynamic_cast<DownloadListItem*>(child(i))->resume();
		}
		setText(5, "");
	}
}

void DownloadListItem::saveSettings() {
	if (isCategory()) {
		QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
		QList<QVariant> tmp;
		DownloadListItem *it = 0;
		for (int i = 0; i < childCount(); ++i) {
			it = dynamic_cast<DownloadListItem*>(child(i));
			if (it && it->getData()) {
				tmp.push_back(it->getData()->getId());
			}
		}
		conf.setValue("/category/" + text(0), tmp);
	}
}

void DownloadListItem::assign(QTreeWidgetItem *i) {
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
	DownloadListItem *it = dynamic_cast<DownloadListItem*>(i);
	if (it && it->getData()) {
		m_childIds.insert(it->getData()->getId());
	}

	addChild(i);
	onUpdated();
	saveSettings();
}

bool DownloadListItem::hasChild(DownloadListItem *it) {
	if (it->getData()) {
		return m_childIds.contains(it->getData()->getId());
	}
	return false;
}

void DownloadListItem::removeCategory() {
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

void DownloadListItem::updateComments() {
	if (m_data) {
		m_data->getNames();
		m_data->getComments();
	}
}
