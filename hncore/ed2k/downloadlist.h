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
 * \file downloadlist.h Interface for Download and DownloadList classes
 */

#ifndef __ED2K_DOWNLOADLIST_H__
#define __ED2K_DOWNLOADLIST_H__

#include <hncore/ed2k/fwd.h>
#include <hnbase/hash.h>
#include <hnbase/event.h>
#include <hncore/fwd.h>
#include <boost/tuple/tuple.hpp>
#include <boost/signals.hpp>

namespace Donkey {

class DownloadList;
namespace Detail {
	struct MIDownloadList;
	struct DownloadIter;
}

/**
 * Constants related to source-exchange times
 */
enum {
	SRCEXCH_CTIME = 40*60*1000, //!< one query per client per 40 minutes
	SRCEXCH_FTIME = 5*60*1000   //!< one query per file per 5 minutes
};

/**
 * Download is a wrapper class around ed2k-compatible PartData objects. It keeps
 * track of which clients are offering this file, providing means of getting an
 * overview of sources-per-file and cross-reference. Download objects are
 * owned and managed by DownloadList class.
 */
class Download : public Trackable {
	//! identical to AnswerSources::Source
	typedef boost::tuple<uint32_t, uint16_t, uint32_t, uint16_t> Source;
	typedef std::set<Client*>::const_iterator CIter;
public:
	uint32_t getSourceCount() const { return m_sources.size(); }
	uint64_t getLastSrcExch() const { return m_lastSrcExch; }
	PartData* getPartData() const { return m_partData; }
	uint32_t getSourceLimit() const { return m_sourceLimit; }
	Hash<ED2KHash> getHash() const { return m_hash; }
	uint32_t getSize() const;

	void addSource(Client *c) { m_sources.insert(c); }
	void delSource(Client *c) { m_sources.erase(c); }
	void setSourceLimit(uint32_t l) { m_sourceLimit = l; }
	void setLastSrcExch(uint64_t t) { m_lastSrcExch = t; }

	/**
	 * Generate a vector of sources, for sending with AnswerSources packet
	 * for example. Up to 500 sources may be returned.
	 */
	std::vector<Source> getSources() const;

	/**
	 * Check if a client is allowed to request sources for this file at
	 * this time.
	 *
	 * @param c      Client wishing to perform SourceExchange request
	 * @return       True if request is allowed, false otherwise
	 */
	bool isSourceReqAllowed(Client *c) const;

private:
	friend class DownloadList;
	friend bool operator<(const Download &x, const Download &y) {
		return x.m_hash < y.m_hash;
	}

	/**
	 * Construct new Download; this is allowed only by DownloadList
	 *
	 * @param pd      PartData object to wrap around
	 * @param hash    Hash of the PartData object
	 */
	Download(PartData *pd, const Hash<ED2KHash> &hash);

	//! Destructor only allowed by DownloadList
	~Download();

	//! Copy-construction is forbidden and not implemented
	Download(const Download&);
	//! Assignment operator is forbidden and not implemented
	Download& operator=(const Download&);

	/**
	 * Called prior to this object's destruction, signals all sources that
	 * download is being destroyed.
	 */
	void destroy();

	/**
	 * Signal handler for PartData::getLinks signal.
	 */
	void getLink(PartData *file, std::vector<std::string>& links);

	PartData*         m_partData;      //!< Implementation object
	Hash<ED2KHash>    m_hash;          //!< hash of this file
	std::set<Client*> m_sources;       //!< list of sources of this file
	uint64_t          m_lastSrcExch;   //!< time of last source-exchange req
	uint32_t          m_sourceLimit;   //!< limit sources
};

namespace Detail {
	struct MIDownloadList;
	struct MIDownloadListIterator;
}

/**
 * DownloadList is a container for Download objects, wrapping around PartData
 * objects. The purpose is to provide a simpler and faster interface than
 * FilesList, and to provide some ed2k-specific features that FilesList cannot
 * offer.
 *
 * Two public signals declared in this class indicate when new downloads are
 * added or removed. Iterating on the list can be done using nested type Iter,
 * and begin() / end() functions.
 */
class DownloadList {
public:
	/**
	 * Iterator for the underlying implementation.
	 */
	class Iter {
		typedef boost::shared_ptr<Detail::MIDownloadListIterator> Impl;
	public:
		Download& operator*();
		Download& operator->();
		void operator++();
		void operator--();
		bool operator==(const Iter&) const;
		bool operator!=(const Iter&) const;
	private:
		friend class DownloadList;
		Iter(Impl impl);  //!< Only allowed by DownloadList
		Impl m_impl;      //!< Implementation object
	};

	/**
	 * \returns Iterator to the beginning of the list
	 */
	Iter begin();

	/**
	 * \return Returns iterator to one-past-end of the list
	 */
	Iter end();

	/**
	 * \returns Reference to the only instance of this Singleton class
	 */
	static DownloadList& instance();

	/**
	 * Find a specific download, searching with hash
	 *
	 * \param hash       Hash to be searched for
	 * \returns          Pointer to the file, or 0 if not found
	 */
	Download* find(const Hash<ED2KHash> &hash) const;

	/**
	 * Check the validity of a Download pointer
	 *
	 * @param ptr      Pointer to check validity
	 * @return         True if the pointer is valid; false otherwise
	 */
	bool valid(Download *ptr) const;

	//! Emitted when a download is removed
	boost::signal<void (Download&)> onRemoved;
	//! Emitted when a download is added
	boost::signal<void (Download&)> onAdded;
private:
	DownloadList();
	DownloadList(const DownloadList&);
	~DownloadList();
	DownloadList& operator=(const DownloadList&);

	friend class ED2K;
	void init();    //!< Initialize this class
	void exit();    //!< Exit this class

	void onPDEvent(PartData *pd, int evt);
	void onSFEvent(SharedFile *sf, int evt);

	/**
	 * Imports ed2k-compatible downloads from location
	 *
	 * @param p   Path to import from
	 */
	void import(boost::filesystem::path p);

	/**
	 * Attempts to import a single file
	 *
	 * @param p   File to be imported
	 */
	void doImport(const boost::filesystem::path &p);

	/**
	 * Try to add a download to the list. The file needs to have ed2k-
	 * compatible hashes and must be <4gb in size. If addition succeeds,
	 * onAdded() signal is emitted (but only if the download is in `running'
	 * state.
	 *
	 * \param file   File to be added
	 */
	void tryAddFile(PartData *pd);

	//! List implementation
	boost::scoped_ptr<Detail::MIDownloadList> m_list;
};

} // end namespace Donkey

#endif
