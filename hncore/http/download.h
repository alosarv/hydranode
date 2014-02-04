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

#ifndef __DOWNLOAD_H__
#define __DOWNLOAD_H__

#include <hnbase/object.h>
#include <hncore/http/http.h>
#include <hncore/http/parser.h>
#include <hncore/http/connection.h>
#include <hncore/http/file.h>
#include <boost/enable_shared_from_this.hpp>


namespace Http {

/**
 * @brief The Download class generally represents a virtual file that is
 *        being downloaded from a HTTP server.
 *
 * A Download object will store multiple (at least one) Connection objects,
 * which are utilized to connect to servers and download data from them.
 * As these Connection objects are rather "dumb", the Download class is there
 * to organize them and care about the various situations that might occur
 * during a download, e.g. the file isn't on the server, or the filesizes
 * of two Connection objects don't match (when doing segmented downloads)...
 *
 * In short: this is the place where all the internal logic goes to... :-D
 */
class Download :
	public Object,
	public Trackable,
	public boost::enable_shared_from_this<Download>
{
public:
	/**
	 * @name Constructors
	 */
	//@{

	/**
	 * First version of the constructor.
	 *
	 * @param sf       The newly created SharedFile object to which
	 *                 the Download object is going to write
	 *                 received data to.
	 */
	Download(SharedFile *sf);	

	//! Generic destructor
	~Download();

	/**
	 * @name Various internal callback functions
	 */
	//@{
	void onPDHalt(PartData *pd);
	void onPDResume(PartData *pd);

	//! Only Connection objects should call this function!
	void onParserEvent(Detail::ConnectionPtr c, Parser *p, ParserEvent evt);
	//@}

	/**
	 * @name onParserEvent's sub-functions
	 *
	 * onParserEvent is split up into various sub-functions:
	 */
	//@{
	void onParserSize(Detail::ConnectionPtr c);
	void onParserSuccessful(Detail::ConnectionPtr c);
	void onParserRedirect(Detail::ConnectionPtr c);
	void onParserFailure(Detail::ConnectionPtr c);
	//@}

	/**
	 * Tries to add a URL to this download, which means that if the
	 * URL is proven to be valid, the file is automatically going to be
	 * downloaded from the added URL too. (--> segmented downloading)
	 *
	 * @param url       URL to be added to the download
	 * @return          "True" when the URL has been added successfully
	 */
	bool tryAddUrl(Detail::ParsedUrl &url);

	//! Used to add another source via hnsh's "addsource" command
	void addSource(PartData *pd, const std::string &url);

	/**
	 * @name Generic accessors
	 */
	//@{
	uint32_t getSourceCnt() { return m_connections.size(); }
	std::vector<std::string> getUrls();
	//@}

	//! Comparison operator for std containers
	bool operator<(const Download &sess) {
		return getName() < sess.getName();
	}

private:
	Download();                             //!< Forbidden
	Download(const Download&);              //!< Forbidden
	Download& operator=(const Download&);   //!< Forbidden

	typedef std::list<Detail::ConnectionPtr> ConnList;
	typedef std::list<Detail::ConnectionPtr>::iterator ConnIter;

	/**
	 * Creates a new Connection object and connects
	 * all needed boost::signal's.
	 */
	void createConnection(Detail::ParsedUrl &url);

	/**
	 * @name onSocketEvent's sub-functions
	 *
	 * onSocketEvent is split up into various sub-functions:
	 */
	//@{
	void onSockConnected(Detail::ConnectionPtr c);
	void onSockLost(Detail::ConnectionPtr c);
	//@}

	/**
	 * Signal handler for PartData::getLinks signal.
	 */
	void getLink(PartData *file, std::vector<std::string> &links);

	Detail::FilePtr m_file;
	ConnList        m_connections;
};


} // End namespace Http

#endif
