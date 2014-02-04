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
 * \file schedbase.cpp Implementation of SchedBase class
 */

#include <hnbase/pch.h>
#include <hnbase/schedbase.h>
#include <hnbase/gettickcount.h>
#include <hnbase/utils.h>               // for bytesToString
#include <hnbase/lambda_placeholders.h>
#include <hnbase/prefs.h>
#include <hnbase/ipv4addr.h>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <boost/multi_index/hashed_index.hpp>

#ifndef WIN32
	#include <sys/select.h> // for FD_SETSIZE value
#endif

namespace Detail {
	enum DefaultSettings {
		DFLT_UPLIMIT = 25*1024,
		DFLT_DNLIMIT = 0,
		DFLT_CNLIMIT = 300,
#if defined(_WIN32) || defined(_WIN64)
		DFLT_CNPSECLIMIT = 9,
		DFLT_CNCTING_LIMIT = 9
#else
		DFLT_CNPSECLIMIT = 100,
		DFLT_CNCTING_LIMIT = 100
#endif
	};

	//! Unary function object for extracting a requests score
	template<typename T>
	struct ScoreExtractor {
		typedef float result_type;
		float operator()(const T &x) {
			return x->getScore();
		}
	};

	//! Indexes for request maps
	template<typename T>
	struct RequestIndex : boost::multi_index::indexed_by<
		boost::multi_index::hashed_unique<
			boost::multi_index::identity<T>
		>,
		boost::multi_index::ordered_non_unique<
			ScoreExtractor<T>
		>
	> {};
	struct UploadReqMap : boost::multi_index_container<
		SchedBase::UploadReqBase*,
		RequestIndex<SchedBase::UploadReqBase*>
	> {};
	struct DownloadReqMap : boost::multi_index_container<
		SchedBase::DownloadReqBase*,
		RequestIndex<SchedBase::DownloadReqBase*>
	> {};
	struct ConnReqMap : boost::multi_index_container<
		SchedBase::ConnReqBase*,
		RequestIndex<SchedBase::ConnReqBase*>
	> {};
}
using namespace Detail;

SchedBase::ReqBase::ReqBase(float score) : m_score(score), m_valid(true) {}
SchedBase::ReqBase::~ReqBase() {}
SchedBase::UploadReqBase::UploadReqBase(float score) : ReqBase(score) {}
SchedBase::UploadReqBase::~UploadReqBase() {}
SchedBase::DownloadReqBase::DownloadReqBase(float score) : ReqBase(score) {}
SchedBase::DownloadReqBase::~DownloadReqBase() {}
SchedBase::ConnReqBase::ConnReqBase(float score) : ReqBase(score), m_out() {}
SchedBase::ConnReqBase::~ConnReqBase() {}


/**
 * Utility function, which performs sets clearing and notifications sending.
 *
 * @param toRemove       Set containing requests to be removed/deleted
 * @param toNotify       Set containing requests to be notified
 * @param cont           Main container to remove the toRemove elements from
 */
template<typename T, typename D>
void clearAndNotify(T &toRemove, T &toNotify, D &cont) {
	typedef typename T::iterator Iter;
	for (Iter i = toRemove.begin(); i != toRemove.end(); ++i) {
		if (toNotify.find(*i) != toNotify.end()) {
			(*i)->notify();
			toNotify.erase(*i);
		}
		if (!(*i)->isValid()) {
			cont.erase(*i);
			delete *i;
		}
	}
	for (Iter i = toNotify.begin(); i != toNotify.end(); ++i) {
		CHECK((*i)->isValid());
		(*i)->notify();
	}
}

// SchedBase class
// ---------------
SchedBase::SchedBase() : m_uploadReqs(new UploadReqMap),
m_downloadReqs(new DownloadReqMap), m_connReqs(new ConnReqMap),
m_lastConnTime(), m_connsPerSec(100), m_upLimit(25*1024),
m_downLimit(std::numeric_limits<uint32_t>::max()),
m_connLimit(300), m_connCnt(), 
m_connectingLimit(std::numeric_limits<uint32_t>::max()), m_connectingCnt(),
m_displayUpSpeed(DEFAULT_HISTSIZE, DEFAULT_PRECISION),
m_displayDownSpeed(DEFAULT_HISTSIZE, DEFAULT_PRECISION), 
m_printStatus(true), m_blocked(), m_downPackets(), m_upPackets() {
}

void SchedBase::init() {
	Prefs::instance().setPath("/");
	m_upLimit = Prefs::instance().read<uint32_t>(
		"UpSpeedLimit", DFLT_UPLIMIT
	);
	m_downLimit = Prefs::instance().read<uint32_t>(
		"DownSpeedLimit", DFLT_DNLIMIT
	);
	m_connLimit = Prefs::instance().read<uint32_t>(
		"ConnectionLimit", DFLT_CNLIMIT
	);
	m_connsPerSec = Prefs::instance().read<uint32_t>(
		"NewConnsPerSec", DFLT_CNPSECLIMIT
	);
	m_connectingLimit = Prefs::instance().read<uint32_t>(
		"ConnectingLimit", DFLT_CNCTING_LIMIT
	);
	m_connDelay = 1000 / m_connsPerSec;

	// write values back to initialize them on first startup
	Prefs::instance().write<uint32_t>("UpSpeedLimit",    m_upLimit);
	Prefs::instance().write<uint32_t>("DownSpeedLimit",  m_downLimit);
	Prefs::instance().write<uint32_t>("ConnectionLimit", m_connLimit);
	Prefs::instance().write<uint32_t>("NewConnsPerSec",  m_connsPerSec);
	Prefs::instance().write<uint32_t>("ConnectingLimit", m_connectingLimit);

	Prefs::instance().valueChanging.connect(
		boost::bind(&SchedBase::onConfigChange, this, _1, _2)
	);

	logMsg(
		boost::format(
			"Networking scheduler started (upload "
			"%s/s, download %s/s)."
		) % Utils::bytesToString(m_upLimit)
		% Utils::bytesToString(m_downLimit)
	);
	if (m_downLimit == 0) {
		m_downLimit = std::numeric_limits<uint32_t>::max();
	}

	// besides the fact that unlimited uploadrate causes really bad
	// performance on DSL links, there are also a LOT of problems in
	// handling unlimited uploadrate in upload-slot management code, so
	// for everyone's best interests, just disallow it completely. Users
	// can always specify insanely-high values to get the same effect, but
	// then it's up to them to handle the situation.
	if (m_upLimit == 0) {
		logError(
			"Unlimited upload rate causes bad download performance."
		);
		logMsg("Info: Using default (25kb/s) limit instead.");
		m_upLimit = 25*1024;
		Prefs::instance().write<uint32_t>("UpSpeedLimit", m_upLimit);
	}

	updateConnLimit();

	// no-limit ip addresses;
	//! \todo Move NoLimit IP address list to some config file
	m_noSpeedLimit.push(Range32(Socket::makeAddr("127.0.0.1")));
	uint32_t ipLow = Socket::makeAddr("192.168.0.0");
	uint32_t ipHigh = Socket::makeAddr("192.168.0.255");
	m_noSpeedLimit.push(SWAP32_ON_LE(ipLow), SWAP32_ON_LE(ipHigh));

	SpeedMeter::setTicker(m_curTick);
}

SchedBase::~SchedBase() {}
SchedBase& SchedBase::instance() {
	static SchedBase sched;
	return sched;
}

void SchedBase::exit() {
	if (m_upLimit == std::numeric_limits<uint32_t>::max()) {
		m_upLimit = 0;
	}
	if (m_downLimit == std::numeric_limits<uint32_t>::max()) {
		m_downLimit = 0;
	}
	if (m_connectingLimit == std::numeric_limits<uint32_t>::max()) {
		m_connectingLimit = DFLT_CNCTING_LIMIT;
	}

	Prefs::instance().setPath("/");
	Prefs::instance().write<uint32_t>("UpSpeedLimit",    m_upLimit);
	Prefs::instance().write<uint32_t>("DownSpeedLimit",  m_downLimit);
	Prefs::instance().write<uint32_t>("ConnectionLimit", m_connLimit);
	Prefs::instance().write<uint32_t>("NewConnsPerSec",  m_connsPerSec);
	Prefs::instance().write<uint32_t>("ConnectingLimit", m_connectingLimit);

	logDebug(
		boost::format("Scheduler: UpLimit: %s DownLimit: %s")
		% m_upLimit % m_downLimit
	);
	logMsg(
		boost::format("Total uploaded: %s Total downloaded: %s")
		% Utils::bytesToString(getTotalUpstream())
		% Utils::bytesToString(getTotalDownstream())
	);
	if (m_blocked) {
		logMsg(
			boost::format("%d connections were blocked by filter.")
			% m_blocked
		);
	}
	SpeedMeter::unsetTicker();
}

// Iterate on download requests
void SchedBase::handleDownloads() {
	typedef DownloadReqMap::nth_index<1>::type ScoreIndex;
	typedef ScoreIndex::iterator DIter;

	std::set<DownloadReqBase*> toRemove;
	std::set<DownloadReqBase*> toNotify;

	uint32_t pendingReqs = m_downloadReqs->size();
	ScoreIndex &scoreIndex = m_downloadReqs->get<1>();

	for (DIter i = scoreIndex.begin(); i != scoreIndex.end(); ++i) {
		if (!(*i)->isValid()) {
			toRemove.insert(*i);
			--pendingReqs;
			continue;
		}
		if (!pendingReqs) {
			logDebug(
				"Scheduler::handleDownloads(): "
				"pendingReqs == 0! This shouldn't happen."
			);
			break;
		}

		uint32_t freeDown = getFreeDown();

		// adds 500-byte variation, in order to 
		// raise the average bytes-per-packet value
		if (freeDown < 500) {
			break;
		}

		uint32_t amount = freeDown / pendingReqs;

		uint32_t ret = 0;
		try {
			ret = (*i)->doRecv(amount);
		} catch (std::exception &e) {
			error(boost::format("doRecv(%d)") % amount, e.what());
		}
		m_downSpeed += ret;
		m_displayDownSpeed += ret;
		++m_downPackets;

		if (ret < amount) {
			toRemove.insert(*i);
			(*i)->invalidate();
		}
		toNotify.insert(*i);
		--pendingReqs;
	}

	clearAndNotify(toRemove, toNotify, *m_downloadReqs);
}

// Iterate on upload requets
void SchedBase::handleUploads() {
	typedef UploadReqMap::nth_index<1>::type ScoreIndex;
	typedef ScoreIndex::iterator UIter;

	std::set<UploadReqBase*> toRemove;
	std::set<UploadReqBase*> toNotify;

	uint32_t pendingReqs = m_uploadReqs->size();
	ScoreIndex &scoreIndex = m_uploadReqs->get<1>();

	for (UIter i = scoreIndex.begin(); i != scoreIndex.end(); ++i) {
		if (!(*i)->isValid()) {
			toRemove.insert(*i);
			--pendingReqs;
			continue;
		}
		if (!pendingReqs) {
			logDebug(
				"Scheduler::handleUploads(): pendingReqs == 0!"
				" This shouldn't happen."
			);
			break;
		}

		uint32_t freeUp = getFreeUp();

		// adds 500-byte variation, in order to 
		// raise the average bytes-per-packet value
		if (freeUp < 500) {
			break;
		}

		uint32_t amount = freeUp / pendingReqs;
		if (!amount) {
			break;
		} else if (amount > 100*1024) {
			amount = 100*1024;
		}
		uint32_t ret = 0;
		try {
			ret = (*i)->doSend(amount);
		} catch (std::exception &e) {
			error(
				boost::format("doSend(%d)") % getFreeUp(),
				e.what()
			);
		}
		m_upSpeed += ret;
		m_displayUpSpeed += ret;
		++m_upPackets;

		if ((*i)->getPending() == 0) {
			toRemove.insert(*i);
			(*i)->invalidate();
		}

		--pendingReqs;
	}

	clearAndNotify(toRemove, toNotify, *m_uploadReqs);
}

// grant connections up to the connection limit
void SchedBase::handleConnections() {
	typedef ConnReqMap::nth_index<1>::type ScoreIndex;
	typedef ScoreIndex::iterator CIter;

	std::set<ConnReqBase*> toRemove;
	std::set<ConnReqBase*> toNotify;

	ConnReqMap::nth_index<1>::type &scoreIndex = m_connReqs->get<1>();

	for (CIter i = scoreIndex.begin(); i != scoreIndex.end(); ++i) {
		if (!(*i)->isValid()) {
			toRemove.insert(*i);
			logTrace(TRACE_SCHED, "Discarding invalid request.");
			continue;
		}
		if (getConnection((*i)->m_out)) try {
			int ret = (*i)->doConn();
			if (ret & ConnReqBase::NOTIFY) {
				toNotify.insert(*i);
			}
			if (ret & ConnReqBase::REMOVE) {
				toRemove.insert(*i);
				(*i)->invalidate();
			}
			if (ret & ConnReqBase::ADDCONN) {
				++m_connCnt;
			}
		} catch (std::exception &e) {
			error(boost::format("doConn()"), e.what());
		} else {
			break;
		}
	}

	clearAndNotify(toRemove, toNotify, *m_connReqs);
}

// main scheduler loop, called from global event table
void SchedBase::process() {
	m_curTick = EventMain::instance().getTick();
	handleDownloads();
	handleUploads();
	handleConnections();

	static uint64_t nextUpdate = m_curTick + 100;
	if (m_printStatus && nextUpdate < m_curTick) {
		boost::format fmt3(
			"Upload: " COL_CYAN "%10s/s" COL_NONE
			" | Download: " COL_GREEN "%10s/s" COL_NONE
			" | Connections:" COL_YELLOW "%4s" COL_NONE
		);
		fmt3 % m_displayUpSpeed % m_displayDownSpeed % m_connCnt;
		boost::recursive_mutex::scoped_lock l(Log::getIosLock());
		std::cerr << '\r';
		Log::instance().writeMsg(fmt3.str());
		std::cerr.flush();
		nextUpdate = m_curTick + 100;
	}
}

void SchedBase::error(const boost::format &pre, const std::string &what) {
	logDebug(boost::format("%s: %s") % pre % what);
}

bool SchedBase::onConfigChange(
	const std::string &key, const std::string &val
) try {
	if (key == "UpSpeedLimit") {
		uint32_t tmp = boost::lexical_cast<uint32_t>(val);
		if (!tmp) {
			logError(
				"Unlimited upload rate causes bad "
				"download performance."
			);
			return false;
		};
		m_upLimit = tmp;
		logMsg(
			boost::format(
				"Scheduler: Upload speed limit set to %s/s"
			) % Utils::bytesToString(m_upLimit)
		);
	} else if (key == "DownSpeedLimit") {
		m_downLimit = boost::lexical_cast<uint32_t>(val);
		logMsg(
			boost::format(
				"Scheduler: Download speed "
				"limit set to %s/s"
			) % Utils::bytesToString(m_downLimit)
		);
		if (!m_downLimit) {
			m_downLimit = std::numeric_limits<uint32_t>::max();
		}
	} else if (key == "ConnectionLimit") {
		m_connLimit = boost::lexical_cast<uint32_t>(val);
		updateConnLimit();
		logMsg(
			boost::format("Scheduler: Connection limit set to %d")
			% m_connLimit
		);
	} else if (key == "ConnectingLimit") {
		m_connectingLimit = boost::lexical_cast<uint32_t>(val);
		logMsg(
			boost::format("Scheduler: Connecting limit set to %d")
			% m_connectingLimit
		);
		if (!m_connectingLimit) {
			m_connectingLimit = std::numeric_limits<
				uint32_t
			>::max();
		}
	}
	return true;
} catch (boost::bad_lexical_cast&) {
	logError(
		boost::format(
			"Scheduler: Failed to change parameter `%s': "
			"expected integral value"
		) % key
	);
	return false;
}
MSVC_ONLY(;)

size_t SchedBase::getConnReqCount()     const { return m_connReqs->size();     }
size_t SchedBase::getUploadReqCount()   const { return m_uploadReqs->size();   }
size_t SchedBase::getDownloadReqCount() const { return m_downloadReqs->size(); }
void SchedBase::addUploadReq(UploadReqBase *r)  { m_uploadReqs->insert(r);   }
void SchedBase::addDloadReq(DownloadReqBase *r) { m_downloadReqs->insert(r); }
void SchedBase::addConnReq(ConnReqBase *r)      { m_connReqs->insert(r);     }

bool SchedBase::getConnection(bool out /* = false */) {
	if (m_connCnt >= m_connLimit) {
		return false;
	}
	if (out && m_connectingCnt >= m_connectingLimit) {
		return false;
	}
	if (out && m_lastConnTime + m_connDelay > m_curTick) {
		return false;
	}
	m_lastConnTime = m_curTick;
	return true;
}

uint32_t SchedBase::getFreeUp() {
	if (getUpSpeed() > getUpLimit()) {
		return 0;
	} else {
		return getUpLimit() - getUpSpeed();
	}
}

uint32_t SchedBase::getFreeDown() {
	if (getDownSpeed() > getDownLimit()) {
		return 0;
	} else {
		return getDownLimit() - getDownSpeed();
	}
}

void SchedBase::updateConnLimit() {
	if (m_connLimit == 0) {
		m_connLimit = FD_SETSIZE - 10;
	} else if (m_connLimit > FD_SETSIZE - 10) {
		m_connLimit = FD_SETSIZE - 10; // 10 non-scheduled sockets
		logMsg(
			boost::format(
				"Info: Open connections limit reduced "
				"to %d due to select() syscall limitations."
			) % (FD_SETSIZE - 10)
		);
		Prefs::instance().write("/ConnectionLimit", FD_SETSIZE - 10);
	}
}
