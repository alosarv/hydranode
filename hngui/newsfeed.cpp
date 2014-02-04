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
 
#include "newsfeed.h"
#include "main.h"
#include <QHttp>
#include <QXmlDefaultHandler>
#include <QXmlSimpleReader>
#include <QDateTime>
#include <QProcess>
#include <QSettings>
#include <QTimer>
#include <QDir>

// RssReader class
// ------------------------
class RssReader : public QXmlDefaultHandler {
public:
	RssReader(int limit = 5);
	virtual bool startElement(
		const QString &namespaceURI, const QString &localName, 
		const QString &qName, const QXmlAttributes &atts
	);
	virtual bool endElement(
		const QString & namespaceURI, const QString & localName, 
		const QString & qName
	);
	virtual bool error(const QXmlParseException &exception);
	virtual bool warning(const QXmlParseException &exception);
	virtual bool fatalError(const QXmlParseException &exception);
	virtual bool characters(const QString &ch);
	void addEntry();
	QString getOutput() { return m_output; }
private:
	enum Location { 
		InEntry = 1, InTitle = 2, InDate = 4, InLink = 8, InDesc = 16
	};
	int m_location;
	QString m_currentTitle, m_currentDate, m_currentLink, m_currentDesc;
	int m_count;
	int m_limit;
	QString m_output; // final output (html)
};

RssReader::RssReader(int limit) : m_location(), m_count(), m_limit(limit) {}

bool RssReader::error(const QXmlParseException &e) {
	QString fmt("<br /><b>Parse error</b> (line:%1 column:%2): %3");
	m_output.append(
		fmt.arg(e.lineNumber()).arg(e.columnNumber()).arg(e.message())
	);
	return QXmlDefaultHandler::error(e);
}
bool RssReader::warning(const QXmlParseException &e) {
	QString fmt("<br /><b>Parse warning</b> (line:%1 column:%2): %3");
	m_output.append(
		fmt.arg(e.lineNumber()).arg(e.columnNumber()).arg(e.message())
	);
	return QXmlDefaultHandler::warning(e);
}
bool RssReader::fatalError(const QXmlParseException &e) {
	QString fmt("<br /><b>Parse error</b> (line:%1 column:%2): %3");
	m_output.append(
		fmt.arg(e.lineNumber()).arg(e.columnNumber()).arg(e.message())
	);
	return QXmlDefaultHandler::fatalError(e);
}

bool RssReader::startElement(
	const QString &namespaceURI, const QString &localName, 
	const QString &qName, const QXmlAttributes &atts
) {
	if (m_count > m_limit) { return false; }
	if (localName == "entry" || localName == "item") {
		m_location |= InEntry;
	} else if (!m_location & InEntry) {
		return true;
	}
	if (localName == "link") {
		if (atts.value("type") == "text/html") {
			m_currentLink = atts.value("href");
		} else {
			m_location |= InLink;
		}
	} else if (localName == "title") {
		m_location |= InTitle;
	} else if (localName == "issued" || localName == "pubDate") {
		m_location |= InDate;
	} else if (localName == "description") {
		m_location |= InDesc;
	}
	return true;
}

bool RssReader::endElement(
	const QString & namespaceURI, const QString & localName, 
	const QString & qName
) {
	if (localName == "entry" || localName == "item") {
		m_location &= ~InEntry;
		addEntry();
	} else if (m_location & InEntry) {
		if (localName == "title") {
			m_location &= ~InTitle;
		} else if (localName == "issued" || localName == "pubDate") {
			m_location &= ~InDate;
		} else if (localName == "link") {
			m_location &= ~InLink;
		} else if (localName == "description") {
			m_location &= ~InDesc;
		}
	}
	return true;
}

void RssReader::addEntry() {
	// unfortunately we cannot handle images properly yet, so in order to
	// avoid breaking the entire parsing, filter out the advertisments for
	// now.
	// TODO: This NEEDS to be fixed as soon as possible, e.g. before feed
	//       owners start complaining!
	if (m_currentTitle == "Featured Advertisement") {
		m_currentLink.clear();
		m_currentTitle.clear();
		m_currentDate.clear();
		m_currentDesc.clear();
		return;
	}

	QDate d = QDate::fromString(m_currentDate.left(10), "yyyy-MM-dd");
	QString writeDate;
	if (d.isValid()) {
		writeDate = d.toString(Qt::LocalDate);
	} else {
		d = QDate::fromString(m_currentTitle.left(10), "dd.MM.yyyy");
		if (d.isValid()) {
			writeDate = d.toString(Qt::LocalDate);
			m_currentTitle.remove(0, 13);
		} else {
			writeDate = m_currentDate.mid(5, 11);
		}
	}
	if (!writeDate.isEmpty()) {
		writeDate.insert(0, " - ");
	}
	m_output += QString("<b><a href=\"%1\">%2</a>%3</b> %4<br />")
	.arg(m_currentLink).arg(m_currentTitle).arg(writeDate).arg(
		m_currentDesc.size() > 155 
		? m_currentDesc.left(155) + "..." : m_currentDesc
	);

	m_currentLink.clear();
	m_currentTitle.clear();
	m_currentDate.clear();
	m_currentDesc.clear();
	++m_count;
}

bool RssReader::characters(const QString &ch) {
	if (m_location & InTitle) {
		m_currentTitle += ch;
	} else if (m_location & InDate) {
		m_currentDate += ch;
	} else if (m_location & InLink) {
		m_currentLink += ch;
	} else if (m_location & InDesc) {
		m_currentDesc += ch;
	}
	return true;
}

// NewsFeed class
// ------------------

NewsFeed::NewsFeed(QWidget *parent) : QTextBrowser(parent) {
	m_timer = new QTimer(this);

	connect(
		this, SIGNAL(anchorClicked(const QUrl&)), 
		SLOT(linkClicked(const QUrl&))
	);
	connect(m_timer, SIGNAL(timeout()), SLOT(getUpdates()));
}

void NewsFeed::init() {
	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	QDateTime lastUpdate = QDateTime::fromString(
		conf.value(objectName() + "Updated", "").toString(), 
		Qt::ISODate
	);
	logDebug("Last update was on " + lastUpdate.toString());
	QDateTime current = QDateTime::currentDateTime();
	int timeDiff = lastUpdate.secsTo(current);
	logDebug(QString("Time diff since last update is %1").arg(timeDiff));
	if (timeDiff >= 60 * 60) {
		timeDiff = 60 * 60;
	}
	m_timer->setSingleShot(true);
	timeDiff = 60 * 60 - timeDiff;
	m_timer->start(timeDiff * 1000);
	logDebug(QString("Next feeds update in %1 seconds").arg(timeDiff));
}

void NewsFeed::addFeed(QString title, QUrl url, int limit) {
	m_urls[title] = url;
	m_limits[title] = limit;
	m_feeds[title] = QString();
	if (m_currentActive.isEmpty()) {
		setActive(title);
	}

	QFile f(confDir() + "/feeds/" + title + ".xml");
	f.open(QIODevice::ReadOnly);
	if (f.isOpen()) {
		QString feed(f.readAll());
		m_feeds[title] = parseFeed(feed, limit);
		setHtml(m_feeds[title]);
	}
}

void NewsFeed::getUpdates() {
	QMap<QString, QUrl>::iterator it = m_urls.begin();
	while (it != m_urls.end()) {
		QString title = it.key();
		logDebug("Getting updates for feed " + title);
		QUrl url = it.value();
		if (m_links[title]) {
			m_links[title]->deleteLater();
		}
		m_links[title] = new QHttp(url.host(), url.port(80), this);
		m_reqs[m_links[title]->get(url.path())] = title;
		connect(
			m_links[title], SIGNAL(requestStarted(int)), 
			SLOT(requestStarted(int))
		);
		connect(
			m_links[title], SIGNAL(requestFinished(int, bool)), 
			SLOT(requestFinished(int, bool))
		);
		++it;
	}
	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	conf.setValue(
		objectName() + "Updated", 
		QDateTime::currentDateTime().toString(Qt::ISODate)
	);

	if (!m_timer->isActive()) {
		logDebug("Next feeds update in one hour.");
		m_timer->start(1000 * 60 * 60);
	}
}

void NewsFeed::setActive(QString title) {
	if (m_feeds.contains(title)) {
		clear();
		setHtml(m_feeds[title]);
		m_currentActive = title;
		currentChanged(m_currentActive);
	}
}

void NewsFeed::requestStarted(int code) {
//	setHtml("Downloading RSS feed...");
}

void NewsFeed::requestFinished(int code, bool error) {
	if (!m_links[m_reqs[code]]) {
		return;
	}

	if (error) {
		m_feeds[m_reqs[code]] = m_links[m_reqs[code]]->errorString();
		return;
	}

	QString feed = m_links[m_reqs[code]]->readAll();
	feed.replace("&", "&#38;");
	feed.replace("“", "\"");
	feed.replace("”", "\"");

	m_feeds[m_reqs[code]] = parseFeed(feed, m_limits[m_reqs[code]]);
	if (!m_feeds[m_reqs[code]].size()) {
		m_feeds[m_reqs[code]] = feed;
	}

	if (currentActive() == m_reqs[code]) {
		setActive(m_reqs[code]);
	}

	// save cache
	QDir d(confDir());
	if (!d.exists("feeds")) {
		d.mkdir("feeds");
	}
	QFile f(d.absolutePath() + "/feeds/" + m_reqs[code] + ".xml");
	f.open(QIODevice::WriteOnly);
	if (f.isOpen()) {
		f.write(feed.toAscii());
	}

	// cleanup
	m_links[m_reqs[code]]->deleteLater();
	m_links.remove(m_reqs[code]);
	m_reqs.remove(code);
}

void NewsFeed::linkClicked(const QUrl &url) {
	setSource(QUrl());
#ifdef Q_OS_WIN32
	QStringList args;
	args << url.toString();
	QSettings s(
		"HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes"
		"\\HTTP\\shell\\open\\command", 
		QSettings::NativeFormat
	);
	QString cmd = s.value("").toString();
	Q_ASSERT(!cmd.isEmpty());
	bool ret = false;
	if (cmd.contains("%1")) {
		cmd.replace("%1", url.toString());
		ret = QProcess::startDetached(cmd);
	} else {
		ret = QProcess::startDetached(cmd, args);
	}
	Q_ASSERT(ret);
#endif
}

QString NewsFeed::parseFeed(const QString &feed, int limit) {
	QXmlSimpleReader reader;
	RssReader handler(limit);
	reader.setContentHandler(&handler);
	QXmlInputSource source;
	source.setData(feed);
	reader.parse(&source);
	return handler.getOutput();
}
