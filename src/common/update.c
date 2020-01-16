// Copyright (C) 2016-2019 Semtech (International) AG. All rights reserved.
//
// This file is subject to the terms and conditions defined in file 'LICENSE',
// which is part of this source code package.

#include "bootloader.h"
#include "update.h"
#include "sha2.h"
#include "lz4.h"

#if !defined(UP_PAGEBUFFER_SZ) || ((UP_PAGEBUFFER_SZ & 3) != 0) || ((UP_PAGEBUFFER_SZ & (UP_PAGEBUFFER_SZ - 1)) != 0)
#error "UP_PAGEBUFFER_SZ must be defined as a multiple of 4 and a power of 2"
#endif

#define PB_WORDS	(UP_PAGEBUFFER_SZ >> 2)


// ------------------------------------------------
// Update functions

// write flash pages (src can be in flash too)
static void flashcopy (void* ctx, uint32_t* dst, const uint32_t* src, uint32_t nwords) {
    while (nwords > 0) {
	uint32_t buf[PB_WORDS];
	int i, m = (nwords < PB_WORDS) ? nwords : PB_WORDS;
	nwords -= m;
	for (i = 0; i < m; i++) {
	    buf[i] = *src++;
	}
	for (; i < PB_WORDS; i++) {
	    buf[i] = 0; // pad last page with 0
	}
	up_flash_wr_page(ctx, dst, buf);
	dst += PB_WORDS;
    }
}

static uint32_t update_plain (void* ctx, boot_uphdr* fwup, bool install) {
    uint32_t* src = (uint32_t*) (fwup + 1);
    uint32_t* dst;
    uint32_t rv;

    // perform size check and get install address
    if ((rv = up_install_init(ctx, fwup->fwsize, (void**) &dst, 0, NULL, NULL)) != BOOT_OK) {
	return rv;
    }

    // copy new firmware to destination
    if (install) {
	up_flash_unlock(ctx);
	flashcopy(ctx, dst, src, fwup->fwsize >> 2);
	up_flash_lock(ctx);
    }
    return BOOT_OK;
}

// process LZ4-compressed self-contained update
static uint32_t update_lz4 (void* ctx, boot_uphdr* fwup, bool install) {
    uint8_t* dst;
    uint8_t* src = (uint8_t*) fwup + sizeof(boot_uphdr);
    uint32_t srclen = fwup->size - sizeof(boot_uphdr);
    uint32_t lz4len = srclen - src[srclen-1]; // strip word padding
    uint32_t rv;

    // perform size check and get install address
    if ((rv = up_install_init(ctx, fwup->fwsize, (void**) &dst, 0, NULL, NULL)) != BOOT_OK) {
	return rv;
    }

    if (install) {
	up_flash_unlock(ctx);
	// uncompress new firmware and replace current firmware at destination
	lz4_decompress(ctx, src, lz4len, dst, NULL, 0);
	up_flash_lock(ctx);
    }

    return BOOT_OK;
}

static bool checkhash (const uint8_t* msg, uint32_t len, uint32_t* hash) {
    uint32_t tmp[8];
    sha256(tmp, msg, len);
    return (tmp[0] == hash[0] && tmp[1] == hash[1]);
}

// process LZ4-compressed block-delta update
static uint32_t update_lz4delta (void* ctx, boot_uphdr* fwup, bool install) {
    boot_updeltahdr* dhdr = (boot_updeltahdr*) ((uint8_t*) fwup + sizeof(boot_uphdr));
    uint8_t* src = (uint8_t*) dhdr + sizeof(boot_updeltahdr);
    uint8_t* end = (uint8_t*) fwup + fwup->size;
    uint32_t blksize = dhdr->blksize;
    uint8_t* dst;
    uint8_t* tmp;
    boot_fwhdr* fwhdr;
    uint32_t rv;

    // perform size check and get install address and temp area
    if ((rv = up_install_init(ctx, fwup->fwsize, (void**) &dst, blksize, (void**) &tmp, &fwhdr)) != BOOT_OK) {
	return rv;
    }

    // check reference firmware crc and size before installing (will be overwritten during install)
    if (!install && (dhdr->refcrc != fwhdr->crc || dhdr->refsize != fwhdr->size)) {
	return BOOT_E_GENERAL;
    }

    // process delta blocks
    while (src < end) {
	boot_updeltablk* b = (boot_updeltablk*) src; // delta block
	uint32_t boff = b->blkidx * blksize;
	uint32_t doff = b->dictidx * blksize;
	if (boff > fwup->fwsize || doff + b->dictlen > dhdr->refsize) {
	    return BOOT_E_SIZE;
	}
	uint8_t* baddr = dst + boff;
	uint32_t bsz = (fwup->fwsize - boff < blksize) ? fwup->fwsize - boff : blksize; // current block size (last block might be shorter)
	if (install) {
	    // verify target block
	    if (!checkhash(baddr, bsz, b->hash)) {
		up_flash_unlock(ctx);
		// verify temp block
		if (!checkhash(tmp, bsz, b->hash)) {
		    // uncompress delta to temp block
		    if (lz4_decompress(ctx, b->lz4data, b->lz4len, tmp, (uint8_t*) fwhdr + doff, b->dictlen) != bsz) {
			return BOOT_E_GENERAL; // unrecoverable error - should not happen!
		    }
		    // verify temp block
		    if (!checkhash(tmp, bsz, b->hash)) {
			return BOOT_E_GENERAL; // unrecoverable error - should not happen!
		    }
		}
		// copy temp block to target
		flashcopy(ctx, (uint32_t*) baddr, (uint32_t*) tmp, bsz >> 2);
		up_flash_lock(ctx);
	    }
	}
	// advance to next delta block (4-aligned)
	src += (sizeof(boot_updeltablk) + b->lz4len + 3) & ~0x3;
    }

    return BOOT_OK;
}

uint32_t update (void* ctx, boot_uphdr* fwup, bool install) {
    // Note: The integrity of the update pointed to by fwup has
    // been verified at this point.

    switch (fwup->uptype) {
	case BOOT_UPTYPE_PLAIN:
	    return update_plain(ctx, fwup, install);
	case BOOT_UPTYPE_LZ4:
	    return update_lz4(ctx, fwup, install);
	case BOOT_UPTYPE_LZ4DELTA:
	    return update_lz4delta(ctx, fwup, install);
	default:
	    return BOOT_E_NOIMPL;
    }
}
