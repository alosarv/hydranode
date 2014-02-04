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

#include "main.h"
#include "transfertabs.h"
#include "downloadlist.h"
#include "mainlayout.h"
#include "hometabs.h"
#include "searchtabs.h"
#include "settingspage.h"
#include "librarytabs.h"
#include "ecomm.h"
#include <QApplication>
#include <QTextEdit>
#include <QMessageBox>
#include <QClipboard>
#include <QKeyEvent>
#include <QSplashScreen>
#include <QDir>
#include <QPainter>
#include <QBitmap>
#include <QDesktopWidget>
#include <QMenu>
#include <QSettings>
#include <boost/lambda/construct.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/bind.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <fstream>
#include <errno.h>
#include <boost/date_time/posix_time/posix_time.hpp>

boost::lambda::placeholder1_type __1;
MainWindow *s_mainWindow;
void doLogDebug(const std::string &msg);

QString s_confDir, s_imgDir;
QString confDir() { return s_confDir; }
QString imgDir()  { return s_imgDir;  }

class Splash : public QSplashScreen {
public:
	Splash(const QPixmap &img);
	void showMessage(
		const QString &message, int alignment = Qt::AlignLeft, 
		const QColor &color = Qt::black
	);
protected:
	void drawContents(QPainter *p);
private:
	QString m_message;
};

Splash::Splash(const QPixmap &img) : QSplashScreen(img) {
	setMask(
		QRegion(
			QPixmap::fromImage(
				pixmap().toImage().createAlphaMask(
					Qt::AutoColor | Qt::DiffuseDither 
					| Qt::DiffuseAlphaDither
				), Qt::AutoColor | Qt::DiffuseDither 
				| Qt::DiffuseAlphaDither
			)
		)
	);
}

void Splash::showMessage(const QString &message, int al, const QColor &c) {
	QSplashScreen::showMessage(message, al, c);
	m_message = message;
	repaint();
}

void Splash::drawContents(QPainter *p) {
	p->setBackground(QColor(0, 0, 0, 0));
	p->setRenderHint(QPainter::Antialiasing);
	p->setRenderHint(QPainter::SmoothPixmapTransform);
	p->drawPixmap(0, 0, pixmap());
	p->drawText(4, height() - 46, m_message);
}

MainWindow::MainWindow() : m_ecomm(), m_core(), m_ui(), m_corePort(9990),
m_showTitle(true) {
	m_splash = new Splash(QPixmap(imgDir() + "splash.png"));
	connect(
		m_splash, SIGNAL(destroyed(QObject*)),
		SLOT(splashDestroyed(QObject*))
	);
	m_splash->showMessage(
		"Checking for running instance of Hydranode...",
		Qt::AlignBottom
	);
	m_splash->show();
	setWindowTitle("Hydranode - v0.3");
#if 0
	m_toggleButton = new QToolButton(this);
	m_toggleButton->setIcon(QIcon(":/icons/progressicon"));
	m_toggleButton->resize(24, 18);
	m_toggleButton->move(5, 15);
	m_toggleButton->setAutoRaise(true);
	m_closeButton = new QToolButton(this);
	m_closeButton->setIcon(QIcon(":/icons/progressicon"));
	m_closeButton->resize(24, 18);
	m_closeButton->setAutoRaise(true);
	m_minButton = new QToolButton(this);
	m_minButton->setIcon(QIcon(":/icons/progressicon"));
	m_minButton->resize(24, 18);
	m_minButton->setAutoRaise(true);
	m_maxButton = new QToolButton(this);
	m_maxButton->setIcon(QIcon(":/icons/progressicon"));
	m_maxButton->resize(24, 18);
	m_maxButton->setAutoRaise(true);
#endif
//	setWindowFlags(Qt::FramelessWindowHint);
//	setWindowFlags(Qt::Desktop);
	m_lb = QPixmap(imgDir() + "framelborder.png");
	m_rb = QPixmap(imgDir() + "framerborder.png");
	m_bb = QPixmap(imgDir() + "framebborder.png");
	m_kaar     = QPixmap(imgDir() + "kaar.png");
	m_kaarOrig = QPixmap(imgDir() + "kaar.png");
	m_leftEar  = QPixmap(imgDir() + "leftear.png");
	m_rightEar = QPixmap(imgDir() + "rightear.png");
}

MainWindow& MainWindow::instance() {
	return *s_mainWindow;
}

void MainWindow::splashDestroyed(QObject *obj) {
	if (obj == m_splash) {
		m_splash = 0;
	}
}

void MainWindow::initConfig() {
	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	if (!conf.contains("EnableBack")) {
		conf.setValue("EnableBack", true);
	}
	if (!conf.contains("EnablePerc")) {
		conf.setValue("EnablePerc", true);
	}
	if (!conf.contains("EnableBars")) {
		conf.setValue("EnableBars", true);
	}
	if (!conf.contains("ShowTitle")) {
		conf.setValue("ShowTitle", true);
	}
}

void MainWindow::initGui() {
	initConfig();

	m_ui = new Ui::MainLayout;
	QWidget *mainWidget = new QWidget(this);
	m_ui->setupUi(mainWidget);
	QGridLayout *contentLayout = new QGridLayout(m_ui->contentFrame);
	contentLayout->setMargin(0);
	contentLayout->setSpacing(0);
	m_ui->contentFrame->setLayout(contentLayout);
	setCentralWidget(mainWidget);

	m_wHomePage = new HomePage(m_ui->contentFrame);
	m_wSearchPage = new SearchPage(m_ui->contentFrame);
	m_wTransferPage = new TransferPage(m_ui->contentFrame);
	m_wLibraryPage = new LibraryPage(m_ui->contentFrame);
	m_wSettingsPage = new SettingsPage(m_ui->contentFrame);

	m_ui->contentFrame->layout()->addWidget(m_wSearchPage);
	m_wSearchPage->hide();
	m_ui->contentFrame->layout()->addWidget(m_wTransferPage);
	m_wTransferPage->hide();
	m_ui->contentFrame->layout()->addWidget(m_wLibraryPage);
	m_wLibraryPage->hide();
	m_ui->contentFrame->layout()->addWidget(m_wSettingsPage);
	m_wSettingsPage->hide();
	m_ui->contentFrame->layout()->addWidget(m_wHomePage);
	m_currentPage = m_wHomePage;

	setWindowTitle("Hydranode");
	setWindowIcon(QIcon(":/hydranode.png"));

	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
#ifdef Q_WS_X11
	QSize sz(690, 540);
#else
	QSize sz(630, 460);
#endif
	if (conf.value("Maximized").toBool()) {
		showMaximized();
	} else {
		if (conf.contains("WindowWidth")) {
			sz.setWidth(conf.value("WindowWidth").toInt());
		}
		if (conf.contains("WindowHeight")) {
			sz.setHeight(conf.value("WindowHeight").toInt());
		}
		resize(sz);
		if (conf.contains("WindowPos")) {
			move(conf.value("WindowPos").toPoint());
		}
	}

	if (!conf.value("ShowTitle").toBool()) {
		toggleTitleBar();
	}
	if (!conf.value("EnableBars").toBool()) {
		toggleSysButtons(false);
	}

	connect(
		m_ui->toolButtonHome, SIGNAL(pressed()), SLOT(switchToHome())
	);
	connect(
		m_ui->toolButtonSearch, SIGNAL(pressed()),
		SLOT(switchToSearch())
	);
	connect(
		m_ui->toolButtonTransfer, SIGNAL(pressed()),
		SLOT(switchToTransfer())
	);
	connect(
		m_ui->toolButtonLibrary, SIGNAL(pressed()),
		SLOT(switchToLibrary())
	);
	connect(
		m_ui->toolButtonSettings, SIGNAL(pressed()),
		SLOT(switchToSettings())
	);

	connect(m_ui->right1, SIGNAL(clicked()), SLOT(doClose()));
	connect(m_ui->right2, SIGNAL(clicked()), SLOT(maxRestore()));
	connect(m_ui->right3, SIGNAL(clicked()), SLOT(doShowMinimized()));
	connect(m_ui->left1, SIGNAL(clicked()), SLOT(toggleTitleBar()));
//	connect(m_ui->left1, SIGNAL(clicked()), SLOT(toggleTitleBar()));
//	connect(m_ui->left1, SIGNAL(clicked()), SLOT(toggleTitleBar()));

	connect(
		m_wSettingsPage, SIGNAL(barsEnabled(bool)),
		SLOT(toggleSysButtons(bool))
	);

	m_ui->left1->installEventFilter(this);
	m_ui->left2->installEventFilter(this);
	m_ui->left3->installEventFilter(this);
	m_ui->right1->installEventFilter(this);
	m_ui->right2->installEventFilter(this);
	m_ui->right3->installEventFilter(this);

	// needed to properly update mask
	if (!m_showTitle) {
		paintEvent(0);
	}

	setMouseTracking(true);
}

void MainWindow::toggleTitleBar() {
	if (isMaximized()) {
		return;
	}
#ifdef Q_OS_WIN32
	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	if (!conf.value("EnableBars").toBool()) {
		m_showTitle = true;
	} else {
		m_showTitle = !m_showTitle;
	}

	if (m_showTitle) {
		clearMask();
	} else {
		updateMask();
	}
	update();
#endif
}

void MainWindow::switchToHome() {
	setUpdatesEnabled(false);
	m_currentPage->hide();
	m_currentPage = m_wHomePage;
	m_currentPage->show();
	m_ui->toolButtonHome->setChecked(true);
	setUpdatesEnabled(true);
}

void MainWindow::switchToSearch() {
	setUpdatesEnabled(false);
	m_currentPage->hide();
	m_currentPage = m_wSearchPage;
	m_currentPage->show();
	m_ui->toolButtonSearch->setChecked(true);
	setUpdatesEnabled(true);
}

void MainWindow::switchToTransfer() {
	setUpdatesEnabled(false);
	m_currentPage->hide();
	m_currentPage = m_wTransferPage;
	m_currentPage->show();
	m_ui->toolButtonTransfer->setChecked(true);
	setUpdatesEnabled(true);
}

void MainWindow::switchToLibrary() {
	setUpdatesEnabled(false);
	m_currentPage->hide();
	m_currentPage = m_wLibraryPage;
	m_currentPage->show();
	m_ui->toolButtonLibrary->setChecked(true);
	setUpdatesEnabled(true);
}

void MainWindow::switchToSettings() {
	setUpdatesEnabled(false);
	m_currentPage->hide();
	m_currentPage = m_wSettingsPage;
	m_currentPage->show();
	m_ui->toolButtonSettings->setChecked(true);
	setUpdatesEnabled(true);
}

MainWindow::~MainWindow() {
	delete m_ecomm;
}

void MainWindow::initData() {
	qDebug("Engine connection up and running.");
	engineConnection(true);
	if (m_splash) {
		m_splash->finish(this);
	}
	show();
	if (m_splash) {
		m_splash->deleteLater();
	}
}

void MainWindow::initCore(const QString &url, quint16 port) {
	if (m_splash) {
		m_splash->showMessage(
			"Checking for running instance of Hydranode...",
			Qt::AlignBottom
		);
	}
	QTcpSocket s(this);
	s.connectToHost(url, port);
	s.waitForConnected(1000);
	if (s.state() == QTcpSocket::ConnectedState) {
		initComm(url, port);
		return;
	}

	// no running core - start one as sub-process
	if (m_splash) {
		m_splash->showMessage(
			"Starting core process...", Qt::AlignBottom
		);
	}
	m_core = new QProcess(this);
	m_core->setReadChannelMode(QProcess::MergedChannels);

	connect(m_core, SIGNAL(readyRead()), SLOT(readyRead()));
	connect(
		m_core, SIGNAL(error(QProcess::ProcessError)),
		SLOT(coreDied(QProcess::ProcessError))
	);
	connect(
		m_core, SIGNAL(finished(int, QProcess::ExitStatus)),
		SLOT(coreExited(int, QProcess::ExitStatus))
	);

	QStringList args;
	args << "--disable-status" << "-l cgcomm,hnsh,ed2k,bt";
	args << "--disable-colors=true";
#ifdef Q_OS_WIN32
	args << "--transform-colors=false";
	args << "--module-dir=engine\\plugins";
#else
	args << "--module-dir=engine/plugins";
#endif
	m_cmdLine = m_core->workingDirectory() + "/hydranode-core ";
	m_cmdLine += args.join(" ");
#ifdef Q_OS_WIN32
	m_core->start("hydranode-core", args);
#else
	m_core->start("./hydranode-core", args);
#endif

	if (m_core->state() == QProcess::Starting) {
		m_core->waitForStarted(1000);
	}
	if (m_core->state() == QProcess::Running) {
		if (m_splash) {
			m_splash->showMessage(
				"Core process started.", Qt::AlignBottom
			);
		}
	} else {
		throw std::runtime_error(
			"Unable to start core process."
		);
	}
}

void MainWindow::initComm(const QString &url, quint16 port) {
	if (m_splash) {
		m_splash->showMessage(
			"Initializing core/gui communication...",
			Qt::AlignBottom
		);
	}
	m_ecomm = new EngineComm;
	connect(m_ecomm, SIGNAL(connectionEstablished()), SLOT(initData()));
	m_ecomm->connectToHost(url, port);
	m_engine.reset(m_ecomm->getMain());
}

void MainWindow::readyRead() {
	char buf[1024];
	qint64 lineLen = -1;
	setUpdatesEnabled(false);
	while  ((lineLen = m_core->readLine(buf, sizeof(buf))) != -1) {
		QString tmp(QString::fromAscii(buf, lineLen));
		tmp = tmp.trimmed();
		if (tmp.left(2) == "- ") {
			tmp.remove(0, 2);
		}
		if (tmp.left(2) == "* ") {
			tmp.remove(0, 2);
		}
		if (tmp.contains("Core/GUI Communication listening on port ")) {
			m_corePort = tmp.right(4).trimmed().toInt();
		}
		if (tmp.contains("up and running")) {
			initComm("127.0.0.1", m_corePort);
		} else if (m_splash) {
			m_splash->showMessage(tmp, Qt::AlignBottom);
		}
		doLogDebug(tmp.toStdString());
	}
	setUpdatesEnabled(true);
}

void MainWindow::coreDied(QProcess::ProcessError err) {
	qDebug("Core process died.");
	QString errorMsg;
	if (err == QProcess::FailedToStart) {
		errorMsg += "Core process failed to start. Command line was:\n";
		errorMsg += m_cmdLine;
	} else if (err == QProcess::Crashed) {
		errorMsg += "Core process crashed with exitcode ";
		errorMsg += QString::number(m_core->exitCode());
		errorMsg += " (errno=" + QString::number(errno) + ")";
		errorMsg += "\nCommand line was:\n" + m_cmdLine;
		if (m_core->bytesAvailable()) {
			errorMsg += "\nCore output follows:\n\n";
			errorMsg += m_core->readAll();
		}
	} else if (err == QProcess::Timedout) {
		errorMsg += "Timeout while attempting to start core process.";
	} else if (err == QProcess::WriteError) {
		errorMsg += "Write error while starting core process ";
		errorMsg += "- check file permissions?";
	} else if (err == QProcess::ReadError) {
		errorMsg += "Read error while starting core process ";
		errorMsg += "- check file permissions?";
	} else if (err == QProcess::UnknownError) {
		errorMsg += "Unknown error while starting core process.";
	}
	QMessageBox::critical(
		0, "Hydranode - Fatal Error", errorMsg, QMessageBox::Ok,
		QMessageBox::NoButton
	);
	if (m_splash) { delete m_splash; }
	QApplication::exit(1);
}

void MainWindow::coreExited(int exitCode, QProcess::ExitStatus exitStatus) {
	if (exitStatus != QProcess::NormalExit) {
		QMessageBox::critical(
			this, "Hydranode - Error",
			"Core crashed during shutdown sequence."
		);
	}
	if (m_splash) {
		delete m_splash;
	}
	delete m_core;
	m_core = 0;
	QApplication::exit(0);
}

void MainWindow::closeEvent(QCloseEvent *e) {
	if (m_core && m_core->state() == QProcess::Running) {
//		if (!m_splash) {
//			hide();
//			m_splash = new QSplashScreen(
//				QPixmap(imgDir() + "splash.png")
//			);
//			connect(
//				m_splash, SIGNAL(destroyed(QObject*)),
//				SLOT(splashDestroyed(QObject*))
//			);
//			m_splash->show();
//		}
		m_ecomm->setTryReconnect(false);
		m_engine->shutdownEngine();
		e->ignore();
	}
	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	if (isMaximized()) {
		conf.setValue("Maximized", true);
	} else {
		conf.setValue("WindowPos", frameGeometry().topLeft());
		conf.setValue("WindowWidth", width());
		conf.setValue("WindowHeight", height());
		conf.setValue("Maximized", false);
	}
	conf.setValue("ShowTitle", m_showTitle);
}

void MainWindow::keyPressEvent(QKeyEvent *evt) {
	int k = evt->key();
	Qt::KeyboardModifiers m = evt->modifiers();
	if (
		(k == Qt::Key_Insert && m == Qt::ShiftModifier) ||
		(k == Qt::Key_V && m == Qt::ControlModifier)
	) {
		QClipboard *c = QApplication::clipboard();
		if (c->text().size() && c->text().size() < 255) {
			DownloadList::downloadLink(c->text());
			evt->accept();
			return;
		}
	}
	evt->ignore();
}

QRegion MainWindow::getHeaderMask() const {
//		QRegion reg(
//			QPixmap::fromImage(
//				m_kaar.toImage().createAlphaMask(
//					Qt::AutoColor | Qt::DiffuseDither 
//					| Qt::DiffuseAlphaDither
//				), Qt::AutoColor | Qt::DiffuseDither 
//				| Qt::DiffuseAlphaDither
//			)
//		);
	return QRegion(
		QPixmap::fromImage(
			m_kaar.toImage().createAlphaMask(
				Qt::AutoColor | Qt::DiffuseDither 
				| Qt::DiffuseAlphaDither
			), Qt::AutoColor | Qt::DiffuseDither 
			| Qt::DiffuseAlphaDither
		)
	);
}

void MainWindow::updateMask() {
	if (!m_kaar.isNull()) {
		QRegion reg = getHeaderMask();
		QRegion frame(
			0, m_kaar.height(), width(), height() - m_kaar.height()
		);
		setMask(reg.unite(frame));
	}
}

// null parameter here is done on startup to force first header scaling
void MainWindow::paintEvent(QPaintEvent *e) {
	if (e) {
		QMainWindow::paintEvent(e);
	}

	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	if (!conf.value("EnableBars").toBool()) {
		return;
	}

	if (m_kaar.isNull()) {
		return;
	} else if (!e || m_kaar.width() != geometry().width()) {
		// generate the header image
		m_kaar = m_kaarOrig.scaled(
			geometry().width(), m_kaar.height(),
			Qt::IgnoreAspectRatio, Qt::SmoothTransformation
		);

		QPainter p2(&m_kaar);
		// when titlebar is disabled, the ears backgrounds are partially
		// transparent, however that creates 'dots' when running on
		// black background, since there's no support for true alpha
		// blending. Hence, we must draw the background OUTSIDE the
		// kaar with white color.
//		p2.setBrush(Qt::white);
//		p2.drawRects(QRegion(0, 0, width(), m_kaar.height()).subtract(mask()).rects());

		p2.drawPixmap(0, 7, m_leftEar);
		p2.drawPixmap(
			m_kaar.width() - m_rightEar.width(), 7, m_rightEar
		);
	}

	QPainter p(this);
	p.setBackground(QColor(0, 0, 0, 0));
	p.setRenderHint(QPainter::Antialiasing);
	p.setRenderHint(QPainter::SmoothPixmapTransform);
	if (m_showTitle) {
		QPixmap back(imgDir() + "headerback.png");
		if (!back.isNull()) {
			int pos = 0;
			while (pos < geometry().width()) {
				p.drawPixmap(pos, 0, back);
				pos += back.width();
			}
		} else {
			p.setBrush(QColor(95, 110, 132));
			p.drawRect(0, 0, m_kaar.width(), m_kaar.height());
		}
	}
	p.drawPixmap(0, 0, m_kaar);

	// frame borders
	if (m_lb.isNull() || m_rb.isNull() || m_bb.isNull()) {
		return;
	}
	if (m_lb.height() != height() || m_lb.width() != 3) {
		m_lb = m_lb.scaled(3, height());
	}
	if (m_rb.height() != height() || m_rb.width() != 3) {
		m_rb = m_rb.scaled(3, height());
	}
	if (m_bb.width() != width() + 2 || m_bb.height() != 8) {
		m_bb = m_bb.scaled(width() + 3, 8);
	}
	p.drawPixmap(0, m_kaar.height() + ACBAR_HEIGHT, m_lb);
	p.drawPixmap(width() - 3, m_kaar.height() + ACBAR_HEIGHT, m_rb); 
	p.drawPixmap(0, height() - 8, m_bb);
}

void MainWindow::toggleSysButtons(bool enable) {
	m_ui->left1->setHidden(!enable);
	m_ui->left2->setHidden(!enable);
	m_ui->left3->setHidden(!enable);
	m_ui->right1->setHidden(!enable);
	m_ui->right2->setHidden(!enable);
	m_ui->right3->setHidden(!enable);
}

void MainWindow::maxRestore() {
	if (m_showTitle) {
		return;
	}

	if (isMaximized()) {
		showNormal();
	} else {
		toggleTitleBar();
		showMaximized();
	}
}

void MainWindow::doClose() {
	if (!m_showTitle) {
		close();
	}
}

void MainWindow::doShowMinimized() {
	if (!m_showTitle) {
		showMinimized();
	}
}

void MainWindow::resizeEvent(QResizeEvent *evt) {
//	int posX = width() - 5 - m_closeButton->width();
//	int posY = 15;
//	m_closeButton->move(posX, posY);
//	posX -= m_closeButton->width() + 3;
//	m_maxButton->move(posX, posY);
//	posX -= m_maxButton->width() + 3;
//	m_minButton->move(posX, posY);
	QMainWindow::resizeEvent(evt);
	if (!m_showTitle) {
		updateMask();
	}
}

bool MainWindow::eventFilter(QObject *obj, QEvent *evt) {
	if (evt->type() != QEvent::Paint) {
		return false;
	}
	if (
		obj == m_ui->left1 || obj == m_ui->left2 || obj == m_ui->left3 
		|| obj == m_ui->right1 || obj == m_ui->right2 
		|| obj == m_ui->right3
	) {
		return true;
	} else {
		return false;
	}
	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	if (obj == m_ui->left1) {
		QPainter p(m_ui->left1);
		QPixmap img(imgDir() + "left1.png");
		if (!img.isNull() && conf.value("EnableBars").toBool()) {
			QPoint pos(0, m_ui->left1->height() - img.height());
			p.drawPixmap(pos, img);
			m_ui->left1->setMinimumWidth(img.width());
			m_ui->left1->setMaximumWidth(img.width());
		}
		return true;
	} else if (obj == m_ui->left2) {
		QPainter p(m_ui->left2);
		QPixmap img(imgDir() + "left2.png");
		if (!img.isNull() && conf.value("EnableBars").toBool()) {
			QPoint pos(0, m_ui->left2->height() - img.height());
			p.drawPixmap(pos, img);
			m_ui->left2->setMinimumWidth(img.width());
			m_ui->left2->setMaximumWidth(img.width());
		}
		return true;
	} else if (obj == m_ui->left3) {
		QPainter p(m_ui->left3);
		QPixmap img(imgDir() + "left3.png");
		if (!img.isNull() && conf.value("EnableBars").toBool()) {
			QPoint pos(0, m_ui->left3->height() - img.height());
			p.drawPixmap(pos, img);
			m_ui->left3->setMinimumWidth(img.width());
			m_ui->left3->setMaximumWidth(img.width());
		}
		return true;
	} else if (obj == m_ui->right1) {
		QPainter p(m_ui->right1);
		QPixmap img(imgDir() + "right1.png");
		if (!img.isNull() && conf.value("EnableBars").toBool()) {
			QPoint pos(0, m_ui->right1->height() - img.height());
			p.drawPixmap(pos, img);
			m_ui->right1->setMinimumWidth(img.width());
			m_ui->right1->setMaximumWidth(img.width());
		}
		return true;
	} else if (obj == m_ui->right2) {
		QPainter p(m_ui->right2);
		QPixmap img(imgDir() + "right2.png");
		if (!img.isNull() && conf.value("EnableBars").toBool()) {
			QPoint pos(0, m_ui->right2->height() - img.height());
			p.drawPixmap(pos, img);
			m_ui->right2->setMinimumWidth(img.width());
			m_ui->right2->setMaximumWidth(img.width());
		}
		return true;
	} else if (obj == m_ui->right3) {
		QPainter p(m_ui->right3);
		QPixmap img(imgDir() + "right3.png");
		if (!img.isNull() && conf.value("EnableBars").toBool()) {
			QPoint pos(0, m_ui->right3->height() - img.height());
			p.drawPixmap(pos, img);
			m_ui->right3->setMinimumWidth(img.width());
			m_ui->right3->setMaximumWidth(img.width());
		}
		return true;
	}
	return false;
}

void MainWindow::mouseMoveEvent(QMouseEvent *e) {
	doLogDebug((boost::format("x=%d y=%d") % e->pos().x() % e->pos().y()).str());
//	if (m_closeButton->geometry().contains(e->pos())) {
//		QMouseEvent event(
//			QEvent::MouseMove,
//			m_closeButton->mapFromParent(e->pos()), Qt::NoButton, 0, 0
//		);
//		QCoreApplication::sendEvent(m_closeButton, &event);
//	}
	if (e->buttons() == Qt::LeftButton && !m_showTitle) {
		if (!snapToBorder(e->globalPos() - m_dragPosition)) {
			move(e->globalPos() - m_dragPosition);
			e->accept();
		}
	}
//	if (e->buttons() == Qt::NoButton) {
//		QRect right(0, 0, width(), height());//width() - 5, 0, 5, height());
//		if (right.contains(e->pos())) {
//			QApplication::setOverrideCursor(QCursor(Qt::SizeHorCursor));
//		} else {
//			setCursor(QCursor(Qt::ArrowCursor));
//		}
//	}
//	QMainWindow::mouseMoveEvent(e);
}

void MainWindow::mousePressEvent(QMouseEvent *e) {
	if (e->button() == Qt::LeftButton) {
		m_dragPosition = e->globalPos() - frameGeometry().topLeft();
		e->accept();
	}
//	QMainWindow::mousePressEvent(e);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *e) {
	if (e->button() == Qt::RightButton) {
		QMenu menu(this);
		menu.addAction("Toggle Titlebar", this, SLOT(toggleTitleBar()));
		menu.addSeparator();
		menu.addAction("Minimize", this, SLOT(showMinimized()));
		menu.addAction("Maximize", this, SLOT(showMaximized()));
		menu.addAction("Exit", this, SLOT(close()));
		menu.exec(e->globalPos());
	}
	QMainWindow::mouseReleaseEvent(e);
}

bool MainWindow::snapToBorder(QPoint pos) {
	const int r = 10; // snap radius
	QDesktopWidget desktop;
	QRect rect(desktop.availableGeometry(this));
	QRect snapRight(rect.right() - r, 0, r * 2, rect.height());
	QRect snapTop(0, rect.top() - r, rect.width(), r * 2);
	QRect snapLeft(rect.left() - r, 0, r * 2, rect.height());
	QRect snapBottom(0, rect.bottom() - r, rect.width(), r * 2);
	snapRight.translate(-frameGeometry().width(), 0);
	snapBottom.translate(0, -frameGeometry().height());
	if (!m_showTitle) {
		snapRight.translate(5, 0);
		snapLeft.translate(-5, 0);
		snapBottom.translate(0, 5);
	}
	bool snapped = false;
	int xPos = 0, yPos = 0;
	if (snapRight.contains(pos)) {
		xPos = rect.right() - frameGeometry().width() + 1;
		if (!m_showTitle) {
			xPos += 5;
		}
		yPos = pos.y();
		snapped = true;
	}
	if (m_showTitle && snapTop.contains(pos)) {
		yPos = rect.top();
		if (!snapped) { // already snapped to right
			xPos = pos.x();
		}
		snapped = true;
	}
	if (snapLeft.contains(pos)) {
		xPos = rect.left();
		if (!m_showTitle) {
			xPos -= 5;
		}
		if (!snapped) {
			yPos = pos.y();
		}
		snapped = true;
	}
	if (snapBottom.contains(pos)) {
		yPos = rect.bottom() - frameGeometry().height() + 1;
		if (!m_showTitle) {
			yPos += 5;
		}
		if (!snapped) {
			xPos = pos.x();
		}
		snapped = true;
	}
	if (snapped) {
		move(xPos, yPos);
	}
	return snapped;
}

void MainWindow::moveEvent(QMoveEvent *e) {
//	if (!snapToBorder(e->pos())) {
		QMainWindow::moveEvent(e);
//	}
}

std::ofstream ofs;
void doLogDebug(const std::string &msg) {
	using namespace boost::posix_time;
	ptime t(second_clock::local_time());
	ofs << "[" << t << "] " << msg << std::endl;
	ofs.flush();
}

int main(int argc, char *argv[]) try {
	QT_REQUIRE_VERSION(argc, argv, "4.1.0");
	QApplication app(argc, argv);

	s_confDir = QApplication::applicationFilePath();
	int lastSeparator = s_confDir.lastIndexOf('/');
	s_confDir = s_confDir.remove(lastSeparator + 1, s_confDir.size() - lastSeparator);
	s_imgDir  = s_confDir + "backgrounds" + "/";
#ifdef Q_OS_WIN32
	s_confDir = s_confDir + "config" + "/";
#else
	char *homeDir = getenv("HOME");
	if (homeDir) {
		s_confDir = QString::fromAscii(homeDir) + "/.hydranode/";
	} else {
		qDebug("HOME environment variable not present.\n");
		s_confDir = s_confDir + "config" + "/";
	}
#endif

	QDir cDir;
	if (!cDir.exists(s_confDir)) {
		cDir.mkdir(s_confDir);
	}
	ofs.open((confDir() + "cgcomm.log").toStdString().c_str());
//#ifndef NDEBUG
	Engine::debugMsg.connect(boost::bind(doLogDebug, _1));
//#endif

	logDebug("Configuration directory is     " + s_confDir);
	logDebug("Background images directory is " + s_imgDir);
#ifdef Q_WS_X11
	QStyle *s = app.style();
	if (s && s->objectName() == "motif") {
		app.setStyle("plastique");
	}
#endif

	s_mainWindow = new MainWindow;
	s_mainWindow->initGui();
	if (argc == 3) try {
		QString ip = argv[1];
		quint16 port = boost::lexical_cast<uint16_t>(argv[2]);
		s_mainWindow->initCore(ip, port);
	} catch (std::exception &e) {
		QMessageBox::critical(
			0, "Error", e.what(), QMessageBox::Ok,
			QMessageBox::NoButton
		);
		return 0;
	} else {
		s_mainWindow->initCore("127.0.0.1", 9990);
	}

	int ret = app.exec();
	return ret;
} catch (std::exception &e) {
	doLogDebug(e.what());
	return 0;
}

