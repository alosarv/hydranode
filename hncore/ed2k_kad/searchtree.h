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

#ifndef __ED2K_KAD_SEARCHTREE_H__
#define __ED2K_KAD_SEARCHTREE_H__

#include <string>

#include <boost/mpl/find_if.hpp>
#include <boost/mpl/deref.hpp>

#include <hnbase/utils.h>

namespace ED2KKad {
	enum __unnamed__ {
		LOGIC = 0,
		CONTAINS_STRING = 1
	};

	typedef unsigned opcode_t;

	/**
	 * A generic search tree
	 */
	struct SearchTree {
		/**
		 * A generic node
		 *
		 * @param T   Return type from execution of this node
		 */
		template<class T>
		struct Node {
			typedef T ReturnType;

			virtual std::string getStr() const = 0;

			virtual T execute() = 0;

			virtual ~Node() { };
		};

		/**
		 * A generic operand
		 *
		 * @param T   Operand type
		 */
		template<class T>
		struct Operand
		: public Node<T> {
			T execute() {
				return m_value;
			}

			Operand(const T& value)
			: m_value(value)
			{ }

		private:
			T m_value;
		};

		/**
		 * A binary operator
		 *
		 * @param T    Operation's return type
		 * @param U    First operand type
		 * @param V    Second operand type
		 * @param Fun  Operation functor
		 */
		template<class T, class U, class V, class Fun>
		struct BinaryOperator
		: public Node<T> {
			Node<U>* m_r;
			Node<V>* m_l;

			T execute() {
				return m_f(m_r->execute(), m_l->execute());
			}

			void setFunction(Fun f) {
				m_f = f;
			}

			BinaryOperator(Fun f)
			: m_f(f)
			{ }

		private:
			Fun m_f;
		};

	};

	/**
	 * Search context
	 */
	struct SearchContext { };

	/**
	 * Keeper
	 */
	struct SearchContextKeeper {
		SearchContext m_sc;
		SearchContextKeeper(SearchContext& sc) : m_sc(sc) { }
	};

	SearchTree::Node<bool>* parseBinary(
		SearchContext& sc, std::ifstream& i
	);

	enum {
		OPCODE_STRING = 1
	};

	// Opcode 1, string
	class ContainsString
	: public SearchTree::Operand<bool>, public SearchContextKeeper {
	public:
		bool execute() {
			return true;
		}

		std::string getStr() const {
			std::stringstream ss;

			Utils::putVal<uint8_t>(ss, OPCODE_STRING);
			Utils::putVal<std::string>(ss, m_value);

			return ss.str();
		}

		void setValue(const std::string& value) {
			m_value = value;
		}

		ContainsString(SearchContext& sc, std::ifstream& i)
		: SearchTree::Operand<bool>(false), SearchContextKeeper(sc) {
			m_value = Utils::getVal<std::string>(i);
		}

		ContainsString(SearchContext& sc)
		: SearchTree::Operand<bool>(false), SearchContextKeeper(sc)
		{ }

	private:
		std::string m_value;
	};

	// Opcode 0, logical function
	class Logical
	: public SearchTree::BinaryOperator<
		bool, bool, bool, bool (*)(bool, bool)
	>, public SearchContext {
		bool execute() {
			return true;
		}

		Logical(SearchContext& sc, std::ifstream& i)
		: SearchTree::BinaryOperator<
			bool, bool, bool, bool (*)(bool, bool)
		>(0), SearchContext(sc)
		{
			uint8_t logicOpcode = Utils::getVal<uint8_t>(i);

			typedef bool (*f_t)(bool, bool);
			f_t fptr[3] = { &lAnd, &lOr, &lAndNot };

			CHECK_THROW(logicOpcode < sizeof(fptr) / sizeof(void*));

			setFunction(fptr[logicOpcode]);

			m_r = parseBinary(sc, i);
			m_l = parseBinary(sc, i);
		}
	private:
		static bool lAnd(bool t, bool u) {
			return t && u;
		}
		static bool lOr(bool t, bool u) {
			return t || u;
		}
		static bool lAndNot(bool t, bool u) {
			return t && !u;
		}
	};
}

#endif
