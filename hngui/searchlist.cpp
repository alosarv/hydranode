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

#include "searchlist.h"
#include <QHeaderView>
#include <boost/bind.hpp>
#include <QContextMenuEvent>
#include <QMenu>
#include <QFileDialog>
#include <QPainter>
#include <QSettings>
#include "filetypes.h"
#include "customheader.h"
#include "settingspage.h"
#include "ecomm.h"
#include "main.h"

extern QString bytesToString(quint64 bytes);
extern void doLogDebug(const std::string &msg);

// SearchList class
///////////////////////

SearchList::SearchList(QWidget *parent) : HTreeWidget(parent) {
	CustomHeader *h = new CustomHeader(
		header()->orientation(), header()->parentWidget()
	);
	connect(h, SIGNAL(restoreDefaults()), SLOT(init()));
	setHeader(h);
	h->setObjectName("search");
	init();
}

void SearchList::init() {
	QStringList searchHeaders;
	searchHeaders << "Name" << "Size" << "Availability" << "Length";
	searchHeaders << "Bitrate" << "Codec" << "Type";
	setHeaderLabels(searchHeaders);
	header()->setSortIndicator(2, Qt::DescendingOrder);
	header()->setFocusPolicy(Qt::NoFocus);
	header()->setMaximumHeight(20);
	
	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	int w = 70;
#ifndef Q_OS_WIN32
	w += 12;
#endif
	header()->resizeSection(1, conf.value("/search/1", w).toInt());
	header()->resizeSection(2, conf.value("/search/2", 120).toInt());
	header()->resizeSection(3, conf.value("/search/3", w - 15).toInt());
	header()->resizeSection(4, conf.value("/search/4", w - 5).toInt());
	header()->resizeSection(5, conf.value("/search/5", w - 20).toInt());
	header()->resizeSection(6, conf.value("/search/6", w - 5).toInt());
	QString key("/search/hide/%1");
	header()->setSectionHidden(0, conf.value(key.arg(0), false).toBool());
	header()->setSectionHidden(1, conf.value(key.arg(1), false).toBool());
	header()->setSectionHidden(2, conf.value(key.arg(2), false).toBool());
	header()->setSectionHidden(3, conf.value(key.arg(3), true ).toBool());
	header()->setSectionHidden(4, conf.value(key.arg(4), true ).toBool());
	header()->setSectionHidden(5, conf.value(key.arg(5), true ).toBool());
	header()->setSectionHidden(6, conf.value(key.arg(6), true ).toBool());
	header()->setObjectName("search");
	conf.setValue("/search/1", w);
	conf.setValue("/search/2", 120);
	conf.setValue("/search/3", w - 15);
	conf.setValue("/search/4", w - 5);
	conf.setValue("/search/5", w - 20);
	conf.setValue("/search/6", w - 5);

	setBackground(QPixmap(":/backgrounds/backgrounds/default.png"));

	connect(
		this, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)),
		SLOT(startDownload(QTreeWidgetItem*, int))
	);
}

void SearchList::startDownload(QTreeWidgetItem *it, int) {
	dynamic_cast<SearchListItem*>(it)->download();
}

void SearchList::contextMenuEvent(QContextMenuEvent *evt) {
	QTreeWidgetItem *it = itemAt(evt->pos());
	if (it) {
		if (!isItemSelected(it)) {
			if (evt->modifiers() == Qt::NoModifier) {
				QList<QTreeWidgetItem*> items(selectedItems());
				Q_FOREACH(QTreeWidgetItem *i, items) {
					setItemSelected(i, false);
				}
			}
			setItemSelected(it, true);
		}
//		SearchListItem *s = dynamic_cast<SearchListItem*>(it);
//		Q_ASSERT(s);
		QMenu menu(this);
		menu.addAction(
			QIcon(":/icons/getfile16.png"),
			"Download", this, SLOT(downloadSelected())
		);
		menu.addAction(
			QIcon(":/icons/getfile16,png"),
			"Download to...", this, SLOT(downloadSelectedTo())
		);
		menu.exec(evt->globalPos());
	}
}

void SearchList::downloadSelected() {
	QList<QTreeWidgetItem*> items(selectedItems());
	Q_FOREACH(QTreeWidgetItem *i, items) {
		if (!isItemHidden(i)) {
			dynamic_cast<SearchListItem*>(i)->download();
		}
	}
}

void SearchList::downloadSelectedTo() {
	QList<QTreeWidgetItem*> items(selectedItems());
	if (!items.size()) {
		return;
	}
	QString s = QFileDialog::getExistingDirectory(
		this, "Choose download destination",
		QString(), QFileDialog::ShowDirsOnly
	);
	if (s.size()) {
		Q_FOREACH(QTreeWidgetItem *i, items) {
			if (!isItemHidden(i)) {
				dynamic_cast<SearchListItem*>(i)->download(s);
			}
		}
	}
}

void SearchList::paintEvent(QPaintEvent *evt) {
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

void SearchList::drawRow(
	QPainter *p, const QStyleOptionViewItem &option, 
	const QModelIndex &index
) const {
	QTreeWidget::drawRow(p, option, index);

	QPixmap greenBar(":/icons/greenbar");
	QPixmap redBar(":/icons/redbar");
	if (greenBar.isNull() || redBar.isNull()) {
		return;
	}
	
	int position = columnViewportPosition(2);
	int y = option.rect.y() + (option.rect.height() - greenBar.height())/ 2;
	quint16 src = topLevelItem(index.row())->data(2, -1).toInt();
	quint16 fSrc = topLevelItem(index.row())->data(3, -1).toInt();
	int width = src * columnWidth(2) / 25;
	if (width > columnWidth(2) - 1) {
		width = columnWidth(2) - 1;
	}
	QPixmap toDraw(fSrc ? greenBar : redBar);
	p->drawPixmap(position, y, toDraw.scaled(width, toDraw.height()));

	QString text(QString("%1 (%2)").arg(src).arg(fSrc));
	QRect rect(QFontMetrics(p->font()).boundingRect(text));
	int textPos = columnViewportPosition(2)+(columnWidth(2)-rect.width())/2;
	if (textPos < columnViewportPosition(2)) {
		return;
	}
	y = option.rect.bottom() - (option.rect.height() - rect.height()) + 1;
	p->drawText(textPos, y, text);
}

///////////////////////////
// SearchListItem class
//////////////////////////

SearchListItem::SearchListItem(
	QTreeWidget *parent, Engine::SearchResultPtr data
) : QTreeWidgetItem(parent, Type), m_data(data) {
	data->onUpdated.connect(boost::bind(&SearchListItem::onUpdated, this));
	onUpdated();
	setTextAlignment(0, Qt::AlignVCenter);
	setTextAlignment(1, Qt::AlignRight  | Qt::AlignVCenter);
	setTextAlignment(2, Qt::AlignCenter | Qt::AlignVCenter);
	setTextAlignment(3, Qt::AlignRight  | Qt::AlignVCenter);
	setTextAlignment(4, Qt::AlignRight  | Qt::AlignVCenter);
	setTextAlignment(5, Qt::AlignCenter | Qt::AlignVCenter);
	setTextAlignment(6, Qt::AlignCenter | Qt::AlignVCenter);
}

void SearchListItem::onUpdated() {
	using namespace Engine;

	if (!m_customName.size()) {
		setText(0, QString::fromStdString(m_data->getName()));
	}
	setText(1, QString::number(m_data->getSize()));
//	setText(2, QString::number(m_data->getSources()));
	setText(3, secondsToString(m_data->getLength(), 3));
	setText(4, QString::number(m_data->getBitrate()) + " kbps");
	setText(5, QString::fromStdString(m_data->getCodec()));
	switch (getFileType(m_data->getName())) {
		case FT_AUDIO:   
			setIcon(0, QIcon(":/types/icons/soundfile16")); 
			setText(6, "Audio");
			break;
		case FT_VIDEO:   
			setIcon(0, QIcon(":/types/icons/movie16"));
			setText(6, "Video");
			break;
		case FT_IMAGE:   
			setIcon(0, QIcon(":/types/icons/image16"));
			setText(6, "Image");
			break;
		case FT_ARCHIVE: 
			setIcon(0, QIcon(":/types/icons/archive16"));
			setText(6, "Archive");
			break;
		case FT_CDDVD:   
			setIcon(0, QIcon(":/types/icons/cd-dvd16"));
			setText(6, "CD/DVD");
			break;
		case FT_DOC:     
			setIcon(0, QIcon(":/types/icons/scroll16"));
			setText(6, "Document");
			break;
		case FT_PROG:    
			setIcon(0, QIcon(":/types/icons/program16"));
			setText(6, "Application");
			break;
		case FT_UNKNOWN:
			setIcon(0, QIcon(":/types/icons/unknown"));
			setText(6, "Unknown");
			break;
	}
//	switch (getFileType(m_data->getName())) {
//	case FT_AUDIO:   setIcon(0, QIcon(":/types/icons/soundfile16")); break;
//	case FT_VIDEO:   setIcon(0, QIcon(":/types/icons/movie16"));     break;
//	case FT_IMAGE:   setIcon(0, QIcon(":/types/icons/image16"));     break;
//	case FT_ARCHIVE: setIcon(0, QIcon(":/types/icons/archive16"));   break;
//	case FT_CDDVD:   setIcon(0, QIcon(":/types/icons/cd-dvd16"));    break;
//	case FT_DOC:     setIcon(0, QIcon(":/types/icons/scroll16"));    break;
//	case FT_PROG:    setIcon(0, QIcon(":/types/icons/program16"));   break;
//	case FT_UNKNOWN: setIcon(0, QIcon(":/types/icons/unknown"));     break;
//	}
}

QVariant SearchListItem::data(int column, int role) const {
	if (column == 1 && role == Qt::DisplayRole) {
		QString d = QTreeWidgetItem::data(column, role).toString();
		return bytesToString(d.toULongLong());
	} else if (column == 1 && role == -1) {
		role = Qt::DisplayRole;
	} else if (column == 2 && role == -1) {
		return m_data->getSources();
	} else if (column == 3 && role == -1) {
		return m_data->getFullSources();
	}
	return QTreeWidgetItem::data(column, role);
}

bool SearchListItem::operator<(const QTreeWidgetItem &other) const {
	int col = treeWidget()->header()->sortIndicatorSection();
	if (col == 1 || col == 2) {
		QString d1 = data(col, -1).toString();
		QString d2 = other.data(col, -1).toString();
		if (d1 == d2) {
			return text(0).toLower() < other.text(0).toLower();
		}
		return d1.toULongLong() < d2.toULongLong();
	} else {
		QString d1 = text(col).toLower();
		QString d2 = other.text(col).toLower();
		if (d1 == d2 && col != 0) {
			return text(0).toLower() < other.text(0).toLower();
		}
		return d1 < d2;
	}
}

void SearchListItem::download() {
	if (m_customDest.size()) { 
		return download(m_customDest); 
	}

	Q_ASSERT(m_data);
	QFont f(font(0));
	f.setBold(true);
	setFont(0, f);
	setFont(1, f);
	setFont(2, f);
	m_data->download();
}

void SearchListItem::download(const QString &dest) {
	Q_ASSERT(m_data);
	QFont f(font(0));
	f.setBold(true);
	setFont(0, f);
	setFont(1, f);
	setFont(2, f);
	m_data->download(dest.toStdString());
}

QString SearchListItem::getName() const {
	if (m_customName.size()) {
		return m_customName;
	} else {
		return QString::fromStdString(m_data->getName());
	}
}

void SearchListItem::setName(const QString &newName) {
	m_customName = newName;
}

QString SearchListItem::getDest() const {
	if (m_customDest.size()) {
		return m_customDest;
	} else {
		return SettingsPage::instance().value("Incoming");
	}
}

void SearchListItem::setDest(const QString &newDest) {
	m_customDest = newDest;
}
