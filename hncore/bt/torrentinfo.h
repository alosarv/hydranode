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
 * \file torrentinfo.h       Interface for TorrentInfo class
 */

#ifndef __BT_TORRENTINFO_H__
#define __BT_TORRENTINFO_H__

#include <hnbase/osdep.h>
#include <hnbase/hash.h>
#include <hnbase/ipv4addr.h>

namespace Bt {

/**
 * TorrentInfo provides a higher-level frontend to reading and writing .torrent
 * files.
 */
class TorrentInfo {
public:
	//! Announce urls iterator
	typedef std::vector<std::string>::const_iterator AIter;

	/**
	 * TorrentFile represents a single file in a multi-file torrent object
	 */
	class TorrentFile {
	public:
		TorrentFile();
		TorrentFile(const std::string &name, uint64_t length);

		uint64_t       getSize()     const { return m_length;   }
		std::string    getName()     const { return m_name;     }
		Hash<SHA1Hash> getSha1Hash() const { return m_sha1Hash; }
		Hash<ED2KHash> getEd2kHash() const { return m_ed2kHash; }
		Hash<MD4Hash>  getMd4Hash()  const { return m_md4Hash;  }
		Hash<MD5Hash>  getMd5Hash()  const { return m_md5Hash;  }

		void clear();
		friend bool operator==(
			const TorrentFile &x, const TorrentFile &y
		);
	private:
		friend class TorrentInfo;

		uint64_t       m_length;      //!< The size of the file
		std::string    m_name;        //!< The name of the file
		Hash<SHA1Hash> m_sha1Hash;    //!< Optional SHA-1 hash
		Hash<ED2KHash> m_ed2kHash;    //!< Optional ED2K hash
		Hash<MD4Hash>  m_md4Hash;     //!< Optional MD4 hash
		Hash<MD5Hash>  m_md5Hash;     //!< Optional MD5 hash

		friend std::ostream& operator<<(
			std::ostream &o, const TorrentFile &f
		);
	};

	/**
	 * Default constructor
	 */
	TorrentInfo();

	/**
	 * Construct TorrentInfo by reading and parsing the passed data, and
	 * filling the members with the parsed / processed data.
	 *
	 * @param data       The contents of a .torrent file
	 */
	TorrentInfo(const std::string &data);

	/**
	 * Read data from input stream
	 *
	 * @param i          Stream to read from
	 */
	TorrentInfo(std::istream &i);

	/**
	 * @name Generic accessors
	 */
	//@{
	std::string       getAnnounceUrl()     const { return m_announceUrl;   }
	int64_t           getCreationDate()    const { return m_creationDate;  }
	uint64_t          getSize()            const { return m_length;        }
	std::string       getName()            const { return m_name;          }
	uint32_t          getChunkSize()       const { return m_chunkSize;     }
	HashSet<SHA1Hash> getHashes()          const { return m_hashes;        }
	uint32_t          getChunkCnt()        const { return m_hashes.size(); }
	std::string       getComment()         const { return m_comment;       }
	std::string       getCreatedBy()       const { return m_createdBy;     }
	Hash<SHA1Hash>    getInfoHash()        const { return m_infoHash;      }
	std::vector<TorrentFile> getFiles()    const { return m_files;         }
	std::vector<IPV4Address> getNodes()    const { return m_nodes;         }
	AIter announceBegin()           const { return m_announceList.begin(); }
	AIter announceEnd()             const { return m_announceList.end();   }
	//@}

	/**
	 * @name Generic setters
	 */
	//@{
	void setAnnounceUrl(const std::string &n) { m_announceUrl = n;    }
	void setCreationDate(int64_t n)           { m_creationDate = n;   }
	void setLength(uint64_t n)                { m_length = n;         }
	void setName(const std::string &n)        { m_name = n;           }
	void setChunkSize(uint32_t n)             { m_chunkSize = n;      }
	void setHashes(const HashSet<SHA1Hash> &n){ m_hashes = n;         }
	void addFile(const TorrentFile &f)        { m_files.push_back(f); }
	void setComment(const std::string &n)     { m_comment = n;        }
	void setCreatedBy(const std::string &n)   { m_createdBy = n;      }
	//@}

	//! Prints the contents of the torrent info
	void print();

	//! Test two Torrent infos for equality
	friend bool operator==(const TorrentInfo &x, const TorrentInfo &y);
private:
	//! Performs actual parsing of data
	void load(const std::string &data);

	std::string              m_announceUrl;  //!< Announcement URL
	int64_t                  m_creationDate; //!< Creation date
	uint64_t                 m_length;       //!< Total size of all files
	std::string              m_name;         //!< Top-level name
	uint32_t                 m_chunkSize;    //!< Size of a single chunk
	HashSet<SHA1Hash>        m_hashes;       //!< All hashes for the torrent
	std::vector<TorrentFile> m_files;        //!< All files in the torrent
	std::string              m_comment;      //!< Optional comment
	std::string              m_createdBy;    //!< Optional creator string
	Hash<SHA1Hash>           m_infoHash;     //!< Hash of the info dict
	std::vector<std::string> m_announceList; //!< Additional announce URLS
	std::vector<IPV4Address> m_nodes;        //!< Node listing
};

} // end namespace Bt

#endif
