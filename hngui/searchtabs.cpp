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

#include "searchtabs.h"
#include "searchlist.h"
#include "searchcontent.h"
//#include "searchdetailsdock.h"
#include "filetypes.h"
#include "main.h"
#include "ecomm.h"
#include <QMouseEvent>
#include <QMessageBox>
#include <hncgcomm/cgcomm.h>
#include <boost/bind.hpp>
#include <QHeaderView>
#include <QPushButton>
#include <QPainter>
#include <QSettings>
#include <QMenu>
#include <QFileDialog>

// SearchPage class
//////////////////////////
SearchPage::SearchPage(QWidget *parent)
: QWidget(parent), m_currentSearch() {
	m_ui = new Ui::SearchContent;
	m_ui->setupUi(this);

//	m_detailsBar = new Ui::SearchDetails;
//	m_detailsBar->setupUi(m_ui->detailsBar);
	m_ui->detailsBar->hide();

	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
//	m_ui->detailsBar->setHidden(!conf.value("SearchDetails").toBool());

	if (
		!MainWindow::instance().getEngine() ||
		!MainWindow::instance().getEngine()->getMain()
	) {
		setEnabled(false);
	}
	MainWindow *m = &MainWindow::instance();
	connect(m, SIGNAL(engineConnection(bool)), SLOT(setEnabled(bool)));

	// action-bar related
	connect(m_ui->searchButton, SIGNAL(clicked()), SLOT(startSearch()));
	connect(m_ui->clearButton, SIGNAL(clicked()), SLOT(clearResults()));
	connect(
		m_ui->searchText, SIGNAL(returnPressed()), 
		m_ui->searchButton, SLOT(click())
	);
	connect(m_ui->typeSelect, SIGNAL(clicked()), SLOT(showTypeMenu()));
	connect(m_ui->netSelect, SIGNAL(clicked()), SLOT(showNetMenu()));

	// details-box related
	connect(
		m_ui->results,
		SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)),
		SLOT(updateDetails(QTreeWidgetItem*, QTreeWidgetItem*))
	);
//	connect(
//		m_detailsBar->destButton, SIGNAL(clicked()), 
//		SLOT(changeFileDest())
//	);
//	connect(
//		m_detailsBar->downloadButton, SIGNAL(clicked()), 
//		SLOT(downloadClicked())
//	);
//	connect(m_detailsBar->cleanButton, SIGNAL(clicked()),SLOT(cleanName()));

	m_ui->minSize->installEventFilter(this);
	m_ui->maxSize->installEventFilter(this);
//	m_ui->detailsBar->installEventFilter(this);
	m_ui->results->installEventFilter(this);
	m_ui->results->viewport()->installEventFilter(this);
	window()->installEventFilter(this);

	QPalette palette = m_ui->minSize->palette();
	palette.setColor(QPalette::Base, QColor(239, 239, 239, 80));
	m_ui->minSize->setPalette(palette);
	m_ui->maxSize->setPalette(palette);

	QRect rect;
	rect = QFontMetrics(m_ui->minSize->font()).boundingRect("min size");
	m_ui->minSize->setMinimumSize(rect.width() + 13, 18);
	m_ui->maxSize->setMinimumSize(rect.width() + 13, 18);
	rect = QFontMetrics(m_ui->typeSelect->font()).boundingRect("Type");
	m_ui->typeSelect->setMinimumSize(rect.width() + 12, 0);

	m_ui->actionBar->installEventFilter(this);

	m_actionBack = QPixmap(imgDir() + "actionback.png");
//	m_detailsBack = QPixmap(imgDir() + "detailsback.png");

	m_actionBackOrig = m_actionBack;
//	m_detailsBackOrig = m_detailsBack;

//	clearDetails();

	m_ui->netSelect->hide(); // not implemented yet
}

void SearchPage::showTypeMenu() {
	QMenu menu(m_ui->typeSelect);
	m_ui->typeSelect->setCheckable(true);
	m_ui->typeSelect->setChecked(true);

	menu.addAction(
		QIcon(":/types/icons/soundfile16"), "Audio", 
		this, SLOT(setTypeAudio())
	);
	menu.addAction(
		QIcon(":/types/icons/movie16"), "Video", 
		this, SLOT(setTypeVideo())
	);
	menu.addAction(
		QIcon(":/types/icons/cd-dvd16"), "CD/DVD", 
		this, SLOT(setTypeCddvd())
	);
	menu.addAction(
		QIcon(":/types/icons/archive16"), "Archive", 
		this, SLOT(setTypeArchive())
	);
	menu.addAction(
		QIcon(":/types/icons/image16"), "Picture", 
		this, SLOT(setTypePicture())
	);
	menu.addAction(
		QIcon(":/types/icons/scroll16"), "Document", 
		this, SLOT(setTypeDocument())
	);
	menu.addAction(
		QIcon(":/types/icons/program16"), "Application", 
		this, SLOT(setTypeApplication())
	);
	menu.addAction("Any type", this, SLOT(setTypeAny()));

	QPoint pos(0, m_ui->typeSelect->height());
	menu.exec(m_ui->typeSelect->mapToGlobal(pos));

	m_ui->typeSelect->setChecked(false);
	m_ui->typeSelect->setCheckable(false);
}

void SearchPage::showNetMenu() {
	QMenu menu(m_ui->netSelect);
	m_ui->netSelect->setCheckable(true);
	m_ui->netSelect->setChecked(true);

	menu.addAction(
		QIcon(":/networks/icons/donkey16"), "eDonkey", 
		this, SLOT(setNetDonkey())
	);
	menu.addAction(
		QIcon(":/transfer/icons/clear16"), "All Networks", 
		this, SLOT(setNetAny())
	);

	QPoint pos(0, m_ui->netSelect->height());
	menu.exec(m_ui->netSelect->mapToGlobal(pos));
	
	m_ui->netSelect->setChecked(false);
	m_ui->netSelect->setCheckable(false);
}

void SearchPage::setTypeAudio() {
	m_ui->typeSelect->setText("Audio");
	m_ui->typeSelect->setIcon(QIcon(":/types/icons/soundfile16"));
}

void SearchPage::setTypeVideo() {
	m_ui->typeSelect->setText("Video");
	m_ui->typeSelect->setIcon(QIcon(":/types/icons/movie16"));
}

void SearchPage::setTypeCddvd() {
	m_ui->typeSelect->setText("CD/DVD");
	m_ui->typeSelect->setIcon(QIcon(":/types/icons/cd-dvd16"));
}

void SearchPage::setTypeArchive() {
	m_ui->typeSelect->setText("Archive");
	m_ui->typeSelect->setIcon(QIcon(":/types/icons/archive16"));
}

void SearchPage::setTypePicture() {
	m_ui->typeSelect->setText("Picture");
	m_ui->typeSelect->setIcon(QIcon(":/types/icons/image16"));
}

void SearchPage::setTypeDocument() {
	m_ui->typeSelect->setText("Document");
	m_ui->typeSelect->setIcon(QIcon(":/types/icons/scroll16"));
}

void SearchPage::setTypeApplication() {
	m_ui->typeSelect->setText("Document");
	m_ui->typeSelect->setIcon(QIcon(":/types/icons/program16"));
}

void SearchPage::setTypeAny() {
	m_ui->typeSelect->setText("Type");
	m_ui->typeSelect->setIcon(QIcon());
}

void SearchPage::setNetDonkey() {
	m_ui->netSelect->setText("eDonkey");
	m_ui->netSelect->setIcon(QIcon(":/networks/icons/donkey16"));
}
void SearchPage::setNetAny() {
	m_ui->netSelect->setText("Networks");
	m_ui->netSelect->setIcon(QIcon(":/transfer/icons/clear16"));
}

bool SearchPage::eventFilter(QObject *obj, QEvent *evt) {
	QLineEdit *ed = 0;
	QString text;
	if (obj == m_ui->minSize) {
		ed = m_ui->minSize;
		text = "min size";
	} else if (obj == m_ui->maxSize) {
		ed = m_ui->maxSize;
		text = "max size";
	}

	if (ed && evt->type() == QEvent::FocusIn) {
		if (ed->text() == text) {
			ed->clear();
			ed->setValidator(new QIntValidator(ed));
		}
	} else if (ed && evt->type() == QEvent::FocusOut) {
		if (ed->text().isEmpty()) {
			ed->setValidator(0);
			ed->setText(text);
		}
	}
//	if (evt->type() == QEvent::MouseButtonPress) {
//		m_pressPos = dynamic_cast<QMouseEvent*>(evt)->globalPos();
//	} else if (evt->type() == QEvent::MouseButtonRelease) {
//		QPoint relPos = dynamic_cast<QMouseEvent*>(evt)->globalPos();
//		QWidget *obj;
//		if (m_ui->detailsBar->isHidden()) {
//			obj = window();
//		} else {
//			obj = m_ui->detailsBar;
//		}
//		if (
//			m_closeButton.contains(obj->mapFromGlobal(m_pressPos))
//			&& m_closeButton.contains(obj->mapFromGlobal(relPos))
//		) {
//			bool hide = !m_ui->detailsBar->isHidden();
//			m_ui->detailsBar->setHidden(hide);
//			QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
//			conf.setValue("SearchDetails", !hide);
//			window()->update();
//			return true;
//		}
//	}
	if (evt->type() != QEvent::Paint) {
		return false;
	}

	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	bool enableBars = conf.value("EnableBars").toBool();

	if (obj == m_ui->actionBar) {
		if (m_actionBack.isNull() || !enableBars) {
			return false;
		}
		QPainter p(m_ui->actionBar);
		int w = m_ui->actionBar->width();
		int h = m_ui->actionBar->height();
		if (m_actionBack.width() != w || m_actionBack.height() != h) {
			m_actionBack = m_actionBackOrig.scaled(
				w, h, Qt::IgnoreAspectRatio,
				Qt::SmoothTransformation
			);
		}
		p.drawPixmap(0, 0, m_actionBack);
//	} else if (obj == m_ui->detailsBar) {
//		QPainter p(m_ui->detailsBar);
//		if (!m_detailsBack.isNull() && enableBars) {
//			int w = m_ui->detailsBar->width() - 2;
//			int h = m_ui->detailsBar->height() - 2;
//			if (
//				m_detailsBack.width() != w || 
//				m_detailsBack.height() != h
//			) {
//				m_detailsBack = m_detailsBackOrig.scaled(
//					w, h, Qt::IgnoreAspectRatio,
//					Qt::SmoothTransformation
//				);
//			}
//			p.drawPixmap(1, 1, m_detailsBack);
//			p.setPen(QColor(153, 153, 153, 132));
//			p.drawRect(
//				0, 0, m_ui->detailsBar->width() - 1,
//				m_ui->detailsBar->height()
//			);
//		}
//		QPixmap btn(imgDir() + "closedetails.png");
//		if (!btn.isNull()) {
//			int posX = (m_ui->detailsBar->width() - btn.width()) /2;
//			int posY = 0;
//			p.drawPixmap(posX, posY, btn);
//			m_closeButton = QRect(
//				posX, posY, btn.width(), btn.height()
//			);
//		}
//	} else if (obj == window() && m_ui->detailsBar->isHidden()) {
//		if (isHidden()) {
//			return false;
//		}
//		QPainter p(window());
//		QPixmap btn(imgDir() + "opendetails.png");
//		if (!btn.isNull()) {
//			int posX = (window()->width() - btn.width()) / 2;
//			int posY = window()->height() - btn.height();
//			p.drawPixmap(posX, posY, btn);
//			m_closeButton = QRect(
//				posX, posY, btn.width(), btn.height()
//			);
//		}
	}

	return false;
}

void SearchPage::startSearch() {
	using namespace Engine;

	if (
		!MainWindow::instance().getEngine() ||
		!MainWindow::instance().getEngine()->getMain()
	) {
		QMessageBox::warning(
			this, "Unable to search",
			"Unable to perform search because "
			"Engine communication is offline.",
			QMessageBox::Ok, QMessageBox::NoButton
		);
		return;
	}

	m_ui->results->clear();
	if (m_currentSearch) {
		delete m_currentSearch;
	}
	m_currentSearch = new Engine::Search(
		MainWindow::instance().getEngine()->getMain(),
		boost::bind(&SearchPage::results, this, _1),
		m_ui->searchText->text().toStdString()
	);
	if (m_ui->typeSelect->text() == "Audio") {
		m_currentSearch->setType(FT_AUDIO);
	} else if (m_ui->typeSelect->text() == "Video") {
		m_currentSearch->setType(FT_VIDEO);
	} else if (m_ui->typeSelect->text() == "CD/DVD") {
		m_currentSearch->setType(FT_CDDVD);
	} else if (m_ui->typeSelect->text() == "Archive") {
		m_currentSearch->setType(FT_ARCHIVE);
	} else if (m_ui->typeSelect->text() == "Picture") {
		m_currentSearch->setType(FT_IMAGE);
	} else if (m_ui->typeSelect->text() == "Document") {
		m_currentSearch->setType(FT_DOC);
	} else if (m_ui->typeSelect->text() == "Application") {
		m_currentSearch->setType(FT_PROG);
	}

	quint64 minSize = m_ui->minSize->text().toULongLong() * 1024 * 1024;
	quint64 maxSize = m_ui->maxSize->text().toULongLong() * 1024 * 1024;
	if (minSize) {
		m_currentSearch->setMinSize(minSize);
	}
	if (maxSize) {
		m_currentSearch->setMaxSize(maxSize);
	}
	m_currentSearch->run();

	QString tmpName(m_ui->searchText->text());
	if (tmpName.size() > 10) {
		tmpName.truncate(10);
		tmpName += "...";
	}
	clearDetails();
}

void SearchPage::results(
	const std::vector<Engine::SearchResultPtr> &res
) {
	if (!m_currentSearch) {
		return;
	}
	setUpdatesEnabled(false);
	for (size_t i = 0; i < res.size(); ++i) {
		new SearchListItem(m_ui->results, res[i]);
	}
	m_ui->results->sortItems(
		m_ui->results->sortColumn(),
		m_ui->results->header()->sortIndicatorOrder()
	);
	setUpdatesEnabled(true);
}

void SearchPage::showEvent(QShowEvent *evt) {
	m_ui->searchText->setFocus();
	QWidget::showEvent(evt);
}

void SearchPage::clearResults() {
	m_ui->results->clear();
	m_ui->searchText->clear();
	setTypeAny();
	setNetAny();
	m_ui->minSize->setValidator(0);
	m_ui->minSize->setText("min size");
	m_ui->maxSize->setValidator(0);
	m_ui->maxSize->setText("max size");
	if (m_currentSearch) {
		delete m_currentSearch;
		m_currentSearch = 0;
	}
}

void SearchPage::changeFileDest() {
//	QString s = QFileDialog::getExistingDirectory(
//		window(), "Choose destination directory",
//		m_detailsBar->dest->text(), QFileDialog::ShowDirsOnly
//	);
//	if (s.size() && s != m_detailsBar->dest->text() && m_currentActive){
//		m_currentActive->setDest(s);
//		m_detailsBar->dest->setText(s);
//	}
}

void SearchPage::updateDetails(QTreeWidgetItem *_cur, QTreeWidgetItem *) {
//	SearchListItem *cur = dynamic_cast<SearchListItem*>(_cur);
//	if (!cur || !cur->getData()) {
//		clearDetails();
//		return;
//	}
//	m_currentActive = cur;
//	m_currentConnection = m_currentActive->getData()->onUpdated.connect(
//		boost::bind(&SearchPage::updateDetails, this)
//	);
//	updateDetails();
}

void SearchPage::updateDetails() {
	if (!m_currentActive) {
		return;
	}

//	m_detailsBar->dest->setText(m_currentActive->getDest());
//	m_detailsBar->fileName->setText(m_currentActive->getName());
//	m_detailsBar->sourceCount->setText(
//		QString::number(m_currentActive->getData()->getSourceCnt())
//	);
//	m_detailsBar->fullSourceCount->setText(
//		QString::number(m_currentActive->getData()->getFullSrcCnt())
//	);
//	m_detailsBar->bitrate->setText(
//		QString::number(m_currentActive->getData()->getBitrate())
//	);
//	m_detailsBar->codec->setText(
//		QString::fromStdString(m_currentActive->getData()->getCodec())
//	);
//	m_detailsBar->length->setText(
//		QString::number(m_currentActive->getData()->getLength())
//	);
//	m_ui->detailsBar->setEnabled(true);
}

void SearchPage::clearDetails() {
//	m_ui->detailsBar->setEnabled(false);
//	m_currentActive = 0;
//	m_currentConnection.disconnect();
}

void SearchPage::downloadClicked() {
	if (!m_currentActive || !m_currentActive->getData()) {
		return;
	}
	m_currentActive->download();
}

void SearchPage::cleanName() {
	if (!m_currentActive || !m_currentActive->getData()) {
		return;
	}

//	QString curName = ::cleanName(m_detailsBar->fileName->text());
//	m_detailsBar->fileName->setText(curName);
//	m_currentActive->setName(curName);
}

SearchInput::SearchInput(QWidget *parent) : QLineEdit(parent) {}
void SearchInput::paintEvent(QPaintEvent *evt) {
	QLineEdit::paintEvent(evt);
/*	QPainter p(this);
	p.setBrush(QColor(0, 0, 255, 80));
	p.setPen(QColor(0, 0, 255));
	p.drawRect(1, 1, width()/2, height()-3);
*/}
