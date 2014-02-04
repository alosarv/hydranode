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

#include <hncore/http/download.h>
#include <hncore/http/client.h>
#include <hnbase/bind_placeholders.h>
#include <hnbase/log.h>
#include <hnbase/timed_callback.h>
#include <hncore/metadata.h>
#include <hncore/fileslist.h>
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>
#include <hncore/http/file.h>


namespace Http {

const std::string TRACE = "http.download";

Download::Download(SharedFile *sf) try
	: Object(&HttpClient::instance(), "httpdownload")
{
	logTrace(TRACE, boost::format("new Download(%p)") % this);

	CHECK_THROW(sf);

	m_file.reset(new Detail::File(sf));
	PartData *pd = m_file->getPartData();
	MetaData *md = m_file->getMetaData();

	pd->getSourceCnt.connect(boost::bind(&Download::getSourceCnt, this));
	pd->getLinks.connect(boost::bind(&Download::getLink, this, _b1, _b2));
	pd->onPaused.connect(boost::bind(&Download::onPDHalt, this, _b1));
	pd->onStopped.connect(boost::bind(&Download::onPDHalt, this, _b1));
	pd->onResumed.connect(boost::bind(&Download::onPDResume, this, _b1));
	pd->addSource.connect(
		boost::bind(&Download::addSource, this, _b1, _b2)
	);

	MetaData::CustomIter it = md->customBegin();
	for ( ; it != md->customEnd(); ++it) {
		if ((*it).substr(0, 6) == "[http]") {
			Detail::ParsedUrl tmp((*it).substr(6));
			tryAddUrl(tmp);
			setName(tmp.getFile());
		}
	}

} catch (std::exception &e) {
	LOG_EXCEPTION(e);
} MSVC_ONLY(;)


Download::~Download() {
	logTrace(TRACE, boost::format("~Download(%p)") % this);
	m_file.reset();
	m_connections.clear();
}


void Download::getLink(PartData *file, std::vector<std::string> &links) try {
	CHECK_RET(file == m_file->getPartData());
	links = getUrls();
} catch (std::exception &e) {
	LOG_EXCEPTION(e);
} MSVC_ONLY(;)


std::vector<std::string> Download::getUrls() {
	std::vector<std::string> tmp;
	ConnIter i = m_connections.begin();
	for ( ; i != m_connections.end(); ++i) {
		tmp.push_back((*i)->getParsedUrl().getUrl());
	}
	return tmp;
}


void Download::addSource(PartData *pd, const std::string &url) try {
	if (!m_file->getPartData() || m_file->getPartData() != pd) {
		return;
	}

	logTrace(TRACE, "Trying to add source to our PartData: " + url);
	Detail::ParsedUrl tmp(url);
	if (tmp.isValid()) {
		tryAddUrl(tmp);
	}

} catch (std::exception &e) {
	LOG_EXCEPTION(e);
} MSVC_ONLY(;)


void Download::onPDHalt(PartData *pd) try {
	CHECK_RET(m_file->getPartData() == pd);
	ConnIter it = m_connections.begin();
	for ( ; it != m_connections.end(); ++it) {
		(*it)->reset();
	}
} catch (std::exception &e) {
	LOG_EXCEPTION(e);
} MSVC_ONLY(;)


void Download::onPDResume(PartData *pd) try {
	CHECK_RET(m_file->getPartData() == pd);
	ConnIter it = m_connections.begin();
	for ( ; it != m_connections.end(); ++it) {
		(*it)->tryReconnect();
	}
} catch (std::exception &e) {
	LOG_EXCEPTION(e);
} MSVC_ONLY(;)


void Download::onSockConnected(Detail::ConnectionPtr c) {
	logTrace(TRACE, "onSockConnected()");
}


void Download::onSockLost(Detail::ConnectionPtr c) {
	logTrace(TRACE, "onSockLost()");

	if (m_file->isComplete()) {
		return logTrace(TRACE, "download is already complete...");

	} else if (m_file->getPartData()) {
		if (m_file->getPartData()->isComplete()) {
			return logTrace(TRACE,
				"onSockLost(): download complete..."
			);
		} else if (!m_file->getPartData()->isRunning()) {
			return logTrace(TRACE,
				"onSockLost(): PartData is paused..."
			);
		}
	} else {
		logMsg(
			boost::format(
				COL_RED "Couldn't get the file " COL_MAGENTA
				"%s" COL_RED " because the connection to "
				COL_MAGENTA "%s" COL_RED " was lost." COL_NONE
			)
			% c->getParsedUrl().getFile()
			% c->getAddr().getStr()
		);
		// FIXME: maybe we should not directly delete the
		// download, but instead try to re-connect after
		// some period of time...?!

		//HttpClient::instance().deleteDownload(shared_from_this());
		//return;
	}

	// Reconnect if we ever reach here...
	c->connect();
}


void Download::onParserEvent(
	Detail::ConnectionPtr c, Parser *p, ParserEvent evt
) try {
	if (evt == EVT_SIZE) {
		onParserSize(c);

	} else if (evt == EVT_REDIRECT) {
		onParserRedirect(c);

	} else if (evt == EVT_SUCCESSFUL) {
		onParserSuccessful(c);

	} else if (evt == EVT_FAILURE) {
		onParserFailure(c);

	} else if (evt == EVT_FILE_COMPLETE) {
		// if no filesize is known we have to assume, that we already
		// got the whole file from the server...
		logTrace(TRACE, "onParserEvent(): file complete...");
		m_file->tryComplete();
		HttpClient::instance().deleteDownload(shared_from_this());

	} else {
		logTrace(TRACE,	boost::format("Parser(%p, evt=%p)") % p % evt);
	}

} catch (std::exception &e) {
	LOG_EXCEPTION(e);
} MSVC_ONLY(;)


void Download::onParserSize(Detail::ConnectionPtr c) {
	logTrace(TRACE, "onParserSize()");
	uint64_t size = c->getParser()->getSize();

	if (m_file->getSize()) {
		// check if the already known filesize and the discovered
		// filesize match...
		if (m_file->getSize() == size) {
			logTrace(TRACE,
				COL_GREEN "Filesizes match, this seems "
				"to be a valid host..." COL_NONE
			);

		// if the filesizes do not match, mark the host as invalid:
		} else {
			logTrace(TRACE,
				COL_RED "Filesizes don't match, this "
				"seems to be an invalid host." COL_NONE
			);
		}
	} else {
		m_file->setSize(size);
	}
}


void Download::onParserSuccessful(Detail::ConnectionPtr c) {
	logTrace(TRACE, "onParserSuccessful()");
	c->setChecked(true);
	m_file->setSourceMask();

	// add the URL to MetaData if it isn't already there...
	Detail::ParsedUrl &url = c->getParsedUrl();
	MetaData::CustomIter it = m_file->getMetaData()->customBegin();
	while (it != m_file->getMetaData()->customEnd()) {
		if ((*it++).substr(6) == url.getUrl()) {
			return; //Url is already in MetaData...
		}
	}

	logTrace(TRACE, "Adding URL to MetaData: " + url.getUrl());
	m_file->getMetaData()->addCustomData("[http]" + url.getUrl());
}


void Download::onParserRedirect(Detail::ConnectionPtr c) {
	Parser *p = c->getParser();
	std::string location, redirect, disposition;
	location = p->getHeader("location");
	disposition = p->getHeader("content-disposition");

	if (!disposition.empty()) {
		boost::regex reg(".*filename=[\\\"]*(.+)[\\\"]");
		boost::match_results<const char*> matches;

		if (boost::regex_match(disposition.c_str(), matches, reg)) {
			m_file->setDestination(std::string(matches[1]));
		}

		return;
	}

	const Detail::ParsedUrl &url = c->getParsedUrl();
	// check if it is a full URL or just a path:
	if (location.substr(0, 7) == "http://") {
		redirect = location;

	} else if (location.substr(0, 1) == "/") {
		boost::format fmt("http://%s%s");
		fmt % url.getHost() % location;
		redirect = fmt.str();

	} else if (location.substr(0, 2) == "./") {
		boost::format f("http://%s%s%s");
		f % url.getHost() % url.getPath() % location.substr(2);
		redirect = f.str();

	} else {
		logError("Don't know how to handle "
			 "this redirection: " + location);
		return;
	}

	Detail::ParsedUrl u(redirect);
	if (!url.getUser().empty() && !url.getPassword().empty()) {
		u.setUser(url.getUser());
		u.setPassword(url.getPassword());
	}

	logMsg(boost::format(COL_GREEN
		"The server %s redirects us to the following "
		"URL: " COL_MAGENTA "%s" COL_NONE)
		% url.getHost() % u.getUrl()
	);

	m_connections.remove(c);
	tryAddUrl(u);
}


void Download::onParserFailure(Detail::ConnectionPtr c) {
	Parser *p = c->getParser();
	int status = p->getStatusCode();

	// code 404 means: file not there
	if (status == STATUS_NOT_FOUND) {
		logMsg(
			boost::format(
				COL_RED	"The requested file (%s) couldn't "
				"be found on the server: %s." COL_NONE
			) % p->getFileName() % p->getHostName()
		);
		m_connections.remove(c);

	} else {
		logWarning(
			boost::format(
				"Parser got an unsuccessful "
				"HTTP response: %s [code=%i]"
			) % codeToStr(status) % status
		);
	}
}


bool Download::tryAddUrl(Detail::ParsedUrl &url) {
	logTrace(TRACE, "tryAddUrl()");

	ConnIter it = m_connections.begin();
	for ( ; it != m_connections.end(); ++it) {
		if ((*it)->getParsedUrl() == url) {
			logTrace(TRACE, "URL is already in m_connections.");
			return false;
		}
	}

	logTrace(TRACE, "tryAddUrl() was successful, ");
	createConnection(url);

	/**
	 * We haven't got any data nor do we know the actual
	 * filesize: this is the case if we're being redirected
	 * by the HTTP-server to another location, so we need to
	 * change the filename.
	 * example:
	 * user requests download.php?file=1, so the filename would be
	 * "download.php", but we're redirected to another location
	 * which finally reveals the actual filename...
	 */
	if (!m_file->getSize()) {
		m_file->setDestination(url.getFile());
	}

	return true;
}


void Download::createConnection(Detail::ParsedUrl &url) {
	Detail::ConnectionPtr c(new Detail::Connection(url, m_file));

	c->onLost.connect(
		boost::bind(&Download::onSockLost, this, _b1)
	);
	c->onConnected.connect(
		boost::bind(&Download::onSockConnected, this, _b1)
	);
	c->onParser.connect(
		boost::bind(&Download::onParserEvent, this, _b1, _b2, _b3)
	);

	m_connections.push_back(c);
}

} // End namespace Http
