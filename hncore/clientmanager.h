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

#ifndef __CLIENTMANAGER_H__
#define __CLIENTMANAGER_H__

/**
 * \file clientmanager.h     Interface for ClientManager class
 */

#include <hnbase/osdep.h>
#include <hnbase/lambda_placeholders.h>
#include <hncore/baseclient.h>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>

/**
 * ClientManager singleton provides various views upon all clients across
 * all modules; a number of views are supported, including "all clients sharing
 * specific file", "all connected clients", "all queued clients" and such.
 *
 * Additionally, ClientManager is responsible for upload management - opening
 * new slots when existing slots don't fill up all available upload bandwidth,
 * and closing slots when too many slots get opened.
 */
class HNCORE_EXPORT ClientManager {
public:
	// the implementation data structures; you don't need to understand this.
	// Public api starts few pages below
	struct Indices : public boost::multi_index::indexed_by<
		boost::multi_index::hashed_unique<
			boost::multi_index::identity<BaseClient*>
		>, // sorted by module
		boost::multi_index::ordered_non_unique<
			boost::multi_index::const_mem_fun<
				BaseClient, ModuleBase*, &BaseClient::getModule
			>
		>, // connected clients + module
		boost::multi_index::ordered_non_unique<
			boost::multi_index::composite_key<
				BaseClient*,
				boost::multi_index::const_mem_fun<
					BaseClient, bool,
					&BaseClient::isConnected
				>,
				boost::multi_index::const_mem_fun<
					BaseClient, ModuleBase*,
					&BaseClient::getModule
				>
			>
		>, // sources
		boost::multi_index::ordered_non_unique<
			boost::multi_index::composite_key<
				BaseClient*,
				boost::multi_index::const_mem_fun<
					BaseClient, bool,
					&BaseClient::canDownload
				>,
				boost::multi_index::const_mem_fun<
					BaseClient, PartData*,
					&BaseClient::getSource
				>
			>
		>, // clients in our upload-queue
		boost::multi_index::ordered_non_unique<
			boost::multi_index::composite_key<
				BaseClient*,
				boost::multi_index::const_mem_fun<
					BaseClient, bool,
					&BaseClient::wantsUpload
				>,
				boost::multi_index::const_mem_fun<
					BaseClient, SharedFile*,
					&BaseClient::getReqFile
				>
			>
		>, // client's we'r uploading to
		boost::multi_index::ordered_non_unique<
			boost::multi_index::composite_key<
				BaseClient*,
				boost::multi_index::const_mem_fun<
					BaseClient, bool,
					&BaseClient::isUploading
				>,
				boost::multi_index::const_mem_fun<
					BaseClient, SharedFile*,
					&BaseClient::getReqFile
				>
			>
		>, // clients we'r downloading from
		boost::multi_index::ordered_non_unique<
			boost::multi_index::composite_key<
				BaseClient*,
				boost::multi_index::const_mem_fun<
					BaseClient, bool,
					&BaseClient::isDownloading
				>,
				boost::multi_index::const_mem_fun<
					BaseClient, PartData*,
					&BaseClient::getSource
				>
			>
		>
	> {};
	struct Container
		: public boost::multi_index_container<BaseClient*,Indices>
	{};
	typedef Container::nth_index<0>::type ClientIndex;
	typedef Container::nth_index<1>::type ModuleIndex;
	typedef Container::nth_index<2>::type ConnectedIndex;
	typedef Container::nth_index<3>::type SourceIndex;
	typedef Container::nth_index<4>::type QueueIndex;
	typedef Container::nth_index<5>::type UploadingIndex;
	typedef Container::nth_index<6>::type DownloadingIndex;
	typedef std::pair<
		ModuleIndex::const_iterator,
		ModuleIndex::const_iterator
	> ModuleClients;
	typedef std::pair<
		ConnectedIndex::const_iterator,
		ConnectedIndex::const_iterator
	> ConnectedClients;
	typedef std::pair<
		SourceIndex::const_iterator,
		SourceIndex::const_iterator
	> SourceClients;
	typedef std::pair<
		QueueIndex::const_iterator,
		QueueIndex::const_iterator
	> QueuedClients;
	typedef std::pair<
		UploadingIndex::const_iterator,
		UploadingIndex::const_iterator
	> UploadingClients;
	typedef std::pair<
		DownloadingIndex::const_iterator,
		DownloadingIndex::const_iterator
	> DownloadingClients;

	//! @returns The only instance of this class
	static ClientManager& instance();

	//! @returns Total number of clients known
	std::size_t count() const { return m_clients.size(); }

	/**
	 * \name Views
	 * All of these return a range of clients matching the query, in form of
	 * std::pair<first, one-past-last>.
	 */
	//!@{
	//! @returns All connected clients
	ConnectedClients getConnected() const {
		return m_connectedIndex.equal_range(boost::make_tuple(true));
	}
	//! @returns All connected clients for specified module
	ConnectedClients getConnected(ModuleBase *mod) const {
		return m_connectedIndex.equal_range(
			boost::make_tuple(true, mod)
		);
	}
	//! @returns All clients for specified module
	ModuleClients find(ModuleBase *mod) const {
		return m_moduleIndex.equal_range(mod);
	}
	//! @returns All sources
	SourceClients getSources() const {
		return m_sourceIndex.equal_range(boost::make_tuple(true));
	}
	//! @returns All sources for specified file
	SourceClients getSources(PartData *file) const {
		return m_sourceIndex.equal_range(
			boost::make_tuple(true, file)
		);
	}
	//! @returns All queued clients
	QueuedClients getQueued() const {
		return m_queueIndex.equal_range(boost::make_tuple(true));
	}
	//! @returns All queued clients for specified file
	QueuedClients getQueued(SharedFile *file) const {
		return m_queueIndex.equal_range(boost::make_tuple(true, file));
	}
	//! @returns All currently uploading clients
	UploadingClients getUploading() const {
		return m_uploadingIndex.equal_range(boost::make_tuple(true));
	}
	//! @returns All clients currently uploading specified file
	UploadingClients getUploading(SharedFile *file) const {
		return m_uploadingIndex.equal_range(
			boost::make_tuple(true, file)
		);
	}
	//! @returns All downloading clients
	DownloadingClients getDownloading() const {
		return m_downloadingIndex.equal_range(boost::make_tuple(true));
	}
	//! @returns All clients currently downloading specified file
	DownloadingClients getDownloading(PartData *file) const {
		return m_downloadingIndex.equal_range(
			boost::make_tuple(true, file)
		);
	}
	//!@}
private:
	// singleton pattern requirements
	ClientManager();
	ClientManager(const ClientManager&);
	ClientManager& operator=(const ClientManager&);
	~ClientManager();

	friend class BaseClient;

	// references to various views of the container
	Container         m_clients;
	ClientIndex      &m_clientIndex;
	ModuleIndex      &m_moduleIndex;
	ConnectedIndex   &m_connectedIndex;
	SourceIndex      &m_sourceIndex;
	QueueIndex       &m_queueIndex;
	UploadingIndex   &m_uploadingIndex;
	DownloadingIndex &m_downloadingIndex;
};

#endif
