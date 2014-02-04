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

#ifndef __SEARCHTABS_H__
#define __SEARCHTABS_H__

#include <hncgcomm/fwd.h>
#include <QWidget>
#include <QLineEdit>
#include <vector>
#include <QPixmap>
#include <boost/signals/connection.hpp>

namespace Ui {
	class SearchContent;
//	class SearchDetails;
}
class SearchListItem;
class QTreeWidgetItem;

class SearchPage : public QWidget {
	Q_OBJECT
public:
	SearchPage(QWidget *parent = 0);
private Q_SLOTS:
	// actionbar
	void startSearch();
	void clearResults();
	void showTypeMenu();
	void showNetMenu();
	void setTypeAudio();
	void setTypeVideo();
	void setTypeCddvd();
	void setTypeArchive();
	void setTypePicture();
	void setTypeDocument();
	void setTypeApplication();
	void setTypeAny();
	void setNetDonkey();
	void setNetAny();
	// details-bar
	void changeFileDest();
	void downloadClicked();
	void cleanName();
	void updateDetails();
	void updateDetails(QTreeWidgetItem*, QTreeWidgetItem*);
	void clearDetails();
protected:
	void showEvent(QShowEvent *evt);
	bool eventFilter(QObject *obj, QEvent *evt);
private:
	void results(const std::vector<Engine::SearchResultPtr> &res);
	Ui::SearchContent *m_ui;
//	Ui::SearchDetails *m_detailsBar;
	Engine::Search *m_currentSearch;
//	QRect m_closeButton;
//	QPoint m_pressPos;
	QPixmap m_actionBack, m_actionBackOrig;
//	QPixmap m_detailsBack, m_detailsBackOrig;
	SearchListItem *m_currentActive;
	boost::signals::scoped_connection m_currentConnection;
};

class SearchInput : public QLineEdit {
public:
	SearchInput(QWidget *parent = 0);
protected:
	void paintEvent(QPaintEvent *evt);
};

#endif
