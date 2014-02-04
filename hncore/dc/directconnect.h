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

#include <hncore/modules.h>
#include <hncore/fileslist.h>
#include <hncore/metadb.h>
#include <hnbase/log.h>
#include <hnbase/object.h>
#include <hnbase/ssocket.h>
#include <boost/function.hpp>
#include <boost/dynamic_bitset.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/algorithm/string.hpp>

namespace DC {

class DC;
class Client;

class DC : public ModuleBase {
	DECLARE_MODULE(DC, "dc");
public:
	bool onInit();
	int onExit();

	virtual uint8_t getOperCount() const;
	virtual Object::Operation getOper(uint8_t n) const;
	virtual void doOper(const Operation &o);

	typedef SSocket<DC, Socket::Client, Socket::TCP> DCSocket;
	typedef std::map<std::string, std::vector<std::string> > FileMap;
	typedef std::vector<std::string> VecStr;

	void connect(std::string server);
	void disconnect();
	void serverHandler(DCSocket *s, SocketEvent evt);
	void readSocket();
	void sendLogin(DCSocket *c);

	std::string m_nick;
	std::string m_desc;
	std::string m_email;
	DCSocket* m_socket;
	std::string m_buff;

	std::string getNick() { return m_nick; }

	typedef boost::function<std::string (Client*, std::string)> ClientCallbackFunction;
	typedef std::map<std::string, ClientCallbackFunction> ClientCommandMap;
	typedef boost::function<std::string (DC*, std::string)> ServerCallbackFunction;
	typedef std::map<std::string, ServerCallbackFunction> ServerCommandMap;

	ClientCommandMap clientCommandMap;
	void buildClientCommandMap();
	ClientCommandMap getClientCommandMap() { return clientCommandMap; }
	ServerCommandMap serverCommandMap;
	void buildServerCommandMap();
	ServerCommandMap getServerCommandMap() { return serverCommandMap; }

	// Callback functions
	std::string cmd_lock(std::string args);
	std::string cmd_hubname(std::string args);
	std::string cmd_connecttome(std::string args);


	std::string fileList;
	std::string fileTotal;
	void buildFileList();
	std::string getFileList() { return fileList; }
	std::string getFileTotal() { return fileTotal; }

	VecStr sortuniq(VecStr input);
};




// This class holds all the client to client related stuff in client.cpp
class Client {
public:
	Client(IPV4Address remoteAddr);
	~Client();


	typedef SSocket<DC, Socket::Client, Socket::TCP> DCSocket;


	void clientHandler(DCSocket *s, SocketEvent evt);
	void readSocket();
	void sendBlock();

	typedef boost::function<std::string (Client*, std::string)> ClientCallbackFunction;
	typedef std::map<std::string, ClientCallbackFunction> ClientCommandMap;

	// Callback functions
	std::string cmd_send(std::string args);
	std::string cmd_get(std::string args);
	std::string cmd_direction(std::string args);
	std::string cmd_key(std::string args);
	std::string cmd_lock(std::string args);
	std::string cmd_mynick(std::string args);

	IPV4Address m_remoteAddr;

	DCSocket* m_socket;
	std::string m_buff;

	// File transfers
	SharedFile* m_sharedFile;
	bool m_sending;
	bool m_fileList;
	uint64_t m_lastSendTime;
	uint64_t m_lastBlockSize;
	uint64_t m_currentPosition;

};



// This stuff is from DC++, thanks guys, I'm no good at algorithms
// I'm retyping this stuff though to learn :) I think I'm understanding it
namespace Crypto {
	class Node;

	typedef std::map<char, boost::dynamic_bitset<> > encodeMap;
	std::string huffmanEncode(std::string input);
	void encodingTable(Node* currentNode, encodeMap* encodingMap, boost::dynamic_bitset<> currentCode);

	class Node {
		public:
		Node* left;
		Node* right;
		uint32_t weight;
		char chr;


		Node(char aChr, int aWeight) {
			chr = aChr;
			weight = aWeight;
			left = NULL;
			right = NULL;
		};

		Node(Node* aLeft, Node* aRight) {
			chr = -1;
			weight = aLeft->weight + aRight->weight;
			left = aLeft;
			right = aRight;
		};

		~Node() {
			delete left;
			delete right;
		}

		bool operator <(const Node& rhs) const {
			return weight<rhs.weight;
		}
		bool operator >(const Node& rhs) const {
			return weight>rhs.weight;
		}
		bool operator <=(const Node& rhs) const {
			return weight<=rhs.weight;
		}
		bool operator >=(const Node& rhs) const {
			return weight>rhs.weight;
		}
	};


} // Crypto namespace End
} // DC namespace End
