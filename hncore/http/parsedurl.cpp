/*
 *  Copyright (C) 2005-2006 Gaubatz Patrick <patrick@gaubatz.at>
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

#include <hncore/http/parsedurl.h>
#include <hnbase/utils.h>
#include <hnbase/log.h>
#include <boost/lexical_cast.hpp>


namespace Http {
namespace Detail {

ParsedUrl::ParsedUrl(const std::string &url) {
	m_url = url;
	m_host = m_path = m_file = m_user = m_password = "";
	m_port = 80;
	m_valid = true;

	std::string tmp = url; //decode(url);

	if (tmp.substr(0, 7) != "http://" && tmp.size() <= 8) {
		m_valid = false;
	}

	size_t pos = tmp.find_last_of('#');

	if (pos != std::string::npos) {
		tmp = tmp.substr(0, pos);
	}

	pos = tmp.find_first_of('/', 7);

	if (pos != std::string::npos) {
		m_host = tmp.substr(7, pos - 7);
		m_path = tmp.substr(pos);

		m_file = "index.htm";

		pos = m_path.find_last_of('/');
		if (pos != std::string::npos && m_path.substr(pos) != "/") {
			m_file = m_path.substr(pos + 1);
		}

		pos = m_file.find_first_of('?');
		if (pos != std::string::npos) {
			m_file = m_file.substr(0, pos);
		}

		pos = m_host.find_last_of('@');
		if (pos != std::string::npos) {
			tmp = m_host.substr(0, pos);
			size_t pos2 = tmp.find_first_of(':');
			if (pos2 != std::string::npos) {
				m_user = tmp.substr(0, pos2);
				m_password = tmp.substr(pos2 + 1);
			}
			m_host = m_host.substr(pos + 1);
		}

		pos = m_host.find(':');
		if (pos != std::string::npos) {
			m_port = boost::lexical_cast<uint16_t>(
				m_host.substr(pos + 1)
			);
			m_host = m_host.substr(0, pos);
		}

		m_file = decode(m_file);

	} else {
		m_valid = false;
	}

	logTrace(
		"http.urlparser",
		boost::format(
			"url=%s | host=%s | port=%i | "
			"path=%s | file=%s | user=%s | pass=%s"
		)
		% m_url % m_host % m_port % m_path
		% m_file % m_user % m_password
	);

}

std::string ParsedUrl::decode(const std::string &input) {
	std::string out, tmp;
	for (uint32_t x = 0; x < input.length(); ++x) {
		if (input[x] == '+') {
			out += ' ';

		} else if (input[x] == '%') {
			tmp = input.substr(x + 1, 2);
			out += Utils::encode(tmp);
			x += 2;

		} else {
			out += input[x];
		}
	}
	return out;
}

} // End namespace Detail
} // End namespace Http
