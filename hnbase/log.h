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

#ifndef __LOG_H__
#define __LOG_H__

/**
 * \file log.h Interface for Logging Subsystem
 */

/**
 * @page logsubsys Logging Subsystem Overview
 *
 * Logging Subsystem is built up from a set of globally available functions
 * as follows:
 *
 * logMsg()     - Logs a message
 * logDebug()   - Logs a message only in Debug build (when NDEBUG isn't defined)
 * logWarning() - Logs a warning message (prepends with "Warning:")
 * logError()   - Logs an error message (preprends with "Error:")
 * logFatalError() - Logs a fatal system error and abort()'s the app
 * logTrace()   - Logs a message only if the given trace mask is enabled
 *
 * The logging functions use Log class as backend for various data storage,
 * for example tracemasks, and (in the future) other logging targets besides
 * the standard error device.
 *
 * Each logging function is duplicated - one version for std::string argument
 * (for usage with literal strings), and second version for boost::format
 * argument (for usage of printf()-style formatting). Thus the following
 * code is a sample usage of this system:
 *
 *  Note - I had to escape few %'s for the Doxygen HTML-generation work
 *  correctly on this file - you'll get the point.
 *
 * <pre>
 *     static const int LOGTRACE = 1;
 *     logMsg(boost::format("Hello %s, This is logtest#%d") \% "Madcat" \% 10);
 *     logDebug("This is a debug message.");
 *     logDebug(boost::format("This is %dnd debug message.") \% 2);
 *     logError("And this is a serious error.");
 *     logError(boost::format("Actually, this one is too. Errorcode: %d") \% 5);
 *
 *     logTrace(LOGTRACE, "This will not be seen.");
 *     Log::addTraceMask(LOGTRACE, "LogTrace");
 *     logTrace(LOGTRACE, "This is a Log Trace.");
 *     Log::remTraceMask(LOGTRACE);
 *     logTrace(LOGTRACE, "This will not be seen.");
 * </pre>
 * The output produced by the above code should be:
 * <pre>
 *     Hello Madcat, This is logtest#10
 *     Debug: This is a debug message.
 *     Debug: This is 2nd debug message.
 *     Error: And this is a serious error.
 *     Error: Actually, this one is too. Errorcode: 5
 *     Trace(LogTrace): This is a Log Trace.
 * </pre>
 */

#include <hnbase/osdep.h>
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <boost/function.hpp>
#include <boost/signals.hpp>
#include <boost/bind.hpp>
#include <map>
#include <set>
#include <vector>
#include <deque>
#include <string>
#include <iostream>
#include <fstream>

/**
 * These trace masks are used internally by the framework's various classes.
 * Users of the framework are required to use string trace masks instead of
 * integer trace masks to avoid masks overlapping between modules.
 */
enum InternalTraceMasks {
	TRACE_HASH = 0x01,    //!< Used in Hash classes
	TRACE_MD,             //!< Used in MetaData classes
	TRACE_MOD,            //!< Used by ModManager class
	TRACE_SOCKET,         //!< Used by Sockets classes
	TRACE_PARTDATA,       //!< Used by PartData class
	TRACE_SHAREDFILE,     //!< Used by SharedFile class
	TRACE_FILESLIST,      //!< Used by FilesList class
	TRACE_OBJECT,         //!< Used by Object class
	TRACE_HT,             //!< Used by HashThread class
	TRACE_SCHED,          //!< Used by Scheduler class
	TRACE_RANGE,          //!< Used by Range Management Subsystem
	TRACE_EVENT,          //!< Used by Event Handling Subsystem
	TRACE_CONFIG,         //!< Used by Config class
	TRACE_RESOLVER,       //!< Used by DNS Resolution Subsystem
	TRACE_HASHER,         //!< Used by Hasher subsystem
	TRACE_CHUNKS          //!< Used by PartData ChunkSelector
};

//! Different message types
enum MessageType {
	MT_MSG     = 0x01,  //!< Normal, informative messages
	MT_DEBUG   = 0x02,  //!< Debugging messages, disabled in release mode
	MT_TRACE   = 0x04,  //!< Trace messages, disabled in release mode
	MT_ERROR   = 0x08,  //!< Error messages
	MT_WARNING = 0x10   //!< Warnings
};

/**
 * Logging wrapper providing shell for trace masks and strings.
 * Only static members are accessible and calls are forwarded to the
 * only instance of this class, accessible via instance() member.
 */
class HNBASE_EXPORT Log {
public:
	//! Retrieve the only instance of this class
	static Log& instance();

	/**
	 * Enable/disable writing messages to std::cerr/std::cout (they will
	 * still be written to log files). Useful for e.g. background mode.
	 */
	//@{
	void disableOutput() { m_output = false; }
	void enableOutput()  { m_output = true;  }
	//@}

	/**
	 * Enable a trace mask.
	 *
	 * @param traceInt   Integer value. logTrace() calls with this
	 *                   value will now be enabled.
	 * @param traceStr   String for the trace messages - will be included
	 *                   in the log messages.
	 */
	void enableTraceMask(int traceInt, const std::string &traceStr) {
		m_traceMasks[traceInt] = traceStr;
	}

	/**
	 * Enable a string trace mask. logTrace() calls using this mask will
	 * now be displayed.
	 *
	 * @param mask      String mask to enable.
	 */
	void enableTraceMask(const std::string &mask) {
		m_enabledStrMasks.insert(mask);
	}

	/**
	 * Add a string trace mask to supported list of string masks. The
	 * mask can later be enabled using enableTraceMask() function.
	 *
	 * @param mask       Mask to be added.
	 *
	 * \note This function is used to 'publish' the availability of a trace
	 * mask for others (e.g. user interfaces et al).
	 */
	void addTraceMask(const std::string &mask) {
		m_strMasks.insert(mask);
	}

	/**
	 * Disable a trace mask. Messages with this mask will no longer be
	 * shown.
	 *
	 * @param traceInt   Integer value of the traceMask to be removed.
	 */
	void disableTraceMask(int traceInt) {
		m_traceMasks.erase(traceInt);
	}

	/**
	 * Disable a string trace mask.
	 *
	 * @param mask       Mask to be disabled.
	 */
	void disableTraceMask(const std::string &mask) {
		m_enabledStrMasks.erase(mask);
	}

	/**
	 * Erase a string trace mask from available string masks.
	 *
	 * @param mask       Mask to be removed.
	 *
	 * \note This also disables the mask.
	 */
	void removeTraceMask(const std::string &mask) {
		disableTraceMask(mask);
		m_strMasks.erase(mask);
	}

	/**
	 * Check if a trace mask is enabled
	 *
	 * @param traceMask      Mask to be checked
	 * @return               True if the mask is currently enabled
	 */
	bool hasTraceMask(int traceMask) {
		MIter i = m_traceMasks.find(traceMask);
		if (i != m_traceMasks.end()) {
			return true;
		}
		return false;
	}

	/**
	 * Retrieve string corresponding to a trace mask
	 *
	 * @param traceMask      Mask for which to look up the string
	 * @return               String corresponding to the trace mask
	 */
	static std::string getTraceStr(int traceMask) {
		MIter i = instance().m_traceMasks.find(traceMask);
		if (i != instance().m_traceMasks.end()) {
			return (*i).second;
		}
		return std::string("");
	}

	/**
	 * Platform-independent console output method.
	 *
	 * @param msg        Message to be written
	 *
	 * On POSIX systems, this method merely writes the specified message
	 * to std::cerr. However, custom code is required on windows to set
	 * console colors, so this method transforms the POSIX color escape
	 * sequences into win32 API calls.
	 *
	 * As a rule, when performing custom console output (Log functions use
	 * this method internally already), use this method instead of direct
	 * console IO.
	 */
	void writeMsg(std::string msg);

	/**
	 * Add a new log file target where all log messages will be
	 * written to.
	 *
	 * @param filename     Path to log file.
	 */
	void addLogFile(const std::string &filename) {
		boost::mutex::scoped_lock l(m_filesLock);
		m_logFiles.push_back(filename);
	}

	/**
	 * Write specified string to all output streams.
	 *
	 * @param msg        String to write.
	 *
	 * \note msg is passed by value instead of reference since this function
	 *       clears up any escaped symbols (e.g. colors) from the message.
	 */
	void sendToFiles(std::string msg);

	/**
	 * Actual message logging.
	 */
	void doLogString(MessageType t, const std::string &msg);

	/**
	 * Log a trace string only if the trace mask has been enabled.
	 */
	void doLogString(int traceMask, const std::string &msg);
	void doLogString(const std::string &mask, const std::string &msg);

	/**
	 * Print last messages to std::cerr.
	 *
	 * @param count       Number of last messages to print.
	 */
	static void printLast(uint32_t count);

	/**
	 * Retrieve the last logged message.
	 *
	 * @return         Last logged message.
	 */
	static std::string getLastMsg();

	/**
	 * Retrieve last N messages.
	 *
	 * @param count      Number of messages to retrieve.
	 * @param cont       Container to push the messages.
	 */
	static void getLast(uint32_t count, std::vector<std::string> *cont);

	/**
	 * Add a string to prepend to all log messages.
	 *
	 * @str               String to prepend to log messages.
	 */
	void addPreStr(const std::string &str) {
		m_preStr += str;
	}

	/**
	 * Remove a string prepended to all log messages.
	 *
	 * @param str         String to be removed.
	 */
	void remPreStr(const std::string &str) {
		size_t i = m_preStr.rfind(str);
		if (i != std::string::npos) {
			m_preStr.erase(i, str.size());
		}
	}

	/**
	 * @returns Current log message prefix as set by addPreStr() method.
	 */
	std::string getPreStr() const {
		return m_preStr;
	}

	/**
	 * Add a handler function which will be passed all log messages which
	 * match the flags specified.
	 *
	 * @param h          Function object taking const std::string& argument
	 * @param flags      Types of log messages to be passed to the handler.
	 *                   This is a bit-field containing one or more
	 *                   MessageType enumeration values.
	 * Example:
	 * addHandler(LOG_HANDLER(&MyClass::onLogMsg), MT_ERROR|MT_WARNING);
	 */
	boost::signals::connection addHandler(
		boost::function<void (const std::string&, MessageType type)> ha
	) {
		return m_sig.connect(ha);
	}

	/**
	 * More convenient version of the above function, performs function
	 * object binding internally.
	 */
	template<typename T>
	boost::signals::connection addHandler(
		T *obj, void (T::*ha)(const std::string&, MessageType)
	) {
		return addHandler(boost::bind(ha, obj, _1, _2));
	}

	/**
	 * @returns Module-defined string-masks listing
	 */
	const std::set<std::string>& getStrMasks() const {
		return m_strMasks;
	}

	/**
	 * @returns Enabled string masks
	 */
	const std::set<std::string>& getEnabledStrMasks() const {
		return m_enabledStrMasks;
	}

	//! @returns map of internal trace masks
	const std::map<std::string, int>& getInternalMasks() const {
		return m_internalMasks;
	}

	//! @returns true if the specified mask is enabled
	bool isEnabled(int mask) const {
		return m_traceMasks.find(mask) != m_traceMasks.end();
	}

	/**
	 * Enables or disables ANSI color codes transformation on win32
	 * platform.
	 */
	void setTransformColors(bool setting) {
		m_transformColors = setting;
	}

	/**
	 * Enables/disables colored output completely.
	 */
	void setDisableColors(bool setting) {
		m_disableColors = setting;
	}

	/**
	 * When other code uses std::cerr or std::cout, aquire this mutex
	 * while performing write operations to sync this over threads.
	 */
	static boost::recursive_mutex& getIosLock() {
		return s_iosLock;
	}
private:
	Log();                         //!< Constructor
	Log(Log&);                     //!< Forbidden
	Log& operator=(const Log&);    //!< Forbidden
	~Log();                        //! Destructor

        //! Pointer to the single instance of this class
        static Log *s_log;

	/**
	 * Adds messages to s_messages array.
	 */
	void addMsg(MessageType t, const std::string &msg);

	//! Enabled trace masks
	std::map<int, std::string> m_traceMasks;
	std::set<std::string> m_strMasks;
	std::set<std::string> m_enabledStrMasks;

	//! Internal trace masks
	std::map<std::string, int> m_internalMasks;

	//! Output streams to write to
	std::vector<std::string> m_logFiles;

	//! Iterator for the above list
	typedef std::vector<std::string>::iterator FIter;

	//! Protects m_logFiles list and the streams contained within
	boost::mutex m_filesLock;

	//! Stores last N log messages (including trace and debug messages)
	static std::deque<std::string> s_messages;

	//! Iterator typedef for the above list
	typedef std::deque<std::string>::const_iterator CMIter;

	//! For easier usaage
	typedef std::map<int, std::string>::iterator MIter;

	//! String to prepend to all log messages.
	std::string m_preStr;

	//! Protects output streams for multiple thread access.
	static boost::recursive_mutex s_iosLock;

	/** @name Various logging functions. */
	//@{
	friend void HNBASE_EXPORT logMsg       (const std::string &msg  );
	friend void HNBASE_EXPORT logMsg       (const boost::format &fmt);
	friend void HNBASE_EXPORT logDebug     (const std::string &msg  );
	friend void HNBASE_EXPORT logDebug     (const boost::format &fmt);
	friend void HNBASE_EXPORT logWarning   (const std::string &msg  );
	friend void HNBASE_EXPORT logWarning   (const boost::format &fmt);
	friend void HNBASE_EXPORT logError     (const std::string &msg  );
	friend void HNBASE_EXPORT logError     (const boost::format &fmt);
	friend void HNBASE_EXPORT logFatalError(const std::string &msg  );
	friend void HNBASE_EXPORT logFatalError(const boost::format &msg);
#if !defined(__FULL_TRACE__) && !defined(NDEBUG) && !defined(NTRACE)
	friend void HNBASE_EXPORT logTrace(
		uint32_t mask, const std::string &msg
	);
	friend void HNBASE_EXPORT logTrace(
		uint32_t mask, const boost::format &msg
	);
	friend void HNBASE_EXPORT logTrace(
		const std::string &mask, const std::string &msg
	);
	friend void HNBASE_EXPORT logTrace(
		const std::string &mask, const boost::format &msg
	);
#endif
	//@}

	boost::signal<void (const std::string&, MessageType)> m_sig;
	bool m_output; //! Whether to write to standard output

	//! Only effective on win32; if true, ANSI color codes are transformed
	//! to win32 API calls to produce colored output on win32 console
	bool m_transformColors;

	//! If set true, completely disables colors in the output
	bool m_disableColors;
};

/**
 * Logs a message only if given trace mask is enabled.
 *
 * @param mask     TraceMask. Message will only be logged if this mask has been
 *                 previously enabled from Log class with addTraceMask function.
 * @param msg      Message to be logged.
 *
 * If __FULL_TRACE__ is defined, we use macro in order to retrieve the source
 * file/line of the call. Otherwise, inline functions are used.
 */
#if defined(__FULL_TRACE__)
#define logTrace(mask, msg) \
	Log::instance().doLogString( \
		MT_TRACE,  \
		mask,  \
		(boost::format("%s:%d: %s") % (__FILE__) % (__LINE__) \
		% (msg)).str() \
	)
#elif defined(NDEBUG) || defined(NTRACE)
	#define logTrace(mask, text)
#endif

#endif
