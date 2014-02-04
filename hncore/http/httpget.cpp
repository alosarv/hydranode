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

#include <hncore/hydranode.h>
#include <hncore/http/http.h>
#include <hncore/http/client.h>
#include <boost/filesystem/operations.hpp>
#include <boost/algorithm/string/predicate.hpp>

extern void initSignalHandlers(boost::function<void ()>, bool);

int main(int argc, char *argv[]) {
#if defined(__GNUC__) && (__GNUC__ == 3 && __GNUC_MINOR__ < 4)
	std::set_terminate(__gnu_cxx::__verbose_terminate_handler);
#endif
	initSignalHandlers(
		boost::bind(&Hydranode::exit, &Hydranode::instance()), false
	);

	if (!argv[1] || !boost::algorithm::istarts_with(argv[1], "http://")) {
		logError("Required argument: [URL]");
		return 1;
	}

	std::string file(argv[1]);

	// bring up configuration before creating downloads
	Hydranode::instance().preInit(argc, argv);
	if (!Hydranode::instance().parseCommandLine()) {
		return 0;
	}
	Hydranode::instance().initConfig();
	Hydranode::instance().initSockets();
	ModManager::instance().loadModule("hnsh");

	if (boost::filesystem::exists("httpget.log")) try {
		boost::filesystem::remove("httpget.log");
	} catch (std::exception &e) {
		logWarning(
			boost::format("Failed to clear logfile: ")
			% e.what()
		);
	}
	Log::instance().addLogFile("httpget.log");

	Log::instance().enableTraceMask("http.parser");
	Log::instance().enableTraceMask("http.download");
	Log::instance().enableTraceMask("http.connection");
	Log::instance().enableTraceMask("http.file");
	Log::instance().enableTraceMask("http.parsedurl");
	//Log::instance().enableTraceMask("partdata");

	Http::HttpClient http;
	http.onInit();
	try {
		Http::HttpClient::instance().tryStartDownload(file);
	} catch (std::exception &e) {
		logError(e.what());
		return -1;
	}

	Hydranode::instance().mainLoop();

	Http::HttpClient::instance().onExit();
	Hydranode::instance().cleanUp();

	return 0;
}
