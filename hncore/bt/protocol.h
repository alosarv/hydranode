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
 * \file protocol.h
 * Set of helper macros for parsing bencoded data using Boost.Spirit library.
 */

#ifndef __BT_PROTOCOL_H__
#define __BT_PROTOCOL_H__

/**
 * Helper macro for parsing bencoded strings
 *
 * @param val      std::string value where to append the parsed string
 */
#define BSTR(val) (uint_p[assign_a(__tmp)] >> ':'                    \
	>> repeat_p(boost::ref(__tmp))[anychar_p[push_back_a(val)]])

/**
 * Helper macro for parsing bencoded integers
 *
 * @param val       int variable which to assign the parsed value
 */
#define BINT(val) (ch_p('i') >> lint_p[assign_a(val)] >> 'e')

/**
 * Initializes parser variables; the temporaries/dummies varnames are mangled
 * to avoid collisions with user-defined variables.
 *
 * \note This macro should never be called directly; it is used internally by
 *       INIT_PARSER, which should be called by client code instead.
 */
#define DECLARE_PARSER_VARS()                              \
	std::string __dummy, __dummy2, __dummy3, __dummy4; \
	uint32_t __tmp;                                    \
	rule<> dummyList, dummyDict, unknown;              \
	(void)__dummy; (void)__dummy2; (void)__dummy3;     \
	(void)__dummy4; (void)__tmp;                       \
	(void)dummyList; (void)dummyDict; (void)unknown;   \
	int_parser<long long> lint_p

/**
 * Declares a dummy list parser, which skips all parsed data.
 *
 * \note This macro should never be called directly; it is used internally by
 *       INIT_PARSER, which should be called by client code instead.
 */
#define DECLARE_DUMMY_LIST()                  \
	dummyList =                           \
		'l' >> *( BINT(__dummy)       \
			| BSTR(__dummy2)      \
			| dummyDict           \
			| dummyList           \
			)                     \
		>> 'e'

/**
 * Declares a dummy dictionary parser, which skips all parsed data
 *
 * \note This macro should never be called directly; it is used internally by
 *       INIT_PARSER, which should be called by client code instead.
 */
#define DECLARE_DUMMY_DICT()                                        \
	dummyDict =                                                 \
		'd' >> *( (BSTR(__dummy2) | BINT(__dummy)) >>       \
				( BSTR(__dummy3)                    \
				| BINT(__dummy4)                    \
				| dummyDict                         \
				| dummyList                         \
				)                                   \
			)                                           \
		>> 'e'

/**
 * Declares a dummy parser which parses any kind of bencoded input, including
 * nested lists, dictionaries and whatnot. Whenever you want to skip some
 * unknown bencoded data, use the "unknown" rule in the place, and all
 * parsed data will be skipped safely.
 *
 * \note This macro should never be called directly; it is used internally by
 *       INIT_PARSER, which should be called by client code instead.
 */
#define DECLARE_DUMMY_UNKNOWN()  \
	unknown = BSTR(__dummy2) \
	    >>  ( BINT(__dummy)  \
		| BSTR(__dummy2) \
		| dummyDict      \
		| dummyList      \
		)

/**
 * Call this macro to initialize bencoded data parser, with support for skipping
 * past unknown data. It is not needed to use this macro whenever parsing
 * bencoded data, but it declares a set of helper rules (unknown, dummyDict
 * and dummyList), which can be used to safely skip past unknown data.
 */
#define INIT_PARSER()                  \
	using namespace boost::spirit; \
	DECLARE_PARSER_VARS();         \
	DECLARE_DUMMY_DICT();          \
	DECLARE_DUMMY_UNKNOWN()

#endif
