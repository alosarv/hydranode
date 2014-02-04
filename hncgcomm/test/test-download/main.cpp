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

#include <qapplication.h>
#include <qlabel.h>
#include <test_download.h>
#include <../ecomm.h>
#include <hncgcomm/cgcomm.h>
#include <qsocket.h>
#include <boost/bind.hpp>
#include <qtimer.h>
#include "main.h"
#include <qlistbox.h>
#include <qlistview.h>

DownloadWnd *s_dnWnd;
QListBox *s_logWnd;
void logMsg(const QString &msg) {
	s_logWnd->insertItem(msg);
	s_logWnd->setCurrentItem(s_logWnd->item(s_logWnd->numRows() - 1));
	s_dnWnd->m_status->setText(msg);
}

TestDownload::TestDownload() : m_eComm(),
m_dList(m_eComm.getMain(), boost::bind(&TestDownload::displayList, this, _1)) {

	connect(&m_eComm, SIGNAL(connectionEstablished()), SLOT(onConnected()));
	connect(&m_eComm, SIGNAL(connectionLost()), SLOT(onDisconnected()));
}

void TestDownload::displayList(const std::vector<Engine::DownloadInfoPtr> &list) {
	logMsg(QString("Received %1 downloads from Engine.").arg(list.size()));
	std::vector<Engine::DownloadInfoPtr>::const_iterator it(list.begin());
	while (it != list.end()) {
		logMsg(
			QString("Received download: name=%1 size=%2")
			.arg((*it)->getName().c_str())
			.arg((*it)->getSize())
		);

		QListViewItem *item = new QListViewItem(
			s_dnWnd->m_list, QString((*it)->getName().c_str()),
			bytesToString((*it)->getSize()),
			bytesToString((*it)->getCompleted()),
			"", "", QString("%1").arg((*it)->getSourceCnt())
		);
		s_dnWnd->m_list->insertItem(item);
		++it;
	}
}

void TestDownload::onConnected() {
	m_dList.getList();
	logMsg("Requesting download list from Engine...");
}

void TestDownload::onDisconnected() {
}

int main( int argc, char ** argv ) {
	QApplication a( argc, argv );

	s_dnWnd = new DownloadWnd;
	s_logWnd = new QListBox;
	s_logWnd->resize(QSize(400, 200));
	s_logWnd->move(QPoint(20, 20));
	s_dnWnd->move(QPoint(430, 20));

	TestDownload tester;

	s_dnWnd->show();
	s_logWnd->show();
	a.connect( &a, SIGNAL( lastWindowClosed() ), &a, SLOT( quit() ) );

	return a.exec();
}
