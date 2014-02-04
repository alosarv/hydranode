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

#ifndef __MYHYDRA_H__
#define __MYHYDRA_H__

#include <QTextBrowser>

namespace Engine {
	class Network;
	class Modules;
}

class MyHydra : public QTextBrowser {
	Q_OBJECT
public:
	MyHydra(QWidget *parent = 0);
	static Engine::Network* getNetworkInfo() { return s_networkInfo; }
	static Engine::Modules* getModulesInfo() { return s_modulesInfo; }
private Q_SLOTS:
	void engineConnection(bool up);
private:
	void onUpdated();
	static Engine::Network *s_networkInfo;
	static Engine::Modules *s_modulesInfo;
};

class StatGraph : public QFrame {
public:
	StatGraph(QWidget *parent = 0);
protected:
	void paintEvent(QPaintEvent *evt);
private:
	friend class MyHydra;
	void onUpdated();
	Engine::Network *s_networkInfo;
	std::list<quint64> m_upValues, m_downValues;
	quint64 m_largestUp, m_largestDown;
};

#endif
