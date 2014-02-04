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
 
#include "customheader.h"
#include "main.h"
#include <QPainter>
#include <QSettings>
#include <QMenu>
#include <QContextMenuEvent>
#include <QTreeWidget>

CustomHeader::CustomHeader(Qt::Orientation orientation, QWidget *parent)
: QHeaderView(orientation, parent), m_restoring(), m_columnSelection(true) {
	setMouseTracking(true);
	connect(
		this, SIGNAL(sectionResized(int, int, int)),
		this, SLOT(onSectionResized(int, int, int))
	);
}

void CustomHeader::mouseMoveEvent(QMouseEvent *evt) {
	QHeaderView::mouseMoveEvent(evt);
}

void CustomHeader::paintEvent(QPaintEvent *evt) {
	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	if (!conf.value("EnableBars").toBool()) {
		QHeaderView::paintEvent(evt);
		return;
	}

	QPainter p(this);
	p.setBrush(QColor(201, 238, 253, 183));
//	p.setPen(QColor(0, 0, 9, 183));
	p.setPen(Qt::black);
	p.drawRect(0, 0, width(), height() - 1);
	QHeaderView::paintEvent(evt);
}

void CustomHeader::paintSection(
	QPainter *p, const QRect &rect, int logicalIndex
) const {
	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	if (!conf.value("EnableBars").toBool()) {
		QHeaderView::paintSection(p, rect, logicalIndex);
		return;
	}
	QPixmap img(imgDir() + "listheader.png");
	if (img.isNull()) {
		QHeaderView::paintSection(p, rect, logicalIndex);
		return;
	}

	p->drawPixmap(
		rect.x(), rect.y(), img.scaled(rect.width(), img.height(), 
		Qt::IgnoreAspectRatio)
	);

/*
	p->setBrush(QColor(201, 238, 253, 183));
	p->setPen(QColor(201, 238, 253, 183));

	p->drawRect(rect.x(), rect.y(), rect.width(), rect.height() - 1);

	p->setPen(QColor(0, 0, 0, 183));
	p->drawLine(rect.bottomLeft(), rect.bottomRight());
	p->drawLine(rect.topLeft(), rect.topRight());
	p->setPen(QColor(0, 0, 0, 50));
*/
	QString text(
		model()->headerData(
			logicalIndex, orientation(), Qt::DisplayRole
		).toString()
	);
	QRect textRect = QFontMetrics(p->font()).boundingRect(text);
	int xPos = rect.x() + (rect.width() / 2 - textRect.width() / 2);
	int yPos = rect.y() + (rect.height() / 2 + textRect.height() / 2) - 2;
	p->setPen(Qt::black);
	p->drawText(xPos, yPos, text);

	QPixmap sep(imgDir() + "listseparator.png");
	if (sep.isNull()) {
		return;
	}
//	if (visualIndexAt(logicalIndex) > 0) {
//		p->drawPixmap(rect.x(), 0, sep);
//	}
	if (visualIndexAt(logicalIndex) < count() - hiddenSectionCount()) {
		p->drawPixmap(rect.x() + rect.width() - 1, 0, sep);
	}
}

QSize CustomHeader::sizeHint() const {
	QSize ret = QHeaderView::sizeHint();
	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	if (conf.value("EnableBars").toBool()) {
		ret.setHeight(18);
	}
	return ret;
}

void CustomHeader::contextMenuEvent(QContextMenuEvent *e) {
	if (!m_columnSelection) {
		return;
	}

	QMenu menu(this);
	for (int i = 0; i < count(); ++i) {
		QString text(
			model()->headerData(
				i, orientation(), Qt::DisplayRole
			).toString()
		);
		QAction *menuAc = new QAction(&menu);
		menuAc->setText(text);
		menuAc->setData(i);
		if (!isSectionHidden(i)) {
			menuAc->setIcon(QIcon(":/transfer/icons/clear16"));
		}
		menu.addAction(menuAc);
	}
	menu.addSeparator();
	QAction *dflts = menu.addAction("Restore defaults");
	QAction *ret = menu.exec(e->globalPos());
	if (ret && ret != dflts) {
		QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
		int col = ret->data().toInt();
		m_restoring = true;
		if (!isSectionHidden(col)) {
			resizeSection(0, sectionSize(0) + sectionSize(col));
			setSectionHidden(col, true);
		} else {
//			setStretchLastSection(false);
			setSectionHidden(col, false);
			QString key("/" + objectName() + "/%1");
			int size = conf.value(key.arg(col)).toInt();
			resizeSection(0, sectionSize(0) - size);
			resizeSection(col, size);
		}
		if (col == 0) {
			resizeSection(0, 100);
		}
		m_restoring = false;
		conf.setValue(
			QString("/" + objectName() + "/hide/%1").arg(col),
			isSectionHidden(col)
		);
	} else if (ret == dflts) {
		QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
		QString key("/" + objectName() + "/%1");
		QString keyH("/" + objectName() + "/hide/%1");
		for (int i = 0; i < count(); ++i) {
			conf.remove(key.arg(i));
			conf.remove(keyH.arg(i));
		}
		setStretchLastSection(false);
		m_restoring = true;
		restoreDefaults();
		m_restoring = false;
	}
	if (!stretchLastSection()) {
		resizeEvent(0);
	}
}

void CustomHeader::resizeEvent(QResizeEvent *e) {
	int left = viewport()->width();
	for (int i = 1; i < count(); ++i) {
		left -= sectionSize(i);
	}
	resizeSection(0, left > 0 ? left : 0);
	setStretchLastSection(true);
	if (e) {
		QHeaderView::resizeEvent(e);
	}
}

void CustomHeader::onSectionResized(int logicalIndex, int oldSize, int newSize){
	if (m_restoring) { // ignore signals while restoring default settings
		return;
	}

	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	conf.setValue(
		QString("/" + objectName() + "/%1").arg(logicalIndex), newSize
	);
}
