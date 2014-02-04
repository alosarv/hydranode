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

#ifndef __ED2K_SEARCH_H__
#define __ED2K_SEARCH_H__

/**
 * \file ed2ksearch.h Interface for ed2k searching classes
 */

#include <hnbase/hash.h>
#include <hncore/search.h>

namespace Donkey {

/**
 * SearchFile is a customized ED2KFile which adds search-result related
 * members. This type should be used when handling search results.
 */
class ED2KSearchResult : public SearchResult {
public:
	/**
	 * Construct using hash, name and type.
	 *
	 * @param h            ED2KHash of the file
	 * @param name         File name
	 * @param size         File size
	 * @param id           ID of the client sharing this file
	 * @param port         Port of the client sharing this file
	 */
	ED2KSearchResult(
		Hash<ED2KHash> h, const std::string &name, uint32_t size
	);

	/**
	 * Download this SearchFile
	 */
	virtual void download();

	/**
	 * @returns unique identifier (hash + size combo) for this result
	 */
	virtual std::string identifier() const;
private:
	Hash<ED2KHash> m_hash;
};

} // end namespace Donkey

#endif
