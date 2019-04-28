// Copyright (C) 2016-2019 Semtech (International) AG. All rights reserved.
//
// This file is subject to the terms and conditions defined in file 'LICENSE',
// which is part of this source code package.

#include <string.h>

#include "sha2.h"

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define ENDIAN_n2b32(x) __builtin_bswap32(x)
#else
#define ENDIAN_n2b32(x) (x)
#endif

// ------------------------------------------------
// SHA-256

#define ROR(a,b)	(((a) >> (b)) | ((a) << (32 - (b))))

#define CH(x,y,z)	(((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z)	(((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x)		(ROR(x, 2)  ^ ROR(x, 13) ^ ROR(x, 22))
#define EP1(x)		(ROR(x, 6)  ^ ROR(x, 11) ^ ROR(x, 25))
#define SIG0(x)		(ROR(x, 7)  ^ ROR(x, 18) ^ ((x) >> 3))
#define SIG1(x)		(ROR(x, 17) ^ ROR(x, 19) ^ ((x) >> 10))

static void sha256_do (uint32_t* state, const uint8_t* block) {
    static const uint32_t K[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
	0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
	0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
	0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
	0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
	0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    uint32_t a, b, c, d, e, f, g, h, i, j, t1, t2, w[64];

    for (i = 0, j = 0; i < 16; i++, j += 4) {
	w[i] = (block[j] << 24) | (block[j + 1] << 16) | (block[j + 2] << 8) | (block[j + 3]);
    }
    for ( ; i < 64; i++) {
	w[i] = SIG1(w[i - 2]) + w[i - 7] + SIG0(w[i - 15]) + w[i - 16];
    }

    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];
    f = state[5];
    g = state[6];
    h = state[7];

    for (i = 0; i < 64; i++) {
	t1 = h + EP1(e) + CH(e, f, g) + K[i] + w[i];
	t2 = EP0(a) + MAJ(a, b, c);
	h = g;
	g = f;
	f = e;
	e = d + t1;
	d = c;
	c = b;
	b = a;
	a = t1 + t2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

#undef ROR
#undef CH
#undef MAJ
#undef EP0
#undef EP1
#undef SIG0
#undef SIG1

void sha256 (uint32_t* hash, const uint8_t* msg, uint32_t len) {
    uint32_t state[8] = {
	0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
	0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    uint32_t bitlen = len << 3;
    while (1) {
	if (len < 64) {
	    union {
		uint8_t bytes[64];
		uint32_t words[16];
	    } tmp;
	    memset(tmp.words, 0, sizeof(tmp));
	    memcpy(tmp.bytes, msg, len);
	    tmp.bytes[len] = 0x80;
	    if (len < 56) {
last:
		tmp.words[15] = ENDIAN_n2b32(bitlen);
		sha256_do(state, tmp.bytes);
		for (int i = 0; i < 8; i++) {
		    hash[i] = ENDIAN_n2b32(state[i]);
		}
		return;
	    } else {
		sha256_do(state, tmp.bytes);
		memset(tmp.words, 0, sizeof(tmp));
		goto last;
	    }
	} else {
	    sha256_do(state, msg);
	    msg += 64;
	    len -= 64;
	}
    }
}

#ifdef SHA2_TEST

#include <unistd.h>

static int readfully (int fd, unsigned char* buf, size_t bufsz) {
    while( bufsz ) {
        ssize_t n = read(fd, buf, bufsz);
        if( n <= 0 ) {
            return -1;
        }
        buf += n;
        bufsz -= n;
    }
    return 0;
}

static int writefully (int fd, unsigned char* buf, size_t bufsz) {
    while( bufsz ) {
        ssize_t n = write(fd, buf, bufsz);
        if( n <= 0 ) {
            return -1;
        }
        buf += n;
        bufsz -= n;
    }
    return 0;
}

int main (void) {
    unsigned char buf[128*1024];
    union {
        uint8_t bytes[32];
        uint32_t words[8];
    } hash;
    while( 1 ) {
        uint32_t sz;
        if( readfully(STDIN_FILENO, (unsigned char*) &sz, sizeof(uint32_t)) < 0
                || sz > sizeof(buf)
                || readfully(STDIN_FILENO, buf, sz) < 0 ) {
            break;
        }
        sha256(hash.words, buf, sz);
        writefully(STDOUT_FILENO, hash.bytes, 32);
    }
    return 0;
}

#endif
