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

IMPLEMENT_MODULE(DC);

const std::string TRACE = "directconnect";


bool DC::onInit() {
	logMsg("Starting dc module...");
	Log::instance().enableTraceMask("directconnect");
	// WARNING: This trace will enable ALOT of debug data
	//          from the compression routines.
	//Log::instance().enableTraceMask("directconnect-crypto");
	logTrace(TRACE, "Trace logging enabled...");

	m_socket = new DCSocket();

	buildClientCommandMap();
	buildServerCommandMap();
	buildFileList();

	return true;
}



int DC::onExit() {
	// Clean up...
	return 0;
}


void DC::buildClientCommandMap() {
	logTrace(TRACE, "Building Client command map");
	clientCommandMap["$MyNick"]    = &Client::cmd_mynick;
	clientCommandMap["$Lock"]      = &Client::cmd_lock;
	clientCommandMap["$Key"]       = &Client::cmd_key;
	clientCommandMap["$Direction"] = &Client::cmd_direction;
	clientCommandMap["$Get"]       = &Client::cmd_get;
	clientCommandMap["$Send"]      = &Client::cmd_send;
}



void DC::buildServerCommandMap() {
	logTrace(TRACE, "Building Server command map");
	serverCommandMap["$Lock"]        = &DC::cmd_lock;
	serverCommandMap["$HubName"]     = &DC::cmd_hubname;
	serverCommandMap["$ConnectToMe"] = &DC::cmd_connecttome;
}



void DC::buildFileList() {
	using namespace std;
	using namespace boost::algorithm;
	using namespace boost::filesystem;
	using boost::lexical_cast;
	using boost::format;

	logTrace(TRACE, "Building filelist...");
	FileMap files;
	vector<string> pathParts;
	string tabs;
	uint64_t totalSize;

	Utils::StopWatch s;
	totalSize = 0; // make sure this is 0 before we start calc :P
	for (
		FilesList::SFIter i = FilesList::instance().begin();
		i != FilesList::instance().end(); ++i
	) {
		/*if (files.find((*i)->getLocation()) != files.end()) {
			files.insert((*i)->getLocation());
		}*/
		files[(*i)->getLocation()].push_back(lexical_cast<string>(format("%s|%d") % (*i)->getName() % (*i)->getSize()));
		totalSize += (*i)->getSize(); // calculate shared total
	}
	fileTotal = lexical_cast<string>(totalSize);

	for (FileMap::iterator map = files.begin(); map != files.end(); map++) {
		// Per directory stuff
		split(pathParts, (*map).first, is_any_of("/"));

		for (
			vector<string>::iterator dir = pathParts.begin();
			dir != pathParts.end(); dir++
		) {
			if (*dir != "") {
				fileList.append(tabs);
				fileList.append(*dir);
				fileList.append("\r\n");
				tabs.append("\t");
			}
		}

		for (
			vector<string>::iterator vector = (*map).second.begin();
			vector != (*map).second.end(); vector++
		) {
			fileList.append(tabs);
			fileList.append(*vector);
			fileList.append("\r\n");
		}
		tabs.clear();
	}

	/*
	for (FileMap::iterator map = files.begin(); map != files.end(); map++) {
		// For every path
		boost::filesystem::path curPath((*map).first);
		for (path::iterator p = curPath.begin(); p != curPath.end(); p++) {
			if (*p != "/") { // ignore root (/ should only be the first element

			}

			logMsg(boost::format("path element: %s") % *p);
		}
	}
	*/
	logTrace(
		TRACE, format("fileList built in %dms, size = %d")
			% s % fileList.size()
	);
	(void)s; // suppress awrning
	fileList = Crypto::huffmanEncode(fileList);
}





/*
 * This does the equivilent of what the *nix tools sort | uniq do on the command line
 */
DC::VecStr DC::sortuniq(DC::VecStr input) {
	VecStr output;
	bool found = false;
	for (VecStr::iterator ic = input.begin(); ic != input.end(); ic++) {
		for (
			VecStr::iterator oc = output.begin();
			oc != output.end(); oc++
		) {
			if ((*oc) == (*ic)) {
				found = true;
				break;
			}
		}
		if (found == false) {
			output.push_back(*ic);
		}
	}
	return output;
}



uint8_t DC::getOperCount() const {
	return 3;
}



Object::Operation DC::getOper(uint8_t n) const {
	std::string name;
	bool hasArgs = false;
	switch (n) {
		case 0:
			name = "connect";
			hasArgs = true;
			break;

		case 1:
			name = "disconnect";
			hasArgs = false;
			break;

		case 2:
			name = "nickname";
			hasArgs = true;
			break;

	}
	Operation op(name, hasArgs);
	switch (n) {
		case 0:
			op.addArg(Operation::Argument("server", true, ODT_STRING));
			op.addArg(Operation::Argument("password", false, ODT_STRING));
			break;

		case 2:
			op.addArg(Operation::Argument("nickname", true, ODT_STRING));
			break;
	}
	return op;
}



void DC::doOper(const Operation &o) {
	using boost::format;
	logTrace(
		TRACE, format("doOper called with %s") % o.getName()
	);
	std::string name = o.getName();
	if (name == "connect") {
		std::string server = o.getArg("server").getValue();
		connect(server);
	} else if (name == "disconnect") {
		disconnect();
	} else if (name == "setnick") {

	}
}



void DC::connect(std::string server) {
	if (m_socket->isConnected()) {
		logError("Already connected to hub, disconnect first");
		return;
	}
	// Check we have a nickname set, or fall back to default
	if (m_nick == "") {
		logTrace(TRACE, "Falling back to default nickname");
		m_nick = "Hydranode";
	}
	if (m_email == "") {
		logTrace(TRACE, "Falling back to default email");
		m_email = "user@example.org";
	}
	if (m_desc == "") {
		logTrace(TRACE, "Falling back to default description");
		m_desc = "DirectConnect Module for Hydranode";
	}
	// connect finally :P
	m_socket->setHandler(this, &DC::serverHandler);
	m_socket->connect(IPV4Address(server, 411));
}



void DC::disconnect() {
	logTrace(TRACE, "disconnecting socket and reinitialising the pointer");
	// Hopefully this does as I hope :/
	m_socket->disconnect();
	delete m_socket;
	m_socket = new DCSocket();
}



void DC::serverHandler(DCSocket *s, SocketEvent evt) {
	switch(evt) {
		case SOCK_CONNECTED:
			logMsg( "[dc] Server connected" );
			sendLogin(s);
			break;
		case SOCK_READ:
			logTrace(TRACE, "Received data on server connection");
			readSocket();
			break;
		case SOCK_ERR:
			logError("[dc] Server connection has errored :/");
			break;
		case SOCK_TIMEOUT:
			logError("[dc] Server connection timed out");
			break;
		case SOCK_CONNFAILED:
			logError("[dc] Server connection failed");
			break;
		case SOCK_LOST:
			logError("[dc] Server connection lost");
			break;
		case SOCK_WRITE:
			break;
		default:
			logError(
				boost::format("[dc] Unknown socket event %p in serverHandler") % evt
			);
			break;
	}
}



void DC::sendLogin(DCSocket *c) {
	logTrace(TRACE, "Sending login info");
	*c << "$ValidateNick " << m_nick << "|";
	*c << "$MyINFO $ALL " << m_nick << " " << m_desc
		<< "$ $LAN(T3) $" << m_email << "$" << fileTotal << "$|";
	// FIXME: make a setting for speed
}



void DC::readSocket() {
	using namespace boost::algorithm;
	using boost::format;
	size_t endpos, argpos;
	std::string commandBuf, sendBuf, command, arguments;
	ServerCommandMap commandMap = serverCommandMap;
	ServerCommandMap::iterator function;

	// Read in the data off the socket
	*m_socket >> m_buff;

	logTrace(
		TRACE, format("readSocket: buff: %1%") % Utils::hexDump(m_buff)
	);

	endpos = m_buff.find("|", 0);
	while (endpos != std::string::npos) {
		commandBuf = m_buff.substr(0, endpos);
		if (commandBuf.substr(0,1) != "$") {
			m_buff.erase(0, endpos + 1);
			endpos = m_buff.find("|", 0);
			continue;
		}
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
	*m_socket << sendBuf;
}



std::string DC::cmd_lock(std::string) {
	return "$Key i_cant_pick_locks";
}



std::string DC::cmd_hubname(std::string args) {
	logMsg(boost::format("Connected to hub: %s") % args);
	return "";
}



std::string DC::cmd_connecttome(std::string args){
	std::string ip;
	uint16_t port;
	size_t space, colon;
	space = args.find(" ");
	colon = args.find(":");
	ip = args.substr(space + 1, colon - (space + 1));
	port = boost::lexical_cast<uint16_t>(args.substr(colon + 1));
	new Client(IPV4Address(ip, port));
	return "";
}

} // End namespace
