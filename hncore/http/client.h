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

#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <hncore/http/http.h>
#include <hncore/modules.h>
#include <hncore/partdata.h>
#include <hnbase/hostinfo.h>


namespace Http {

/**
 * @brief The HttpClient class, which is derived from ModuleBase, simply creates
 *        and afterwards deletes Download objects.
 *
 * It takes care about settings in the config-file and initializes
 * a link handler to tell the Hydranode-core that this module is able
 * to download files through the HTTP protocol.
 * Furthermore it tries to resume incomplete (HTTP-) downloads on
 * start-up.
 */
class HTTPEXPORT HttpClient : public ModuleBase {
	DECLARE_MODULE(HttpClient, "http");
public:
	/**
	 * @name ModuleBase related functions
	 */
	//@{
	virtual std::string getDesc() const; 
	virtual bool onInit();
	virtual int  onExit();
	//@}

	//! Link handler callback
	virtual bool linkHandler(const std::string &link);

	/**
	 * Check if the HTTP module is ready.
	 * (configuration has been done, hostname have been resolved,...)
	 *
	 * @return       "True" if the module is ready
	 */
	bool isReady();

	/**
	 * Check if a HTTP proxy has to be used.
	 * (According to the configuration)
	 *
	 * @return       "True" when a HTTP proxy should be used
	 */
	bool useProxy() const { return m_useProxy; }

	/**
	 * Returns the IP address of the HTTP proxy specified
	 * in the config-file.
	 *
	 * @return       The IP4Address of the HTTP proxy
	 */
	IPV4Address getProxy() const { return m_proxy; }

	/**
 	 * This function "translates" HTTP statuscodes, such as 404 into
 	 * human-readable strings like "Not found".
 	 *
 	 * @param code       The HTTP statuscode to be "translated".
 	 * @return           The "translated" statuscode.
 	 */
	std::string codeToStr(int code);

	/**
	 * Deletes a Download object.
	 *
	 * @param down       The Download obect to be deleted
	 */
	void deleteDownload(DownloadPtr down);

	/**
	 * Tries to start a new download.
	 * This may fail if the file has already been downloaded, or it is
	 * currently being downloaded by another Download-object.
	 *
	 * @param url       The URL of the file to be downloaded
	 * @param silent    If this is "true" the function will not display
	 *                  any log-messages if the download could not be
	 *                  started
	 * @return          "True" when the download could be started
	 */
	bool tryStartDownload(const std::string &url, bool silent = false);

	/**
	 * @name Various internal callback functions
	 */
	//@{
	void onMDEvent(MetaData *md, int evt);
	void onResolverEvent(HostInfo info);
	//@}

	//! This signal is emitted when the module is ready.
	boost::signal<void ()> notifyOnReady;

private:
	typedef std::list<DownloadPtr>           DownloadList;
	typedef std::list<DownloadPtr>::iterator DownloadIter;
	
	/**
	 * Loops through the FilesList and tries to complete
	 * unfinished HTTP downloads...
	 */
	void processFilesList();

	/**
	 * This finally creates and starts a new download.
	 *
	 * @note Do not directly call this function!
	 *       Use tryStartDownload() instead!
	 *
	 * @see HttpClient::tryStartDownload()
	 */
	bool startDownload(const std::string &url);

	//! This loads and parses the settings in the configuration file.
	void loadConfig(std::string key = "http/");

	//! This called when the module gets into ready state.
	void onReady();

	//! This fills the m_statusCodes variable.
	void loadStatusCodes();

	DownloadList m_downloads;
	bool         m_useProxy;
	std::string  m_proxyHost;
	uint32_t     m_proxyPort;
	IPV4Address  m_proxy;
	bool         m_ready;
	std::vector<std::string>    m_pendingDownloads;
	boost::signals::connection  m_configChange;
	std::map<int, std::string>  m_statusCodes;
};

} // End namespace Http

#endif
