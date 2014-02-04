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

#ifndef __PARSEDURL_H__
#define __PARSEDURL_H__

#include <string>
#include <hnbase/osdep.h>


namespace Http {
namespace Detail {

/**
 * @brief ParsedUrl provides an abstract class that parses and splits up
 *        HTTP URLs into its various components.
 *
 * It provides various accessors that return the specific components of
 * the URL, e.g. the hostname, path, username, ...
 */
class ParsedUrl {
public:
	/**
	 * This class takes a "raw" string of an URL and parses it,
	 * e.g. ParsedUrl url("http://hydranode.com/index.php");
	 *
	 * @param url       The URL to be parsed.
	 */
	ParsedUrl(const std::string &url);

	/**
	 * Decodes URL encoded URLs, as per RFC1738 and RFC2396.
	 *
	 * @param input       The "raw" URL to be decoded.
	 * @return            The decoded URL.
	 */
	std::string decode(const std::string &input);

	/**
	 * @name Generic accessors
	 */
	//@{
	const std::string& getUrl()      const { return m_url;  }
	const std::string& getHost()     const { return m_host; }
	const std::string& getPath()     const { return m_path; }
	const std::string& getFile()     const { return m_file; }
	uint16_t           getPort()     const { return m_port; }
	const std::string& getUser()     const { return m_user; }
	const std::string& getPassword() const { return m_password; }
	//@}

	/**
	 * @name Generic setters
	 */
	//@{
	/**
	 * Sets the username for HTTP Basic Authentications.
	 *
	 * @param user       The username used for Authentication.
	 */
	void setUser(const std::string &user) { m_user = user; }
	/**
	 * Sets the password for HTTP Basic Authentications.
	 * @param password       The password used for Authentication.
	 */
	void setPassword(const std::string &password) {
		m_password = password;
	}
	//@}

	/**
	 * Check if a URL is "valid", e.g. there were
	 * no parsing failures.
	 *
	 * @return       "True" if it's valid.
	 */
	bool isValid() const { return m_valid; }

	//! Check for equality of two ParsedUrl object
	bool operator==(const Http::Detail::ParsedUrl &x) {
		return (x.getUrl() == getUrl());
	}

private:
	ParsedUrl(); //<! Forbidden

	//! Is "true" when the URL could successfully be parsed.
	bool m_valid;

	/**
	 * Stores the "raw" URL, as given as parameter
	 * to the constructor.
	 */
	std::string m_url;
	std::string m_host;
	std::string m_path;
	std::string m_file;
	std::string m_user;
	std::string m_password;
	uint16_t    m_port;
};

} // End namespace Detail
} // End namespace Http

#endif
