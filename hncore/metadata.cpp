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
 * \file metadata.cpp Implementation of various metadata-related classes.
 */

#include <hncore/pch.h>

#include <hnbase/log.h>
#include <hnbase/hash.h>

#include <hncore/metadata.h>

using namespace CGComm;

// StreamData
// ----------
StreamData::StreamData() : m_bitrate() {}
StreamData::StreamData(const std::string &codec, uint32_t bitrate, uint32_t len)
: m_bitrate(bitrate), m_codec(codec), m_length(len) {}
StreamData::StreamData(std::istream &i) : m_bitrate(), m_length() {
	m_bitrate = Utils::getVal<uint32_t>(i);
	m_codec   = Utils::getVal<std::string>(i);
	m_length  = Utils::getVal<uint32_t>(i);
}
StreamData::~StreamData() {}

std::ostream& operator<<(std::ostream &o, const StreamData &s) {
	std::ostringstream tmp;
	Utils::putVal<uint32_t>(tmp, s.m_bitrate);
	Utils::putVal<std::string>(tmp, s.m_codec);
	Utils::putVal<uint32_t>(tmp, s.m_length);
	Utils::putVal<uint16_t>(o, tmp.str().size());
	o.write(tmp.str().c_str(), tmp.str().size());
	return o;
}

// VideoMetaData
// -------------
// Default constructor
VideoMetaData::VideoMetaData()
: m_runtime(), m_frames(), m_framerate(), m_framesize(0, 0), m_subtitles() {}
//! Destructor
VideoMetaData::~VideoMetaData() {}

// Construct and load
VideoMetaData::VideoMetaData(std::istream &i)
: m_runtime(), m_frames(), m_framerate(), m_framesize(0, 0), m_subtitles() {
	logTrace(TRACE_MD, "Reading VideoMetaData from stream.");
	uint16_t tagcount = Utils::getVal<uint16_t>(i);
	logTrace(TRACE_MD, boost::format("Reading %d tags.") % tagcount);
	while (tagcount--) {
		uint8_t opcode = Utils::getVal<uint8_t>(i);
		uint16_t len = Utils::getVal<uint16_t>(i);
		switch (opcode) {
			case OP_VMD_RUNTIME:
				m_runtime = Utils::getVal<uint32_t>(i);
				logTrace(
					TRACE_MD,
					boost::format("Got runtime: %d")
					% m_runtime
				);
				break;
			case OP_VMD_FRAMES:
				m_frames = Utils::getVal<uint32_t>(i);
				logTrace(
					TRACE_MD,
					boost::format("Got framecount: %d")
					% m_frames
				);
				break;
			case OP_VMD_FRAMERATE:
				m_framerate = Utils::getVal<float>(i);
				logTrace(
					TRACE_MD,
					boost::format("Got framerate: %f")
					% m_framerate
				);
				break;
			case OP_VMD_FRAMESIZE:
				m_framesize.first = Utils::getVal<uint32_t>(i);
				m_framesize.second = Utils::getVal<uint32_t>(i);
				logTrace(
					TRACE_MD,
					boost::format("Got framesize: %dx%d")
					% m_framesize.first % m_framesize.second
				);
				break;
			case OP_VMD_SUBTITLES:
				m_subtitles = Utils::getVal<uint32_t>(i);
				logTrace(
					TRACE_MD,
					boost::format("Got subtitle count: %d")
					% m_subtitles
				);
				break;
			case OP_VMD_VIDSTREAM:
				m_vidData.push_back(StreamData(i));
				logTrace(
					TRACE_MD,
					boost::format("Got video stream: %s/%d")
					% m_vidData.back().getCodec()
					% m_vidData.back().getBitrate()
				);
				break;
			case OP_VMD_AUDSTREAM:
				m_audData.push_back(StreamData(i));
				logTrace(
					TRACE_MD,
					boost::format("Got audio stream: %s/%d")
					% m_audData.back().getCodec()
					% m_audData.back().getBitrate()
				);
				break;
			default:
				logWarning(
					boost::format(
						"Unhandled tag %s found at "
						"offset %s while parsing "
						"VideoMetaData stream."
					) % Utils::hexDump(opcode)
					% Utils::hexDump(-3+i.tellg())
				);
				i.seekg(len, std::ios::cur);
				break;
		}
		if (!i) {
			logError(
				"Unexpected end of stream while parsing "
				"VideoMetaData stream."
			);
			break;
		}

	}
}

// Write to stream
std::ostream& operator<<(std::ostream &o, const VideoMetaData &vmd) {
	logTrace(TRACE_MD, "Writing VideoMetaData to stream.");
	Utils::putVal<uint8_t>(o, OP_VMD);
	std::ostringstream str;
	uint16_t tagcount = 0;
	if (vmd.m_runtime) {
		Utils::putVal<uint8_t>(str, OP_VMD_RUNTIME);
		Utils::putVal<uint16_t>(str, 4);
		Utils::putVal<uint32_t>(str, vmd.m_runtime);
		++tagcount;
	}
	if (vmd.m_frames) {
		Utils::putVal<uint8_t>(str, OP_VMD_FRAMES);
		Utils::putVal<uint16_t>(str, 4);
		Utils::putVal<uint32_t>(str, vmd.m_frames);
		++tagcount;
	}
	if (vmd.m_framerate) {
		Utils::putVal<uint8_t>(str, OP_VMD_FRAMERATE);
		Utils::putVal<uint16_t>(str, 4);
		Utils::putVal<float>(str, vmd.m_framerate);
		++tagcount;
	}
	if (vmd.m_framesize.first && vmd.m_framesize.second) {
		Utils::putVal<uint8_t>(str, OP_VMD_FRAMESIZE);
		Utils::putVal<uint16_t>(str, 8);
		Utils::putVal<uint32_t>(str, vmd.m_framesize.first);
		Utils::putVal<uint32_t>(str, vmd.m_framesize.second);
		++tagcount;
	}
	if (vmd.m_subtitles) {
		Utils::putVal<uint8_t>(str, OP_VMD_SUBTITLES);
		Utils::putVal<uint16_t>(str, 4);
		Utils::putVal<uint32_t>(str, vmd.m_subtitles);
		++tagcount;
	}
	if (vmd.m_vidData.size() > 0) {
		for (
			VideoMetaData::CSIter i = vmd.m_vidData.begin();
			i != vmd.m_vidData.end(); i++
		) {
			Utils::putVal<uint8_t>(str, OP_VMD_VIDSTREAM);
			str << *i;
			++tagcount;
		}
	}
	if (vmd.m_audData.size() > 0) {
		for (
			VideoMetaData::CSIter i = vmd.m_audData.begin();
			i != vmd.m_audData.end(); i++
		) {
			Utils::putVal<uint8_t>(str, OP_VMD_AUDSTREAM);
			str << *i;
			++tagcount;
		}
	}
	Utils::putVal<uint16_t>(o, str.str().size() + 2);
	Utils::putVal<uint16_t>(o, tagcount);
	Utils::putVal<std::string>(o, str.str(), str.str().size());
	logTrace(
		TRACE_MD,
		boost::format("VideoMetaData: %d tags written.") % tagcount
	);
	return o;
}

// AudioMetaData
// -------------
// Default constructor
AudioMetaData::AudioMetaData()
	: m_title(), m_artist(), m_album(), m_genre(), m_comment(),
	m_composer(), m_origartist(), m_copyright(), m_url(), m_encoded(),
	m_year() {
}
// Destructor
AudioMetaData::~AudioMetaData() {
	if (m_title)      { free(m_title);      }
	if (m_artist)     { free(m_artist);     }
	if (m_album)      { free(m_album);      }
	if (m_genre)      { free(m_genre);      }
	if (m_comment)    { free(m_comment);    }
	if (m_composer)   { free(m_composer);   }
	if (m_origartist) { free(m_origartist); }
	if (m_copyright)  { free(m_copyright);  }
	if (m_url)        { free(m_url);        }
	if (m_encoded)    { free(m_encoded);    }
}

// Construct and load
AudioMetaData::AudioMetaData(std::istream &i)
: m_title(), m_artist(), m_album(), m_genre(), m_comment(), m_composer(),
m_origartist(), m_copyright(), m_url(), m_encoded(), m_year() {
	using boost::format;
	logTrace(TRACE_MD, "Reading AudioMetaData from stream.");
	uint16_t tagcount = Utils::getVal<uint16_t>(i);
	logTrace(TRACE_MD, boost::format("%d tags to read.") % tagcount);
	while (tagcount--) {
		uint8_t opcode = Utils::getVal<uint8_t>(i);
		uint16_t len = Utils::getVal<uint16_t>(i);
		switch (opcode) {
			case OP_AMD_TITLE:
				setTitle(Utils::getVal<std::string>(
					i, len
				).value().c_str());
				logTrace(
					TRACE_MD,
					format("Got title: %s") % getTitle()
				);
				break;
			case OP_AMD_ARTIST:
				setArtist(Utils::getVal<std::string>(
					i, len
				).value().c_str());
				logTrace(
					TRACE_MD,
					format("Got artist: %s") % getArtist()
				);
				break;
			case OP_AMD_ALBUM:
				setAlbum(Utils::getVal<std::string>(
					i, len
				).value().c_str());
				logTrace(
					TRACE_MD,
					format("Got album: %s") % getAlbum()
				);
				break;
			case OP_AMD_GENRE:
				setGenre(Utils::getVal<std::string>(
					i, len
				).value().c_str());
				logTrace(
					TRACE_MD,
					format("Got genre: %s") % getGenre()
				);
				break;
			case OP_AMD_COMMENT:
				setComment(Utils::getVal<std::string>(
					i, len
				).value().c_str());
				logTrace(
					TRACE_MD,
					format("Got comment: %s") % getComment()
				);
				break;
			case OP_AMD_ORIGARTIST:
				setOrigArtist(Utils::getVal<std::string>(
					i, len
				).value().c_str());
				logTrace(
					TRACE_MD,
					format("Got origartist: %s") %
					getOrigArtist()
				);
				break;
			case OP_AMD_COPYRIGHT:
				setCopyright(Utils::getVal<std::string>(
					i, len
				).value().c_str());
				logTrace(
					TRACE_MD, format("Got copyright: %s")
					% getCopyright()
				);
				break;
			case OP_AMD_URL:
				setUrl(Utils::getVal<std::string>(
					i, len
				).value().c_str());
				logTrace(
					TRACE_MD, format("Got url: %s")
					% getUrl()
				);
				break;
			case OP_AMD_ENCODED:
				setEncoded(Utils::getVal<std::string>(
					i, len
				).value().c_str());
				logTrace(
					TRACE_MD,
					format("Got encoded: %s") % getEncoded()
				);
				break;
			case OP_AMD_YEAR:
				setYear(Utils::getVal<uint16_t>(i));
				logTrace(
					TRACE_MD,
					format("Got year: %d") % getYear()
				);
				break;
			case OP_AMD_COMPOSER:
				setComposer(Utils::getVal<std::string>(
					i, len
				).value().c_str());
				logTrace(
					TRACE_MD,
					format("Got composer: %s")
					% getComposer()
				);
				break;
			default:
				logWarning(
					boost::format(
						"Unhandled tag %s found at "
						"offset %s while parsing "
						"AudioMetaData stream."
					) % Utils::hexDump(opcode)
					% Utils::hexDump(-3+i.tellg())
				);
				i.seekg(len, std::ios::cur);
				break;
		}
		if (!i) {
			logError(
				"Unexpected end of stream while parsing "
				"AudioMetaData stream."
			);
			break;
		}
	}
}

// Write to stream
std::ostream& operator<<(std::ostream &o, const AudioMetaData &amd) {
	logTrace(TRACE_MD, "Writing AudioMetaData to stream.");
	Utils::putVal<uint8_t>(o, OP_AMD);
	uint16_t tagcount = 0;
	std::ostringstream str;
	if (amd.m_title) {
		Utils::putVal<uint8_t>(str, OP_AMD_TITLE);
		Utils::putVal<std::string>(str, amd.m_title);
		++tagcount;
	}
	if (amd.m_artist) {
		Utils::putVal<uint8_t>(str, OP_AMD_ARTIST);
		Utils::putVal<std::string>(str, amd.m_artist);
		++tagcount;
	}
	if (amd.m_album) {
		Utils::putVal<uint8_t>(str, OP_AMD_ALBUM);
		Utils::putVal<std::string>(str, amd.m_album);
		++tagcount;
	}
	if (amd.m_year) {
		Utils::putVal<uint8_t>(str, OP_AMD_YEAR);
		Utils::putVal<uint16_t>(str, 2);
		Utils::putVal<uint16_t>(str, amd.m_year);
		++tagcount;
	}
	if (amd.m_genre) {
		Utils::putVal<uint8_t>(str, OP_AMD_GENRE);
		Utils::putVal<std::string>(str, amd.m_genre);
		++tagcount;
	}
	if (amd.m_comment) {
		Utils::putVal<uint8_t>(str, OP_AMD_COMMENT);
		Utils::putVal<std::string>(str, amd.m_comment);
		++tagcount;
	}
	if (amd.m_composer) {
		Utils::putVal<uint8_t>(str, OP_AMD_COMPOSER);
		Utils::putVal<std::string>(str, amd.m_composer);
		++tagcount;
	}
	if (amd.m_origartist) {
		Utils::putVal<uint8_t>(str, OP_AMD_ORIGARTIST);
		Utils::putVal<std::string>(str, amd.m_origartist);
		++tagcount;
	}
	if (amd.m_copyright) {
		Utils::putVal<uint8_t>(str, OP_AMD_COPYRIGHT);
		Utils::putVal<std::string>(str, amd.m_copyright);
		++tagcount;
	}
	if (amd.m_url) {
		Utils::putVal<uint8_t>(str, OP_AMD_URL);
		Utils::putVal<std::string>(str, amd.m_url);
		++tagcount;
	}
	if (amd.m_encoded) {
		Utils::putVal<uint8_t>(str, OP_AMD_ENCODED);
		Utils::putVal<std::string>(str, amd.m_encoded);
		++tagcount;
	}
	Utils::putVal<uint16_t>(o, str.str().size() + 2);
	Utils::putVal<uint16_t>(o, tagcount);
	Utils::putVal<std::string>(o, str.str(), str.str().size());
	logTrace(
		TRACE_MD,
		boost::format("AudioMetaData: %d tags written.") % tagcount
	);
	return o;
}

// ArchiveMetaData
// ---------------
// Default constructor
ArchiveMetaData::ArchiveMetaData()
: m_format(), m_files(), m_uncomprsize(), m_ratio(), m_comment(), m_password() {
}

// Constructo and load
ArchiveMetaData::ArchiveMetaData(std::istream &i)
: m_format(), m_files(), m_uncomprsize(), m_ratio(), m_comment(), m_password() {
	logTrace(TRACE_MD, "Reading ArchiveMetaData from stream.");
	uint16_t tagcount = Utils::getVal<uint16_t>(i);
	logTrace(TRACE_MD, boost::format("%d tags to read") % tagcount);
	while (tagcount--) {
		uint8_t opcode = Utils::getVal<uint8_t>(i);
		uint16_t len = Utils::getVal<uint16_t>(i);
		switch (opcode) {
			case OP_ARMD_FORMAT:
				m_format = Utils::getVal<uint32_t>(i);
				logTrace(
					TRACE_MD,
					boost::format("Got format: %d")
					% m_format
				);
				break;
			case OP_ARMD_FILES:
				m_files = Utils::getVal<uint32_t>(i);
				logTrace(
					TRACE_MD,
					boost::format("Got filecount: %d")
					% m_files
				);
				break;
			case OP_ARMD_UNCOMPR:
				m_uncomprsize = Utils::getVal<uint64_t>(i);
				logTrace(
					TRACE_MD,
					boost::format("Got uncomprsize: %d")
					% m_uncomprsize
				);
				break;
			case OP_ARMD_RATIO:
				m_ratio = Utils::getVal<float>(i);
				logTrace(
					TRACE_MD,
					boost::format("Got ratio: %f")
					% m_ratio
				);
				break;
			case OP_ARMD_COMMENT:
				setComment(Utils::getVal<std::string>(
					i, len
				).value().c_str());
				logTrace(
					TRACE_MD,
					boost::format("Got comment: %s")
					% m_comment
				);
				break;
			case OP_ARMD_PASSWORD:
				m_password = Utils::getVal<uint8_t>(i);
				logTrace(
					TRACE_MD,
					boost::format("Got password: %s")
					% (m_password ? "true" : "false")
				);
				break;
			default:
				logWarning(
					boost::format(
						"Unhandled tag %s found at "
						"offset %s while parsing "
						"ArchiveMetaData stream."
					) % Utils::hexDump(opcode)
					% Utils::hexDump(-3+i.tellg())
				);
				i.seekg(len, std::ios::cur);
				break;
		}
		if (!i) {
			logError(
				"Unexpected end of stream while parsing "
				"ArchiveMetaData stream."
			);
			break;
		}
	}
}

// Write to stream
std::ostream& operator<<(std::ostream &o, const ArchiveMetaData &amd) {
	logTrace(TRACE_MD, "Writing ArchiveMetaData to stream.");
	Utils::putVal<uint8_t>(o, OP_ARMD);
	uint16_t tagcount = 0;
	std::ostringstream str;
	if (amd.m_format) {
		Utils::putVal<uint8_t>(str, OP_ARMD_FORMAT);
		Utils::putVal<uint16_t>(str, 4);
		Utils::putVal<uint32_t>(str, amd.m_format);
		++tagcount;
	}
	if (amd.m_files) {
		Utils::putVal<uint8_t>(str, OP_ARMD_FILES);
		Utils::putVal<uint16_t>(str, 4);
		Utils::putVal<uint32_t>(str, amd.m_files);
		++tagcount;
	}
	if (amd.m_uncomprsize) {
		Utils::putVal<uint8_t>(str, OP_ARMD_UNCOMPR);
		Utils::putVal<uint16_t>(str, 8);
		Utils::putVal<uint64_t>(str, amd.m_uncomprsize);
		++tagcount;
	}
	if (amd.m_ratio) {
		Utils::putVal<uint8_t>(str, OP_ARMD_RATIO);
		Utils::putVal<uint16_t>(str, 4);
		Utils::putVal<float>(str, amd.m_ratio);
		++tagcount;
	}
	if (amd.m_comment) {
		Utils::putVal<uint8_t>(str, OP_ARMD_COMMENT);
		Utils::putVal<std::string>(str, amd.m_comment);
		++tagcount;
	}
	if (amd.m_password) {
		Utils::putVal<uint8_t>(str, OP_ARMD_PASSWORD);
		Utils::putVal<uint16_t>(str, 1);
		Utils::putVal<uint8_t>(str, amd.m_password);
		++tagcount;
	}
	Utils::putVal<uint16_t>(o, str.str().size() + 2);
	Utils::putVal<uint16_t>(o, tagcount);
	Utils::putVal<std::string>(o, str.str(), str.str().size());
	logTrace(
		TRACE_MD,
		boost::format("ArchiveMetaData: %d tags written.") % tagcount
	);
	return o;
}

// ImageMetaData
// -------------
// Default constructor
ImageMetaData::ImageMetaData() : m_format(), m_width(), m_height(), m_comment(),
m_created() {
}

// Construct and load
ImageMetaData::ImageMetaData(std::istream &i) : m_format(), m_width(),
m_height(), m_comment(), m_created() {
	logTrace(TRACE_MD, "Reading ImageMetaData from stream.");
	uint16_t tagcount = Utils::getVal<uint16_t>(i);
	logTrace(TRACE_MD, boost::format("%d tags to read.") % tagcount);
	while (tagcount--) {
		uint8_t opcode = Utils::getVal<uint8_t>(i);
		uint16_t len = Utils::getVal<uint16_t>(i);
		switch (opcode) {
			case OP_IMD_FORMAT:
				m_format = Utils::getVal<uint32_t>(i);
				logTrace(
					TRACE_MD,
					boost::format("Got format: %d")
					% m_format
				);
				break;
			case OP_IMD_WIDTH:
				m_width = Utils::getVal<uint32_t>(i);
				logTrace(
					TRACE_MD,
					boost::format("Got width: %d")
					% m_width
				);
				break;
			case OP_IMD_HEIGHT:
				m_height = Utils::getVal<uint32_t>(i);
				logTrace(
					TRACE_MD,
					boost::format("Got height: %d")
					% m_height
				);
				break;
			case OP_IMD_DATE:
				m_created = Utils::getVal<uint32_t>(i);
				logTrace(
					TRACE_MD,
					boost::format("Got creation date: %d")
					% m_created
				);
				break;
			case OP_IMD_COMMENT:
				setComment(Utils::getVal<std::string>(
					i, len
				).value().c_str());
				logTrace(
					TRACE_MD,
					boost::format("Got comment: %s")
					% m_comment
				);
				break;
			default:
				logWarning(
					boost::format(
						"Unhandled tag %s found at "
						"offset %s while parsing "
						"ImageMetaData stream."
					) % Utils::hexDump(opcode)
					% Utils::hexDump(-3+i.tellg())
				);
				i.seekg(len, std::ios::cur);
				break;
		}
		if (!i) {
			logError(
				"Unexpected end of stream while parsing "
				"ImageMetaData stream."
			);
			break;
		}
	}
}

// Write to stream
std::ostream& operator<<(std::ostream &o, const ImageMetaData &imd) {
	logTrace(TRACE_MD, "Writing ImageMetaData to stream.");
	Utils::putVal<uint8_t>(o, OP_IMD);
	uint16_t tagcount = 0;
	std::ostringstream str;
	if (imd.m_format) {
		Utils::putVal<uint8_t>(str, OP_IMD_FORMAT);
		Utils::putVal<uint16_t>(str, 4);
		Utils::putVal<uint32_t>(str, imd.m_format);
		++tagcount;
	}
	if (imd.m_width) {
		Utils::putVal<uint8_t>(str, OP_IMD_WIDTH);
		Utils::putVal<uint16_t>(str, 4);
		Utils::putVal<uint32_t>(str, imd.m_width);
		++tagcount;
	}
	if (imd.m_height) {
		Utils::putVal<uint8_t>(str, OP_IMD_HEIGHT);
		Utils::putVal<uint16_t>(str, 4);
		Utils::putVal<uint32_t>(str, imd.m_height);
		++tagcount;
	}
	if (imd.m_comment) {
		Utils::putVal<uint8_t>(str, OP_IMD_COMMENT);
		Utils::putVal<std::string>(str, imd.m_comment);
		++tagcount;
	}
	if (imd.m_created) {
		Utils::putVal<uint8_t>(str, OP_IMD_DATE);
		Utils::putVal<uint16_t>(str, 4);
		Utils::putVal<uint32_t>(str, imd.m_created);
		++tagcount;
	}
	Utils::putVal<uint16_t>(o, str.str().size() + 2);
	Utils::putVal<uint16_t>(o, tagcount);
	Utils::putVal<std::string>(o, str.str(), str.str().size());
	logTrace(
		TRACE_MD,
		boost::format("ImageMetaData: %d tags written.") % tagcount
	);
	return o;
}

// MetaData
// --------
IMPLEMENT_EVENT_TABLE(MetaData, MetaData*, int);

// Constructor
MetaData::MetaData(uint64_t fileSize, FileType type, bool typeGuessed)
: m_archiveData(), m_imageData(), m_fileSize(fileSize), m_modDate(),
m_fileType(type), m_typeGuessed(typeGuessed), m_uploaded() {
}

// Default constructor
MetaData::MetaData() : m_archiveData(), m_imageData(), m_fileSize(),
m_modDate(), m_fileType(FT_UNKNOWN), m_typeGuessed(),
m_uploaded() {}

// Construct & load
MetaData::MetaData(std::istream &i) : m_archiveData(), m_imageData(),
m_fileSize(), m_modDate(), m_fileType(FT_UNKNOWN), m_typeGuessed(),
m_uploaded() {
	logTrace(TRACE_MD, "Reading MetaData from stream.");
	uint16_t tagcount = Utils::getVal<uint16_t>(i);
	logTrace(TRACE_MD, boost::format("%d tags to read.") % tagcount);
	while (tagcount--) {
		uint8_t opcode = Utils::getVal<uint8_t>(i);
		uint16_t len = Utils::getVal<uint16_t>(i);
		switch (opcode) {
			case OP_AMD:
				m_audioData.push_back(new AudioMetaData(i));
				logTrace(TRACE_MD, "Got AudioMetaData.");
				break;
			case OP_VMD:
				m_videoData.push_back(new VideoMetaData(i));
				logTrace(TRACE_MD, "Got VideoMetaData.");
				break;
			case OP_ARMD:
				m_archiveData = new ArchiveMetaData(i);
				logTrace(TRACE_MD, "Got ArchiveMetaData.");
				break;
			case OP_IMD:
				m_imageData = new ImageMetaData(i);
				logTrace(TRACE_MD, "Got ImageMetaData.");
				break;
			case OP_HASHSET:
				m_hashSets.push_back(loadHashSet(i));
				logTrace(TRACE_MD, "Got Hashset.");
				break;
			case OP_MD_FILENAME:
				m_fileName = Utils::getVal<std::string>(i, len);
				logTrace(TRACE_MD,
					boost::format("Got filename: %s")
					% m_fileName
				);
				break;
			case OP_MD_CUSTOM: {
				std::string tmp(
					Utils::getVal<std::string>(i, len)
				);
				m_customData.insert(tmp);
				logTrace(TRACE_MD,
					boost::format("Got CustomField: %s")
					% tmp
				);
				break;
			}
			case OP_MD_FILESIZE:
				m_fileSize = Utils::getVal<uint64_t>(i);
				logTrace(
					TRACE_MD,
					boost::format("Got size: %d")
					% m_fileSize
				);
				break;
			case OP_MD_MODDATE:
				m_modDate = Utils::getVal<uint32_t>(i);
				logTrace(
					TRACE_MD,
					boost::format("Got moddate: %d")
					% m_modDate
				);
				break;
			case OP_MD_FILETYPE:
				m_fileType = static_cast<FileType>(
					Utils::getVal<uint32_t>(i).value()
				);
				logTrace(
					TRACE_MD,
					boost::format("Got filetype: %d")
					% m_fileType
				);
				break;
			case OP_MD_TYPEGUESSED:
				m_typeGuessed = Utils::getVal<uint8_t>(i);
				logTrace(
					TRACE_MD,
					boost::format("Got typeguessed: %s")
					% (m_typeGuessed ? "true" : "false")
				);
				break;
			case OP_MD_UPLOADED:
				m_uploaded = Utils::getVal<uint64_t>(i);
				logTrace(
					TRACE_MD,
					boost::format("Got uploaded: %d")
					% m_uploaded
				);
				break;
			default:
				logWarning(
					boost::format(
						"Unknown tag %s found at "
						"offset %s while parsing "
						"MetaData stream."
					) % Utils::hexDump(opcode)
					% Utils::hexDump(-3+i.tellg())
				);
				i.seekg(len, std::ios::cur);
				break;
		}
		if (!i) {
			logError(
				"Unexpected end of stream while parsing "
				"MetaData stream."
			);
			break;
		}
	}
	for (uint32_t i = 0; i < m_hashSets.size(); ++i) {
		HashSetBase *hs = m_hashSets[i];
		if (!hs->getChunkCnt()) {
			continue;
		}
		uint32_t expected = m_fileSize / hs->getChunkSize() + 1;
		// ed2k has 1 more dummy hash when size % chunksize == 0
		if (hs->getFileHashTypeId() != OP_HT_ED2K) {
			if (m_fileSize % hs->getChunkSize() == 0) {
				--expected;
			}
		}
		if (expected != hs->getChunkCnt()) {
			hs->clearChunkHashes();
		}
	}
}

//! Destructor
MetaData::~MetaData() {
	if (m_archiveData) {
		delete m_archiveData;
	}
	if (m_imageData) {
		delete m_imageData;
	}
	while (m_videoData.size() > 0) {
		delete m_videoData.back();
		m_videoData.pop_back();
	}
	while (m_audioData.size() > 0) {
		delete m_audioData.back();
		m_audioData.pop_back();
	}
	while (m_hashSets.size() > 0) {
		delete m_hashSets.back();
		m_hashSets.pop_back();
	}
	getEventTable().delHandlers(this);
}

std::ostream& operator<<(std::ostream &o, const MetaData &md) {
	logTrace(TRACE_MD, "Writing MetaData to stream.");
	Utils::putVal<uint8_t>(o, OP_METADATA);

	uint16_t tagcount = 0;
	std::ostringstream str;

	Utils::putVal<uint8_t>(str, OP_MD_FILESIZE);
	Utils::putVal<uint16_t>(str, 8);
	Utils::putVal<uint64_t>(str, md.m_fileSize);
	++tagcount;
	Utils::putVal<uint8_t>(str, OP_MD_MODDATE);
	Utils::putVal<uint16_t>(str, 4);
	Utils::putVal<uint32_t>(str, md.m_modDate);
	++tagcount;
	Utils::putVal<uint8_t>(str, OP_MD_FILETYPE);
	Utils::putVal<uint16_t>(str, 4);
	Utils::putVal<uint32_t>(str, md.m_fileType);
	++tagcount;
	Utils::putVal<uint8_t>(str, OP_MD_TYPEGUESSED);
	Utils::putVal<uint16_t>(str, 1);
	Utils::putVal<uint8_t>(str, md.m_typeGuessed);
	++tagcount;
	Utils::putVal<uint8_t>(str, OP_MD_FILENAME);
	Utils::putVal<uint16_t>(str, md.m_fileName.size());
	Utils::putVal<std::string>(
		str, md.m_fileName.data(), md.m_fileName.size()
	);
	++tagcount;
	Utils::putVal<uint8_t>(str, OP_MD_UPLOADED);
	Utils::putVal<uint16_t>(str, 8);
	Utils::putVal<uint64_t>(str, md.m_uploaded);
	++tagcount;

	MetaData::CustomIter it = md.customBegin();
	while (it != md.customEnd()) {
		Utils::putVal<uint8_t>(str, OP_MD_CUSTOM);
		Utils::putVal<std::string>(str, *it++);
		++tagcount;
	}
	for (uint32_t i = 0; i < md.m_videoData.size(); ++i) {
		str << *md.m_videoData[i];
		++tagcount;
	}
	for (uint32_t i = 0; i < md.m_audioData.size(); ++i) {
		str << *md.m_audioData[i];
		++tagcount;
	}
	if (md.m_archiveData != 0) {
		str << *md.m_archiveData;
		++tagcount;
	}
	if (md.m_imageData != 0) {
		str << *md.m_imageData;
		++tagcount;
	}
	for (uint32_t i = 0; i < md.m_hashSets.size(); ++i) {
		str << *md.m_hashSets[i];
		++tagcount;
	}
	Utils::putVal<uint16_t>(o, str.str().size() + 2);
	Utils::putVal<uint16_t>(o, tagcount);
	Utils::putVal<std::string>(o, str.str(), str.str().size());
	logTrace(
		TRACE_MD,
		boost::format("MetaData: %d tags written.") % tagcount
	);
	return o;
}

void MetaData::setSize(uint64_t newSize) {
	CHECK_THROW(!m_fileSize);
	m_fileSize = newSize;
	m_hashSets.clear();
	if (newSize) {
		getEventTable().postEvent(this, MD_SIZE_CHANGED);
	}
}

void MetaData::setName(const std::string &name) {
	if (name == m_fileName) {
		return;
	}
	m_fileName = name;
	m_fileNames[name]++;

	getEventTable().postEvent(this, MD_NAME_CHANGED);
	if (m_fileNames[name] == 1) {
		getEventTable().postEvent(this, MD_ADDED_FILENAME);
	}
}

void MetaData::addFileName(const std::string &name) {
	if (!m_fileName.size()) {
		setName(name);
		m_fileNames[name]++;
	} else {
		m_fileNames[name]++;
		if (m_fileNames[name] == 1) {
			getEventTable().postEvent(
				this, MD_ADDED_FILENAME
			);
		}
	}
}

void MetaData::delFileName(const std::string &name) {
	if (m_fileNames.find(name) == m_fileNames.end()) {
		return;
	}
	m_fileNames[name]--;
	if (!m_fileNames[name]) {
		if (name == m_fileName) {
			m_fileNames[name]++;
		} else {
			m_fileNames.erase(name);
		}
	}
}

void MetaData::addHashSet(HashSetBase *hashset) {
	m_hashSets.push_back(hashset);
	getEventTable().postEvent(this, MD_ADDED_HASHSET);
}

void MetaData::addCustomData(const std::string &data) {
	if (m_customData.insert(data).second) {
		getEventTable().postEvent(this, MD_ADDED_CUSTOMDATA);
	}
}

void MetaData::addComment(const std::string &comment) {
	if (m_comments.insert(comment).second) {
		getEventTable().postEvent(this, MD_ADDED_COMMENT);
	}
}

void MetaData::addVideoData(VideoMetaData *vmd) {
	m_videoData.push_back(vmd);
	getEventTable().postEvent(this, MD_ADDED_VIDEO);
}

void MetaData::addAudioData(AudioMetaData *amd) {
	m_audioData.push_back(amd);
	getEventTable().postEvent(this, MD_ADDED_AUDIO);
}

void MetaData::setArchiveData(ArchiveMetaData *amd) {
	m_archiveData = amd;
	getEventTable().postEvent(this, MD_ADDED_ARCHIVE);
}

void MetaData::setImageData(ImageMetaData *imd) {
	m_imageData = imd;
	getEventTable().postEvent(this, MD_ADDED_IMAGE);
}

void MetaData::mergeCustom(MetaData *other) {
	CHECK_THROW(other);

	m_fileNames  = other->m_fileNames;
	m_customData = other->m_customData;
	m_comments   = other->m_comments;
}
