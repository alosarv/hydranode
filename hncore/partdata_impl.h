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
 * \file partdata_impl.h
 * Provides API for Chunk and ChunkMap classes, used by PartData; this header
 * is needed to be included when using PartData::getChunks() method.
 *
 * The purpose for keeping this separate is compile time - heavy multi_index
 * library usage of the implementation and the wide-spread inclusion of
 * partdata.h caused unneccery increase in compile times.
 */

#ifndef __PARTDATA_IMPL_H__
#define __PARTDATA_IMPL_H__

#include <hncore/partdata.h>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/key_extractors.hpp>
#include <boost/multi_index/ordered_index.hpp>

namespace Detail {

class Chunk : public Range64, public Trackable {
public:
	/**
	 * Construct new chunk.
	 *
	 * @param parent      Parent object
	 * @param begin       Begin offset (inclusive)
	 * @param end         End offset (inclusive)
	 * @param size        Ideal size for this chunk
	 * @param hash        Optional chunkhash for this data
	 */
	Chunk(
		PartData *parent, uint64_t begin, uint64_t end,
		uint32_t size, const HashBase *hash = 0
	);

	/**
	 * Construct Chunk from pre-existing Range object
	 *
	 * @param parent      Parent object
	 * @param range       The range
	 * @param size        Ideal size for this chunk
	 * @param hash        Optional chunkhash for this data
	 */
	Chunk(
		PartData *parent, Range64 range,
		uint32_t size, const HashBase *hash = 0
	);

	//! \name Getters
	//! @{
	bool            isVerified() const { return m_verified; }
	bool            isPartial()  const { return m_partial;  }
	uint32_t        getAvail()   const { return m_avail;    }
	bool            hasAvail()   const { return m_avail;    }
	uint32_t        getUseCnt()  const { return m_useCnt;   }
	uint32_t        getSize()    const { return m_size;     }
	const HashBase* getHash()    const { return m_hash;     }
	bool            isComplete() const{ return m_parent->isComplete(*this);}
	//! @}

	/**
	 * Schedules hash job to verify this chunk
	 *
	 * @param save       Value passed to PartData::verify() function
	 */
	void verify(bool save = true);

	//! Updates m_partial and m_complete states
	void updateState() const;

	/**
	 * Hash event handler; actually this is called from PartData Hash event
	 * handler.
	 *
	 * @param w         Event source object
	 * @param evt       Event itself
	 */
	void onHashEvent(HashWorkPtr w, HashEvent evt);

public: // Needed public for Boost::multi_index
	PartData       *m_parent;   //!< File this chunk belongs to
	const HashBase *m_hash;     //!< Optional
	bool            m_verified; //!< If it is verified.
	bool            m_partial;  //!< If it is partially downloaded
	bool            m_complete; //!< If it is completed
	uint32_t        m_avail;    //!< Availability count
	uint32_t        m_useCnt;   //!< Use count
	//! Ideal size, e.g. 9500kb for ed2k-chunk etc. Note that the
	//! real size of the chunk may be lower (in case of last chunk)
	uint32_t        m_size;
private:
	Chunk(); //!< Forbidden
	friend class Detail::LockedRange;
	friend class PartData;

	//! Allowed by LockedRange class
	void write(uint64_t begin, const std::string &data);
};

// chunk indices; since this file has using boost::multi_index in effect,
// this can be written in a compact way w/o explicit namespace qualifications
//
// the second composite_key-based index is used for selecting next chunk for
// downloading in getRange() method. The logic in plain english goes like this:
//
// Select an incomplete chunk, which has non-zero availability, prefering less-
// used, partiallly-downloaded and rare chunks.
//
// This means that for all chunks with usecnt 0, we prefer partially-downloaded
// ones (in order to complete them), and out of those, we prefer the rarest
// one. If there are no partially-downloaded chunks with usecnt 0, we still
// select the rarest of them. And if there aren't any chunks with usecnt 0,
// we move on to usecnt=1, and try again, first the partial ones (sorted by
// availability), then impartial, and so on, moving up the usecnt.
//
// This guarantees that the rarest chunk, with smallest use-count, prefering
// partially-downloaded ones, is always selected.
//
// Implementation note: the second index must be used to filter out chunks
// with zero availability, since they can never be selected.
//
// Implementation note: Overriding comparison functors is required for the
// fourth index, where we want 'true' values be of higher priority than false.
//
// A side-effect of this is that first and last chunks are almost always
// selected first. The first is selected since it's positioned at the beginning
// of the index (based on identiy-index, which seems to affect the other indices
// as well). Why the last chunk is selected next I'm unsure though.
struct ChunkMapIndices : boost::multi_index::indexed_by<
	boost::multi_index::ordered_non_unique<
		boost::multi_index::identity<Chunk>
	>,
	boost::multi_index::ordered_non_unique<
		boost::multi_index::composite_key<
			Chunk,
			boost::multi_index::member<
				Chunk, bool, &Chunk::m_complete
			>,
			boost::multi_index::const_mem_fun<
				Chunk, bool, &Chunk::hasAvail
			>,
			boost::multi_index::member<
				Chunk, uint32_t, &Chunk::m_useCnt
			>,
			boost::multi_index::member<
				Chunk, bool, &Chunk::m_partial
			>,
			boost::multi_index::member<
				Chunk, uint32_t, &Chunk::m_avail
			>
		>,
		boost::multi_index::composite_key_compare<
			std::less<bool>,
			std::less<bool>,
			std::less<uint32_t>,
			std::greater<bool>,
			std::less<uint32_t>
		>
	>,
	boost::multi_index::ordered_non_unique<
		boost::multi_index::member<Chunk, uint32_t, &Chunk::m_size>
	>
> {};
enum { ID_Pos, ID_Selector, ID_Length };
struct ChunkMap : public boost::multi_index_container<Chunk,ChunkMapIndices> {};
typedef ChunkMap::nth_index<Detail::ID_Pos     >::type CMPosIndex;
typedef ChunkMap::nth_index<Detail::ID_Selector>::type CMSelectIndex;
typedef ChunkMap::nth_index<Detail::ID_Length  >::type CMLenIndex;
struct PosIter : public CMPosIndex::iterator {
	template<typename T>
	PosIter(const T &t) : CMPosIndex::iterator(t) {}
};

} // namespace Detail

#endif
