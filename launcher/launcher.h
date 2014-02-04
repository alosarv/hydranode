/*
 *  Copyright (C) 2005-2006 Alo Sarv <madcat_@users.sourceforge.net>
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

#ifndef __LAUNCHER_H__
#define __LAUNCHER_H__

#include <QMainWindow>
#include <QtCore/QProcess>
#include <QtCore/QDir>
#include <QtGui/QLabel>
#include <boost/scoped_ptr.hpp>
#include <hncgcomm/cgcomm.h>

class EngineComm;
class QTreeWidgetItem;
namespace Ui {
	class Console;
}

/**
 * Launcher provides a number of services, include:
 * - Simple command-line interface
 * - Hydranode process(es) management, external tools launching
 * - A small number of GUI controls for most-used operations
 */
class Launcher : public QMainWindow {
	Q_OBJECT
public:
	Launcher();
	~Launcher();

	void init();
	//! Log message to console
	friend void logMsg(const QString &msg);

	Ui::Console *m_console;         //!< console window controls
protected:
	virtual void closeEvent(QCloseEvent *event);
	virtual void keyPressEvent(QKeyEvent *event);
private:
	void initGui();                  //!< initializes user interface
	void initSignals();              //!< initializes signals
	void initStyle();                //!< initializes style-related things

	void scrollToEnd();             //!< Scroll to end of text
	QProcess     m_coreProc;        //!< hydranode core process
	bool         m_statEnabled;     //!< whether to print [statistics] lines
	bool         m_autoScroll;      //!< Whether to scroll on log messages
	EngineComm  *m_engineComm;      //!< handles TCP socket and Engine::Main
	QString      m_incDir;          //!< Incoming files directory


	// Engine communication object
	boost::scoped_ptr<Engine::Search> m_curSearch;
	boost::scoped_ptr<Engine::Config> m_config;

	// initializes core/gui communication, detecting the core port from
	// the log message
	void initCgComm(const QString &message);

	// start search from commandline
	// @param tok contains the entire commandline, tokenized
	void startSearch(const QStringList &tok);

	// handler for Engine search results event
	void searchResults(const std::vector<Engine::SearchResultPtr> &results);

	// signal handler for config values update
	void configValues(const std::vector<Engine::ConfigValue> &val);

	// prints command listing and information
	void printHelp();

	// prints help on a specific command
	void printHelp(const QString &command);

	class AutoScroller {
	public:
		AutoScroller(Launcher *parent);
		~AutoScroller();
	private:
		Launcher *m_parent;
	};
	friend class AutoScroller;
private slots:
	// HydraEngine process handling
	void processCoreOutput();       //!< handles and outputs core output
	void onCoreExit(int exitCode);  //!< handles core exits
	void newSearch();               //!< starts new search
	void openIncoming();            //!< opens incoming files folder
	void connectionEstablished();   //!< CGComm established

	// User interaction
	void processCommand();          //!< processes user input in console
	void searchButtonReleased();
	void searchButtonClicked(bool);
	void downloadButtonClicked(bool);
	void resultsButtonClicked();
	void startDownload(QTreeWidgetItem *item, int column);

};

/**
 * HydraLogo is a transparent, standalone window which contains Hydranode
 * logo in it; by default, it's palette is set to match console window
 * background.
 *
 * Currently, X11 doesn't support true transparency, so this works only on
 * Win2K+ and Darwin ports.
 */
class HydraLogo : public QLabel {
public:
	HydraLogo(QWidget *parent);
};

#endif
