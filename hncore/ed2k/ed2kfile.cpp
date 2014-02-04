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
 * \file ed2kfile.cpp Implementation of ED2KFile class
 */

#include <hncore/ed2k/ed2kfile.h>
#include <hncore/ed2k/opcodes.h>
#include <hncore/ed2k/ed2k.h>
#include <hncore/ed2k/tag.h>
#include <hncore/sharedfile.h>

namespace Donkey {

// ED2KFile class
// --------------
// Constructor.
// Convert Hydranode FileType enum to eDonkey2000-compatible file type string.
ED2KFile::ED2KFile(
	Hash<ED2KHash> h, const std::string &name, uint32_t size,
	FileType type, uint8_t flags
) : m_hash(h), m_name(name), m_size(size), m_hnType(type), m_flags(flags),
m_id(0), m_port(0) {
	m_ed2kType = HNType2ED2KType(m_hnType);
}

// Constructor
// Convert eDonkey2000-compatible filetype string into Hydranode FileType enum
ED2KFile::ED2KFile(
	Hash<ED2KHash> h, const std::string &name, uint32_t size,
	const std::string &type, uint8_t flags
) : m_hash(h), m_name(name), m_size(size), m_ed2kType(type), m_flags(flags),
m_id(0), m_port(0) {
	m_hnType = ED2KType2HNType(type);
}
ED2KFile::ED2KFile(
	Hash<ED2KHash> h, const std::string &name, uint32_t size,
	uint32_t id, uint16_t port
) : m_hash(h), m_name(name), m_size(size), m_hnType(FT_UNKNOWN), m_id(id),
m_port(port) {}

std::ostream& operator<<(std::ostream &o, const ED2KFile &f) {
	Utils::putVal<std::string>(o, f.m_hash.getData(), 16);
	if (f.m_flags & ED2KFile::FL_USECOMPLETEINFO) {
		if (f.m_flags & ED2KFile::FL_COMPLETE) {
			Utils::putVal<uint32_t>(o, FL_COMPLETE_ID);
			Utils::putVal<uint16_t>(o, FL_COMPLETE_PORT);
		} else {
			Utils::putVal<uint32_t>(o, FL_PARTIAL_ID);
			Utils::putVal<uint16_t>(o, FL_PARTIAL_PORT);
		}
	} else {
		Utils::putVal<uint32_t>(o, ED2K::instance().getId());
		Utils::putVal<uint16_t>(o, ED2K::instance().getTcpPort());
	}
	uint32_t tagCount = 2;
	if (f.getStrType().size()) {
		tagCount++;
	}
	Utils::putVal<uint32_t>(o, tagCount);
	o << Tag(CT_FILENAME, f.getName());
	o << Tag(CT_FILESIZE, f.getSize());
	if (f.getStrType().size()) {
		o << Tag(CT_FILETYPE, f.getStrType());
	}
	return o;
}

// Type conversions
std::string ED2KFile::HNType2ED2KType(FileType type) {
	switch (type) {
		case FT_VIDEO:    return FT_ED2K_VIDEO;
		case FT_AUDIO:    return FT_ED2K_AUDIO;
		case FT_IMAGE:    return FT_ED2K_IMAGE;
		case FT_DOCUMENT: return FT_ED2K_DOCUMENT;
		case FT_ARCHIVE:
		case FT_PROGRAM:
		case FT_CDIMAGE:  return FT_ED2K_PROGRAM;
		default:          return "";
	}
}
FileType ED2KFile::ED2KType2HNType(const std::string &type) {
	if (type == FT_ED2K_AUDIO) {
		return FT_AUDIO;
	} else if (type == FT_ED2K_VIDEO) {
		return FT_VIDEO;
	} else if (type == FT_ED2K_IMAGE) {
		return FT_IMAGE;
	} else if (type == FT_ED2K_DOCUMENT) {
		return FT_DOCUMENT;
	} else if (type == FT_ED2K_PROGRAM) {
		return FT_PROGRAM;
	} else {
		return FT_UNKNOWN;
	}
}

// construct ED2KFile from data
boost::shared_ptr<ED2KFile> makeED2KFile(
	SharedFile *sf, MetaData *md, HashSetBase *hs, bool useCompleteInfo
) {
	uint8_t flags = 0;
	if (sf->isComplete()) {
		flags &= ED2KFile::FL_COMPLETE;
	}
	if (useCompleteInfo) {
		flags &= ED2KFile::FL_USECOMPLETEINFO;
	}
	return boost::shared_ptr<ED2KFile>(
		new ED2KFile(
			hs->getFileHash().getData(), sf->getName(),
			sf->getSize(), md->getFileType(), flags
		)
	);
}

std::string ED2KFile::ratingToString(const ED2KFile::Rating &r) {
	switch (r) {
		case FR_INVALID:   return "Invalid";
		case FR_POOR:      return "Poor";
		case FR_GOOD:      return "Good";
		case FR_FAIR:      return "Fair";
		case FR_EXCELLENT: return "Excellent";
		case FR_NORATING:
		default:           return "";
	}
}

} // end namespace Donkey
