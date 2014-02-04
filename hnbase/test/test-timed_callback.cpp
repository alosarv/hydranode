/*
 *  Copyright (C) 2005 Andrea Leofreddi <andrea.leofreddi@libero.it>
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
 * \file test-timed_callback.cpp Timed callback test
 */

#include <iostream>
#include <map>
#include <hnbase/timed_callback.h>

using namespace Utils;

std::map<void *, std::string> ptr2Name;

template<class T>
void deleteFun(T* ptr) {
	std::cerr << "deleteFun(), deleting " << ptr2Name[ptr] << std::endl;

	delete ptr;
}

void f() {
	std::cerr << "C-like callback invoked!" << std::endl;
}

struct F {
	void operator()() const {
		std::cerr << "Functor callback invoked!" << std::endl;
	}
};

struct X : public Trackable {
	void f() {
		std::cerr << "f() on instance " << ptr2Name[this] << std::endl;

		Utils::timedCallback(boost::bind(&X::f, this), 1000);
	}
};

int main() {
	X *a(new X), *b(new X), *c(new X);

	ptr2Name[a] = "a";
	ptr2Name[b] = "b";
	ptr2Name[c] = "c";

	// C-like function
	Utils::timedCallback(&f, 1000);

	// Functor
	Utils::timedCallback(F(), 1000);

	// Binded member function
	Utils::timedCallback(boost::bind(&X::f, a), 1000);
	Utils::timedCallback(boost::bind(&X::f, b), 1000);
	Utils::timedCallback(boost::bind(&X::f, c), 1000);

	// Schedule deletion of instance 'c' in 3 seconds, from instance a and b
	Utils::timedCallback(boost::bind(&deleteFun<X>, c), 3000);

	EventMain::initialize();

	EventMain::instance().mainLoop();

	return 0;
}
