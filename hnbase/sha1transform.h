/**
 *  \file sha1transform.h
 *
 *      This is the header file for code which implements the Secure
 *      Hashing Algorithm 1 as defined in FIPS PUB 180-1 published
 *      April 17, 1995.
 *
 *      Many of the variable names in this code, especially the
 *      single character names, were used because those were the names
 *      used in the publication.
 *
 *      Please read the file sha1transform.cpp for more information.
 *
 */

#ifndef __SHA1TRANSFORM_H__
#define __SHA1TRANSFORM_H__

#include <hnbase/hash.h>

#define SHA1HashSize 20
enum
{
    shaSuccess = 0,
    shaNull,            //!< Null pointer parameter
    shaInputTooLong,    //!< input data too long
    shaStateError       //!< called Input after Result
};

/**
 * Perform SHA-1 Checksumming on data
 */
class HNBASE_EXPORT Sha1Transform {
public:
	//! Constructor
	Sha1Transform();

	//! Destructor
	~Sha1Transform();

	//! Checksum data
	int sumUp(const char *data, uint32_t length);

	//! Checksum data
	int sumUp(const unsigned char *data, uint32_t length);

	//! Retrieve results. Note that this object should be destroyed
	//! after retrieving hash - continueing to calculate hash will result
	//! in abnormal program termination (in debug mode).
	Hash<SHA1Hash> getHash();
private:
	//! Message Digest
	uint32_t Intermediate_Hash[SHA1HashSize/4];

	//! Message length in bits
	uint32_t Length_Low;

	//! Message length in bits
	uint32_t Length_High;

	//! Index into message block array
	int Message_Block_Index;

	//! 512-bit message blocks
	uint8_t Message_Block[64];

	//! Is the digest computed?
	int Computed;

	//! Is the message digest corrupted?
	int Corrupted;

	void SHA1ProcessMessageBlock();
	void SHA1PadMessage();
};

#endif
