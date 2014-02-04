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

#include <hncore/metadb.h>
#include <hncore/metadata.h>
#include <QApplication>
#include <QFileDialog>
#include <QProgressDialog>
#include <QHeaderView>
#include "fileviewer.h"

int main(int argc, char *argv[]) {
	QApplication app(argc, argv);
	QMainWindow *wnd = new QMainWindow;
	Ui::MainWindow *ui = new Ui::MainWindow;
	ui->setupUi(wnd);
	QStringList headerLabels;
	headerLabels << "File Name" << "File Size" << "Modification Date";
	ui->outWnd->setHeaderLabels(headerLabels);
	ui->outWnd->header()->setStretchLastSection(false);
	ui->outWnd->header()->resizeSection(0, 400);
#ifndef WIN32
	wnd->show();
#endif

	QString s = QFileDialog::getOpenFileName(
		wnd, "Choose metadb.dat file to load",
		"", "metadb.dat"
	);
	if (!s.size()) {
		return 0;
	}

	std::ifstream ifs(s.toStdString().c_str(), std::ios::binary);
	MetaDb::instance().load(ifs);
	MetaDb::CIter it = MetaDb::instance().begin();
	while (it != MetaDb::instance().end()) {
		QTreeWidgetItem *item = new QTreeWidgetItem(ui->outWnd);
		item->setText(0, QString::fromStdString((*it)->getName()));
		item->setText(1, QString::number((*it)->getSize()));
		item->setText(2, QString::number((*it)->getModDate()));
		++it;
	}

#ifdef WIN32
	wnd->show();
#endif

	return app.exec();
}
