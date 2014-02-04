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

#ifndef __FTP_H__
#define __FTP_H__

/**
 * \file ftp.h    Interface for Hydranode FTP module
 */

/**
 * \mainpage Hydranode FTP Module
 * \author Alo Sarv
 * \date April 5th, 2005
 *
 * Ftp module in Hydranode provides simple way of accessing remote FTP servers,
 * setting up our own FTP server, and transfering data between them.
 *
 * Outgoing FTP connections are implemented using Session class. Our own FTP
 * server is not implemented yet at this time.
 *
 * \todo DNS lookups; problem is, we need async dns lookups in core...
 * \todo Actual data transfer using PartData API
 * \todo FTP server, offering files found in FilesList API
 * \todo Directories navigation; we need to build a local virtual tree (using
 *       Directory class), and navigate on that. The parsing must be done from
 *       "list" command output, which is kinda verbose and crappy to parse...
 *       We also want to cache the directories for later, for searching
 *       capabilities - once a user has visited an FTP, he can "cache" the
 *       contents of the server, and later (when he's not even connected), the
 *       contents would still show up in search results.
 * \todo Figure out what to do with the data-sockets. Basically, every time we
 *       send PASV, we get a new data socket, to which our next data request is
 *       sent. What we want is to be able to do multiple transfers from same
 *       server simultaneously, so we need a way to keep track of which sockets
 *       are doing what, and why, and where. This all should be managed by
 *       session class, but since everything there works in non-linear manner,
 *       state variables quickly start becoming painful ...
 */

#include <hncore/modules.h>
#include <hncore/fwd.h>
#include <hnbase/ipv4addr.h>

namespace Ftp {

class FtpModule;
class Session;
class Directory;

typedef SSocket<FtpModule, Socket::Client, Socket::TCP> FtpSocket;
typedef SSocket<FtpModule, Socket::Server, Socket::TCP> FtpServer;

/**
 * Main class of Ftp module, provides user interface via Object::Arguments,
 * and keeps map of ongoing Ftp sessions.
 */
class FtpModule : public ModuleBase {
	DECLARE_MODULE(FtpModule, "ftp");
public:
	/**
	 * Called on module init
	 */
	bool onInit();

	/**
	 * Called on module exit
	 */
	int onExit();

	/**
	 * Connect to a specific ftp server
	 *
	 * @param url       Fully qualified ftp:// url
	 * @param username  Username to log into server with
	 * @param password  Password to log into server
	 * @returns         Session object representing the connection
	 */
	Session* connect(
		const std::string &url,
		const std::string &username = "anonymous",
		const std::string &password = "hydranode@localhost"
	);
private:
	/**
	 * Add a supported operation to this object
	 *
	 * @param op      Operation to be added
	 */
	void addOperation(const Operation &op);

	//! List of operations that can be performed
	std::vector<Operation> m_ops;

	virtual void doOper(const Operation &op);
	virtual uint8_t getOperCount() const;
	virtual Operation getOper(uint8_t n) const;

	typedef std::map<std::string, Session*> SessionMap;
	typedef SessionMap::iterator SIter;

	//! Map of currently ongoing sessions
	SessionMap m_sessions;
};

/**
 * Session object indicates one Ftp session with a remote peer. Event handling
 * is implemented via signals.
 *
 * Upon first request to dirlist, Session will fetch the dir-list from server,
 * and populate m_rootDir with the fields. Directory traversing can be
 * performed via chDir() function (altough it should be changed to
 * Object::Operation in Directory class instead).
 */
class Session : public Object {
public:
	/**
	 * Construct a session, contact a specific ftp server and log in.
	 *
	 * @param url           Fully-qualified ftp:// url to server
	 * @param username      Username to log into server with
	 * @param password      Password to log into server with
	 */
	Session(
		const std::string &url,
		const std::string &username = "anonymous",
		const std::string &password = "hydranode@localhost"
	);

	IPV4Address getAddr() const { return m_addr; }

	/**
	 * Change current working directory
	 *
	 * @param path      New location; can be absolute or relative
	 */
	void chDir(const std::string &path);

	boost::signal<void (Session&)> onConnected;
	boost::signal<void (Session&, int)> onError;

	//! Comparison operator for std containers
	bool operator<(const Session &sess) {
		return getName() < sess.getName();
	}
private:
	Session();                             //!< Forbidden
	Session(const Session&);               //!< Forbidden
	Session& operator=(const Session&);    //!< Forbidden

	//! Attempt to establish connection with server
	void connect();

	//! Event handler for control socket events
	void onSocketEvent(FtpSocket *sock, SocketEvent event);

	//! Event handler for data socket events
	void onDataEvent(FtpSocket *sock, SocketEvent event);

	//! Attempt to parse m_buffer and act accordingly
	bool parseBuffer();

	//! Log into the server
	void init();

	/**
	 * Parses 227 line to aquire address for passive connections
	 *
	 * @param info    227 Entering Passive Mode ... line from server log
	 */
	void getPassiveAddr(const std::string &info);

	/**
	 * Request data from server
	 *
	 * @param msg     Message sent to control socket
	 */
	void reqData(const std::string &msg);

	IPV4Address m_addr;        //!< Address where this session connects to
	IPV4Address m_dataAddr;    //!< Used for data connections
	std::string m_serverName;  //!< Name of the server (e.g. aaa.bbb.ccc)
	std::string m_serverPath;  //!< Current full path on server

	Directory *m_rootDir;      //!< FtpRoot dir
	std::string m_username;    //!< Username for connecting
	std::string m_password;    //!< Password for connecting
	std::string m_buffer;      //!< Input parser buffer

	FtpSocket *m_cSocket;      //!< Control connection socket
	FtpSocket *m_dSocket;      //!< Data connection socket

	std::string m_pendingReq;  //!< Pending data connection request
};

/**
 * Directory represents a single directory in a FTP Session object.
 * Directories may contain files, and other directories.
 */
class Directory : public Object {
public:
	Directory(Directory *parent, const std::string &name);
private:
	Directory();                             //!< Forbidden
	Directory(const Directory&);             //!< Forbidden
	Directory& operator=(const Directory&);  //!< Forbidden

	// Session manages addFile()/addDir() calls
	friend class Session;

	/**
	 * Add a file to this directory
	 *
	 * @param name      Name of the file
	 */
	void addFile(const std::string &name);

	/**
	 * Add a subdirectory to this directory
	 */
	void addDir(Directory *dir);

	typedef std::vector<std::string> FilesList;
	typedef FilesList::iterator FileIter;

	FilesList m_filesList;    //!< List of files found in this directory

	typedef std::map<std::string, Directory*> DirList;
	typedef DirList::iterator DirIter;

	DirList m_subDirs;       //!< List of subdirectories
};

} // namespace Ftp

#endif
