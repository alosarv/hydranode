/*
 *  Copyright (c) 2005-2006 Alo Sarv <madcat_@users.sourceforge.net>
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

#include <hnbase/upnp.h>
#include <hnbase/log.h>
#include <hnbase/sockets.h>
#include <boost/program_options.hpp>

std::vector<uint16_t> toForward;
std::vector<uint16_t> internalPorts;
std::vector<uint16_t> toRemove;
std::vector<std::string> reasons;
std::vector<std::string> protocols;
uint16_t timeout = 5000;

void foundRouter(boost::shared_ptr<UPnP::Router> r) {
	logMsg(boost::format("Found router: %s.") % r->getName());
	for (uint32_t i = 0; i < toForward.size(); ++i) {
		uint16_t ePort = toForward[i];
		uint16_t iPort = ePort;
		std::string desc;
		UPnP::Protocol prot = UPnP::TCP;
		if (protocols[i] == "udp") {
			prot = UPnP::UDP;
		}
		if (internalPorts.size() > i) {
			iPort = internalPorts[i];
		}
		if (reasons.size() > i) {
			desc = reasons[i];
		}
		boost::format fmt("Forwarding: %1% %2%->%3% %4%");
		fmt % protocols[i] % ePort % iPort;
		if (desc.size()) {
			fmt % ("(" + desc + ")");
		} else {
			fmt % "";
		}
		logMsg(fmt);
		r->addForwarding(prot, ePort, iPort, desc);
	}
	for (uint32_t i = 0; i < toRemove.size(); ++i) {
		UPnP::Protocol prot = UPnP::TCP;
		if (protocols[i] == "udp") {
			prot = UPnP::UDP;
		}
		logMsg(
			boost::format("Removing: %1% %2%")
			% protocols[i] % toRemove[i]
		);
		r->removeForwarding(prot, toRemove[i]);
	}
}

int main(int argc, char *argv[]) {
	namespace po = boost::program_options;

	po::options_description desc("Commandline options");
	desc.add_options()
		("help,h", "displays this message")
		(
			"add-port,a",
			po::value< std::vector<uint16_t> >(&toForward),
			"add a port to be forwarded"
		)
		(
			"del-port,d",
			po::value< std::vector<uint16_t> >(&toRemove),
			"remove port forwarding"
		)
		(
			"timeout,t",
			po::value<uint16_t>(&timeout)->default_value(5000),
			"how long to wait before giving up"
		)
		(
			"internal-port,i",
			po::value< std::vector<uint16_t> >(&internalPorts),
			"internal ports for forwarded ports"
		)
		(
			"reason,r",
			po::value< std::vector<std::string> >(&reasons),
			"reasons/descriptions for the ports forwarded"
		)
		(
			"protocol,p",
			po::value< std::vector<std::string> >(&protocols),
			"protocols for the ports to be forwarded"
		)
	;
	po::variables_map vm;
	try {
		po::store(po::parse_command_line(argc, argv, desc), vm);
		po::notify(vm);
	} catch (std::exception &e) {
		logMsg(
			boost::format("Error while parsing command-line: %1%")
			% e.what()
		);
		logMsg("Run `pfwd --help' for command-line options.");
		return -1;
	}

	if (internalPorts.size() && internalPorts.size() != toForward.size()) {
		logMsg(
			"When specifying internal ports, the count of internal"
		);
		logMsg("ports must match the count of forwarded ports.");
		return -1;
	}
	if (reasons.size() && reasons.size() != toForward.size()) {
		logMsg("When specifying reasons for ports, the number of");
		logMsg("reasons must match the number of ports forwarded.");
		return -1;
	}
	if (toForward.size() && protocols.size() != toForward.size()) {
		logMsg("One protocol option must be specified for each ");
		logMsg("forwarded port.");
		return -1;
	}
	if (toRemove.size() && protocols.size() != toRemove.size()) {
		logMsg("One protocol option must be specified for each ");
		logMsg("removed port.");
		return -1;
	}

	for (uint32_t i = 0; i < protocols.size(); ++i) {
		if (protocols[i] != "tcp" && protocols[i] != "udp") {
			logMsg(
				boost::format(
					"Expected \"tcp\" or \"udp\" as "
					"protocol value for #%1%, got %2%."
				) % i % protocols[i]
			);
			return -1;
		}
	}

	if (vm.count("help")) {
		logMsg(boost::format("%s") % desc);
		return 0;
	}

	Log::instance().enableTraceMask("UPnP");
	Log::instance().enableTraceMask(TRACE_SOCKET, "sockets");
	logMsg("Broadcasting search for routers...");
	UPnP::findRouters(&foundRouter);
	Utils::StopWatch s;
	while (s.elapsed() < timeout) {
		try {
			EventMain::instance().process();
		} catch (std::exception &e) {
			logError(
				boost::format("Unhandled exception: %s")
				% e.what()
			);
		}
	}
	logMsg("Timed out, exiting.");
	return 0;
}

