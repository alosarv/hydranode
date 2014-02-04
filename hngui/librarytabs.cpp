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

#include "librarytabs.h"
#include "librarycontent.h"
#include "settingspage.h"
#include "main.h"
#include <QKeyEvent>
#include <QPainter>
#include <QSettings>
#include <QHeaderView>
#include <QMenu>
#include <QFileDialog>

LibraryPage::LibraryPage(QWidget *parent) : QWidget(parent) {
	m_ui = new Ui::LibraryContent;
	m_ui->setupUi(this);
	m_ui->sharedList->init();

	connect(m_ui->filterText, SIGNAL(returnPressed()), SLOT(applyFilter()));
	connect(m_ui->filterButton, SIGNAL(toggled(bool)), SLOT(toggleFilter()));
	connect(m_ui->dirButton, SIGNAL(clicked()), SLOT(showDirMenu()));
	connect(m_ui->typeSelect, SIGNAL(clicked()), SLOT(showTypeMenu()));
	connect(m_ui->addButton, SIGNAL(clicked()), SLOT(addShared()));
	connect(m_ui->removeButton, SIGNAL(clicked()), SLOT(remShared()));

	QPalette palette = m_ui->filterText->palette();
	palette.setColor(QPalette::Base, QColor(239, 239, 239, 80));
	m_ui->filterText->setPalette(palette);

	QRect rect =QFontMetrics(m_ui->typeSelect->font()).boundingRect("Type");
	m_ui->typeSelect->setMinimumSize(rect.width() + 12, 0);

	m_ui->actionBar->installEventFilter(this);
	updateButtons();

	m_actionBack = QPixmap(imgDir() + "actionback.png");
	m_actionBackOrig = m_actionBack;
}

void LibraryPage::showTypeMenu() {
	QMenu menu(m_ui->typeSelect);
	menu.addAction(QIcon(":/types/icons/soundfile16"), "Audio");
	menu.addAction(QIcon(":/types/icons/movie16"), "Video");
	menu.addAction(QIcon(":/types/icons/cd-dvd16"), "CD/DVD");
	menu.addAction(QIcon(":/types/icons/archive16"), "Archive");
	menu.addAction(QIcon(":/types/icons/image16"), "Picture");
	menu.addAction(QIcon(":/types/icons/scroll16"), "Document");
	menu.addAction(QIcon(":/types/icons/program16"), "Application");
	menu.addAction("Any type");

	QPoint pos(0, m_ui->typeSelect->height());

	m_ui->typeSelect->setCheckable(true);
	m_ui->typeSelect->setChecked(true);
	QAction *ret = menu.exec(m_ui->typeSelect->mapToGlobal(pos));
	m_ui->typeSelect->setChecked(false);
	m_ui->typeSelect->setCheckable(false);

	if (ret && ret->text() == "Any type") {
		m_ui->sharedList->filterByText("");
		m_ui->typeSelect->setIcon(ret->icon());
		m_ui->filterText->clear();
		m_ui->dirButton->setText("Filter");
		m_ui->filterButton->setChecked(false);
	} else if (ret) {
		m_ui->sharedList->filterByType(ret->text());
		m_ui->typeSelect->setIcon(ret->icon());
		m_ui->filterText->clear();
		m_ui->dirButton->setText("Filter");
		m_ui->filterButton->setChecked(false);
	}
}

void LibraryPage::showDirMenu() {
	QMenu menu(this);
	m_ui->dirButton->setCheckable(true);
	m_ui->dirButton->setChecked(true);

	int fCol = m_ui->sharedList->filterColumn();
	QString fText = m_ui->sharedList->filterText();
	int count = SettingsPage::instance().value("SharedDirs/Count").toInt();

	for (int i = 0; i < count; ++i) {
		QString dirName = SettingsPage::instance().value(
			QString("SharedDirs/Dir_%1").arg(i)
		);
#ifdef Q_OS_WIN32
		dirName = dirName.replace("/", "\\");
#endif
		QAction *ac = menu.addAction(dirName);
		if (fCol == 4 && fText == dirName) {
			ac->setIcon(QIcon(":/transfer/icons/clear16"));
		}
	}

	if (fCol == -1) {
		menu.addAction(QIcon(":/transfer/icons/clear16"), "All Files");
	} else {
		menu.addAction("All Files");
	}

	QPoint pos(0, m_ui->dirButton->height());
	pos = m_ui->dirButton->mapToGlobal(pos);

	QAction *ret = menu.exec(pos);

	if (ret && ret->text() == "All Files") {
		m_ui->sharedList->filterByText("");
		m_ui->dirButton->setText("Filter");
		m_ui->typeSelect->setText("Type");
		m_ui->typeSelect->setIcon(QIcon());
	} else if (ret) {
		QString dir = ret->text();
//#ifdef Q_OS_WIN32
//		dir = dir.replace("\\", "/");
//#endif
		m_ui->sharedList->filterByDir(dir);
		m_ui->dirButton->setText(dir.left(20));
		m_ui->filterButton->setChecked(false);
		m_ui->typeSelect->setText("Type");
		m_ui->typeSelect->setIcon(QIcon());
	}
	m_ui->dirButton->setChecked(false);
	m_ui->dirButton->setCheckable(false);
}

void LibraryPage::addShared() {
	QString dirName = QFileDialog::getExistingDirectory(
		window(), "Choose directory to be shared",
		"", QFileDialog::ShowDirsOnly
	);
	if (dirName.size()) {
//#ifdef Q_OS_WIN32
//		dirName = dirName.replace("/", "\\");
//#endif
		LibraryList::getList()->addShared(
			dirName.toStdString()
		);
	}
}

void LibraryPage::remShared() {
	int count = SettingsPage::instance().value("SharedDirs/Count").toInt();
	if (!count) {
		return;
	}

	QMenu menu(this);
	m_ui->removeButton->setCheckable(true);
	m_ui->removeButton->setChecked(true);

	for (int i = 0; i < count; ++i) {
		QString dirName = SettingsPage::instance().value(
			QString("SharedDirs/Dir_%1").arg(i)
		);
//#ifdef Q_OS_WIN32
//		dirName = dirName.replace("/", "\\");
//#endif
		menu.addAction(dirName);
	}
	QPoint pos(0, m_ui->removeButton->height());
	pos = m_ui->removeButton->mapToGlobal(pos);

	QAction *ret = menu.exec(pos);
	if (ret) {
		LibraryList::getList()->remShared(ret->text().toStdString());
	}
	m_ui->removeButton->setChecked(false);
	m_ui->removeButton->setCheckable(false);
}

void LibraryPage::clearFilter() {
	m_ui->filterText->setText("");
	m_ui->sharedList->filterByText("");
	m_ui->filterButton->setChecked(false);
	m_ui->typeSelect->setText("Type");
	m_ui->typeSelect->setIcon(QIcon());
}

void LibraryPage::toggleFilter() {
	if (m_ui->filterButton->isChecked()) {
		applyFilter();
	} else {
		clearFilter();
	}
}

void LibraryPage::applyFilter() {
	m_ui->sharedList->filterByText(m_ui->filterText->text());
	m_ui->filterButton->setChecked(m_ui->filterText->text().size());
	m_ui->dirButton->setText("Filter");
	m_ui->typeSelect->setText("Type");
	m_ui->typeSelect->setIcon(QIcon());
}

void LibraryPage::updateButtons() {
//	bool setting = m_ui->sharedList->selectedItems().count();
//	m_ui->prioritySelect->setEnabled(setting);
}

void LibraryPage::keyPressEvent(QKeyEvent *evt) {
	if (evt->key() == Qt::Key_Escape) {
		clearFilter();
	} else {
		evt->ignore();
	}
}

bool LibraryPage::eventFilter(QObject *obj, QEvent *evt) {
	if (obj != m_ui->actionBar || evt->type() != QEvent::Paint) {
		return false;
	}
	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	if (!conf.value("EnableBars").toBool() || m_actionBack.isNull()) {
		return false;
	}
	QPainter p(m_ui->actionBar);
	int w = m_ui->actionBar->width();
	int h = m_ui->actionBar->height();
	if (m_actionBack.width() != w || m_actionBack.height() != h) {
		m_actionBack = m_actionBackOrig.scaled(
			w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation
		);
	}
	p.drawPixmap(0, 0, m_actionBack);
	return false;
}
