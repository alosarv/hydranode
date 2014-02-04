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
 * \file signals.cpp
 * Implements platform-specific signal handler for ctrl+c and similar,
 * performing automatic stack / log trace (where supported), and calling user-
 * defined exit function.
 */

#include <signal.h>
#include <boost/function.hpp>
#include <hnbase/log.h>
#include <hnbase/utils.h>

//! Keeps user-defined exit function to call on fatal exceptions / ctrl+c
boost::function<void ()> s_exitFunc;

/**
 * Handles termination event from system
 */
void on_terminate();

/**
 * Handles fatal exceptions event from system
 */
void on_fatal_exception();

/**
 * Signal handler (on POSIX systems only). Attempts to perform a clean shutdown
 * when SIGQUIT or SIGTERM is received.
 */
#ifdef WIN32
	bool onSignal(int signum);
#else
	void onSignal(int signum);
#endif

//! Signal handler for fatal crash signals
void onCrash(int signum);

void HNBASE_EXPORT initSignalHandlers(
	boost::function<void ()> handler, bool autoTrace
) {
	if (autoTrace) {
		std::set_terminate(&on_terminate);
		std::set_unexpected(&on_fatal_exception);
	}

	assert(!s_exitFunc);
#ifdef WIN32
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)onSignal, true);
#else
	#ifdef SIGQUIT
		signal(SIGQUIT, &onSignal);
	#endif
		signal(SIGTERM, &onSignal);
		signal(SIGINT,  &onSignal);
	#ifdef SIGXFSZ
		signal(SIGXFSZ, &onSignal);
	#endif
	#ifdef SIGPIPE
		signal(SIGPIPE, SIG_IGN); // Ignore SIGPIPE
	#endif
#endif
	s_exitFunc = handler;
}

#ifdef WIN32
bool onSignal(int signum) {
	s_exitFunc();
	return true;
}
#else
void onSignal(int signum) {
	logDebug(boost::format("Program received signal %d") % signum);
	if (
#ifdef SIGQUIT
		signum != SIGQUIT &&
#endif
		signum != SIGINT && signum != SIGTERM
	) {
		return;
	}
#ifdef SIGXFSZ
	if (signum == SIGXFSZ) {
		logWarning(
			"Program received signal SIGXFSZ "
			"(File size limit exceeded)"
		);
		return;
	}
#endif
	s_exitFunc();
}
#endif // ifdef WIN32

void on_terminate() {
        std::cerr << "\n\nOoops..." << std::endl;
	std::cerr << "Hydranode has performed an illegal operation ";
	std::cerr << "and has been shut down." << std::endl;
#ifdef __linux__
	std::cerr << "Call trace:" << std::endl;
        Utils::stackTrace();
#endif
	std::cerr << "\nAttempting to retrieve last log messages:\n";
	Log::printLast(10);
}

void on_fatal_exception() {
        std::cerr << "\n\nOoops..." << std::endl;
	std::cerr << "A fatal exception happened within Hydranode.";
#ifdef __linux__
	std::cerr << "\n\nCall trace:" << std::endl;
	Utils::stackTrace();
#endif
	std::cerr << "\nAttempting to retrieve last log messages:\n";
	Log::printLast(10);
}
