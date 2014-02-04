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

#include <hnbase/config.h>
#include <hnbase/log.h>
#include <iostream>
#include <boost/timer.hpp>

template<class T> void check(const std::string &msg, T got, T expected) {
	if (got != expected) {
		std::cerr << "FATAL: Reading " << msg << ", expected '";
		std::cerr << expected << "', got '" << got << "'";
		std::cerr << std::endl;
		abort();
	}
}

Config *c;
void put() {
	c->write("one", -100);      // int
	c->write("two", 10);        // uint
	c->write("three", 3.14);    // double
	c->write("four", 3.14f);    // float
	c->write("five", "hi");     // string
	c->write("six", true);      // bool
}
void get() {
	int i;
	c->read("one", &i, 0);
	check<int>("one", i, -100);

	unsigned int u;
	c->read("two", &u, 0);
	check<unsigned int>("two", u, 10);

	double d;
	c->read("three", &d, 0.0);
	check<double>("three", d, 3.14);

	float f;
	c->read("four", &f, 0.0f);
	check<float>("four", f, 3.14f);

	std::string s;
	c->read("five", &s, 0);
	check<std::string>("five", s, "hi");
}
const unsigned int TESTCOUNT = 10000;
int main() {
	// Functionality testing
	c = new Config;
	put();
	get();
	// Write speed testing
	boost::timer t1;
	std::cerr << "Time for writing " << TESTCOUNT << " values: ";
	for (uint32_t i=0; i<TESTCOUNT; i++) {
		put();
	}
	std::cerr << t1.elapsed() << "ms\n";

	// Read speed testing
	boost::timer t2;
	std::cerr << "Time for reading " << TESTCOUNT << " values: ";
	for (uint32_t j=0; j<TESTCOUNT; j++) {
		get();
	}
	std::cerr << t2.elapsed() << "ms\n";

	// Loading and saving testing
	Config conf;
	conf.load("test.cfg");

	// Read by using default values
	std::cerr << "Loading default config values and writing config file...\n";

	// Test integer
	uint32_t intval;
	conf.read("Integer Value", &intval, 10);
	check<uint32_t>("Integer Value", intval, 10);
	conf.write("Integer Value", intval);

	// Test string and path
	conf.setPath("/Test");
	std::string strval;
	conf.read("HelloWorld", &strval, "Good Morning");
	check<std::string>("HelloWorld", strval, "Good Morning");
	conf.write("HelloWorld", strval);

	// test float
	conf.write("/Floating/Yeah/Pi", 3.141);

	// test bool
	conf.write("/Boo/Boolean", true);

	conf.write("/Doh", false);

	conf.save();

	// Reload the file and test if values got written correctly
	std::cerr << "Reloading config file and verifying written data...\n";

	Config d;
	d.load("test.cfg");
	uint32_t intval2;

	d.read("Integer Value", &intval2, 0);
	check<uint32_t>("integer value", intval2, 10);

	d.setPath("/Floating/Yeah");

	d.read("Integer Value", &intval2, 0);
	check<uint32_t>("integer value", intval2, 0);

	d.read("/Integer Value", &intval2, 0);
	check<uint32_t>("integer value", intval2, 10);

	std::string strval2;
	d.read("/Test/HelloWorld", &strval2, "Good Evening");
	check<std::string>("/Test/HelloWorld", strval2, "Good Morning");

	bool b2;
	d.read("/Boo/Boolean", &b2, false);
	check<bool>("/Boo/Boolean", b2, true);

	float pi;
	d.read("/Floating/Yeah/Pi", &pi, 3.0f);
	check<float>("/Floating/Yeah/Pi", pi, 3.141f);

	bool b3;
	d.setPath("/");
	d.read("Doh", &b3, true);
	check<bool>("Doh", b3, false);

	std::string strval3 = d.read<std::string>("BoraBora", "Hi there");
	check<std::string>("BoraBora", strval3, "Hi there");

	std::cerr << "All tests passed successfully.\n";
#ifdef _MSC_VER
	std::cerr << "Press <enter> to exit application.\n";
	std::cin.get();
#endif
}

