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
 * \file baseclient.cpp  Implementation of BaseClient class
 */

#include <hncore/baseclient.h>
#include <hncore/clientmanager.h>
#include <hncore/partdata.h>
#include <hnbase/lambda_placeholders.h>
#include <hnbase/ipv4addr.h>
#include <boost/lambda/bind.hpp>

namespace Detail {
	class ClientManagerIterator
		: public ClientManager::ClientIndex::iterator
	{
	public:
		template<typename T>
		ClientManagerIterator(T t)
		: ClientManager::ClientIndex::iterator(t)
		{}
	};
}

BaseClient::BaseClient(ModuleBase *module) : m_module(module), m_reqFile(),
m_partData(), m_isConnected(), m_isUploading(), m_isDownloading(), m_iter(
	new Detail::ClientManagerIterator(
		ClientManager::instance().m_clientIndex.insert(this).first
	)
) {}

BaseClient::~BaseClient() {
	ClientManager::instance().m_clientIndex.erase(*m_iter);
}

// implement base versions of virtuals
// -----------------------------------
IPV4Address BaseClient::getAddr()              const { return IPV4Address(); }
std::string BaseClient::getNick()              const { return std::string(); }
std::string BaseClient::getSoft()              const { return std::string(); }
std::string BaseClient::getSoftVersion()       const { return std::string(); }
uint64_t    BaseClient::getSessionUploaded()   const { return 0; }
uint64_t    BaseClient::getSessionDownloaded() const { return 0; }
uint64_t    BaseClient::getTotalUploaded()     const { return 0; }
uint64_t    BaseClient::getTotalDownloaded()   const { return 0; }
uint32_t    BaseClient::getUploadSpeed()       const { return 0; }
uint32_t    BaseClient::getDownloadSpeed()     const { return 0; }
uint32_t    BaseClient::getQueueRanking()      const { return 0; }
uint32_t    BaseClient::getRemoteQR()          const { return 0; }

// modifiers
// ---------

void BaseClient::setConnected(bool state) {
	ClientManager::instance().m_clientIndex.modify(
		*m_iter,
		boost::lambda::bind(&BaseClient::m_isConnected, __1) = state
	);
}

void BaseClient::setDownloading(bool state, PartData *file) {
	if (state) {
		assert(file);
	}
	ClientManager::instance().m_clientIndex.modify(
		*m_iter,
		boost::lambda::bind(&BaseClient::m_isDownloading, __1) = state
	);
//	if (!state) {
//		m_usedRange.reset();
//	}
	setSource(file);
}

void BaseClient::setUploading(bool state, SharedFile *file) {
	if (state) {
		assert(file);
	}
	ClientManager::instance().m_clientIndex.modify(
		*m_iter,
		boost::lambda::bind(&BaseClient::m_isUploading, __1) = state
	);
	setRequest(file);
}

void BaseClient::addOffered(PartData *file) {
	assert(file);
// 	if (m_offered.find(file) == m_offered.end()) {
// 		std::insert_iterator<std::set<PartData*> > it(
// 			m_offered, m_offered.begin()
// 		);
// 		ClientManager::instance().m_clientIndex.modify(
// 			*m_iter, boost::lambda::var(*it) = file
// 		);
// 	}
	if (!m_partData) {
		setSource(file);
	}
}

void BaseClient::remOffered(PartData *file, PartData *other) {
// 	if (m_offered.find(file) != m_offered.end()) {
// 		ClientManager::instance().m_clientIndex.modify(
// 			*m_iter,
// 			boost::lambda::bind<void>(
// 				&std::set<PartData*>::erase, &m_offered, file
// 			)
// 		);
// 	}
	if (m_partData == file) {
		setSource(other);
	}
}

void BaseClient::setSource(PartData *file) {
	if (m_partData != file) {
		ClientManager::instance().m_clientIndex.modify(
			*m_iter,
			boost::lambda::bind(&BaseClient::m_partData, __1) =file
		);
	}
}

void BaseClient::setRequest(SharedFile *file) {
	if (m_reqFile != file) {
		ClientManager::instance().m_clientIndex.modify(
			*m_iter,
			boost::lambda::bind(&BaseClient::m_reqFile, __1) = file
		);
	}
}

// utility functions
// -----------------
#if 0
Detail::LockedRangePtr BaseClient::getChunkReq(
	uint32_t size, const ChunkMask &pred
) try {
	assert(m_partData);
	if (!m_usedRange) {
		m_usedRange = m_partData->getRange(pred.getChunkSize());
	}
	if (m_usedRange) try {
		return m_usedRange->getLock(size);
	} catch (std::exception&) {
		m_usedRange.reset();
		return getChunkReq(size, pred);
	}
} catch (std::exception&) {
	return Detail::LockedRangePtr();
}
#endif
