/**
 *  \file sha1transform.cpp
 *
 *      This file implements the Secure Hashing Algorithm 1 as
 *      defined in FIPS PUB 180-1 published April 17, 1995.
 *
 *      The SHA-1, produces a 160-bit message digest for a given
 *      data stream.  It should take about 2**n steps to find a
 *      message with the same digest as a given message and
 *      2**(n/2) to find any two messages with the same digest,
 *      when n is the digest size in bits.  Therefore, this
 *      algorithm can serve as a means of providing a
 *      "fingerprint" for a message.
 *
 *  Portability Issues:
 *      SHA-1 is defined in terms of 32-bit "words".  This code
 *      uses <stdint.h> (included via "sha1.h" to define 32 and 8
 *      bit unsigned integer types.  If your C compiler does not
 *      support 32 bit unsigned integers, this code is not
 *      appropriate.
 *
 *  Caveats:
 *      SHA-1 is designed to work with messages less than 2^64 bits
 *      long.  Although SHA-1 allows a message digest to be generated
 *      for messages of any number of bits less than 2^64, this
 *      implementation only works with messages with a length that is
 *      a multiple of the size of an 8-bit character.
 *
 */

#include <hnbase/pch.h>
#include <hnbase/sha1transform.h>

/*
 *  Define the SHA1 circular left shift macro
 */
#define SHA1CircularShift(bits,word) \
                (((word) << (bits)) | ((word) >> (32-(bits))))

/*
 *  SHA1ProcessMessageBlock
 *
 *  Description:
 *      This function will process the next 512 bits of the message
 *      stored in the Message_Block array.
 *
 *  Parameters:
 *      None.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *      Many of the variable names in this code, especially the
 *      single character names, were used because those were the
 *      names used in the publication.
 *
 *
 */
void Sha1Transform::SHA1ProcessMessageBlock() {
	const uint32_t K[] = {          /* Constants defined in SHA-1   */
		0x5A827999,
		0x6ED9EBA1,
		0x8F1BBCDC,
		0xCA62C1D6
	};
	int           t;                 /* Loop counter                */
	uint32_t      temp;              /* Temporary word value        */
	uint32_t      W[80];             /* Word sequence               */
	uint32_t      A, B, C, D, E;     /* Word buffers                */

	/*
	 *  Initialize the first 16 words in the array W
	 */
	for(t = 0; t < 16; t++) {
		W[t] = Message_Block[t * 4] << 24;
		W[t] |= Message_Block[t * 4 + 1] << 16;
		W[t] |= Message_Block[t * 4 + 2] << 8;
		W[t] |= Message_Block[t * 4 + 3];
	}

	for(t = 16; t < 80; t++) {
		W[t] = SHA1CircularShift(1,W[t-3] ^ W[t-8] ^ W[t-14]^W[t-16]);
	}

	A = Intermediate_Hash[0];
	B = Intermediate_Hash[1];
	C = Intermediate_Hash[2];
	D = Intermediate_Hash[3];
	E = Intermediate_Hash[4];

	for(t = 0; t < 20; t++) {
		temp =  SHA1CircularShift(5,A) +
			((B & C) | ((~B) & D)) + E + W[t] + K[0];
		E = D;
		D = C;
		C = SHA1CircularShift(30,B);
		B = A;
		A = temp;
	}

	for(t = 20; t < 40; t++) {
		temp = SHA1CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[1];
		E = D;
		D = C;
		C = SHA1CircularShift(30,B);
		B = A;
		A = temp;
	}

	for(t = 40; t < 60; t++) {
		temp = SHA1CircularShift(5,A) +
		((B & C) | (B & D) | (C & D)) + E + W[t] + K[2];
		E = D;
		D = C;
		C = SHA1CircularShift(30,B);
		B = A;
		A = temp;
	}

	for(t = 60; t < 80; t++) {
		temp = SHA1CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[3];
		E = D;
		D = C;
		C = SHA1CircularShift(30,B);
		B = A;
		A = temp;
	}

	Intermediate_Hash[0] += A;
	Intermediate_Hash[1] += B;
	Intermediate_Hash[2] += C;
	Intermediate_Hash[3] += D;
	Intermediate_Hash[4] += E;

	Message_Block_Index = 0;
}


/*
 *  SHA1PadMessage
 *
 *  Description:
 *      According to the standard, the message must be padded to an even
 *      512 bits.  The first padding bit must be a '1'.  The last 64
 *      bits represent the length of the original message.  All bits in
 *      between should be 0.  This function will pad the message
 *      according to those rules by filling the Message_Block array
 *      accordingly.  It will also call the ProcessMessageBlock function
 *      provided appropriately.  When it returns, it can be assumed that
 *      the message digest has been computed.
 *
 *  Parameters:
 *      context: [in/out]
 *          The context to pad
 *      ProcessMessageBlock: [in]
 *          The appropriate SHA*ProcessMessageBlock function
 *  Returns:
 *      Nothing.
 *
 */
void Sha1Transform::SHA1PadMessage() {
	/*
	 *  Check to see if the current message block is too small to hold
	 *  the initial padding bits and length.  If so, we will pad the
	 *  block, process it, and then continue padding into a second
	 *  block.
	 */
	if (Message_Block_Index > 55) {
		Message_Block[Message_Block_Index++] = 0x80;
		while(Message_Block_Index < 64) {
			Message_Block[Message_Block_Index++] = 0;
		}

		SHA1ProcessMessageBlock();

		while(Message_Block_Index < 56) {
			Message_Block[Message_Block_Index++] = 0;
		}
	} else {
		Message_Block[Message_Block_Index++] = 0x80;
		while (Message_Block_Index < 56) {
			Message_Block[Message_Block_Index++] = 0;
		}
	}

	/*
	 *  Store the message length as the last 8 octets
	 */
	Message_Block[56] = Length_High >> 24;
	Message_Block[57] = Length_High >> 16;
	Message_Block[58] = Length_High >> 8;
	Message_Block[59] = Length_High;
	Message_Block[60] = Length_Low >> 24;
	Message_Block[61] = Length_Low >> 16;
	Message_Block[62] = Length_Low >> 8;
	Message_Block[63] = Length_Low;

	SHA1ProcessMessageBlock();
}


Sha1Transform::Sha1Transform() {
	Length_Low             = 0;
	Length_High            = 0;
	Message_Block_Index    = 0;

	Intermediate_Hash[0]   = 0x67452301;
	Intermediate_Hash[1]   = 0xEFCDAB89;
	Intermediate_Hash[2]   = 0x98BADCFE;
	Intermediate_Hash[3]   = 0x10325476;
	Intermediate_Hash[4]   = 0xC3D2E1F0;

	Computed   = 0;
	Corrupted  = 0;
}

Sha1Transform::~Sha1Transform() {
}

int Sha1Transform::sumUp(const char *data, uint32_t length) {
	if (!length) {
		return shaSuccess;
	}

	if (!data) {
		return shaNull;
	}

	if (Computed) {
		Corrupted = shaStateError;
		return shaStateError;
	}

	if (Corrupted) {
		return Corrupted;
	}
	while(length-- && !Corrupted) {
		Message_Block[Message_Block_Index++] =
			(*data & 0xFF);
		Length_Low += 8;
		if (Length_Low == 0) {
			Length_High++;
			if (Length_High == 0) {
				/* Message is too long */
				Corrupted = 1;
			}
		}

		if (Message_Block_Index == 64) {
			SHA1ProcessMessageBlock();
		}

		data++;
	}

	return shaSuccess;
}

int Sha1Transform::sumUp(const unsigned char *data, uint32_t length) {
	return sumUp(reinterpret_cast<const char *>(data), length);
}

Hash<SHA1Hash> Sha1Transform::getHash() {
	int i;

	if (Corrupted) {
		throw std::runtime_error("Corrupted!");
	}

	if (!Computed) {
		SHA1PadMessage();
		for(i=0; i<64; ++i) {
			/* message may be sensitive, clear it out */
			Message_Block[i] = 0;
		}
		Length_Low = 0;    /* and clear length */
		Length_High = 0;
		Computed = 1;
	}

	unsigned char hash[SHA1HashSize];

	for(i = 0; i < SHA1HashSize; ++i) {
		hash[i] = Intermediate_Hash[i>>2]
				>> 8 * ( 3 - ( i & 0x03 ) );
	}

	return hash;
}
