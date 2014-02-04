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
 * @file shellclient.cpp Implementation of ShellClient class
 */

#include <hncore/hnsh/shellclient.h>
#include <hncore/hnsh/hnsh.h>
#include <hncore/hydranode.h>
#include <hnbase/log.h>
#include <hnbase/sockets.h>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/trim.hpp>

namespace Shell {

const std::string TRACE = "shell.client";
//! @name Char-mode POSIX telnet-related constants
//@{
//! BACKSPACE + SPACE (erase) + BACKSPACE
const char CHARMODE_BACK[]   =  { 0x08, 0x20, 0x08 };
const char WILL_SUPRESS_GA[] = { 0xFF, 0xFB, 0x03, 0x00 };
const char WILL_ECHO[]       = { 0xFF, 0xFB, 0x01, 0x00 };
const char IAC_DO_NAWS[]     = { 0xff, 0xfd, 0x1f, 0x00 };
const std::string ERASE(CHARMODE_BACK, 3);
//@}

// command_tokenizer class implementation
template<typename InputIterator, typename Token>
bool command_tokenizer::operator()(InputIterator &curr, InputIterator &end,
Token &tok) {
	tok = Token();

	// assume that the string is surrounded by spaces
	m_unescapeChar = ' ';

	if (curr == end) {
		return false;
	}

	// skip preceeding spaces
	while (curr != end && *curr == ' ') {
		++curr;
	}

	while (curr != end) {
		if (*curr == '\\') {
			curr++;
			if (curr == end) {
				break;
			}
			if (*curr == m_unescapeChar) {
				tok += *curr++;
			} else {
				tok += *curr++;
			}
		} else {
			if (*curr == m_unescapeChar) {
				curr++;
				break;
			} else if ((m_unescapeChar == ' ') &&
				(*curr == '"' || *curr == '\'')
			) {
				m_unescapeChar = *curr;
			} else {
				tok += *curr;
			}

			curr++;
		}
	}

	return true;
}

// ShellClient class implementation
IMPLEMENT_EVENT_TABLE(ShellClient, ShellClient*, ShellClientEvent);

ShellClient::ShellClient(SocketClient *c) : m_socket(c),
m_commandInProgress(false), m_width(std::numeric_limits<uint16_t>::max()),
m_height(std::numeric_limits<uint16_t>::max()), m_pos(0), m_mode(MODE_NORMAL),
m_commands(this, c), m_showLog() {
	assert(c != 0);
	assert(c->isConnected());

	c->setPriority(SocketBase::PR_HIGH);

//	Log::instance().enableTraceMask(TRACE);

	c->setHandler(boost::bind(&ShellClient::onEvent, this, _1, _2));

	logTrace(TRACE, "Client connected. Sending welcome message.");
	readFromSocket();

	try {
		// Send some telnet stuff to switch to char mode
		*c << WILL_SUPRESS_GA;
		*c << WILL_ECHO;
		// enable window size reporting
		*c << IAC_DO_NAWS;

		// Send welcome message
		*c << Hydranode::instance().getAppVerLong() << Socket::Endl;

		char *usrname = getenv("USER");
		if (usrname == 0) {
			m_userName = "root";
		} else {
			m_userName = usrname;
		}

		setPrompt();
	} catch (std::exception &) {
		// silently ignored
	}
	m_logConn = Log::instance().addHandler(this, &ShellClient::onLogMsg);

}

ShellClient::~ShellClient() {
	assert(m_socket);

	if (m_socket->isConnected()) {
		m_socket->disconnect();
	}
	m_socket->destroy();
	m_socket = 0;
	m_logConn.disconnect();
}

void ShellClient::onLogMsg(const std::string &msg, MessageType t) {
	if (t == MT_TRACE || t == MT_DEBUG) {
		return;
	}
	if (!m_socket || (m_socket && !m_socket->isConnected())) {
		return;
	}
	if (!Hydranode::instance().isRunning()) {
		return;
	}
	if (!m_showLog && !m_commandInProgress) {
		return;
	}

	// erase the prompt
	*m_socket << "\r" << std::string(getPrompt().size(), ' ') << "\r";

	std::string tmp = msg;
	size_t pos = tmp.find('\n');
	while (pos != std::string::npos) {
		tmp.insert(pos++, 1, '\r');
		pos = tmp.find('\n', ++pos);
	}

	// Write our message
	*m_socket << tmp << Socket::Endl;

	// Re-set prompt
	if (!m_commandInProgress) {
		setPrompt();
		*m_socket << m_buffer;
	}
}

void ShellClient::onEvent(SocketClient*, SocketEvent evt) {
	logTrace(TRACE, "Socket event.");

	switch (evt) {
		case SOCK_READ:
			readFromSocket();
			break;
		case SOCK_TIMEOUT:
		case SOCK_LOST:
		case SOCK_ERR:
			HNShell::instance().removeClient(this);
			break;
		default:
			break;
	}
}

// Low-level reading
void ShellClient::readFromSocket() try {
	using boost::algorithm::trim_right_if;
	using boost::algorithm::iends_with;

//	assert(m_socket->isConnected());

	*m_socket >> m_buffer;

	logTrace(
		TRACE, boost::format("readFromSocket(): buffer:%1%")
		% Utils::hexDump(m_buffer)
	);

	// first layer of processing - filters out all telnet protocol stuff
	size_t i = m_buffer.find(0xffu);
	while (i != std::string::npos && m_buffer.substr(i, 3).size() == 3) {
		uint8_t doDont = m_buffer[++i];
		uint8_t oc = m_buffer[++i];
		boost::format fmt("Telnet Negotiation: %s");
		if (doDont == 0xfd && oc == 0x01) {
			logTrace(TRACE, fmt % "Echoing server side.");
		} else if (doDont == 0xfd && oc == 0x03) {
			logTrace(TRACE, fmt % "Full Duplex Enabled.");
			m_mode = MODE_CHAR;
		} else if (doDont == 0xfa && oc == 0x1f) {
			if (m_buffer.substr(i, 7).size() < 7) {
				return;
			}
			std::istringstream tmp(m_buffer.substr(i + 1, 4));
			m_width  = Utils::getVal<uint16_t>(tmp);
			m_height = Utils::getVal<uint16_t>(tmp);
			m_width = SWAP16_ON_LE(m_width);
			m_height = SWAP16_ON_LE(m_height);
			boost::format fmt2("Window size %dx%d");
			logTrace(TRACE, fmt % (fmt2 % m_width % m_height));
			m_buffer.erase(i, 7);
		} else if (doDont == 0xfb && oc == 0x1f) {
			logTrace(TRACE, fmt % "Window size updates enabled.");
		} else {
			logTrace(TRACE, fmt % "Unknown telnet command.");
		}
		m_buffer.erase(i - 2, 3);
		i = m_buffer.find(0xffu);
	}

	// Remove trailing nulls
	trim_right_if(m_buffer, __1 == 0x00);

	if (!m_buffer.size()) {
		return;
	}
	if (m_mode == MODE_NORMAL) {
		i = m_buffer.find(Socket::Endl);
		while (i != std::string::npos) {
			*m_socket << Socket::Endl;
			handleCommand(m_buffer.substr(0, i));
			m_buffer.erase(0, i + 2);
			i = m_buffer.find(Socket::Endl);
		}
		return;
	}

	// final processing - process stuff
	while (m_pos < m_buffer.size()) {
		logTrace(TRACE, "Char mode parsing:");
		if (m_buffer[m_pos] > 31 && m_buffer[m_pos] < 127) {
			logTrace(TRACE, "Normal char, echoing");
			*m_socket << std::string(1, m_buffer[m_pos++]);
		} else if (m_buffer[m_pos] == 0x7f || m_buffer[m_pos] == 0x08) {
			// 0x7f == DEL, 0x08 == BACKSPACE
			// should be handled separately, but ohwell ...
			logTrace(TRACE, "Backspace char");
			if (m_pos) {
				m_buffer.erase(--m_pos, 2);
				*m_socket << ERASE;
			} else {
				m_buffer.erase(m_pos, 1);
			}
		} else if (m_buffer[m_pos] == 0x09) { // tab
			logTrace(TRACE, "Tab char");
			if (m_pos) {
				m_buffer.erase(m_pos--, 1); // erase the tab
				*m_socket << ERASE;
			} else {
				m_buffer.erase(m_pos, 1);
			}
		} else if (m_buffer[m_pos] == 0x0d) { // EOL
			logTrace(TRACE, "EOL");
			*m_socket << Socket::Endl;
			handleCommand(m_buffer.substr(0, m_pos));
			m_buffer.erase(0, m_pos + 1);
			if (m_buffer.size() && m_buffer[0] == 0x0a) {
				m_buffer.erase(0, 1);
			}
			m_pos = 0;
		} else if (m_buffer[m_pos] == 0x0a) { // win32 eol
			logTrace(TRACE, "EOL");
			*m_socket << Socket::Endl;
			handleCommand(m_buffer.substr(0, m_pos));
			m_buffer.erase(0, m_pos + 1);
			if (m_buffer.size() && m_buffer[0] == 0x0d) {
				m_buffer.erase(0, 1);
			}
			m_pos = 0;
		} else if (
			m_buffer[m_pos] == 0x1b &&
			m_buffer.substr(m_pos,3).size() == 3
		) {
			char c = m_buffer[m_pos + 2];
			m_buffer.erase(m_pos, 3);
			if (c != 0x41 && c != 0x42) {
				continue;
			}
			if (m_commands.getHistorySize()) {
				for (uint32_t i = 0; i < m_buffer.size(); ++i) {
					*m_socket << ERASE;
				}
				if (c == 0x41) {
					m_buffer = m_commands.getPrevCommand();
				} else if (c == 0x42) {
					m_buffer = m_commands.getNextCommand();
				}
				*m_socket << m_buffer;
				m_pos = m_buffer.size();
			}
		} else {
			logDebug(
				boost::format(
					"ShellClient char-mode parser: "
					"Unexpected character %s found in "
					"buffer at offset %s. Buffer: %s"
				) % Utils::hexDump(m_buffer[m_pos])
				% Utils::hexDump(m_pos)
				% Utils::hexDump(m_buffer)
			);
			m_buffer.erase(m_pos, 1);
		}
	}
} catch (std::exception &e) {
	logDebug(boost::format("Unhandled exception in shell: %s") % e.what());
	(void)e;
	HNShell::instance().removeClient(this);
}
MSVC_ONLY(;)

void ShellClient::handleCommand(const std::string &cmd) {
	if (cmd.empty()) {
		// Tokenizer dies if we pass empty string, so return here.
		setPrompt();
		return;
	}

	LogDisabler d(this);

	m_commands.addCommand(cmd);
	Tokenizer args(cmd);
	bool ret = true;
	try {
		ret = m_commands.dispatch(args);
	} catch (std::exception &e) {
		boost::format fmt("Command '%s' returned with error: %s");
		fmt % *args.begin() % e.what();
		if (m_socket && m_socket->isConnected()) {
			*m_socket << fmt.str() << Socket::Endl;
		} else {
			logError(fmt.str());
		}
	}
	if (!ret && m_socket && m_socket->isConnected()) {
		if (m_commands.getError() == "") {
			*m_socket << "hnsh: execution failed" << Socket::Endl;
 		} else {
			*m_socket << "hnsh: " << m_commands.getError();
			*m_socket << Socket::Endl;
 		}
 	}

	// ugly hack
	if (
		*args.begin() != "exit" && *args.begin() != "shutdown" &&
		*args.begin() != "q" && m_socket && m_socket->isConnected()
	) {
		setPrompt();
 	}
}

void ShellClient::setPrompt() {
	CHECK_THROW(m_socket && m_socket->isConnected());

	*m_socket << getPrompt();
}

std::string ShellClient::getPrompt() const {
	return m_userName + "@hnsh:" + m_commands.getPathStr() + "$ ";
}

uint16_t ShellClient::getWidth() {
	return m_width;
}

void ShellClient::showLog(bool val) {
	m_showLog = val;
}

} // namespace Shell
