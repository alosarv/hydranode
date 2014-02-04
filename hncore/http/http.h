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

#ifndef __HTTP_H__
#define __HTTP_H__

#include <hnbase/ssocket.h>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/function.hpp>
#include <hncore/http/parsedurl.h>
#include <hnbase/osdep.h>
#include <hnbase/range.h>

// workaround for a rare MSVC linker bug
// see http://support.microsoft.com/default.aspx?scid=kb;en-us;309801
MSVC_ONLY(template class IMPORT Range<uint64_t>;)

// needed to link httpget on msvc
#ifdef __HTTP_IMPORTS__
	#define HTTPEXPORT IMPORT
#else
	#define HTTPEXPORT EXPORT
#endif

#define LOG_EXCEPTION(exception)                         \
	logError(                                        \
		boost::format("%s failed: %s")           \
		% __PRETTY_FUNCTION__ % exception.what() \
	);
/**
 * @brief The Http namespace covers all HTTP related namespaces, objects
 *        and functions.
 */
namespace Http {

/**
 * @brief Objects that are owned by Objects of the Http namespace go into
 *        the Detail namespace.
 */
namespace Detail {
	class Connection;
	class ParsedUrl;
	class File;

	/**
 	 * @name Boost smart-pointers
 	 *
 	 * Boost smart-pointer declaration for the different components
 	 * of the Detail namespace.
 	 */
	//@{
	typedef boost::shared_ptr<Connection> ConnectionPtr;
	typedef boost::weak_ptr<Connection>   ConnectionWeakPtr;
	typedef boost::shared_ptr<File>       FilePtr;
	//@}
}

class HttpClient;
class Download;
class Parser;

typedef SSocket<HttpClient, Socket::Client, Socket::TCP> HttpSocket;

/**
 * @name Boost smart-pointers
 *
 * Boost smart-pointer declaration for the different components of the module.
 */
//@{
typedef boost::shared_ptr<Download>   DownloadPtr;
typedef boost::shared_ptr<Parser>     ParserPtr;
typedef boost::shared_ptr<HttpSocket> HttpSocketPtr;
//@}

/**
 * This notifies other modules depending on the HTTP plugin, when the
 * plugin is "ready". The term "ready" basically means that the module is
 * configured according to the plugin's settings and all the other tasks
 * have already been done, e.g. resolving the hostname of the HTTP proxy...
 *
 * @note At the moment you will generally do not need to care about the
 *       module being ready, or not. Even if it is not ready yet, you will be
 *       able to start HTTP downloads, as they will certainly be started
 *       when the module gets into "ready-state" afterwards.
 *       However, if you need to access other functions than download() of
 *       HttpClient, e.g. getProxy() or codeToStr(),
 *       you will have to use this callback!
 *
 * Example:
 * @code
 * void MyModule::onInit() {
 *    Http::notifyOnReady(boost::bind(&MyModule::onHttpReady, this));
 * }
 *
 * void MyModule::onHttpReady() {
 *    IPV4Address proxyAddr = HttpClient::instance().getProxy();
 * }
 * @endcode
 *
 * @param callback       The function that is going to be called,
 *                       when the module is ready.
 */
void notifyOnReady(boost::function<void()> callback);

/**
 * This function "translates" HTTP statuscodes, such as 404 into
 * human-readable strings like "Not found".
 * Keep in mind that you cannot use this function, until the HTTP module
 * is fully initialized, so you'll probably have to have look at the
 * Http::notifyOnReady() function...
 *
 * @param code       The HTTP statuscode to be "translated".
 * @return           The "translated" statuscode.
 */
std::string codeToStr(int code);

//! HTTP/1.0 status codes from RFC1945, provided for reference.
enum HttpStatus {

	/* Successful 2xx.  */
	STATUS_OK                       = 200,
	STATUS_CREATED                  = 201,
	STATUS_ACCEPTED                 = 202,
	STATUS_NO_CONTENT               = 204,
	STATUS_PARTIAL_CONTENTS         = 206,

	/* Redirection 3xx.  */
	STATUS_MULTIPLE_CHOICES         = 300,
	STATUS_MOVED_PERMANENTLY        = 301,
	STATUS_MOVED_TEMPORARILY        = 302,
	STATUS_NOT_MODIFIED             = 304,
	STATUS_TEMPORARY_REDIRECT       = 307,

	/* Client error 4xx.  */
	STATUS_BAD_REQUEST              = 400,
	STATUS_UNAUTHORIZED             = 401,
	STATUS_FORBIDDEN                = 403,
	STATUS_NOT_FOUND                = 404,

	/* Server errors 5xx.  */
	STATUS_INTERNAL                 = 500,
	STATUS_NOT_IMPLEMENTED          = 501,
	STATUS_BAD_GATEWAY              = 502,
	STATUS_UNAVAILABLE              = 503
};

} // End namespace Http

#endif
