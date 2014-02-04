/*
 *  Copyright (C) 2005-2006 Gaubatz Patrick <patrick@gaubatz.at>
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

#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#include <hncore/http/http.h>
#include <hncore/http/parser.h>
#include <hncore/http/file.h>
#include <hncore/baseclient.h>
#include <hnbase/hostinfo.h>
#include <boost/enable_shared_from_this.hpp>


namespace Http {
namespace Detail {

/**
 * @brief The Connection class generally represents a single connection to a
 *        HTTP server.
 *
 * It takes care about all the socket-related tasks, such as
 * resolving of hostnames, connecting, reconnecting, ..
 * Furthermore it is derived from BaseClient which provides various accessors
 * to generate statistics and runtime information.
 * It is Connection's job to provide these accessors.
 */
class Connection :
	public BaseClient,
	public Trackable,
	public boost::enable_shared_from_this<Connection>
{
public:
	/**
	 * This creates a new Connection-object.
	 *
	 * @note The last argument is optional.	 *

	 * @param url       The URL that will be connected to
	 * @param file      The File object that is "attached"
	 *                  to this Connection
	 */
	Connection(ParsedUrl url, Detail::FilePtr file);

	//! Generic destructor.
	~Connection();

	/**
	 * @name Generic accessors
	 */
	//@{
	HttpSocket*              getSocket() const { return m_socket.get(); }
	Parser*                  getParser() const { return m_parser.get(); }
	ParsedUrl&               getParsedUrl()    { return m_url;      }
	::Detail::LockedRangePtr getLocked() const { return m_locked; }
	IPV4Address              getAddr()         { return *m_curAddr; }
	//@}

	/**
	 * @name Generic setters
	 */
	//@{
	/**
	 * This is a std::vector because a hostname may resolve to
	 * multiple IP addresses.
	 */
	void setAddr(std::vector<IPV4Address> addr);
	void setAddr(IPV4Address addr);
	void setParsedUrl(ParsedUrl url) { m_url = url; }
	void setChecked(bool x) { m_checked = x; }
	//@}

	/**
	 * @name Various internal callback functions
	 */
	//@{
	void onParserEvent(Parser *p, ParserEvent evt);
	void onSocketEvent(HttpSocket *s, SocketEvent evt);
	void onResolverEvent(HostInfo info);
	//@}

	/**
	 * Sends data trough the object's socket.
	 *
	 * @param p       The Parser "requesting" to send data
	 * @param data    The data to be sent to the server
	 */
	void sendData(Parser *p, const std::string &data);

	/**
	 * Tries to write data the object's PartData object.
	 *
	 * @note If that fails, onWriteData() signal is called.
	 *
	 * @param p       The Parser "requesting" to write data
	 * @param data    The data to be written to the harddisk
	 * @param offset  The offset of the file to write the data to
	 */
	void writeData(Parser *p, const std::string &data, uint64_t offset);

	/**
	 * This will reconnect to the server, if:
	 *   a) the Connection is valid
	 *   b) the Connection is not connected
	 *   c) if a PartData object exists, which
	 *      is neither paused nor complete
	 */
	void tryReconnect();

	/**
	 * If the hostname resolved to multiple IP addresses, the next
	 * IP address will be used in the next connection attempt.
	 *
	 * @note After a whole loop through the address-list it will start
	 *       again from the beginning of the list.
	 */
	void tryNextAddr();

	/**
	 * Connect to the server, if the socket is neither connected nor
	 * currently trying to connect.
	 */
	void connect();

	//! Indicates if a HTTP HEAD request has already been done
	bool isChecked() { return m_checked; }

	/**
	 * Resets all data collected by this object.
	 *
	 * @note This function must be called after each HTTP request!
	 */
	void reset();

	//! Resets the object's UsedRangePtr
	void resetUsed() { m_used.reset(); }

	//! Resets the object's LockedRangePtr
	void resetLocked() { m_locked.reset(); }

	/**
	 * Try to aquire a LockedRangePtr from the object's PartData.
	 *
	 * @param chunksize       Number of bytes that should be requested
	 * @throws                std::exception
	 */
	void getLocks(uint32_t chunksize);

	//! Check for equality between two Connection objects.
	bool operator==(Connection &c) {
		return (
			c.getSocket() == m_socket.get() &&
			c.getParser() == m_parser.get()
		);
	}

	/**
	 * @name Accessors for BaseClient
	 */
	//@{
	virtual std::string getNick() const { return "none"; }
	virtual IPV4Address getAddr() const { return *m_curAddr; }
	virtual std::string getSoft() const;
	virtual uint64_t    getSessionDownloaded() const;
	virtual uint64_t    getTotalDownloaded()   const;
	virtual uint32_t    getDownloadSpeed()     const;
	//@}

	/**
	 * @name Various Boost::signal's
	 */
	//@{
	//! Called when the socket is in connected state.
	boost::signal<void(Detail::ConnectionPtr)> onConnected;

	//! Called when the socket lost its connection.
	boost::signal<void(Detail::ConnectionPtr)> onLost;

	//! Called when the socket timed out or an error occured.
	boost::signal<void(Detail::ConnectionPtr)> onFailure;

	//! Called when Http::Parser emitted a ParserEvent.
	boost::signal<
		void(Detail::ConnectionPtr, Parser*, ParserEvent)
	> onParser;
	//@}

private:
	Connection();                             //!< Forbidden
	Connection(const Connection&);            //!< Forbidden
	Connection& operator=(const Connection&); //!< Forbidden

	//! Start a HTTP request
	void doGet();

	/**
	 * There are some servers out that don't correctly support
	 * byte-ranges. As a result, the whole file needs to be requested
	 * in one go. requestChunk() maintains a (small) list of these servers.
	 *
	 * @returns       "false" if the whole file needs to be requested
	 *                as the server doesn't support byte-ranges.
	 */
	bool requestChunk();

	/**
	 * This initializes the speedmeter.
	 *
	 * @note (only if PartData was specified in the object's constructor)
	 */
	void initSpeeder();

        //! Try to resolve the hostname. (m_url.getHost())
	void hostLookup();

	typedef std::vector<IPV4Address>           AddrVec;
	typedef std::vector<IPV4Address>::iterator AddrIter;

	Detail::FilePtr               m_file;
	ParsedUrl                     m_url;
	boost::scoped_ptr<HttpSocket> m_socket;
	boost::scoped_ptr<Parser>     m_parser;
	::Detail::UsedRangePtr        m_used;
	::Detail::LockedRangePtr      m_locked;
	AddrVec                       m_addr;
	AddrIter                      m_curAddr;

	//! Indicates if a HTTP HEAD request has already been done
	bool m_checked;

	//! Connection between socket's speedmeter and PartData getSpeed signal.
	boost::signals::connection m_speeder;
};


} // End namespace Detail
} // End namespace Http

#endif
