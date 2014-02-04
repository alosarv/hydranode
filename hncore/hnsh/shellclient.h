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
 * \file shellclient.h Interface for ShellClient class
 */

#ifndef __SHELLCLIENT_H__
#define __SHELLCLIENT_H__

#include <hnbase/event.h>
#include <hnbase/fwd.h>
#include <string>
#include <boost/tokenizer.hpp>

namespace Shell {

class HNShell;
class ShellClient;
class command_tokenizer;
typedef boost::tokenizer<command_tokenizer> Tokenizer;

}

#include "shellcommands.h"

namespace Shell {

/**
 * Command tokenizer to be used with boost::tokenizer. The main difference
 * between this tokenizer and boost::char_separator tokenizer is that this one
 * also considers non-alphanumeric characeters as part of the token. This is
 * needed for input commands parsing, which may include '?', '-' etc characters,
 * which the real-parser would certainly want to handle.
 *
 * The separator considered by this tokenizer is ' ' only. This conforms to
 * standard POSIX-styled shell commands and their arguments.
 *
 * Usage: boost::tokenizer<command_tokenizer> tok(stringtoparse);
 */
class command_tokenizer {
	char m_unescapeChar;
public:
	template<typename InputIterator, typename Token>
	bool operator()(InputIterator &curr, InputIterator &end, Token &tok);
	void reset() {}
};

//! Events emitted from ShellClient
enum ShellClientEvent {
	EVT_DESTROY = 1
};

//! Mode enumeration
enum TelnetMode {
	MODE_NORMAL = 0,       //!< Normal line-mode
	MODE_CHAR = 1          //!< character-mode
};

/**
 * ShellClient class represents a client connected to HNShell server. This class
 * handles parsing input from the client and responding to that.
 */
class ShellClient : public Trackable {
public:
	DECLARE_EVENT_TABLE(ShellClient*, ShellClientEvent);

	/**
	 * Constructor, generally called from HNShell.
	 *
	 * @param c         Pointer to connected socket to "talk" to.
	 */
	ShellClient(SocketClient *c);

	//! Destructor
	~ShellClient();

	//! Returns current window width (in characters)
	uint16_t getWidth();

	//! Write prompt
	void setPrompt();
	uint16_t getWidth() const { return m_width; }
	uint16_t getHeight() const { return m_height; }

	//! Enable/disable log messages printing
	void showLog(bool val);
private:
	//! Event handler for socket events
	void onEvent(SocketClient *c, SocketEvent evt);

	//! Generates the prompt string which should be shown to user.
	std::string getPrompt() const;

	/**
	 * Handles various commands.
	 *
	 * @param cmd         Command to be handled.
	 */
	void handleCommand(const std::string &cmd);

	/**
	 * Reads data from socket and calls handleCommand when a command is
	 * ready for processing (e.g. new-line was received).
	 */
	void readFromSocket();

	//! Handler for log messages
	void onLogMsg(const std::string &msg, MessageType t);

	//! Connected socket
	SocketClient *m_socket;

	//! User name of the user logged in
	std::string m_userName;

	//! Is true if there is an operation in progress, false otherwise.
	//! This is used to control whether to print shell after writing
	//! log messages in onLogMsg() method.
	bool m_commandInProgress;

	//! Connection to Log signals
	boost::signals::connection m_logConn;

	uint16_t m_width;  //!< Width of telnet window
	uint16_t m_height; //!< Height of telnet window

	//! Input data buffer
	std::string m_buffer;
	size_t m_pos; //!< Echo-er position in buffer

	//! Mode (can be char or line)
	TelnetMode m_mode;

	//! ShellCommands instance
	ShellCommands m_commands;

	//! Whether to print log messages to shell
	bool m_showLog;

	friend struct LogDisabler;

	/**
	 * Exception-safe implementation for enabling/disabling log messages
	 * while command is in progress.
	 */
	struct LogDisabler {
		LogDisabler(ShellClient *parent) : m_parent(parent) {
			parent->m_commandInProgress = true;
			Log::instance().disableOutput();
		}
		~LogDisabler() {
			m_parent->m_commandInProgress = false;
			Log::instance().enableOutput();
		}
		ShellClient *m_parent;
	};
};

} // namespace Shell

#endif
