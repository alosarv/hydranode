/*
 *  Copyright (C) 2004-2006 Alo Sarv <madcat_@users.sourceforge.net>
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

/**
 * \file log.cpp Implementation of Log class
 */

#include <hnbase/pch.h>
#include <hnbase/osdep.h>
#include <hnbase/log.h>

#include <boost/date_time/posix_time/posix_time.hpp> // for ptime

boost::recursive_mutex Log::s_iosLock;
Log *Log::s_log = new Log;
std::deque<std::string> Log::s_messages;

Log::Log() : m_output(true), m_transformColors(true), m_disableColors(false) {
	m_internalMasks["hash"]       = TRACE_HASH;
	m_internalMasks["metadata"]   = TRACE_MD;
	m_internalMasks["modules"]    = TRACE_MOD;
	m_internalMasks["socket"]     = TRACE_SOCKET;
	m_internalMasks["partdata"]   = TRACE_PARTDATA;
	m_internalMasks["sharedfile"] = TRACE_SHAREDFILE;
	m_internalMasks["fileslist"]  = TRACE_FILESLIST;
	m_internalMasks["object"]     = TRACE_OBJECT;
	m_internalMasks["hashthread"] = TRACE_HT;
	m_internalMasks["scheduler"]  = TRACE_SCHED;
	m_internalMasks["range"]      = TRACE_RANGE;
	m_internalMasks["event"]      = TRACE_EVENT;
	m_internalMasks["config"]     = TRACE_CONFIG;
	m_internalMasks["resolver"]   = TRACE_RESOLVER;
	m_internalMasks["hasher"]     = TRACE_HASHER;
	m_internalMasks["chunks"]     = TRACE_CHUNKS;
}

Log::~Log() {
        s_log = 0;
}
Log& Log::instance() {
	return s_log ? *s_log : *(s_log = new Log);
}

void Log::sendToFiles(std::string msg) {
	boost::mutex::scoped_lock l(m_filesLock);
        using namespace boost::posix_time;
        ptime t(second_clock::local_time());

	size_t pos = msg.find(0x1b, 0);
	while (pos != std::string::npos) {
		uint32_t j = msg.find('m', pos);
		msg.erase(pos, j - pos + 1);
		pos = msg.find(0x1b, 0);
	}

	FIter i = m_logFiles.begin();
	while (i != m_logFiles.end()) try {
		std::ofstream ofs((*i).c_str(), std::ios::app);
		ofs << "[" << t << "] " << msg << std::endl;
		ofs.flush();
		++i;
	} catch (std::exception &e) {
		logError(boost::format("Writing log file: %s") % e.what());
	}
}

void Log::printLast(uint32_t count) {
	if (count > s_messages.size()) {
		count = s_messages.size();
	}
	CMIter i = s_messages.end();
	while (--i != s_messages.begin() && --count);
	while (i != s_messages.end()) {
		std::cerr << " [" << count++ << "] " <<  *i++ << std::endl;
	}
	std::cerr.flush();
}

void Log::getLast(uint32_t count, std::vector<std::string> *cont) {
	CHECK(count);

	if (count > s_messages.size()) {
		count = s_messages.size();
	}
	while (count--) {
		cont->push_back(s_messages.at(s_messages.size()-count-1));
	}
}

void Log::writeMsg(std::string msg) {
	if (m_disableColors) {
		size_t pos = msg.find(0x1b, 0);
		while (pos != std::string::npos) {
			uint32_t j = msg.find('m', pos);
			msg.erase(pos, j - pos + 1);
			pos = msg.find(0x1b, 0);
		}
	}

#ifdef WIN32 // custom code for win32 console colors handling
	if (!m_transformColors) {
		std::cerr << msg;
		return;
	}
	std::string tmp;
	DWORD cWritten;
	WORD wOldColors;
	CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_ERROR_HANDLE), &csbiInfo);
	wOldColors = csbiInfo.wAttributes;
	for (size_t i = 0; i < msg.size(); ++i) {
		if (msg[i] == '\33' && msg[i+1] == '[' && msg.size() >= i+5) {
			DWORD color = 0;
			if (msg[i+2] == '1') {
				color |= FOREGROUND_INTENSITY;
			}
			if (msg[i+5] == '1') {
				color |= FOREGROUND_RED;
			} else if (msg[i+5] == '2') {
				color |= FOREGROUND_GREEN;
			} else if (msg[i+5] == '3') {
				color |= FOREGROUND_RED;
				color |= FOREGROUND_GREEN;
			} else if (msg[i+5] == '4') {
				color |= FOREGROUND_BLUE;
			} else if (msg[i+5] == '5') {
				color |= FOREGROUND_BLUE;
				color |= FOREGROUND_RED;
			} else if (msg[i+5] == '6') {
				color |= FOREGROUND_GREEN;
				color |= FOREGROUND_BLUE;
			} else {
				color = wOldColors;
			}
			WriteConsole(
				GetStdHandle(STD_ERROR_HANDLE),
				tmp.c_str(), tmp.size(), &cWritten, NULL
			);
			tmp.clear();
			SetConsoleTextAttribute(
				GetStdHandle(STD_ERROR_HANDLE), color
			);
			if (msg[i+5] == 'm') {
				i += 5;
			} else if (msg[i+6] == 'm') {
				i += 6;
			}
		} else {
			tmp.push_back(msg[i]);
		}
	}
	if (tmp.size()) {
		WriteConsole(
			GetStdHandle(STD_ERROR_HANDLE),
			tmp.c_str(), tmp.size(), &cWritten, NULL
			);
		tmp.clear();
	}
#else
	std::cerr << msg;
#endif
}

void Log::doLogString(MessageType t, const std::string &msg) {
	if (m_output) {
		// clear any existing garbage
		std::cerr << '\r' << m_preStr;
		writeMsg(msg);
		int amount = 77 - (m_preStr.size() + msg.size());
		if (amount > 0) {
			std::cerr << std::string(amount, ' ');
		}
		std::cerr << std::endl;
	}
	sendToFiles(msg);
	addMsg(t, msg);
}

void Log::addMsg(MessageType t, const std::string &msg) {
	Log::s_messages.push_back(msg);
	if (Log::s_messages.size() > 100) {
		Log::s_messages.pop_front();
	}
	m_sig(msg, t);
}

std::string Log::getLastMsg() {
	return s_messages.back();
}

void Log::doLogString(int traceMask, const std::string &msg) {
	boost::recursive_mutex::scoped_lock l(s_iosLock);
	if (Log::hasTraceMask(traceMask)) {
		doLogString(
			MT_TRACE,
			(boost::format("Trace(%s): %s") %
			Log::getTraceStr(traceMask) % msg).str()
		);
	} else {
		addMsg(MT_TRACE, msg);
	}
}

void Log::doLogString(const std::string &mask, const std::string &msg) {
	boost::recursive_mutex::scoped_lock l(s_iosLock);
	if (m_enabledStrMasks.find(mask) != m_enabledStrMasks.end()) {
		doLogString(
			MT_TRACE,
			(boost::format("Trace(%s): %s") %
			mask % msg).str()
		);
	} else {
		addMsg(MT_TRACE, msg);
	}
}

/**
 * Log a simple message.
 *
 * @param msg    Message to be logged
 */
void logMsg(const std::string &msg) {
	boost::recursive_mutex::scoped_lock l(Log::s_iosLock);
	Log::instance().doLogString(MT_MSG, msg);
}

/**
 * Log a formatted mesage
 *
 * @param fmt    Format object to be logged
 */
void logMsg(const boost::format &fmt) {
	boost::recursive_mutex::scoped_lock l(Log::s_iosLock);
	logMsg(fmt.str());
}

/**
 * Log a simple message only in debug mode
 *
 * @param msg     Message to be logged
 */
void logDebug(const std::string &msg) {
#ifndef NDEBUG
	boost::recursive_mutex::scoped_lock l(Log::s_iosLock);
	Log::instance().doLogString(
		MT_DEBUG, (boost::format("Debug: %s") % msg).str()
	);
#else
	(void)msg; // Suppress compiler warning
#endif
}
/**
 * Log a formatted message only in debug mode
 *
 * @param fmt     Format object to be logged
 */
void logDebug(const boost::format &fmt) {
#ifndef NDEBUG
	boost::recursive_mutex::scoped_lock l(Log::s_iosLock);
	logDebug(fmt.str());
#else
	(void)fmt; // Suppress compiler warning
#endif
}

/**
 * Log a warning (message will be prepended by "Warning:"
 *
 * @param msg      Message to be logged
 */
void logWarning(const std::string &msg) {
	boost::recursive_mutex::scoped_lock l(Log::s_iosLock);
	Log::instance().doLogString(
		MT_WARNING,
		(boost::format(COL_YELLOW "Warning: %s" COL_NONE) % msg).str()
	);
}
/**
 * Log a formatted warning (message will be prepended by "Warning:"
 *
 * @param fmt      Formatted message to be logged
 */
void logWarning(const boost::format &fmt) {
	boost::recursive_mutex::scoped_lock l(Log::s_iosLock);
	logWarning(fmt.str());
}

/**
 * Log an error (message will be prepended by "Error:")
 *
 * @param msg      Message to be logged
 */
void logError(const std::string &msg) {
	boost::recursive_mutex::scoped_lock l(Log::s_iosLock);
	Log::instance().doLogString(
		MT_ERROR,
		(boost::format(COL_BRED "Error: %s" COL_NONE) % msg).str()
	);
}
/**
 * Log a formatted error message (message will be prepended by "Error:")
 *
 * @param fmt    Formatted message to be logged
 */
void logError(const boost::format &fmt) {
	boost::recursive_mutex::scoped_lock l(Log::s_iosLock);
	logError(fmt.str());
}

/**
 * Log a fatal error and abort the application.
 *
 * @param msg    Message to be logged
 */
void logFatalError(const std::string &msg) {
	boost::recursive_mutex::scoped_lock l(Log::s_iosLock);
	logError(boost::format("Fatal Error, aborting: %s") %msg);
	abort();
}
/**
 * Log a fatal error and abort the application
 *
 * @param fmt      Formatted message to be logged
 */
void logFatalError(const boost::format &fmt) {
	boost::recursive_mutex::scoped_lock l(Log::s_iosLock);
	logFatalError(fmt.str());
}

#if !defined(NDEBUG) && !defined(NTRACE)
void logTrace(uint32_t mask, const boost::format &msg) {
	Log::instance().doLogString(mask, msg.str());
}
void logTrace(uint32_t mask, const std::string &msg) {
	Log::instance().doLogString(mask, msg);
}
void logTrace(const std::string &mask, const std::string &msg) {
	Log::instance().doLogString(mask, msg);
}
void logTrace(const std::string &mask, const boost::format &msg) {
	Log::instance().doLogString(mask, msg.str());
}
#endif
