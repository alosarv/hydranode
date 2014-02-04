/*
 * Copyright (C) 1990-1992, RSA Data Security, Inc. All rights reserved.
 *
 * License to copy and use this software is granted provided that it
 * is identified as the "RSA Data Security, Inc. MD4 Message-Digest
 * Algorithm" in all material mentioning or referencing this software
 * or this function.
 *
 * License is also granted to make and use derivative works provided
 * that such works are identified as "derived from the RSA Data
 * Security, Inc. MD4 Message-Digest Algorithm" in all material
 * mentioning or referencing the derived work.
 *
 * RSA Data Security, Inc. makes no representations concerning either
 * the merchantability of this software or the suitability of this
 * software for any particular purpose. It is provided "as is"
 * without express or implied warranty of any kind.
 *
 * These notices must be retained in any copies of any part of this
 * documentation and/or software.
 */

/**
 * \file md4transform.h Interface for Md4Transform class.
 */

#ifndef __MD4TRANSFORM_H__
#define __MD4TRANSFORM_H__

#include <hnbase/hash.h>
#include <hnbase/osdep.h>

/**
 * Md4Transform class handles data md4 checksumming functionality.
 *
 * The entire implementation is hidden within this class's implementation
 * file's static functions, and only this interface is provided to users. To
 * use this class, instantiate Md4Transform object (either on stack or heap),
 * and pass data to overloaded SumUp functions. You can call SumUp function as
 * many times as you want, the data will be accomulated. When you are done
 * passing data, call GetHash() function ONCE to retrieve the hash. After
 * GetHash() has been called once, the instance of the class will become
 * useless and cannot be reused for new data. Never use same Md4Transform
 * instance to generate multiple checksums.
 */
class HNBASE_EXPORT Md4Transform {
private:
	uint32_t state[4];
	uint32_t count[2];
	unsigned char buffer[64];
	bool wasFlushed;
public:
	/**
	 * Default constructor
	 */
	Md4Transform();

	/**
	 * Default destructor
	 */
	~Md4Transform();

	/**
	 * Adds data to this Md4Transform instance.
	 *
	 * @param data       Pointer pointing to array containing the data.
	 * @param size       Size of the data array.
	 */
	void sumUp(const uint8_t *data, uint32_t size);

	/**
	 * Adds data to this Md4Transform instance.
	 *
	 * @param data       Pointer to data.
	 * @param size       Size of data.
	 */
	void sumUp(const char *data, uint32_t size);

	/**
	 * Finalizes Md4 checksumming operation and returns the checksum of
	 * the data. Call this function only ONCE per each instance of this
	 * class.
	 *
	 * @return     Md4 checksum of the input data
	 */
	Hash<MD4Hash> getHash();
};

#endif /* #ifndef __MD4TRANSFORM_H__ */
