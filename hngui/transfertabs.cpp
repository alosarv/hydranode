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

#include "transfertabs.h"
#include "transfercontent.h"
//#include "filedetailsdock.h"
#include "commentframe.h"
#include "customheader.h"
#include "catdialog.h"
#include "filetypes.h"
#include "main.h"
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QMenu>
#include <QSettings>
#include <QFileDialog>
#include <QClipboard>
#include <QMessageBox>
#include <QTimer>
#include <boost/bind.hpp>

// CommentList class
// -----------------
CommentList::CommentList(QWidget *parent) : HTreeWidget(parent) {
	m_commentB = QPixmap(imgDir() + "commentsback.png");
	m_commentBOrig = m_commentB;
//	for (int i = 0; i < 16; ++i) {
//		palette.setColor(QPalette::ColorRole(i), QColor(0, 0, 0, 0));
//	}
//	palette.setColor(QPalette::Base, QColor(0, 0, 0, 0));
//	viewport()->setPalette(palette);
}

void CommentList::paintEvent(QPaintEvent *evt) {
	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	if (conf.value("EnableBars").toBool()) {
		QPalette palette(this->palette());
		palette.setColor(QPalette::Base, QColor(204, 210, 214, 255));
		palette.setColor(QPalette::AlternateBase, QColor(204, 210,214));
		palette.setColor(QPalette::Highlight, QColor(222, 224, 228));
		setPalette(palette);
	} else {
		setPalette(QApplication::palette());
	}

	HTreeWidget::paintEvent(evt);
#if 0
	if (m_commentB.width() != width() || m_commentB.height() != height()) {
		m_commentB = m_commentBOrig.scaled(
			width(), height(), Qt::IgnoreAspectRatio,
			Qt::SmoothTransformation
		);
	}
	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	if (conf.value("EnableBars").toBool()) {
		QPainter p(viewport());
		p.drawPixmap(0, 0, m_commentB);
	}
#endif
//	QPainter p(viewport());
//	QPixmap btn(imgDir() + "opendetails.png");
//	if (!btn.isNull()) {
//		int posX = (window()->width() - btn.width()) / 2;
//		int posY = window()->height() - btn.height();
//		p.drawPixmap(
//			viewport()->mapFrom(window(), QPoint(posX, posY)), btn
//		);
//	}
}

// CommentListItem class
////////////////////////
CommentListItem::CommentListItem(QTreeWidget *parent)
: QTreeWidgetItem(parent) {}

CommentListItem::CommentListItem(QTreeWidgetItem *parent)
: QTreeWidgetItem(parent) {}

bool CommentListItem::operator<(const QTreeWidgetItem &other) const {
	int col = treeWidget()->header()->sortIndicatorSection();
	if (col == 0) {
		text(0) < other.text(0);
	} else if (col == 1) {
		return text(1).toUInt() < other.text(1).toUInt();
	}
	return QTreeWidgetItem::operator<(other);
}


// TransferPage class
///////////////////////////
TransferPage::TransferPage(QWidget *parent) : QWidget(parent),m_currentActive(),
m_catDlgWidget(), m_commentTimer(new QTimer(this)) {
	m_background = QPixmap(":/backgrounds/backgrounds/default");

	m_ui = new Ui::TransferContent;
	m_ui->setupUi(this);
//	m_detailsBar = new Ui::DetailsBar;
//	m_detailsBar->setupUi(m_ui->detailsBar);
	m_commentFrame = new Ui::CommentFrame;
	m_commentFrame->setupUi(m_ui->commentFrame);

	CustomHeader *h = new CustomHeader(
		m_commentFrame->commentList->header()->orientation(),
		m_commentFrame->commentList->header()->parentWidget()
	);
	h->disableColumnSelection();
	m_commentFrame->commentList->setHeader(h);
	h->setObjectName("comments");
	h->setFocusPolicy(Qt::NoFocus);
	m_commentFrame->commentList->setHeaderLabels(
		QStringList() << "File Name or Comment" << "Frequency"
	);

	m_ui->allList->init();
	m_ui->notifyMsg->hide();

	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	m_ui->detailsBar->hide();
//	m_ui->detailsBar->setHidden(!conf.value("TransferDetails").toBool());
	m_ui->commentFrame->setHidden(!conf.value("ShowComments").toBool());
//	m_detailsBar->namesButton->setChecked(
//		conf.value("ShowComments").toBool()
//	);

	connect(
		m_ui->allList, SIGNAL(itemSelectionChanged()),
		SLOT(updateButtons())
	);
	connect(
		m_ui->clearButton, SIGNAL(clicked()), m_ui->allList,
		SLOT(clear())
	);
	connect(m_ui->statusButton, SIGNAL(clicked()), SLOT(showStatusMenu()));
	connect(m_ui->addButton, SIGNAL(clicked()), SLOT(showAddMenu()));

	connect(m_ui->filterText, SIGNAL(returnPressed()), SLOT(applyFilter()));
	connect(m_ui->filterButton, SIGNAL(toggled(bool)), SLOT(toggleFilter()));
	connect(m_ui->catButton, SIGNAL(clicked()), SLOT(newCategory()));
	connect(
		m_ui->allList,
		SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)),
		SLOT(updateDetails(QTreeWidgetItem*, QTreeWidgetItem*))
	);
	connect(
		m_commentFrame->closeButton, SIGNAL(clicked()),
		SLOT(hideComments())
	);
//	connect(
//		m_commentFrame->closeButton, SIGNAL(clicked()),
//		m_detailsBar->namesButton, SLOT(click())
//	);
//	connect(
//		m_detailsBar->namesButton, SIGNAL(toggled(bool)),
//		SLOT(showComments(bool))
//	);
	connect(
		m_ui->allList, SIGNAL(showComments()), SLOT(showComments())
	);
	connect(m_ui->typeSelect, SIGNAL(clicked()), SLOT(showTypeMenu()));
//	connect(
//		m_detailsBar->destButton, SIGNAL(clicked()), 
//		SLOT(changeFileDest())
//	);
//	connect(
//		m_detailsBar->setButton, SIGNAL(clicked()), 
//		SLOT(renameClicked())
//	);
//	connect(m_detailsBar->cleanButton, SIGNAL(clicked()),SLOT(cleanName()));
	connect(m_commentTimer, SIGNAL(timeout()), SLOT(getMoreComments()));
	connect(
		m_ui->allList, SIGNAL(namesUpdated(Engine::DownloadInfoPtr)),
		SLOT(updateComments(Engine::DownloadInfoPtr))
	);
	connect(
		m_ui->allList, SIGNAL(commentsUpdated(Engine::DownloadInfoPtr)),
		SLOT(updateComments(Engine::DownloadInfoPtr))
	);
	connect(
		m_commentFrame->commentList,
		SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)),
		SLOT(renameFromComment(QTreeWidgetItem*, int))
	);

	m_ui->actionBar->installEventFilter(this);
//	m_ui->detailsBar->installEventFilter(this);
	m_ui->allList->installEventFilter(this);
	m_ui->allList->viewport()->installEventFilter(this);
	m_commentFrame->commentList->installEventFilter(this);
	m_commentFrame->commentList->viewport()->installEventFilter(this);
	m_commentFrame->commentHeaderF->installEventFilter(this);
	window()->installEventFilter(this);
	updateButtons();
	clearDetails();
	getMoreComments(); // starts comment-updating timer

	QPalette palette = m_ui->allList->viewport()->palette();
	palette.setColor(QPalette::Base, QColor(0, 0, 0, 0));
//	m_detailsBar->fileName->setPalette(palette);
//	m_detailsBar->location->setPalette(palette);
//	m_detailsBar->title->setPalette(palette);
//	m_detailsBar->artist->setPalette(palette);
//	m_detailsBar->album->setPalette(palette);
	palette = m_ui->filterText->palette();
	palette.setColor(QPalette::Base, QColor(239, 239, 239, 80));
	m_ui->filterText->setPalette(palette);

	QRect rect =QFontMetrics(m_ui->typeSelect->font()).boundingRect("Type");
	m_ui->typeSelect->setMinimumSize(rect.width() + 12, 0);

	m_actionBack = QPixmap(imgDir() + "actionback.png");
//	m_detailsBack = QPixmap(imgDir() + "detailsback.png");
	m_commentH = QPixmap(imgDir() + "commentsheader.png");
	m_commentHOrig = m_commentH;
	m_actionBackOrig = m_actionBack;
//	m_detailsBackOrig = m_detailsBack;
}

void TransferPage::showTypeMenu() {
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
		m_ui->allList->filterByText("");
		m_ui->typeSelect->setIcon(ret->icon());
		m_ui->filterText->clear();
		m_ui->filterButton->setChecked(false);
	} else if (ret) {
		m_ui->allList->filterByType(ret->text());
		m_ui->typeSelect->setIcon(ret->icon());
		m_ui->filterText->clear();
		m_ui->filterButton->setChecked(false);
	}
}

void TransferPage::clearFilter() {
	m_ui->filterText->setText("");
	m_ui->allList->filterByText("");
	m_ui->filterButton->setChecked(false);
	m_ui->typeSelect->setText("Type");
	m_ui->typeSelect->setIcon(QIcon());
}

void TransferPage::toggleFilter() {
	if (m_ui->filterButton->isChecked()) {
		applyFilter();
	} else {
		clearFilter();
	}
}

void TransferPage::applyFilter() {
	m_ui->allList->filterByText(m_ui->filterText->text());
	m_ui->filterButton->setChecked(m_ui->filterText->text().size());
	m_ui->typeSelect->setText("Type");
	m_ui->typeSelect->setIcon(QIcon());
}

void TransferPage::updateButtons() {
	bool setting = m_ui->allList->selectedItems().count();
	m_ui->statusButton->setEnabled(setting);
}

void TransferPage::showStatusMenu() {
	QMenu menu(m_ui->statusButton);
	m_ui->statusButton->setCheckable(true);
	m_ui->statusButton->setChecked(true);

	menu.addAction(
		QIcon(":/transfer/icons/pause16"),
		"Pause", m_ui->allList, SLOT(pause())
	);
	menu.addAction(
		QIcon(":/transfer/icons/stop16"),
		"Stop", m_ui->allList, SLOT(stop())
	);
	menu.addAction(
		QIcon(":/transfer/icons/resume16"),
		"Resume", m_ui->allList, SLOT(resume())
	);
	menu.addAction(
		QIcon(":/transfer/icons/cancel16"),
		"Cancel", m_ui->allList, SLOT(cancel())
	);

	QPoint pos(0, m_ui->statusButton->height());
	menu.exec(m_ui->statusButton->mapToGlobal(pos));

	m_ui->statusButton->setChecked(false);
	m_ui->statusButton->setCheckable(false);
}

void TransferPage::showAddMenu() {
	QMenu menu(m_ui->addButton);
	m_ui->addButton->setCheckable(true);
	m_ui->addButton->setChecked(true);

	QAction *fromFile = menu.addAction("From File");
	QAction *fromClip = menu.addAction("From Clipboard");
	QAction *doImport = menu.addAction("Import");
	QPoint pos(0, m_ui->addButton->height());

	QAction *ret = menu.exec(m_ui->addButton->mapToGlobal(pos));

	if (ret == fromFile) {
		QString s = QFileDialog::getOpenFileName(
			this, "Select file to open", "",
			"Download packages (*.torrent)"
		);
		QFile f(s);
		if (s.size() && !f.open(QIODevice::ReadOnly)) {
			QMessageBox::critical(
				this, "Hydranode error",
				"Failed to open file " + s + " for reading.",
				QMessageBox::Ok, QMessageBox::NoButton
			);
		} else if (s.size()) {
			QByteArray data(f.readAll());
			if (!data.size()) {
				QMessageBox::critical(
					this, "Hydranode error",
					"The specified file seems to be empty.",
					QMessageBox::Ok,
					QMessageBox::NoButton
				);
			} else {
				logDebug(QString("File size is %1").arg(data.size()));
				DownloadList::downloadFile(data);
			}
		}
	} else if (ret == fromClip) {
		QString clipText = QApplication::clipboard()->text();
		QStringList links(clipText.split("\n",QString::SkipEmptyParts));
		Q_FOREACH(QString link, links) {
			DownloadList::downloadLink(link.trimmed());
		}
	} else if (ret == doImport) {
		QString dir = QFileDialog::getExistingDirectory(
			this, "Select directory to import from"
		);
		if (dir.size()) {
#ifdef Q_OS_WIN32
			dir = dir.replace("/", "\\");
#endif
			DownloadList::importDownloads(dir);
		}
	}

	m_ui->addButton->setChecked(false);
	m_ui->addButton->setCheckable(false);
}

void TransferPage::keyPressEvent(QKeyEvent *evt) {
	if (evt->key() == Qt::Key_Escape) {
		clearFilter();
	} else {
		evt->ignore();
	}
}

void TransferPage::newCategory() {
	m_catDlg = new Ui::CatDialog;
	if (m_catDlgWidget) {
		delete m_catDlgWidget;
	}
	m_catDlgWidget = new QWidget;
	m_catDlg->setupUi(m_catDlgWidget);
	m_catDlgWidget->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
	QPoint pos(m_ui->catButton->geometry().bottomLeft());
	pos.setY(pos.y() + 7);
	m_catDlgWidget->move(mapToGlobal(pos));
	m_catDlg->catName->setFocus();
	m_catDlgWidget->show();
	connect(m_catDlg->okButton, SIGNAL(clicked()), SLOT(doNewCat()));
}

void TransferPage::doNewCat() {
	if (m_catDlg && m_catDlg->catName->text().size()) {
		m_ui->allList->newCategory(m_catDlg->catName->text());
	}
}

void TransferPage::paintEvent(QPaintEvent *evt) {
/*	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	if (!conf.value("EnableBack").toBool()) {
		QWidget::paintEvent(evt);
		return;
	}

	QPainter p(this);

	p.setPen(palette().color(QPalette::Mid));
	p.setBrush(palette().base());
	p.drawRect(
		5, 16 + m_ui->actionBar->height(),
		width() - 14, height() - 22 - m_ui->actionBar->height()
	);
	if (!m_background.isNull()) {
		p.drawPixmap(
			width() - m_background.width() - 6,
			height() - m_background.height() - 6,
			m_background
		);
	}
*/	QWidget::paintEvent(evt);
}

bool TransferPage::eventFilter(QObject *obj, QEvent *evt) {
//	if (evt->type() == QEvent::MouseButtonPress) {
//		m_pressPos = dynamic_cast<QMouseEvent*>(evt)->globalPos();
//	} else if (evt->type() == QEvent::MouseButtonRelease) {
//		QPoint relPos = dynamic_cast<QMouseEvent*>(evt)->globalPos();
//		QWidget *w;
//		if (m_ui->detailsBar->isHidden()) {
//			w = window();
//		} else {
//			w = m_ui->detailsBar;
//		}
//		if (
//			m_closeButton.contains(w->mapFromGlobal(m_pressPos))
//			&& m_closeButton.contains(w->mapFromGlobal(relPos))
//		) {
//			bool hide = !m_ui->detailsBar->isHidden();
//			m_ui->detailsBar->setHidden(hide);
//			if (!hide) {
//				updateDetails();
//			}
//			window()->update();
//			QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
//			conf.setValue("TransferDetails", !hide);
//			return true;
//		}
//	}
	if (evt->type() != QEvent::Paint) {
		return false;
	}
	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	if (obj == m_ui->actionBar && !m_actionBack.isNull()) {
		if (conf.value("EnableBars").toBool()){
			QPainter p(m_ui->actionBar);
			int w = m_ui->actionBar->width();
			int h = m_ui->actionBar->height();
			if (
				m_actionBack.width() != w ||
				m_actionBack.height() != h
			) {
				m_actionBack = m_actionBackOrig.scaled(
					w, h, Qt::IgnoreAspectRatio,
					Qt::SmoothTransformation
				);
			}
			p.drawPixmap(0, 0, m_actionBack);
		}
	} else if (obj == m_commentFrame->commentHeaderF) {
		if (!m_commentH.isNull()&&conf.value("EnableBars").toBool()){
			QPainter p(m_commentFrame->commentHeaderF);
			int w = m_commentFrame->commentHeaderF->width();
			int h = m_commentFrame->commentHeaderF->height();
			if (
				m_commentH.width() != w ||
				m_commentH.height() != h
			) {
				m_commentH = m_commentHOrig.scaled(
					w, h, Qt::IgnoreAspectRatio,
					Qt::SmoothTransformation
				);
			}
			p.drawPixmap(0, 0, m_commentH);
		}
//	} else if (obj == m_ui->detailsBar) {
//		QPainter p(m_ui->detailsBar);
//		if (!m_detailsBack.isNull()&&conf.value("EnableBars").toBool()){
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

void TransferPage::updateDetails(QTreeWidgetItem *_cur, QTreeWidgetItem *) {
	DownloadListItem *cur = dynamic_cast<DownloadListItem*>(_cur);
	if (!cur || !cur->getData()) {
//		clearDetails();
		clearComments();
		return;
	}
	m_currentActive = cur;
	m_currentConnection = m_currentActive->getData()->onUpdated.connect(
		boost::bind(&TransferPage::updateDetails, this, false)
	);
	m_currentDestroyConnection = m_currentActive->destroyed.connect(
		boost::bind(&TransferPage::clearDetails, this)
	);
//	if (m_ui->detailsBar->isVisible()) {
//		updateDetails(true);
//	}
	if (m_ui->commentFrame->isVisible()) {
		if (!cur->getData()->ncount()) {
			logDebug("Requesting names for file " + m_currentActive->text(0));
			cur->getData()->getNames();
			cur->getData()->getComments();
		}
		updateComments();
	}
}

void TransferPage::clearDetails() {
//	m_ui->detailsBar->setEnabled(false);
	m_currentActive = 0;
	m_currentConnection.disconnect();
	m_currentDestroyConnection.disconnect();
//	m_detailsBar->location->clear();
//	m_detailsBar->fileName->clear();
//	m_detailsBar->sourceCount->clear();
//	m_detailsBar->fullSourceCount->clear();
}

void TransferPage::updateDetails(bool force) {
	if (!m_currentActive) {
		return;
	}

//	if (force || !m_detailsBar->location->hasFocus()) {
//		m_detailsBar->location->setText(
//			QString::fromStdString(
//				m_currentActive->getData()->getDestDir()
//			)
//		);
//	}
//	if (force || !m_detailsBar->fileName->hasFocus()) {
//		m_detailsBar->fileName->setText(
//			QString::fromStdString(
//				m_currentActive->getData()->getName()
//			)
//		);
//	}
//	m_detailsBar->sourceCount->setText(
//		QString::number(m_currentActive->getData()->getSourceCnt())
//	);
//	m_detailsBar->fullSourceCount->setText(
//		QString::number(m_currentActive->getData()->getFullSrcCnt())
//	);
//	m_ui->detailsBar->setEnabled(true);
}

void TransferPage::getMoreComments() {
	if (!m_currentActive || !m_currentActive->getData()) {
		m_commentTimer->stop();
		m_commentTimer->start(5000);
		m_commentTimer->setSingleShot(true);
		return;
	} else {
		logDebug("Requesting names for file " + m_currentActive->text(0));
		m_currentActive->getData()->getNames();
		m_currentActive->getData()->getComments();
	}

	if (m_currentActive->getData()->ncount() <= 10) {
		logDebug("Next GetNames in 5 seconds for file " + m_currentActive->text(0));
		m_commentTimer->stop();
		m_commentTimer->start(5000);
		m_commentTimer->setSingleShot(true);
	} else if (m_currentActive->getData()->ncount() <= 20) {
		logDebug("Next GetNames in 15 seconds for file " + m_currentActive->text(0));
		m_commentTimer->stop();
		m_commentTimer->start(15000);
		m_commentTimer->setSingleShot(true);
	} else {
		logDebug("Next GetNames in 60 seconds for file " + m_currentActive->text(0));
		m_commentTimer->stop();
		m_commentTimer->start(60000);
		m_commentTimer->setSingleShot(true);
	}
}

void TransferPage::updateComments(Engine::DownloadInfoPtr item) {
	if (!m_currentActive || m_currentActive->getData() != item) {
		return;
	} else {
		updateComments();
	}
}

void TransferPage::updateComments() {
	DownloadListItem *item = m_currentActive;
	if (!item) {
		return;
	}

	QString tmp("Known file names and comments for \"");
	tmp += item->text(0).left(60);
	if (item->text(0).size() > 60) {
		tmp += "...";
	}
	tmp += "\"";
	m_commentFrame->commentHeader->setText(tmp);

	if (!item->getData()) {
		return;
	}

	m_commentFrame->commentList->clear();
	QTreeWidgetItem *names = new QTreeWidgetItem(
		m_commentFrame->commentList
	);
	names->setText(0, "File Names");
	Engine::DownloadInfo::NameIter ni = item->getData()->nbegin();
	while (ni != item->getData()->nend()) {
		if (!ni->first.size()) {
			++ni;
			continue;
		}
		QTreeWidgetItem *it = new CommentListItem(names);
		it->setText(0, QString::fromStdString(ni->first));
		it->setText(1, QString::number(ni->second));
		++ni;
	}
	QTreeWidgetItem *comments = new QTreeWidgetItem(
		m_commentFrame->commentList
	);
	comments->setText(0, "Comments");
	Engine::DownloadInfo::CommentIter ci = item->getData()->cbegin();
	while (ci != item->getData()->cend()) {
		if (!ci->size()) {
			++ci;
			continue;
		}
		QTreeWidgetItem *it = new CommentListItem(comments);
		it->setText(0, QString::fromStdString(*ci));
		++ci;
	}
	m_commentFrame->commentList->setItemsExpandable(true);
	if (!item->getData()->ncount()) {
		delete names;
	} else {
		m_commentFrame->commentList->setItemExpanded(names, true);
	}
	if (!item->getData()->ccount()) {
		delete comments;
	} else {
		m_commentFrame->commentList->setItemExpanded(comments, true);
	}
	m_commentFrame->commentList->setItemsExpandable(false);
	m_commentFrame->commentList->sortItems(1, Qt::DescendingOrder);
}

void TransferPage::changeFileDest() {
//	QString s = QFileDialog::getExistingDirectory(
//		window(), "Choose destination directory",
//		m_detailsBar->location->text(), QFileDialog::ShowDirsOnly
//	);
//	if (s.size() && s != m_detailsBar->location->text() && m_currentActive){
//		m_currentActive->getData()->setDest(s.toStdString());
//	}
}

void TransferPage::renameClicked() {
	if (!m_currentActive || !m_currentActive->getData()) {
		return;
	}

//	QString newName = m_detailsBar->fileName->text();
//	QString oldName = QString::fromStdString(
//		m_currentActive->getData()->getName()
//	);
//	if (newName != oldName) {
//		m_currentActive->getData()->setName(newName.toStdString());
//	}
//	QString newDest = m_detailsBar->location->text();
//	QString oldDest = QString::fromStdString(
//		m_currentActive->getData()->getDestDir()
//	);
//	if (newDest != oldDest) {
//		m_currentActive->getData()->setDest(newDest.toStdString());
//	}
}

void TransferPage::cleanName() {
	if (!m_currentActive || !m_currentActive->getData()) {
		return;
	}

//	QString curName = m_detailsBar->fileName->text();
//	curName = ::cleanName(curName);
//	m_detailsBar->fileName->setText(curName);
}

void TransferPage::clearComments() {
	m_commentFrame->commentList->clear();
	m_commentFrame->commentHeader->setText(
		"Known file names and comments"
	);
}

void TransferPage::hideComments() {
	m_ui->commentFrame->hide();
	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	conf.setValue("ShowComments", false);
}

void TransferPage::showComments() {
	m_ui->commentFrame->show();
	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	conf.setValue("ShowComments", true);
	if (m_currentActive && m_currentActive->getData()) {
		if (!m_currentActive->getData()->ncount()) {
			logDebug("Requesting names for file " + m_currentActive->text(0));
			m_currentActive->getData()->getNames();
			m_currentActive->getData()->getComments();
		}
	}
	updateComments();
}

void TransferPage::hideComments(bool hide) {
	m_ui->commentFrame->setHidden(hide);
	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	conf.setValue("ShowComments", !hide);
}

void TransferPage::showComments(bool show) {
	m_ui->commentFrame->setVisible(show);
	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	conf.setValue("ShowComments", show);
	if (m_currentActive && m_currentActive->getData()) {
		if (!m_currentActive->getData()->ncount()) {
			logDebug("Requesting names for file " + m_currentActive->text(0));
			m_currentActive->getData()->getNames();
			m_currentActive->getData()->getComments();
		}
	}
	updateComments();
}

void TransferPage::renameFromComment(QTreeWidgetItem *it, int col) {
	if (!m_currentActive || !m_currentActive->getData()) {
		return;
	}
	QTreeWidgetItem *parent = it->parent();
	if (parent && parent->text(0) != "Comments") {
		m_currentActive->getData()->setName(it->text(0).toStdString());
	}
}
