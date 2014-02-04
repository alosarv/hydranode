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
 * \file modules.cpp Implementation of Modules Management Subsystem
 */

#include <hncore/pch.h>
#include <hncore/modules.h>
#include <hncore/hydranode.h>
#include <hnbase/log.h>
#include <hnbase/schedbase.h>
#include <hnbase/prefs.h>
#include <hnbase/timed_callback.h>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem/operations.hpp>
 
//! Platform-specific stuff
#if defined(unix) || defined(__NetBSD__) || defined(__OpenBSD__)
	#define HAVE_LIBDL
#endif

#ifdef WIN32
	#include <windows.h>
	static const std::string MOD_EXT = ".dll";
	static const std::string PATH_SEPARATOR = "\\";
	std::string getError() {
		LPVOID lpMsgBuf;
		DWORD dw = GetLastError();

		FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM,
			NULL,
			dw,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR) &lpMsgBuf,
			0, NULL
		);
		std::string msg((LPTSTR)lpMsgBuf);
		LocalFree(lpMsgBuf);

		// get rid of newline(s)
		for (uint32_t i = 0; i < msg.size(); ++i) {
			if (msg[i] == '\n' || msg[i] == '\r') {
				msg.erase(i--, 1);
				--i;
			}
		}

		return (boost::format("%s (error %d)") % msg % dw).str();
	}
#elif defined(HAVE_LIBDL)
	#include <dlfcn.h>
	static const std::string MOD_EXT = ".so";
	static const std::string PATH_SEPARATOR = "/";
	std::string getError() {
		#ifndef STATIC_BUILD
			const char *ret = dlerror();
			return ret ? ret : "no error";
		#else
			return "";
		#endif
	}
#elif defined(__MACH__)
	#include <mach-o/dyld.h>
	static const std::string MOD_EXT = ".so";
	static const std::string PATH_SEPARATOR = "/";
	std::string getError() {
		int null;
		NSLinkEditErrors c;
		const char *errorMsg;  // error message
		const char *modName;   // module name
		NSLinkEditError(&c, &null, &modName, &errorMsg);
		return errorMsg ? errorMsg : "no error";
	}
#else
	#error No dynamic loading support implemented for your platform
#endif

/**
 * Built-in modules
 *
 * This function contains built-in modules initializers in static vector, and
 * allows accessing them. The reason for this kind of implementation is to have
 * at least some control of static initialization order; if implemented using
 * static file-scope variable, things go wrong during initialization.
 *
 * \param n      Request to return the initializer at this position
 * \param i      Add initializer to the vector
 * \return       0 when adding, or when requesting invalid position; nonzero
 *               pointer to InitializerBase object otherwise.
 */
Detail::InitializerBase* builtIn(uint8_t n = 0, Detail::InitializerBase *i = 0){
	static std::vector<Detail::InitializerBase*> s_vec;
	if (i) {
		s_vec.push_back(i);
	} else if (n < s_vec.size()) {
		return s_vec.at(n);
	}
	return 0;
}

namespace Detail {
	// InitializerBase class
	// ---------------------
	InitializerBase::InitializerBase() {
		builtIn(0, this);
	}
	InitializerBase::~InitializerBase() {}
}

// ModuleBase class
// ----------------
ModuleBase::ModuleBase(const std::string &name)
: Object(&ModManager::instance(), name), m_priority(PR_NORMAL), m_name(name),
m_sesUploaded(), m_sesDownloaded(), m_totalUploaded(), m_totalDownloaded(),
m_socketCount(), m_upSpeedLimit(), m_downSpeedLimit(), m_lastSentLocal(),
m_lastSentGlobal(), m_lastCheckTick() {
	migrateSettings();
	Config &stats = Hydranode::instance().getStats();
	m_totalUploaded = stats.read<uint64_t>(
		"/" + m_name + "/TotalUploaded", 0
	);
	m_totalDownloaded = stats.read<uint64_t>(
		"/" + m_name + "/TotalDownloaded", 0
	);
	getUpSpeed();
	getDownSpeed();
	checkStartUpload();
}
ModuleBase::~ModuleBase() {}

std::string ModuleBase::getDesc() const { return m_name; }

void ModuleBase::addUploaded(uint32_t amount) {
	m_sesUploaded   += amount;
	m_totalUploaded += amount;
}
void ModuleBase::addDownloaded(uint32_t amount) {
	m_sesDownloaded   += amount;
	m_totalDownloaded += amount;
}
void ModuleBase::saveSettings() {
	Config &stats = Hydranode::instance().getStats();
	stats.write("/" + m_name + "/TotalUploaded", m_totalUploaded);
	stats.write("/" + m_name + "/TotalDownloaded", m_totalDownloaded);
}

void ModuleBase::migrateSettings() {
	Config &oldConfig = Prefs::instance();
	oldConfig.setPath("/" + m_name);
	uint64_t up = oldConfig.read<uint64_t>("TotalUploaded", 0);
	uint64_t dn = oldConfig.read<uint64_t>("TotalDownloaded", 0);
	Config &newConfig = Hydranode::instance().getStats();
	newConfig.setPath("/" + m_name);
	uint64_t newUp = newConfig.read<uint64_t>("TotalUploaded", 0);
	uint64_t newDn = newConfig.read<uint64_t>("TotalDownloaded", 0);
	if (up && !newUp) {
		newConfig.write("TotalUploaded", up);
	}
	if (dn && !newDn) {
		newConfig.write("TotalDownloaded", dn);
	}
	if ((up && !newUp) || (dn && !newDn)) {
		logMsg(
			"Info: Statistics values moved to statistics.ini."
		);
	}
}

// checks and opens upload slots within this module based on multiple conditions
//
// note that if module has upload limit set, we only test against the module's
// upload limit, since the purpose is for each module to use up all it's upload
// bandwidth; hence, module that's not using up all it's allowed bandwidth
// (e.g. which gets distributed to other modules then by scheduler) opens more
// upload slots, thus forcing scheduler to re-arrange traffic, hopefully enough
// to satisfy this module's upload speed limit.
//
// the upload speed limit can change at any time (the scheduler can dynamically
// adjust module's upload limits, based on various factors [including humans])
// so calculations done in here should keep that in mind.
void ModuleBase::checkStartUpload() {
	if (m_lastSentLocal) {
		uint64_t curTick = Utils::getTick();
		uint64_t curGlobal = SchedBase::instance().getTotalUpstream();
		uint64_t curLocal = getSessionUploaded();
		float tickDiff = (curTick - m_lastCheckTick) / 1000.0f;
		uint32_t avgGlobal = static_cast<uint32_t>(
			(curGlobal - m_lastSentGlobal) / tickDiff
		);
		uint32_t avgLocal = static_cast<uint32_t>(
			(curLocal - m_lastSentLocal) / tickDiff
		);
		bool openSlot = true;
		if (getUpLimit()) {
			openSlot &= avgLocal < getUpLimit() * .85;
			openSlot &= getUpSpeed() < getUpLimit() * .9;
		} else {
			uint32_t gUpLimit = SchedBase::instance().getUpLimit();
			uint64_t gUpSpeed = SchedBase::instance().getUpSpeed();
			openSlot &= avgGlobal < gUpLimit * .85;
			openSlot &= gUpSpeed < gUpLimit * .9;
		}
		if (openSlot) {
			openUploadSlot();
		}
		logTrace(TRACE_MOD,
			boost::format(
				"[%s] Average: %s/s Current: %s/s AvgGlobal: "
				"%s/s CurGlobal: %s/s %s"
			) % getName() % Utils::bytesToString(avgLocal)
			% Utils::bytesToString(getUpSpeed())
			% Utils::bytesToString(avgGlobal)
			% Utils::bytesToString(
				SchedBase::instance().getUpSpeed()
			) % (openSlot ? "(slot opened)" : "")
		);
	}
	m_lastSentLocal = getSessionUploaded();
	m_lastSentGlobal = SchedBase::instance().getTotalUpstream();
	m_lastCheckTick = Utils::getTick();
	Utils::timedCallback(
		boost::bind(&ModuleBase::checkStartUpload, this), 10000
	);
}

void ModuleBase::openUploadSlot() {}

// ModManager class
// ----------------
//! Constructor
ModManager::ModManager() : Object(&Hydranode::instance(), "modules") {}
//! Destructor
ModManager::~ModManager() {}

ModManager& ModManager::instance() {
	static ModManager mm;
	return mm;
}

//! Called from Hydranode::run(), perform ModManager-specific initialization
void ModManager::onInit() {
//	Log::instance().enableTraceMask(TRACE_MOD, "modmanager");

	uint8_t i = 0;
	Detail::InitializerBase *toInit = 0;
	for (toInit = builtIn(i); toInit; toInit = builtIn(++i)) {
		ModuleBase *mod = toInit->doInit();
		logMsg(
			boost::format("= Initializing built-in module `%s`...")
			% mod->getName()
		);
		Log::instance().addPreStr("  [init_" + mod->getName() + "] ");
		mod->onInit();
		Log::instance().remPreStr("  [init_" + mod->getName() + "] ");
		m_list.insert(std::make_pair(mod->getName(), mod));
	}
	adjustLimits();
}

//! Unloads _all_ loaded modules
void ModManager::onExit() {
	boost::format preStr;
	while (m_list.size()) {
		preStr = boost::format("[exit_%s] ");
		preStr % ((*m_list.begin()).first);
		Log::instance().addPreStr(preStr.str());
		(*m_list.begin()).second->onExit();
		(*m_list.begin()).second->saveSettings();
		delete (*m_list.begin()).second;
		Log::instance().remPreStr(preStr.str());
		m_list.erase(m_list.begin());
	}
}

// Load a module [private]
HNMODULE ModManager::load(const std::string &name) {
#ifdef STATIC_BUILD
	return 0; // can't load plugins in static build
#endif

	CHECK(!name.empty());
#ifdef WIN32
	HNMODULE plg = LoadLibrary(name.c_str());
#elif defined(HAVE_LIBDL)
	HNMODULE plg = dlopen(name.c_str(), RTLD_LAZY | RTLD_GLOBAL);
#elif defined(__MACH__)
	NSObjectFileImage fileimage;
	NSObjectFileImageReturnCode returnCode;
	returnCode = NSCreateObjectFileImageFromFile(name.c_str(), &fileimage);
	HNMODULE plg;
	if (returnCode == NSObjectFileImageSuccess) {
		plg = NSLinkModule(
			&fileimage, name.c_str(),
			NSLINKMODULE_OPTION_RETURN_ON_ERROR
			| NSLINKMODULE_OPTION_NONE
		);
		NSDestroyObjectFileImage(&fileimage);
	}
#endif
	if (plg == 0) {
		throw std::runtime_error(getError());
	} else {
		return plg;
	}
}

// Initialize a module [private]
ModuleBase* ModManager::initialize(HNMODULE plg) {
#ifdef STATIC_BUILD
	return 0;
#endif

	CHECK_THROW(plg != 0);

	logTrace(TRACE_MOD, "Initializing module.");

	ModuleBase* (*initMod)() = 0;
#ifdef WIN32
	initMod = (ModuleBase*(*)())(GetProcAddress(plg, "onInit"));
#elif defined(HAVE_LIBDL)
	initMod = (ModuleBase*(*)())(dlsym(plg, "onInit"));
#elif defined(__MACH__)
	NSSymbol sym = NSLookupSymbolInModule(plg, "onInit");
	initMod = (ModuleBase*(*)())(NSAddressOfSymbol(sym));
#endif
	if (!initMod) {
		throw std::runtime_error("Failed to get symbol `onInit`.");
	}
	ModuleBase *module = initMod();
	if (!module) {
		throw std::runtime_error("Module initialization failed.");
	}
	if (module->onInit() == false) {
		throw std::runtime_error("Module initialization failed.");
	}
	return module;
}

// \todo Clean this up, this is ugly
bool ModManager::loadModule(const std::string &name) try {
#ifdef STATIC_BUILD // No loading in static builds
	logWarning("Cannot load modules in static build!");
	return false;
#endif
	if (m_list.find(name) != m_list.end()) {
		logDebug(boost::format(
			"Requested to load module `%s` which is already loaded."
		) % name);
		return false;
	}

	logMsg(boost::format("- Loading module `%s`.") % name);
	Log::instance().addPreStr("  ");

	// build platform-dependant module object name
	std::string mod = name;

	mod.insert(0, "cmod_");
#ifndef _WIN32
	mod.insert(0, "lib");
#endif
	mod.append(MOD_EXT);

	std::vector<std::string> paths;
	if (m_moduleDir.size()) {
		if (*--m_moduleDir.end() != PATH_SEPARATOR[0]) {
			m_moduleDir.append(PATH_SEPARATOR);
		}
		paths.push_back(m_moduleDir);
	}
	paths.push_back("." + PATH_SEPARATOR);
	paths.push_back("modules" + PATH_SEPARATOR);
	paths.push_back("plugins" + PATH_SEPARATOR);
#ifdef MODULE_DIR
	paths.push_back(MODULE_DIR + PATH_SEPARATOR);
#endif
	paths.push_back(
		Hydranode::instance().getAppPath().native_directory_string()
		+ PATH_SEPARATOR
	);
	paths.push_back(
		(Hydranode::instance().getAppPath()/"modules")
		.native_directory_string() + PATH_SEPARATOR
	);
	paths.push_back("");

	HNMODULE plg = 0;
	std::vector<std::string>::iterator i = paths.begin();
	std::string lastError("file not found");
	logTrace(TRACE_MOD, "Loading module '" + name + "' ...");
	for (; i != paths.end(); ++i) {
		// skip over non-existant or directory entries
		boost::format msg(" ... '%s': %s");
		msg % (*i + mod);
		if (!boost::filesystem::exists(*i + mod)) {
			logTrace(TRACE_MOD, msg % "not found");
			continue;
		} else if (boost::filesystem::is_directory(*i + mod)) {
			logTrace(TRACE_MOD, msg % "is directory");
			continue;
		}
		try {
			std::string toLoad(*i + mod);
			plg = load(toLoad);
			logTrace(TRACE_MOD, msg % "loaded successfully");
			lastError = "no error";
			break;
		} catch (std::exception &e) {
			lastError = e.what();
			logTrace(TRACE_MOD, msg % e.what());
		}
	}
	if (!plg) {
		logError(
			boost::format("Failed to load module %s: %s")
			% name % lastError
		);
		throw std::runtime_error(lastError);
	}

	const std::string preStr = (boost::format("[init_%s] ") % name).str();
	ModuleBase* module;
	try {
		Log::instance().addPreStr(preStr);
		module = initialize(plg);
		Log::instance().remPreStr(preStr);
	} catch (std::exception &err) {
		logError(
			boost::format("Failed to init module %s: %s")
			% name % err.what()
		);
		Log::instance().remPreStr(preStr);
		throw;
	}

	m_list.insert(std::make_pair(name, module));
	logMsg(boost::format("Module `%s` loaded successfully.") % name);
	Log::instance().remPreStr("  ");
	onModuleLoaded(module);
	return true;
} catch (std::exception &e) {
	logTrace(TRACE_MOD, boost::format("Error while loading: %s") %e.what());
	Log::instance().remPreStr("  ");
	return false;
}
MSVC_ONLY(;)

// Modules unloading... this has a myriad of problems. First we have dlclose()
// which corrupts stack. If we disable dlclose() call, then unload+load causes
// segfault due to singleton's re-instanciating in modules. So - disabling this
// functionality altogether for now.
bool ModManager::unloadModule(const std::string &name) {
#ifdef STATIC_BUILD // No unloading in static builds
	return false;
#endif

	logWarning("Sorry, modules unloading is not supported right now.");
	return false;

	logTrace(TRACE_MOD, boost::format("Unloading module %s") % name);

	Iter i = m_list.find(name);
	if (i == m_list.end()) {
		logWarning(boost::format(
			"Attempt to unload module `%s` which is not loaded."
		) % name);
		return false;
	}

	//! Call module's exit function
	(*i).second->onExit();
	onModuleUnloaded((*i).second);
	HNMODULE handle = (*i).second->m_handle;
	delete (*i).second;
	(*i).second = 0;

	// \todo Something is broken about modules unloading ... closing the
	// handle corrupts the stack.
	int ret = 0;
#ifdef WIN32
//	ret = FreeLibrary(handle);
#elif defined(HAVE_LIBDL)
//	ret = dlclose(handle);
#elif defined(__MACH__)
//      ret = NSUnLinkModule(handle, 0);
#endif
	(void)handle; // surpress compiler warning until the above line is fixed

	if (ret != 0) {
		logError(
			boost::format("Error unloading module `%s`: %s")
			% getError()
		);
		return false;
	}

	// Important:
	// Keep this line above m_list.erase() call, since we are passed
	// reference to the list string entry from onEvent() when application
	// is shutting down, and if we erase the original and attempt to use
	// it then for logMsg() call, we end up using a dead reference.
	logMsg(boost::format("Module `%s` unloaded.") % name);

	// Remove from list _after_ emitting log message.
	m_list.erase(name);

	return true;
}

//! Fill the passed vector with all loaded modules and their descriptions
void ModManager::getList(
	std::vector<std::pair<std::string, std::string> > *ret
) const {
	CHECK_THROW(ret != 0);

	for (MIter i = m_list.begin(); i != m_list.end(); ++i) {
		ret->push_back(
			std::make_pair((*i).first, (*i).second->getDesc())
		);
	}
}

void ModManager::saveSettings() {
	for (MIter i = m_list.begin(); i != m_list.end(); ++i) {
		(*i).second->saveSettings();
	}
}

ModuleBase* ModManager::find(const std::string &name) {
	MIter i = m_list.find(name);
	if (i != m_list.end()) {
		return (*i).second;
	}
	return 0;
}

void ModManager::adjustLimits() {
	uint64_t totalDown = 0, totalUp = 0;
	for (MIter i = m_list.begin(); i != m_list.end(); ++i) {
		if ((*i).second->getTotalUploaded()) { 
			// module is upload-capable
			totalDown += (*i).second->getTotalDownloaded();
			totalUp   += (*i).second->getTotalUploaded();
		}
	}
	for (MIter i = m_list.begin(); i != m_list.end(); ++i) {
		float ratio = 1.0 / m_list.size(); // divide equally by default
		if (totalDown) {
			ratio = (*i).second->getTotalDownloaded() / totalDown;
		}
		(*i).second->setUpLimit(
			static_cast<uint32_t>(
				SchedBase::instance().getUpLimit() * ratio
			)
		);
	}
	Utils::timedCallback(
		boost::bind(&ModManager::adjustLimits, this), 60 * 1000
	);
}
