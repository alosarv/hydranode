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
 * \file test-ipfilter.cpp Tests IpFilter API
 */

#include <hncore/ipfilter.h>
#include <hnbase/log.h>
#include <hnbase/ipv4addr.h>
#include <hnbase/utils.h>
#include <boost/test/minimal.hpp>

void test_filter(boost::shared_ptr<IpFilter> flt) {
	BOOST_CHECK(flt->isAllowed(IPV4Address("62.65.192.1").getIp()));
	BOOST_CHECK(!flt->isAllowed(IPV4Address("64.93.9.205").getIp()));
	BOOST_CHECK(!flt->isAllowed(IPV4Address("64.93.12.100").getIp()));
	BOOST_CHECK(!flt->isAllowed(IPV4Address("195.71.167.184").getIp()));
	BOOST_CHECK(!flt->isAllowed(IPV4Address("195.98.240.0").getIp()));
	BOOST_CHECK(!flt->isAllowed(IPV4Address("195.101.169.246").getIp()));
	BOOST_CHECK(!flt->isAllowed(IPV4Address("213.56.56.15").getIp()));
}

int test_main(int, char*[]) {
	logMsg("Testing mldonkey ipfilter format loading...");
	boost::shared_ptr<IpFilter> p1(new IpFilter);
	p1->load("guarding.p2p");
	test_filter(p1);
	logMsg("Testing eMule ipfilter.dat format loading...");
	boost::shared_ptr<IpFilter> p2(new IpFilter);
	p2->load("ipfilter.dat");
	test_filter(p2);

	logMsg("Testing lookup times...");
	Utils::StopWatch s1;
	for (uint32_t i = 0; i < 20000; ++i) {
		p1->isAllowed(IPV4Address("62.65.192.1").getIp());
		p1->isAllowed(IPV4Address("64.93.9.205").getIp());
		p1->isAllowed(IPV4Address("123.255.32.23").getIp());
		p1->isAllowed(IPV4Address("12.32.44.7").getIp());
		p1->isAllowed(IPV4Address("213.44.24.1").getIp());
	}
	logMsg(boost::format("100'000 isAllowed() queries took %fs") % s1);
	return 0;
}
