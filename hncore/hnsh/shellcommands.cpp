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
 * \file shellcommands.cpp ShellCommands implementation
 */

#include <hncore/hnsh/shellclient.h>
#include <hncore/hnsh/shellcommands.h>
#include <hncore/hnsh/hnsh.h>
#include <hncore/hydranode.h>
#include <hncore/modules.h>
#include <hncore/search.h>
#include <hncore/partdata.h>
#include <hncore/partdata_impl.h>
#include <hncore/sharedfile.h>
#include <hncore/fileslist.h>
#include <hncore/metadb.h>
#include <hncore/metadata.h>
#include <hncore/clientmanager.h>
#include <hnbase/schedbase.h>
#include <hnbase/utils.h>
#include <hnbase/prefs.h>
#include <hnbase/ssocket.h>
#include <hnbase/log.h>
#include <boost/bind.hpp>
#include <boost/lambda/if.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/spirit.hpp>
#include <vector>

namespace Shell {

// Exception class
class CommandNotFound : public std::runtime_error {
public:
	CommandNotFound() : std::runtime_error("command not found.") {}
};

ShellCommands::ShellCommands(ShellClient *parent, SocketClient *cli)
: m_parent(parent), m_socket(cli), m_curPath(), m_lastCancel(),
m_curCmd(m_cmdHistory.end()) {
	CHECK_THROW(parent);
	CHECK_THROW(cli);

	using boost::bind;

	// set path to root
	m_curPath = &Hydranode::instance();

	// Init cmd -> function map
	m_commands["cd"] = bind(&ShellCommands::cmdChangeDir, this, _1);
	m_commands["ls"] = bind(&ShellCommands::cmdListDir, this, _1);

	// connection handling
	m_commands["shutdown"] = bind(&ShellCommands::cmdShutdown, this, _1);
	m_commands["exit"]     = bind(&ShellCommands::cmdDisconnect, this, _1);
	m_commands["quit"]     = bind(&ShellCommands::cmdDisconnect, this, _1);

	// modules
	m_commands["lsmod"]    = bind(&ShellCommands::cmdListModules, this, _1);
	m_commands["modprobe"] = bind(&ShellCommands::cmdLoadModules, this, _1);
	m_commands["rmmod"]  = bind(&ShellCommands::cmdUnloadModules, this, _1);

	// downloads
	m_commands["search"]  = bind(&ShellCommands::cmdSearch, this, _1);
	m_commands["vr"]      = bind(&ShellCommands::cmdViewResults, this, _1);
	m_commands["ss"]      = bind(&ShellCommands::cmdStopSearch, this, _1);
	m_commands["download"]  = bind(&ShellCommands::cmdDownload, this, _1);
	m_commands["vd"]     = bind(&ShellCommands::cmdListDownloads, this, _1);
	m_commands["cancel"] = bind(&ShellCommands::cmdCancelDownload, this,_1);
	m_commands["pause"]  = bind(&ShellCommands::cmdPauseDownload, this, _1);
	m_commands["stop"]   = bind(&ShellCommands::cmdStopDownload, this, _1);
	m_commands["resume"] = bind(&ShellCommands::cmdResumeDownload, this,_1);
	m_commands["addsource"] = bind(&ShellCommands::cmdAddSource, this, _1);

	// misc.
	m_commands["dmesg"]   = bind(&ShellCommands::cmdListMessages, this, _1);
	m_commands["help"]    = bind(&ShellCommands::cmdHelp, this, _1);
	m_commands["uname"]   = bind(&ShellCommands::cmdShowInfo, this, _1);
	m_commands["hs"]   = bind(&ShellCommands::cmdShowHasherStats, this, _1);
	m_commands["history"] = bind(&ShellCommands::cmdShowHistory, this, _1);
	m_commands["scs"]     = bind(&ShellCommands::cmdSchedStats, this, _1);
	m_commands["ifs"]     = bind(&ShellCommands::cmdFilterStats, this, _1);
#if !defined(NDEBUG) && !defined(NTRACE)
	m_commands["trace"]   = bind(&ShellCommands::cmdTrace, this, _1);
#endif
	m_commands["import"]  = bind(&ShellCommands::cmdImport, this, _1);
	m_commands["config"]  = bind(&ShellCommands::cmdConfig, this, _1);
	m_commands["log"]     = bind(&ShellCommands::cmdLog, this, _1);
	m_commands["vc"]      = bind(&ShellCommands::cmdViewClients, this, _1);
	m_commands["clear"]   = bind(&ShellCommands::cmdClear, this, _1);
	m_commands["uptime"]  = bind(&ShellCommands::cmdUptime, this, _1);
	m_commands["share"]   = bind(&ShellCommands::cmdShare, this, _1);
	m_commands["links"]   = bind(&ShellCommands::cmdLinkDownloads,this, _1);
	m_commands["memstat"] = bind(&ShellCommands::cmdMemStats, this, _1);
	m_commands["alloc"]   = bind(&ShellCommands::cmdAlloc, this, _1);
	m_commands["_rehash"]  = bind(&ShellCommands::cmdRehash, this, _1);

	std::vector<std::pair<std::string, std::string> > ret;
	ModManager::instance().getList(&ret);
	for (uint32_t i = 0; i < ret.size(); ++i) {
		m_commands[ret[i].first] = bind(
			&ShellCommands::cmdCustom, this, _1
		);
	}
	ModManager::instance().onModuleLoaded.connect(
		bind(&ShellCommands::onModuleLoaded, this, _1)
	);
	ModManager::instance().onModuleUnloaded.connect(
		bind(&ShellCommands::onModuleUnloaded, this, _1)
	);
	PartData::getEventTable().addAllHandler(this,&ShellCommands::onPDEvent);

	// populate downloadlist to allow vd #num work initially
        FilesList::CSFIter it = FilesList::instance().begin();
        for (; it != FilesList::instance().end(); ++it) {
                if (!(*it)->isPartial() || (*it)->getPartData()->getParent()) {
                        continue;
                }
                PartData *pd = (*it)->getPartData();
                m_downloads.push_back(pd);
	}
}

// dispatch a command to handlers; first try current object (herlperCallObjOper)
// then try global command-map lookup
bool ShellCommands::dispatch(Tokenizer args) try {
	CHECK_THROW(m_socket);
	CHECK_THROW(m_socket->isConnected());

	m_curCmd = m_cmdHistory.end();
	setError("");

	try {
		helperCallObjOper(m_curPath, args);
		return true;
	} catch (CommandNotFound&) {}

	typedef CommandMap::iterator Iter;
	Iter found = m_commands.end();
	for (Iter it = m_commands.begin(); it != m_commands.end(); ++it) {
		std::string cmd = (*it).first;
		if (cmd.size() < (*args.begin()).size()) {
			continue;
		}
		if (cmd.substr(0, (*args.begin()).size()) != *args.begin()) {
			continue;
		}
		// cmd is exact match; break out then
		if (cmd == *args.begin()) {
			found = it;
			break;
		} else if (found == m_commands.end()) {
			found = it;
		} else {
			setError(
				"Ambigious command; candidates are: "
				+ (*found).first + " " + (*it).first
			);
			return false;
		}
	}
	if (found == m_commands.end()) {
		setError(*args.begin() + ": command not found");
		return false;
	} else {
		// when using abbrevated commands, we must still make sure that
		// full command is passed to handler (some expect it, e.g.
		// cmdCustom())
		//
		// technically, this should be within an if statement, checking
		// if the first tokenizer element (command) matches the found
		// command. However, for some reason, when args is updated in the
		// inner scope, it's properly updated there, but when the scope
		// ends, the variable is just empty (in the outer scope). This
		// seems to happen both on MSVC and GCC, so shouldn't be compiler-
		// specific issue...
		// if (*args.begin() != (*found).first) {
			std::string tmp = (*found).first + " ";
			Tokenizer::iterator it = ++args.begin();
			while (it != args.end()) {
				tmp += *it++ + " ";
			}
			args.assign(tmp);
		// }
		if (!((*found).second(args))) {
			setError(*args.begin() + ": " + getError());
			return false;
		}
		if ((*found).first != "cancel") {
			m_lastCancel.clear();
		}
	}
	return true;
} catch (std::exception &e) {
	setError(std::string("hnsh: ") + e.what());
	return false;
}
MSVC_ONLY(;)

size_t ShellCommands::getHistorySize() {
	return m_cmdHistory.size();
}

std::string ShellCommands::getPrevCommand() {
	if (m_cmdHistory.size() < 1) {
		return std::string();
	} else if (m_curCmd == m_cmdHistory.begin()) {
		return *m_curCmd;
	} else {
		return *--m_curCmd;
	}
}

std::string ShellCommands::getNextCommand() {
	if (m_cmdHistory.size() < 1) {
		return std::string();
	} else if (
		m_curCmd == m_cmdHistory.end() ||
		++m_curCmd == m_cmdHistory.end()
	) {
		return std::string();
	} else {
		return *m_curCmd;
	}
}

void ShellCommands::setError(const std::string str) {
	m_lastError = str;
}

void ShellCommands::setError(const boost::format fmt) {
	m_lastError = fmt.str();
}

std::string ShellCommands::getError() {
	return m_lastError;
}

std::string ShellCommands::getPathStr() const {
	std::string path;
	std::string slash;
	Object *recursivePath;
	if (m_curPath->getName() == "Hydranode") {
		return "/";
	} else {
		recursivePath = m_curPath;
		while (recursivePath->getParent()) {
			slash = path.size() ? "/" : "";
			path = recursivePath->getName() + slash + path;
			recursivePath = recursivePath->getParent();
		}
		return "/" + path;
	}
}

bool ShellCommands::cmdShutdown(Tokenizer) {
	try {
//		*m_socket << "Bye, bye." << Socket::Endl;
	} catch (...) {
		// silently ignored
	}

	Hydranode::instance().exit();
	return true;
}

bool ShellCommands::cmdDisconnect(Tokenizer) {
	ShellClient::getEventTable().postEvent(m_parent, EVT_DESTROY);
	return true;
}

// 'Filesystem' related
// ---
bool ShellCommands::cmdChangeDir(Tokenizer args) {
	if (++args.begin() == args.end()) {
		// Go back to root dir
		return true;
	}

	std::string dir = *++args.begin();

	// one dir up
	if (dir.size() > 1 && dir.substr(0, 2) == "..") {
		if (m_curPath->getParent()) {
			m_curPath = m_curPath->getParent();
			return true;
		}
	}

	// change to subdir
	Object *newPath = 0;
	for (Object::CIter i = m_curPath->begin(); i != m_curPath->end(); ++i) {
		if ((*i).second->getName() == dir) {
			newPath = (*i).second;
			break;
		}
	}

	if (!newPath) {
		setError(
			boost::format("%s: No such file or directory") %
			dir
		);
		return false;
	} else {
		m_curPath = newPath;
		return true;
	}
}

bool ShellCommands::cmdListDir(Tokenizer) {
	// Output sub-objects
	if (!m_curPath->hasChildren()) {
		return true;
	}

	for (Object::CIter i = m_curPath->begin(); i != m_curPath->end(); ++i) {
		*m_socket << (*i).second->getName() << "/" << Socket::Endl;
	}

	// Output data fields
	boost::format fmt("[%s] %|10t|%s");
	for (uint32_t i = 0; i < m_curPath->getDataCount(); ++i) {
		fmt % m_curPath->getFieldName(i) % m_curPath->getData(i);
		*m_socket << fmt.str() << Socket::Endl;
	}

	return true;
}

// Module related
// ---
bool ShellCommands::cmdListModules(Tokenizer) {
	ModManager::MIter i = ModManager::instance().begin();

	while (i != ModManager::instance().end()) {
		ModuleBase *mod = (*i).second;
		*m_socket << "\33[4m" << mod->getDesc();
		*m_socket << " (" << mod->getName() << ")\33[0m" <<Socket::Endl;
		boost::format fmt1(
			"  [Current] Upload:   %10s/s Download:   %10s/s "
			"Sockets: %d"
		);
		boost::format fmt2(
			"  [Session] Uploaded: %10s   Downloaded: %10s"
		);
		boost::format fmt3(
			"  [Overall] Uploaded: %10s   Downloaded: %10s"
		);
		fmt1 % Utils::bytesToString(mod->getUpSpeed());
		fmt1 % Utils::bytesToString(mod->getDownSpeed());
		fmt1 % mod->getSocketCount();
		fmt2 % Utils::bytesToString(mod->getSessionUploaded());
		fmt2 % Utils::bytesToString(mod->getSessionDownloaded());
		fmt3 % Utils::bytesToString(mod->getTotalUploaded());
		fmt3 % Utils::bytesToString(mod->getTotalDownloaded());
		*m_socket << fmt1.str() << Socket::Endl;
		*m_socket << fmt2.str() << Socket::Endl;
		*m_socket << fmt3.str() << Socket::Endl;
		++i;
	}

	return true;
}

bool ShellCommands::cmdLoadModules(Tokenizer args) {
	for (Tokenizer::iterator i = ++args.begin(); i != args.end(); ++i) {
		ModManager::instance().loadModule(*i);
	}

	return true;
}

bool ShellCommands::cmdUnloadModules(Tokenizer args) {
	for (Tokenizer::iterator i = ++args.begin(); i != args.end(); ++i) {
		ModManager::instance().unloadModule(*i);
		if (*i == "hnsh") {
			// we are unloaded, run!
			return true;
		}
	}

	return true;
}

// Download related
// ---
bool ShellCommands::cmdSearch(Tokenizer args) {
	if (++args.begin() == args.end()) {
		*m_socket << "Syntax:" << Socket::Endl;
		*m_socket << "  s[earch] [--<option>=<value>] <terms>";
		*m_socket << Socket::Endl;
		*m_socket << Socket::Endl;
		*m_socket << "Options:" << Socket::Endl;
		*m_socket << "  minsize  Minimum filesize" << Socket::Endl;
		*m_socket << "  maxsize  Maximum filesize" << Socket::Endl;
		*m_socket << "  type     Filetype" << Socket::Endl;
		*m_socket << Socket::Endl;
		*m_socket << "Filetype can be one of:" << Socket::Endl;
		*m_socket << "  vid  Video files" << Socket::Endl;
		*m_socket << "  aud  Audio files" << Socket::Endl;
		*m_socket << "  arc  Archived files" << Socket::Endl;
		*m_socket << "  doc  Documents" << Socket::Endl;
		*m_socket << "  pro  Programs" << Socket::Endl;
		*m_socket << "  img  Images" << Socket::Endl;

		return true;
	}

	if (m_currentSearch) {
		m_currentSearch->stop();
	}
	m_currentSearch.reset(new Search);

	// Parse arguments
	typedef std::pair<std::string, std::string> Arg;
	typedef std::vector<Arg>::iterator ArgIter;
	std::vector<Arg> argList;

	for (Tokenizer::iterator i = ++args.begin(); i != args.end(); ++i) {
		std::string key, val;

		// Figure out if it is a term to search for or an argument
		if ((*i).substr(0, 2) == "--") {
			uint32_t pos = (*i).find("=");
			key = (*i).substr(2, pos - 2);
			val = (*i).substr(pos + 1);
			argList.push_back(std::make_pair(key, val));

			logTrace("shell.client",
				boost::format("Found search argument '%s'='%s'")
				% key % val
			);
		} else {
			logTrace("shell.client",
				boost::format("Adding search term '%s'.") % *i
			);
			m_currentSearch->addTerm(*i);
		}
	}

	// Handle arguments
	using boost::lexical_cast;
	for (ArgIter i = argList.begin(); i != argList.end(); ++i) try {
		if ((*i).first == "minsize") {
			uint64_t mz = lexical_cast<uint64_t>((*i).second);
			m_currentSearch->setMinSize(mz);
		} else if ((*i).first == "maxsize") {
			uint64_t mz = lexical_cast<uint64_t>((*i).second);
			m_currentSearch->setMaxSize(mz);
		} else if ((*i).first == "type") {
			m_currentSearch->setType(helperStrToType((*i).second));
		} else {
			boost::format fmt("Unknown argument: '%s'");
			fmt % (*i).first;

			logWarning(fmt);
		}
	} catch (std::exception &e) {
		logError(
			boost::format("Error handling argument '%s': ") %
			e.what()
		);
		logError("Search command aborted");

		setError(
			boost::format("failed: argument '%s' is invalid") %
			(*i).first
		);
		m_currentSearch.reset();
		return false;
	}

	// Parsing is done, check if everything we need is in place
	if (!m_currentSearch->getTermCount()) {
		setError("please specify a term to search for");
		m_currentSearch.reset();
		return false;
	}

	m_searchResults.clear();
	m_currentSearch->addResultHandler(
		boost::bind(&ShellCommands::helperOnSearchEvent, this, _1)
	);

	logMsg("Initialising new search.");
	m_currentSearch->run();

	return true;
}

/**
 * Functor changing the destination path of temp files.
 */
struct DestChanger {
	DestChanger(const boost::filesystem::path &dest) : m_newDest(dest) {}
	void operator()(PartData *file) const {
		using namespace boost::filesystem;
		file->setDestination(m_newDest / path(file->getName(), native));
	}
	boost::filesystem::path m_newDest;
};

bool ShellCommands::cmdDownload(Tokenizer args) try {
	using namespace boost::filesystem;

	if (++args.begin() == args.end()) {
		*m_socket << "Syntax:" << Socket::Endl;
		*m_socket << "  d[ownload] ( <search result no.> | <link> )";
		*m_socket << Socket::Endl;

		setError("wrong syntax");
		return false;
	}

	Tokenizer::iterator it = args.begin();
	std::vector<uint32_t> toDownload = selectObjects(*++it);

	// allows overriding destination dir
	boost::signals::scoped_connection c;
	if (++it != args.end()) try {
		path newDest(*it, native);
		if (!exists(newDest) || !is_directory(newDest)) {
			boost::format fmt(
				"Destination directory '%s' doesn't exist."
			);
			fmt % newDest.native_directory_string();
			*m_socket << fmt.str() << Socket::Endl;
			return true;
		}
		c = FilesList::instance().onDownloadCreated.connect(
			DestChanger(newDest)
		);
		*m_socket << "Downloads destination: " << *it << Socket::Endl;
	} catch (std::exception &e) {
		boost::format fmt("Invalid destination path: %s");
		fmt % e.what();
		*m_socket << fmt.str() << Socket::Endl;
		return true;
	}

	// this loop is here to guarantee strong exception-safety - either
	// all requested results are downloaded, or none are.
	for (uint32_t i = 0; i < toDownload.size(); ++i) {
		(void)m_searchResults.at(toDownload[i]);
	}

	for (uint32_t i = 0; i < toDownload.size(); ++i) {
		m_searchResults.at(toDownload[i])->download();
	}

	return true;
} catch (std::runtime_error&) {
	std::string link;
	for (Tokenizer::iterator it = ++args.begin(); it != args.end(); ++it) {
		link += *it + " ";
	}
	link.erase(link.size() - 1);
	logTrace("shell.client",
		boost::format("Attempting to download link '%s'") %
		link
	);
	Search::downloadLink(link);

	return true;
} catch (std::out_of_range&) {
	setError("invalid search result");
	return false;
}
MSVC_ONLY(;)

bool ShellCommands::cmdListDownloads(Tokenizer args) {
	if (++args.begin() != args.end()) {
		int num = boost::lexical_cast<int>(*++args.begin());
		helperDownloadDetails(num);
		return true;
	}
	// print all downloads
	std::vector<uint64_t> completed;
	std::vector<uint64_t> sizes;
	std::vector<uint32_t> srcCounts;
	std::vector<uint32_t> speeds;

	m_downloads.clear();

	FilesList::CSFIter i = FilesList::instance().begin();
	for (; i != FilesList::instance().end(); ++i) {
		if (!(*i)->isPartial() || (*i)->getPartData()->getParent()) {
			continue;
		}
		PartData *pd = (*i)->getPartData();
		m_downloads.push_back(pd);

		completed.push_back(pd->getCompleted());
		sizes.push_back(pd->getSize());
		srcCounts.push_back(pd->getSourceCnt());
		speeds.push_back(pd->getDownSpeed());
	}

	if (!m_downloads.size()) {
		return true;
	}

	std::sort(
		m_downloads.begin(), m_downloads.end(),
		boost::bind(
			std::less<std::string>(),
			bind(&PartData::getName, _1),
			bind(&PartData::getName, _2)
		)
	);

	for (uint32_t num = 0; num < m_downloads.size(); ++num) {
		helperPrintDownload(num, m_downloads[num]);
	}

	uint64_t completedSize = 0;
	for_each(completed.begin(), completed.end(), completedSize += __1);
	uint64_t totalSize = 0;
	for_each(sizes.begin(), sizes.end(), totalSize += __1);
	uint32_t totalSources = 0;
	for_each(srcCounts.begin(), srcCounts.end(), totalSources += __1);
	uint64_t totalSpeed = 0;
	for_each(speeds.begin(), speeds.end(), totalSpeed += __1);
	float completedPerc = completedSize * 100.0 / totalSize;

	*m_socket << std::string(m_parent->getWidth() - 1, '_') << Socket::Endl;
	boost::format fmt(
		"Completed %s/%s (" COL_BCYAN "%5.2f%%" COL_NONE
		")%s, " COL_GREEN "%d" COL_NONE " sources"
	);
	fmt % Utils::bytesToString(completedSize);
	fmt % Utils::bytesToString(totalSize) % completedPerc;
	if (totalSpeed) {
		boost::format fmt2(
			", transferring @ " COL_BGREEN "%s/s" COL_NONE
		);
		fmt % (fmt2 % Utils::bytesToString(totalSpeed));
	} else {
		fmt % "";
	}
	fmt % totalSources;
	*m_socket << fmt.str() << Socket::Endl;

	return true;
}

bool ShellCommands::cmdCancelDownload(Tokenizer args) try {
	if (++args.begin() == args.end()) {
		*m_socket << "Syntax:" << Socket::Endl;
		*m_socket << "  c[ancel] <num>" << Socket::Endl;
		m_lastCancel.clear();
		return true;
	}

	std::vector<uint32_t> obj = selectObjects(*++args.begin());
	std::ostringstream verifyMessage;
	std::set<PartData*> toCancel;

	verifyMessage << "Are you sure you want to cancel ";
	verifyMessage << "following downloads:" << Socket::Endl;

	for (uint32_t i = 0; i < obj.size(); ++i) {
		if (m_downloads.size() > obj[i] && m_downloads[obj[i]]) {
			toCancel.insert(m_downloads[obj[i]]);
			verifyMessage << "  " << m_downloads[obj[i]]->getName();
			verifyMessage << Socket::Endl;
		} else {
			boost::format fmt("Download #%d doesn't exist.");
			*m_socket << (fmt % obj[i]).str() << Socket::Endl;
			return true;
		}
	}
	if (m_lastCancel != toCancel) {
		*m_socket << verifyMessage.str();
		*m_socket << "Re-enter this command to verify." << Socket::Endl;
		m_lastCancel = toCancel;
		return true;
	} else {
		std::set<PartData*>::iterator it = toCancel.begin();
		while (it != toCancel.end()) {
			*m_socket << "Canceling download '" << (*it)->getName();
			*m_socket << "'" << Socket::Endl;
			(*it++)->cancel();
		}
		m_lastCancel.clear();
	}
	return true;
} catch (std::exception &e) {
	*m_socket << "Object selection failed: " << e.what() << Socket::Endl;
	return true;
}
MSVC_ONLY(;)

bool ShellCommands::cmdPauseDownload(Tokenizer args) try {
	if (++args.begin() == args.end()) {
		*m_socket << "Syntax: p[ause] <num>" << Socket::Endl;
		return true;
	}

	std::vector<uint32_t> obj = selectObjects(*++args.begin());
	std::vector<PartData*> toPause;

	for (uint32_t i = 0; i < obj.size(); ++i) {
		if (m_downloads.size() > obj[i] && m_downloads[obj[i]]) {
			toPause.push_back(m_downloads[obj[i]]);
		} else {
			boost::format fmt("Download #%d doesn't exist.");
			*m_socket << (fmt % obj[i]).str() << Socket::Endl;
			return true;
		}
	}

	for (uint32_t i = 0; i < toPause.size(); ++i) {
		*m_socket << "Pausing download '" << toPause[i]->getName();
		*m_socket << "'" << Socket::Endl;
		toPause[i]->pause();
	}

	return true;
} catch (std::exception &e) {
	*m_socket << "Object selection failed: " << e.what() << Socket::Endl;
	return true;
}
MSVC_ONLY(;)

bool ShellCommands::cmdStopDownload(Tokenizer args) try {
	if (++args.begin() == args.end()) {
		*m_socket << "Syntax: st[op] <num>" << Socket::Endl;
		return true;
	}

	std::vector<uint32_t> obj = selectObjects(*++args.begin());
	std::vector<PartData*> toStop;

	for (uint32_t i = 0; i < obj.size(); ++i) {
		if (m_downloads.size() > obj[i] && m_downloads[obj[i]]) {
			toStop.push_back(m_downloads[obj[i]]);
		} else {
			boost::format fmt("Download #%d doesn't exist.");
			*m_socket << (fmt % obj[i]).str() << Socket::Endl;
			return true;
		}
	}

	for (uint32_t i = 0; i < toStop.size(); ++i) {
		*m_socket << "Stopping download '" << toStop[i]->getName();
		*m_socket << "'" << Socket::Endl;
		toStop[i]->stop();
	}

	return true;
} catch (std::exception &e) {
	*m_socket << "Object selection failed: " << e.what() << Socket::Endl;
	return true;
}
MSVC_ONLY(;)

bool ShellCommands::cmdResumeDownload(Tokenizer args) try {
	if (++args.begin() == args.end()) {
		*m_socket << "Syntax: r[esume] <num>" << Socket::Endl;
		return true;
	}

	std::vector<uint32_t> obj = selectObjects(*++args.begin());
	std::vector<PartData*> toResume;

	for (uint32_t i = 0; i < obj.size(); ++i) {
		if (m_downloads.size() > obj[i] && m_downloads[i]) {
			toResume.push_back(m_downloads[obj[i]]);
		} else {
			boost::format fmt("Download #%d doesn't exist.");
			*m_socket << (fmt % obj[i]).str() << Socket::Endl;
			return true;
		}
	}

	for (uint32_t i = 0; i < toResume.size(); ++i) {
		*m_socket << "Resuming download '" << toResume[i]->getName();
		*m_socket << "'" << Socket::Endl;
		toResume[i]->resume();
	}

	return true;
} catch (std::exception &e) {
	*m_socket << "Object selection failed: " << e.what() << Socket::Endl;
	return true;
}
MSVC_ONLY(;)

bool ShellCommands::cmdLinkDownloads(Tokenizer args) try {
	if (++args.begin() == args.end()) {
		*m_socket << "Syntax: links <num>" << Socket::Endl;
		return true;
	}

	std::vector<uint32_t> obj = selectObjects(*++args.begin());
	std::vector<PartData*> toLink;
	for (uint32_t i = 0; i < obj.size(); ++i) {
		if (m_downloads.size() > obj[i] && m_downloads[i]) {
			toLink.push_back(m_downloads[obj[i]]);
		} else {
			boost::format fmt("Download #%d doesn't exist.");
			*m_socket << (fmt % obj[i]).str() << Socket::Endl;
			return true;
		}
	}

	for (uint32_t i = 0; i < toLink.size(); ++i) {
		std::vector<std::string> links;
		toLink[i]->getLinks(toLink[i], links);
		*m_socket << toLink[i]->getName() << Socket::Endl;
		if (links.size()) {
			for (uint32_t j = 0; j < links.size(); ++j) {
				*m_socket << links[j] << Socket::Endl;
			}
		} else {
			*m_socket << "No links to display - module owning this";
			*m_socket << " download not loaded?" << Socket::Endl;
		}
		if (toLink.size() > 1 && i + 1 < toLink.size()) {
			std::string line(m_parent->getWidth(), '-');
			*m_socket << line << Socket::Endl;
		}
	}

	return true;
} catch (std::exception &e) {
	*m_socket << "Object selection failed: " << e.what() << Socket::Endl;
	return true;
}
MSVC_ONLY(;)

// Misc. commands
// ---
bool ShellCommands::cmdListMessages(Tokenizer args) {
	uint32_t num = 5;
	if (++args.begin() != args.end()) {
		try {
			num = boost::lexical_cast<uint32_t>(*++args.begin());
		} catch (boost::bad_lexical_cast&) {
			setError(
				boost::format("Bad argument: '%s'; "
				"Expecting number.") %
				*++args.begin()
			);
			return false;
		}
	}

	std::vector<std::string> msgs;
	Log::instance().getLast(num, &msgs);
	while (msgs.size()) {
		*m_socket << msgs.back() << Socket::Endl;
		msgs.pop_back();
	}

	return true;
}

/** @todo Show only commands that are really implemented */
bool ShellCommands::cmdHelp(Tokenizer) {
	*m_socket << Hydranode::instance().getAppVerLong()      << Socket::Endl;
	*m_socket << "Available commands:"                      << Socket::Endl;
	*m_socket << "exit          Exit this session."         << Socket::Endl;
	*m_socket << "shutdown      Shut down Hydranode."       << Socket::Endl;
	*m_socket << "help          Display this help message." << Socket::Endl;
	*m_socket << "lsmod         List loaded modules."       << Socket::Endl;
	*m_socket << "modprobe      Load a module."             << Socket::Endl;
	*m_socket << "rmmod         Unload a module."           << Socket::Endl;
	*m_socket << "dmesg [n]     List last [n] log messages."<< Socket::Endl;
	*m_socket << "ls            List entries in current dir."<<Socket::Endl;
	*m_socket << "cd            Change directory."          << Socket::Endl;
	*m_socket << "uname         Show Hydranode version."    << Socket::Endl;
	*m_socket << "search        Search for files."          << Socket::Endl;
	*m_socket << "ss            Stop current search."       << Socket::Endl;
	*m_socket << "vr [pred]     View search results."       << Socket::Endl;
	*m_socket << "download      Download a search result or link.";
	*m_socket << Socket::Endl;
	*m_socket << "vd            View downloads."            << Socket::Endl;
	*m_socket << "cancel        Cancel a download."         << Socket::Endl;
	*m_socket << "pause         Pause a download."          << Socket::Endl;
	*m_socket << "stop          Stop a download."           << Socket::Endl;
	*m_socket << "resume        Resume a download."         << Socket::Endl;
	*m_socket << "links [num]   Display links for download."<< Socket::Endl;
	*m_socket << "addsource     Add a source to download."  << Socket::Endl;
	*m_socket << "hs            View Hasher Statistics."    << Socket::Endl;
	*m_socket << "scs           View Scheduler Statistics." << Socket::Endl;
	*m_socket << "ifs           View IPFilter Statistics."  << Socket::Endl;
	*m_socket << "log [on/off]  Enable/Disable log printing."<<Socket::Endl;
	*m_socket << "share [dir] -r Share folder, optionally recursivly.";
	*m_socket << Socket::Endl;
#if !defined(NDEBUG) && !defined(NTRACE)
	*m_socket << "trace         Enable/Disable/View Trace Masks.";
	*m_socket << Socket::Endl;
#endif
	*m_socket << Socket::Endl;

	ModManager::MIter i = ModManager::instance().begin();
	while (i != ModManager::instance().end()) {
		if ((*i).second->getOperCount()) {
			boost::format fmt("%s ...%|14t|%s");
			fmt % (*i).second->getName() % "Custom commands";
			*m_socket << fmt.str() << Socket::Endl;
		}
		++i;
	}

	if (m_curPath->getOperCount()) {
		*m_socket << "Context-sensitive commands: " << Socket::Endl;
		helperPrintObjOpers(m_curPath);
	}

	return true;
}

bool ShellCommands::cmdShowInfo(Tokenizer) {
	*m_socket << Hydranode::instance().getAppVerLong() << Socket::Endl;
	return true;
}

bool ShellCommands::cmdShowHasherStats(Tokenizer) {
	boost::format fmt("Hasher: %s hashed in %fs (%s/s)");
	fmt % Utils::bytesToString(HashWork::getHashed());
	fmt % HashWork::getTime();

	if (HashWork::getTime() != 0.0) {
		fmt % Utils::bytesToString(
			static_cast<uint64_t>(
				HashWork::getHashed()/HashWork::getTime()
			)
		);
	} else {
		fmt % "0 b";
	}

	*m_socket << fmt.str() << Socket::Endl;

	return true;
}

bool ShellCommands::cmdShowHistory(Tokenizer) {
	if (m_cmdHistory.size() > 0) {
		*m_socket << "Command history:" << Socket::Endl;

		uint16_t width = m_parent->getWidth();
		for (uint32_t i = 0; i < m_cmdHistory.size(); ++i) {
			std::string cmd = "  ";
			cmd += m_cmdHistory[i];

			if (cmd.size() > width) {
				uint16_t size = (width - 4) / 2;
				std::string beg = cmd.substr(0, size);
				std::string end = cmd.substr(cmd.size() - size);
				cmd = beg + "[...]" + end;
			}

			*m_socket << cmd << Socket::Endl;
		}
	} else {
		*m_socket << "No commands in history." << Socket::Endl;
	}

	return true;
}

bool ShellCommands::cmdSchedStats(Tokenizer) {
	using Utils::bytesToString;

	uint64_t totalDown = SchedBase::instance().getTotalDownstream();
	uint64_t totalUp = SchedBase::instance().getTotalUpstream();

	*m_socket << "Scheduler statistics: Uploaded: ";
	*m_socket << bytesToString(totalUp);
	*m_socket << " Downloaded: ";
	*m_socket << bytesToString(totalDown);
	*m_socket << Socket::Endl;
	boost::format fmt("%d open connections, %d half-open connections.");
	fmt % SchedBase::instance().getConnCount();
	fmt % SchedBase::instance().getConnectingCount();
	*m_socket << fmt.str() << Socket::Endl;
	boost::format fmt2("%d incoming packets, %d/s (avg %d bytes/packet)");
	boost::format fmt3("%d outgoing packets, %d/s (avg %d bytes/packet)");

	uint64_t downPackets = SchedBase::instance().getDownPackets();
	uint64_t upPackets = SchedBase::instance().getUpPackets();
	uint64_t timeDiff = 0;
	timeDiff = Utils::getTick() - Hydranode::instance().getStartTime();
	timeDiff /= 1000;
	uint64_t downPerSec = downPackets / timeDiff;
	uint64_t upPerSec = upPackets / timeDiff;
	uint64_t downAvg = totalDown / downPackets;
	uint64_t upAvg = totalUp / upPackets;

	fmt2 % downPackets % downPerSec % downAvg;
	fmt3 % upPackets % upPerSec % upAvg;

	*m_socket << fmt2.str() << Socket::Endl;
	*m_socket << fmt3.str() << Socket::Endl;

	return true;
}

bool ShellCommands::cmdFilterStats(Tokenizer) {
	boost::format fmt("IPFilter has blocked %d connections.");
	fmt % SchedBase::instance().getBlocked();
	*m_socket << fmt.str() << Socket::Endl;
	return true;
}

bool ShellCommands::cmdImport(Tokenizer args) {
	if (++args.begin() == args.end()) {
		*m_socket << "Please specify path to import from.";
		*m_socket << Socket::Endl;
		return true;
	}
	if (FilesList::instance().import.empty()) {
		*m_socket << "No import filters installed." << Socket::Endl;
		return true;
	}

	*m_socket << "Importing files from " << *++args.begin();
	*m_socket << Socket::Endl;
	using namespace boost::filesystem;
	FilesList::instance().import(path(*++args.begin(), native));
	return true;
}

bool ShellCommands::cmdConfig(Tokenizer args) {
	if (++args.begin() == args.end()) {
		printConfigValues();
	} else if (*++args.begin() == "list") {
		printConfigValues();
	} else if (*++args.begin() == "set") {
		Tokenizer::iterator it(++args.begin());
		++it;
		std::string key, val;
		if (it == args.end()) {
			printConfigHelp();
			return true;
		}
		key = *it++;
		if (it == args.end()) {
			printConfigHelp();
			return true;
		} else {
			val = *it;
		}
		if (key.size() && key[0] != '/') {
			key.insert(0, "/");
			bool ret = Prefs::instance().write(key, val);
			val = Prefs::instance().read<std::string>(key, "");
			if (ret) {
				*m_socket << "Ok - " << key.substr(1);
				*m_socket << " set to " << val << Socket::Endl;
			} else {
				*m_socket << "Value changing failed.";
				*m_socket << Socket::Endl;
			}
		}
	} else if (*++args.begin() == "get") {
		Tokenizer::iterator it(++args.begin());
		++it;
		if (it == args.end()) {
			printConfigHelp();
			return true;
		}
		std::string key = *it;
		if (key.size() && key[0] != '/') {
			key.insert(0, "/");
		}
		std::string val = Prefs::instance().read<std::string>(key, "");
		*m_socket << key.substr(1) << " = " << val << Socket::Endl;
	} else {
		printConfigHelp();
	}
	return true;
}

bool ShellCommands::cmdLog(Tokenizer args) {
	if (++args.begin() == args.end()) {
		*m_socket << "Enables/disables logging output; ";
		*m_socket << "arguments: on/off" << Socket::Endl;
		return true;
	} else if (*++args.begin() == "on") {
		m_parent->showLog(true);
	} else if (*++args.begin() == "off") {
		m_parent->showLog(false);
	} else {
		*m_socket << "Enables/disables logging output; ";
		*m_socket << "arguments: on/off" << Socket::Endl;
	}
	return true;
}

bool ShellCommands::cmdAddSource(Tokenizer args) {
	if (++args.begin() == args.end()) {
		*m_socket << "Syntax: addsource #dlnum source" << Socket::Endl;
		return true;
	}
	try {
		Tokenizer::iterator it = ++args.begin();
		uint32_t num = boost::lexical_cast<uint32_t>(*it);
		PartData *pd = m_downloads.at(num);
		CHECK_THROW(++it != args.end());
		pd->addSource(pd, *it);
	} catch (boost::bad_lexical_cast&) {
		*m_socket << "Invalid download number." << Socket::Endl;
	} catch (std::out_of_range&) {
		*m_socket << "No such download." << Socket::Endl;
	} catch (std::exception&) {
		*m_socket << "Missing source to be added." << Socket::Endl;
	}
	return true;
}

bool ShellCommands::cmdViewClients(Tokenizer args) {
	if (++args.begin() == args.end()) {
		*m_socket << "ClientManager statistics:" << Socket::Endl;
		boost::format fmt1("  Total:       %d");
		boost::format fmt2("  Connected:   %d");
		boost::format fmt3("  Sources:     %d");
		boost::format fmt4("  Queued:      %d");
		boost::format fmt5("  Uploading:   %d");
		boost::format fmt6("  Downloading: %d");
		ClientManager::ConnectedClients c1;
		c1 = ClientManager::instance().getConnected();
		ClientManager::SourceClients c2;
		c2 = ClientManager::instance().getSources();
		ClientManager::QueuedClients c3;
		c3 = ClientManager::instance().getQueued();
		ClientManager::UploadingClients c4;
		c4 = ClientManager::instance().getUploading();
		ClientManager::DownloadingClients c5;
		c5 = ClientManager::instance().getDownloading();
		fmt1 % ClientManager::instance().count();
		fmt2 % std::distance(c1.first, c1.second);
		fmt3 % std::distance(c2.first, c2.second);
		fmt4 % std::distance(c3.first, c3.second);
		fmt5 % std::distance(c4.first, c4.second);
		fmt6 % std::distance(c5.first, c5.second);
		*m_socket << fmt1.str() << Socket::Endl;
		*m_socket << fmt2.str() << Socket::Endl;
		*m_socket << fmt3.str() << Socket::Endl;
		*m_socket << fmt4.str() << Socket::Endl;
		*m_socket << fmt5.str() << Socket::Endl;
		*m_socket << fmt6.str() << Socket::Endl;
		*m_socket << "Info: type 'vc -h' for more options";
		*m_socket << Socket::Endl;
		return true;
	}
	Tokenizer::iterator it = ++args.begin();
	if (*it == "-h" || *it == "--help") {
		*m_socket << "Arguments: " << Socket::Endl;
		*m_socket << "-c       View connected clients" << Socket::Endl;
		*m_socket << "-c name  View connected clients for module";
		*m_socket << Socket::Endl;
		*m_socket << "name     View clients for module" <<Socket::Endl;
		*m_socket << "-s       View sources" << Socket::Endl;
		*m_socket << "-s num   View sources for file" << Socket::Endl;
		*m_socket << "-q       View upload queue" << Socket::Endl;
//		*m_socket << "-q num   View upload queue for file";
//		*m_socket << Socket::Endl;
		*m_socket << "-u       View uploading list" << Socket::Endl;
//		*m_socket << "-u num   View upload list for file";
//		*m_socket << Socket::Endl;
		*m_socket << "-d       View downloading clients"<<Socket::Endl;
		*m_socket << "-d num   View downloading clients for file";
		*m_socket << Socket::Endl;
	} else if (*it == "-c") {
		ClientManager::ConnectedClients c;
		if (++it == args.end()) {
			c = ClientManager::instance().getConnected();
		} else {
			ModuleBase *mod = ModManager::instance().find(*it);
			if (!mod) {
				*m_socket << "No such module." << Socket::Endl;
				return true;
			}
			c = ClientManager::instance().getConnected(mod);
		}
		for_each(
			c.first, c.second,
			boost::bind(&ShellCommands::printClient, this, _1)
		);
	} else if (*it == "-s") {
		ClientManager::SourceClients c;
		if (++it == args.end()) {
			c = ClientManager::instance().getSources();
		} else try {
			PartData *d = m_downloads.at(
				boost::lexical_cast<uint32_t>(*it)
			);
			c = ClientManager::instance().getSources(d);
		} catch (std::exception &) {
			*m_socket << "No such download." << Socket::Endl;
			return true;
		}
		for_each(
			c.first, c.second,
			boost::bind(&ShellCommands::printClient, this, _1)
		);
	} else if (*it == "-q") {
		ClientManager::QueuedClients c;
		c = ClientManager::instance().getQueued();
		for_each(
			c.first, c.second,
			boost::bind(&ShellCommands::printClient, this, _1)
		);
	} else if (*it == "-u") {
		ClientManager::UploadingClients c;
		c = ClientManager::instance().getUploading();
		for_each(
			c.first, c.second,
			boost::bind(&ShellCommands::printClient, this, _1)
		);
	} else if (*it == "-d") {
		ClientManager::DownloadingClients c;
		c = ClientManager::instance().getDownloading();
		for_each(
			c.first, c.second,
			boost::bind(&ShellCommands::printClient, this, _1)
		);
	} else {
		ModuleBase *mod = ModManager::instance().find(*it);
		if (!mod) {
			*m_socket << "Invalid argument or no such module.";
			*m_socket << Socket::Endl;
			*m_socket << "Enter 'vc' without arguments for help.";
			*m_socket << Socket::Endl;
			return true;
		}
		ClientManager::ModuleClients c;
		c = ClientManager::instance().find(mod);
		for_each(
			c.first, c.second,
			boost::bind(&ShellCommands::printClient, this, _1)
		);
	}
	return true;
}

void ShellCommands::printClient(BaseClient *c) {
	boost::format fmt(
		"\33[4m[%18s]\33[0m Network: %s | Nick: %s | Software: %s %s"
	);
	fmt % c->getAddr() % c->getModule()->getName() % c->getNick();
	fmt % c->getSoft() % c->getSoftVersion();
	*m_socket << fmt.str() << Socket::Endl;
	if (c->getSessionUploaded() || c->getSessionDownloaded()) {
		boost::format fmt2(" -> Uploaded: %s(%s) Downloaded: %s(%s)");
		fmt2 % Utils::bytesToString(c->getSessionUploaded());
		fmt2 % Utils::bytesToString(c->getTotalUploaded());
		fmt2 % Utils::bytesToString(c->getSessionDownloaded());
		fmt2 % Utils::bytesToString(c->getTotalDownloaded());
		*m_socket << fmt2.str() << Socket::Endl;
	}
	if (c->isUploading()) {
		boost::format fmt2(" -> Uploading (to) file %s at %s/s");
		std::string fname = c->getReqFile()->getName();
		if (fname.size() > m_parent->getWidth() - 43u) {
			fmt2 % fname.substr(0, m_parent->getWidth() - 43u);
		} else {
			fmt2 % fname;
		}
		fmt2 % Utils::bytesToString(c->getUploadSpeed());
		*m_socket << fmt2.str() << Socket::Endl;
	}
	if (c->isDownloading()) {
		boost::format fmt2(" -> Downloading (from) file %s at %s/s");
		std::string fname = c->getSource()->getName();
		if (fname.size() > m_parent->getWidth() - 43u) {
			fmt2 % fname.substr(0, m_parent->getWidth() - 43u);
		} else {
			fmt2 % fname;
		}
		fmt2 % Utils::bytesToString(c->getDownloadSpeed());
		*m_socket << fmt2.str() << Socket::Endl;
	}
	if (c->isConnected() && (!c->isUploading() && !c->isDownloading())) {
		boost::format fmt2(" -> Connected: Upload: %s/s Download: %s/s");
		fmt2 % Utils::bytesToString(c->getUploadSpeed());
		fmt2 % Utils::bytesToString(c->getDownloadSpeed());
		*m_socket << fmt2.str() << Socket::Endl;
	}
	if (c->wantsUpload() && !c->isUploading()) {
		boost::format fmt2(
			" -> UploadQueue position for file %s at QR "
			COL_GREEN "%d" COL_NONE
		);
		std::string fname = c->getReqFile()->getName();
		if (fname.size() > m_parent->getWidth() - 45u) {
			fmt2 % fname.substr(0, m_parent->getWidth() - 45u);
		} else {
			fmt2 % fname;
		}
		fmt2 % c->getQueueRanking();
		*m_socket << fmt2.str() << Socket::Endl;
	}
	if (c->canDownload() && !c->isDownloading()) {
		boost::format fmt2(
			" -> Waiting for download for file %s at QR "
			COL_CYAN "%d" COL_NONE
		);
		std::string fname = c->getSource()->getName();
		if (fname.size() > m_parent->getWidth() - 45u) {
			fmt2 % fname.substr(0, m_parent->getWidth() - 45u);
		} else {
			fmt2 % fname;
		}
		fmt2 % c->getRemoteQR();
		*m_socket << fmt2.str() << Socket::Endl;
	}
}

void ShellCommands::printConfigHelp() {
	*m_socket << "config command arguments:" << Socket::Endl;
	*m_socket << " list                Lists all configuration values";
	*m_socket << Socket::Endl;
	*m_socket << " get <key>           Print the value of <key>";
	*m_socket << Socket::Endl;
	*m_socket << " set <key> <val>     Set the value of <key> to <val>";
	*m_socket << Socket::Endl;
}

void ShellCommands::printConfigValues() {
	Prefs::CIter it(Prefs::instance().begin());
	while (it != Prefs::instance().end()) {
		*m_socket << it->first.substr(1);
		*m_socket << " = " << it->second << Socket::Endl;
		++it;
	}
}

bool ShellCommands::cmdTrace(Tokenizer args) {
	typedef const std::set<std::string>& MaskSet;
	typedef const std::map<std::string, int>& BuiltInMaskMap;
	typedef std::map<std::string, int>::const_iterator BIter;
	typedef std::set<std::string>::const_iterator MIter;

	MaskSet masks(Log::instance().getStrMasks());
	MaskSet enabled(Log::instance().getEnabledStrMasks());
	BuiltInMaskMap builtIn(Log::instance().getInternalMasks());

	// enable/disable specified masks, as needed
	for (Tokenizer::iterator it(++args.begin()); it != args.end(); ++it) {
		std::string mask(*it);
		bool enable = mask[0] != '-';

		if (mask[0] == '+' || mask[0] == '-') {
			mask = mask.substr(1);
		}

		// check if a built-in mask matches
		BIter iter(builtIn.find(mask));
		if (iter != builtIn.end() && enable) {
			Log::instance().enableTraceMask((*iter).second, mask);
			continue;
		} else if (iter != builtIn.end() && !enable) {
			Log::instance().disableTraceMask((*iter).second);
			continue;
		}

		// enable string-mask instead
		if (enable) {
			Log::instance().enableTraceMask(mask);
		} else {
			Log::instance().disableTraceMask(mask);
		}
	}

	*m_socket << "Trace Masks:" << Socket::Endl;
	boost::format fmt("[%s] %s");
	for (BIter iter(builtIn.begin()); iter != builtIn.end(); ++iter) {
		fmt % (Log::instance().isEnabled((*iter).second) ? "x" : " ");
		fmt % (*iter).first;
		*m_socket << fmt.str() << Socket::Endl;
	}
	for (MIter iter(masks.begin()); iter != masks.end(); ++iter) {
		fmt % (enabled.find(*iter) == enabled.end() ? " " : "x");
		fmt % *iter;
		*m_socket << fmt.str() << Socket::Endl;
	}

	*m_socket << "To enable/disable masks, use +/-[maskname].";
	*m_socket << Socket::Endl;

	return true;
}

bool ShellCommands::cmdViewResults(Tokenizer args) {
	std::string pred;
	if (++args.begin() == args.end()) {
		pred = Prefs::instance().read<std::string>(
			"/SearchSortOrder", "rsrc"
		);
	} else {
		pred = *++args.begin();
	}
	sortResults(pred);
	printSearchResults(true);
	return true;
}

bool ShellCommands::cmdClear(Tokenizer) {
	for (uint32_t i = 1; i < m_parent->getHeight(); ++i) {
		*m_socket << Socket::Endl;
	}
	*m_socket << "\33[H";
	return true;
}

bool ShellCommands::cmdUptime(Tokenizer) {
	uint64_t timeDiff = 0;
	timeDiff = Utils::getTick() - Hydranode::instance().getStartTime();
	uint64_t seconds = 0, minutes = 0, hours = 0, days = 0, months = 0;
	seconds = timeDiff / 1000;
	if (seconds > 60) {
		minutes = seconds / 60;
		seconds = seconds - minutes * 60;
	}
	if (minutes > 60) {
		hours = minutes / 60;
		minutes = minutes - hours * 60;
	}
	if (hours > 24) {
		days = hours / 24;
		hours = hours - days * 24;
	}
	if (days > 30) {
		months = days / 30;
		days = days - months * 30;
	}
	if (months) {
		boost::format fmt("Uptime %dmo %dd %2d:%2d:%2d");
		fmt % months % days % hours % minutes % seconds;
		*m_socket << fmt.str();
	} else if (days) {
		boost::format fmt("Uptime %dd %2d:%2d:%2d");
		fmt % days % hours % minutes % seconds;
		*m_socket << fmt.str();
	} else {
		boost::format fmt("Uptime %2d:%2d:%2d");
		fmt % hours % minutes % seconds;
		*m_socket << fmt.str();
	}
	uint64_t totalUp = SchedBase::instance().getTotalUpstream();
	uint64_t totalDown = SchedBase::instance().getTotalDownstream();
	uint64_t avgUp = totalUp / (timeDiff / 1000);
	uint64_t avgDown = totalDown / (timeDiff / 1000);
	boost::format fmt(", speed averages %s/s up, %s/s down");
	fmt % Utils::bytesToString(avgUp) % Utils::bytesToString(avgDown);
	*m_socket << fmt.str() << Socket::Endl;
	return true;
}

bool ShellCommands::cmdStopSearch(Tokenizer) {
	if (m_currentSearch) {
		m_currentSearch->stop();
		m_currentSearch.reset();
	} else {
		*m_socket << "'ss': Stops current active search.";
		*m_socket << Socket::Endl;
	}
	return true;
}

bool ShellCommands::cmdCustom(Tokenizer args) {
	ModuleBase *mod = ModManager::instance().find(*args.begin());
	if (!mod) {
		setError("No such module or command.");
		return false;
	}

	if (++args.begin() == args.end() || *++args.begin() == "help") {
		*m_socket << "Available commands for module '" << *args.begin();
		*m_socket << "':" << Socket::Endl;
		Log::instance().addPreStr("  ");
		helperPrintObjOpers(mod);
		Log::instance().remPreStr("  ");
		return true;
	}
	std::string tmp;
	for (Tokenizer::iterator it = ++args.begin(); it != args.end(); ++it) {
		tmp += *it + " ";
	}
	helperCallObjOper(mod, Tokenizer(tmp));
	return true;
}

bool ShellCommands::cmdMemStats(Tokenizer args) {
	if (++args.begin() == args.end()) {
		uint32_t tmp = 0;
		FilesList::SFIter i = FilesList::instance().begin();
		while (i != FilesList::instance().end()) {
			if ((*i)->isPartial()) {
				tmp += (*i)->getPartData()->amountBuffered();
			}
			++i;
		}
		*m_socket << "Total file buffers: ";
		*m_socket << Utils::bytesToString(tmp) << Socket::Endl;
	} else {
		uint32_t n = boost::lexical_cast<uint32_t>(*++args.begin());
		if (m_downloads.size() < n && m_downloads.at(n)) {
			helperPrintDownload(n, m_downloads[n]);
			*m_socket << "Buffered data amount: ";
			*m_socket << Utils::bytesToString(
				m_downloads[n]->amountBuffered()
			) << Socket::Endl;
		} else {
			*m_socket << "No such download." << Socket::Endl;
		}
	}
	return true;
}

bool ShellCommands::cmdShare(Tokenizer args) {
	if (++args.begin() == args.end()) {
		*m_socket << "Usage: share <folder>" << Socket::Endl;
	} else {
		bool recurse = false;
		Tokenizer::iterator it = ++args.begin();
		std::string dir = *it;
		if (++it != args.end() && *it == "-r") {
			recurse = true;
		}
		FilesList::instance().addSharedDir(dir, recurse);
	}
	return true;
}

bool ShellCommands::cmdAlloc(Tokenizer args) {
	if (++args.begin() == args.end()) {
		*m_socket << "Usage: alloc <objects>" << Socket::Endl;
	}
	std::vector<uint32_t> obj(selectObjects(*++args.begin()));
	for (uint32_t i = 0; i < obj.size(); ++i) {
		if (obj[i] < m_downloads.size() && m_downloads[obj[i]]) {
			m_downloads[obj[i]]->allocDiskSpace();
		}
	}
	return true;
}

bool ShellCommands::cmdRehash(Tokenizer args) {
	if (++args.begin() == args.end()) {
		*m_socket << "Usage: rehash <objects>" << Socket::Endl;
	}
	std::vector<uint32_t> obj(selectObjects(*++args.begin()));
	for (uint32_t i = 0; i < obj.size(); ++i) {
		if (obj[i] < m_downloads.size() && m_downloads[obj[i]]) {
			m_downloads[obj[i]]->rehashCompleted();
		}
	}
	return true;
}

void ShellCommands::sortResults(const std::string &pred) {
	if (pred == "size") {
		sort(
			m_searchResults.begin(), m_searchResults.end(),
			boost::bind(
				std::greater<uint64_t>(),
				bind(&SearchResult::getSize, _1),
				bind(&SearchResult::getSize, _2)
			)
		);
	} else if (pred == "rsize") {
		sort(
			m_searchResults.begin(), m_searchResults.end(),
			boost::bind(
				std::less<uint64_t>(),
				bind(&SearchResult::getSize, _1),
				bind(&SearchResult::getSize, _2)
			)
		);
	} else if (pred == "name") {
		sort(
			m_searchResults.begin(), m_searchResults.end(),
			boost::bind(
				std::less<std::string>(),
				bind(&SearchResult::getName, _1),
				bind(&SearchResult::getName, _2)
			)
		);
	} else if (pred == "rname") {
		sort(
			m_searchResults.begin(), m_searchResults.end(),
			boost::bind(
				std::greater<std::string>(),
				bind(&SearchResult::getName, _1),
				bind(&SearchResult::getName, _2)
			)
		);
	} else if (pred == "src") {
		sort(
			m_searchResults.begin(), m_searchResults.end(),
			boost::bind(
				std::greater<uint32_t>(),
				bind(&SearchResult::getSources, _1),
				bind(&SearchResult::getSources, _2)
			)
		);
	} else if (pred == "rsrc") {
		sort(
			m_searchResults.begin(), m_searchResults.end(),
			boost::bind(
				std::less<uint32_t>(),
				bind(&SearchResult::getSources, _1),
				bind(&SearchResult::getSources, _2)
			)
		);
	} else {
		*m_socket << "Supported predicates: 'size rsize name ";
		*m_socket << "rname src rsrc'" << Socket::Endl;
		*m_socket << "Default: Config.SearchSortOrder=";
		*m_socket << Prefs::instance().read<std::string>(
			"/SearchSortOrder", "rsrc"
		) << Socket::Endl;
	}
}

// Helpers
// ---

#ifdef _MSC_VER  // MSVC has strnicmp instead of strncasecmp
	#define strncasecmp _strnicmp
#endif

// Oh the evilness. C string comparison functions. But WHY ON EARTH doesn't
// STL string have case-insensitive string comparison functions?!
FileType ShellCommands::helperStrToType(const std::string &name) {
	if (strncasecmp(name.c_str(), "vid", 3) == 0) {
		return FT_VIDEO;
	} else if (strncasecmp(name.c_str(), "aud", 3) == 0) {
		return FT_AUDIO;
	} else if (strncasecmp(name.c_str(), "arc", 3) == 0) {
		return FT_ARCHIVE;
	} else if (strncasecmp(name.c_str(), "doc", 3) == 0) {
		return FT_DOCUMENT;
	} else if (strncasecmp(name.c_str(), "pro", 3) == 0) {
		return FT_PROGRAM;
	} else if (strncasecmp(name.c_str(), "img", 2) == 0) {
		return FT_IMAGE;
	} else {
		throw std::runtime_error("Invalid file type.");
	}
}

void ShellCommands::helperOnSearchEvent(SearchPtr search) try {
	if (!search->getResultCount()) {
		return;
	}

	uint32_t cnt = m_searchResults.size();
	while (cnt < search->getResultCount()) {
		m_searchResults.push_back(search->getResult(cnt++));
	}

	sortResults("src");
	printSearchResults(false);

	m_parent->setPrompt();
} catch (std::exception &err) {
	// don't let exceptions up from event handler
	logError(err.what());
} MSVC_ONLY(;)

void ShellCommands::printSearchResults(bool printAll) {
	CHECK_THROW(m_socket);
	if (!m_searchResults.size()) {
		*m_socket << "No results to display." << Socket::Endl;
	}

	uint32_t toPrint = m_parent->getHeight() - 2;
	if (printAll) {
		toPrint = m_searchResults.size();
	}
	if (m_searchResults.size() < toPrint) {
		toPrint = m_searchResults.size();
		uint16_t emptyLines = m_parent->getHeight() - 2 - toPrint;
		// clear screen
		*m_socket << "\33[H";
		for (uint32_t i = 1; i < m_parent->getHeight(); ++i) {
			*m_socket << std::string(m_parent->getWidth(), ' ');
			*m_socket << Socket::Endl;
		}
		*m_socket << "\33[H";
		// navigate to correct position
		while (emptyLines--) {
			*m_socket << Socket::Endl;
		}
	} else if (m_searchResults.size() > toPrint) {
		toPrint -= 2;    // 2-line Notice line in the end
	}
	
	*m_socket << "\r   #   Size Src(Compl) Rating Filename" << Socket::Endl;

	boost::format fmt("%3d) %10s %4d(%d) %|27t|%3d %|30t|%s");
	for (uint32_t i = 0; i < toPrint; ++i) {
		SearchResultPtr sr(m_searchResults[i]);

		fmt % i % Utils::bytesToString(sr->getSize());
		fmt % sr->getSources() % sr->getComplete();
		fmt % static_cast<int>(sr->getRating());

		// can't truncate name if screen width is <31
		if (m_parent->getWidth() <= 31) {
			fmt % sr->getName();
		}
		// truncate name to fit on screen single line
		if (sr->getName().size() > m_parent->getWidth() - 31u) {
			std::string tmp(
				sr->getName().substr(
					0, m_parent->getWidth() - 40
				)
			);
			tmp += "[...]" + sr->getName().substr(
				sr->getName().size() - 4
			);
			fmt % tmp;
		} else {
			fmt % sr->getName();
		}
		*m_socket << fmt.str() << Socket::Endl;
	}
	if (toPrint < m_searchResults.size()) {
		*m_socket << 
		"  => Notice: Only the most popular results are printed. ";
		*m_socket << Socket::Endl;
		*m_socket << "  => To view all results, type 'vr'.";
		*m_socket << " To stop searching, type 'ss'.";
		*m_socket << Socket::Endl;
	}
}

void ShellCommands::helperPrintDownload(uint32_t num, PartData *pd) {
	float perc = pd->getCompleted() * 100.0 / pd->getSize();
	std::string buf("[          ]");
	for (uint32_t i = 0; i < 10; ++i) {
		if (perc > i * 10 && perc < (i + 1) * 10) {
			uint8_t c = static_cast<uint8_t>(perc) % 10;
			if (c > 0 && c < 4) {
				buf[i + 1] = '.';
			} else if (c >= 4 && c < 7) {
				buf[i + 1] = '-';
			} else if (c >= 7 && c < 10) {
				buf[i + 1] = '|';
			}
		} else if (perc && perc >= i * 10) {
			buf[i + 1] = '#';
		}
	}

	boost::format fmt1("%3d) " COL_BCYAN "%s" COL_NONE);
	fmt1 % num;
	// file name
	std::string name = pd->getName();
	if (name.size() > m_parent->getWidth() - 6u) {
		std::string ext = name.substr(name.size() - 4);
		name = name.substr(0, m_parent->getWidth() - (7 + 3 + 5));
		name.append("[...]" + ext);
	}
	fmt1 % name;
	*m_socket << fmt1.str() << Socket::Endl;
	boost::format fmt2(
		"     %s " COL_CYAN "%3.0f%%" COL_NONE" %9s/%9s %13s"
		COL_GREEN " %4d " COL_NONE "sources %6d"
	);
	fmt2 % buf % perc % Utils::bytesToString(pd->getCompleted());
	fmt2 % Utils::bytesToString(pd->getSize());
	// speed
	if (pd->getDownSpeed()) {
		boost::format tmp(COL_BGREEN "%10s/s " COL_NONE);
		fmt2 % (tmp % Utils::bytesToString(pd->getDownSpeed()));
	} else if (pd->isPaused()) {
		fmt2 % "  (paused)";
	} else if (pd->isStopped()) {
		fmt2 % "  (stopped)";
	} else if (pd->isComplete()) {
		fmt2 % " (completing)";
	} else {
		fmt2 % "";
	}
	fmt2 % pd->getSourceCnt();

	// calculate availability
	Detail::ChunkMap &c = pd->getChunks();
	RangeList64 r;
	for (Detail::CMPosIndex::iterator it = c.begin(); it != c.end(); ++it) {
		if ((*it).getAvail()) {
			r.merge(*it);
		}
	}
	uint64_t sum = 0;
	for (RangeList64::Iter i = r.begin(); i != r.end(); ++i) {
		sum += (*i).length();
	}
	uint32_t avail = pd->getSize() ? (sum * 100ull / pd->getSize()) : 100;
	if (pd->getFullSourceCnt()) {
		avail = 100;
	}
	if (avail < 100 && pd->isRunning()) {
		fmt2 % (boost::format(COL_BRED "%d%% avail" COL_NONE) % avail);
	} else if (pd->getDownSpeed()) {
		// display ETA instead
		uint64_t need = pd->getSize() - pd->getCompleted();
		fmt2 % Utils::secondsToString(need / pd->getDownSpeed(), 2);
	} else {
		fmt2 % "";
	}
	*m_socket << fmt2.str() << Socket::Endl;
}

void ShellCommands::helperDownloadDetails(uint32_t num) {
	if (m_downloads.size() <= num || !m_downloads[num]) {
		*m_socket << "No such download." << Socket::Endl;
		return;
	}
	PartData *pd = m_downloads[num];
	helperPrintDownload(num, pd);
	Detail::ChunkMap &c = pd->getChunks();

	uint32_t verified = 0, partial = 0, unavail = 0, pending = 0, nohash =0;
	uint32_t cnt = 0, availSum = 0;
	for (Detail::CMPosIndex::iterator it = c.begin(); it != c.end(); ++it) {
		if ((*it).isVerified()) ++verified;
		if ((*it).isPartial()) ++partial;
		if (!(*it).hasAvail()) ++unavail;
		if ((*it).isComplete() && !(*it).isVerified()) ++pending;
		if (!(*it).getHash()) ++nohash;
		++cnt;
		availSum += (*it).getAvail();
	}

	boost::format fmt("  Chunk status:%s%s%s%s%s%s");
	if (verified) {
		fmt % (boost::format(" verified (%d)") % verified);
	} else {
		fmt % "";
	}
	if (pending) {
		fmt % (
			boost::format(" hashing (%d)")
			% pending
		);
	} else {
		fmt % "";
	}
	if (nohash) {
		fmt % (boost::format(" no hash (%d)") % nohash);
	} else {
		fmt % "";
	}
	if (unavail) {
		fmt % (boost::format(" not available (%d)") % unavail);
	} else {
		fmt % "";
	}
	if (partial) {
		fmt % (
			boost::format(" partial (%d)")
			% partial
		);
	} else {
		fmt % "";
	}
	uint32_t avgAvail = cnt ? (availSum / cnt) : 0;
	fmt % (boost::format(" avg. avail (%d)") % avgAvail);
	*m_socket << fmt.str() << Socket::Endl;

	MetaData *md = pd->getMetaData();
	if (!md) {
		return;
	}

	if (std::distance(md->namesBegin(), md->namesEnd())) {
		*m_socket << "  File Names:" << Socket::Endl;
		// sort by frequency
		typedef std::multimap<
			uint32_t, std::string, std::greater<uint32_t>
		> NamesMap;
		NamesMap nm;
		MetaData::NameIter ni = md->namesBegin();
		while (ni != md->namesEnd()) {
			nm.insert(std::make_pair((*ni).second, (*ni).first));
			++ni;
		}

		uint32_t limit = 15; // limit to 15 most popular names
		for (NamesMap::iterator it = nm.begin(); it != nm.end(); ++it) {
			boost::format fmt("  [%4d] %s");
			fmt % (*it).first % (*it).second;
			*m_socket << fmt.str() << Socket::Endl;
			if (!--limit) {
				break;
			}
		}
	}

	if (std::distance(md->commentsBegin(), md->commentsEnd())) {
		*m_socket << "  Comments:" << Socket::Endl;
		MetaData::CommentIter i = md->commentsBegin();
		while (i != md->commentsEnd()) {
			*m_socket << "  " << *i++ << Socket::Endl;
		}
	}

	if (pd->hasChildren()) {
		*m_socket << "This package download contains the following";
		*m_socket << " files:" << Socket::Endl;
	}
	for (Object::CIter oi = pd->begin(); oi != pd->end(); ++oi) {
		PartData *d = dynamic_cast<PartData*>((*oi).second);
		if (d) {
			m_downloads.push_back(d);
			helperPrintDownload(m_downloads.size() - 1, d);
		}
	}

}

void ShellCommands::helperPrintObjOpers(Object *obj) {
	CHECK_THROW(obj);

	for (uint8_t i = 0; i < obj->getOperCount(); ++i) {
		Object::Operation o(obj->getOper(i));
		*m_socket << Log::instance().getPreStr() << o.getName();

		// Generate list of arguments.
		// First get all required arguments. These will be used w/o
		// their names, so we simply output them here.
		std::deque<std::string> requiredArgs;
		std::deque<std::pair<std::string, std::string> > options;
		for (uint8_t j = 0; j < o.getArgCount(); ++j) {
			if (o.getArg(j).isRequired()) {
				requiredArgs.push_back(o.getArg(j).getName());
			} else {
				std::pair<std::string, std::string> tmp;
				tmp = std::make_pair(
					o.getArg(j).getName(),
					helperGetTypeName(o.getArg(j).getType())
				);
				options.push_back(tmp);
			}
		}
		while (requiredArgs.size()) {
			*m_socket << " " << requiredArgs.front();
			requiredArgs.pop_front();
		}
		while (options.size()) {
			*m_socket << " --" << options.front().first;
			*m_socket << "=<" << options.front().second << ">";
			options.pop_front();
		}
		*m_socket << Socket::Endl;
	}
}

std::string ShellCommands::helperGetTypeName(DataType t) {
	switch (t) {
		case ODT_UNKNOWN: return "unknown_type";
		case ODT_INT:     return "int";
		case ODT_STRING:  return "string";
		case ODT_BOOL:    return "true|false";
		case ODT_CHOICE:  return "choice";
		default:          return "unknown_type";
	}
}

void ShellCommands::helperCallObjOper(Object *obj, Tokenizer tok) {
	CHECK_THROW(obj);

	if (obj->getOperCount() == 0) {
		throw CommandNotFound();
	}
	int found = -1;
	for (uint8_t num = 0; num < obj->getOperCount(); ++num) {
		std::string cmd(obj->getOper(num).getName());
		if (cmd == *tok.begin()) {
			found = num; // exact match - break out
			break;
		}
		if (cmd.size() < (*tok.begin()).size()) {
			continue;
		}
		if (cmd.substr(0, (*tok.begin()).size()) == *tok.begin()) {
			if (found < 0) {
				found = num;
			} else {
				// ambigious
				setError("Ambigious command.");
				throw CommandNotFound();
			}
		}
	}
	if (found < 0) {
		throw CommandNotFound();
	}

	// Make local copy of the operation for simplicity
	Object::Operation oper(obj->getOper(found));

	// find all argumens the object has that are 'required'
	std::deque<std::string> required;
	for (uint8_t j = 0; j < oper.getArgCount(); ++j) {
		if (oper.getArg(j).isRequired()) {
			required.push_back(oper.getArg(j).getName());
			logTrace("shell.client",
				boost::format("Required argument: %s")
				% required.back()
			);
		} else {
			logTrace("shell.client",
				boost::format("Argument `%s` is not required.")
				% oper.getArg(j).getName()
			);
		}
	}

	// Ok, now it gets interesting - arguments parsing
	std::vector<Object::Operation::Argument> args;
	for (Tokenizer::iterator i = ++tok.begin(); i != tok.end(); ++i) {
		// An argument always has a name and a value.
		std::string argn, argv;

		// Deliminater, separates argn and argv
		size_t pos = (*i).find('=');

		// First try to find out the argument name. This is begin..pos
		if ((*i).substr(0, 2) == "--") {
			argn = (*i).substr(2, pos);
			// Now we need value
			if (pos == std::string::npos) {
				argv = "1";    // Assume boolean true
			} else {
				argv = (*i).substr(pos+2);
			}
		} else {
			if (required.size()) {
				argn = required.front();
				required.pop_front();
				argv = *i;
			} else {
				throw std::runtime_error("invalid argument");
			}
		}
		args.push_back(Object::Operation::Argument(argn, argv));
	}
	std::deque<std::string>::iterator it = required.begin();
	std::string error;
	while (it != required.end()) {
		error += *it++ + " ";
	}
	if (required.size()) {
		throw std::runtime_error(
			"Missing required argument(s): " + error
		);
	}

	obj->doOper(Object::Operation(oper.getName(), args));
}

void ShellCommands::onPDEvent(PartData *pd, int evt) {
	if (evt != PD_DESTROY) {
		return;
	}
	for_each(
		m_downloads.begin(), m_downloads.end(),
		if_(__1 == pd)[__1 = reinterpret_cast<PartData*>(0)]
	);
	m_lastCancel.clear();
}

std::vector<uint32_t> ShellCommands::selectObjects(const std::string &input) {
	using namespace boost::spirit;
	using boost::lexical_cast;
	using namespace boost::algorithm;

	std::vector<uint32_t> ret;
	std::vector<std::string> subStrings;
	subStrings = split(subStrings, input, is_any_of(","));
	uint32_t first, last;
	rule<> parser = uint_p[assign_a(first)] >> '-'
		>> uint_p[assign_a(last)]
	;

	for (uint32_t i = 0; i < subStrings.size(); ++i) {
		if (parse(subStrings[i].c_str(), parser).full) {
			if (last < first) {
				std::swap(first, last);
			}
			ret.push_back(first);
			while (first++ != last) {
				ret.push_back(first);
			}
			continue;
		} else try {
			ret.push_back(lexical_cast<uint32_t>(subStrings[i]));
			continue;
		} catch (std::exception&) {}
		// if we get here, something went wrong
		throw std::runtime_error("Invalid object selection sequence.");
	}

	return ret;
}

void ShellCommands::onModuleLoaded(ModuleBase *mod) {
	m_commands[mod->getName()] = boost::bind(
		&ShellCommands::cmdCustom, this, _1
	);
}

void ShellCommands::onModuleUnloaded(ModuleBase *mod) {
	m_commands.erase(mod->getName());
}

} // namespace Shell
