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

#include <QtCore/QProcess>
#include <QtCore/QFile>
#include <QtGui/QApplication>
#include <QtGui/QBitmap>
#include <QtGui/QLabel>
#include <QtGui/QTextCursor>
#include <QtGui/QToolButton>
#include <QtGui/QMenu>
#include <QtGui/QKeyEvent>
#include <QtGui/QScrollBar>
#include <QtGui/QDesktopWidget>
#include <QtGui/QHeaderView>
#include <QtNetwork/QTcpSocket>
#include "launcher.h"
#include "console.h"
#include "searchlist.h"
#include "qhex.h"
#include "ecomm.h"
#include <boost/bind.hpp>
#ifdef _WIN32
	#define _WINSOCKAPI_
	#include <windows.h>
#endif

// utility functions
// -----------------
QString errorString(qint32 errnum) {
#ifdef _WIN32
	LPVOID lpMsgBuf;

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		errnum,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR) &lpMsgBuf,
		0, NULL
	);
	QString msg((LPTSTR)lpMsgBuf);
	LocalFree(lpMsgBuf);

	// get rid of newline(s)
	for (qint32 i = 0; i < msg.size(); ++i) {
		if (msg[i] == '\n' || msg[i] == '\r') {
			msg.remove(i--, 1);
			--i;
		}
	}

	return QString("%1 (error %2)").arg(msg).arg(errnum);
#else
	return QString("(error %1)").arg(errnum);
#endif
}

// Launcher::AutoScroller class
// ----------------------------
Launcher::AutoScroller::AutoScroller(Launcher *parent) : m_parent(parent) {
	m_parent->m_autoScroll = false;
}
Launcher::AutoScroller::~AutoScroller() {
	m_parent->m_autoScroll = true;
	m_parent->scrollToEnd();
}

Launcher::Launcher() : QMainWindow(0, Qt::Window), m_console(new Ui::Console),
m_statEnabled(true) {
// useful stuff - for later
//	QApplication::addLibraryPath(QApplication::applicationDirPath());
//	QApplication::addLibraryPath(
//		QApplication::applicationDirPath() + "/libs"
//	);
//	QApplication::addLibraryPath(
//		QApplication::applicationDirPath() + "/plugins"
//	);

	initGui();
	initSignals();
	initStyle();

	m_console->searchList->hide();
	m_console->downloadList->hide();
	m_console->searchBar->hide();
	resize(750, 480);
	QDesktopWidget d;
	QPoint pos(d.availableGeometry().bottomLeft());
	pos.setY(pos.y() - frameGeometry().height());
	move(pos);
	m_console->outWnd->setWindowOpacity(0.1);
	show();
}

void Launcher::init() {
	m_coreProc.setReadChannel(QProcess::StandardError);

	// check if engine is already running
	logMsg("Checking for running instance of Hydranode Engine...");
	QTcpSocket *s = new QTcpSocket(this);
	s->connectToHost("127.0.0.1", 9990);
	s->waitForConnected(1000);
	if (s->state() == QTcpSocket::ConnectedState) {
		logMsg("Found running Hydranode, attaching to process...");
		m_engineComm = new EngineComm("127.0.0.1", 9990);
		m_config.reset(
			new Engine::Config(
				m_engineComm->getMain(),
				boost::bind(&Launcher::configValues, this, _1)
			)
		);
	} else {
		logMsg("No Hydranode Engine detected, starting one.");

		QStringList args;
		args.push_back("--disable-status");
		args.push_back("-l cgcomm,hnsh,ed2k,http");

// this would allow cwd and exedir differ, but breaks in core right now
//	QString exe(QApplication::applicationDirPath() + "/hydranode");
//	exe = exe.replace('/', QDir::separator());
		m_coreProc.start("./hydranode", args);

		if (!m_coreProc.waitForStarted(1000)) {
			m_console->outWnd->append("Unable to start Hydranode.");
		}
	}
}

Launcher::~Launcher() {
	m_coreProc.kill();
	m_coreProc.waitForFinished(1000);
}


void Launcher::initGui() {
	m_console->setupUi(this);
	m_console->hboxLayout->setSpacing(0);
	m_console->gridLayout->setMargin(0);
	m_console->gridLayout->setSpacing(0);
	m_console->gridLayout1->setMargin(2);
	m_console->gridLayout1->setSpacing(0);
	m_console->vboxLayout->setMargin(8);
	m_console->searchList->init();
	m_console->downloadList->init();
}


void Launcher::initSignals() {
	connect(
		&m_coreProc, SIGNAL(readyReadStandardError()),
		SLOT(processCoreOutput())
	);
	connect(
		&m_coreProc, SIGNAL(readyReadStandardOutput()),
		SLOT(processCoreOutput())
	);
	connect(&m_coreProc, SIGNAL(finished(int)), SLOT(onCoreExit(int)));
	connect(
		m_console->input, SIGNAL(returnPressed()),
		SLOT(processCommand())
	);
	connect(m_console->searchButton, SIGNAL(clicked()), SLOT(newSearch()));
	connect(
		m_console->toggleSearch, SIGNAL(released()),
		SLOT(searchButtonReleased())
	);
	connect(
		m_console->viewIncoming, SIGNAL(clicked()), SLOT(openIncoming())
	);
	connect(
		m_console->searchList,
		SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)),
		SLOT(startDownload(QTreeWidgetItem*, int))
	);
	connect(
		m_console->toggleSearch, SIGNAL(clicked(bool)),
		SLOT(searchButtonClicked(bool))
	);
	connect(
		m_console->toggleDownload, SIGNAL(clicked(bool)),
		SLOT(downloadButtonClicked(bool))
	);
	connect(
		m_console->resultsButton, SIGNAL(clicked()),
		SLOT(resultsButtonClicked())
	);
}

void Launcher::initStyle() {
	// set up colors
	QPalette p1(m_console->outWnd->palette());
	p1.setColor(QPalette::Base, QColor(230, 247, 255));
	p1.setColor(QPalette::AlternateBase, QColor(212, 241, 255));
	m_console->outWnd->setTextColor(Qt::black);
	m_console->outWnd->setPalette(p1);
	m_console->searchList->setPalette(p1);
	m_console->downloadList->setPalette(p1);
	m_console->searchBox->setPalette(p1);

	QPalette p2(m_console->input->palette());
	p2.setColor(QPalette::Base, QColor(230, 255, 247));
	m_console->input->setPalette(p2);

	QPalette p3;
	p3.setColor(QPalette::Background, QColor(230, 255, 247));
	m_console->searchBar->setPalette(p3);

	// set up fonts
	QFont font;
#ifdef _WIN32
	font.setFamily("Terminal");
	font.setPointSize(10);
	font.setWeight(50);
#else
//	font.setFamily("Console");
//	font.setPointSize(16);
#endif
//	m_console->outWnd->setFont(font);
//	m_console->input->setFont(font);

	// default button font looks really ugly there on linux
#ifndef _WIN32
	m_console->toggleSearch->setFont(font);
	m_console->toggleDownload->setFont(font);
	m_console->viewIncoming->setFont(font);
#endif

	// this is needed to properly over-ride the uic-generated html
	m_console->outWnd->setHtml("");
}

void Launcher::closeEvent(QCloseEvent *event) {
	m_coreProc.kill();
	m_coreProc.waitForFinished(1000);
}

void Launcher::keyPressEvent(QKeyEvent *event) {
	if (
		event->key() == Qt::Key_Escape
		&& event->modifiers() != Qt::ControlModifier
		&& event->modifiers() != Qt::AltModifier
		&& event->modifiers() != Qt::ShiftModifier
	) {
		if (
			m_console->input->hasFocus()
			&& !m_console->input->text().isEmpty()
		) {
			m_console->input->clear();
		} else {
			m_console->searchBar->hide();
			m_console->downloadList->hide();
			m_console->searchList->hide();
			m_console->outWnd->show();
			m_console->toggleDownload->setChecked(false);
			m_console->toggleSearch->setChecked(false);
			m_console->searchBox->clear();
			m_console->input->setFocus();
		}
		event->accept();
		return;
	} else if (event->modifiers() & Qt::ControlModifier) {
		if (event->key() == Qt::Key_S) {
			bool vis = m_console->searchBar->isVisible();
			m_console->toggleSearch->setChecked(!vis);
			m_console->searchBar->setVisible(!vis);
			if (vis) {
				m_console->input->setFocus();
			} else {
				m_console->searchBox->setFocus();
			}
			event->accept();
			return;
		} else if (event->key() == Qt::Key_Q) {
			bool vis = m_console->downloadList->isVisible();
			m_console->toggleDownload->setChecked(!vis);
			m_console->downloadList->setVisible(!vis);
			m_console->searchList->setVisible(false);
			m_console->outWnd->setVisible(vis);
			event->accept();
			return;
		} else if (event->key() == Qt::Key_I) {
			m_console->viewIncoming->click();
			event->accept();
			return;
		} else if (event->key() == Qt::Key_R) {
			m_console->resultsButton->click();
			event->accept();
			return;
		}
	}
	event->ignore();
}

void Launcher::processCoreOutput() {
	if (!m_coreProc.canReadLine()) {
		return;
	}
	AutoScroller scroller(this);

	QByteArray data(m_coreProc.readAllStandardError());
	if (data.endsWith("\n")) {
		data.remove(data.size() - 1, 1);
	} else if (data.startsWith("\n")) {
		data.remove(0, 1);
	}

	qDebug(QString(data).toAscii());

	data.replace("&", "&amp;");
	QString style("<span style=\"color: %1; white-space: pre\">");
	data.replace("\33[0;30m", style.arg("black").toAscii());
	data.replace("\33[0;31m", style.arg("red").toAscii());
	data.replace("\33[1;31m", style.arg("red").toAscii());
	data.replace("\33[0;32m", style.arg("green").toAscii());
	data.replace("\33[1;32m", style.arg("green").toAscii());
	data.replace("\33[0;33m", style.arg("magenta").toAscii());
	data.replace("\33[1;33m", style.arg("magenta").toAscii());
	data.replace("\33[0;34m", style.arg("blue").toAscii());
	data.replace("\33[1;34m", style.arg("blue").toAscii());
	data.replace("\33[0;35m", style.arg("magenta").toAscii());
	data.replace("\33[1;35m", style.arg("magenta").toAscii());
	data.replace("\33[0;36m", style.arg("blue").toAscii());
	data.replace("\33[1;36m", style.arg("blue").toAscii());
	data.replace("\33[0;37m", style.arg("white").toAscii());
	data.replace("\33[1;37m", style.arg("white").toAscii());
	data.replace("\33[0;0m", "</span>");
	data.replace("\r", "");

	QStringList out(QString(data).split("\n"));
	for (QStringList::iterator it(out.begin()); it != out.end(); ++it) {
		if ((*it).startsWith("Upload: ")) {
			continue;
		} else if ((*it).startsWith("[Statistics]") && !m_statEnabled) {
			continue;
		} else if ((*it).startsWith("Trace")) {
			continue;
		} else if ((*it).startsWith("000")) {
			continue;
		}
		if ((*it).trimmed().startsWith("Incoming files dir")) {
			QRegExp r("Incoming files directory:  (\\S+)");
			if (r.indexIn(*it) != -1) {
				m_incDir = r.cap(1);
				QDir tmp(m_incDir);
				tmp.makeAbsolute();
				m_incDir = tmp.path().replace(
					'/', QDir::separator()
				);
			}
		}
		m_console->outWnd->append(
			"<span style=\"white-space: pre;\">" + *it + "</span>"
		);
		if ((*it).contains("Core/GUI Communication listening ")) {
			initCgComm(*it);
		}
	}
}

void Launcher::scrollToEnd() {
	QScrollBar *s = m_console->outWnd->verticalScrollBar();
	s->setSliderPosition(s->maximum());
}

void Launcher::initCgComm(const QString &msg) {
	QRegExp reg("Core/GUI Communication listening on port (\\d+)");
	if (reg.indexIn(msg) == -1) {
		logMsg(
			"Internal error: unable to detect "
			"Core/GUI Communication port."
		);
		return;
	}

	int port = reg.cap(1).toLong();
	if (!port) {
		logMsg(
			"Unable to initialize Core/GUI "
			"Communication: invalid port 0"
		);
		return;
	}

	m_engineComm = new EngineComm("127.0.0.1", port);
	connect(
		m_engineComm,
		SIGNAL(connectionEstablished()), SLOT(connectionEstablished())
	);
	m_config.reset(
		new Engine::Config(
			m_engineComm->getMain(),
			boost::bind(&Launcher::configValues, this, _1)
		)
	);
}

int strToBytes(int value, const QString &amount) {
	if (amount.startsWith("bytes")) {
		return value;
	} else if (amount.startsWith("KB")) {
		return value * 1024;
	} else if (amount.startsWith("MB")) {
		return value * 1024 * 1024;
	} else if (amount.startsWith("GB")) {
		return value * 1024 * 1024 * 1024;
	} else {
		return value;
	}
}

void Launcher::onCoreExit(int exitCode) {
	processCoreOutput();
	if (!exitCode) {
		logMsg("Hydranode core has exited cleanly.");
	} else {
		logMsg("Hydranode core exited: " + errorString(exitCode));
	}
}

void Launcher::processCommand() {
	QString cmd = m_console->input->text();
	m_console->input->clear();

	if (cmd.trimmed().isEmpty()) {
		return;
	}
	AutoScroller scroller(this);

	m_console->outWnd->append("> " + cmd);
	QScrollBar *s = m_console->outWnd->verticalScrollBar();
	s->setSliderPosition(s->maximum());

	QStringList tok(cmd.split(" "));

	// clear out empty entries (e.g. empty after trimming)
	for (qint32 i = 0; i < tok.size(); ++i) {
		if (!tok[i].trimmed().size()) {
			tok.removeAt(i--);
		}
	}

	if (tok.first() == "shutdown") {
		m_coreProc.kill();
	} else if (tok.first().startsWith("opa")) {
		if (tok.size() == 1 || !tok[1].trimmed().size()) {
			printHelp("opacity");
			return;
		}
		setWindowOpacity(tok[1].toFloat());
		logMsg(
			QString("Setting opacity to %1 ... ok, set to %1")
			.arg(tok[1].toFloat())
			.arg(windowOpacity())
		);
	} else if (tok.first().startsWith("stat")) {
		if (tok.size() > 1) {
			if (tok[1] == "on") {
				m_statEnabled = true;
			} else if (tok[1] == "off") {
				m_statEnabled = false;
			} else {
				printHelp("stat");
			}
		} else {
			printHelp("stat");
		}
	} else if (tok.first() == "config") {
		if (!m_config || !m_engineComm) {
			logMsg(
				"Core/GUI communication "
				"subsystem is not available currently."
			);
		} else if (
			tok.size() == 1
			|| (tok.size() == 2 && tok[1] == "list")
			|| (tok.size() == 2 && !tok[1].trimmed().size())
		) {
			m_config->getList();
		} else if (tok.size() == 2 && tok[1] == "monitor") {
			m_config->monitor();
		} else if (tok.size() == 3 && tok[1] == "get") {
			m_config->getValue(tok[2].toStdString());
		} else if (tok.size() == 4 && tok[1] == "set") {
			std::string key = tok[2].toStdString();
			std::string val = tok[3].toStdString();
			m_config->setValue(key, val);
		} else {
			printHelp("config");
		}
	} else if (tok.first().startsWith("sea")) {
		if (!m_engineComm) {
			logMsg(
				"Core/GUI communication "
				"subsystem is not available currently."
			);
		} else {
			startSearch(tok);
		}
	} else if (tok.first() == "help") {
		if (tok.size() > 1) {
			printHelp(tok[1]);
		} else {
			printHelp();
		}
	} else {
		printHelp();
	}
}

void Launcher::startSearch(const QStringList &tok) {
	QStringList::const_iterator it(++tok.begin());
	QString keyWords;
	QString type;
	quint32 minSize = 0;
	quint32 maxSize = 0;
	while (it != tok.end()) {
		if ((*it).startsWith("--")) {
			QStringList::const_iterator tmp(it);
			if (++tmp == tok.end()) {
				logMsg("--" + *it + ": need value");
				printHelp("search");
				return;
			}
		}
		if (*it == "--type") {
			type = *++it;
			++it;
		} else if (*it == "--minsize") {
			minSize = (*++it).toLong();
			++it;
		} else if (*it == "--maxsize") {
			maxSize = (*++it).toLong();
			++it;
		} else {
			keyWords += " " + *it++;
		}
	}
	if (keyWords.isEmpty()) {
		logMsg("Unable to search without keywords.");
		printHelp("search");
		return;
	}
	m_curSearch.reset(
		new Engine::Search(
			m_engineComm->getMain(),
			boost::bind(&Launcher::searchResults, this, _1)
		)
	);
	m_curSearch->addKeywords(keyWords.toStdString());
	if (minSize) {
		m_curSearch->setMinSize(minSize);
	}
	if (maxSize) {
		m_curSearch->setMaxSize(maxSize);
	}
	if (type.startsWith("aud")) {
		m_curSearch->setType(Engine::FT_AUDIO);
	} else if (type.startsWith("vid")) {
		m_curSearch->setType(Engine::FT_VIDEO);
	} else if (type.startsWith("arc")) {
		m_curSearch->setType(Engine::FT_ARCHIVE);
	} else if (type.startsWith("ima") || type.startsWith("img")) {
		m_curSearch->setType(Engine::FT_IMAGE);
	} else if (type.startsWith("cd") || type.startsWith("dvd")) {
		m_curSearch->setType(Engine::FT_CDIMAGE);
	} else if (type.startsWith("doc") || type.startsWith("text")) {
		m_curSearch->setType(Engine::FT_DOCUMENT);
	} else if (type.startsWith("pro") || type.startsWith("app")) {
		m_curSearch->setType(Engine::FT_PROGRAM);
	} else if (type.size()) {
		logMsg("Unknown file type.");
		printHelp("search");
		return;
	}
	m_curSearch->run();
	m_console->searchList->clear();
}

void Launcher::printHelp() {
	AutoScroller scroller(this);
	logMsg("<u>Available commands:</u>");
	logMsg(" opacity <val>   Sets console window transparency.");
	logMsg(" stat <on/off>   Enable/disable statistics printing.");
	logMsg(" shutdown        Shut down Hydranode engine.");
	logMsg(" config          Access information about configuration.");
	logMsg(" search <args>   Search P2P networks.");
	logMsg(" ");
	logMsg("<u>Keyboard shortcuts:</u>");
	logMsg(" Ctrl + S        Show/hide Search bar");
	logMsg(" Ctrl + Q        Show/hide Download view");
	logMsg(" Ctrl + R        Display search results");
	logMsg(" Esc             Display Console view and close Search bar");
	logMsg("");
	logMsg("See 'help <command>' for more information.");
}

void Launcher::printHelp(const QString &cmd) {
	AutoScroller scroller(this);
	if (cmd == "config") {
		logMsg("'config' command options:");
		logMsg(" list            Lists current configuration values");
		logMsg(" monitor         Enables updates on value changes");
		logMsg(" get <key>       Retrieves the current value of key");
		logMsg(" set <key> <val> Sets the value of <key> to <val>");
	} else if (cmd == "stat") {
		logMsg("'stat' command options:");
		logMsg(" on              Enable [Statistics] line printing");
		logMsg(" off             Disable [Statistics] line printing");
	} else if (cmd.startsWith("sea")) {
		logMsg("'search' command options:");
		logMsg(" --minsize bytes  Minimum file size");
		logMsg(" --maxsize bytes  Maximum file size");
		logMsg(" --type           Specify file type:");
		logMsg("       audio         Audio files      (mp3, ogg, wma)");
		logMsg("       video         Video files      (avi, mpg, wmv)");
		logMsg("       archive       Archive files    (rar, zip, bz2)");
		logMsg("       document      Text documents   (doc, txt, nfo)");
		logMsg("       cd / dvd      CD/DVD images    (iso, bin, cue)");
		logMsg("       img / pic     Images, Pictures (jpg, gif, png)");
		logMsg("       pro / app     Applications     (exe, com, bin)");
		logMsg("  All other arguments are treated as search keywords.");
	} else if (cmd.startsWith("opa")) {
		logMsg("'opacity' command options:");
		logMsg(" 0            Completely transparent, invisible");
		logMsg(" 0.1 .. 0.99  Partially transparent");
		logMsg(" 1            Completely visible");
		logMsg(
			"Note that the window resizing and updating performance"
		);
		logMsg(
			"degrades when the window is partially transparent."
		);
	} else {
		logMsg("Additional help is not available for this command.");
	}
}

void Launcher::newSearch() {
	QString logPre("Unable to perform search: ");
//	if (m_coreProc.state() != QProcess::Running) {
//		logMsg(logPre + "Hydranode is not running.");
//		return;
//	} else
	if (!m_engineComm) {
		logMsg(
			logPre + "Core/GUI Communication Subsystem not "
			"available - cgcomm plugin not loaded?"
		);
		return;
	} else if (m_console->searchBox->text().isEmpty()) {
		logMsg(logPre + "No search terms specified.");
		return;
	}

	m_curSearch.reset(
		new Engine::Search(
			m_engineComm->getMain(),
			boost::bind(&Launcher::searchResults, this, _1)
		)
	);
	m_curSearch->addKeywords(m_console->searchBox->text().toStdString());
	m_curSearch->run();
	m_console->searchBox->clear();
	m_console->searchList->clear();
}

void Launcher::searchResults(const std::vector<Engine::SearchResultPtr> &res) {
	logMsg(QString("Received %1 results from engine.").arg(res.size()));

	foreach(Engine::SearchResultPtr r, res) {
		new SearchListItem(m_console->searchList, r);
	}
	m_console->searchList->sortItems(2, Qt::DescendingOrder);
	m_console->resultsButton->click();
	return;
}

void Launcher::configValues(const std::vector<Engine::ConfigValue> &val) {
	logMsg(QString("Received %1 configuration values.").arg(val.size()));
	AutoScroller scroller(this);

	foreach(Engine::ConfigValue v, val) {
		logMsg(
			QString("Configuration update: %1 = %2")
			.arg(QString::fromStdString(v.key()))
			.arg(QString::fromStdString(v.value()))
		);
	}
}

// if we weren't currently on downloadlist page, this means we were on search
// results page, and thus when releasing search button, switch to console view
void Launcher::searchButtonReleased() {
	if (!m_console->downloadList->isVisible()) {
		m_console->outWnd->show();
	}
	m_console->input->setFocus();
}

void Launcher::searchButtonClicked(bool checked) {
	if (checked) {
		qDebug("Setting focus to searchbox");
		m_console->searchBox->setFocus();
	} else {
		qDebug("Setting focus to input box");
		m_console->input->setFocus();
	}
}

void Launcher::downloadButtonClicked(bool checked) {
	setUpdatesEnabled(false);
	if (checked) {
		m_console->searchList->hide();
		m_console->outWnd->hide();
		m_console->downloadList->show();
	} else {
		m_console->downloadList->hide();
		m_console->searchList->hide();
		m_console->outWnd->show();
	}
	setUpdatesEnabled(true);
}

void Launcher::resultsButtonClicked() {
	setUpdatesEnabled(false);
	m_console->outWnd->hide();
	m_console->downloadList->hide();
	m_console->toggleDownload->setChecked(false);
	m_console->searchList->show();
	setUpdatesEnabled(true);
}

void Launcher::openIncoming() {
#ifdef _WIN32
	QProcess::startDetached("explorer.exe", QStringList() << m_incDir);
#else
	QProcess::startDetached("konqueror", QStringList() << m_incDir);
#endif
}


void Launcher::startDownload(QTreeWidgetItem *item, int) {
	SearchListItem *i = dynamic_cast<SearchListItem*>(item);
	Q_ASSERT(i);
	if (i) {
		i->download();
	}
}

void Launcher::connectionEstablished() {
	Engine::DownloadList *dList = new Engine::DownloadList(
		m_engineComm->getMain(),
		boost::bind(
			&DownloadList::updateList,
			m_console->downloadList, _1, _2
		)
	);
	m_console->downloadList->setSource(dList);
}
