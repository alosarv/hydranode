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

#include <hnbase/utils.h>
#include <hncore/http/parser.h>
#include <hncore/hydranode.h>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/tokenizer.hpp>
#include <boost/regex.hpp>


namespace Http {

const std::string TRACE = "http.parser";
const std::string Endl = "\r\n";

Parser::Parser() : m_range(0, 0) {
	logTrace(TRACE, boost::format("new Parser(%p)") % this);
	reset();
}


Parser::~Parser() {
	logTrace(TRACE, boost::format("~Parser(%p)") % this);
	reset();
}


void Parser::reset() {
	m_range = Range64(0, 0);
	m_offset = 0;
	m_statusCode = 0;
	m_overhead = 0;
	m_payload = 0;
	m_header.clear();
	m_customHeader.clear();
	m_fileName.clear();
	m_mode = 0;
	m_requestUrl = false;
	m_chunkedTransfer = false;
	m_toRead = 0;
	m_complete = false;
	m_buffer.clear();
	m_size = 0;
}


void Parser::setHeader(const std::string &header, const std::string &value) {
	if (header.empty() || value.empty()) {
		return;
	}
	logTrace(TRACE,
		boost::format("Setting custom header: %s = %s")
		% header % value
	);
	m_customHeader[header] = value;
}


void Parser::getFile(Detail::ParsedUrl obj) {
	m_mode = MODE_FILE;
	doRequest(obj);
}


void Parser::getInfo(Detail::ParsedUrl obj) {
	m_mode = MODE_INFO;
	doRequest(obj);
}


void Parser::getChunk(Detail::ParsedUrl obj, Range64 range) {
	CHECK_THROW(range.length() > 1);
	m_mode = MODE_CHUNK;
	m_range = range;
	m_offset = m_range.begin();
	doRequest(obj);
}


int Parser::getStatusCode() const {
	return m_statusCode;
}


uint64_t Parser::getSize() const try {
	return boost::lexical_cast<uint64_t>(getHeader("content-length"));
} catch (boost::bad_lexical_cast &) {
	logTrace(TRACE, "No content-length header found!");
	return 0;
} MSVC_ONLY(;)


void Parser::setSize(uint64_t filesize) {
	CHECK_RET(filesize);
	m_size = filesize;
}


std::string Parser::getMD5() const {
	return getHeader("content-md5");
}


std::string Parser::getCustomHeader(const std::string &header) const {
	std::map<std::string, std::string>::const_iterator it;
	it = m_customHeader.find(header);
	return (it != m_customHeader.end()) ? it->second : std::string("");
}


void Parser::postFormData(
	Detail::ParsedUrl obj,
	std::map<std::string, std::string> data
) {
	CHECK_THROW(data.size());
	m_mode = MODE_POST;

	std::string tmp;
	std::map<std::string, std::string>::iterator it = data.begin();
	for ( ; it != data.end(); ++it) {
		boost::format fmt("%s=%s&");
		fmt % it->first % it->second;
		tmp += fmt.str();
	}
	// chop the last '&' from the string:
	tmp = tmp.substr(0, tmp.length() - 1);

	setHeader("Content-Type", "application/x-www-form-urlencoded");
	setData(tmp);

	logTrace(TRACE, boost::format("Posting FormData: %s") % tmp);

	doRequest(obj);
}


void Parser::setData(const std::string &data) try {
	CHECK_THROW(data.size());

	std::string size = boost::lexical_cast<std::string>(data.size());
	setHeader("Content-Length", size);
	m_data = data;
	m_data += Endl + Endl;

} catch (std::exception &e) {
	LOG_EXCEPTION(e);
} MSVC_ONLY(;)


void Parser::doRequest(Detail::ParsedUrl obj) {
	CHECK_THROW(obj.isValid());

	std::ostringstream req;
	std::string hostname = obj.getHost();
	if (obj.getPort() != 80) {
		boost::format fmt("%s:%i");
		fmt % hostname % obj.getPort();
		hostname = fmt.str();
	}
	m_hostName = hostname;
	m_fileName = obj.getFile();

	std::string method;
	switch (m_mode) {
		case MODE_FILE:
		case MODE_CHUNK:   method = "GET";     break;
		case MODE_INFO:    method = "HEAD";    break;
		case MODE_POST:    method = "POST";    break;
		case MODE_CONNECT: method = "CONNECT"; break;
	}

	std::string request = m_requestUrl ? obj.getUrl() : obj.getPath();
	//req << method << " " << Utils::urlEncode(request) << " HTTP/1.1";
	req << method << " " << request << " HTTP/1.1";
	req << Endl;

	if (!m_hostName.empty()) {
		setHeader("Host", m_hostName);
	}

	//Do not accept any encoding at the moment
	//setHeader("Accept-Encoding", "*;q=0");
	setHeader("TE", "chunked"); //request chunked encoding...
	setHeader("Connection", "Close");
	setHeader("User-Agent", Hydranode::instance().getAppVerLong());

	if (m_mode == MODE_CHUNK && m_range.length() > 1) {
		//maybe we should use uint32_t here,
		//as many servers don't have large file support...
		boost::format fmt("bytes=%i-%i");
		fmt % m_range.begin() % m_range.end();
		setHeader("Range", fmt.str());
	}

	if (!obj.getUser().empty() && !obj.getPassword().empty()) {
		std::string tmp = obj.getUser() + ":" + obj.getPassword();
		setHeader("Authorization", "Basic " + Utils::encode64(tmp));
	}

	std::map<std::string, std::string>::iterator it;
	for (it = m_customHeader.begin(); it != m_customHeader.end(); ++it) {
		req << it->first << ": " << it->second << Endl;
	}

	req << Endl;

	if (m_data.size()) {
		req << m_data;
	}

	logTrace(TRACE,
		boost::format(
			"created Request:\n"
			"================\n"
			"%s"
			"================\n"
		) % req.str()
	);

	m_overhead += req.str().size();
	sendData(this, req.str());
}


bool Parser::parseHeader(std::string &data) {
	std::string header;

	size_t pos = data.find("\r\n\r\n");

	if (pos == std::string::npos) {
		return false;
	}

	size_t begin = data.find("\r\n");
	header = data.substr(begin, pos - begin);

	typedef boost::char_separator<char> separator;
	typedef boost::tokenizer<separator> tokenizer;
	typedef tokenizer::iterator         iter;

	logTrace(TRACE,
		boost::format(
			"Received header:\n"
			"================\n"
			"%s\n"
			"================\n"
		) % header
	);

	m_overhead += header.size();
	separator sep("\r\n");
	tokenizer tok(header, sep);

	for (iter it = tok.begin(); it != tok.end(); ++it) {
		size_t p = it->find(":");
		if (p == std::string::npos) {
			continue;
		}
		std::string header = boost::to_lower_copy(it->substr(0, p));
		std::string value = it->substr(p + 2);
		m_header.insert(std::make_pair(header, value));
	} //end for

	try {
		m_statusCode = boost::lexical_cast<int>(data.substr(9, 3));
		logTrace(TRACE,
			boost::format("Got HTTP statuscode: %d")
			% m_statusCode
		);
	} catch (boost::bad_lexical_cast &) {
		logError("No statuscode was found!");
		return false;
	}

	if (getHeader("transfer-encoding") == "chunked") {
		logTrace(TRACE, "Server uses chunked transfer encoding.");
		m_chunkedTransfer = true;
	}

	//remove the HTTP header from the buffer
	data.erase(0, pos + 4);
	return true;
}


void Parser::parseChunkedTransfer(const std::string &data) {
	logTrace(TRACE, "parseChunkedTransfer()");
	if (m_mode == MODE_INFO || m_toRead != 0) {
		return;
	}
	std::string &stream = const_cast<std::string&>(data);
	std::string line = getLine(stream);
	m_overhead += line.length();

	boost::regex reg("^([0-9a-fA-F]+).*$");
	boost::match_results<const char*> matches;

	try {
		CHECK_THROW(boost::regex_match(line.c_str(), matches, reg));
		std::istringstream tmp(matches[1]);
		tmp >> std::hex >> m_toRead;

		logTrace(TRACE,
			boost::format("Found chunksize: %i") % m_toRead
		);

		if (m_toRead < 1) {
			m_complete = true;
		}
	} catch (std::exception &e) {
		LOG_EXCEPTION(e);
	}
}


bool Parser::supportsRanges() const {
	return getHeader("accept-ranges") == "bytes";
}


std::string Parser::getLine(std::string &data) {
	//Remove any new lines at the beginning of the string...
	if (data.substr(0, 2) == "\r\n") {
		data = data.substr(2);
	} else if (data.substr(0, 1) == "\n") {
		data = data.substr(1);
	}
	std::string pattern = "\r\n";
	size_t pos = data.find(pattern);
	if (pos != std::string::npos) {
		pos = data.find(pattern = "\n");
		CHECK_THROW(pos != std::string::npos);
	}
	std::string line = data.substr(0, pos);
	//Finally this line gets erased from the input string...
	data.erase(0, pos + pattern.length());
	return line;
}


void Parser::parse(const std::string &data) try {
	logTrace(TRACE, "parse()");
	CHECK_RET(!m_complete);
	m_buffer += data;

	try {
		if (boost::algorithm::starts_with(m_buffer, "HTTP/1.")) {
			if (!parseHeader(m_buffer)) {
				return;
			}
		}
	} catch (...) {
		return;
	}

	if (m_chunkedTransfer) {
		parseChunkedTransfer(m_buffer);
	}

	if (m_statusCode >= 400 && m_statusCode < 600) {
		onEvent(this, EVT_FAILURE);
		return;
	}

	if (getHeader("accept-ranges") != "bytes") {
		onEvent(this, EVT_NORANGES);
	}

	if (
		!getHeader("content-disposition").empty() ||
		!getHeader("location").empty() ||
		m_statusCode == STATUS_MOVED_PERMANENTLY ||
		m_statusCode == STATUS_MOVED_TEMPORARILY ||
		m_statusCode == STATUS_TEMPORARY_REDIRECT
	) {
		onEvent(this, EVT_REDIRECT);
	}

	if (m_mode == MODE_INFO) {
		if (!getHeader("content-length").empty()) {
			onEvent(this, EVT_SIZE);
		}
		m_buffer.clear();
		onEvent(this, EVT_SUCCESSFUL);
		return;

	} else if (m_mode == MODE_CHUNK || m_mode == MODE_FILE) {
		if (m_chunkedTransfer) {
			while (m_buffer.size() && m_toRead > 0) {
				uint64_t toWrite = m_toRead;
				if (m_toRead > m_buffer.size()) {
					toWrite = m_buffer.size();
				}
				logTrace(TRACE,
					boost::format(
						"m_toRead=%i | data.size()=%i"
						" | m_buffer.size()=%i"
						" | toWrite=%i"
					) % m_toRead % data.size()
					% m_buffer.size() % toWrite
				);
				write(m_buffer.substr(0, toWrite));
				parseChunkedTransfer(m_buffer);
			}

		} else {
			write(m_buffer);
		}

		if (m_mode == MODE_CHUNK && m_offset == m_range.end() + 1) {
			onEvent(this, EVT_CHUNK_COMPLETE);
			return;
		}
		if (
			(m_chunkedTransfer && m_complete) ||
			(m_mode == MODE_FILE && getSize() == m_offset)
		) {
			onEvent(this, EVT_FILE_COMPLETE);
			return;
		}
	}
} catch (std::exception &e) {
	LOG_EXCEPTION(e);
} MSVC_ONLY(;)


void Parser::write(const std::string &data) {
	writeData(this, data, m_offset);
	m_payload += data.size();
	m_offset  += data.size();
	if (m_chunkedTransfer) {
		m_toRead -= data.size();
	}
	m_buffer.erase(0, data.size());
}


std::string Parser::getHeader(const std::string &header) const {
	std::map<std::string, std::string>::const_iterator it;
	it = m_header.find(header);
	return (it != m_header.end()) ? it->second : std::string("");
}


void Parser::setProxyAuth(const std::string &user, const std::string &pass) {
	if (user.empty() || pass.empty()) {
		return;
	}
	std::string tmp = "Basic " + Utils::encode64(user + ":" + pass);
	setHeader("Proxy-Authorization", tmp);
}

} // End namespace Http
