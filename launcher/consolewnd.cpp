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

#include "consolewnd.h"
#include <QPainter>
#include <QPixmap>
#include <QTextCursor>

ConsoleWnd::ConsoleWnd(QWidget *parent) : QTextEdit(parent), m_lineCnt() {}

// this doesn't work on linux due to:
// "Widget painting can only begin as a result of a paintEvent"
void ConsoleWnd::paintEvent(QPaintEvent *event) {
	QTextEdit::paintEvent(event);
#ifdef _WIN32
	QPainter p(this);
	QPixmap img(":/background/background.png");
	p.drawPixmap(viewport()->width() - img.width(), 0, img);
#endif
}

void ConsoleWnd::append(const QString &text) {
	++m_lineCnt;
	QTextEdit::append(text);
	if (m_lineCnt > 350) {
		QTextCursor t(document());
		t.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor);
		t.removeSelectedText();
		--m_lineCnt;
	}
}
