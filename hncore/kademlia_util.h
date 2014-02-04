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
 * \file kademlia_util.h Kademlia utils
 */

#ifndef __KADEMLIA_UTIL_H__
#define __KADEMLIA_UTIL_H__

#include <hnbase/config.h>
#include <hnbase/hash.h>
#include <hnbase/ipv4addr.h>
#include <hnbase/utils.h>
#include <hncore/modules.h>

#include <boost/static_assert.hpp>

#include <list>
#include <bitset>
#include <iomanip>
#include <stdexcept>

/**
 * Generic Kademlia implementation
 */
namespace Kad {
	namespace KUtils {
		//! Lightweight type representative object
		template<class T>
		struct TypeToType {
			typedef T type;
		};

		/**
		 * Given a IdType, builds a mask for prefix matching operations
		 *
		 * @param bits	Mask wideness
		 */
		template<size_t N>
		std::bitset<N> buildMask(unsigned bits, TypeToType<std::bitset<N> >) {
			CHECK_THROW(bits < N);

			std::bitset<N> m;
			m.reset();

			int b(bits);
			b--;

			while(b >= 0) {
				m[b--] = true;
			}

			return m;
		}

		/**
		 * Convenience function to dump a bitset to string (hex dump)
		 */
		template<size_t N>
		std::string bitsetHexDump(const std::bitset<N>& bit) {
			std::stringstream ss;

			ss << std::hex;

			for(unsigned i = N; i > 0; i -= 8) {
				unsigned char c(0);

				// Retrieve last byte
				for(unsigned j = 0; j < 8; ++j) {
					if(bit[i - 8 + j]) {
						c |= (1 << j);
					}
				}

				ss << std::setw(2) << std::setfill('0') << (unsigned)c;
			}

			return ss.str();
		}

		/**
		 * Convenience function to dump a bitset to string (binary dump)
		 */
		template<size_t N>
		std::string bitsetBitDump(const std::bitset<N>& bit) {
			std::stringstream ss;

			for(int i = N - 1; i >= 0; i--) {
				ss << bit[i];
			}

			return ss.str();
		}

		/**
		 * Convenience function to create a random bitset
		 */
		template<size_t N>
		std::bitset<N> randomBitset() {
			std::bitset<N> b;
			uint32_t r;

			for(unsigned i = 0; i < N;) {
				r = Utils::getRandom();

				for(unsigned j = 0; j < 32 && j < N - i; ++j) {
					b[i++] = r & 1;

					r >>= 1;
				}
			}

			return b;
		}
		template<size_t N>
		std::bitset<N> randomBitset(TypeToType<std::bitset<N> >) {
			return randomBitset<N>();
		}
	}
}

#endif
