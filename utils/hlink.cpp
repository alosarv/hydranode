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

 #if defined(WIN32) || defined(_WIN32)
	#include <winsock2.h>
	typedef SOCKADDR sockaddr;
	typedef int socklen_t;
	#define close(fd) closesocket(fd)
#else
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <sys/un.h>
	#include <fcntl.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <errno.h>
#endif
#include <string>
#include <iostream>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/replace.hpp>

int processArguments(int argc, char *argv[]) {
	using boost::lexical_cast;
#if defined(WIN32) || (_WIN32)
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2,2), &wsaData) != NO_ERROR) {
		return -1;
	}
#endif

	if (argc == 1) {
		std::cerr << "Usage: hlink <link> --ip=ADDRESS --port=PORT\n";
		return 0;
	}

	long addr = inet_addr("127.0.0.1");
	short port = htons(9999);
	if (argc > 2) {
		for (int i = 2; i < argc; ++i) {
			std::string arg(argv[i]);
			if (arg.substr(0, 5) == "--ip=") {
				addr = inet_addr(arg.substr(5).c_str());
			} else if (arg.substr(0, 7) == "--port=") {
				port = lexical_cast<short>(arg.substr(7));
				port = htons(port);
			} else {
				std::cerr << "Unknown arg: " << arg << "\n";
			}
		}
	}
	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == -1) {
		std::cerr << "Unable to create a socket.\n";
		return -1;
	}

	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = addr;
	sin.sin_port = port;

	int ret = connect(sock, (sockaddr*)&sin, sizeof(sin));

	if (ret == -1) {
		std::cerr << "Unable to connect to Hydranode.\n";
		return -1;
	}

	std::string toSend(argv[1]);
	boost::algorithm::replace_all(toSend, "%7C", "|");
	// all backslashes must be escaped twice since hnsh parser eats them
	// up otherwise.
	boost::algorithm::replace_all(toSend, "\\", "\\\\\\\\");
	std::cerr << "Sending link '" << toSend << "'" << std::endl;
	toSend.insert(0, "do ");
	toSend.append("\r\n");

	ret = send(sock, toSend.data(), toSend.size(), 0);

	if (ret == -1) {
		std::cerr << "Send() call failed.\n";
		return -1;
	}
	if (ret == static_cast<int>(toSend.size())) {
		std::cerr << "Sent link to Hydranode\n";
	}

	char buf[1024];
	ret = recv(sock, buf, 1024, 0);
	if (ret > 0) {
		buf[ret] = 0;
		std::cerr << buf << std::endl;
	}
	close(sock);
	return 0;
}

/*
 * Small application that sends links to Hydranode via connecting to
 * Hydranode shell.
 *
 * Optionally, ip and port can be passed from commandline.
 */
int main(int argc, char *argv[]) {
	return processArguments(argc, argv);
}
