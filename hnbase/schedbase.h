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
 * \file schedbase.h    Interface for SchedBase class
 */

#ifndef __SCHEDBASE_H__
#define __SCHEDBASE_H__

#include <hnbase/osdep.h>
#include <hnbase/fwd.h>
#include <hnbase/rangelist.h>
#include <hnbase/event.h>
#include <hnbase/speedmeter.h>

namespace Detail {
	struct UploadReqMap;
	struct DownloadReqMap;
	struct ConnReqMap;
}

/**
 * SchedBase implements the third level of hydranode networking scheduler.
 * This class performs the actual bandwidth and connections dividing between
 * requests in it's main scheduler loop. It is derived from EventTableBase,
 * and as such will be run during each event loop, performing bandwidth requests
 * handling.
 */
class HNBASE_EXPORT SchedBase : public EventTableBase {
public:
	//! Singleton class - accessor to the only instance (created on demand)
	static SchedBase& instance();

	//! @name Accessors for various limits and other internal variables
	//@{
	void setUpLimit(uint32_t amount)   { m_upLimit   = amount; }
	void setDownLimit(uint32_t amount) { m_downLimit = amount; }
	void setConnLimit(uint32_t amount) { m_connLimit = amount; }

	uint32_t getUpLimit()         const { return m_upLimit;              }
	uint32_t getDownLimit()       const { return m_downLimit;            }
	uint32_t getConnLimit()       const { return m_connLimit;            }
	uint32_t getConnectingLimit() const { return m_connectingLimit;      }
	uint64_t getTotalUpstream()   const { return m_upSpeed.getTotal();   }
	uint64_t getTotalDownstream() const { return m_downSpeed.getTotal(); }
	uint32_t getUpSpeed()         const { return m_upSpeed.getSpeed();   }
	uint32_t getDownSpeed()       const { return m_downSpeed.getSpeed(); }
	uint32_t getConnCount()       const { return m_connCnt;              }
	uint32_t getConnectingCount() const { return m_connectingCnt;        }
	uint64_t getDownPackets()     const { return m_downPackets;          }
	uint64_t getUpPackets()       const { return m_upPackets;            }
	uint32_t getDisplayUpSpeed()  const { 
		return m_displayUpSpeed.getSpeed(); 
	}
	uint32_t getDisplayDownSpeed() const {
		return m_displayDownSpeed.getSpeed();
	}
	//@}

	//! Get number of pending connection requests
	size_t getConnReqCount() const;
	//! Get number of pending upload requests
	size_t getUploadReqCount() const;
	//! Get number of pending download requests
	size_t getDownloadReqCount() const;

	template<typename Module, typename ImplPtr>
	static float getScore(ImplPtr s) {
		return Module::getPriority() + s->getPriority();
	}

	//! Get the number of blocked connections
	uint32_t getBlocked() const { return m_blocked; }

	//! Disable statusbar printing
	void disableStatus() { m_printStatus = false; }

	/**
	 * Combiner for isAllowed signal; combines return values.
	 *
	 * If there are no slots to be called, this defaults to true (allowed).
	 * Otherwise, it loops through the slots, and breaks out of the loop
	 * as soon as a 'false' value is found; the return value can be
	 * determined if we reached the end of slot-listing (all returned true),
	 * or not (one returned false - overall return will be false as well).
	 */
	struct Combiner {
		typedef bool result_type;

		template<typename InputIterator>
		result_type operator()(InputIterator first, InputIterator last){
			if (first == last) {
				return true;
			}
			while (first != last && *first) {
				++first;
			}
			return first == last;
		}
	};
	/**
	 * Check if connection with an IP address is allowed
	 *
	 * @param uint32_t    IP address, in network byte order
	 * @return            True if connection is allowed, false otherwise
	 */
	boost::signal<bool (uint32_t), Combiner> isAllowed;

	//! Add to blocked connections counter
	void addBlocked() { ++m_blocked; }

	/**
	 * Controls default values for sockets/scheduler speed-o-meters
	 */
	enum SpeedMeterDefaults {
		DEFAULT_HISTSIZE  = 50,  //!< history entries
		DEFAULT_PRECISION = 100  //!< precision
	};
private:
	friend class Hydranode;

	//! @name Singleton
	//@{
	SchedBase();
	~SchedBase();
	SchedBase(const SchedBase&);
	SchedBase& operator=(const SchedBase&);
	//@}

	//! Called by Hydranode on application startup
	void init();

	//! Called by Hydranode on application shutdown
	void exit();

	//! Small utility function, sets appropriate m_connLimit value
	void updateConnLimit();
public:
	//! Request base, only contains score of the request
	class HNBASE_EXPORT ReqBase {
	public:
		ReqBase(float score);
		virtual ~ReqBase();

		//! Perform notification of frontend(s)
		virtual void notify() const = 0;

		float getScore() const { return m_score; }

		//! Make this request invalid, to be deleted as soon as possible
		void invalidate() { m_valid = false; }

		//! Check the validity of this request
		bool isValid() const { return m_valid; }

		//! Change validity
		void setValid(bool v) { m_valid = v; }
	private:
		float m_score;      //!< Score of this request
		bool  m_valid;      //!< Validity of this request
	};

	//! Request of type upload
	class HNBASE_EXPORT UploadReqBase : public ReqBase {
	public:
		UploadReqBase(float score);
		virtual ~UploadReqBase();
		virtual uint32_t doSend(uint32_t amount) = 0;
		virtual uint32_t getPending() const = 0;
	};
	//! Request of type download
	class HNBASE_EXPORT DownloadReqBase : public ReqBase {
	public:
		DownloadReqBase(float score);
		virtual ~DownloadReqBase();
		virtual uint32_t doRecv(uint32_t amount) = 0;
	};
	//! Request of type connection
	class HNBASE_EXPORT ConnReqBase : public ReqBase {
	public:
		enum ConnRet {
			REMOVE = 1,   //!< Request should be removed
			NOTIFY = 2,   //!< notify() should be called
			ADDCONN = 4   //!< Connection has been added
		};

		ConnReqBase(float score);
		virtual ~ConnReqBase();

		/**
		 * Perform connection attempt
		 *
		 * @return Bitfield of values from ConnRet enumeration
		 */
		virtual int doConn() = 0;

		bool m_out; //! if this is outgoing connection
	};

private:
	//! @name Request sets
	//@{
	boost::scoped_ptr<Detail::UploadReqMap>   m_uploadReqs;
	boost::scoped_ptr<Detail::DownloadReqMap> m_downloadReqs;
	boost::scoped_ptr<Detail::ConnReqMap>     m_connReqs;
	//@}

	//! @name Main networking loop and helper functions
	//@{
	void process();
	void handleDownloads();
	void handleUploads();
	void handleConnections();
	//@}

	/**
	 * @name Get the amount of free bandwidth/connections at this moment
	 *
	 * \note The checks prior to substraction in the following functions are
	 *       needed when changing speeds on runtime.
	 */
	//!@{
	uint32_t getFreeDown();
	uint32_t getFreeUp();
	bool getConnection(bool out = false);
	//!@}

	/**
	 * Trace-logs an error
	 *
	 * @param where       Where the error happened
	 * @param what        What error happened
	 */
	void error(const boost::format &where, const std::string &what);

	/**
	 * Signal handler for configuration changes; updates internal variables
	 * as needed.
	 *
	 * @param key         Key that was changed
	 * @param val         New value of the key
	 * @returns           True if change was accepted, false otherwise
	 */
	bool onConfigChange(const std::string &key, const std::string &val);

	//! Keeps current tick value - to reduce getTick() calls somewhat
	uint64_t m_curTick;
	uint64_t m_lastConnTime; //!< Tick when last connection was made
	uint32_t m_connsPerSec;  //!< Num/new connections/sec
	uint32_t m_connDelay;    //!< Delay (in ms) between connections

	//! @name Various limits and counts
	//@{
	uint32_t m_upLimit;         //!< upstream limit
	uint32_t m_downLimit;       //!< downstream limit
	uint32_t m_connLimit;       //!< open connections limit
	uint32_t m_connCnt;         //!< open connections count
	uint32_t m_connectingLimit; //!< concurrent outgoing connections
	uint32_t m_connectingCnt;   //!< concurrent outgoing connections
	SpeedMeter m_upSpeed;
	SpeedMeter m_downSpeed;
	//@}

	/**
	 * These duplicate m_upSpeed and m_downSpeed, but are used for
	 * displaying on statusbar only. They differ by having longer history,
	 * thus showing more stable rates.
	 */
	SpeedMeter m_displayUpSpeed, m_displayDownSpeed;

	/**
	 * Ip addresses in this list are not affected by speed limits; traffic
	 * to these addresses is not counted towards total traffic. Normally,
	 * this list contains localhost and LAN ip addresses.
	 */
	RangeList32 m_noSpeedLimit;

	//! Whether to write the "statusbar" after every loop
	bool m_printStatus;

	//! Number of blocked connections
	uint32_t m_blocked;

	//! Number of ::send() / ::recv() calls made
	uint64_t m_downPackets, m_upPackets;
public:
	//! @name Internal stuff - add new requests
	//@{
	void addUploadReq(UploadReqBase *r);
	void addDloadReq(DownloadReqBase *r);
	void addConnReq(ConnReqBase *r);
	//@}

	//! @name Modify open connection count
	//@{
	void addConn() { ++m_connCnt; }
	void delConn() { assert(m_connCnt); --m_connCnt; }
	void addConnecting() { ++m_connectingCnt; }
	void delConnecting() { assert(m_connectingCnt); --m_connectingCnt; }
	//@}

	/**
	 * Check if speed limiting should be applied to specified IP address.
	 *
	 * @param ip      Ip address being interested in
	 * @return        True if limiting should be applied, false otherwise
	 */
	bool isLimited(uint32_t ip) {
		return !m_noSpeedLimit.contains(Range32(ip));
	}

	static const uint32_t INPUT_BUFSIZE = 100*1024;
};

#endif
