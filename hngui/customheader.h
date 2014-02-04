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
 
#ifndef __CUSTOM_HEADER_H__
#define __CUSTOM_HEADER_H__

#include <QHeaderView>

class QTimer;

class CustomHeader : public QHeaderView {
	Q_OBJECT
public:
	CustomHeader(Qt::Orientation orientation, QWidget *parent = 0);
	void init();
	void disableColumnSelection() { m_columnSelection = false; }
	void enableColumnSelection()  { m_columnSelection = true;  }
Q_SIGNALS:
	void restoreDefaults();
protected:
	void paintEvent(QPaintEvent *evt);
	void paintSection(QPainter *p, const QRect &rect, int logicalIdx) const;
	void mouseMoveEvent(QMouseEvent*);
	void contextMenuEvent(QContextMenuEvent *e);
	void resizeEvent(QResizeEvent *e);
	QSize sizeHint() const;
public Q_SLOTS:
	void onSectionResized(int logicalIndex, int oldSize, int newSize);
private:
	bool m_restoring;
	bool m_columnSelection;
};

#endif
