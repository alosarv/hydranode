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

#ifndef __HOMETABS_H__
#define __HOMETABS_H__

#include <QPixmap>
#include <QFrame>
#include <QMap>

namespace Ui {
	class HomeContent;
	class HomeActionBar;
	class PageTemplate;
}

class QVBoxLayout;

class HomePage : public QWidget {
	Q_OBJECT
public:
	HomePage(QWidget *parent = 0);
	QWidget* contentWidget() const;
	void addPage(QString name, QWidget *object);
	void setActive(QString name);
	bool hasPage(const QString &name) { return m_pageMap.contains(name); }
	static HomePage& instance();
protected:
	void paintEvent(QPaintEvent *evt);
	bool eventFilter(QObject *obj, QEvent *evt);
private Q_SLOTS:
	void rssButtonClicked();
	void setNewsTitle(const QString&);
	void changePage();
private:
	Ui::PageTemplate *m_template;
	Ui::HomeActionBar *m_actionBar;
	Ui::HomeContent *m_ui;
	QPixmap m_background;
	QPalette m_palette;
	QWidget *m_overviewPage;
	QPixmap m_actionBack, m_actionBackOrig;
	QMap<QString, QWidget*> m_pageMap;
	QWidget *m_curPage;
	QVBoxLayout *m_contentLayout;
};

class NewsHeader : public QFrame {
public:
	NewsHeader(QWidget *parent = 0);
protected:
	void paintEvent(QPaintEvent *evt);
};
class MyHydraHeader : public QFrame {
public:
	MyHydraHeader(QWidget *parent = 0);
protected:
	void paintEvent(QPaintEvent *evt);
};

#endif
