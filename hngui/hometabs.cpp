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

#include "hometabs.h"
#include "home_homepage.h"
#include "home_homeactions.h"
#include "pagetemplate.h"
#include "main.h"
#include <QPainter>
#include <QMenu>
#include <QSettings>
#include <QTreeWidget>

static HomePage *s_instance = 0;
HomePage& HomePage::instance() { return *s_instance; }

HomePage::HomePage(QWidget *parent) : QWidget(parent),
m_background(":/backgrounds/backgrounds/default.png"), m_curPage() {
	s_instance = this;

	m_template = new Ui::PageTemplate;
	m_template->setupUi(this);
	m_actionBar = new Ui::HomeActionBar;
	m_actionBar->setupUi(m_template->actionBar);

	m_contentLayout = new QVBoxLayout(m_template->contentFrame);
	m_contentLayout->setMargin(0);
	m_contentLayout->setSpacing(0);
	m_template->contentFrame->setLayout(m_contentLayout);

	m_overviewPage = new QWidget(contentWidget());
	m_ui = new Ui::HomeContent;
	m_ui->setupUi(m_overviewPage);
	addPage("Overview", m_overviewPage);

	// default active is overview - TODO: save setting in config
	setActive("Overview");

	// make text controls backgrounds 100% transparent to allow background
	// to be displayed below it
	QPalette palette = m_ui->devFeed->viewport()->palette();
	palette.setColor(QPalette::Base, QColor(0, 0, 0, 0));
	m_ui->devFeed->viewport()->setPalette(palette);
	m_ui->newsFeed->viewport()->setPalette(palette);
	m_ui->myHydra->viewport()->setPalette(palette);

	// this doesn't seem to work yet (tested with Qt 4.1.1 on Windows)
	// adjust link coloring in text controls to slightly darker blue
//	palette = m_ui->devFeed->palette();
//	palette.setColor(QPalette::Link, QColor(0, 0, 204));
//	m_ui->devFeed->setPalette(palette);
//	m_ui->newsFeed->setPalette(palette);
//	m_ui->myHydra->setPalette(palette);

	// gray'ish frame borders
	palette = m_ui->newsFeedFrame->palette();
	QPalette oldPalette = palette;
	palette.setColor(QPalette::WindowText, QColor(153, 153, 153));
	m_ui->newsFeedFrame->setPalette(palette);
	m_ui->devFeedFrame->setPalette(palette);
	m_ui->myHydraFrame->setPalette(palette);

	m_template->actionBar->installEventFilter(this);

	connect(
		m_actionBar->rssButton, SIGNAL(clicked()), 
		SLOT(rssButtonClicked())
	);
	connect(
		m_ui->newsFeed, SIGNAL(currentChanged(const QString&)),
		SLOT(setNewsTitle(const QString&))
	);
	connect(
		m_actionBar->pageButton, SIGNAL(clicked()),
		SLOT(changePage())
	);

	m_ui->devFeed->addFeed(
		"Hydranode Development News", 
		QUrl("http://hydranode.com/blog/atom.xml"), 5
	);
	m_ui->devFeed->setActive("Hydranode Development News");

	m_ui->newsFeed->addFeed(
		"Slashdot - Stuff that matters", 
		QUrl("http://rss.slashdot.org/Slashdot/slashdot"), 10
	);
	m_ui->newsFeed->addFeed(
		"BBC World News", 
		QUrl(
			"http://newsrss.bbc.co.uk/rss/"
			"newsonline_world_edition/front_page/rss.xml"
		), 10
	);
	m_ui->newsFeed->addFeed(
		"CNN World News", 
		QUrl("http://rss.cnn.com/rss/cnn_topstories.rss"), 10
	);
	m_ui->newsFeed->addFeed(
		"Tom's Hardware News", 
		QUrl("http://www.pheedo.com/f/toms_hardware"), 10
	);
	m_ui->newsFeed->addFeed(
		"Slyck - File Sharing News", 
		QUrl("http://slyck.com/slyckrss.xml"), 30
	);
	m_ui->devFeed->init();
	m_ui->newsFeed->init();

	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	if (conf.contains("ActiveFeed")) {
		m_ui->newsFeed->setActive(conf.value("ActiveFeed").toString());
	}

	m_actionBack = QPixmap(imgDir() + "actionback.png");
	m_actionBackOrig = m_actionBack;
}

QWidget* HomePage::contentWidget() const {
	return m_template->contentFrame;
}

void HomePage::addPage(QString name, QWidget *w) {
	m_contentLayout->addWidget(w);
	w->hide();
	m_pageMap[name] = w;
}

void HomePage::setActive(QString name) {
	setUpdatesEnabled(false);
	QWidget *w = m_pageMap.value(name);
	if (w && w != m_curPage) {
		if (m_curPage) {
			m_curPage->hide();
		}
		w->show();
		m_curPage = w;
		if (name == "Overview") {
			m_actionBar->rssButton->show();
			name = "Plugins";
		} else {
			m_actionBar->rssButton->hide();
		}
		m_actionBar->pageButton->setText(name);
	}
	setUpdatesEnabled(true);
}

void HomePage::changePage() {
	QMenu menu(this);
	Q_FOREACH(QString title, m_pageMap.keys()) {
		if (m_pageMap[title] == m_curPage) {
			menu.addAction(QIcon(":/transfer/icons/clear16"),title);
		} else {
			menu.addAction(title);
		}
	}
	m_actionBar->pageButton->setCheckable(true);
	m_actionBar->pageButton->setChecked(true);
	QPoint pos(0, m_actionBar->pageButton->height());
	QAction *ret = menu.exec(m_actionBar->pageButton->mapToGlobal(pos));
	if (ret) {
		setActive(ret->text());
	}
	m_actionBar->pageButton->setChecked(false);
	m_actionBar->pageButton->setCheckable(false);
}

void HomePage::setNewsTitle(const QString &title) {
	QString header("<span style='color:black'>%1</span>");
	m_ui->newsTitle->setText(header.arg(title));
}

void HomePage::paintEvent(QPaintEvent *evt) {
	QPainter p(this);
	p.setPen(palette().color(QPalette::Mid));
	p.setBrush(palette().base());
	p.drawRect(
		FRAMELBORDER, ACBAR_HEIGHT, 
		width() - FRAMELBORDER - FRAMERBORDER - 1, 
		height() - ACBAR_HEIGHT - FRAMEBBORDER - 1
	);

	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	if (!m_background.isNull() && conf.value("EnableBack").toBool()) {
		p.drawPixmap(
			width() - m_background.width() - FRAMELBORDER - 1,
			height() - m_background.height() - FRAMEBBORDER - 1,
			m_background
		);
	}
	QWidget::paintEvent(evt);
}

bool HomePage::eventFilter(QObject *obj, QEvent *evt) {
	if (obj != m_template->actionBar || evt->type() != QEvent::Paint) {
		return false;
	}
	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	if (!conf.value("EnableBars").toBool() || m_actionBack.isNull()) {
		return false;
	}

	QPainter p(m_template->actionBar);
	int w = m_template->actionBar->width();
	int h = m_template->actionBar->height();
	if (m_actionBack.width() != w || m_actionBack.height() != h) {
		m_actionBack = m_actionBackOrig.scaled(
			w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation
		);
	}
	p.drawPixmap(0, 0, m_actionBack);
	return false;
}

void HomePage::rssButtonClicked() {
	QMenu m(this);
	m_actionBar->rssButton->setCheckable(true);
	m_actionBar->rssButton->setChecked(true);

	QMap<QString, QAction*> feeds;
	feeds["BBC World News"] = m.addAction("BBC World News");
	feeds["CNN World News"] = m.addAction("CNN World News");
	feeds["Tom's Hardware News"] = m.addAction("Tom's Hardware News");
	feeds["Slashdot - Stuff that matters"] = m.addAction(
		"Slashdot - Stuff that matters"
	);
	feeds["Slyck - File Sharing News"] = m.addAction(
		"Slyck - File Sharing News"
	);
	feeds[m_ui->newsFeed->currentActive()]->setIcon(
		QIcon(":/transfer/icons/clear16")
	);
//	m.addSeparator();
//	m.addAction("Add...");

	QPoint pos(0, m_actionBar->rssButton->height());
	pos = m_actionBar->rssButton->mapToGlobal(pos);
	pos.setX(pos.x());
	QAction *ret = m.exec(pos);
	if (ret && ret->text().size() && ret->text() != "Add...") {
		m_ui->newsFeed->setActive(ret->text());
		QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
		conf.setValue("ActiveFeed", ret->text());
	}

	m_actionBar->rssButton->setChecked(false);
	m_actionBar->rssButton->setCheckable(false);
}

// NewsHeader class
// ----------------
NewsHeader::NewsHeader(QWidget *parent) : QFrame(parent) {}

void NewsHeader::paintEvent(QPaintEvent *evt) {
//	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
//	if (!conf.value("EnableBars").toBool()) {
//		QFrame::paintEvent(evt);
//		return;
//	}
	QPainter p(this);
	p.setPen(QColor(153, 153, 153));
	p.setBrush(QColor(196, 196, 195, 183));
	p.drawRect(-1, -1, width() + 1, height());
	return;

//	QPixmap px(":/backgrounds/backgrounds/myhydrabar");
//	if (!px.isNull()) {
//		px = px.scaledToHeight(height(), Qt::SmoothTransformation);
//		for (int i = 0; i < width() + px.width(); ) {
//			p.drawPixmap(i, 0, px);
//			i += px.width() - 1;
//		}
//	}
}

// MyHydraHeader class
// ----------------
MyHydraHeader::MyHydraHeader(QWidget *parent) : QFrame(parent) {}

void MyHydraHeader::paintEvent(QPaintEvent *evt) {
//	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
//	if (!conf.value("EnableBars").toBool()) {
//		QFrame::paintEvent(evt);
//		return;
//	}

	QPainter p(this);
	p.setPen(QColor(153, 153, 153));
	p.setBrush(QColor(132, 160, 185, 183));
	p.drawRect(-1, -1, width() + 1, height());
	return;

//	QPixmap px(":/backgrounds/backgrounds/myhydrabar");
//	if (!px.isNull()) {
//		px = px.scaledToHeight(height(), Qt::SmoothTransformation);
//		for (int i = 0; i < width() + px.width(); ) {
//			p.drawPixmap(i, 0, px);
//			i += px.width() - 1;
//		}
//	}
}

