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

#include "htreewidget.h"
#include "main.h"
#include <QPainter>
#include <QScrollBar>
#include <QHeaderView>
#include <QSettings>

HTreeWidget::HTreeWidget(QWidget *parent) : QTreeWidget(parent) {}

void HTreeWidget::setBackground(const QPixmap &img) {
	m_background = img;
}

void HTreeWidget::paintEvent(QPaintEvent *evt) {
	QTreeWidget::paintEvent(evt);
	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	if (!m_background.isNull() && conf.value("EnableBack").toBool()) {
		QPainter p(viewport());
		QPoint pos;
		pos.setX(
			window()->width() - m_background.width() 
			- FRAMERBORDER - 1
		);
		pos.setY(
			window()->height() - m_background.height() 
			- FRAMEBBORDER - 1
		);
		pos = viewport()->mapFrom(window(), pos);
		p.drawPixmap(pos, m_background);
	}
}

void HTreeWidget::scrollContentsBy(int dx, int dy) {
	QTreeWidget::scrollContentsBy(dx, dy);
//	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
//	if (!m_background.isNull() && !conf.value("DisableBack").toBool()) {
		viewport()->update();
//	}
}
