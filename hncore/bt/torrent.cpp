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
 * \file torrent.cpp         Implementation of Torrent class
 */

#include <hnbase/osdep.h>
#include <hncore/sharedfile.h>
#include <hncore/partdata.h>
#include <hncore/bt/torrent.h>
#include <hncore/bt/client.h>
#include <hncore/bt/bittorrent.h>
#include <hncore/bt/tracker.h>
#include <hnbase/timed_callback.h>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/lambda/construct.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/spirit.hpp>

namespace Bt {

const std::string TORRENT("bt.torrent");

// Torrent class
// -------------
IMPLEMENT_EVENT_TABLE(Torrent, Torrent*, int);

Torrent::Torrent(SharedFile *file, const TorrentInfo &info)
: m_file(file), m_partData(), m_info(info), m_uploaded(), 
m_downloaded() {
	CHECK_THROW(file);

	m_partData = file->getPartData();

	using namespace boost::spirit;

	TorrentInfo::AIter i = info.announceBegin();
	while (i != info.announceEnd()) {
		std::string tmp = *i;
		std::string host, url;
		uint16_t port = 80;
		parse_info<> nfo = parse(
			tmp.data(), tmp.data() + tmp.size(),
			(str_p("http://") | str_p("udp://"))
			>> *( ( ':' >> uint_p[assign_a(port)] )
			    | ( '/' >> *graph_p[push_back_a(url)] >> end_p)
			    |   graph_p[push_back_a(host)]
			    )
		);
		try {
			Tracker *tr = new Tracker(host, url, port, info);
			tr->foundPeer.connect(
				boost::bind(&Torrent::createClient, this, _1)
			);
			tr->getUploaded.connect(
				boost::bind(&Torrent::getUploaded, this)
			);
			tr->getDownloaded.connect(
				boost::bind(&Torrent::getDownloaded, this)
			);
			tr->getPartData.connect(
				boost::bind(&Torrent::getPartData, this)
			);
			m_trackers.insert(tr);
		} catch (std::exception &e) {
			logDebug(boost::format("Tracker: %s") % e.what());
		}
		++i;
	}

	std::vector<IPV4Address> nodes = info.getNodes();
	uint32_t foundNodes = 0;
	for (uint32_t i = 0; i < nodes.size(); ++i) {
		try {
			createClient(nodes[i]);
			++foundNodes;
		} catch (std::exception &e) {
			logError(
				boost::format(
					"Creating BT client from node info "
					"found in .torrent: %s."
				) % e.what()
			);
		}
	}
	if (foundNodes) {
		logMsg(
			boost::format("Found %d nodes in .torrent file.")
			% foundNodes
		);
	}
	initBitfield();

	SharedFile::getEventTable().addHandler(
		m_file, this, &Torrent::onSharedFileEvent
	);
	if (m_partData) {
		PartData::getEventTable().addHandler(
			m_partData, this, &Torrent::onPartDataEvent
		);
		m_partData->onVerified.connect(
			boost::bind(&Torrent::onChunkVerified, this, _1, _2, _3)
		);
		m_partData->getSourceCnt.connect(
			boost::bind(&Torrent::getPeerCount, this)
		);
	}
}

Torrent::~Torrent() {
	using namespace boost::lambda;
	for_each(m_clients.begin(), m_clients.end(), bind(delete_ptr(), __1));
	for_each(m_trackers.begin(),m_trackers.end(),bind(delete_ptr(), __1));
}

void Torrent::handshakeReceived(Client *c) {
	CHECK_RET(m_clients.find(c) != m_clients.end());
	using namespace boost::lambda;
	if (c->getInfoHash() != getInfoHash()) {
		logTrace(
			TORRENT, boost::format(
				"[%s] Client sent wrong info_hash "
				"%s (expected %s)"
			) % c->getAddr() % c->getInfoHash().decode()
			% getInfoHash().decode()
		);
		Utils::timedCallback(bind(delete_ptr(), c), 1);
		m_clients.erase(c);
	} else if (m_file) {
		logTrace(TORRENT,
			boost::format("[%s] Attaching to %s")
			% c->getAddr() % getName()
		);
		c->setFile(m_file);
		c->setFile(m_partData);
		c->setTorrent(this);
	} else {
		Utils::timedCallback(bind(delete_ptr(), c), 1);
		m_clients.erase(c);
	}
}

void Torrent::connectionLost(Client *c) {
	CHECK_RET(m_clients.find(c) != m_clients.end());
	delete c;
	m_clients.erase(c);
}

void Torrent::addClient(Client *c) {
	CHECK_RET(m_clients.find(c) == m_clients.end());
	using namespace boost::lambda;

	if (!m_file) {
		Utils::timedCallback(bind(delete_ptr(), c), 1);
	}

	c->setFile(m_file);
	c->setFile(m_partData);
	c->setTorrent(this);
	c->connectionLost.connect(
		boost::bind(&Torrent::connectionLost, this, _b1)
	);
	m_clients.insert(c);
}

void Torrent::addRequest(const Client::Request &r) {
	m_sharedReqs.push_back(r.m_locked);
}

void Torrent::delRequest(const Client::Request &r) {
	typedef std::deque<
		boost::weak_ptr< ::Detail::LockedRange>
	>::iterator Iter;
	for (Iter i = m_sharedReqs.begin(); i != m_sharedReqs.end(); ++i) {
		::Detail::LockedRangePtr l = (*i).lock();
		if (!l || l == r.m_locked) {
			m_sharedReqs.erase(i);
			i = m_sharedReqs.begin();
		}
		if (!m_sharedReqs.size()) {
			break;
		}
	}
}

Client::Request Torrent::getRequest(const std::vector<bool> &chunks) {
	CHECK_THROW(m_sharedReqs.size());
	logDebug(
		boost::format("There are %d shared requests.")
		% m_sharedReqs.size()
	);

	typedef std::deque<
		boost::weak_ptr< ::Detail::LockedRange>
	>::iterator Iter;
	for (Iter i = m_sharedReqs.begin(); i != m_sharedReqs.end(); ++i) {
		if (!(*i).lock()) {
			m_sharedReqs.erase(i);
			i = m_sharedReqs.begin();
			if (!m_sharedReqs.size()) {
				break;
			}
		} else {
			Client::Request r((*i).lock(), this);
			if (!chunks.size() || chunks[r.m_index]) {
				m_sharedReqs.erase(i);
				addRequest(r);
				return r;
			}
		}
	}
	throw std::runtime_error("Unable to find suitable request.");
}

void Torrent::clearRequests(uint32_t index) {
	typedef std::deque<
		boost::weak_ptr< ::Detail::LockedRange>
	>::iterator Iter;
	for (Iter i = m_sharedReqs.begin(); i != m_sharedReqs.end(); ++i) {
		::Detail::LockedRangePtr l = (*i).lock();
		if (!l || Client::Request(l, this).m_index == index) {
			m_sharedReqs.erase(i);
			i = m_sharedReqs.begin();
		}
		if (!m_sharedReqs.size()) {
			break;
		}
	}
}

struct Pred {
	bool operator()(const Client *const c1, const Client *const c2) const {
		int i1 = c1->amChoking() * !c1->isUploading()
			* c1->getDownloadSpeed();
		int i2 = c2->amChoking() * !c2->isUploading()
			* c2->getDownloadSpeed();
		return i1 < i2;
	}
};

bool Torrent::tryUnchoke(Client *cc /* = 0 */, bool *dontChoke /* = 0 */) {
	std::multiset<Client*, Pred> candidates;
	std::set<Client*>::iterator itor(m_clients.begin());
	while (itor != m_clients.end()) {
		candidates.insert(*itor);
		++itor;
	}

	uint32_t numUploading = 0;
	std::multiset<Client*, Pred>::iterator it(candidates.begin());
	while (it != candidates.end()) {
		numUploading += (*it)->isUploading();
		if (numUploading > 4) {
			return false;
		}
		++it;
	}

	it = candidates.begin();
	std::vector<Client*> cand2;
	while (it != candidates.end()) {
 		Client *c = *it++;
		if (c->amChoking() && c->isInterested()) {
			if (c == cc && dontChoke) {
				*dontChoke = true;
			} else {
 				c->sendUnchoke();
				++numUploading;
				if (numUploading >= 4) {
					return true;
				}
			}
		} else if (c->amChoking()) {
			cand2.push_back(c);
		}
	}

	// uninterested clients - unchoke so if they become interested, they
	// get slot instantly
	while (cand2.size() > 1 && numUploading < 4) {
		uint32_t num = Utils::getRandom() % cand2.size();
		assert(num < cand2.size());
		if (cand2[num] == cc && dontChoke) {
			*dontChoke = true;
		} else {
			cand2[num]->sendUnchoke();
			++numUploading;
		}
	}
	return true;
}

void Torrent::createClient(IPV4Address addr) {
	CHECK_RET(m_file);

	Client *c = new Client(addr);
	c->setFile(m_file);
	c->setFile(m_partData);
	c->setTorrent(this);
	c->setSource(m_partData);
	c->handshakeReceived.connect(
		boost::bind(&Torrent::handshakeReceived, this, _1)
	);
	c->connectionLost.connect(
		boost::bind(&Torrent::connectionLost, this, _1)
	);
	m_clients.insert(c);
}

void Torrent::initBitfield() {
	m_bitField.clear();
	std::vector<bool> chunks;
	if (m_partData) {
		chunks = m_partData->getPartStatus(m_info.getChunkSize());
	} else {
		chunks = std::vector<bool>(m_info.getChunkCnt(), true);
	}
	std::vector<bool>::const_iterator it = chunks.begin();
	while (it != chunks.end()) {
		uint8_t tmp = 0;
		bool exitLoop = false;
		for (int8_t i = 7; i >= 0; --i, ++it) {
			if (it == chunks.end()) {
				m_bitField.push_back(tmp);
				exitLoop = true;
				break;
			} else {
				tmp |= *it << i;
			}
		}
		if (!exitLoop) {
			m_bitField.push_back(tmp);
		}
	}

	// self-check, ensures bitfield is properly set up as per BT spec
#ifndef NDEBUG
	uint32_t trueBits = 0;
	std::vector<bool> tmp;
	for (uint32_t i = 0; i < m_bitField.size(); ++i) {
		std::bitset<8> b((unsigned)m_bitField[i]);
		for (int8_t j = 7; j >= 0; --j) {
			tmp.push_back(b[j]);
			if (b[j]) {
				++trueBits;
			}
			if (tmp.size() == m_info.getChunkCnt()) {
				break;
			}
		}
		if (tmp.size() == m_info.getChunkCnt()) {
			break;
		}
	}
	CHECK_FAIL(tmp.size() == m_info.getChunkCnt());
#endif
}

void Torrent::onChunkVerified(
	PartData *file, uint64_t chunkSize, uint64_t chunk
) {
	CHECK_RET(file == m_partData);

	if (chunkSize != m_info.getChunkSize()) {
		return;
	}
	uint32_t num = chunk / 8;
	m_bitField.at(num) |= 1 << 7 - chunk % 8;

	// self-check, ensures we set the right bit above
#ifndef NDEBUG
	std::vector<bool> tmp;
	for (uint32_t i = 0; i < m_bitField.size(); ++i) {
		std::bitset<8> b((unsigned)m_bitField[i]);
		for (int8_t j = 7; j >= 0; --j) {
			tmp.push_back(b[j]);
		}
	}
	assert(tmp[chunk]);
#endif

	clearRequests(chunk);
}

void Torrent::onPartDataEvent(PartData *file, int evt) {
	if (evt == PD_DESTROY) {
		m_partData = 0;
	}
}

void Torrent::onSharedFileEvent(SharedFile *file, int evt) {
	using namespace boost::lambda;
	if (!m_file) {
		// if we get events from file after SF_DESTROY, ignore
		return;
	}
	assert(file == m_file);
	if (evt == SF_DESTROY) {
		m_file = 0;
		getEventTable().postEvent(this, EVT_DESTROY);
		for_each(
			m_clients.begin(), m_clients.end(),
			bind(delete_ptr(), __1)
		);
		m_clients.clear();
	} else if (evt == EVT_CHILDDESTROYED) {
		initBitfield(); // easier to rebuild completely
	}
}

void Torrent::addUploaded(uint32_t amount) {
	m_uploaded += amount;
	if (m_file) {
		m_file->addUploaded(amount);
	}
}

} // end namespace Bt
