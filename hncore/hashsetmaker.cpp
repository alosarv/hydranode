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
 * \file hashsetmaker.cpp Implementation of HashSet generation classes
 */

#include <hncore/pch.h>

#include <hncore/hashsetmaker.h>

// HashSetMaker - abstract base - Implementaiton
// ---------------------------------------------
// Constructor
HashSetMaker::HashSetMaker() {
}

// Destructor
HashSetMaker::~HashSetMaker() {
}

// ED2KHashSetMaker Implementation
// --------------------------------
// Constructor
ED2KHashMaker::ED2KHashMaker()
: m_completed(false), m_transformer(new Md4Transform()), m_dataCount(0) {
}

// Destructor
ED2KHashMaker::~ED2KHashMaker() {
	if (!m_completed) {
		delete m_transformer;
	}
}

// Continue generating hash
void ED2KHashMaker::sumUp(const char *data, uint32_t length) {
	CHECK_THROW(data);                       // Data may not be null
	if (m_dataCount + length >= ED2K_PARTSIZE) {
		// About to complete a part - hash up to partsize
		m_transformer->sumUp(data, ED2K_PARTSIZE - m_dataCount);
		// Get the hash
		m_chunks.push_back(m_transformer->getHash());
		// Reset the transformer
		delete m_transformer;
		m_transformer = new Md4Transform();
		// Hash the remainder of this data chunk
		data   += ED2K_PARTSIZE - m_dataCount;
		length -= ED2K_PARTSIZE - m_dataCount;
		if (length) {
			m_transformer->sumUp(data, length);
		}
		m_dataCount = length;
	} else {
		m_transformer->sumUp(data, length);
		m_dataCount += length;
	}
}

// Retrieve results
HashSetBase* ED2KHashMaker::getHashSet() {
	CHECK_THROW(m_transformer);        // Transformer must be alive
	CHECK_THROW(!m_completed);         // Retrieving twice is an error

	// Finalize the ed2k hashset generation
	// 1. Retrieve the last part hash
	m_chunks.push_back(m_transformer->getHash());

	// 2. Generate Md4 over the part hashes. This is rather
	//    fast operation and can be done in-place here.
	//    Note - Only do this if we have more than one part hash.
	//    ED2K Protocol specifies that for files that are less than
	//    9728000 bytes, the only part hash is also the file hash, and
	//    is omitted (e.g. parthashcount == 0)
	delete m_transformer;
	if (m_chunks.size() > 1) {
		m_transformer = new Md4Transform();
		for (Iter i = m_chunks.begin(); i != m_chunks.end(); ++i) {
			m_transformer->sumUp((*i).getData().get(), (*i).size());
		}
		m_fileHash = m_transformer->getHash();
		delete m_transformer;
	} else {
		m_fileHash = m_chunks[0];
		m_chunks.pop_back();
	}

	// 3. Construct HashSet object, copy data there and return it.
	ED2KHashSet *hs = new ED2KHashSet();
	for (uint32_t i = 0; i < m_chunks.size(); ++i) {
		hs->addChunkHash(m_chunks[i]);
	}

	// Ok, we know that ED2KHash comes from MD4Hash, but the rest of the
	// world doesn't need to know that - copy it over manually to the
	// right type.
	hs->setFileHash(Hash<ED2KHash>(m_fileHash.getData()));
	m_completed = true;
	return hs;
}

// BTHashSetMaker Implementation
// -----------------------------
// Constructor
BTHashMaker::BTHashMaker(uint32_t chunkSize)
: m_completed(false), m_transformer(new Sha1Transform()),
m_chunkSize(chunkSize) {
}

// Destructor
BTHashMaker::~BTHashMaker() {
	if (!m_completed) {
		delete m_transformer;
	}
}

// Continue generating hash
void BTHashMaker::sumUp(const char *data, uint32_t length) {
	CHECK_THROW(!m_completed);                // Shouldn't have finished
	CHECK_THROW(data);                        // Shouldn't be null
	static uint32_t dataCount = 0;

	if (dataCount + length >= m_chunkSize) {
		// Sum until end of part
		m_transformer->sumUp(
			data, m_chunkSize - dataCount
		);
		data   += (m_chunkSize - dataCount);
		length -= (m_chunkSize - dataCount);

		m_partHashes.push_back(m_transformer->getHash());
		// Reset transformer
		delete m_transformer;
		m_transformer = new Sha1Transform();

		// Sum remainder of data with new transformer
		if (length) {
			m_transformer->sumUp(data, length);
		}

		// Reset the counter
		dataCount = length;
	} else {
		m_transformer->sumUp(data, length);
		dataCount += length;
	}
}

// Retrieve results
HashSetBase* BTHashMaker::getHashSet() {
	CHECK_THROW(m_transformer);            // Should be alive
	CHECK_THROW(!m_completed);             // Shouldn't be completed yet

	// Retrieve the last hash
	m_partHashes.push_back(m_transformer->getHash());
	delete m_transformer;

	// Copy data to new object and return it.
	HashSet<SHA1Hash> *hs = new HashSet<SHA1Hash>(m_chunkSize);
	for (
		Iter i = m_partHashes.begin();
		i != m_partHashes.end(); ++i
	) {
		hs->addChunkHash(*i);
	}

	m_completed = true;
	return hs;
}

// MD5HashMaker implementation
// ------------------------------
MD5HashMaker::MD5HashMaker() : m_completed(false) {}
MD5HashMaker::~MD5HashMaker() {}
void MD5HashMaker::sumUp(const char *data, uint32_t length) {
	m_transformer.sumUp(data, length);
}
HashSetBase* MD5HashMaker::getHashSet() {
	CHECK_THROW(!m_completed);

	HashSet<MD5Hash> *hs = new HashSet<MD5Hash>();
	hs->setFileHash(m_transformer.getHash());
	m_completed = true;
	return hs;
}

// SHA1HashMaker implementation
// ----------------------------
SHA1HashMaker::SHA1HashMaker() : m_completed(false) {}
SHA1HashMaker::~SHA1HashMaker() {}
void SHA1HashMaker::sumUp(const char *data, uint32_t length) {
	m_transformer.sumUp(data, length);
}
HashSetBase* SHA1HashMaker::getHashSet() {
	CHECK_THROW(!m_completed);
	HashSet<SHA1Hash> *hs = new HashSet<SHA1Hash>();
	hs->setFileHash(m_transformer.getHash());
	m_completed = true;
	return hs;
}

// MD4HashMaker implementation
// ---------------------------
MD4HashMaker::MD4HashMaker() : m_completed(false) {}
MD4HashMaker::~MD4HashMaker() {}
void MD4HashMaker::sumUp(const char *data, uint32_t length) {
	m_transformer.sumUp(data, length);
}
HashSetBase* MD4HashMaker::getHashSet() {
	CHECK_THROW(!m_completed);
	HashSet<MD4Hash> *hs = new HashSet<MD4Hash>();
	hs->setFileHash(m_transformer.getHash());
	m_completed = true;
	return hs;
}
