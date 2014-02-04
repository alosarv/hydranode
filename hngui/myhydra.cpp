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

#include "myhydra.h"
#include "main.h"
#include "ecomm.h"
#include "hometabs.h"
#include <boost/bind.hpp>
#include <hncgcomm/cgcomm.h>
#include <QPaintEvent>
#include <QPainter>
#include <QSettings>
// Plugins - these should be loaded dynamically in the future
#include "plugins/donkeypage.h"

extern QString bytesToString(uint64_t bytes);
extern QString secondsToString(qint64 sec, quint8 trunc);

// MyHydra class
////////////////
StatGraph *s_statGraph = 0;
Engine::Modules* MyHydra::s_modulesInfo = 0;
Engine::Network* MyHydra::s_networkInfo = 0;

MyHydra::MyHydra(QWidget *parent) : QTextBrowser(parent) {
	MainWindow *m = &MainWindow::instance();
	connect(m, SIGNAL(engineConnection(bool)),SLOT(engineConnection(bool)));
	if (m->getEngine() && m->getEngine()->getMain()) {
		engineConnection(true);
	}
#ifndef Q_OS_WIN32
	parent->setMinimumWidth(240);
#endif
}

void MyHydra::engineConnection(bool up) {
	if (up) {
		s_networkInfo = new Engine::Network(
			MainWindow::instance().getEngine()->getMain(),
			boost::bind(&MyHydra::onUpdated, this)
		);
		s_networkInfo->getList();
		s_networkInfo->monitor(750);
		s_statGraph->s_networkInfo = s_networkInfo;
		s_modulesInfo = new Engine::Modules(
			MainWindow::instance().getEngine()->getMain()
		);
		s_modulesInfo->getList();
		QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
		s_modulesInfo->monitor(
			conf.value("/stats/UpdateRate", 750).toInt()
		);
	}
}

void MyHydra::onUpdated() {
	QString valueField("<td align='right'><b>%1</b></td>");
	QString text;

	text += "<table style='width:100%'>";
	text += "<tr><td>Download Speed:</td><td align='right'>";
	text += "<span style='color:dark red;font-weight:bold'>%1</span>";
	text += "</td></tr>";
	text += "<tr><td>Downloaded (session):</td><td align='right'>";
	text += "<span style='color:dark red'>%3</span></td></tr>";
	text += "<tr><td>Downloaded (total):</td><td align='right'>";
	text += "<span style='color:dark red'>%5</span></td></tr>";

	text += "<tr><td>Upload Speed:</td><td align='right'>";
	text += "<span style='color:dark green;font-weight:bold'>%2</span>";
	text += "</td></tr>";
	text += "<tr><td>Uploaded (session):</td><td align='right'>";
	text += "<span style='color:dark green'>%4</span></td></tr>";
	text += "<tr><td>Uploaded (total):</td><td align='right'>";
	text += "<span style='color:dark green'>%6</span></td></tr>";

	text += "<tr><td>Session length:</td><td align='right'>%7</td></tr>";
	text += "<tr><td>Overall runtime:</td><td align='right'>%8</td></tr>";
	text += "</table>";
	text += "<br />";
	text += "Enabled modules: ";
	Engine::Modules::CIter j = s_modulesInfo->begin();
	while (j != s_modulesInfo->end()) {
		text += QString::fromStdString((*j).second->getDesc());
		++j;
		if (j != s_modulesInfo->end()) {
			text += ", ";
		}
	}

	QString downSpeed(bytesToString(s_networkInfo->getDownSpeed()) + "/s");
	QString upSpeed(bytesToString(s_networkInfo->getUpSpeed()) + "/s");
	for (int i = downSpeed.size(); i < 11; ++i) {
		downSpeed.insert(0, "&nbsp;");
	}
	for (int i = upSpeed.size(); i < 11; ++i) {
		upSpeed.insert(0, "&nbsp;");
	}

	setHtml(
		text.arg(downSpeed).arg(upSpeed)
		.arg(bytesToString(s_networkInfo->getSessionDown()))
		.arg(bytesToString(s_networkInfo->getSessionUp()))
		.arg(bytesToString(s_networkInfo->getTotalDown()))
		.arg(bytesToString(s_networkInfo->getTotalUp()))
		.arg(secondsToString(s_networkInfo->getSessionLength()/1000, 2))
		.arg(secondsToString(s_networkInfo->getOverallRuntime()/1000,2))
	);
	s_statGraph->onUpdated();

	if (s_modulesInfo->count() && !HomePage::instance().hasPage("eDonkey")){
		window()->setUpdatesEnabled(false);
		HomePage::instance().addPage(
			"eDonkey", new DonkeyPage(
				HomePage::instance().contentWidget(), 
				s_modulesInfo
			)
		);
		window()->setUpdatesEnabled(true);
	}
}

// StatGraph class
//////////////////
StatGraph::StatGraph(QWidget *parent) : QFrame(parent), m_largestUp(1),
m_largestDown(1) {
	s_statGraph = this;
}

void StatGraph::onUpdated() {
	m_upValues.push_back(s_networkInfo->getUpSpeed());
	m_downValues.push_back(s_networkInfo->getDownSpeed());

	quint64 gAvgDown = s_networkInfo->getSessionDown();
	quint64 sessionSeconds = s_networkInfo->getSessionLength() / 1000;
	if (sessionSeconds) {
		gAvgDown /= sessionSeconds;
	}

	if (s_networkInfo->getUpSpeed() > m_largestUp) {
		m_largestUp = s_networkInfo->getUpSpeed();
	}
	if (s_networkInfo->getDownSpeed() > m_largestDown) {
		m_largestDown = s_networkInfo->getDownSpeed();
	}
	if (m_upValues.size() > 25) {
		if (m_upValues.front() == m_largestUp) {
			m_largestUp = 0;
		}
		m_upValues.pop_front();
		if (!m_largestUp) {
			std::list<quint64>::iterator i(m_upValues.begin());
			quint64 largest = 0;
			while (i != m_upValues.end()) {
				if (*i > largest) {
					largest = *i;
				}
				++i;
			}
			m_largestUp = largest;
		}
	}
	if (m_downValues.size() > 25) {
		if (m_downValues.front() == m_largestDown) {
			m_largestDown = 0;
		}
		m_downValues.pop_front();
		if (!m_largestDown) {
			std::list<quint64>::iterator i(m_downValues.begin());
			quint64 largest = 0;
			while (i != m_downValues.end()) {
				if (*i > largest) {
					largest = *i;
				}
				++i;
			}
			m_largestDown = largest;
		}
	}
	if (gAvgDown * 2.5 > m_largestDown) {
		m_largestDown = static_cast<quint64>(gAvgDown * 2.5);
	}
	update();
}

void StatGraph::paintEvent(QPaintEvent *evt) {
	if (!m_largestUp || !m_largestDown || !m_downValues.size()) {
		return;
	}

	QPainter p(this);
	float scaleLimit = std::max(m_largestUp, m_largestDown);
	std::list<quint64>::reverse_iterator i;
	QPixmap bar(":/backgrounds/backgrounds/downbar.png");
	Q_ASSERT(!bar.isNull());
	int curPos = width() - 15;

	QColor borderColor(153, 153, 204);
	p.setPen(borderColor);
	p.drawRect(8, 7, width() - 16, 70);

	i = m_downValues.rbegin();
	while (i != m_downValues.rend() && curPos > 0) {
		int yPos = static_cast<int>(70.0 - *i / scaleLimit * 65.0);
		int imgHeight = static_cast<int>(*i / scaleLimit * 65.0 + 9.0);
		if (yPos < 8) {
			yPos = 8;
		}
		if (imgHeight > 70) {
			imgHeight = 70;
		}
		p.drawPixmap(
			curPos, yPos, bar.scaled(
				bar.width(), imgHeight,
				Qt::IgnoreAspectRatio, Qt::SmoothTransformation
			)
		);
		curPos -= bar.width() + 2;
		++i;
	}
}
