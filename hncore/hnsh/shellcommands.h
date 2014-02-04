/*
 *  Copyright (C) 2005-2006 Alo Sarv <madcat_@users.sourceforge.net>
 *  Copyright (C) 2005 Lorenz Bauer
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
 * \file shellcommands.h Interface for ShellCommands class
 */

#ifndef __SHELLCOMMANDS_H__
#define __SHELLCOMMANDS_H__

#include <hncore/search.h>
#include <hncore/fwd.h>
#include <hncore/hnsh/shellclient.h>
#include <hnbase/object.h>
#include <hnbase/sockets.h>
#include <boost/tokenizer.hpp>
#include <boost/function.hpp>
#include <string>
#include <map>

namespace Shell {

// reserved for later use
/*class path_tokenizer {
public:
	template<typename InputIterator, typename Token>
	bool operator()(InputIterator &curr, InputIterator &end, Token &tok) {
		tok = Token();

		if (curr == end) {
			return false;
		}
		for (; curr != end && *curr != '/'; tok += *curr++);
		if (curr != end && *curr == '/') {
			curr++;
		}
	}
	void reset() {}
};*/

/**
 * \todo document this, and the typedefs below
 */
class ShellCommands : public Trackable {
public:
	typedef boost::function<bool (Tokenizer)> CommandHandler;
	typedef std::pair<std::string, CommandHandler> CommandPair;
	typedef std::map<std::string, CommandHandler> CommandMap;
	typedef CommandMap::iterator Iter;

	/**
	 * Constructor
	 *
	 * @param parent  Pointer to a ShellClient instance
	 * @param cli     Client socket
	 */
	ShellCommands(ShellClient *parent, SocketClient *cli);

	/**
	 * Dispatches a command.
	 *
	 * @param args      Arguments for the command
	 */
	bool dispatch(Tokenizer args);

	//! Returns the last error message
	std::string getError();

	//! Get the current path
	std::string getPathStr() const;

	//! Returns command history size
	size_t getHistorySize();

	/**
	 * Add a command to history
	 *
	 * @param cmd       Command to be added
	 */
	void addCommand(const std::string &cmd) {
		m_cmdHistory.push_back(cmd);
	}

	//! Get previous command
	std::string getPrevCommand();

	//! Get next command
	std::string getNextCommand();

	//! Shutdown Hydranode
	bool cmdShutdown(Tokenizer args);

	//! Change cwd
	bool cmdChangeDir(Tokenizer args);

	//! List cwd
	bool cmdListDir(Tokenizer args);

	//! Quit session
	bool cmdDisconnect(Tokenizer args);

	//! List modules
	bool cmdListModules(Tokenizer args);

	//! Load module
	bool cmdLoadModules(Tokenizer args);

	//! Unload module
	bool cmdUnloadModules(Tokenizer args);

	//! Search
	bool cmdSearch(Tokenizer args);

	//! View search results (and optionally sort them)
	bool cmdViewResults(Tokenizer args);

	//! Start downloading of a file
	bool cmdDownload(Tokenizer args);

	//! View downloads
	bool cmdListDownloads(Tokenizer args);

	//! Download manipulation
	bool cmdCancelDownload(Tokenizer args);
	bool cmdPauseDownload(Tokenizer args);
	bool cmdStopDownload(Tokenizer args);
	bool cmdResumeDownload(Tokenizer args);
	bool cmdLinkDownloads(Tokenizer args);

	//! Show last log messages
	bool cmdListMessages(Tokenizer args);

	//! Display help
	bool cmdHelp(Tokenizer args);

	//! Show hasher statistics
	bool cmdShowHasherStats(Tokenizer args);

	//! Show info about Hydranode
	bool cmdShowInfo(Tokenizer args);

	//! Show command history
	bool cmdShowHistory(Tokenizer args);

	//! Show scheduler statistics
	bool cmdSchedStats(Tokenizer args);

	//! Show IPFilter statistics
	bool cmdFilterStats(Tokenizer args);

	//! List/Enable/Disable trace masks
	bool cmdTrace(Tokenizer args);

	//! Import files from a location
	bool cmdImport(Tokenizer args);

	//! Display / Change configuration values
	bool cmdConfig(Tokenizer args);

	//! Enables/Disables log output to shell
	bool cmdLog(Tokenizer args);

	//! Add source to a download
	bool cmdAddSource(Tokenizer args);

	//! View clients
	bool cmdViewClients(Tokenizer args);

	//! Clear the shell
	bool cmdClear(Tokenizer);

	//! Displays application uptime
	bool cmdUptime(Tokenizer);

	//! This is used for module-specific commands
	bool cmdCustom(Tokenizer);

	//! Share a folder
	bool cmdShare(Tokenizer);

	//! Displays file buffers amount and such
	bool cmdMemStats(Tokenizer);

	//! Stops current search
	bool cmdStopSearch(Tokenizer);

	//! Forces disk space allocation for download
	bool cmdAlloc(Tokenizer);

	//! Rehashes a temp file chunks
	bool cmdRehash(Tokenizer);

	//! Print one client
	void printClient(BaseClient *c);

	// --- Helpers ---
	//! (cmdSearch) Convert a three-char string into FileType
	FileType helperStrToType(const std::string&);

	//! Prints help about 'config' command
	void printConfigHelp();

	//! Prints all configuration values
	void printConfigValues();

	//! (cmdSearch) Handle new search results
	void helperOnSearchEvent(SearchPtr);

	/**
	 *  Prints all search results currently in buffer
	 *
	 * @param printAll    If false, only m_parent->height() number
	 *                    of results are printed (sorted by avail).
	 */
	void printSearchResults(bool printAll = true);

	//! Sort m_searchResults container, based on predicate
	void sortResults(const std::string &pred);

	//! Parses input of format 1-3,5,6 and returns vector of 12356
	std::vector<uint32_t> selectObjects(const std::string &input);

	/**
	 * Print info about a download
	 *
	 * @param n     Number identifying this download
	 * @param pd    The download object
	 */
	void helperPrintDownload(uint32_t n, PartData *pd);

	/**
	 * Prints details about a specific download
	 *
	 * @param num    Index in m_downloads map
	 */
	void helperDownloadDetails(uint32_t num);

	//! (cmdHelp) Print context sensitive information
	void helperPrintObjOpers(Object *obj);

	//! (cmdHelp) Translate DataType into human-readable data type name
	std::string helperGetTypeName(DataType t);

	//! (dispatch) Try to call Object opers
	void helperCallObjOper(Object *obj, Tokenizer args);

	//! Signal handler for module-loaded event
	void onModuleLoaded(ModuleBase *mod);
	//! Signal handler for module-unloaded event
	void onModuleUnloaded(ModuleBase *mod);
private:
	//! Parent ShellClient instance
	ShellClient *m_parent;

	//! Client socket
	SocketClient *m_socket;

	//! Current working directory
	Object *m_curPath;

	//! Current active search (if any)
	SearchPtr m_currentSearch; 

	//! Last search results. Gets cleared at every new search
	std::vector<SearchResultPtr> m_searchResults;

	//! Temporary container for temp files, filled by `view downloads`
	//! command. Event handler for PartData events ensures that when
	//! temp files are deleted, they are erased from this list aswell
	//! (e.g. the pointer set to 0, not erased, to avoid breaking num-
	//! based access from this list in `cancel` download call).
	std::vector<PartData*> m_downloads;

	//! Objects mentioned in last cancel attempt
	std::set<PartData*> m_lastCancel;

	//! Event handler for PartData events
	void onPDEvent(PartData *pd, int evt);

	//! Last error message
	std::string m_lastError;

	//! Command history
	std::vector<std::string> m_cmdHistory;

	//! Current command
	std::vector<std::string>::iterator m_curCmd;

	//! Map maintaining cmd -> function association
	CommandMap m_commands;

	//! Set last error message
	void setError(const std::string str);
	void setError(const boost::format fmt);

};

} // namespace Shell

#endif//__SHELLCOMMANDS_H__
