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
 
#ifndef __TRANSFERTABS_H__
#define __TRANSFERTABS_H__

#include "htreewidget.h"
#include <QWidget>
#include <QPixmap>
#include <QTreeWidgetItem>
#include <boost/signals/connection.hpp>
#include <hncgcomm/fwd.h>

namespace Ui {
	class TransferContent;
//	class DetailsBar;
	class CatDialog;
	class CommentFrame;
}

class QTreeWidgetItem;
class DownloadListItem;
class QTimer;

class CommentList : public HTreeWidget {
public:
	CommentList(QWidget *parent = 0);
protected:
	void paintEvent(QPaintEvent *evt);
private:
	QPixmap m_commentB, m_commentBOrig;
};

class CommentListItem : public QTreeWidgetItem {
public:
	CommentListItem(QTreeWidget *parent);
	CommentListItem(QTreeWidgetItem *parent);
	bool operator<(const QTreeWidgetItem &other) const;
};

class TransferPage : public QWidget {
	Q_OBJECT
public:
	TransferPage(QWidget *parent = 0);
private Q_SLOTS:
	void clearFilter();
	void applyFilter();
	void toggleFilter();

	void updateButtons();
	void showStatusMenu();
	void showAddMenu();
	void showTypeMenu();

	void updateDetails(QTreeWidgetItem *cur, QTreeWidgetItem *prev);
	void updateDetails(bool force = false);
	void updateComments();
	void updateComments(Engine::DownloadInfoPtr ptr);
	void getMoreComments();
	void clearComments();
	void newCategory();
	void doNewCat();
	void renameFromComment(QTreeWidgetItem *it, int col);

	void cleanName();
	void changeFileDest();
	void renameClicked();
	void clearDetails();

	void showComments();
	void hideComments();
	void showComments(bool show);
	void hideComments(bool hide);
protected:
	void keyPressEvent(QKeyEvent *evt);
	bool eventFilter(QObject *obj, QEvent *evt);
	void paintEvent(QPaintEvent *evt);
private:
	Ui::TransferContent *m_ui;
//	Ui::DetailsBar      *m_detailsBar;
	Ui::CommentFrame    *m_commentFrame;
	Ui::CatDialog       *m_catDlg;
	QPixmap m_background;
//	QRect   m_closeButton;
//	QPoint  m_pressPos;
	QPixmap m_actionBack, m_actionBackOrig;
//	QPixmap m_detailsBack, m_detailsBackOrig;
	QPixmap m_commentH, m_commentHOrig;
	DownloadListItem *m_currentActive;
	boost::signals::scoped_connection m_currentConnection;
	boost::signals::scoped_connection m_currentDestroyConnection;
	QWidget *m_catDlgWidget;
	QTimer *m_commentTimer;
};

#endif
