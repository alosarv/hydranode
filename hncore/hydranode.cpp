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

/**
 * \file hydranode.cpp Implementation of Hydranode class.
 */

#include <hncore/pch.h>
#include <hncore/hydranode.h>                  // Interface
#include <hncore/modules.h>                    // Modules subsystem init
#include <hncore/fileslist.h>                  // FMS init
#include <hncore/metadb.h>                     // MetaDb init
#include <hncore/ipfilter.h>                   // IPFilter init
#include <hnbase/sockets.h>                    // Sockets API init
#include <hnbase/schedbase.h>                  // Networking Scheduler
#include <hnbase/prefs.h>                      // Configuration Subsystem init
#include <hnbase/log.h>                        // Logging subsystem init
#include <hnbase/hostinfo.h>                   // Resolver init
#include <boost/filesystem/operations.hpp>     // Path operations for config
#include <boost/date_time/posix_time/posix_time.hpp> // for ptime
#include <boost/program_options.hpp>           // for parseCommandLine
#include <boost/algorithm/string/predicate.hpp>  // for iends_with
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <fcntl.h>

/**
 * @name Initialized from main(), these are passed from process starter.
 */
//@{
static char **s_argv = 0;
static int s_argc = 0;
//@}

static const uint32_t SAVE_INTERVAL = 60*1000*10; //!< 10 minutes

// Hydranode class
// ---------------
IMPLEMENT_EVENT_TABLE(Hydranode, Hydranode*, HNEvent);

//! Constructors/Destructors. Note: Don't do initialization here. That's what
//! run() member function is for.
Hydranode::Hydranode() : Object(0, "Hydranode"), m_running(false),
m_lastTotalUp(), m_lastTotalDown() {}
Hydranode::~Hydranode() {}

Hydranode& Hydranode::instance() {
	static Hydranode hn;
	return hn;
}

void Hydranode::preInit(int argc, char **argv) {
	s_argc = argc;
	s_argv = argv;
	boost::filesystem::path::default_name_check(boost::filesystem::native);

	m_buildDate = Utils::getModDate(argv[0]);
	m_startTime = Utils::getTick();

	// seeding the random number generator for proper randomness
	Utils::getRandom.seed(static_cast<uint32_t>(std::time(0)));
}

// Higher-level app entry point - do initialization here.
void Hydranode::init(int argc, char **argv) {
	preInit(argc, argv);

	logMsg("Initializing Hydranode Core...");
	Object::disableNotify();

	logMsg(" * Initializing Configuration...");
	Log::instance().addPreStr("     ");
	initConfig();
	Log::instance().remPreStr("     ");

	logMsg(" * Initializing Logging...");
	Log::instance().addPreStr("     ");
	initLog();
	Log::instance().remPreStr("     ");

	logMsg(" * Initializing Networking...");
	Log::instance().addPreStr("     ");
	initSockets();
	Log::instance().remPreStr("     ");

	// starting from here, we get to resource-intensive parts, so make sure
	// workthread doesn't start IO things before we'r finished with startup
	IOThread::Pauser p(IOThread::instance());

	logMsg(" * Initializing Shared Files List...");
	Log::instance().addPreStr("     ");
	initFiles();
	Log::instance().remPreStr("     ");

	logMsg(" * Initializing Modules...");
	Log::instance().addPreStr("     ");
	initModules();
	Log::instance().remPreStr("     ");

	postInit();

	Prefs::instance().save((m_confDir/"config.ini").string());
	Utils::timedCallback(this, &Hydranode::saveSettings, SAVE_INTERVAL);
	m_running = true;
	Object::enableNotify();
	logMsg("Hydranode Core is up and running.");
}

int Hydranode::run(int argc, char **argv) {
	s_argc = argc;
	s_argv = argv;

	if (!parseCommandLine()) {
		return 0;
	}

	init(argc, argv);
	mainLoop();
	return cleanUp();
}

int Hydranode::cleanUp() {
	logMsg("Exiting Hydranode Core...");
	Object::disableNotify();

	ModManager::instance().onExit();
	SchedBase::instance().exit();

	logMsg("Saving temp files...");
	FilesList::instance().savePartFiles(true);
	FilesList::instance().exit();

	logMsg("Saving MetaDb...");
	std::ofstream metadb(
		(getConfigDir()/"metadb.dat").string().c_str(),
		std::ios::binary
	);
	metadb << MetaDb::instance();
	MetaDb::instance().exit();

	logMsg("Saving configuration...");
	Prefs::instance().save();
	m_stats.write("/TotalUploaded",   getTotalUploaded());
	m_stats.write("/TotalDownloaded", getTotalDownloaded());
	m_stats.save();

	SocketWatcher::instance().cleanupSockets();

	logMsg("Hydranode exited cleanly.");

	return 0;
}

boost::filesystem::path Hydranode::checkCreateDir(
	boost::filesystem::path p, const std::string &alt
) {
	using namespace boost::filesystem;
	if (!exists(p)) try {
		create_directory(p);
		return p;
	} catch (std::exception&) {
		p = path(s_argv[0], native);
		p /= alt;
		if (!exists(p)) try {
			create_directory(p);
		} catch (std::exception &e) {
			logError(
				boost::format(
					"Unable to create directory %s: %s"
				) % p.string() % e.what()
			);
		}
		return p;
	}
	return p;
}

// On Win32, we use current working directory, plus "config" path. The
// idea is to have the app installed in Program Files\Hydranode, and
// have config dir at same point. On POSIX systems, we use system home
// directory and hidden subdir ".hydranode" there, as is standard on
// those platforms.
void Hydranode::initConfig() {
	using boost::filesystem::path;
	using boost::filesystem::native;

	if (m_confDir.empty()) {
		path confdir;

		std::string tmp(s_argv[0]);
		if (boost::algorithm::iends_with(tmp, ".exe")) {
			tmp.erase(tmp.size() - 4);
		}

#ifdef WIN32
		confdir = path(tmp, native).branch_path();
		confdir /= "config";
#else
		char *home = getenv("HOME");
		if (home == 0) {
			// No HOME environment variable, use subdir from cwd
			confdir = path(s_argv[0], native);
			confdir /= "config";
		} else {
			// Hidden subdir from $(HOME)
			confdir = path(home, native);
			confdir /= ".hydranode";
		}
#endif
		m_confDir = confdir;
	}
	m_confDir = checkCreateDir(m_confDir, "config");

	Prefs::instance().load((m_confDir/"config.ini").string());
	m_stats.load((m_confDir/"statistics.ini").string());
	m_lastTotalUp   = m_stats.read<uint64_t>("/TotalUploaded",   0);
	m_lastTotalDown = m_stats.read<uint64_t>("/TotalDownloaded", 0);
	m_lastRuntime   = m_stats.read<uint64_t>("/TotalRuntime",    0);

	path tmpDir = path(
		Prefs::instance().read<std::string>("Temp", ""), native
	);
	path incDir = path(
		Prefs::instance().read<std::string>("Incoming",""), native
	);
	if (tmpDir.empty()) {
		tmpDir = m_confDir/"temp";
	}
	if (incDir.empty()) {
		incDir = m_confDir/"incoming";
	}
	tmpDir = checkCreateDir(tmpDir, "temp");
	incDir = checkCreateDir(incDir, "incoming");
	Prefs::instance().write("Temp", tmpDir.native_directory_string());
	Prefs::instance().write("Incoming", incDir.native_directory_string());
	logMsg(
		boost::format("Configuration directory:   %s")
		% m_confDir.string()
	);
	logMsg(
		boost::format("Temporary files directory: %s") % tmpDir.string()
	);
	logMsg(
		boost::format("Incoming files directory:  %s") % incDir.string()
	);
}

// Initialize logging-related systems
void Hydranode::initLog() {
	using boost::filesystem::exists;
	using boost::filesystem::rename;
	using boost::filesystem::remove;

	std::string newName("hydranode.log"), oldName("hydranode.log.old");
	// if hydranode.log exists, rename to .old (removing previous .old)
	if (exists(getConfigDir()/newName)) {
		if (exists(getConfigDir()/oldName)) {
			remove(getConfigDir()/oldName);
		}
		rename(getConfigDir()/newName, getConfigDir()/oldName);
	}
	Log::instance().addLogFile(
		(getConfigDir()/newName).string()
	);
	using namespace boost;
	posix_time::ptime t1(posix_time::second_clock::local_time());
	logMsg(boost::format("Logging started on %s.") % t1);
	logMsg(
		boost::format("%s built on %s GMT.")
		% getAppVerLong() % posix_time::from_time_t(m_buildDate)
	);
}

// Initialize networking
void Hydranode::initSockets() {
	// Make sure these get initialized from main app, not from other thread
	// or - from some module.
//	(void)SocketClient::getEventTable();
//	(void)SocketServer::getEventTable();
//	(void)UDPSocket::getEventTable();
	(void)SchedBase::instance().init();
	(void)DNS::ResolverThread::instance();

	std::string fName= Prefs::instance().read<std::string>("/IPFilter", "");

	// don't attempt to load w/o filename
	if (!fName.size()) {
		return;
	}

	Prefs::instance().write<std::string>("/IPFilter", fName);

	IpFilter *flt = new IpFilter; // Note: this isn't deleted anywhere atm

	if (!boost::filesystem::path(fName).is_complete()) {
		fName = (getConfigDir()/fName).native_file_string();
	}
	if (fName.size()) try {
		flt->load(fName);
		SchedBase::instance().isAllowed.connect(
			boost::bind(&IpFilter::isAllowed, flt, _1)
		);
	} catch (std::exception &e) {
		logError(
			boost::format("Failed to load IPFilter: %s") % e.what()
		);
		delete flt;
	}
}

void Hydranode::initFiles() {
	(void)FilesList::instance();

	std::ifstream metadb(
		(getConfigDir()/"metadb.dat").string().c_str(),
		std::ios::binary
	);
	MetaDb::instance().load(metadb);

	// Get list of shared files dirs from prefs.
	Prefs::instance().setPath("/SharedDirs");
	uint32_t sharedCount = Prefs::instance().read("Count", 0);

	// Generate list of dirs to be added
	// This also has the added benefit of removing duplicate entries
	typedef std::map<std::string, bool> DirMap;
	DirMap dirs;
	while (sharedCount) {
		boost::format key("Dir_%s");
		key % sharedCount;
		std::string d =Prefs::instance().read(key.str(), std::string());
		bool rec = Prefs::instance().read(key.str()+"_recurse", false);
		// Also ignore empty default values in case of count/value
		// mismatch
		if (!d.empty()) {
			dirs.insert(std::make_pair(d, rec));
		}
		sharedCount--;
	}
	// Incoming dir is always shared
	Prefs::instance().setPath("/");
	std::string incDir = Prefs::instance().read<std::string>("Incoming","");
	dirs.insert(std::make_pair(incDir, false));

	Utils::StopWatch t;      // For performance testing/debugging
	for (DirMap::iterator i = dirs.begin(); i != dirs.end(); ++i) try {
		FilesList::instance().addSharedDir((*i).first, (*i).second);
	} catch (std::exception &er) {
		logError(
			boost::format("Adding shared directory %s:")
			% er.what()
		);
	}
	logMsg(
		boost::format("%d shared files loaded in %dms")
		% FilesList::instance().size() % t
	);

	// Temp files ...
	// primary temp dir
	std::string tmpDir = Prefs::instance().read<std::string>("/Temp", "");
	FilesList::instance().addTempDir(tmpDir);

	// additional temp dirs
	uint32_t cnt = Prefs::instance().read<uint32_t>("/TempCnt", 0);
	while (cnt) {
		boost::format fmt("Temp_%d");
		fmt % --cnt;
		tmpDir = Prefs::instance().read<std::string>(fmt.str(), "");
		FilesList::instance().addTempDir(tmpDir);
	}
}

void Hydranode::postInit() {
	if (!m_lastTotalDown || !m_lastTotalUp) {
		// upgrade-related: we prevously didn't store overall total
		// values, but we stored per-module totals, so calculate the
		// overall total from those then.
		m_lastTotalDown = 0;
		m_lastTotalUp = 0;
		ModManager::MIter i = ModManager::instance().begin();
		while (i != ModManager::instance().end()) {
			m_lastTotalDown += (*i).second->getTotalDownloaded();
			m_lastTotalUp   += (*i).second->getTotalUploaded();
			++i;
		}
	}
}

// Main event loop. Poll sockets, and handle events, until we exit. This isn't
// very elegant really, but it'll do for now (e.g. until someone has a better
// idea on how to do in a better way).
int Hydranode::mainLoop() {
	EventMain::initialize();
	m_running = true;
	while (m_running) {
		doLoop();
	}
	Hydranode::getEventTable().postEvent(this, EVT_EXIT);
	// Let ppl handle the EVT_EXIT too
	EventMain::instance().process();
	return true;
}

void Hydranode::doLoop() {
	EventMain::instance().process();
}

void Hydranode::initModules() {
	ModManager::instance().onInit();

#ifdef STATIC_BUILD
	return;
#endif

	std::vector<std::string>::iterator it(m_autoLoad.begin());
	while (it != m_autoLoad.end()) {
		boost::tokenizer<> tok(*it++);
		for_each(
			tok.begin(), tok.end(),
			bind(
				&ModManager::loadModule,
				&ModManager::instance(),
				_1
			)
		);
	}
	// if modules were loaded from commandline, don't load default ones
	// from prefs anymore
	if (m_autoLoad.size()) {
		return;
	}

	std::string mod = Prefs::instance().read<std::string>(
		"/LoadModules", "ed2k hnsh bt cgcomm"
	);
	Prefs::instance().write("/LoadModules", mod);
	std::vector<std::string> toLoad;
	boost::algorithm::split(toLoad, mod, boost::algorithm::is_any_of(" "));
	for_each(
		toLoad.begin(), toLoad.end(),
		boost::bind(&ModManager::loadModule, &ModManager::instance(),_1)
	);
}

uint32_t Hydranode::getAppVer() const {
	return (APPVER_MAJOR << 17 | APPVER_MINOR << 10 || APPVER_PATCH << 7);
}

std::string Hydranode::getAppVerLong() const {
	boost::format fmt("Hydranode v%d.%d.%d");
	fmt % APPVER_MAJOR % APPVER_MINOR % APPVER_PATCH;
	return fmt.str();
}

void Hydranode::saveSettings() {
	logMsg("Saving configuration...");
	ModManager::instance().saveSettings();
	Prefs::instance().save();
	m_stats.write("/TotalUploaded",   getTotalUploaded());
	m_stats.write("/TotalDownloaded", getTotalDownloaded());
	m_stats.write("/TotalRuntime",    getTotalRuntime());
	m_stats.save();

	logMsg("Saving MetaDb...");
	std::ofstream metadb(
		(getConfigDir()/"metadb.dat").string().c_str(),
		std::ios::binary
	);
	metadb << MetaDb::instance();

	logMsg("Saving temp files...");
	FilesList::instance().savePartFiles();

	Utils::timedCallback(this, &Hydranode::saveSettings, SAVE_INTERVAL);
}

bool Hydranode::parseCommandLine() try {
	namespace po = boost::program_options;

	uint32_t upLimit = 0;
	uint32_t dnLimit = 0;
	std::string confDir, moduleDir;
#ifdef WIN32
	bool transformColors = true;
#endif
	bool disableColors = false;

	po::options_description desc("Commandline options");
	desc.add_options()
		("help", "displays this message")
		("version,v", "version information")
#ifndef WIN32
		("background,b", "fork to background after startup")
#endif
		(
			"uplimit,u",
			po::value<uint32_t>(&upLimit)->default_value(25*1024),
			"upload speed limit"
		)
		(
			"downlimit,d",
			po::value<uint32_t>(&dnLimit)->default_value(0),
			"download speed limit (0 = unlimited)"
		)
		("disable-status", "disable statusbar printing")
		("quiet,q", "don't print any logging output")
#if !defined(NDEBUG) && !defined(NTRACE)
		(
			"trace,t",
			po::value< std::vector<std::string> >(),
			"enable trace masks"
		)
#endif
		(
			"load-modules,l",
			po::value< std::vector<std::string> >(&m_autoLoad),
			"load modules on startup"
		)
		(
			"config-dir,c",
			po::value<std::string>(&confDir),
			"override default configuration directory location"
		)
		(
			"disable-colors",
			po::value<bool>(&disableColors)->default_value(false),
			"enable/disable colored output"
		)
#ifdef WIN32
		(
			"transform-colors",
			po::value<bool>(&transformColors)->default_value(true),
			"enable/disable ANSI color codes transformation"
		)
#endif
		(
			"module-dir",
			po::value<std::string>(&moduleDir),
			"location where to search for modules"
		)
	;
	po::variables_map vm;
	po::store(po::parse_command_line(s_argc, s_argv, desc), vm);
	po::notify(vm);
	if (vm.count("help")) {
		printInfo();
		logMsg(boost::format("%s") % desc);
		return false;
	}
	if (vm.count("disable-status")) {
		SchedBase::instance().disableStatus();
	}
	if (vm.count("version")) {
		printInfo();
		return false;
	}
	if (vm.count("uplimit")) {
		SchedBase::instance().setUpLimit(upLimit);
	}
	if (vm.count("dnlimit")) {
		SchedBase::instance().setDownLimit(dnLimit);
	}
	if (vm.count("quiet")) {
		Log::instance().disableOutput();
	}
	if (vm.count("config-dir")) {
		m_confDir = boost::filesystem::path(
			confDir, boost::filesystem::native
		);
	}
	if (vm.count("module-dir")) {
		ModManager::instance().setModuleDir(moduleDir);
	}
	if (vm.count("disable-colors")) {
		Log::instance().setDisableColors(disableColors);
	}
#ifdef WIN32
	if (vm.count("transform-colors")) {
		Log::instance().setTransformColors(transformColors);
	}
#endif

#if !defined(NDEBUG) && !defined(NTRACE)
	if (vm.count("trace")) {
		typedef const std::map<std::string, int>& InternalMasks;
		typedef std::map<std::string, int>::const_iterator IIter;

		InternalMasks iMasks(Log::instance().getInternalMasks());

		std::vector<std::string> flags(
			vm["trace"].as<std::vector<std::string> >()
		);
		std::string tmp;

		// since we also accept comma-separated values, put them
		// all together into string and re-tokenize it together
		for_each(flags.begin(), flags.end(), (tmp += __1) += " ");
 		flags.clear();

		typedef boost::char_separator<char> Sep;
		Sep sep(", ");
		boost::tokenizer<Sep> tok(tmp, sep);
		boost::tokenizer<Sep>::iterator tik(tok.begin());
		while (tik != tok.end()) {
			flags.push_back(*tik++);
		}

		std::vector<std::string>::iterator it = flags.begin();
		tmp = "Enabling trace masks: ";
		while (it != flags.end()) {
			tmp += *it + " ";
			IIter iter(iMasks.find(*it));
			if (iter != iMasks.end()) {
				Log::instance().enableTraceMask(
					(*iter).second, *it
				);
			} else {
				Log::instance().enableTraceMask(*it);
			}
			++it;
		}
		logMsg(tmp);
	}
#endif

	// do backgrounding stuff after all other args have been processed
#ifndef WIN32
	if (vm.count("background")) {

		int pid = fork();
		if (pid < 0) {
			perror("fork() failed: ");
			::exit(1);
		} else if (pid == 0) {
			setsid();
			init(s_argc, s_argv);

			logMsg("Backgrounding...");

			// redirect stdout/stderr etc to /dev/null
			close(0); close(1); close(2);
			SchedBase::instance().disableStatus();
			Log::instance().disableOutput();
			int fd = open("/dev/null", O_RDWR);
			dup(fd); dup(fd); dup(fd);

			mainLoop();
			cleanUp();
		}

		return false;
	}
#endif

	return true;
} catch (std::exception &e) {
	logError(e.what());
	return false;
} catch (...) {
	logError("Unknown exception caught while parsing command line.");
	return false;
}
MSVC_ONLY(;)

void Hydranode::printInfo() {
	uint32_t buildDate = Utils::getModDate(s_argv[0]);
	using boost::posix_time::from_time_t;
	logMsg(
		boost::format("%s (built on %s GMT).")
		% getAppVerLong() % from_time_t(buildDate)
	);
	logMsg("Copyright (c) 2004-2006 Alo Sarv");
	logMsg(
		"Hydranode is free software;  see the source "
		"for copying conditions.  There is"
	);
	logMsg(
		"NO warranty; not even for MERCHANTABILITY or "
		"FITNESS FOR A PARTICULAR PURPOSE."
	);
	logMsg("");
}

boost::filesystem::path Hydranode::getAppPath() const {
#ifdef WIN32
	const char SEP = '\\';
#else
	const char SEP = '/';
#endif
	std::string ret = s_argv[0];
	while (ret.size() && *--ret.end() != SEP) {
		ret.erase(--ret.end());
	}
	return ret;
}

uint64_t Hydranode::getTotalUploaded() const {
	return m_lastTotalUp + SchedBase::instance().getTotalUpstream();
}

uint64_t Hydranode::getTotalDownloaded() const {
	return m_lastTotalDown + SchedBase::instance().getTotalDownstream();
}
