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

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#include <hnbase/object.h>
#include <hnbase/log.h>
#include <boost/lexical_cast.hpp>

// Root -> singleton
class Root : public Object {
public:
	static Root& instance() {
		static Root r;
		return r;
	}
private:
	Root() : Object(0, "Root") {
		logTrace("test-object", "Constructing Root.");
	}
	~Root() {
		logTrace("test-object", "Destructing Root.");
	}
	Root(Root&);
	Root& operator=(const Root&);
};

// Modules singleton
class Modules : public Object {
public:
	static Modules& instance() {
		static Modules m;
		return m;
	}
public:
	Modules() : Object(&Root::instance(), "Modules") {
		logTrace("test-object", "Constructing Modules.");
	}
	~Modules() {
		logTrace("test-object", "Destructing Modules.");
	}
};

// Abstract -> Derive specific modules from here
class Module : public Object {
public:
	Module(const std::string &name) : Object(&Modules::instance(), name) {
		logTrace("test-object", boost::format("Constructing Module %s") % name);
	}
	virtual ~Module() {};
};
// ED2K module entry point/singleton
class ED2K : public Module {
public:
	static ED2K& instance() {
		static ED2K ed2k;
		return ed2k;
	}
private:
	ED2K() : Module("ED2K") {
		logTrace("test-object", "Constructing ED2K");
	}
	~ED2K() {
		logTrace("test-object", "Destructing ED2K");
	}
};

// Another singleton - ServerList
class ServerList : public Object {
public:
	static ServerList& instance() {
		static ServerList s;
		return s;
	}
private:
	ServerList() : Object(&ED2K::instance(), "ServerList") {
		logTrace("test-object", "Constructing ServerList");
	}
	~ServerList() {
		logTrace("test-object", "Destructing ServerList");
	}
};

class Server : public Object {
public:
	// Construction
	Server(const std::string &ip, uint32_t port, const std::string &name)
	: Object(&ServerList::instance(), name), m_name(name), m_ip(ip),
	m_port(port) {
		logTrace(
			"test-object",
			boost::format("Constructing Server(%s, %s, %d)")
			% name % ip % port
		);
	}

	// Getters
	std::string getName() const { return m_name; }
	std::string getIp() const { return m_ip; }
	uint32_t getPort() const { return m_port; }

	// Setters
	void setName(const std::string &name) {
		m_name = name;
		notify(getId(), OBJ_MODIFIED);
	}
	void setIp(const std::string &ip) {
		m_ip = ip;
		notify(getId(), OBJ_MODIFIED);
	}
	void setPort(uint32_t port) {
		m_port = port;
		notify(getId(), OBJ_MODIFIED);
	}

	// Override Object virtuals to give user access to members and operations
	// Getters
	virtual uint8_t getDataCount() const { return 3; }
	virtual std::string getData(uint8_t num) const {
		switch (num) {
			case 0: return m_ip;
			case 1: return boost::lexical_cast<std::string>(m_port);
			case 2: return m_name;
			default: return std::string();
		}
	}
	virtual std::string getFieldName(uint8_t num) const {
		switch (num) {
			case 0: return "IP";
			case 1: return "Port";
			case 2: return "Name";
			default: return std::string();
		}
	}
	virtual uint8_t getOperCount() const { return 2; }
	virtual std::string getOper(uint8_t n) {
		switch (n) {
			case 0: return "Connect";
			case 1: return "Static";
			default: return std::string();
		}
	}

	// Setters
	virtual void doOper(uint8_t oper) {
		switch (oper) {
			case 0: logMsg(boost::format("Connecting to %s.") % m_name);
				break;
			case 1: logMsg(boost::format("Making %s static.") % m_name);
				break;
			default: logDebug(boost::format("Unsupporter oper %d") % oper);
				break;
		}
	}
	virtual void setData(uint8_t num, const std::string &data) {
		switch (num) {
			case 0: m_ip = data; break;
			case 1: m_port = boost::lexical_cast<uint32_t>(data); break;
			case 2: m_name = data; break;
			default: break;
		}
	}
private:
	friend class ServerList;
	// Only allowed by ServerList
	~Server() {
		logTrace("test-object", "Destructing Server");
	}

	// Data
	std::string m_name;
	std::string m_ip;
	uint32_t m_port;

};

void print(Object *obj) {
	Log::instance().addPreStr(obj->getName() + "->");
	if (obj->getChildCount() > 0) {
		for (Object::CIter i = obj->begin(); i != obj->end(); ++i) {
			print((*i).second);
		}
	}
	if (obj->getDataCount() > 0) {
		std::string header;
		std::string data;
		for (uint8_t i = 0; i < obj->getDataCount(); ++i) {
			logMsg(boost::format("[%s] = %s") % obj->getFieldName(i) % obj->getData(i));
		}
	}
	Log::instance().remPreStr(obj->getName() + "->");
}

int main() {
	Log::instance().addTraceMask("test-object");
	Log::instance().enableTraceMask("test-object");
	logTrace("test-object", "Initializing Object Test sequence.");
	new Server("127.0.0.1", 4662, "localhost");
	new Server("62.65.192.1", 3532, "host");
	new Server("128.123.532.23", 1653, "unknown");

	print(&Root::instance());

	logMsg(boost::format("sizeof(Object) = %1%") % sizeof(Object));
}

#endif

