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
 * \file ed2kfile.h Interface for ED2KFile class
 */

#ifndef __ED2K_ED2KFILE_H__
#define __ED2K_ED2KFILE_H__

#include <hnbase/hash.h>               // for Hash<ED2KHash>
#include <hncore/metadata.h>           // for FileType
#include <hncore/fwd.h>

namespace Donkey {

/**
 * ED2KFile class represents a file as is known to ED2K network. ED2K network
 * puts some restrictions on the files it can support - namely, the file must
 * have a valid ED2KHash present. Additional fields ED2KFile is required to
 * include are size, type, and completeness.
 *
 * Note that file size is 32-bit value in eDonkey2000 network - max ED2KFile
 * size is 2^32 bytes. While Hydranode supports up to 2^64 byte files
 * internally, eDonkey2000 network doesn't - thus sharing files with size > 4gb
 * is not possible on ed2k.
 *
 * File type is used in eDonkey2000 network as strings (refer to opcodes.h for
 * the actual strings definitions), and not all file types that Hydranode
 * supports are also supported by eDonkey2000 network. In any case, we provide
 * convenience accessors and constructors for using Hydranode FileType enum
 * values here, and handle the details internally as needed.
 */
class ED2KFile {
public:
	/**
	 * Construct using hash, name and type.
	 *
	 * @param h            ED2KHash of the file
	 * @param name         File name (excluding path)
	 * @param type         Type of file, as specified in hn/metadata.h
	 * @param flags        Bitfield of Flags
	 */
	ED2KFile(
		Hash<ED2KHash> h, const std::string &name, uint32_t size,
		FileType type, uint8_t flags
	);

	/**
	 * Construct using hash, name and type.
	 *
	 * @param h            ED2KHash of the file
	 * @param name         File name (excluding path)
	 * @param type         Type of file, as used in ED2K network (strings)
	 * @param flags        Bitfield of Flags
	 */
	ED2KFile(
		Hash<ED2KHash> h, const std::string &name, uint32_t size,
		const std::string &type, uint8_t flags
	);

	/**
	 * Construct an ED2KFile object which has bare minimum of information,
	 * plus client id and client port. This omit type and flags data,
	 * however adds clientid/port, which comes in handy when handling
	 * search results for example. The type and flags may be added later
	 * using accessor methods.
	 *
	 * @param h         ED2KHash of the file
	 * @param name      File name
	 * @param size      File size
	 * @param id        ClientId of the client sharing this file
	 * @param port      ClientPort of the client sharing this file
	 */
	ED2KFile(
		Hash<ED2KHash> h, const std::string &name, uint32_t size,
		uint32_t id, uint16_t port
	);

	//! @name Accessors
	//@{
	Hash<ED2KHash> getHash() const { return m_hash;        }
	std::string    getName() const { return m_name;        }
	uint32_t       getSize() const { return m_size;        }
	FileType     getHNType() const { return m_hnType;      }
	std::string getStrType() const { return m_ed2kType;    }
	uint32_t         getId() const { return m_id;          }
	uint16_t       getPort() const { return m_port;        }
	//@}

	//! Output operator to streams for usage in ed2k protocol
	friend std::ostream& operator<<(std::ostream &o, const ED2KFile &f);

	//! Flags usable at ED2KFile constructor arguments
	enum Flags {
		FL_COMPLETE        = 0x01, //!< If this file is complete
		FL_USECOMPLETEINFO = 0x02  //!< Whether to send complete info
	};

	//! File ratings
	enum Rating {
		FR_NORATING = 0x00,
		FR_INVALID,
		FR_POOR,
		FR_GOOD,
		FR_FAIR,
		FR_EXCELLENT
	};

	//! Converts rating value to rating string.
	static std::string ratingToString(const Rating &r);

	//! @name Static utility functions for file type conversions
	//@{
	//! Converts Hydranode filetype into ED2K filetype
	static std::string HNType2ED2KType(FileType type);
	//! Converts ED2K file type to Hydranode file type
	static FileType    ED2KType2HNType(const std::string &type);
	//@}
private:
	const Hash<ED2KHash> m_hash;        //!< File hash
	const std::string    m_name;        //!< File name
	uint32_t             m_size;        //!< File size
	FileType             m_hnType;      //!< File type (HN-compatible)
	std::string          m_ed2kType;    //!< File type (ed2k-compatible)
	uint8_t              m_flags;       //!< Various useful flags
	uint32_t             m_id;          //!< ClientId sharing this file
	uint16_t             m_port;        //!< ClientPort of the client

	/**
	 * Convenience method for constructing complex ED2KFile object from
	 * pre-given data.
	 *
	 * @param sf               SharedFile to refer to
	 * @param md               MetaData corresponding to the SharedFile
	 * @param hs               ED2KHashSet corresponding to this file
	 * @param useCompletEInfo  Whether to include "complete" information
	 * @return                 Newly allocated ED2KFile object
	 */
	friend boost::shared_ptr<ED2KFile> makeED2KFile(
		SharedFile *sf, MetaData *md, HashSetBase *hs,
		bool useCompleteInfo
	);
};

} // end namespace Donkey

#endif
