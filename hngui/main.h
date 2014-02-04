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

#ifndef __MAIN_H__
#define __MAIN_H__

#include <QMainWindow>
#include <QProcess>
#include <QPointer>
#include <QPixmap>
#include <boost/scoped_ptr.hpp>
#include <hncgcomm/cgcomm.h>

extern void doLogDebug(const std::string &msg);
inline void logDebug(const QString &msg) { doLogDebug(msg.toStdString()); }
extern QString imgDir();
extern QString confDir();

namespace Ui {
	class MainLayout;
	class SettingsPage;
}
class EngineComm;
class Splash;
class QToolButton;

enum {
	ACBAR_HEIGHT = 22,
	FRAMELBORDER = 3,
	FRAMERBORDER = 3,
	FRAMEBBORDER = 8
};

class MainWindow : public QMainWindow {
	Q_OBJECT
public:
	MainWindow();
	~MainWindow();
	static MainWindow& instance();
	EngineComm* getEngine() const { return m_ecomm; }
Q_SIGNALS:
	void engineConnection(bool status);
protected:
	void closeEvent(QCloseEvent *e);
	void keyPressEvent(QKeyEvent *e);
	void resizeEvent(QResizeEvent *e);
	void mouseMoveEvent(QMouseEvent *e);
	void mousePressEvent(QMouseEvent *e);
	void mouseReleaseEvent(QMouseEvent *e);
	void paintEvent(QPaintEvent *e);
	void moveEvent(QMoveEvent *e);
	bool eventFilter(QObject *obj, QEvent *evt);
private Q_SLOTS:
	void initData();
	void readyRead();
	void coreDied(QProcess::ProcessError error);
	void coreExited(int exitCode, QProcess::ExitStatus exitStatus);
	void splashDestroyed(QObject *obj = 0);
	void toggleSysButtons(bool enable);
	void maxRestore();
	void doShowMinimized();
	void doClose();
public Q_SLOTS:
	// page switching
	void switchToHome();
	void switchToSearch();
	void switchToTransfer();
	void switchToLibrary();
	void switchToSettings();

	void toggleTitleBar();
private:
	friend int main(int argc, char *argv[]);

	// initialization
	void initConfig();
	void initGui();
	void initComm(const QString &url, quint16 port);
	void initCore(const QString &url, quint16 port);

	// returns true if snap was made, false otherwise
	bool snapToBorder(QPoint pos);
	// updates region mask (if no title bar)
	void updateMask();

	QRegion getHeaderMask() const;

	boost::scoped_ptr<Engine::Main> m_engine;
	boost::scoped_ptr<Engine::DownloadList> m_downloadList;
	EngineComm       *m_ecomm;
	QProcess         *m_core;
	Ui::MainLayout   *m_ui;

	// pages (parent widgets)
	QWidget *m_wHomePage, *m_wSearchPage, *m_wTransferPage, 
		*m_wLibraryPage, *m_wSettingsPage;
	// current active page
	QWidget          *m_currentPage;

	Splash *m_splash;
	int            m_corePort;
	QString m_cmdLine; // command-line used for starting core process

	QToolButton *m_toggleButton, *m_minButton, *m_maxButton, *m_closeButton;
	bool m_showTitle;
	QPoint m_dragPosition;

	QPixmap m_kaar, m_kaarOrig, m_leftEar, m_rightEar, m_rb, m_lb, m_bb;
};

#endif
