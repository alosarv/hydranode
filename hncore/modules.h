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
 * \file modules.h Interface for Module Management Subsystem
 */

#ifndef __MODULES_H__
#define __MODULES_H__

#include <hnbase/osdep.h>                  // For HNCORE_EXPORT
#include <hnbase/object.h>                 // for Object
#include <hnbase/utils.h>                  // for Sum

#include <string>                          // For std::string
#include <vector>                          // For std::vector
#include <map>                             // For std::map

//! Typedef HNMODULE (Hydranode Module) depending on platform
#ifdef WIN32
	struct HINSTANCE__;
	typedef HINSTANCE__* HNMODULE;
#else
	typedef void* HNMODULE;
#endif

//! Forward-declare this for making ModuleBase friends with ModManager
class ModManager;
namespace Detail {
	struct InitializerBase;
	template<typename T>
	struct Initializer;
}

/**
 * Abstract base class for Modules. Plugins must derive their main class
 * from this class, and override the pure virtual functions onInit() and
 * onExit().
 */
class HNCORE_EXPORT ModuleBase : public Object {
public:
	ModuleBase(const std::string &name);
	virtual ~ModuleBase();

	/**
	 * Called from main application when the module is loaded.
	 *
	 * @return         True if the module initialized successfully.
	 *                 False otherwise. Note: If you return false from here,
	 *                 the module will be unloaded immediately, and onExit()
	 *                 will not be called.
	 */
	virtual bool onInit() = 0;

	/**
	 * Called from main application when the module is about to be unloaded.
	 *
	 * @return         Standard return value indicating module exit status.
	 *                 Exit status 0 means everything is ok, nonzero means
	 *                 some kind of an error. Note that the nonzero return
	 *                 value serves only as informative value - the module
	 *                 unloading will still proceed even if the return value
	 *                 is nonzero.
	 */
	virtual int onExit() = 0;

	/**
	 * Retrieve a descriptive module name.
	 *
	 * @return          Descriptive name for the module. The return value
	 *                  length should not exceed 40 characters. Overriding
	 *                  this virtual function is purely optional. The base
	 *                  class implementation of this function returns an
	 *                  empty string.
	 */
	virtual std::string getDesc() const;

	/**
	 * Returns the name of this module, as declared with DECLARE_MODULE
	 * macro.
	 */
	std::string getName() const { return m_name; }

	/**
	 * Module priority; this affects network scheduling policy for sockets
	 * of this module.
	 */
	enum ModulePriority {
		PR_LOW = -1,
		PR_NORMAL = 0,
		PR_HIGH = 1
	};

	int8_t getPriority() const { return m_priority; }
	void setPriority(ModulePriority pri) { m_priority = pri; }
	void saveSettings();

	/**
	 * @returns The current download speed for this module
	 */
	boost::signal<uint32_t (), Utils::Sum<uint32_t> > getDownSpeed;

	/**
	 * @returns The current upload speed for this module
	 */
	boost::signal<uint32_t (), Utils::Sum<uint32_t> > getUpSpeed;

	/**
	 * Add upload data to this module
	 *
	 * @param amount    Amount (in bytes) to be added
	 */
	void addUploaded(uint32_t amount);

	/**
	 * Add download data to this module
	 *
	 * @param amount     Amount (in bytes) to be added
	 */
	void addDownloaded(uint32_t amount);

	/**
	 * @name Accessors for counters et al
	 */
	//!@{
	uint64_t getSessionUploaded()   const { return m_sesUploaded;     }
	uint64_t getSessionDownloaded() const { return m_sesDownloaded;   }
	uint64_t getTotalUploaded()     const { return m_totalUploaded;   }
	uint64_t getTotalDownloaded()   const { return m_totalDownloaded; }
	uint32_t getSocketCount()       const { return m_socketCount;     }
	uint32_t getUpLimit()           const { return m_upSpeedLimit;    }
	uint32_t getDownLimit()         const { return m_downSpeedLimit;  }
	//!@}

	//! Add 1 socket to the socket counter
	void addSocket() { ++m_socketCount; }
	//! Remove 1 socket from the socket counter
	void delSocket() { assert(m_socketCount); --m_socketCount; }

	//! Set the recommended upload speed setting for this module
	void setUpLimit(uint32_t limit)   { m_upSpeedLimit = limit;   }
	//! Set the recommended download speed setting for this module
	void setDownLimit(uint32_t limit) { m_downSpeedLimit = limit; }
protected:
	/**
	 * Called when this module should open new upload slot; override this
	 * if your module supports uploading.
	 */
	virtual void openUploadSlot();
private:
	ModuleBase(); // Default constructor forbidden

	// copying is not allowed
	ModuleBase(const ModuleBase&);
	ModuleBase& operator=(const ModuleBase&);

	/**
	 * Callback event handler, checks if this module should open more
	 * upload slots. Calls openUploadSlot() virtual method when needed.
	 */
	void checkStartUpload();

	/**
	 * Originally, statistics values for modules were stored in config.ini;
	 * however, nowadays we store that in statistics.ini, so this method
	 * imports existing values from config.ini to statistics.ini if values
	 * in statistics.ini don't exist yet.
	 */
	void migrateSettings();

	//! Platform-specific handle for the module.
	HNMODULE m_handle;

	//! This modules' priority (networking)
	int8_t m_priority;

	//! ModManager needs to access the above handle.
	friend class ModManager;
	friend struct Detail::InitializerBase;

	//! Name of the module, as passed to constructor
	std::string m_name;

	uint64_t m_sesUploaded;
	uint64_t m_sesDownloaded;
	uint64_t m_totalUploaded;
	uint64_t m_totalDownloaded;
	uint32_t m_socketCount;
	uint32_t m_upSpeedLimit;
	uint32_t m_downSpeedLimit;

	// context data for upload-slot opening code
	uint64_t m_lastSentLocal;
	uint64_t m_lastSentGlobal;
	uint64_t m_lastCheckTick;
};

/**
 * Use this macro in your module's entrance class (derived from ModuleBase) to
 * initialize Module declaration. This also makes the class a Singleton, which's
 * instance may be retrieved using instance() member function from now on.
 *
 * @param Class      Name of the class being declared.
 * @param Name       Short, single-word name of the class, e.g. "ed2k" or "hnsh"
 */
#define DECLARE_MODULE(Class, Name)                                          \
	public:                                                              \
		static Class& instance() { return *s_instance##Class ; }     \
		Class() : ModuleBase(Name) { s_instance##Class = this; }     \
		static uint8_t getPriority() {                               \
			return s_instance##Class->ModuleBase::getPriority(); \
		}                                                            \
		static void setPriority(ModulePriority pri) {                \
			s_instance##Class->ModuleBase::setPriority(pri);     \
		}                                                            \
	private:                                                             \
		static Class *s_instance##Class

/**
 * Use this macro in your module's main implmenetation file to complete the
 * module implementation.
 *
 * @param Class     Name of the module's main class, derived from ModuleBase
 */
#ifndef BUILT_IN
	#define IMPLEMENT_MODULE(Class)                                       \
		Class* Class::s_instance##Class = 0;                          \
		extern "C" EXPORT ModuleBase* onInit() {                      \
			return new Class();                                   \
		} class Class
#else
	#define IMPLEMENT_MODULE(Class) \
		Class* Class::s_instance##Class = 0;                          \
		static ::Detail::Initializer<Class> s_initializer
#endif

namespace Detail {
	/**
	 * Base class for built-modules system; not to be used directly.
	 */
	struct InitializerBase {
		InitializerBase();
		virtual ~InitializerBase();
		virtual ModuleBase* doInit() = 0;
		virtual void doExit() = 0;
	};

	/**
	 * Specific initializer class for built-in modules; not to be used
	 * directly by user code.
	 */
	template<typename T>
	struct Initializer : InitializerBase {
		virtual ModuleBase* doInit() { return new T; }
		virtual void doExit() {
			Log::instance().addPreStr(
				"[exit_" + T::instance().getName() + "] "
			);
			T::instance().onExit();
			Log::instance().remPreStr(
				"[exit_" + T::instance().getName() + "] "
			);
		}
	};
}

/**
 * Central module manager, keeping track of which modules are currently
 * loaded, and providing an API for loading/unloading modules.
 */
class HNCORE_EXPORT ModManager : public Object {
public:
	/**
	 * Retrives the single instance of this Singleton class.
	 */
	static ModManager& instance();

	/**
	 * Load a module. The module is searched in standard module directories.
	 *
	 * @param name      Name of the module to be loaded.
	 * @return          True if loading succeeded, false otherwise.
	 */
	bool loadModule(const std::string &name);

	/**
	 * Unload a module. The module should have been previously loaded with
	 * loadModule() function.
	 *
	 * @param name      Module to be unloaded.
	 * @return          True if successful, false otherwise.
	 */
	bool unloadModule(const std::string &name);

	/**
	 * Get the list of loaded modules.
	 *
	 * @param ret       Vector which is to be filled with list of all loaded
	 *                  modules. First element in the vector pair is the
	 *                  unix name of the module, second is the module
	 *                  description.
	 */
	void getList(
		std::vector<std::pair<std::string, std::string> > *ret
	) const;

	/**
	 * Find module by name
	 *
	 * @param name        Name to search for
	 * @returns           Module if found, null if not found
	 */
	ModuleBase* find(const std::string &name);

	/**
	 * Saves module-related settings to Prefs class. Call this before
	 * saving Prefs to disk to ensure the values are updated properly.
	 */
	void saveSettings();

	/**
	 * Sets the location where plugins are searched (before checking other
	 * standard locations).
	 */
	void setModuleDir(const std::string &dir) { m_moduleDir = dir; }

	/**
	 * @returns User-defined module directory which is checked before any
	 *          other dirs.
	 */
	std::string getModuleDir() const { return m_moduleDir; }

	//! Iterator for the internal data structure
	typedef std::map<std::string, ModuleBase*>::const_iterator MIter;

	//! @returns iterator to the beginning of the modules listing
	MIter begin() const { return m_list.begin(); }

	//! @returns iterator to one-past-end of the modules listing
	MIter end() const { return m_list.end(); }

	//! Emitted when a module is loaded
	boost::signal<void (ModuleBase*)> onModuleLoaded;
	//! Emitted when module is unloaded
	boost::signal<void (ModuleBase*)> onModuleUnloaded;
private:
	ModManager();
	~ModManager();
	ModManager(const ModManager &);
	ModManager& operator=(const ModManager&);

	//! Map of loaded modules
	std::map<std::string, ModuleBase*> m_list;
	//! Iterator for the above map
	typedef std::map<std::string, ModuleBase*>::iterator Iter;
	//! User-defined module directory (from command-line)
	std::string m_moduleDir;

	friend class Hydranode;
	friend struct Initializer;

	/**
	 * Initialize ModManager class. Should be called from Hydranode class
	 * on application startup.
	 */
	void onInit();

	/**
	 * Shutdown/cleanup this class. Should be called on application
	 * shutdown from Hydranode class.
	 */
	void onExit();

	/**
	 * Load a module using os-specific methods.
	 *
	 * @param name         Path to the module to be loaded.
	 * @return             Module handle.
	 *
	 * \throws std::runtime_error if module loading fails for any reason.
	 */
	HNMODULE load(const std::string &name);

	/**
	 * Initialize a module using os-specific methods.
	 *
	 * @param plg          Module handle to be initialized, generally
	 *                     previously returned by load().
	 * @return             Pointer to abstract ModuleBase class representing
	 *                     this module.
	 *
	 * \throws std::runtime_error if anything goes wrong.
	 */
	ModuleBase* initialize(HNMODULE plg);

	/**
	 * Adjusts each module's recommended transfer speed settings based on
	 * historical data.
	 */
	void adjustLimits();
};


#endif
