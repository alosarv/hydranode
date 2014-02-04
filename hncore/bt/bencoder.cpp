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
 * \file bencoder.cpp        Implementation of Bencoder class
 */

#include <hnbase/osdep.h>
#include <hnbase/lambda_placeholders.h>
#include <hncore/bt/bencoder.h>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>
#include <iostream>
#include <string>
#include <sstream>

namespace Bt {

// Bencoder implementation
// -----------------------
BenCoder::BenCoder() : m_data(new std::ostringstream) {}

BenCoder::BenCoder(const int32_t &data) : m_data(new std::ostringstream) {
	*m_data << boost::format("i%de") % data;
}

BenCoder::BenCoder(const std::string &data) : m_data(new std::ostringstream) {
	*m_data << boost::format("%d:%s") % data.size() % data;
}

BenCoder::BenCoder(const char *data) : m_data(new std::ostringstream) {
	*m_data << boost::format("%d:%s") % std::string(data).size() % data;
}

BenCoder::BenCoder(const BList &data) : m_data(new std::ostringstream) {
	*m_data << 'l';
	for_each(data.begin(), data.end(), *m_data << __1);
	*m_data << 'e';
}

BenCoder::BenCoder(const BDict &data) : m_data(new std::ostringstream) {
	*m_data << 'd';
	for (BDict::const_iterator it = data.begin(); it != data.end(); ++it) {
		*m_data << BenCoder((*it).first) << (*it).second;
	}
	*m_data << 'e';
}

std::ostream& operator<<(std::ostream &o, const BenCoder &b) {
	return o << b.m_data->str();
}

} // end namespace Bt
