/*
 *  Copywrite (C) Infinite  Adam Smith <hellfire00@sourceforge.net>
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

#include <hncore/dc/directconnect.h>

namespace DC {

const std::string TRACE = "directconnect-client";

Client::Client(IPV4Address remoteAddr) {
	m_remoteAddr = remoteAddr;

	Log::instance().enableTraceMask(TRACE);
	logTrace(TRACE, boost::format("Making new client connection to %s")
			                      % m_remoteAddr.getStr());

	m_socket = new DCSocket();
	m_socket->setHandler(this, &Client::clientHandler);
	m_socket->connect(m_remoteAddr);

	m_sending = false;
}

Client::~Client() {
	delete m_socket;
}

void Client::clientHandler(DCSocket *s, SocketEvent evt) {
	using boost::format;
	switch(evt) {
		case SOCK_CONNECTED:
			logMsg( "[dc] Client connected" );
			*s << "$MyNick " << DC::instance().getNick()
				 << "|$Lock this_wont_be_checked|";
			break;
		case SOCK_READ:
			logTrace(TRACE, "Received data on client connection");
			readSocket();
			break;
		case SOCK_ERR:
			logError("[dc] Client connection erred");
			delete this;
			break;
		case SOCK_TIMEOUT:
			logError("[dc] Client connection timed out");
			delete this;
			break;
		case SOCK_CONNFAILED:
			logError("[dc] Client connection connection failed");
			delete this;
			break;
		case SOCK_LOST:
			logMsg("[dc] Client connection lost");
			delete this;
			break;
		case SOCK_WRITE:
			if (m_sending) {
				sendBlock();
			}
			break;
		default:
			logError(
				format("[dc] Unknown socket event %p") % evt
			);
			break;
	}
}


void Client::sendBlock() {
	using namespace boost;
	if (m_sharedFile == NULL) {
		// TODO: replace this comment with client manager deletes
		logTrace(TRACE, "Shared file became NULL");
		return;
	}
	std::string sendBuf;

	// Calculate the next block size
	m_lastBlockSize = (::uint64_t) (m_lastBlockSize / float( (Utils::getTick() - m_lastSendTime) / float(1000) ));
	if (m_lastBlockSize < 51200) {
		m_lastBlockSize = 51200;
	} else if (m_lastBlockSize > 10485760) {
		m_lastBlockSize = 10485760;
	}
	m_lastSendTime = Utils::getTick();

	// Check we arn't going outside the file, if so, last block
	if (m_currentPosition + m_lastBlockSize > (m_sharedFile->getSize() - 1)) {
		m_sending = 0;
		m_lastBlockSize = (m_sharedFile->getSize() - 1) - m_currentPosition;
	} else {
		m_sending = 1;
	}

	// Buffer in the block and move the position marker
	*m_socket << m_sharedFile->read(m_currentPosition, m_currentPosition + m_lastBlockSize);
	m_currentPosition = m_currentPosition + m_lastBlockSize + 1;
}


void Client::readSocket() {
	using boost::format;
	ClientCallbackFunction funcPtr;
	std::string command, commandBuf, arguments;
	size_t endpos, argpos;
	ClientCommandMap commandMap = DC::instance().getClientCommandMap();
	ClientCommandMap::iterator function;
	std::string sendBuf;

	*m_socket >> m_buff;

	logTrace(
		TRACE, format("Client::readSocket: m_buff: %1%")
			% Utils::hexDump(m_buff)
	);

	endpos = m_buff.find("|", 0);
	while (endpos != std::string::npos) {
		commandBuf = m_buff.substr(0, endpos);
		argpos = commandBuf.find(" ");
		command = commandBuf.substr(0, argpos);
		function = commandMap.find(command);
		if (function != commandMap.end()) {
			if (argpos != std::string::npos) {
				arguments = commandBuf.substr(argpos+1);
			}
			sendBuf.append(((*function).second)(this, arguments));
		} else {
			logTrace(
				TRACE, format("Command %s not found") % command
			);
		}
		m_buff.erase(0, endpos + 1);
		endpos = m_buff.find("|", 0);
	}
	// send the sendBuf (this processes commands received in batches,
	// needed for some parts of the protocol expecting same packet replies)
	*m_socket << sendBuf;
}



std::string Client::cmd_send(std::string ) {
	if (m_fileList) {
		return DC::instance().getFileList();
	}
	m_lastBlockSize = 0;
	m_lastSendTime = Utils::getTick();
	sendBlock();
	return "";
}



std::string Client::cmd_get(std::string args) {
	using namespace boost::algorithm;
	using namespace boost::filesystem;
	using namespace boost;

	std::vector<SharedFile *> search;
	std::string pathstr;

	pathstr.append(args.substr(0, args.find("$")));
	logTrace(TRACE, format("Client requested file %s") % pathstr);

	if (pathstr == "MyList.DcLst") { // Special file
		m_fileList = true;
		return lexical_cast<std::string>(
			format("$FileLength %d|") % DC::instance().getFileList().size()
		);
	}
	m_fileList = false;

	replace_all(pathstr, "\\", "/");
	path request(pathstr);

	logTrace(TRACE, format("path leaf: %s") % request.leaf());
	search = MetaDb::instance().findSharedFile(request.leaf());
	logTrace(TRACE, format("search returned %d") % search.size());
	for (
		std::vector<SharedFile *>::iterator iter = search.begin();
		iter != search.end(); ++iter
	) {
		logTrace(TRACE, format("SharedFile: %s") % (*iter)->getPath().string());
		if ((*iter)->getPath().relative_path() == request.relative_path()) {
			logTrace(TRACE, " - found!");
			// TODO: add an EVT_DESTROY handler to handle the disappearance of this pointer
			m_sharedFile = *iter;
			m_currentPosition = lexical_cast< ::uint64_t>(args.substr(args.find("$") + 1)) - 1;

			return lexical_cast<std::string>(
				format("$FileLength %d|") % (*iter)->getSize()
			);
		}
	}

	return "$Error File not found|";

}

std::string Client::cmd_direction(std::string) {
	logTrace(TRACE, "only uploading is supported right now");
	return "$Direction Upload 666|";
}

std::string Client::cmd_key(std::string) {
	logTrace(TRACE, "we don't check keys yet, probably never will");
	return "";
}

std::string Client::cmd_lock(std::string) {
	logTrace(TRACE, "we can't pick locks yet, sending dud reply");
	return "$Key sorry__I_cant_pick_locks|";
}

std::string Client::cmd_mynick(std::string) {
	return "";
}

} // End namespace
