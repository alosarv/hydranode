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
 * \file md4transform.cpp Implementation of MD4 Checksumming Algorithm
 */

#include <hnbase/pch.h>
#include <hnbase/md4transform.h>

static void Decode(uint32_t * output, const uint8_t * input, size_t len) {
	unsigned int i,j;

	for (i=j=0; j<len; ++i,j+=4)
		output[i]=
			( (uint32_t)input[j  ]     )|
			(((uint32_t)input[j+1])<< 8)|
			(((uint32_t)input[j+2])<<16)|
			(((uint32_t)input[j+3])<<24);
}

static void Encode(uint8_t * output, const uint32_t * input, size_t len) {
	unsigned int i,j;

	for (i=j=0; j<len; ++i,j+=4) {
		output[j  ]= input[i]     &0xff;
		output[j+1]=(input[i]>> 8)&0xff;
		output[j+2]=(input[i]>>16)&0xff;
		output[j+3]= input[i]>>24      ;
	}
}

/* Constants for MD4Transform routine. */
#define S11 3
#define S12 7
#define S13 11
#define S14 19
#define S21 3
#define S22 5
#define S23 9
#define S24 13
#define S31 3
#define S32 9
#define S33 11
#define S34 15

static const uint8_t padding[64] = { 0x80 };

/* F, G and H are basic MD4 functions. */
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (y)) | ((x) & (z)) | ((y) & (z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))

/* ROL rotates x left n bits. */
#define ROL(x, n) (((x) << (n)) | ((x) >> (32-(n))))
#define XX(f,a,b,c,d,x,n,s) do { a+=f(b,c,d)+x+s; a=ROL(a,n); } while (0)

/*
 * FF, GG and HH are transformations for rounds 1, 2 and 3
 * Rotation is separate from addition to prevent recomputation
 */
#define FF(a,b,c,d,x,n) XX(F,a,b,c,d,x,n,0)
#define GG(a,b,c,d,x,n) XX(G,a,b,c,d,x,n,(uint32_t)0x5a827999LU)
#define HH(a,b,c,d,x,n) XX(H,a,b,c,d,x,n,(uint32_t)0x6ed9eba1LU)

static void MD4Transform(uint32_t state[4],const uint8_t block[64]) {
	uint32_t a=state[0];
	uint32_t b=state[1];
	uint32_t c=state[2];
	uint32_t d=state[3];
	uint32_t x[16];

	Decode(x,block,64);

	FF(      a,b,c,d,x[ 0],S11);
	FF(    d,a,b,c,  x[ 1],S12);
	FF(  c,d,a,b,    x[ 2],S13);
	FF(b,c,d,a,      x[ 3],S14);
	FF(      a,b,c,d,x[ 4],S11);
	FF(    d,a,b,c,  x[ 5],S12);
	FF(  c,d,a,b,    x[ 6],S13);
	FF(b,c,d,a,      x[ 7],S14);
	FF(      a,b,c,d,x[ 8],S11);
	FF(    d,a,b,c,  x[ 9],S12);
	FF(  c,d,a,b,    x[10],S13);
	FF(b,c,d,a,      x[11],S14);
	FF(      a,b,c,d,x[12],S11);
	FF(    d,a,b,c,  x[13],S12);
	FF(  c,d,a,b,    x[14],S13);
	FF(b,c,d,a,      x[15],S14);

	GG(      a,b,c,d,x[ 0],S21);
	GG(    d,a,b,c,  x[ 4],S22);
	GG(  c,d,a,b,    x[ 8],S23);
	GG(b,c,d,a,      x[12],S24);
	GG(      a,b,c,d,x[ 1],S21);
	GG(    d,a,b,c,  x[ 5],S22);
	GG(  c,d,a,b,    x[ 9],S23);
	GG(b,c,d,a,      x[13],S24);
	GG(      a,b,c,d,x[ 2],S21);
	GG(    d,a,b,c,  x[ 6],S22);
	GG(  c,d,a,b,    x[10],S23);
	GG(b,c,d,a,      x[14],S24);
	GG(      a,b,c,d,x[ 3],S21);
	GG(    d,a,b,c,  x[ 7],S22);
	GG(  c,d,a,b,    x[11],S23);
	GG(b,c,d,a,      x[15],S24);

	HH(      a,b,c,d,x[ 0],S31);
	HH(    d,a,b,c,  x[ 8],S32);
	HH(  c,d,a,b,    x[ 4],S33);
	HH(b,c,d,a,      x[12],S34);
	HH(      a,b,c,d,x[ 2],S31);
	HH(    d,a,b,c,  x[10],S32);
	HH(  c,d,a,b,    x[ 6],S33);
	HH(b,c,d,a,      x[14],S34);
	HH(      a,b,c,d,x[ 1],S31);
	HH(    d,a,b,c,  x[ 9],S32);
	HH(  c,d,a,b,    x[ 5],S33);
	HH(b,c,d,a,      x[13],S34);
	HH(      a,b,c,d,x[ 3],S31);
	HH(    d,a,b,c,  x[11],S32);
	HH(  c,d,a,b,    x[ 7],S33);
	HH(b,c,d,a,      x[15],S34);

	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;

	memset(x,0,sizeof(x));
}

/* MD4 initialization. Begins an MD4 operation, writing a new context. */
Md4Transform::Md4Transform() : wasFlushed(false) {
	count[0]=count[1]=0;

	/* Load magic initialization constants.
	 */
	state[0]=0x67452301LU;
	state[1]=0xefcdab89LU;
	state[2]=0x98badcfeLU;
	state[3]=0x10325476LU;
}

/*
 * MD4 block update operation. Continues an MD4 message-digest operation,
 * processing another message block, and updating the context.
 */
void Md4Transform::sumUp(const uint8_t *data, uint32_t size) {
	CHECK_THROW(!wasFlushed);

	unsigned int i, index, partLen;

	/* Compute number of bytes mod 64 */
	index = (unsigned int) ((count[0] >> 3) & 0x3F);
	/* Update number of bits */
	if ((count[0]+=((uint32_t)size << 3)) < ((uint32_t)size << 3))
		count[1]++;
	count[1] += ((uint32_t) size >> 29);

	partLen = 64 - index;
	/* Transform as many times as possible.
	 */
	if (size >= partLen) {
		memcpy(&buffer[index],data,partLen);
		MD4Transform(state, buffer);

		for (i = partLen; i + 63 < size; i += 64)
			MD4Transform(state, &data[i]);

		index = 0;
	} else
		i = 0;

	/* Buffer remaining data */
	memcpy(&buffer[index],&data[i],size-i);
}

/*
 * MD4 finalization. Ends an MD4 message-digest operation, writing the
 * the message digest and zeroizing the context.
 */
Hash<MD4Hash> Md4Transform::getHash() {
	CHECK_THROW(!wasFlushed);

	unsigned char digest[16];
	uint8_t bits[8];
	unsigned int index, padLen;

	/* Save number of bits */
	Encode(bits, count, 8);

	/* Pad out to 56 mod 64. */
	index = (unsigned int) ((count[0] >> 3) & 0x3f);
	padLen = (index < 56) ? (56 - index) : (120 - index);
	Md4Transform::sumUp(padding, padLen);

	/* Append length (before padding) */
	Md4Transform::sumUp(bits, 8);
	/* Store state in digest */
	Encode(digest, state, 16);

	/* Clear sensitive information */
	memset(buffer,0,sizeof(buffer));
	memset(state ,0,sizeof(state ));
	count[0]=count[1]=0;

	wasFlushed=true;

	return Hash<MD4Hash>(digest);
}

Md4Transform::~Md4Transform() {
}

void Md4Transform::sumUp(const char *data, uint32_t size) {
	sumUp(reinterpret_cast<const unsigned char*>(data), size);
}
