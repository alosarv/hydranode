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
 * \file lambda_placerholders.h Defines Boost.Lambda placeholder variables
 */

#ifndef __LAMBDA_PLACEHOLDERS_H__
#define __LAMBDA_PLACEHOLDERS_H__

#include <boost/lambda/lambda.hpp>

namespace {
	/**
	 * @name Boost.Lambda placeholder types
	 *
	 * Need to redefine placeholders to avoid conflicting with Boost.Bind
	 * placeholders.
	 */
	//! @{
	static boost::lambda::placeholder1_type __1;
	static boost::lambda::placeholder2_type __2;
	static boost::lambda::placeholder3_type __3;
	//! @}

	//! Suppresses unused variables warnings
	struct __use_boost_lambda_symbols {
		void __use_symbols_func() {
			(void)__1; (void)__2; (void)__3;
		}
	};
}

#endif
