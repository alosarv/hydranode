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

#ifndef __LIBRARYTABS_H__
#define __LIBRARYTABS_H__

#include <QWidget>
#include <QTabWidget>
#include <QPixmap>

namespace Ui {
	class LibraryContent;
}
class QModelIndex;
class QDirModel;

class LibraryPage : public QWidget {
	Q_OBJECT
public:
	LibraryPage(QWidget *parent = 0);
private Q_SLOTS:
	void clearFilter();
	void applyFilter();
	void toggleFilter();
	void showDirMenu();
	void showTypeMenu();
	void addShared();
	void remShared();

	void updateButtons();
	bool eventFilter(QObject *obj, QEvent *evt);
protected:
	void keyPressEvent(QKeyEvent *evt);
private:
	Ui::LibraryContent *m_ui;
	QPixmap m_actionBack, m_actionBackOrig;
};

#endif
