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

#include <hncgcomm/cgcomm.h>

#include "ecomm.h"
#include <QtCore/QTimer>
#include <boost/bind.hpp>

QString bytesToString(quint64 bytes) {
	if (bytes == 1) {
		return QString("1 b");
	} else if (bytes < 1024) {
		return QString("%1 b").arg(bytes);
	} else if (bytes < 1024*1024) {
		return QString("%1 kB").arg(bytes/1024.0, 0, 'f', 2);
	} else if (bytes < 1024*1024*1024) {
		return QString("%1 MB").arg(bytes/1024.0/1024.0, 0, 'f', 2);
	} else if (bytes < 1024ull*1024ull*1024ull*1024ull) {
		return QString("%1 GB").arg(
			bytes/1024.0/1024.0/1024.0, 0, 'f', 2
		);
	} else {
		return QString("%1 TB").arg(
			bytes/1024.0/1024.0/1024.0/1024.0, 0, 'f', 2
		);
	}
}

QString secondsToString(qint64 sec, quint8 trunc) {
	QString speedStr;
	quint32 min = 0, hour = 0, day = 0, mon = 0, year = 0;
	if (trunc && sec >= (60*60*24*30*12)) {
		year = sec / (60*60*24*30*12);
		sec -= (year * 60*60*24*30*12);
		speedStr += QString("%1y ").arg(year);
		--trunc;
	}
	if (trunc && sec >= (60*60*24*30)) {
		mon  = sec / (60*60*24*30);
		sec -= (mon * 60*60*24*30);
		speedStr += QString("%1mo ").arg(mon);
		--trunc;
	}
	if (trunc && sec >= (60*60*24)) {
		day  = sec / (60*60*24);
		sec -= (day * 60*60*24);
		speedStr += QString("%1d ").arg(day);
		--trunc;
	}
	if (trunc && sec >= 3600) {
		hour = sec / 3600;
		sec -= hour * 3600;
		speedStr += QString("%1h ").arg(hour);
		--trunc;
	}
	if (trunc && sec >= 60) {
		min = sec / 60;
		sec -= min * 60;
		speedStr += QString("%1m ").arg(min);
		--trunc;
	}
	if (trunc && sec) {
		speedStr += QString("%1s ").arg(sec);
	}
	return speedStr.trimmed();
}

// EngineComm class
// ----------------
EngineComm::EngineComm() : m_socket(new QTcpSocket(this)), m_engine(),
m_reconnectTimer(), m_tryReconnect(true) {

	connect(m_socket, SIGNAL(connected()), SLOT(socketConnected()));
	connect(
		m_socket, SIGNAL(error(QAbstractSocket::SocketError)),
		SLOT(socketError(QAbstractSocket::SocketError))
	);
	connect(m_socket, SIGNAL(disconnected()), SLOT(socketClosed()));
	connect(m_socket, SIGNAL(readyRead()), SLOT(newData()));
}

void EngineComm::connectToHost(const QString &host, quint16 port) {
	m_socket->connectToHost(host, port);
	m_socket->waitForConnected(1000);
	switch (m_socket->state()) {
		case QTcpSocket::UnconnectedState:
			break;
		case QTcpSocket::HostLookupState:
			break;
		case QTcpSocket::ConnectingState:
			break;
		case QTcpSocket::ConnectedState:
			break;
		case QTcpSocket::ClosingState:
			break;
		default:
			break;
	}
}

void EngineComm::socketConnected() {
	m_engine = new Engine::Main(
		boost::bind(&EngineComm::sendData, this, _1)
	);
	connectionEstablished();
}

void EngineComm::socketError(QAbstractSocket::SocketError) {
	if (!m_tryReconnect) {
		return;
	}	
	if (!m_reconnectTimer) {
		m_reconnectTimer = new QTimer(this);
		connect(
			m_reconnectTimer, SIGNAL(timeout()),
			SLOT(tryReconnect())
		);
	}
	m_reconnectTimer->start(1000);
	connectionLost();
}

void EngineComm::socketClosed() {
	if (!m_tryReconnect) {
		return;
	}	
	if (!m_reconnectTimer) {
		m_reconnectTimer = new QTimer(this);
		connect(
			m_reconnectTimer, SIGNAL(timeout()),
			SLOT(tryReconnect())
		);
	}
	m_reconnectTimer->start(1000);
	connectionLost();
}

void EngineComm::sendData(const std::string &data) {
	Q_ASSERT(m_socket->state() == QTcpSocket::ConnectedState);
	qint64 ret = m_socket->write(data.data(), data.size());
	Q_ASSERT(ret == data.size());
	(void)ret;
}

void EngineComm::newData() {
	std::string buffer;
	char tmp[1024];
	qint64 lastRead = 0;
	while ((lastRead = m_socket->read(tmp, 1024)) > 0) {
		buffer.append(tmp, lastRead);
	}
	m_engine->parse(buffer);
}

void EngineComm::tryReconnect() {
	switch (m_socket->state()) {
		case QTcpSocket::ConnectedState:
		case QTcpSocket::ConnectingState:
			delete m_reconnectTimer;
			m_reconnectTimer = 0;
			return;
		default:
			break;
	}
	m_socket->connectToHost("127.0.0.1", 9990);
	Engine::debugMsg("Attempting to re-connect to Engine...");
	connectionLost();
}

bool EngineComm::isConnected() const {
	return m_socket->state() & QAbstractSocket::ConnectedState;
}

void EngineComm::setTryReconnect(bool tryReconnect) {
	m_tryReconnect = tryReconnect;
}
