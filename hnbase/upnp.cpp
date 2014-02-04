/*
 *  Copyright (c) 2005-2006 Alo Sarv <madcat_@users.sourceforge.net>
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
 * \file upnp.cpp  Implementation of UPnP port forwarding API
 */


#include <hnbase/upnp.h>
#include <hnbase/sockets.h>
#include <boost/regex.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/tokenizer.hpp>

std::string TRACE_UPNP("UPnP");

namespace UPnP {

/**
 * Manager class manages UDP broadcast socket used to detect routers
 * on subnet, and also constructs Router objects as needed. It also
 * keeps an internal list of found Routers.
 *
 * This class is a Singleton.
 */
class Manager {
public:
	static Manager& instance() {
		static Manager m;
		return m;
	}
	void findRouters(
		boost::function<void (boost::shared_ptr<Router>)> handler
	);
private:
	/**
	 * @name Singleton pattern
	 */
	//@{
	Manager();
	~Manager();
	Manager(const Manager&);
	Manager& operator=(const Manager&);
	//@}

	friend class Router;

	/**
	 * UDP broadcast socket event handler
	 */
	void onSocketEvent(UDPSocket *s, SocketEvent evt);

	/**
	 * Handles answers via UDP from routers, and constructs Router
	 * object based on this data.
	 *
	 * @param data       Data the router sent
	 * @param from       The location where the data was sent from
	 */
	void handleRouterAnswer(std::string data, IPV4Address from);

	//! Broadcast socket
	UDPSocket *m_socket;

	//! User-defined event handler for newly found routers
	boost::function<void (boost::shared_ptr<Router>)> m_handler;

	//! All found routers
	std::vector<boost::shared_ptr<Router> > m_routers;
};

void findRouters(boost::function<void (boost::shared_ptr<Router>)> handler) {
	Manager::instance().findRouters(handler);
}

Manager::Manager() : m_socket(new UDPSocket) {
	m_socket->setHandler(boost::bind(&Manager::onSocketEvent, this,_1,_2));
	m_socket->setBroadcast(true);
	m_socket->listen(IPV4Address(0, 12245));
}

Manager::~Manager() {}

void Manager::findRouters(
	boost::function<void (boost::shared_ptr<Router>)> handler
) {
	m_handler = handler;
	std::string toSend(
		"M-SEARCH * HTTP/1.1\r\n"
		"Host:239.255.255.250:1900\r\n"
		"ST:upnp:rootdevice\r\n"
		"Man:\"ssdp:discover\"\r\n"
		"MX:3\r\n\r\n\r\n"
	);
	m_socket->send(toSend, IPV4Address("239.255.255.250", 1900));
	logTrace(TRACE_UPNP, boost::format("Broadcasting:\n%s") % toSend);
	m_routers.clear();
}

void Manager::onSocketEvent(UDPSocket *sock, SocketEvent evt) {
	if (evt == SOCK_READ) {
		char buf[1024];
		IPV4Address from;
		int ret = m_socket->recv(buf, 1024, &from);
		handleRouterAnswer(std::string(buf, ret), from);
	}
}

void Manager::handleRouterAnswer(std::string data, IPV4Address from) {
	logTrace(TRACE_UPNP, boost::format("Broadcast response:\n%s") % data);
	boost::algorithm::to_lower(data);
	boost::regex r1("location:\\s+http://(\\S+):(\\S+)/(\\S+)");
	boost::regex r2("location:http://(\\S+):(\\S+)/(\\S+)"); // w/o spaces
	boost::match_results<const char*> m;
	boost::char_separator<char> sep("\r\n");
	boost::tokenizer<boost::char_separator<char> > tok(data, sep);
	boost::tokenizer<boost::char_separator<char> >::iterator i(tok.begin());
	while (i != tok.end()) {
		bool ret = boost::regex_match((*i).c_str(), m, r1);
		if (!ret) {
			ret = boost::regex_match((*i).c_str(), m, r2);
		}
		if (ret) {
			IPV4Address ip(
				m[1], boost::lexical_cast<uint16_t>(m[2])
			);
			boost::shared_ptr<Router> p(new Router(ip, m[3]));
			m_routers.push_back(p);
		}
		++i;
	}
}

Router::Connection::Connection(SocketClient *s) : m_sock(s) {}
Router::Connection::~Connection() { m_sock->destroy(); }

Router::Router(IPV4Address ip, const std::string &loc) : m_ip(ip) {
	boost::format fmt(
		"GET /%s HTTP/1.1\r\n"
		"HOST: %s\r\n"
		"ACCEPT-LANGUAGE: en\r\n\r\n\r\n"
	);
	sendRequest((fmt % loc % ip).str());
}

void Router::sendRequest(const std::string &data) {
	Connection *c = new Connection(new SocketClient);
	c->m_sock->setHandler(
		boost::bind(&Router::onSocketEvent, this, _1, _2)
	);
	c->m_sock->connect(m_ip);
	c->m_outBuffer.append(data);
	m_sockets[c->m_sock] = c;
	logTrace(TRACE_UPNP, boost::format("Sending request:\n%s") % data);
}

void Router::sendData() {
	for (Iter i = m_sockets.begin(); i != m_sockets.end(); ++i) {
		Connection *c = (*i).second;
		if (c->m_outBuffer.size()) {
			uint32_t ret = c->m_sock->write(
				c->m_outBuffer.data(), c->m_outBuffer.size()
			);
			logTrace(
				TRACE_UPNP,
				boost::format("Sending to %s:\n%s")
				% c->m_sock->getPeer()
				% c->m_outBuffer.substr(0, ret)
			);
			if (ret < c->m_outBuffer.size()) {
				c->m_outBuffer.erase(0, ret);
			} else {
				c->m_outBuffer.clear();
			}
		}
	}
}

void Router::onSocketEvent(SocketClient *sock, SocketEvent event) {
	if (event == SOCK_CONNECTED || event == SOCK_WRITE) {
		m_internalIp = sock->getAddr();
		sendData();
	} else if (event == SOCK_READ) {
		std::string buf;
		*sock >> buf;

		Iter i = m_sockets.find(sock);
		assert(i != m_sockets.end());
		Connection *c = (*i).second;

		logTrace(
			TRACE_UPNP,
			boost::format("Received data from router:\n%s") % buf
		);

		c->m_inBuffer.append(buf);
		boost::algorithm::to_lower(buf);
		c->m_inBufferLower.append(buf);

		parseBuffer(c);
	} else if (
		event == SOCK_LOST ||
		event == SOCK_ERR ||
		event == SOCK_CONNFAILED ||
		event == SOCK_TIMEOUT
	) {
		logTrace(
			TRACE_UPNP,
			boost::format("Connection lost to %s") % sock->getPeer()
		);
		Iter i = m_sockets.find(sock);
		assert(i != m_sockets.end());
		delete (*i).second;
		m_sockets.erase(i);
	}
}

void Router::parseBuffer(Connection *c) {
	if (!c->m_inBuffer.size()) {
		return;
	}
	if (!m_name.size()) {
		size_t pos1 = c->m_inBufferLower.find("<modeldescription>");
		size_t pos2 = c->m_inBufferLower.find("</modeldescription>");
		if (pos1 != std::string::npos && pos2 != std::string::npos) {
			m_name = c->m_inBuffer.substr(
				pos1 + 18, pos2 - pos1 - 18
			);
		}
	}
	if (!m_curl.size()) {
		size_t pos = c->m_inBufferLower.find(
			"<servicetype>"
			"urn:schemas-upnp-org:service:wanipconnection:1"
			"</servicetype>"
		);
		if (pos != std::string::npos) {
			size_t cur1 = c->m_inBufferLower.find(
				"<controlurl>", pos
			);
			if (cur1 != std::string::npos) {
				size_t cur2 = c->m_inBufferLower.find(
					"</controlurl>", cur1
				);
				if (cur2 != std::string::npos) {
					m_curl = c->m_inBuffer.substr(
						cur1 + 12, cur2 - cur1 - 12
					);
				}
			}
		}
	}
	if (c->m_inBufferLower.find("</root>") != std::string::npos) {
		Iter i = m_sockets.find(c->m_sock);
		assert(i != m_sockets.end());
		m_sockets.erase(c->m_sock);
		delete c;
		Manager::instance().m_handler(shared_from_this());
	}
}

void Router::addForwarding(
	Protocol prot, uint16_t ePort, uint16_t iPort,
	const std::string &reason, uint32_t leaseTime
) {
	boost::format doForward_content(
		"<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org"
		"/soap/envelope/\" "
		"s:encodingStyle=\"http://schemas.xmlsoap.org"
		"/soap/encoding/\">\r\n"
		" <s:Body>\r\n"
		" <u:AddPortMapping xmlns:u=\"urn:schemas-upnp-org"
		":service:WANIPConnection:1\">\r\n"
		" <NewRemoteHost>%s</NewRemoteHost>\r\n"
		" <NewExternalPort>%d</NewExternalPort>\r\n"
		" <NewProtocol>%s</NewProtocol>\r\n"
		" <NewEnabled>%d</NewEnabled>\r\n"
		" <NewInternalClient>%s</NewInternalClient>\r\n"
		" <NewInternalPort>%d</NewInternalPort>\r\n"
		" <NewPortMappingDescription>%s</NewPortMappingDescription>\r\n"
		" <NewLeaseDuration>%d</NewLeaseDuration>\r\n"
		" </u:AddPortMapping>\r\n"
		" </s:Body>\r\n"
		"</s:Envelope>\r\n\r\n\r\n"
	);

	doForward_content % "";                  // remote host, blank for any
	doForward_content % ePort;               // external port
	doForward_content % (prot == TCP ? "TCP" : "UDP");  // protocol
	doForward_content % 1;                         // enabled/disabled
	doForward_content % m_internalIp.getAddrStr(); // internal ip
	doForward_content % (iPort ? iPort : ePort);   // internal port
	doForward_content % reason;                    // description
	doForward_content % leaseTime;                 // lease duration

	boost::format doForward_header(
		"POST %s HTTP/1.1\r\n"
		"HOST: %s\r\n"
		"CONTENT-LENGTH: %d\r\n"
		"CONTENT-TYPE: text/xml; charset=\"utf-8\"\r\n"
		"SOAPACTION: \"urn:schemas-upnp-org:service:"
		"WANIPConnection:1#AddPortMapping\""
		"\r\n\r\n"
	);
	doForward_header % m_curl;                          // control url
	doForward_header % m_ip;                            // ip address
	doForward_header % doForward_content.str().size();  // content length

	sendRequest(doForward_header.str() + doForward_content.str());
}

void Router::removeForwarding(Protocol prot, uint16_t ePort) {
	boost::format doRemove_content(
		"<s:Envelope xmlns:"
		"s=\"http://schemas.xmlsoap.org/soap/envelope/\""
		"s:encodingStyle="
		"\"http://schemas.xmlsoap.org/soap/encoding/\">"
		"<s:Body>"
		"<u:DeletePortMapping xmlns:"
		"u=\"urn:schemas-upnp-org:service:WANIPConnection:1\">"
		" <NewRemoteHost>%1%</NewRemoteHost>"
		" <NewExternalPort>%2%</NewExternalPort>"
		" <NewProtocol>%3%</NewProtocol>"
		"</u:DeletePortMapping>"
		"</s:Body>"
		"</s:Envelope>"
		"\r\n\r\n\r\n"
	);
	doRemove_content % "";                             // external ip
	doRemove_content % ePort;                          // external port
	doRemove_content % (prot == TCP ? "TCP" : "UDP");  // protocol

	boost::format doRemove_header(
		"POST %s HTTP/1.1\r\n"
		"HOST: %s\r\n"
		"CONTENT-LENGTH: %d\r\n"
		"CONTENT-TYPE: text/xml; charset=\"utf-8\"\r\n"
		"SOAPACTION: \"urn:schemas-upnp-org:service:"
		"WANIPConnection:1#DeletePortMapping\""
		"\r\n\r\n"
	);
	doRemove_header % m_curl;                         // control url
	doRemove_header % m_ip;                           // ip address
	doRemove_header % doRemove_content.str().size();  // content length

	sendRequest(doRemove_header.str() + doRemove_content.str());
}

} // end namespace UPnP

