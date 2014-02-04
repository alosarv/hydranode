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
 * \file hash.h Interface for Hash<>, HashSet<> and their base classes.
 */

#ifndef __HASH_H__
#define __HASH_H__

#include <hnbase/utils.h>
#include <hnbase/log.h>
#include <hnbase/osdep.h>
#include <stdexcept>
#include <iosfwd>
#include <string>
#include <vector>

/**
 * @page hashes Hash System Overview
 *
 * A Hash represents what is generally known as a "checksum". There are roughly
 * 10 widely-used checksumming algorithms widely used, and we will sooner or
 * later need to implement most of them in Hydranode core. As such, we need
 * some way of storing all kinds of checksums, of arbitary lengths, while
 * keeping type-safety. Additionally, we want to store the checksums in
 * lists of checksums, for example for files checksum sets per ranges.
 *
 * In order to accomplish that, the following has been implemented:
 *
 * - HashBase is abstract and provides pure virtual accessors for various Hash
 *   internals. The need for this class is by HashSetBase class, which needs to
 *   return Hash objects without knowing their exact types.
 * - Hash<> class is a concrete implementation of a checksum wrapper. It is
 *   parametered with specific Hash type (the pattern is generally known as
 *   template Policy, however we do not exploit the full potential of it here).
 *   Basically what we attempted to achieve here was to have compiler generate
 *   specific Hash classes for each of the implemented hashes, without us
 *   having to write each of them by hand (or using runtime checking, which
 *   derivation could give us). With this design, we get a concrete special
 *   class for each and every different Hash we create, thus attempts to mix
 *   different Hash objects in containers et al will result in compile-time
 *   errors.
 * - HashSetBase is abstract and provides pure virtual accessors for various
 *   HashSet-related functions. The reason for this class's existance is that
 *   we will later need to store different hashsets in same container, which
 *   breaks compile-time typesafety since HashSet<> is template. This is also
 *   the reason why we needed HashBase classe - since this class doesn't
 *   know about the specific Hashes it contains (its implemented by derivation),
 *   it can only return HashBase objects.
 * - HashSet<> template class is container for specific Hash objects. HashSet
 *   object may contain a list of chunk hashes, and master-hash (filehash).
 *   The hash types of those two may differ, however attempts to add
 *   different Hash types into container will result in compile-time errors.
 *
 */

class HashBase;
class HashSetBase;

namespace CGComm {
	//! Hash types and the relevant op codes
	enum HashTypeId {
		OP_HASH         = 0xa0,      //!< Hash object
		OP_HT_ADLER     = 0xa1,      //!< length =  4
		OP_HT_CRC32     = 0xa2,      //!< length =  4
		OP_HT_ED2K      = 0xa3,      //!< length = 16
		OP_HT_MD4       = 0xa4,      //!< length = 16
		OP_HT_MD5       = 0xa5,      //!< length = 16
		OP_HT_PANAMA    = 0xa6,      //!< length = 32
		OP_HT_RIPEMD160 = 0xa7,      //!< length = 20
		OP_HT_SHA1      = 0xa8,      //!< length = 20
		OP_HT_SHA256    = 0xa9,      //!< length = 32
		OP_HT_SHA384    = 0xaa,      //!< length = 48
		OP_HT_SHA512    = 0xab,      //!< length = 64
		OP_HT_TIGER     = 0xac,      //!< length = 24
		OP_HT_UUHASH    = 0xad,      //!< length = ??
		OP_HT_UNKNOWN   = 0xff       //!< unknown/userdefined/invalid
	};
	//! Load a hash from stream.
	HNBASE_EXPORT HashBase* loadHash(std::istream &i);
	//! Load HashSet from stream
	HNBASE_EXPORT HashSetBase* loadHashSet(std::istream &i);
}

//! Abstract base for hash
class HNBASE_EXPORT HashBase {
public:
	HashBase();
	virtual ~HashBase();
	virtual uint16_t size() const = 0;
	virtual boost::shared_array<char> getData() const = 0;
	virtual std::string getType() const = 0;
	virtual CGComm::HashTypeId getTypeId() const = 0;
	bool isEmpty() const { return getData() ? false : true; }

	std::string toString() const {
		return std::string(getData().get(), size());
	}

	virtual std::string decode() const {
		if (isEmpty()) {
			return std::string();
		}
		return Utils::decode(getData().get(), size());
	}

	//! Output operator for streams
	friend HNBASE_EXPORT std::ostream& operator<<(
		std::ostream &o, const HashBase &h
	);

	//! Comparison operator
	bool operator==(const HashBase &h) const {
		if (h.getTypeId() != getTypeId()) {
			return false;
		}
		if (!getData() && !h.getData()) { return true; }
		if (
			(getData() && !h.getData()) ||
			(!getData() && h.getData())
		) {
			return false;
		}
		return !memcmp(h.getData().get(), getData().get(), size());
	}

	bool operator!=(const HashBase &h) const {
		return !(*this == h);
	}

	friend bool operator<(const HashBase &x, const HashBase &y) {
		if (!x.getData() && !y.getData()) {
			return true;
		}
		if (
			(x.getData() && !y.getData())
			|| (!x.getData() && y.getData())
			|| x.size() != y.size()
		) {
			return false;
		}
		return memcmp(
			x.getData().get(), y.getData().get(), x.size()
		) < 0;
	}
	//! Comparison operator to bool
	operator bool() const { return getData(); }
};

//! Concrete implementation for Hash
template<typename HashType>
class Hash : public HashBase {
public:
	//! Default constructor
	Hash() {}
	//! Construct and read from stream
	Hash(std::istream &i) {
		m_data.reset(new char[HashType::size()]);
		i.read(m_data.get(), HashType::size());
	}

	//! Construct from character array
	Hash(const char *data) {
		m_data.reset(new char[HashType::size()]);
		memcpy(m_data.get(), data, HashType::size());
	}
	Hash(const unsigned char *data) {
		m_data.reset(new char[HashType::size()]);
		memcpy(m_data.get(), data, HashType::size());
	}
	Hash(const boost::shared_array<char> &data) : m_data(data) {}

	//! Construct from character array and size - asserts if length !=
	//! HashType::size()
	Hash(const char *data, uint16_t length) {
		assert(length == HashType::size());
		m_data.reset(new char[HashType::size()]);
		memcpy(m_data.get(), data, HashType::size());
	}
	Hash(const unsigned char *data, uint16_t length) {
		assert(length == HashType::size());
		m_data.reset(new char[HashType::size()]);
		memcpy(m_data.get(), data, HashType::size());
	}
	Hash(const std::string &data) {
		assert(data.size() == HashType::size());
		m_data.reset(new char[HashType::size()]);
		memcpy(m_data.get(), data.data(), HashType::size());
	}
	Hash(const boost::shared_array<char> *data) : m_data(data) {}

	//! Destructor
	~Hash() {}

	// compiler-generated copy-constructor and assignment operators are ok

	//! Accessors
	//@{
	uint16_t size() const { return HashType::size(); }
	boost::shared_array<char> getData() const { return m_data; }
	std::string getType() const { return HashType::getType(); }
	CGComm::HashTypeId getTypeId() const { return HashType::getTypeId(); }
	std::string toString() const {
		return std::string(m_data.get(), size());
	}
	//@}

	/**
	 * Clear the contents of this hash
	 */
	void clear() { m_data.reset(); }

	//! @name Operations
	//@{
	bool operator==(const Hash &h) const {
		if (!m_data && !h.m_data) { return true; }
		if ((m_data && !h.m_data) || (!m_data && h.m_data)) {
			return false;
		}
		return !memcmp(h.m_data.get(), m_data.get(), HashType::size());
	}
	bool operator!=(const Hash &h) const {
		if (!m_data && !h.m_data) { return false; }
		if ((m_data && !h.m_data) || (!m_data && h.m_data)) {
			return true;
		}
		return memcmp(h.m_data.get(), m_data.get(), HashType::size());
	}

	//! Comparison operator to bool
	operator bool() const { return m_data; }

	friend bool operator<(const Hash &x, const Hash &y) {
		if (!x.m_data && !y.m_data) {
			return true;
		}
		if ((!x.m_data && y.m_data) || (x.m_data && !y.m_data)) {
			return false;
		}
		return memcmp(
			x.m_data.get(), y.m_data.get(), HashType::size()
		) < 0;
	}
	//@}
private:
	boost::shared_array<char> m_data;        //!< Internal data storage
};

//! MD4Hash specification
class HNBASE_EXPORT MD4Hash {
public:
	static uint16_t size()                { return 16;                }
	static std::string getType()          { return "MD4Hash";         }
	static CGComm::HashTypeId getTypeId() { return CGComm::OP_HT_MD4; }
};
//! MD5Hash specification
class HNBASE_EXPORT MD5Hash {
public:
	static uint16_t size()                { return 16;                }
	static std::string getType()          { return "MD5Hash";         }
	static CGComm::HashTypeId getTypeId() { return CGComm::OP_HT_MD5; }
};
//! ED2KHash specification
class HNBASE_EXPORT ED2KHash {
public:
	static uint16_t size()                { return 16;                 }
	static std::string getType()          { return "ED2KHash";         }
	static CGComm::HashTypeId getTypeId() { return CGComm::OP_HT_ED2K; }
};
//! SHA1Hash specification
class HNBASE_EXPORT SHA1Hash {
public:
	static uint16_t size()                { return 20;                 }
	static std::string getType()          { return "SHA1Hash";         }
	static CGComm::HashTypeId getTypeId() { return CGComm::OP_HT_SHA1; }
};

namespace CGComm {
	enum HashSetTypeIds {
		OP_HASHSET     = 0xc0,         //!< Hashset
		OP_HS_FILEHASH = 0xc1,         //!< <hash>filehash
		OP_HS_PARTHASH = 0xc2,         //!< <hash>chunkhash
		OP_HS_PARTSIZE = 0xc3          //!< <uint32_t>chunksize
	};
}

/**
 * Abstract base class representing a Hash Set. Provides pure virtual functions
 * which derived classes must override to create concrete HashSet types.
 */
class HNBASE_EXPORT HashSetBase {
public:
	HashSetBase();
	virtual ~HashSetBase();

	/**
	 * @name Pure virtual accessors
	 */
	//@{
	virtual const HashBase&    getFileHash()              const = 0;
	virtual uint32_t           getChunkCnt()              const = 0;
	virtual const HashBase&    getChunkHash(uint32_t num) const = 0;
	virtual uint32_t           getChunkSize()             const = 0;
	virtual std::string        getFileHashType()          const = 0;
	virtual CGComm::HashTypeId getFileHashTypeId()        const = 0;
	virtual std::string        getChunkHashType()         const = 0;
	virtual CGComm::HashTypeId getChunkHashTypeId()       const = 0;
	//@}

	//! Clears all chunk hashes
	virtual void clearChunkHashes() = 0;

	// Compares two hashets. Returns true if they are equal, false otherwise
	bool compare(const HashSetBase &ref) const;

	//! Output operator for streams
	friend HNBASE_EXPORT std::ostream& operator<<(
		std::ostream &o, const HashSetBase &h
	);

	//! Inequality operator
	bool operator!=(const HashSetBase &ref) const {
		return !compare(ref);
	}

	//! Equality operator
	bool operator==(const HashSetBase &ref) const {
		return compare(ref);
	}
	const HashBase& operator[](uint32_t c) const { return getChunkHash(c); }
};

/**
 * Implements concrete HashSet class.
 *
 * @param HashType      Type of hashes to store in
 * @param ChunkSize     Optionally set chunk size
 * @param FileHashType  Optionally set file hash type
 */
template<class HashType, class FileHashType = HashType, uint32_t ChunkSize = 0>
class HashSet : public HashSetBase {
public:
	//! Default constructor
	HashSet() : m_chunkSize(ChunkSize) {}

	//! Initialize with custom chunk size
	HashSet(uint32_t chunkSize) : m_chunkSize(chunkSize) {}

	//! Construct with existing file hash
	HashSet(Hash<FileHashType> h) : m_fileHash(h), m_chunkSize(ChunkSize) {}

	//! Construct with existing filehash and chunkhashes
	HashSet(Hash<FileHashType> h, const std::vector<Hash<HashType> > &c)
	: m_fileHash(h), m_hashSet(c), m_chunkSize(ChunkSize) {}

	//! Construct and read from stream
	HashSet(std::istream &i) : m_chunkSize(ChunkSize) {
		uint16_t tagcount = Utils::getVal<uint16_t>(i);
		while (tagcount--) {
			uint8_t opcode = Utils::getVal<uint8_t>(i);
			uint16_t len = Utils::getVal<uint16_t>(i);
			switch (opcode) {
				case CGComm::OP_HS_FILEHASH: {
					if (!m_fileHash.isEmpty()) {
						logError(boost::format(
							"Multiple filehash "
							"tags in HashSet!"
						));
						break;
					}
					uint8_t typ = Utils::getVal<uint8_t>(i);
					if (typ != CGComm::OP_HASH) {
						logError(boost::format(
							"Unexpected symbol %s "
							"found at offset %s "
							"while loading HashSet."
						) % Utils::hexDump(typ)
						% Utils::hexDump(-1+i.tellg()));
						break;
					}
					// length - ignored
					(void)Utils::getVal<uint16_t>(i);
					uint8_t id = Utils::getVal<uint8_t>(i);
					if (id != FileHashType::getTypeId()) {
						logError(boost::format(
							"Incorrect FileHashType"
							" %s found at offset %s"
							" in stream.")
							% Utils::hexDump(id)
							% Utils::hexDump(
								-1+i.tellg()
							)
						);
						break;
					}
					m_fileHash = Hash<FileHashType>(i);
					break;
				}
				case CGComm::OP_HS_PARTHASH: {
					uint8_t typ = Utils::getVal<uint8_t>(i);
					if (typ != CGComm::OP_HASH) {
						logError(boost::format(
							"Unexpected symbol %s "
							"found at offset %s "
							"while loading HashSet."
						) % Utils::hexDump(typ)
						% Utils::hexDump(-1+i.tellg()));
						break;
					}
					// length - ignored
					(void)Utils::getVal<uint16_t>(i);
					uint8_t id = Utils::getVal<uint8_t>(i);
					if (id != HashType::getTypeId()) {
						logError(boost::format(
							"Incorrect "
							"ChunkHashType"
							" %s found in stream.")
							% Utils::hexDump(id)
						);
						break;
					}
					m_hashSet.push_back(Hash<HashType>(i));
					break;
				}
				case CGComm::OP_HS_PARTSIZE:
					m_chunkSize =Utils::getVal<uint32_t>(i);
					break;
				default:
					logError(boost::format(
						"Unexpected tag %s found at "
						"offset %s while parsing "
						"HashSet."
					) % Utils::hexDump(opcode)
					% Utils::hexDump(-1+i.tellg()));
					i.seekg(len, std::ios::cur);
					break;
			}
		}
	}

	//! Destructor
	~HashSet() {}

	/**
	 * @name Getters
	 */
	//@{

	/**
	 * Get the file hash.
	 *
	 * @return        File Hash - may be empty.
	 */
	const HashBase& getFileHash() const {
		return m_fileHash;
	}

	/**
	 * Number of chunk hashes
	 *
	 * @return       Number of chunk hashes
	 */
	uint32_t getChunkCnt() const { return m_hashSet.size(); }

	/**
	 * Retrieve Nth chunk hash
	 *
	 * @param num     Which chunk hash to retrieve.
	 * @return        The requested chunk hash.
	 *
	 * \throws std::runtime_error if num > m_hashSet.size()
	 */
	const HashBase& getChunkHash(uint32_t num) const {
		return m_hashSet.at(num);
	}

	/**
	 * Retrieve the size of one chunk
	 *
	 * @return      Chunk size
	 */
	uint32_t getChunkSize() const { return m_chunkSize; }

	/**
	 * RTTI - return string identifying the file hash type
	 *
	 * @return       String representation of file hash type
	 */
	std::string getFileHashType() const { return FileHashType::getType(); }

	/**
	 * RTTI - return TypeId of file hash
	 *
	 * @return       Enumeration value indicating file hash type
	 */
	CGComm::HashTypeId getFileHashTypeId() const {
		return FileHashType::getTypeId();
	}

	/**
	 * RTTI - return string identifyin the chunk hash type
	 *
	 * @return       String representation of chunk hash type
	 */
	std::string getChunkHashType() const { return HashType::getType(); }

	/**
	 * RTTI - return TypeId of chunk hash
	 *
	 * @return       Enumeration value indicating chunk hash type
	 */
	CGComm::HashTypeId getChunkHashTypeId() const {
		return HashType::getTypeId();
	}

	//! Returns number of chunk-hashes in the set
	uint32_t size() const { return m_hashSet.size(); }
	//@}       // End Getters


	/**
	 * @name Setters
	 */
	//@{

	/**
	 * Add a chunk hash
	 *
	 * @param h       Chunk hash to add
	 */
	void addChunkHash(Hash<HashType> h) {
		m_hashSet.push_back(h);
	}

	/**
	 * Set the file hash, overwriting existing (if any)
	 *
	 * @param h      New file hash
	 */
	void setFileHash(Hash<FileHashType> h) {
		m_fileHash = h;
	}

	//! Clears all chunk hashes
	void clearChunkHashes() {
		m_hashSet.clear();
	}
	//@}       // End Setters

private:
	//! Contains file hash. May be empty.
	Hash<FileHashType> m_fileHash;

	//! Contains all chunk/chunk/piece hashes. May be empty.
	std::vector< Hash<HashType> > m_hashSet;

	//! Size of one chunk/chunk/piece
	uint32_t m_chunkSize;
};

/**
 * Commonly used data types
 */
//@{
enum Hash_Constants {
	/**
	 * Size of a single file chunk as used in ED2K network. One chunk-hash
	 * corresponds to each ED2K_PARTSIZE amount of file data. Only full
	 * chunks of this size may be shared.
	 */
	ED2K_PARTSIZE = 9728000
};
typedef HashSet<MD4Hash, ED2KHash, ED2K_PARTSIZE> ED2KHashSet;
//@}

#endif
