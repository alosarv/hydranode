/*
 *  Copyright (C) 2005-2006 Alo Sarv <madcat_@users.sourceforge.net>
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
 * \file bget.cpp
 * Standalone command-line utility for downloading .torrent files using
 * Hydranode Bittorrent module.
 */

#include <hncore/hydranode.h>
#include <hncore/bt/bittorrent.h>
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
	std::string file;
	for (int i = 0; i < argc; ++i) {
		if (boost::algorithm::iends_with(argv[i], ".torrent")) {
			file = argv[i];
			break;
		}
	}
	if (!file.size()) {
		logError("Required argument: .torrent file");
		return 1;
	}

	// bring up configuration before creating downloads
	Hydranode::instance().preInit(argc, argv);
	if (!Hydranode::instance().parseCommandLine()) {
		return 0;
	}
	Hydranode::instance().initConfig();
	Hydranode::instance().initSockets();
	ModManager::instance().loadModule("hnsh");

	if (boost::filesystem::exists("bget.log")) try {
		boost::filesystem::remove("bget.log");
	} catch (std::exception &e) {
		logWarning(
			boost::format("Failed to clear logfile: ")
			% e.what()
		);
	}
	Log::instance().addLogFile("bget.log");

	Bt::BitTorrent bt;
	bt.onInit();
	try {
		Bt::BitTorrent::instance().createTorrent(
			file, boost::filesystem::path()
		);
	} catch (std::exception &e) {
		logError(e.what());
		return -1;
	}

	Hydranode::instance().mainLoop();

	Bt::BitTorrent::instance().onExit();
	Hydranode::instance().cleanUp();

	return 0;
}
