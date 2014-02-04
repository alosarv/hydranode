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

#ifndef __PARSER_H__
#define __PARSER_H__

#include <hncore/http/http.h>
#include <boost/signal.hpp>


namespace Http {

//! Defines various events that will be emitted by Parser.
enum ParserEvent {
	EVT_SIZE = 1,
	EVT_MD5,
	EVT_HEAD,
	EVT_DATA,
	EVT_FAILURE,
	EVT_FILE_COMPLETE,
	EVT_CHUNK_COMPLETE,
	EVT_NORANGES,
	EVT_SUCCESSFUL,
	EVT_REDIRECT
};

/**
 * @brief Parser provides a higher-level frontend for HTTP-related tasks,
 *        such as parsing "raw" data streams (as received by Sockets) or
 *        handling communication with HTTP servers.
 *
 * An example usage of this class might be something like this:
 *
 * @code
 *  Http::Parser *m_parser = new HttpParser;
 *  m_parser->writeData.connect(boost::bind(&onWrite, this, _b1, _b2, _b3));
 *  m_parser->sendData.connect(boost::bind(&onSend, this, _b1, _b2));
 *  m_parser->getFile(Http::Detail::ParsedUrl("http://hydranode.com"));
 *
 *  ...
 *
 *  void onWrite(Http::Parser *p, const std::string &data, uint64_t offset) {
 *       m_lockedRange->write(offset, data);
 *  }
 *
 *  void onSend(Http::Parser *p, const std::string &data) {
 *       m_socket->write(data);
 *  }
 *
 *  void onSocketEvent(Socket *s, SocketEvent evt) {
 *       if (evt == EVT_READ) {
 *             m_parser->parse(s->getData());
 *       }
 *  }
 * @endcode
 */
class Parser : public Trackable {
public:
	Parser();
	~Parser();

	/**
	 * Parses a "raw" HTTP stream as received by a Socket for example.
	 *
	 * @param data       The "raw" HTTP data stream
	 */
	void parse(const std::string &data);

	/**
	 * Clears and resets all member variables and settings.
	 * This should be done after every single HTTP request!
	 */
	void reset();

	/**
	 * @name Generic setters
	 */
	//@{
	/**
	 * Sets up a custom header that is going to be sent during the next
	 * HTTP request.
	 * Example:
	 *
	 * @code
	 *    setHeader("User-Agent", "Hydranode");
	 * @endcode
	 *
	 * @param header       The HTTP header to be set
	 * @param value        The header's value
	 */
	void setHeader(const std::string &header, const std::string &value);

	/**
	 * Sets the HTTP request's "body", e.g. form-data...
	 *
	 * @param data       Data to be sent as HTTP body
	 */
	void setData(const std::string &data);

	/**
	 * This determines whether the full URL, or just the path is going
	 * to be requested from the server.
	 *
	 * @note You must always request full URLs when using HTTP proxies!
	 *
	 * For example:
	 *     GET http://hydranode.com/index.php HTTP/1.0
	 * instead of:
	 *     GET /index.php HTTP/1.0
	 *
	 * @param x       Set "true" to request the full URL
	 */
	void setRequestUrl(bool x) { m_requestUrl = x; }

	/**
	 * Set HTTP Proxy authentication credentials.
	 *
	 * @param user       The Proxy username
	 * @param pass       The Proxy password
	 */
	void setProxyAuth(const std::string &user, const std::string &pass);
	//@}

	/**
	 * If the filesize of the downloaded file is already known, setting
	 * the filesize of the Parser object will add extra security, as
	 * the Parser will then directly request a file from the server
	 * with the given filesize.
	 *
	 * @note This is especially useful when doing segmented downloads where
	 *       one has to make sure that all concurrent downloads actually
	 *       download exactly the same file.
	 *
	 * @param filesize       The filesize of the download being requested
	 */
	void setSize(uint64_t filesize);

	/**
	 * Download a single file from a HTTP server.
	 *
	 * @param obj       The full HTTP URL of the file
	 */
	void getFile(Detail::ParsedUrl obj);

	/**
	 * Get information, like the filesize, of a single file
	 * from a HTTP server.
	 * This should also be used to "check" if a server actually has
	 * the requested file, or not.
	 *
	 * @note This sends a HTTP HEAD request, so no data is being downloaded!
	 *
	 * @param obj       The full HTTP URL of the file
	 */
	void getInfo(Detail::ParsedUrl obj);

	/**
	 * Download a specific file-range of a single file from a HTTP server.
	 *
	 * @param obj       The full HTTP URL of the file
	 * @param range     The file-range to be downloaded
	 */
	void getChunk(Detail::ParsedUrl obj, Range64 range);

	/**
	 * Post "formdata" using a HTTP POST request.
	 * Example:
	 *
	 * @code
	 *    std::map<std::string, std::string> data;
	 *    data["username"] = "wubbla";
	 *    data["password"] = "secr3t";
	 *    postFormData(url, data);
	 * @endcode
	 *
	 * @param obj       The full HTTP URL of the file
	 * @param data      The data to be posted
	 */
	void postFormData(
		Detail::ParsedUrl obj,
		std::map<std::string, std::string> data
	);

	/**
	 * @name Generic accessors
	 */
	//@{
	std::string getHeader(const std::string &header) const;
	std::string getMD5()         const;
	int         getStatusCode()  const;
	uint64_t    getSize()        const;
	uint64_t    getPayload()     const { return m_payload;  }
	uint64_t    getOverhead()    const { return m_overhead; }
	std::string getHostName()    const { return m_hostName; }
	std::string getFileName()    const { return m_fileName; }
	std::string getCustomHeader(const std::string &header) const;
	//@}

	//! Returns "true" if a server supports file-ranges.
	bool supportsRanges() const;

	/**
	 * This signal is emitted when contents of the requested file have
	 * been received and are ready to be written to the disk for example.
	 */
	boost::signal<void (Parser*, const std::string&, uint64_t)> writeData;

	/**
	 * This singal is emitted when a HTTP request, that is ready to be
	 * sent to a HTTP server, has been generated.
	 */
	boost::signal<void (Parser*, const std::string&)> sendData;

	boost::signal<void (Parser*, uint64_t)> onSize;
	boost::signal<void (Parser*)> onSuccess;
	boost::signal<void (Parser*)> onFailure;
	boost::signal<void (Parser*, ParserEvent)> onEvent;

private:
	void doRequest(Detail::ParsedUrl obj);

	/**
	 * Parses the HTTP header and fills m_header with the results.
	 *
	 * @param data       The HTTP header as a "raw" string
	 * @return	     "true" if parsing was successful
	 */
	bool parseHeader(std::string &data);

	/**
	 * Handles HTTP chunked transfer encoded streams.
	 * It will parse the chunked transfer headers and strip it from
	 * the string passed to the function.
	 *
	 * @param data       The "raw" HTTP stream
	 */
	void parseChunkedTransfer(const std::string &data);

	/**
	 * Mainly calls the writeData signal and also takes care to correctly
	 * alter the values of m_offset, m_payload and m_toRead.
	 */
	void write(const std::string &data);

	/**
	 * This returns the first (\r\n or \n terminated) line in a string
	 * and removes it from the input string (including the \r\n or \n).
	 *
	 * @param data      The string where the first line should be extracted
	 * @return          The extracted first line
	 */
	std::string getLine(std::string &data);

	//! This represents the HTTP request types, e.g. GET/POST/HEAD.
	enum RequestMode {
		MODE_FILE = 0,
		MODE_CHUNK,
		MODE_INFO,
		MODE_POST,
		MODE_CONNECT
	};

	std::string m_fileName; //!< filename of the requested file

	/**
	 * Data that should be sent as "body" of the HTTP request is
	 * added to m_data.
	 * This is used when posting form data for example.
	 */
	std::string m_data;

	//! hostname of the host, data is being requested or sent to
	std::string m_hostName;

	int m_statusCode; //!< the server's HTTP response status code

	//! file-offset it is currently being received
	uint64_t m_offset;

	uint64_t m_overhead; //!< HTTP-protocol overhead
	uint64_t m_payload;  //!< HTTP-protocol's payload

	//! the file-range that is going to be requested
	Range64 m_range;

	//! this determines which HTTP request type is going to be sent: eg. GET
	uint8_t m_mode;

	//! have a look at setRequestUrl() for an explanation
	bool m_requestUrl;

	//! all received headers from the server are stores in this container
	std::map<std::string, std::string> m_header;

	//! this stores a list of custom headers that are going to be sent
	std::map<std::string, std::string> m_customHeader;

	//! this is try if the server uses HTTP's chunked transfer encoding
	bool m_chunkedTransfer;

	/**
	 * If m_chunkedTransfer is true this value determines how much data
	 * is left to read from the server until the next chunk of data
	 * is going to be sent by the server.
	 * This value decreases steadily and a value of 0 means that we will
	 * receive the next "chunked-encoding-header".
	 */
	uint64_t m_toRead;

	//! Indicates that the download is finished.
	bool m_complete;

	/**
	 * Needed (when the server sends chunked encoded) to buffer all
	 * buffer all incoming data from the server, as it might happen, that
	 * the server sends the data faster than we can internally handle it
	 * and write it to disk. This prevents the user from losing this data.
	 */
	std::string m_buffer;

	//! The filesize of the file being requested.
	uint64_t m_size;
};

} // End namespace Http

#endif
