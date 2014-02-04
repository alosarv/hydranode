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
 * \file ed2ksearch.cpp Implementation of ED2KSearchResult and
 *                      ED2KSearch classes
 */

#include <hncore/ed2k/ed2ksearch.h>
#include <hncore/ed2k/ed2ktypes.h>
#include <hncore/ed2k/clientlist.h>
#include <hnbase/log.h>
#include <hnbase/timed_callback.h>
#include <hncore/metadb.h>
#include <hncore/metadata.h>
#include <hncore/fileslist.h>
#include <hncore/sharedfile.h>
#include <boost/lexical_cast.hpp>

namespace Donkey {

ED2KSearchResult::ED2KSearchResult(
	Hash<ED2KHash> h, const std::string &name, uint32_t size
) : SearchResult(name, size), m_hash(h) {}

void ED2KSearchResult::download() {
	logDebug(
		boost::format("Starting download %s Hash: %s")
		% getName() % m_hash.decode()
	);
	SharedFile *sf = MetaDb::instance().findSharedFile(m_hash);
	if (sf && sf->isPartial()) {
		logMsg(
			boost::format(
				"You are already attempting to download %s"
			) % sf->getName()
		);
		return;
	} else if (sf) {
		logMsg(
			boost::format("You already have file %s")
			% sf->getName()
		);
		return;
	}

	MetaData *md = new MetaData(getSize());
	md->addFileName(getName());
	ED2KHashSet *hs = new ED2KHashSet(m_hash);

	// files smaller than chunksize have 1 chunkhash equal to filehash
	if (md->getSize() <= ED2K_PARTSIZE) {
		logDebug(
			"File is smaller than 9500KB - "
			"adding place-holder chunkhash."
		);
		hs->addChunkHash(m_hash.getData());
	}

	md->addHashSet(hs);
	MetaDb::instance().push(md);
	FilesList::instance().createDownload(getName(), md);

	for (CSIter j = sbegin(); j != send(); ++j) {
		Utils::timedCallback(
			boost::bind(
				&ClientList::addSource, &ClientList::instance(),
				m_hash, *j, IPV4Address(), true
			), 500
		);
	}
}

std::string ED2KSearchResult::identifier() const {
	return m_hash.toString() + boost::lexical_cast<std::string>(getSize());
}

} // end namespace Donkey
