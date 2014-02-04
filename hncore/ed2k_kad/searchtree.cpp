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

#include <hnbase/log.h>
#include <hncore/ed2k_kad/searchtree.h>

#include <boost/mpl/vector.hpp>

namespace ED2KKad {
	// STILL NOT IMPLEMENTED
#if 0
	std::string searchOneWord(std::string word) {
		SearchContext none;

		ContainsString *n = new ContainsString(none);
		//SearchTree::Node<bool>* n = new ContainsString(none);
		n->setValue(word);

		logMsg(n->getStr());
		logMsg(boost::format("Lenght %i") % n->getStr().length());

		return n->getStr();
	} 

	SearchTree::Node<bool>* parseBinary(
		SearchContext& sc, std::ifstream& i
	) {
		uint8_t opcode = Utils::getVal<uint8_t>(i);
		SearchTree::Node<bool>* node;

		switch(opcode) {
			case 1:
				return new ContainsString(sc, i);
		}

		CHECK_THROW(0);
	}
#endif
}
