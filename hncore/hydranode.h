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
 * \file hydranode.h Main application header file
 */

#ifndef __HYDRANODE_H__
#define __HYDRANODE_H__

/**
 * \mainpage
 *
 * Hydranode Core - Modular Peer-to-peer client framework
 * \date   August 2004
 * \author Alo Sarv <madcat_ (at) users (dot) sourceforge (dot) net>
 *
 * \section maintainers Notes to Core Framework Maintainers
 *
 * As is known, programmers greatest enemy is second programmer. During
 * maintainance phase of a codebase, it is very easy to make the codebase
 * into <i>spaghetty</i> code. In order to prevent that, I have attempted to
 * write the initial implementation as safely as possible to keep the codebase
 * from falling apart as soon as tens of maintainers jump into it. Here are
 * some things you should keep in mind:
 *
 * - Coding Standard. I have used a very strict coding / documenting style in
 *   the codebase, and I'd prefer if it stayed that way. It is very important
 *   to keep the style same all over the codebase, as well as to keep
 *   documentation up to date. So if you modify a function, verify that the
 *   function's documentation reflects the new behaviour (do so even during
 *   trivial changes). Likewise, most files have "overview" documentation
 *   section at the top (headers). Review that on regular basis to keep that
 *   up to date. Now all this might sound very time-consuming and boring, but
 *   trust me - it pays off in long-term. I know you hate documenting, I did too
 *   until I had to maintain 100'000-line codebase with absolutely no
 *   documentation.
 *   <br><b>Follow the coding standard rules. Fully document all your code. Keep
 *   the documentation up to date.</b>
 * - Objects Decoupling. I have went through long sleepless nights in order
 *   to design the subsystems so that they would be decoupled maximally from
 *   each other. Systems communicate with each other at VERY MINIMAL base.
 *   Contained objects DO NOT know about the containers (e.g. MetaData and
 *   MetaDb, SharedFile and FilesList). The order of class declarations in
 *   headers reflects that idea - contained classes are declared prior to
 *   containers, so there won't be any chances they could 'accidentally' call
 *   container's functions (this last one applies only to inline functions in
 *   headers tho, you could still call the Container's functions in source
 *   files). If you really need to pass information from Containable to
 *   Container, use event tables (as I have done with MetaData and MetaDb).
 *   In short - if you REALLY need to pass messages over systems which, by
 *   common sense, shouldn't be aware of each other's existance, use event
 *   tables. They are the hammer of decoupling. Use them!
 *   <br><b>Do not couple together objects that are not supposed to know about
 *   each other!</b>
 * - Destructors. No, you can't do things in destructors. The reason for
 *   that is that most top-level container classes are Singletons and get
 *   destroyed during application shutdown <b>AFTER</b> exiting from main().
 *   And the order of destruction is undefined, so you CANNOT rely that
 *   any other systems are still up and running if you are in some destructor -
 *   it may be the application shutting down, and the other subsystems are
 *   already down.
 *   <br><b>Do not do anything except cleanup in destructors.</b>
 * - Testing Apps. Nearly each and every class and subsystem should have a
 *   corresponding test application, located in tests/ subdir, which tests the
 *   classes' or subsystems features and verifies it's integrity. These tests
 *   should immediately fail as soon as the target behaves differently than it
 *   should. ALWAYS run the corresponding tests after modifying the contents of
 *   a class or subsystem, no matter how trivial the change may sound. Also,
 *   when new bugs are found in subsystems, the FIRST step should be to update
 *   the corresponding test case to detect this bug, and only after that the
 *   bug itself be addressed. This ensures that codebase stays operational
 *   during maintainance, bugs mostly don't even make it to CVS, and old bugs
 *   do not return. Only exception here is cross-platform issues - not all
 *   developers have access to all supported platforms, so it's usually
 *   forgivable if the tests work on developer's platform, but fail on some
 *   other platform. But don't make this your habit - this means someone else
 *   must fix your broken code, and that some else isn't going to be happy about
 *   it. <br><b>Regularly regress-test your code. Write regress-tests for all
 *   new classes/subsystems. Keep regress-tests up-to-date with the codebase.
 *   </b><br>
 */

#include <hnbase/event.h>                        // EventSink
#include <hnbase/object.h>                       // Object
#include <hnbase/config.h>                       // Config
#include <boost/filesystem/path.hpp>             // getConfigDir() return val

enum HNEvent {
	EVT_EXIT = 0,
	EVT_SAVE_MDB   //! instructs to save MetaDb. Done every X minutes
};

/**
 * Main application class.
 */
class HNCORE_EXPORT Hydranode : public Object {
public:
	DECLARE_EVENT_TABLE(Hydranode*, HNEvent);

	//! Returns the single instance of this Singleton
	static Hydranode& instance();

	/**
	 * Does prelimiary initialization things, like starting random
	 * number generator, taking a copy of argc/argv etc. This is called
	 * from init() automatically, but can be called manually of calling
	 * init() is not wanted.
	 *
	 * @param argc        Number of command-line arguments
	 * @param argv        Command-line arguments
	 */
	void preInit(int argc, char **argv);

	/**
	 * Parses commandline given to application.
	 *
	 * @returns true if we should continue with startup, false if we should
	 *          exit immediately.
	 */
	bool parseCommandLine();

	/**
	 * Initialize Hydranode core.
	 *
	 * @param argc        Number of arguments
	 * @param argv        Arguments
	 */
	void init(int argc, char **argv);

	/**
	 * Runs the hydranode core, performing initialization, mainloop until
	 * exit command is received, and then cleanup.
	 *
	 * @param argc        Number of arguments
	 * @param argv        Arguments
	 * @return            0 on clean shutdown, nonzero error code otherwise.
	 */
	int run(int argc, char **argv);

	/**
	 * Called on shutdown, this method performs all cleanup and saving all
	 * settings.
	 *
	 * @return            0 on clean shutdown, nonzero error code otherwise.
	 */
	int cleanUp();

	/**
	 * Main event loop. This method enters Hydranode main loop, calling
	 * doLoop() until m_running is true, and then exits the application.
	 *
	 * @return            0 on clean exit, nonzero error code otherwise.
	 */
	int mainLoop();

	/**
	 * Perform a single event loop.
	 */
	void doLoop();

	/**
	 * Accessor, retrieves configuration directory
	 *
	 * @return   Current configuration directory
	 */
	boost::filesystem::path getConfigDir() {
		return m_confDir;
	}

	/**
	 * Exit the application.
	 */
	void exit() { m_running = false; }

	/**
	 * Retrieve the application version. The return value bytes are set as
	 * follows:
	 * [null] [major] [minor] [patch]
	 * Thus version 2.5.3 would return a 4-byte value with the following
	 * content:
	 * 0253
	 */
	uint32_t getAppVer() const;

	/**
	 * Get a human-readable application version string. The returned string
	 * is something like this:
	 * "Hydranode v0.4.6"
	 */
	std::string getAppVerLong() const;

	//! Check if the application is running. This indicates whether the
	//! application is inside main loop, e.g. not starting up or shutting
	//! down.
	bool isRunning() const { return m_running; }

	//! Returns absolute path to application executable dir
	boost::filesystem::path getAppPath() const;

	//! @name Various initialization functions
	//@{
	void initConfig();
	void initLog();
	void initSockets();
	void initModules();
	void initFiles();
	//@}

	//! Called after all other initialization is done; performs any final tasks
	void postInit();

	/**
	 * @returns The tick the application started
	 */
	uint64_t getStartTime() const { return m_startTime; }

	/**
	 * @returns reference to config file where all statistics-related things
	 *          should be stored.
	 */
	Config& getStats() { return m_stats; }

	/**
	 * @returns Total (for all sessions) uploaded data amount in bytes
	 */
	uint64_t getTotalUploaded() const;

	/**
	 * @returns Total (for all sessions) uploaded data amount in bytes
	 */
	uint64_t getTotalDownloaded() const;

	/**
	 * @returns Application runtime (session), in milliseconds.
	 */
	uint64_t getRuntime() const { 
		return EventMain::instance().getTick() - getStartTime(); 
	}

	/**
	 * @returns Application runtime (overall), in milliseconds.
	 */
	uint64_t getTotalRuntime() const {
		return getRuntime() + m_lastRuntime;
	}
private:
	/**
	 * Prints warranty and version info to stdout.
	 */
	void printInfo();

	/**
	 * Check for directories existance, and create it if neccesery.
	 *
	 * @param dir     Path to check/create.
	 * @param alt     Alternative directory name to be used if smth fails
	 * @return        The directory checked/created.
	 *
	 * If something goes terribly wrong in the directory checking/creation,
	 * this method attempts to use a subdirectory from current working
	 * directory. If that also fails, it will throw an exception. The return
	 * value indicates the actual directory after all these checks, and may
	 * differ from the original passed path.
	 */
	boost::filesystem::path checkCreateDir(
		boost::filesystem::path dir, const std::string &alt
	);

	//! Keeps track if we are running.
	bool m_running;

	//! Global configuration directory
	boost::filesystem::path m_confDir;

	//! Application binary build/modification date
	uint32_t m_buildDate;

	//! Application startup time
	uint64_t m_startTime;

	//! configuration file that stores statistics data
	Config m_stats;

	//! Modules to auto-load on startup (filled from commandline)
	std::vector<std::string> m_autoLoad;

	//! Statistics values
	uint64_t m_lastTotalUp, m_lastTotalDown, m_lastRuntime;

	//! Saves settings every now andt hen
	void saveSettings();

	/** @name Singleton */
	//@{
	Hydranode();                                        //!< Forbidden
	Hydranode(const Hydranode&);                        //!< Forbidden
	Hydranode& operator=(Hydranode&);                   //!< Forbidden
	~Hydranode();                                       //!< Forbidden
	//@}
};

#endif
