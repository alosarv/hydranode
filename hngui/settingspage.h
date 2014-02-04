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

#ifndef __SETTINGSPAGE_H__
#define __SETTINGSPAGE_H__

#include <QWidget>
#include <QPixmap>
#include <hncgcomm/cgcomm.h>

namespace Ui {
	class SettingsTabs;
}

class SettingsPage : public QWidget {
	Q_OBJECT
public:
	SettingsPage(QWidget *parent = 0);
	static SettingsPage& instance();
	Engine::Config* getConfig() const { return m_config; }
	QString value(const QString &key) {
		return QString::fromStdString(m_values[key.toStdString()]);
	}
Q_SIGNALS:
	void barsEnabled(bool enabled);
	void backEnabled(bool enabled);
private Q_SLOTS:
	void enableButtons();
	void revertSettings();
	void saveSettings();
	void selectIncoming();
	void selectTemp();
	void selectIpfilter();
	void engineConnection(bool state);

	void updateDownLimitBox();
	void updateUpLimitBox();
	void updateConnLimitBox();
	void updateNewConnLimitBox();
	void updateHopenConnLimitBox();
protected:
	bool eventFilter(QObject *obj, QEvent *evt);
	void paintEvent(QPaintEvent *evt);
private:
	void configValues(const std::map<std::string, std::string> &v);

	Ui::SettingsTabs *m_ui;
	Engine::Config *m_config;
	std::map<std::string, std::string> m_values;
	QPixmap m_actionBack, m_background, m_actionBackOrig, m_backgroundOrig;
};


#endif
