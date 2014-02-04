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

#include <hnbase/log.h>
#include <hncore/http/download.h>
#include <hncore/http/client.h>
#include <hncore/fileslist.h>
#include <hncore/metadb.h>
#include <hncore/search.h>
#include <hncore/metadata.h>
#include <hnbase/prefs.h>
#include <boost/filesystem/operations.hpp>
#include <boost/regex.hpp>


namespace Http {

IMPLEMENT_MODULE(HttpClient);

const std::string TRACE = "http.client";

bool HttpClient::onInit() {
	logMsg("Starting HTTP-Client module...");

	Log::instance().addTraceMask(TRACE);
	Log::instance().addTraceMask("http.urlparser");
	Log::instance().addTraceMask("http.download");
	Log::instance().addTraceMask("http.parser");
	Log::instance().addTraceMask("http.connection");
	Log::instance().addTraceMask("http.file");

	//Log::instance().enableTraceMask(TRACE);
	//Log::instance().enableTraceMask("http.download");

	setPriority(PR_LOW); // http downloads should be at low priority

	Search::addLinkHandler(
		boost::bind(&HttpClient::linkHandler, this, _1)
	);
	MetaData::getEventTable().addHandler(0, this, &HttpClient::onMDEvent);

	//m_configChange = Prefs::instance().valueChanged.connect(
	//boost::bind(&HttpClient::loadConfig, this, _b1)
	//);

	loadStatusCodes();
	loadConfig();

	return true;
}


int HttpClient::onExit() {
	logMsg("Stopping HTTP module...");
	m_downloads.clear();
	m_configChange.disconnect();

	Prefs::instance().write<bool>("/http/UseProxy", m_useProxy);
	Prefs::instance().write<std::string>("/http/ProxyHost", m_proxyHost);
	Prefs::instance().write<uint32_t>("/http/ProxyPort", m_proxyPort);

	return 0;
}


void HttpClient::onReady() {
	logMsg(COL_GREEN "The HTTP-module is now fully operational." COL_NONE);
	m_ready = true;

	if (m_useProxy) {
		logMsg(
			boost::format(
				COL_GREEN "Using HTTP Proxy " COL_MAGENTA
				"%s:%i" COL_GREEN "." COL_NONE
			)
			% m_proxyHost % m_proxyPort
		);
	}

	//Checking for partial HTTP downloads and resume them (if possible):
	processFilesList();

	while (m_pendingDownloads.size()) {
		tryStartDownload(m_pendingDownloads.back());
		m_pendingDownloads.pop_back();
	}	

	notifyOnReady();
}


bool HttpClient::isReady() {
	if (m_ready) {
		return true;
	} else {
		logMsg("The HTTP-module isn't ready yet.");
		return false;
	}
}


void HttpClient::loadConfig(std::string key) {
	if (key.substr(0, 5) != "http/") {
		return;
	}
	logTrace(TRACE, "loadConfig()");
	m_ready = false;
	Prefs::instance().setPath("/http");
	m_useProxy = Prefs::instance().read<bool>("UseProxy", false);
	m_proxyHost = Prefs::instance().read<std::string>("ProxyHost", "");
	m_proxyPort = Prefs::instance().read<uint32_t>("ProxyPort", 0);

	if (!m_useProxy) {
		onReady();
		return;
	}

	if (m_useProxy && (m_proxyHost.empty() || !m_proxyPort)) {
		logMsg(
			COL_RED "Your HTTP Proxy configuration "
			"is invalid." COL_NONE
		);
		m_useProxy = false;
	}

	// check if m_proxyHost is an IP address:
	boost::regex reg("^(\\d+)(\\.\\d+){3}$");
	if (boost::regex_match(m_proxyHost, reg)) {
		m_proxy.setAddr(m_proxyHost);
		m_proxy.setPort(m_proxyPort);
		onReady();

	} else {
		DNS::lookup(m_proxyHost, this, &HttpClient::onResolverEvent);
	}
}


void HttpClient::onResolverEvent(HostInfo info) {
	if (info.error()) {
		logError(
			boost::format(
				"Cannot use HTTP proxy %s, because host "
				"lookup failed with error: %s"
			) % info.getName() % info.errorMsg()
		);
	} else {
		logTrace(TRACE,
			"Resolver resolved hostname \""
			+ m_proxyHost + "\" to " + info.begin()->getAddrStr()
		);

		// at the moment, we're just using the first
		// IP-address suggested by Resolver.
		m_proxy = *info.begin();
		m_proxy.setPort(m_proxyPort);
		onReady();

	}
}


void HttpClient::onMDEvent(MetaData *md, int evt) {
	if (!isReady() || evt != MD_ADDED_CUSTOMDATA) {
		return;
	}

	logTrace(TRACE, "New customData was added, checking if "
			"there are HTTP-URLs in MetaData..."
	);
	MetaData::CustomIter it = md->customBegin();
	for ( ; it != md->customEnd(); ++it) {
		if ((*it).substr(0, 6) == "[http]") {
			if (tryStartDownload((*it).substr(6), true)) {
				logMsg(COL_GREEN "Source was "
				       "successfully added." COL_NONE
				);
			}
		}
	}
}


bool HttpClient::linkHandler(const std::string &link) {
	//Check if the URL is valid:
	if (link.substr(0, 7) == "http://" && link.size() > 8 && isReady()) {
		return tryStartDownload(link);
	}
	return false;
}


bool HttpClient::tryStartDownload(const std::string &url, bool silent) {
	if (!isReady()) {
		m_pendingDownloads.push_back(url);
		return false;
	}

	// loop through all HTTP downloads and check if we are already
	// downloading the given URL...
	bool urlFound = false;
	DownloadIter it = m_downloads.begin();
	for ( ; it != m_downloads.end(); ++it) {
		std::vector<std::string> links = (*it)->getUrls();
		std::vector<std::string>::iterator i = links.begin();
		for ( ; i != links.end(); ++i) {
			if ((*i)== url) {
				urlFound = true;
				break;
			}
		}
	}

	if(urlFound) {
		if (!silent) {
			logMsg("You are already downloading: " + url);
		}
		return false;
	} else {
		return startDownload(url);
	}
}


bool HttpClient::startDownload(const std::string &url) {
	//logTrace(TRACE, boost::format("startDownload(%s)") % url);
	if (!isReady()) {
		return false;
	}
	Detail::ParsedUrl purl(url);

	std::vector<SharedFile*> files;
	std::vector<SharedFile*>::iterator it;
	SharedFile *sf;

	files = MetaDb::instance().findSharedFile(purl.getFile());

	if (files.size()) {
		for (it = files.begin(); it != files.end(); ++it) {
			sf = (*it);
			MetaData *md = sf->getMetaData();
			bool found = false;
			MetaData::CustomIter it2 = md->customBegin();
			while (it2 != md->customEnd()) {
				if (*it2++ == "[http]" + url) {
					found = true;
				}
			}
			if (found && sf->isComplete()) {
				logMsg("You already have: " + sf->getName());
				return false;

			} else if (sf->isPartial()) {
				DownloadPtr tmp(new Download(sf));
				PartData *pd = sf->getPartData();
				pd->onCanceled.connect(
					boost::bind(
						&HttpClient::deleteDownload,
						this, tmp
					)
				);
				pd->onCompleted.connect(
					boost::bind(
						&HttpClient::deleteDownload,
						this, tmp
					)
				);
				m_downloads.push_back(tmp);
				return true;
			}
		}
	}
	logTrace(TRACE, "Creating new dowload: " + purl.getFile());
	MetaData *md = new MetaData(0);
	CHECK_THROW(md);
	md->addFileName(purl.getFile());
	md->addCustomData("[http]" + url);
	MetaDb::instance().push(md);
	FilesList::instance().createDownload(purl.getFile(), md);

	return startDownload(url);
}

void HttpClient::deleteDownload(DownloadPtr down) {
	m_downloads.remove(down);
}

void HttpClient::processFilesList() {
	logTrace(TRACE, "processFilesList()");
	FilesList::SFIter it = FilesList::instance().begin();
	for ( ; it != FilesList::instance().end(); ++it) {
		SharedFile *sf = *it;
		MetaData *md = sf->getMetaData();

		if (!sf->isPartial() || !md) {
			continue;
		}

		MetaData::CustomIter i = md->customBegin();
		for ( ; i != md->customEnd(); ++i) {
			if ((*i).substr(0, 6) == "[http]") {
				//Create a new Download
				tryStartDownload((*i).substr(6));
				break;
			}
		} //end for
	} //end for
}


void HttpClient::loadStatusCodes() {
	logTrace(TRACE, "loadStatusCodes()");
	m_statusCodes[100] = "Continue";
	m_statusCodes[101] = "Switching Protocols";
	m_statusCodes[200] = "OK";
	m_statusCodes[201] = "Created";
	m_statusCodes[202] = "Accepted";
	m_statusCodes[203] = "Non-Authoritative Information";
	m_statusCodes[204] = "No Content";
	m_statusCodes[205] = "Reset Content";
	m_statusCodes[206] = "Partial Content";
	m_statusCodes[300] = "Multiple Choices";
	m_statusCodes[301] = "Moved Permanently";
	m_statusCodes[302] = "Found";
	m_statusCodes[303] = "See Other";
	m_statusCodes[304] = "Not Modified";
	m_statusCodes[305] = "Use Proxy";
	m_statusCodes[307] = "Temporary Redirect";
	m_statusCodes[400] = "Bad Request";
	m_statusCodes[401] = "Unauthorized";
	m_statusCodes[402] = "Payment Required";
	m_statusCodes[403] = "Forbidden";
	m_statusCodes[404] = "Not Found";
	m_statusCodes[405] = "Method Not Allowed";
	m_statusCodes[406] = "Not Acceptable";
	m_statusCodes[407] = "Proxy Authentification Required";
	m_statusCodes[408] = "Request Time-Out";
	m_statusCodes[409] = "Conflict";
	m_statusCodes[410] = "Gone";
	m_statusCodes[411] = "Length Required";
	m_statusCodes[412] = "Precondition Failed";
	m_statusCodes[413] = "Request Entity Too Large";
	m_statusCodes[414] = "Request-URI Too Large";
	m_statusCodes[415] = "Unsupported Media Type";
	m_statusCodes[416] = "Requested range not satisfiable";
	m_statusCodes[417] = "Expectation Failed";
	m_statusCodes[500] = "Internal Server Error";
	m_statusCodes[501] = "Not Implemented";
	m_statusCodes[502] = "Bad Gateway";
	m_statusCodes[503] = "Service unavailable";
	m_statusCodes[504] = "Gateway Time-out";
	m_statusCodes[505] = "HTTP Version not supported";
}


std::string HttpClient::codeToStr(int code) {
	return m_statusCodes[code];
}


void notifyOnReady(boost::function<void()> func) {
	if (HttpClient::instance().isReady()) {
		func();
	} else {
		HttpClient::instance().notifyOnReady.connect(func);
	}
}


std::string codeToStr(int code) {
	return HttpClient::instance().codeToStr(code);
}


std::string HttpClient::getDesc() const { 
	return "HTTP"; 
}


} // End namespace Http
