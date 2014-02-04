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

#include "searchlist.h"
#include "launcher.h"
#include "console.h"
#include <hncgcomm/cgcomm.h>
#include <QHeaderView>
//#include <QPainter>
//#include <QPixmap>

// bring in some globals
extern Launcher *g_launcher;
extern QString bytesToString(uint64_t bytes);

///////////////////////
// SearchList class
///////////////////////

SearchList::SearchList(QWidget *parent) : QTreeWidget(parent) {}

void SearchList::init() {
	QStringList searchHeaders;
	searchHeaders << "File Name" << "File Size" << "Availability";
	setHeaderLabels(searchHeaders);
	header()->setStretchLastSection(false);
//	m_img = new QPixmap(":/background/transparentlogo.png");
}


void SearchList::resizeEvent(QResizeEvent *event) {
	int nameCol = 0;
	if (headerItem()->text(1) == "File Name") {
		nameCol = 1;
		header()->resizeSection(0, 100);
		header()->resizeSection(2, 100);
	} else if (headerItem()->text(2) == "File Name") {
		nameCol = 2;
		header()->resizeSection(0, 100);
		header()->resizeSection(1, 100);
	} else {
		header()->resizeSection(1, 100);
		header()->resizeSection(2, 100);
	}
	header()->resizeSection(nameCol, viewport()->width() - 200);
}

//void SearchList::paintEvent(QPaintEvent *event) {
//	qDebug("Received paint event");
//	QTreeWidget::paintEvent(event);
//	QPainter p(this);
//	p.drawPixmap(
//		viewport()->width() - m_img->width(),
//		viewport()->height() - m_img->height(), *m_img
//	);
//}

//void SearchList::scrollContentsBy(int dx, int dy) {
//	qDebug(QString("%1 %2").arg(dx).arg(dy).toAscii());
//	QTreeWidget::scrollContentsBy(dx, dy);
//	repaint(0, 0, viewport()->width(), viewport()->height());
//		viewport()->width() - m_img->width() + dx,
//		viewport()->height() - m_img->height() + dy,
//		m_img->width() + dx,
//		m_img->height() + dy
//	);
//}


///////////////////////////
// SearchListItem class
//////////////////////////

SearchListItem::SearchListItem(
	QTreeWidget *parent, Engine::SearchResultPtr data
) : QTreeWidgetItem(parent, Type), m_data(data) {
	setText(0, QString::fromStdString(data->getName()));
	setText(1, QString::number(data->getSize()));
	setText(2, QString::number(data->getSources()));
}

QVariant SearchListItem::data(int column, int role) const {
	if (column == 1 && role == Qt::DisplayRole) {
		QString d = QTreeWidgetItem::data(column, role).toString();
		return bytesToString(d.toULongLong());
	} else if ((column == 1 || column == 2) && role == -1) {
		role = Qt::DisplayRole;
	}
	return QTreeWidgetItem::data(column, role);
}

bool SearchListItem::operator<(const QTreeWidgetItem &other) const {
	QTreeWidget *parent = g_launcher->m_console->searchList;
	int col = parent->header()->sortIndicatorSection();
	if (col == 1 || col == 2) {
		QString d1 = data(col, -1).toString();
		QString d2 = other.data(col, -1).toString();
		return d1.toULongLong() < d2.toULongLong();
	} else {
		return text(col).toLower() < other.text(col).toLower();
	}
}

void SearchListItem::download() {
	Q_ASSERT(m_data);
	QFont f(font(0));
	f.setBold(true);
	setFont(0, f);
	setFont(1, f);
	setFont(2, f);
	m_data->download();
}
