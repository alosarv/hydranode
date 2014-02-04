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

#include "test-search.h"
#include "../ecomm.h"
#include "main.h"
#include <hncgcomm/cgcomm.h>
#include <boost/bind.hpp>
#include <QApplication>
#include <QLabel>
#include <QMessageBox>
#include <QTimer>
#include <QPushButton>
#include <QComboBox>
#include <QListView>
#include <Qt/QTcpSocket>
#include <Qt/QLineEdit>

SearchWnd *s_srcWnd;
EngineComm *s_eComm;

void logMsg(const QString &msg) {
	s_srcWnd->m_status->setText(msg);
}


// SearchListItem class
// --------------------

class SearchListItem : public QListViewItem {
public:
	SearchListItem(Engine::SearchResultPtr res);
	virtual int compare(QListViewItem *i, int cool, bool ascending) const;
	void download();
private:
	uint64_t m_realBytes;
	Engine::SearchResultPtr m_item;
};

SearchListItem::SearchListItem(Engine::SearchResultPtr res)
: QListViewItem(
	s_srcWnd->m_searchResults,
	QString(res->getName().c_str()), bytesToString(res->getSize()),
	QString("%1").arg(res->getSources())
), m_realBytes(res->getSize()), m_item(res) {

}

int SearchListItem::compare(QListViewItem *i, int col, bool ascending) const {
	if (col == 0) {
		return key(0, ascending).compare(i->key(0, ascending));
	} else if (col == 2) {
		if (text(col).toLongLong() < i->text(col).toLongLong()) {
			return -1;
		} else if (text(col).toLongLong() > i->text(col).toLongLong()) {
			return 1;
		} else {
			return 0;
		}
	} else if (col == 1) {
		if (m_realBytes <dynamic_cast<SearchListItem*>(i)->m_realBytes){
			return -1;
		} else if (
			m_realBytes >
			dynamic_cast<SearchListItem*>(i)->m_realBytes
		) {
			return 1;
		} else {
			return 0;
		}
	}
}
void SearchListItem::download() {
	m_item->download();
	logMsg(QString("Downloading <b>%1</b>").arg(m_item->getName().c_str()));
}

// TestSearch class
// ----------------
TestSearch::TestSearch() : m_search() {
	connect(s_eComm, SIGNAL(connectionEstablished()), SLOT(enableGui()));
	connect(s_eComm, SIGNAL(connectionLost()), SLOT(disableGui()));
	connect(s_srcWnd->m_searchButton, SIGNAL(clicked()), SLOT(newSearch()));
	connect(
		s_srcWnd->m_searchResults,
		SIGNAL(doubleClicked(QListViewItem*, const QPoint&, int)),
		SLOT(downloadResult(QListViewItem*, const QPoint&, int))
	);
	setGuiEnabled(false);
}

void TestSearch::newSearch() {
	std::string keywords = s_srcWnd->m_searchKeywords->text();
	if (m_search) {
		delete m_search;
		s_srcWnd->m_searchResults->clear();
	}
	Engine::FileType ft = Engine::FT_UNKNOWN;
	if (s_srcWnd->m_searchType->currentText() == "Video") {
		ft = Engine::FT_VIDEO;
	} else if (s_srcWnd->m_searchType->currentText() == "Audio") {
		ft = Engine::FT_AUDIO;
	} else if (s_srcWnd->m_searchType->currentText() == "Archive") {
		ft = Engine::FT_ARCHIVE;
	} else if (s_srcWnd->m_searchType->currentText() == "Document") {
		ft = Engine::FT_DOCUMENT;
	} else if (s_srcWnd->m_searchType->currentText() == "CD/DVD") {
		ft = Engine::FT_CDIMAGE;
	} else if (s_srcWnd->m_searchType->currentText() == "Application") {
		ft = Engine::FT_PROGRAM;
	}
	m_search = new Engine::Search(
		s_eComm->getMain(),
		boost::bind(&TestSearch::newResults, this, _1), keywords, ft
	);
	m_search->run();
	logMsg(
		QString("Searching for <b>%1</b>").arg(
			s_srcWnd->m_searchKeywords->text()
		)
	);
}

void TestSearch::enableGui() {
	setGuiEnabled(true);
}
void TestSearch::disableGui() {
	setGuiEnabled(false);
}

void TestSearch::setGuiEnabled(bool state) {
	s_srcWnd->m_searchButton->setEnabled(state);
	s_srcWnd->m_searchKeywords->setEnabled(state);
	s_srcWnd->m_searchResults->setEnabled(state);
	s_srcWnd->m_searchType->setEnabled(state);
	if (state) {
		s_srcWnd->m_searchKeywords->setFocus();
	}
}

void TestSearch::newResults(
	const std::vector<Engine::SearchResultPtr> &results
) {
	logMsg(QString("Received %1 results from Engine.").arg(results.size()));

	std::vector<Engine::SearchResultPtr>::const_iterator it=results.begin();
	for (; it != results.end(); ++it) {
		new SearchListItem(*it);
	};
}

void TestSearch::downloadResult(QListViewItem *res, const QPoint&, int) {
	dynamic_cast<SearchListItem*>(res)->download();
}

int main( int argc, char ** argv ) {
	QApplication a( argc, argv );

	s_srcWnd = new SearchWnd;
	s_eComm = new EngineComm;
	TestSearch src;
	s_srcWnd->show();

	s_srcWnd->m_searchResults->setSorting(2, false);
	s_srcWnd->m_searchResults->setColumnWidthMode(0, QListView::Manual);
	s_srcWnd->m_searchResults->setColumnWidthMode(1, QListView::Manual);
	s_srcWnd->m_searchResults->setColumnWidthMode(2, QListView::Manual);
	s_srcWnd->m_searchResults->setColumnWidth(0, 425);
	s_srcWnd->m_searchResults->setColumnWidth(1, 80);
	s_srcWnd->m_searchResults->setColumnWidth(2, 50);
	s_srcWnd->m_searchResults->setColumnAlignment(2, Qt::AlignRight);

	a.connect( &a, SIGNAL( lastWindowClosed() ), &a, SLOT( quit() ) );
	return a.exec();
}
