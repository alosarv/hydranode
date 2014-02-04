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
 * \file hash.cpp Implementation of Hash classes
 */

#include <hnbase/pch.h>
#include <hnbase/hash.h>

namespace CGComm {

HashBase* loadHash(std::istream &i) {
	uint8_t type = Utils::getVal<uint8_t>(i);
	switch (type) {
		case OP_HT_MD4:
			return new Hash<MD4Hash>(i);
		case OP_HT_MD5:
			return new Hash<MD5Hash>(i);
		case OP_HT_SHA1:
			return new Hash<SHA1Hash>(i);
		case OP_HT_ED2K:
			return new Hash<ED2KHash>(i);
		default:
			throw std::runtime_error(
				(boost::format("Requested creation of "
				"unsupported Hash type %s")
				% Utils::hexDump(type)).str()
			);
		}
	return 0;
}

inline void invalidType(uint8_t x, uint8_t y) {
	boost::format fmt(
		"Unknown HashSet type found in stream: "
		"FileHashType: %s PartHashType: %s"
	);
	fmt % Utils::hexDump(x) % Utils::hexDump(y);
	throw std::runtime_error(fmt.str());
}

//! Create a specific object based on information found from stream.
//! \todo This must be one the ugliest constructs I'v ever seen ... it
//! _probably_ could be done in a much nicer way than nested duplicating switch
//! statements...
HashSetBase* loadHashSet(std::istream &i) {
	logTrace(TRACE_HASH, "Creating new HashSet object.");
	// Types
	uint8_t parthashtype = Utils::getVal<uint8_t>(i);
	uint8_t filehashtype = Utils::getVal<uint8_t>(i);
	logTrace(TRACE_HASH,
		boost::format("FileHashType: %s PartHashType: %s")
		% Utils::hexDump(filehashtype)
		% Utils::hexDump(parthashtype)
	);
	// Special case - ED2KHashSet is defined with template-parameter
	// partsize (9728000), thus we must specifically construct it here
	// out of the ordinary, otherwise we will get a similar type, however,
	// typecasts between ED2KHashSet and whatever we create here wouldn't
	// work anymore. Thus - make this exception.
	// Note: Similar exceptions might be needed for other kinds of hashsets
	//       we will be supporting in the future which also have static
	//       chunk size. So add those here too, and use the below big
	//       switch/case statement only on last resort.
	if (filehashtype == OP_HT_ED2K && parthashtype == OP_HT_MD4) {
		return new ED2KHashSet(i);
	}
	switch (filehashtype) {
	case OP_HT_MD4:
		switch (parthashtype) {
			case OP_HT_MD4:
				return new HashSet<MD4Hash, MD4Hash>(i);
			case OP_HT_MD5:
				return new HashSet<MD5Hash, MD4Hash>(i);
			case OP_HT_SHA1:
				return new HashSet<SHA1Hash, MD4Hash>(i);
			default:
				invalidType(filehashtype, parthashtype);
		}
	case OP_HT_MD5:
		switch (parthashtype) {
			case OP_HT_MD4:
				return new HashSet<MD4Hash, MD5Hash>(i);
			case OP_HT_MD5:
				return new HashSet<MD5Hash, MD5Hash>(i);
			case OP_HT_SHA1:
				return new HashSet<SHA1Hash, MD5Hash>(i);
			default:
				invalidType(filehashtype, parthashtype);
		}
	case OP_HT_SHA1:
		switch (parthashtype) {
			case OP_HT_MD4:
				return new HashSet<MD4Hash, SHA1Hash>(i);
			case OP_HT_MD5:
				return new HashSet<MD5Hash, SHA1Hash>(i);
			case OP_HT_SHA1:
				return new HashSet<SHA1Hash, SHA1Hash>(i);
			default:
				invalidType(filehashtype, parthashtype);
		}
	case OP_HT_ED2K:
		switch (parthashtype) {
			case OP_HT_MD4:
				return new HashSet<MD4Hash, ED2KHash>(i);
			case OP_HT_MD5:
				return new HashSet<MD5Hash, ED2KHash>(i);
			case OP_HT_SHA1:
				return new HashSet<SHA1Hash, ED2KHash>(i);
			case OP_HT_ED2K:
				return new HashSet<ED2KHash, ED2KHash>(i);
			default:
				invalidType(filehashtype, parthashtype);
		}
	default:
		invalidType(filehashtype, parthashtype);
	}
	return 0;
}

} //! end CGComm namespace

using namespace CGComm;

// HashBase
// --------
HashBase::HashBase() {}
HashBase::~HashBase() {}

std::ostream& operator<<(std::ostream &o, const HashBase &h) {
	Utils::putVal<uint8_t>(o, OP_HASH);
	Utils::putVal<uint16_t>(o, h.size()+1);
	Utils::putVal<uint8_t>(o, h.getTypeId());
	Utils::putVal<std::string>(o, h.getData(), h.size());
	return o;
}

// HashSetBase
// -----------
HashSetBase::HashSetBase() {}
HashSetBase::~HashSetBase() {}

/**
 * HashSet object format:
 *
 * <uint8>OP_HASHSET<uint16>objlength<uint8>filehashtype<uint8>parthashtype
 * <uint16>tagcount[<tagcount>*Tag]
 */
std::ostream& operator<<(std::ostream &o, const HashSetBase &h) {
	Utils::putVal<uint8_t>(o, OP_HASHSET);
	uint16_t tagcount = 0;
	std::ostringstream str;
	if (!h.getFileHash().isEmpty()) {
		Utils::putVal<uint8_t>(str, OP_HS_FILEHASH);
		std::ostringstream tmp;
		tmp << h.getFileHash();
		Utils::putVal<uint16_t>(str, tmp.str().size());
		Utils::putVal<std::string>(str, tmp.str(), tmp.str().size());
		++tagcount;
	}
	for (uint32_t i = 0; i < h.getChunkCnt(); ++i) {
		Utils::putVal<uint8_t>(str, OP_HS_PARTHASH);
		std::ostringstream tmp;
		tmp << h.getChunkHash(i);
		Utils::putVal<uint16_t>(str, tmp.str().size());
		Utils::putVal<std::string>(str, tmp.str(), tmp.str().size());
		++tagcount;
	}
	if (h.getChunkSize()) {
		Utils::putVal<uint8_t>(str, OP_HS_PARTSIZE);
		Utils::putVal<uint16_t>(str, 4);
		Utils::putVal<uint32_t>(str, h.getChunkSize());
		++tagcount;
	}
	// +4 -> <uint16>tagcount<uint8>filehashtype<uint8>parthashtype
	Utils::putVal<uint16_t>(o, str.str().size() + 4);
	Utils::putVal<uint8_t>(o, h.getChunkHashTypeId());
	Utils::putVal<uint8_t>(o, h.getFileHashTypeId());
	Utils::putVal<uint16_t>(o, tagcount);
	Utils::putVal<std::string>(o, str.str(), str.str().size());
	return o;
}

// Compare us to ref, returning true if 100% matches
bool HashSetBase::compare(const HashSetBase &ref) const {
	logTrace(TRACE_HASH, "Comparing two hashsets.");
	if (getFileHashTypeId() != ref.getFileHashTypeId()) {
		logTrace(TRACE_HASH,
			boost::format("FileHashType %s != %s")
			% getFileHashTypeId() % ref.getFileHashTypeId()
		);
		return false;
	}
	if (getChunkHashTypeId() != ref.getChunkHashTypeId()) {
		logTrace(TRACE_HASH,
			boost::format("PartHashType %s != %s")
			% getChunkHashTypeId() % ref.getChunkHashTypeId()
		);
		return false;
	}
	if (getFileHash() != ref.getFileHash()) {
		logTrace(TRACE_HASH,
			boost::format("FileHash %s != %s")
			% getFileHash().decode() % ref.getFileHash().decode()
		);
		return false;
	}
	if (getChunkCnt() != ref.getChunkCnt()) {
		logTrace(TRACE_HASH,
			boost::format("PartHashCount %s != %s")
			% getChunkCnt() % ref.getChunkCnt()
		);
		return false;
	}
	for (uint32_t i = 0; i < getChunkCnt(); ++i) {
		if (getChunkHash(i) != ref.getChunkHash(i)) {
			logTrace(TRACE_HASH,
				boost::format("PartHash[%d] %s != %s")
				% i % getChunkHash(i).decode()
				% ref.getChunkHash(i).decode()
			);
			return false;
		}
	}
	logTrace(TRACE_HASH, "Hashsets are equal.");
	return true;
}

