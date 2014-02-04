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

#include "settingspage.h"
#include "settingstabs.h"
#include "main.h"
#include "ecomm.h"
#include <boost/bind.hpp>
#include <QFileDialog>
#include <QDoubleValidator>
#include <QIntValidator>
#include <boost/lexical_cast.hpp>
#include <QSettings>
#include <QMessageBox>
#include <QPainter>

static SettingsPage *s_instance;
SettingsPage::SettingsPage(QWidget *parent) : QWidget(parent), m_config() {
	s_instance = this;

	// set up ui and engine communication
	m_ui = new Ui::SettingsTabs;
	m_ui->setupUi(this);
	m_ui->downSpeed->setValidator(new QDoubleValidator(m_ui->downSpeed));
	m_ui->upSpeed->setValidator(new QDoubleValidator(m_ui->upSpeed));
	m_ui->openConns->setValidator(new QIntValidator(m_ui->openConns));
	m_ui->newConns->setValidator(new QIntValidator(m_ui->newConns));
	m_ui->newHopen->setValidator(new QIntValidator(m_ui->newHopen));

	connect(
		&MainWindow::instance(), SIGNAL(engineConnection(bool)),
		SLOT(engineConnection(bool))
	);
	engineConnection(
		MainWindow::instance().getEngine() &&
		MainWindow::instance().getEngine()->getMain()
	);

	// whenever a value is changed in ui, enable buttons
	connect(
		m_ui->nickEdit, SIGNAL(textEdited(const QString&)),
		SLOT(enableButtons())
	);
	connect(
		m_ui->incEdit, SIGNAL(textChanged(const QString&)),
		SLOT(enableButtons())
	);
	connect(
		m_ui->tempEdit, SIGNAL(textChanged(const QString&)),
		SLOT(enableButtons())
	);
//	connect(
//		m_ui->filterEdit, SIGNAL(textChanged(const QString&)),
//		SLOT(enableButtons())
//	);
	connect(
		m_ui->langSelect, SIGNAL(currentIndexChanged(int)),
		SLOT(enableButtons())
	);
	connect(
		m_ui->checkLimitDown, SIGNAL(stateChanged(int)),
		SLOT(enableButtons())
	);
	connect(
		m_ui->checkLimitUp, SIGNAL(stateChanged(int)),
		SLOT(enableButtons())
	);
	connect(
		m_ui->checkLimitConn, SIGNAL(stateChanged(int)),
		SLOT(enableButtons())
	);
	connect(
		m_ui->checkLimitNewConn, SIGNAL(stateChanged(int)),
		SLOT(enableButtons())
	);
	connect(
		m_ui->checkLimitNewHopen, SIGNAL(stateChanged(int)),
		SLOT(enableButtons())
	);
	connect(
		m_ui->downSpeed, SIGNAL(textEdited(const QString&)),
		SLOT(enableButtons())
	);
	connect(
		m_ui->upSpeed, SIGNAL(textEdited(const QString&)),
		SLOT(enableButtons())
	);
	connect(
		m_ui->openConns, SIGNAL(textEdited(const QString&)),
		SLOT(enableButtons())
	);
	connect(
		m_ui->newConns, SIGNAL(textEdited(const QString&)),
		SLOT(enableButtons())
	);
	connect(
		m_ui->newHopen, SIGNAL(textEdited(const QString&)),
		SLOT(enableButtons())
	);
	connect(
		m_ui->checkAssocEd2k, SIGNAL(stateChanged(int)),
		SLOT(enableButtons())
	);
	connect(
		m_ui->checkAssocTorrent, SIGNAL(stateChanged(int)),
		SLOT(enableButtons())
	);
	connect(
		m_ui->checkEnableBars, SIGNAL(stateChanged(int)),
		SLOT(enableButtons())
	);
	connect(
		m_ui->checkEnableBack, SIGNAL(stateChanged(int)),
		SLOT(enableButtons())
	);
	connect(
		m_ui->checkEnablePerc, SIGNAL(stateChanged(int)),
		SLOT(enableButtons())
	);

	connect(
		m_ui->checkLimitDown, SIGNAL(stateChanged(int)),
		SLOT(updateDownLimitBox())
	);
	connect(
		m_ui->checkLimitUp, SIGNAL(stateChanged(int)),
		SLOT(updateUpLimitBox())
	);
	connect(
		m_ui->checkLimitConn, SIGNAL(stateChanged(int)),
		SLOT(updateConnLimitBox())
	);
	connect(
		m_ui->checkLimitNewConn, SIGNAL(stateChanged(int)),
		SLOT(updateNewConnLimitBox())
	);
	connect(
		m_ui->checkLimitNewHopen, SIGNAL(stateChanged(int)),
		SLOT(updateHopenConnLimitBox())
	);
//	connect(
//		m_ui->checkAssocEColl, SIGNAL(stateChanged(int)),
//		SLOT(enableButtons())
//	);

	// apply/revert button behaviour
	connect(m_ui->applyButton, SIGNAL(clicked()), SLOT(saveSettings()));
	connect(m_ui->revertButton, SIGNAL(clicked()), SLOT(revertSettings()));

	// directory select buttons
	connect(m_ui->incButton, SIGNAL(clicked()), SLOT(selectIncoming()));
	connect(m_ui->tempButton, SIGNAL(clicked()), SLOT(selectTemp()));
//	connect(
//		m_ui->ipfilterButton, SIGNAL(clicked()), SLOT(selectIpfilter())
//	);

	m_ui->actionBar->installEventFilter(this);

	QPalette palette = m_ui->nickEdit->palette();
	palette.setColor(QPalette::Base, QColor(255, 255, 255, 150));
	palette.setColor(QPalette::Inactive, QPalette::Base, QColor(255, 255, 255, 120));
	m_ui->nickEdit->setPalette(palette);
	m_ui->incEdit->setPalette(palette);
	m_ui->tempEdit->setPalette(palette);
	m_ui->downSpeed->setPalette(palette);
	m_ui->upSpeed->setPalette(palette);
	m_ui->openConns->setPalette(palette);
	m_ui->newConns->setPalette(palette);
	m_ui->newHopen->setPalette(palette);
	m_ui->langSelect->setPalette(palette);

	m_background = QPixmap(imgDir() + "settingsback.png");
	m_actionBack = QPixmap(imgDir() + "actionback.png");
	m_backgroundOrig = m_background;
	m_actionBackOrig = m_actionBack;
}

SettingsPage& SettingsPage::instance() { return *s_instance; }

void SettingsPage::configValues(const std::map<std::string, std::string> &v) {
	m_values = v;
	revertSettings();
}

void SettingsPage::enableButtons() {
	m_ui->applyButton->setEnabled(true);
	m_ui->revertButton->setEnabled(true);
}

void SettingsPage::saveSettings() {
	bool valueChanged = false;

	std::string newNick = m_ui->nickEdit->text().toStdString();
	if (m_values["ed2k/Nick"] != newNick) {
		m_config->setValue("ed2k/Nick", newNick);
		valueChanged = true;
	}

	std::string newInc = m_ui->incEdit->text().toStdString();
	if (m_values["Incoming"] != newInc) {
		m_config->setValue("Incoming", newInc);
		valueChanged = true;
	}

	std::string newTemp = m_ui->tempEdit->text().toStdString();
	if (m_values["Temp"] != newTemp) {
		m_config->setValue("Temp", newTemp);
		valueChanged = true;
	}

//	std::string newFilter = m_ui->filterEdit->text().toStdString();
//	if (m_values["IPFilter"] != newFilter) {
//		m_config->setValue("IPFilter", newFilter);
//		valueChanged = true;
//	}

	std::string newDownLimit;
	if (!m_ui->checkLimitDown->isChecked()) {
		newDownLimit = "0";
	} else {
		newDownLimit = boost::lexical_cast<std::string>(
			static_cast<int>(m_ui->downSpeed->text().toFloat()*1024)
		);
	}
	if (m_values["DownSpeedLimit"] != newDownLimit) {
		m_config->setValue("DownSpeedLimit", newDownLimit);
		valueChanged = true;
	}

	std::string newUpLimit;
	if (!m_ui->checkLimitUp->isChecked()) {
		newUpLimit = "0";
	} else {
		newUpLimit = boost::lexical_cast<std::string>(
			static_cast<int>(m_ui->upSpeed->text().toFloat() * 1024)
		);
	}
	if (m_values["UpSpeedLimit"] != newUpLimit) {
		m_config->setValue("UpSpeedLimit", newUpLimit);
		valueChanged = true;
	}

	if (!m_ui->checkLimitConn->isChecked()) {
		m_ui->openConns->setText("0");
	}
	std::string newConnLimit = m_ui->openConns->text().toStdString();
	if (m_values["ConnectionLimit"] != newConnLimit) {
		m_config->setValue("ConnectionLimit", newConnLimit);
		valueChanged = true;
	}

	if (!m_ui->checkLimitNewConn->isChecked()) {
		m_ui->newConns->setText("0");
	}
	std::string newNewConn = m_ui->newConns->text().toStdString();
	if (m_values["NewConnsPerSec"] != newNewConn) {
		m_config->setValue("NewConnsPerSec", newNewConn);
		valueChanged = true;
	}

	if (!m_ui->checkLimitNewHopen->isChecked()) {
		m_ui->newHopen->setText("0");
	}
	std::string newHopen = m_ui->newHopen->text().toStdString();
	if (m_values["ConnectingLimit"] != newHopen) {
		m_config->setValue("ConnectingLimit", newHopen);
		valueChanged = true;
	}
#ifdef Q_OS_WIN32
	QSettings settings(
		"HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes"
		"\\ed2k\\shell\\open\\command",
		QSettings::NativeFormat
	);
	QString hLinkPath = QCoreApplication::applicationDirPath();
	hLinkPath = hLinkPath.replace('/', '\\');
	hLinkPath += "\\hlink.exe";
	if (m_ui->checkAssocEd2k->isChecked()) {
		settings.setValue("", "\"" + hLinkPath + "\" \"%1\"");
	} else if (
		settings.value("").toString().endsWith(
			"hlink.exe\" \"%1\""
		)
	) {
		settings.setValue("", "");
	}
	QSettings(
		"HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\ed2k",
		QSettings::NativeFormat
	).setValue("", "URL: ed2k Protocol");
	QSettings(
		"HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\ed2k",
		QSettings::NativeFormat
	).setValue("URL Protocol", "");

	// open command for hydranode-associated files
	QString path(
		"HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\"
		"Hydranode\\shell\\open\\command"
	);
	QSettings(path, QSettings::NativeFormat).setValue(
		"", "\"" + hLinkPath + "\" \"%1\""
	);

//	QSettings settings2(
//		"HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes"
//		"\\.emulecollection", QSettings::NativeFormat
//	);
//	if (m_ui->checkAssocEColl->isChecked()) {
//		settings2.setValue("", "Hydranode");
//	} else if (settings.value("").toString() == "Hydranode") {
//		settings2.setValue("", "");
//	}
	QSettings settings3(
		"HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\.torrent",
		QSettings::NativeFormat
	);
	if (m_ui->checkAssocTorrent->isChecked()) {
		settings3.setValue("", "Hydranode");
	} else if (settings.value("").toString() == "Hydranode") {
		settings3.setValue("", "");
	}
#endif
	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	conf.setValue("EnableBars", m_ui->checkEnableBars->isChecked());
	conf.setValue("EnableBack", m_ui->checkEnableBack->isChecked());
	conf.setValue("EnablePerc", m_ui->checkEnablePerc->isChecked());

	bool enableBars = conf.value("EnableBars").toBool();
	bool enableBack = conf.value("EnableBack").toBool();
	barsEnabled(enableBars);
	backEnabled(enableBack);
	if (enableBars && enableBack) {
		m_ui->pageFrame->setFrameShape(QFrame::NoFrame);
	} else {
		m_ui->pageFrame->setFrameShape(QFrame::Panel);
	}

	// ui-only changes don't trigger revertSettings callback being invoked
	// via cgcomm, so update the buttons in that case here.
	if (!valueChanged) {
		m_ui->applyButton->setEnabled(false);
		m_ui->revertButton->setEnabled(false);
	}
	// some changes force us to enable titlebar
	if (!conf.value("EnableBars").toBool()) {
		MainWindow::instance().toggleTitleBar();
	}
	window()->update(); // needed for gui-related changes
}

void SettingsPage::revertSettings() {
	m_ui->nickEdit->setText(QString::fromStdString(m_values["ed2k/Nick"]));
	m_ui->incEdit->setText(QString::fromStdString(m_values["Incoming"]));
	m_ui->tempEdit->setText(QString::fromStdString(m_values["Temp"]));
//	m_ui->filterEdit->setText(
//		QString::fromStdString(m_values["IPFilter"])
//	);
	m_ui->checkLimitDown->setChecked(m_values["DownSpeedLimit"] != "0");
	m_ui->downSpeed->setText(
		QString::number(
			QString::fromStdString(m_values["DownSpeedLimit"])
			.toFloat() / 1024.0, 'g', 5
		)
	);
	m_ui->checkLimitUp->setChecked(m_values["UpSpeedLimit"] != "0");
	m_ui->upSpeed->setText(
		QString::number(
			QString::fromStdString(m_values["UpSpeedLimit"])
			.toFloat() / 1024.0, 'g', 5
		)
	);
	m_ui->checkLimitConn->setChecked(m_values["ConnectionLimit"] != "0");
	m_ui->openConns->setText(
		QString::fromStdString(m_values["ConnectionLimit"])
	);
	m_ui->checkLimitNewConn->setChecked(m_values["NewConnsPerSec"] != "0");
	m_ui->newConns->setText(
		QString::fromStdString(m_values["NewConnsPerSec"])
	);
	m_ui->checkLimitNewHopen->setChecked(
		m_values["ConnectingLimit"] != "0"
	);
	m_ui->newHopen->setText(
		QString::fromStdString(m_values["ConnectingLimit"])
	);

	if (m_values["DownSpeedLimit"] == "0") {
		m_ui->downSpeed->setText("unlimited");
	}
	if (m_values["UpSpeedLimit"] == "0") {
		m_ui->upSpeed->setText("unlimited");
	}
	if (m_values["ConnectionLimit"] == "0") {
		m_ui->openConns->setText("unlimited");
	}
	if (m_values["NewConnsPerSec"] == "0") {
		m_ui->newConns->setText("unlimited");
	}
	if (m_values["ConnectingLimit"] == "0") {
		m_ui->newHopen->setText("unlimited");
	}

#ifdef Q_OS_WIN32
	QString hLinkPath = QCoreApplication::applicationDirPath();
	hLinkPath = hLinkPath.replace('/', '\\');
	hLinkPath += "\\hlink.exe";

	QString path(
		"HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\"
		"Hydranode\\shell\\open\\command"
	);
	QSettings(path, QSettings::NativeFormat).setValue(
		"", "\"" + hLinkPath + "\" \"%1\""
	);

	m_ui->checkAssocEd2k->setChecked(
		QSettings(
			"HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes"
			"\\ed2k\\shell\\open\\command",
			QSettings::NativeFormat
		).value("").toString().contains("\"" + hLinkPath + "\" \"%1\"")
	);
//	m_ui->checkAssocEColl->setChecked(
//		QSettings(
//			"HKEY_LOCAL_MACHINE\\SOFTWARE\\"
//			"Classes\\.emulecollection",
//			QSettings::NativeFormat
//		).value("").toString() == "Hydranode"
//	);
	bool assocTorrent = true;
	assocTorrent &= QSettings(
		"HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\.torrent",
		QSettings::NativeFormat
	).value("").toString() == "Hydranode";
//	assocTorrent &= QSettings(
//		"HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\"
//		"Hydranode\\shell\\open\\command", QSettings::NativeFormat
//	).value("").toString() == "\"" + hLinkPath + "\" \"%1\"";
	m_ui->checkAssocTorrent->setChecked(assocTorrent);
#endif

	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	bool enableBars = conf.value("EnableBars").toBool();
	bool enableBack = conf.value("EnableBack").toBool();
	bool enablePerc = conf.value("EnablePerc").toBool();
	m_ui->checkEnableBars->setChecked(enableBars);
	m_ui->checkEnableBack->setChecked(enableBack);
	m_ui->checkEnablePerc->setChecked(enablePerc);

	if (enableBars && enableBack) {
		m_ui->pageFrame->setFrameShape(QFrame::NoFrame);
	} else {
		m_ui->pageFrame->setFrameShape(QFrame::Panel);
	}

	m_ui->applyButton->setEnabled(false);
	m_ui->revertButton->setEnabled(false);
}

void SettingsPage::selectIncoming() {
	QString s = QFileDialog::getExistingDirectory(
		window(), "Choose location for incoming files",
		m_ui->incEdit->text(), QFileDialog::ShowDirsOnly
	);
	if (s.size()) {
		m_ui->incEdit->setText(s);
	}
}

void SettingsPage::selectTemp() {
	QString s = QFileDialog::getExistingDirectory(
		window(), "Choose location for temporary files",
		m_ui->tempEdit->text(), QFileDialog::ShowDirsOnly
	);
	if (s.size()) {
		m_ui->tempEdit->setText(s);
	}
}

void SettingsPage::selectIpfilter() {
//	QString s = QFileDialog::getOpenFileName(
//		this, "Open ipfilter",
//		m_ui->filterEdit->text(), "IP Filters (*.dat *.p2p)"
//	);
//	if (s.size()) {
//		m_ui->filterEdit->setText(s);
//	}
}

void SettingsPage::engineConnection(bool state) {
	setEnabled(state);
	if (state) {
		m_config = new Engine::Config(
			MainWindow::instance().getEngine()->getMain(),
			boost::bind(&SettingsPage::configValues, this, _1)
		);
		m_config->getList();
		m_config->monitor();
	} else {
		delete m_config;
		m_config = 0;
		m_values.clear();
	}
}

bool SettingsPage::eventFilter(QObject *obj, QEvent *evt) {
	if (obj != m_ui->actionBar || evt->type() != QEvent::Paint) {
		return false;
	}
	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	if (!conf.value("EnableBars").toBool() || m_actionBack.isNull()) {
		return false;
	}
	QPainter p(m_ui->actionBar);
	int w = m_ui->actionBar->width();
	int h = m_ui->actionBar->height();
	if (m_actionBack.width() != w || m_actionBack.height() != h) {
		m_actionBack = m_actionBackOrig.scaled(
			w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation
		);
	}
	p.drawPixmap(0, 0, m_actionBack);
	return false;
}

void SettingsPage::paintEvent(QPaintEvent *evt) {
	QSettings conf(confDir() + "gui.ini", QSettings::IniFormat);
	if (!conf.value("EnableBack").toBool()) {
		return;
	}
	if (!conf.value("EnableBars").toBool()) {
		return;
	}

	if (m_background.isNull()) {
		return;
	}

	int w = width() - 10;
	int h = height() - m_ui->actionBar->height() - 5;
	
	if (m_background.width() != w || m_background.height() != h) {
		m_background = m_backgroundOrig.scaled(
			w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation
		);
	}
	QPainter p(this);
	p.drawPixmap(5, m_ui->actionBar->height(), m_background);
}

void SettingsPage::updateDownLimitBox() {
	if (!m_ui->checkLimitDown->isChecked()) {
		m_ui->downSpeed->setText("unlimited");
	} else {
		m_ui->downSpeed->setText(
			QString::number(
				QString::fromStdString(
					m_values["DownSpeedLimit"]
				).toFloat() / 1024.0, 'g', 5
			)
		);
	}
}

void SettingsPage::updateUpLimitBox() {
	if (!m_ui->checkLimitUp->isChecked()) {
		m_ui->upSpeed->setText("unlimited");
	} else {
		m_ui->upSpeed->setText(
			QString::number(
				QString::fromStdString(
					m_values["UpSpeedLimit"]
				).toFloat() / 1024.0, 'g', 5
			)
		);
	}
}

void SettingsPage::updateConnLimitBox() {
	if (!m_ui->checkLimitConn->isChecked()) {
		m_ui->openConns->setText("unlimited");
	} else {
		m_ui->openConns->setText(
			QString::fromStdString(m_values["ConnectionLimit"])
		);
	}
}

void SettingsPage::updateNewConnLimitBox() {
	if (!m_ui->checkLimitNewConn->isChecked()) {
		m_ui->newConns->setText("unlimited");
	} else {
		m_ui->newConns->setText(
			QString::fromStdString(m_values["NewConnsPerSec"])
		);
	}
}

void SettingsPage::updateHopenConnLimitBox() {
	if (!m_ui->checkLimitNewHopen->isChecked()) {
		m_ui->newHopen->setText("unlimited");
	} else {
		m_ui->newHopen->setText(
			QString::fromStdString(m_values["ConnectingLimit"])
		);
	}
}

