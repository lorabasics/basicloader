// Copyright (C) 2016-2019 Semtech (International) AG. All rights reserved.
//
// This file is subject to the terms and conditions defined in file 'LICENSE',
// which is part of this source code package.

// The same code can be used for bootloader,
// mkupdate tool, or standalone test

#include <string.h>
#include "lz4.h"


// ------------------------------------------------
// LZ4 decompression function with flash writing
// and page-buffer support

#ifdef LZ4_PAGEBUFFER_SZ
#if ((LZ4_PAGEBUFFER_SZ & 3) != 0) || ((LZ4_PAGEBUFFER_SZ & (LZ4_PAGEBUFFER_SZ - 1)) != 0)
#error "LZ4_PAGEBUFFER_SZ must be a multiple of 4 and a power of 2"
#endif
#endif

#ifdef LZ4_FLASHWRITE
#ifndef LZ4_PAGEBUFFER_SZ
#error "LZ4_FLASHWRITE requires LZ4_PAGEBUFFER_SZ"
#endif
#endif

typedef struct {
    unsigned char* dst;
    int dstlen;
    unsigned char* dictend;
#ifdef LZ4_PAGEBUFFER_SZ
    uint32_t pagebuf[LZ4_PAGEBUFFER_SZ / 4];
#ifdef LZ4_FLASHWRITE
    lz4_flash_wr_page flash_wr_page;
#endif
#endif
} lz4state;

// store byte in output buffer (negative b is match offset, else literal)
// auto-flush buffer on page boundaries
static void putbyte (lz4state* z, int b) {
#ifdef LZ4_PAGEBUFFER_SZ
    int pageoff = z->dstlen & (LZ4_PAGEBUFFER_SZ - 1);
    // check for match reference
    if (b < 0) { // use referenced byte at distance 1..65535
	b = (pageoff+b >= 0) ?
	    ((unsigned char*) z->pagebuf)[pageoff + b] : // referenced byte in page buffer
	    ((z->dstlen + b < 0) ? z->dictend[z->dstlen + b] : z->dst[z->dstlen + b]); // referenced byte in dict or in previous output
    }
    // store byte in page buffer
    ((unsigned char*) z->pagebuf)[pageoff] = (unsigned char) b;
    // flush page when last byte is set
    if (pageoff == (LZ4_PAGEBUFFER_SZ - 1)) {
	// z->dst+z->dstlen still points to current page!
#ifdef LZ4_FLASHWRITE
	z->flash_wr_page((uint32_t*) (z->dst + (z->dstlen & ~(LZ4_PAGEBUFFER_SZ - 1))), z->pagebuf);
#else
	memcpy(z->dst + (z->dstlen & ~(LZ4_PAGEBUFFER_SZ - 1)), z->pagebuf, LZ4_PAGEBUFFER_SZ);
#endif
    }
#else
    z->dst[z->dstlen] = (b < 0) ? ((z->dstlen + b < 0) ? z->dictend[z->dstlen + b] : z->dst[z->dstlen + b]) : (unsigned char) b;
#endif
    z->dstlen++;
}

// decompress from src to dst optionally using dict, return uncompressed size
// depending on configuration the uncompressed data is written directly or
// buffered to ram, or buffered to flash
// if buffering is used, the last page will be padded with FF
int lz4_decompress (
#ifdef LZ4_FLASHWRITE
	lz4_flash_wr_page flash_wr_page,
#endif
	unsigned char* src, int srclen, unsigned char* dst, unsigned char* dict, int dictlen) {

    unsigned char* srcend = src + srclen;
    lz4state z;

    // init state
    z.dst = dst;
    z.dstlen = 0;
    z.dictend = dict + dictlen;
#ifdef LZ4_FLASHWRITE
    z.flash_wr_page = flash_wr_page;
#endif

    // decode sequences
    while (src < srcend) {
	// get token
	unsigned char token = *src++;
	// get literal length
	int l, len = token >> 4;
	if (len == 15) do { l = *src++; len += l; } while (l == 255);
	// copy literals
	while (len--) {
	    putbyte(&z, *src++);
	}
	if (src < srcend) { // last sequence is incomplete and stops after the literals
	    // get offset
	    int offset = *src++;
	    offset = (*src++ << 8) | offset; // 16-bit LSB-first
	    // get match length
	    len = token & 0x0F;
	    if (len == 15) do { l = *src++; len += l; } while(l == 255);
	    len += 4; // minmatch
	    // copy matches from output stream or from dict
	    while (len--) {
		putbyte(&z, -offset);
	    }
	}
    }

    // fill and flush last page
    int n = z.dstlen;
#ifdef LZ4_PAGEBUFFER_SZ
    while (z.dstlen & (LZ4_PAGEBUFFER_SZ - 1)) {
	putbyte(&z, 0xFF);
    }
#endif
    return n;
}


#ifdef LZ4_standalone
// ------------------------------------------------
// Standalone test program
// gcc lz4.c -DLZ4_standalone -DLZ4_compress -o lz4 -llz4 -Wall

#include <stdio.h>
#include <lz4hc.h>

// compress src buffer to dst buffer optionally using dict buffer, return compressed size
int lz4_compress (unsigned char* src, int srclen, unsigned char* dict, int dictlen, unsigned char* dst, int dstlen) {
    unsigned char buf[srclen+64];
    LZ4_streamHC_t* lz4;

    // init LZ4 stream
    if( (lz4 = LZ4_createStreamHC()) == NULL ) {
	return -1;
    }
    LZ4_resetStreamHC(lz4, 9);

    // set dictionary
    if(dictlen) {
	LZ4_loadDictHC(lz4, (char*)dict, dictlen);
    }

    // compress
    dstlen = LZ4_compress_HC_continue(lz4, (char*)src, (char*)dst, srclen, dstlen);
    LZ4_freeStreamHC(lz4);

    // verify
    int len = lz4_decompress(dst, dstlen, buf, dict, dictlen);
    if(len != srclen || memcmp(buf, src, srclen) != 0) {
	return -1;
    }

    return dstlen;
}

// static buffers
static unsigned char data[1024*1024], dict[1024*1024], zdata[1024*1024];

int main (int argc, char **argv) {
    int datalen, dictlen = 0, zlen;
    FILE *fp;

    // check usage
    if (argc < 3) {
	printf("usage %s: <input-file> <output-file> [<dict-file>]\n", argv[0]);
	return 1;
    }

    // load input file
    if ((fp = fopen(argv[1], "rb")) == NULL) {
	printf("can't open input file '%s'\n", argv[1]);
	return 1;
    }
    datalen = fread(data, 1, sizeof(data), fp);
    if (!feof(fp)) {
	printf("input file too large!\n");
	fclose(fp);
	return 1;
    }
    fclose(fp);
    printf("input file '%s': %d bytes\n", argv[1], datalen);

    // load dictionary file
    if (argc == 4) {
	if ((fp = fopen(argv[3], "rb")) == NULL) {
	    printf("can't open dictionary file '%s'\n", argv[3]);
	    return 1;
	}
	dictlen = fread(dict, 1, sizeof(dict), fp);
	if (!feof(fp)) {
	    printf("dictionary file too large!\n");
	    fclose(fp);
	    return 1;
	}
	fclose(fp);
	printf("dictionary file '%s': %d bytes\n", argv[3], dictlen);
    }

    // compress with liblz4hc, verify with own decompressor
    if ((zlen = lz4_compress(data, datalen, dict, dictlen, zdata, sizeof(zdata))) < 0) {
	printf("compression / verification failed!\n");
	return 1;
    }

    // write output file
    if ((fp = fopen(argv[2], "wb")) == NULL ||
	    fwrite(zdata, zlen, 1, fp) != 1 ||
	    fclose(fp)) {
	printf("cannot write output file '%s'\n", argv[2]);
	return 1;
    }
    printf("output file '%s': %d bytes\n", argv[2], zlen);

    // print results
    printf("compression: %d / %d = %d%%\n", zlen, datalen, zlen*100/datalen);
    return 0;
}
#endif
