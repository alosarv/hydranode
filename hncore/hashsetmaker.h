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
 * \file hashsetmaker.h
 * Interface for various HashSet generators used in P2P networks
 *
 * These classes are used internally by Files Identification Subsystem and
 * shouldn't be needed by plugins.
 */

#ifndef __HASHSETMAKER_H__
#define __HASHSETMAKER_H__

#include <hnbase/hash.h>
#include <hnbase/osdep.h>
#include <hnbase/md4transform.h>
#include <hnbase/md5transform.h>
#include <hnbase/sha1transform.h>

             /* --------------------------------------------------- */
             /* Second level of FIS - HashSets used in P2P Networks */
             /* --------------------------------------------------- */

/**
 * Abstract base for HashSet maker
 */
class HNCORE_EXPORT HashSetMaker {
public:
	HashSetMaker();
	virtual ~HashSetMaker();
	virtual void sumUp(const char *data, uint32_t length) = 0;
	virtual HashSetBase* getHashSet() = 0;
private:
	HashSetMaker(const HashSetMaker&);
	HashSetMaker& operator=(const HashSetMaker&);
};

// Implementation of Concrete HashSet Makers
// -----------------------------------------

/**
 * Generates ED2KHashSet. Uses MD4Hasher internally.
 */
class HNCORE_EXPORT ED2KHashMaker : public HashSetMaker {
public:
	//! Constructor
	ED2KHashMaker();

	//! Destructor
	virtual ~ED2KHashMaker();

	/**
	 * Continue ED2KHashset generation
	 *
	 * @param data      Data to be hashed
	 * @param length    Length of passed data
	 */
	virtual void sumUp(const char *data, uint32_t length);

	/**
	 * @return      Resulting hashset, downcasted to abstract base class
	 *
	 * @return      The resulting hashset
	 */
	virtual HashSetBase* getHashSet();
private:
	//! If HashSet generation has been completed.
	bool m_completed;

	//! Md4 transformer pointer used for checksumming
	Md4Transform *m_transformer;

	//! Temporary counter used to detect chunk size hits
	uint32_t m_dataCount;

	//! Gets filled with part hashes
	std::vector< Hash<MD4Hash> > m_chunks;

	//! Makes life simpler
	typedef std::vector< Hash<MD4Hash> >::iterator Iter;

	//! Receives final file hash
	Hash<MD4Hash> m_fileHash;
};

/**
 * Generates BTHashSet, with varying chunksize. Uses Sha1Transformer internally.
 * BTHashSet is a tricky one - it doesn't have a file hash at all, to the
 * best of my knowledge. And the chunk size varies. As such, we cannot generate
 * BTHashSet by default at all - only on specific request.
 */
class HNCORE_EXPORT BTHashMaker : public HashSetMaker {
public:
	/**
	 * Constructor
	 *
	 * @param chunkSize     Size of one piece/chunk/part
	 */
	BTHashMaker(uint32_t chunkSize);

	//! Destructor
	virtual ~BTHashMaker();

	/**
	 * Continue hash calculation.
	 *
	 * @param data      Data to be hashed. May not be 0.
	 * @param length    Length of data. May not be 0.
	 */
	virtual void sumUp(const char *data, uint32_t length);

	/**
	 * @return       Resulting hashset, downcasted to abstract base class
	 *
	 * @return       Pointer to newly allocated HashSet object
	 */
	virtual HashSetBase* getHashSet();
private:
	BTHashMaker();      //!< Default constructor protected - need chunkSize

	//! If hashset generation has been completed
	bool m_completed;
	//! Transformer used for calculation
	Sha1Transform *m_transformer;
	//! Receives part hashes
	std::vector< Hash<SHA1Hash> > m_partHashes;
	//! Makes life simpler
	typedef std::vector< Hash<SHA1Hash> >::iterator Iter;
	//! Size of one chunk
	const uint32_t m_chunkSize;
};

/**
 * Generates MD5 file hash, no part hashes.
 */
class HNCORE_EXPORT MD5HashMaker : public HashSetMaker {
public:
	//! Constructor
	MD5HashMaker();
	//! Destructor
	virtual ~MD5HashMaker();
	//! Add more data for transforming
	virtual void sumUp(const char *data, uint32_t length);
	// Retrieve results.
	virtual HashSetBase* getHashSet();
private:
	bool m_completed;
	Md5Transform m_transformer;
};

/**
 * Generates SHA-1 file hash, no part hashes.
 */
class HNCORE_EXPORT SHA1HashMaker : public HashSetMaker {
public:
	//! Constructor
	SHA1HashMaker();
	//! Destructor
	virtual ~SHA1HashMaker();
	//! Add more data
	virtual void sumUp(const char *data, uint32_t length);
	//! Retrieve results
	virtual HashSetBase* getHashSet();
private:
	bool m_completed;
	Sha1Transform m_transformer;
};

/**
 * Generates MD4 file hash, no part hashes
 */
class HNCORE_EXPORT MD4HashMaker : public HashSetMaker {
public:
	MD4HashMaker();
	virtual ~MD4HashMaker();
	virtual void sumUp(const char *data, uint32_t length);
	virtual HashSetBase* getHashSet();
private:
	bool m_completed;
	Md4Transform m_transformer;
};

#endif
