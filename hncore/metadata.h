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
 * \file metadata.h Interface for various MetaData structurs
 */

#ifndef __METADATA_H__
#define __METADATA_H__

#include <hnbase/osdep.h>
#include <hnbase/utils.h>
#include <hnbase/event.h>

#include <hncore/fwd.h>

#include <iostream>
#include <set>
#include <string>

//! Object describing an audio/video stream.
class HNCORE_EXPORT StreamData {
public:
	//! Default constructor
	StreamData();

	//! Construct with pre-set values
	StreamData(const std::string &codec, uint32_t bitrate, uint32_t len =0);

	//! Load from input stream
	StreamData(std::istream &i);

	//! Destructor
	~StreamData();

	/**
	 * @name Setters
	 */
	//@{
	void setBitrate(uint32_t bitrate)       { m_bitrate = bitrate; }
	void setCodec(const std::string &codec) { m_codec   = codec;   }
	void setLength(uint32_t length)         { m_length  = length;  } 
	//@}

	/**
	 * @name Getters
	 */
	//@{
	uint32_t    getBitrate() const { return m_bitrate; }
	std::string getCodec()   const { return m_codec;   }
	uint32_t    getLength()  const { return m_length;  }
	//@}

	/**
	 * @name Comparison operators
	 */
	//!@{
	bool operator==(const StreamData &o) {
		bool ret = m_bitrate == o.m_bitrate;
		ret     &= m_codec == o.m_codec;
		ret     &= m_length == o.m_length;
		return ret;
	}
	bool operator!=(const StreamData &o) {
		return !(*this == o);
	}
	//!@}
private:
	uint32_t    m_bitrate;       //!< Bitrate
	std::string m_codec;         //!< Codec used
	uint32_t    m_length;        //!< Length of stream

	//! Output operator for streams
	friend std::ostream& operator<<(
		std::ostream &o, const StreamData &s
	);
};

namespace CGComm {
	//! VideoMetaData opcodes
	enum VMDOpCodes {
		OP_VMD           = 0x40, //!<                  VideoMetaData
		OP_VMD_RUNTIME   = 0x41, //!< <uint32>         Runtime
		OP_VMD_FRAMES    = 0x42, //!< <uint32>         Frame count
		OP_VMD_FRAMERATE = 0x43, //!< <float>          Frame rate (fps)
		OP_VMD_FRAMESIZE = 0x44, //!< <uint32><uint32> Frame size
		OP_VMD_SUBTITLES = 0x45, //!< <uint16>         Subtitle count
		OP_VMD_VIDSTREAM = 0x46, //!< <StreamData>     Video stream
		OP_VMD_AUDSTREAM = 0x47  //!< <StreamData>     Audio stream
	};
}

//! Video meta data
class HNCORE_EXPORT VideoMetaData {
public:
	VideoMetaData();                            //!< Default constructor
	VideoMetaData(std::istream&);               //!< Construct & load

	/**
	 * @name Setters
	 */
	//@{
	void setRunTime(uint32_t runtime)     { m_runtime = runtime;     }
	void setFrameCount(uint32_t frames)   { m_frames = frames;       }
	void setFrameRate(float framerate)    { m_framerate = framerate; }
	void setSubtitleCount(uint32_t count) { m_subtitles = count;     }
	void setFrameSize(uint32_t width, uint32_t height) {
		m_framesize.first = width;
		m_framesize.second = height;
	}
	void addVideoStream(const char *codec, uint32_t bitrate) {
		m_vidData.push_back(StreamData(codec, bitrate));
	}
	void addAudioStream(const char *codec, uint32_t bitrate) {
		m_audData.push_back(StreamData(codec, bitrate));
	}
	//@}

	/**
	 * @name Getters
	 */
	//@{
	uint32_t getRunTime()          const { return m_runtime;        }
	uint32_t getFrameCount()       const { return m_frames;         }
	float    getFrameRate()        const { return m_framerate;      }
	uint32_t getVideoStreamCount() const { return m_vidData.size(); }
	uint32_t getAudioStreamCount() const { return m_audData.size(); }
	uint32_t getSubtitleCount()    const { return m_subtitles;      }
	std::pair<uint32_t, uint32_t> getFrameSize() const {
		return m_framesize;
	}
	/**
	 * Retrieve a specific stream. Note: Stream counting starts from 0.
	 */
	StreamData getVideoStream(uint32_t num) const {
		assert(num <= m_vidData.size());
		return m_vidData.at(num);
	}
	/**
	 * Retrieve a specific stream. Note: Stream counting starts from 0.
	 */
	StreamData getAudioStream(uint32_t num) const {
		assert(num <= m_audData.size());
		return m_audData.at(num);
	}
	//@}
private:
	friend class MetaData;
	~VideoMetaData();                           //!< Allowed by MetaData
	VideoMetaData(const VideoMetaData&);            //!< Forbidden
	VideoMetaData& operator=(const VideoMetaData&); //!< Forbidden

	typedef std::vector<StreamData>::iterator SIter;
	typedef std::vector<StreamData>::const_iterator CSIter;

	uint32_t m_runtime;                         //!< Length of movie
	uint32_t m_frames;                          //!< Total number of frames
	float m_framerate;                          //!< frames-per-second
	std::pair<uint32_t, uint32_t> m_framesize;  //!< Size of a frame
	std::vector<StreamData> m_vidData;          //!< Video streams data
	std::vector<StreamData> m_audData;          //!< Audio streams data
	uint32_t m_subtitles;                       //!< Number of subtitles

	//! Output operator for streams
	friend std::ostream& operator<<(
		std::ostream &o, const VideoMetaData &vmd
	);

	//! In testsuite
	friend void test_videodata();
};

namespace CGComm {
	//! AudioMetaData opcodes
	enum AMD_OpCodes {
		OP_AMD            = 0x20,         //!<          AudioMetaData
		OP_AMD_TITLE      = 0x21,         //!< <string> Title
		OP_AMD_ARTIST     = 0x22,         //!< <string> Artist
		OP_AMD_ALBUM      = 0x23,         //!< <string> Album
		OP_AMD_GENRE      = 0x24,         //!< <string> Genre
		OP_AMD_COMMENT    = 0x25,         //!< <string> Comment
		OP_AMD_COMPOSER   = 0x26,         //!< <string> Composer
		OP_AMD_ORIGARTIST = 0x27,         //!< <string> Original artist
		OP_AMD_COPYRIGHT  = 0x28,         //!< <string> Copyright
		OP_AMD_URL        = 0x29,         //!< <string> Web URL
		OP_AMD_ENCODED    = 0x2a,         //!< <string> Encoded
		OP_AMD_YEAR       = 0x2b          //!< <uint16> Year
	};
}

// \todo MetaData structures copy constructors - for string copies

class HNCORE_EXPORT AudioMetaData {
public:
	//! Default constructor
	AudioMetaData();

	//!< Construct and load from stream
	AudioMetaData(std::istream &i);

	/**
	 * @name Setters
	 */
	//@{
	void setTitle(const char *title) {
		Utils::copyString(title, m_title);
	}
	void setArtist(const char *artist) {
		Utils::copyString(artist, m_artist);
	}
	void setAlbum(const char *album) {
		Utils::copyString(album, m_album);
	}
	void setGenre(const char *genre) {
		Utils::copyString(genre, m_genre);
	}
	void setComment(const char *comment) {
		Utils::copyString(comment, m_comment);
	}
	void setComposer(const char *composer) {
		Utils::copyString(composer, m_composer);
	}
	void setOrigArtist(const char *origartist) {
		Utils::copyString(origartist, m_origartist);
	}
	void setCopyright(const char *copyright) {
		Utils::copyString(copyright, m_copyright);
	}
	void setUrl(const char *url) {
		Utils::copyString(url, m_url);
	}
	void setEncoded(const char *encoded) {
		Utils::copyString(encoded, m_encoded);
	}
	void setYear(uint16_t year) { m_year = year; }
	//@}

	/**
	 * @name Getters
	 */
	//@{
	std::string getTitle()      const { return m_title;      }
	std::string getArtist()     const { return m_artist;     }
	std::string getAlbum()      const { return m_album;      }
	std::string getGenre()      const { return m_genre;      }
	std::string getComment()    const { return m_comment;    }
	std::string getComposer()   const { return m_composer;   }
	std::string getOrigArtist() const { return m_origartist; }
	std::string getCopyright()  const { return m_copyright;  }
	std::string getUrl()        const { return m_url;        }
	std::string getEncoded()    const { return m_encoded;    }
	uint16_t    getYear()       const { return m_year;       }
	//@}

	//! Output operator for streams
	friend std::ostream& operator<<(
		std::ostream &o, const AudioMetaData &amd
	);
private:
	friend class MetaData;
	~AudioMetaData();
	AudioMetaData(const AudioMetaData&);            //!< Forbidden
	AudioMetaData& operator=(const AudioMetaData&); //!< Forbidden

	char* m_title;                              //!< Track title
	char* m_artist;                             //!< Track performer
	char* m_album;                              //!< Album
	char* m_genre;                              //!< Genre
	char* m_comment;                            //!< Comment
	char* m_composer;                           //!< Composer
	char* m_origartist;                         //!< Original artist
	char* m_copyright;                          //!< Copyright string
	char* m_url;                                //!< Web url
	char* m_encoded;                            //!< Encoded
	uint16_t m_year;                            //!< Release year
};

namespace CGComm {
	//! ArchiveMetaData tags
	enum ARMDOpCodes {
		OP_ARMD          = 0x60,    //!< ArchiveMetaData
		OP_ARMD_FORMAT   = 0x61,    //!< <uint32> Format
		OP_ARMD_FILES    = 0x62,    //!< <uint32> File count
		OP_ARMD_UNCOMPR  = 0x63,    //!< <uint64> Uncompressed size
		OP_ARMD_RATIO    = 0x64,    //!< <float>  Compression ratio
		OP_ARMD_COMMENT  = 0x65,    //!< <string> Comment
		OP_ARMD_PASSWORD = 0x66     //!< <bool>   If password is set
	};
}

//! Archive meta data
class HNCORE_EXPORT ArchiveMetaData {
public:
	ArchiveMetaData();                  //!< Default constructor
	ArchiveMetaData(std::istream &i);   //!< Construct and load from stream

	/**
	 * @name Setters
	 */
	//@{
	void setFormat(uint32_t format)      { m_format = format;     }
	void setFileCount(uint32_t files)    { m_files = files;       }
	void setUnComprSize(uint64_t size)   { m_uncomprsize = size;  }
	void setComprRatio(float ratio)      { m_ratio = ratio;       }
	void setPassword(bool password)      { m_password = password; }
	void setComment(const char *comment) {
		Utils::copyString(comment, m_comment);
	}
	//@}

	/**
	 * @name Getters
	 */
	//@{
	uint32_t          getFormat()      const { return m_format;      }
	uint32_t          getFileCount()   const { return m_files;       }
	uint64_t          getUnComprSize() const { return m_uncomprsize; }
	float             getComprRatio()  const { return m_ratio;       }
	std::string       getComment()     const { return m_comment;     }
	bool              getPassword()    const { return m_password;    }
	//@}
private:
	friend class MetaData;
	~ArchiveMetaData() {
		if (m_comment) { free(m_comment); }
	}
	ArchiveMetaData(const ArchiveMetaData&);             //!< Forbidden
	ArchiveMetaData& operator=(const ArchiveMetaData&);  //!< Forbidden

	uint32_t m_format;                          //!< Compression format
	uint32_t m_files;                           //!< Number of files stored
	uint64_t m_uncomprsize;                     //!< Uncompressed size
	float m_ratio;                              //!< Compression ratio
	char* m_comment;                            //!< Comment
	bool m_password;                            //!< If password is set

	//! Output operator for streams
	friend std::ostream& operator<<(
		std::ostream &o, const ArchiveMetaData &amd
	);
	//! In testsuite
	friend void test_archivedata();
};

namespace CGComm {
	//! ImageMetaData opcodes
	enum IMDOpCodes {
		OP_IMD         = 0x80,             //!< ImageMetaData
		OP_IMD_FORMAT  = 0x81,             //!< <uint32> Format
		OP_IMD_WIDTH   = 0x82,             //!< <uint32> Height
		OP_IMD_HEIGHT  = 0x83,             //!< <uint32> Width
		OP_IMD_DATE    = 0x84,             //!< <uint32> Creation date
		OP_IMD_COMMENT = 0x85              //!< <string> Comment
	};
}

//! Image meta data
class HNCORE_EXPORT ImageMetaData {
public:
	ImageMetaData();                            //!< Default constructor
	ImageMetaData(std::istream &i);             //!< Construct & load

	/**
	 * @name Setters
	 */
	//@{
	void setFormat(uint32_t format)      { m_format = format;   }
	void setWidth(uint32_t width)        { m_width = width;     }
	void setHeight(uint32_t height)      { m_height = height;   }
	void setCreated(uint32_t created)    { m_created = created; }
	void setComment(const char *comment) {
		Utils::copyString(comment, m_comment);
	}
	//@}

	/**
	 * @name Getters
	 */
	//@{
	uint32_t          getFormat()  const { return m_format;  }
	uint32_t          getWidth()   const { return m_width;   }
	uint32_t          getHeight()  const { return m_height;  }
	std::string       getComment() const { return m_comment; }
	uint32_t          getCreated() const { return m_created; }
	//@}

private:
	friend class MetaData;
	~ImageMetaData() {
		if (m_comment) { free(m_comment); }
	}
	ImageMetaData(const ImageMetaData&);            //!< Copying forbidden
	ImageMetaData& operator=(const ImageMetaData&); //!< Assign forbidden

	uint32_t m_format;                          //!< Format, e.g png/jpg etc
	uint32_t m_width;                           //!< Image width
	uint32_t m_height;                          //!< Image height
	char* m_comment;                            //!< Image comment
	uint32_t m_created;                         //!< Image creation date

	//! Output operator for streams
	friend std::ostream& operator<<(
		std::ostream &o, const ImageMetaData &imd
	);
	//! In testsuite
	friend void test_imagedata();
};

//! Event emitted by MetaData object
enum MDEvent {
	MD_ADDED_FILENAME = 1,  //!< A file name has been added
	MD_ADDED_HASHSET,       //!< A Hashset has been added
	MD_ADDED_VIDEO,         //!< Video data has been added
	MD_ADDED_AUDIO,         //!< Audio data has been added
	MD_ADDED_ARCHIVE,       //!< Archive data has been added
	MD_ADDED_IMAGE,         //!< Image data has been added
	MD_ADDED_CUSTOMDATA,    //!< Custom data was added
	MD_ADDED_COMMENT,       //!< Comment was added
	MD_SIZE_CHANGED,        //!< File size has been changed
	MD_NAME_CHANGED         //!< File name has been changed
};

namespace CGComm {
	//! MetaData opcodes
	enum MetaDataOpCodes {
		OP_METADATA       = 0x90, //!< MetaData
		OP_MD_FILENAME    = 0x91, //!< <string> File Name
		OP_MD_FILESIZE    = 0x92, //!< <uint64> size
		OP_MD_MODDATE     = 0x93, //!< <uint32> modification date
		OP_MD_FILETYPE    = 0x94, //!< <uint32> file type
		OP_MD_TYPEGUESSED = 0x95, //!< <bool> if type has been guessed
		OP_MD_CUSTOM      = 0x96, //!< <string> custom string data
		OP_MD_UPLOADED    = 0x97  //!< <uint64> amount uploaded
	};
}

class HashSetBase;

/**
 * MetaData container which can contain any number of any type of different
 * Metadata-like objects. By default none of the memory-heavy structures
 * are created - they are stored as smart pointers.
 *
 * This class also submits events when file names, hashests and so on are added
 * to it. Refer to MDEvent documentation on event types submitted from this
 * class, and EventTable documentation on how to handle events.
 *
 * \todo CustomData and FileNames containers should be sets instead of vectors
 */
class HNCORE_EXPORT MetaData {
public:
	DECLARE_EVENT_TABLE(MetaData*, int);
	typedef std::set<std::string>::const_iterator CommentIter;
	typedef std::set<std::string>::const_iterator CustomIter;
	typedef std::map<std::string, uint32_t>::const_iterator NameIter;
	//! Constructor
	MetaData(
		uint64_t fileSize, FileType type = FT_UNKNOWN,
		bool typeGuessed = true
	);

	/**
	 * Construct by reading data from input stream.
	 */
	MetaData(std::istream &ifs);

	//! Default constructor
	MetaData();

	/**
	 * Destructor is exposed to allow more generic usage of the object.
	 * While the main purpose of this object was to remain hidden inside
	 * MetaDb and owned only by MetaDb, practice has shown this object
	 * is useful outside the MetaDb context also, and thus the destructor
	 * has been exposed. This does open a possibility of abuse though by
	 * objects using MetaData pointers given out by MetaDb.
	 */
	~MetaData();

	/**
	 * @name Setters
	 */
	//@{
	void addVideoData(VideoMetaData *vmd);
	void addAudioData(AudioMetaData *amd);
	void setArchiveData(ArchiveMetaData *amd);
	void setImageData(ImageMetaData *imd);
	void setName(const std::string &name);
	void addFileName(const std::string &name);
	void delFileName(const std::string &name);
	void addHashSet(HashSetBase *hashset);
	void addCustomData(const std::string &data);
	void addComment(const std::string &comment);
	void setModDate(uint32_t date)    { m_modDate = date;        }
	void setFileType(FileType type)   { m_fileType = type;       }
	void setTypeGuessed(bool guessed) { m_typeGuessed = guessed; }
	void addUploaded(uint32_t amount) { m_uploaded += amount;    }

	/**
	 * \brief Adjust the size
	 *
	 * \pre m_size == 0 && m_md->getSize() == 0
	 * \throws std::runtime_error if precondition is violated
	 *
	 * @param newSize      New size to be set
	 *
	 * \remarks Since resizing causes all hashes to change, this function
	 * also clears any hashes already stored in this object.
	 */
	void setSize(uint64_t newSize);
	//@}

	/**
	 * @name Getters
	 */
	//@{
	std::string getName() const { return m_fileName; }
	uint32_t getVideoDataCount() const { return m_videoData.size(); }
	VideoMetaData *const getVideoData(uint32_t num) {
		return m_videoData.at(num);
	}
	uint32_t getAudioDataCount() const { return m_audioData.size(); }
	AudioMetaData *const getAudioData(uint32_t num) const {
		return m_audioData.at(num);
	}
	ArchiveMetaData *const getArchiveData() const { return m_archiveData; }
	ImageMetaData *const getImageData() const { return m_imageData; }

	/// \name Accesses the HashSet's in this object
	//@{
	uint32_t getHashSetCount() const { return m_hashSets.size(); }
	HashSetBase *const getHashSet(uint32_t num) const {
		return m_hashSets.at(num);
	}
	//@}

	/// \name Accesses the File Names in this object
	//@{
	uint32_t getFileNameCount() const { return m_fileNames.size();  }
	NameIter namesBegin()       const { return m_fileNames.begin(); }
	NameIter namesEnd()         const { return m_fileNames.end();   }
	//@}

	/**
	 * \name Access to comments set
	 */
	//!@{
	uint32_t    getCommentCount() const { return m_comments.size();  }
	CommentIter commentsBegin()   const { return m_comments.begin(); }
	CommentIter commentsEnd()     const { return m_comments.end();   }
	//!@}

	/// \name Access custom string-data fields in this object (links etc)
	//@{
	/**
	 * \name Access to custom data
	 */
	//!@{
	uint32_t   getCustomCount() const { return m_customData.size();  }
	CustomIter customBegin()    const { return m_customData.begin(); }
	CustomIter customEnd()      const { return m_customData.end();   }
	//@}

	uint64_t getSize()        const { return m_fileSize;    }
	uint32_t getModDate()     const { return m_modDate;     }
	FileType getFileType()    const { return m_fileType;    }
	bool     getTypeGuessed() const { return m_typeGuessed; }
	uint64_t getUploaded()    const { return m_uploaded;    }
	//@}

	/**
	 * Merges names, comments and custom fields from other metadata to
	 * this metadata.
	 *
	 * \note All existing names, comments and custom fields in this object
	 *       are destroyed.
	 */
	void mergeCustom(MetaData *other);
private:
	MetaData(const MetaData &md);             //!< Copy construct forbidden
	MetaData& operator=(const MetaData&);     //!< Assignment forbidden

	//! Video data streams
	std::vector<VideoMetaData*> m_videoData;
	typedef std::vector<VideoMetaData*>::const_iterator CVMDIter;

	//! Audio data streams
	std::vector<AudioMetaData*> m_audioData;
	typedef std::vector<AudioMetaData*>::const_iterator CAMDIter;

	//! Archive data
	ArchiveMetaData *m_archiveData;

	//! Image data
	ImageMetaData *m_imageData;

	//! Hash sets
	std::vector<HashSetBase*> m_hashSets;
	typedef std::vector<HashSetBase*>::const_iterator CHIter;

	//! Active file name
	std::string m_fileName;

	/**
	 * All known file names, along with repeat-count (per-session).
	 * This data is NOT stored across sessions.
	 */
	std::map<std::string, uint32_t> m_fileNames;

	/**
	 * Custom data that modules can use for module-specific data.
	 * This is stored across sessions.
	 */
	std::set<std::string> m_customData;

	/**
	 * Comments about this file; this is not stored across sessions.
	 */
	std::set<std::string> m_comments;

	uint64_t m_fileSize;                  //!< File size
	uint32_t m_modDate;                   //!< File modification date
	FileType m_fileType;                  //!< File type
	bool     m_typeGuessed;               //!< If type was guessed
	uint64_t m_uploaded;                  //!< Amount uploaded

	//! Output operator for streams
	friend std::ostream& operator<<(std::ostream &o, const MetaData &md);
};

#endif
