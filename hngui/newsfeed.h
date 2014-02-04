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

#ifndef __NEWSFEED_H__
#define __NEWSFEED_H__

#include <QTextBrowser>
#include <QMap>

class QHttp;

class NewsFeed : public QTextBrowser {
	Q_OBJECT
public:
	NewsFeed(QWidget *parent = 0);
	void addFeed(QString title, QUrl url, int limit = 5);
	QString currentActive() const { return m_currentActive; }
Q_SIGNALS:
	void currentChanged(const QString &current);
public Q_SLOTS:
	void getUpdates();
	void init();
	void setActive(QString title);
private Q_SLOTS:
	void requestStarted(int code);
	void requestFinished(int code, bool error);
	void linkClicked(const QUrl &link);
private:
	QString parseFeed(const QString &feed, int limit);

	QMap<QString, QUrl>    m_urls;
	QMap<QString, int>     m_limits;
	QMap<QString, QHttp*>  m_links;
	QMap<int, QString>     m_reqs;
	QMap<QString, QString> m_feeds;
	QString                m_currentActive;
	QTimer                *m_timer;
};

#endif
