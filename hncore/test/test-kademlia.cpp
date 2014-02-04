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

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#include <hncore/kademlia.h>

//! Routing table, with 32bit ids
typedef Kad::RoutingTable<
	Kad::KBucket<
		Kad::Contact<32>, 
		4
	>,
	boost::function<void (void)>
> RTable;

template<size_t N>
std::string prefixHexDump(const std::pair<std::bitset<N>, unsigned> &prefix) {
	std::string s = bitsetHexDump(prefix.first);

	s.insert(prefix.second, "*");

	return s;
}

Kad::Contact<32> build(uint32_t v) {
	Kad::Contact<32> c;

	for(unsigned i = 0; i < 32; ++i) {
		c.m_id[i] = (v & (1 << i)) ? 1 : 0;
	}

	return c;
}

void f() { } // null callback!

int main() {
	RTable rTable(&f);

	for(unsigned i = 0; i < 5; ++i) {
		rTable.add(build(i));
	}

	// RTable::iterator test
	rTable.dump();

	for(
		RTable::iterator itor = rTable.begin();
		itor != rTable.end();
	) {
		std::cout << " Contact " << itor++->m_id << std::endl;
	}
}

#endif

