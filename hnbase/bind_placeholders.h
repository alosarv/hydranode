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
 * \file bind_placeholders.h Defines Boost.Bind placeholder variables
 */

#ifndef __BIND_PLACEHOLDERS_H__
#define __BIND_PLACEHOLDERS_H__

/**
 * Somehow, boost/bind/placeholders.hpp causes multiply-defined symbols on
 * darwin. Replace that header with our own in-place symbols here on OSX.
 */
//#if !defined(BOOST_BIND_PLACEHOLDERS_HPP_INCLUDED)
//        #define BOOST_BIND_PLACEHOLDERS_HPP_INCLUDED
        #include <boost/bind/arg.hpp>
        #include <boost/config.hpp>
        namespace {
                static boost::arg<1> _b1;
                static boost::arg<2> _b2;
                static boost::arg<3> _b3;
                static boost::arg<4> _b4;
                static boost::arg<5> _b5;
                static boost::arg<6> _b6;
                static boost::arg<7> _b7;
                static boost::arg<8> _b8;
                static boost::arg<9> _b9;

                // This machinery is needed to suppress unused variable
                // warnings resulting from the above static variables.
                struct __use_boost_placeholder_symbols {
                        void __use_vars_func() {
                                (void)_b1;
                                (void)_b2;
                                (void)_b3;
                                (void)_b4;
                                (void)_b5;
                                (void)_b6;
                                (void)_b7;
                                (void)_b8;
                                (void)_b9;
                        }
                };
        }

//	#include <boost/bind/placeholders.hpp>
//#endif

#endif
