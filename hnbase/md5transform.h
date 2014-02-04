/*
 * Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
 * rights reserved.
 *
 * License to copy and use this software is granted provided that it
 * is identified as the "RSA Data Security, Inc. MD5 Message-Digest
 * Algorithm" in all material mentioning or referencing this software
 * or this function.
 *
 * License is also granted to make and use derivative works provided
 * that such works are identified as "derived from the RSA Data
 * Security, Inc. MD5 Message-Digest Algorithm" in all material
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
 * \file md5transform.h Interface for MD5 checksumming class.
 */

#ifndef __MD5TRANSFORM_H__
#define __MD5TRANSFORM_H__

#include <hnbase/osdep.h>
#include <hnbase/hash.h>

class HNBASE_EXPORT Md5Transform {
public:
	Md5Transform();
	~Md5Transform();
	void sumUp(const unsigned char *data, uint32_t length);
	void sumUp(const char *data, uint32_t length);
	Hash<MD5Hash> getHash();
private:
	uint32_t state[4];                                   /* state (ABCD) */
	uint32_t count[2];        /* number of bits, modulo 2^64 (lsb first) */
	unsigned char buffer[64];                            /* input buffer */
};

#endif
