// Copyright (C) 2016-2019 Semtech (International) AG. All rights reserved.
//
// This file is subject to the terms and conditions defined in file 'LICENSE',
// which is part of this source code package.

#include "bootloader.h"
#include "update.h"

#if !defined(UP_PAGEBUFFER_SZ) || ((UP_PAGEBUFFER_SZ & 3) != 0) || ((UP_PAGEBUFFER_SZ & (UP_PAGEBUFFER_SZ - 1)) != 0)
#error "UP_PAGEBUFFER_SZ must be defined as a multiple of 4 and a power of 2"
#endif

#define PB_WORDS	(UP_PAGEBUFFER_SZ >> 2)


// ------------------------------------------------
// Update functions

static uint32_t update_plain (void* ctx, boot_uphdr* fwup, bool install) {
    int n = fwup->fwsize;
    uint32_t* src = (uint32_t*) (fwup + 1);
    uint32_t* dst;
    uint32_t rv;

    // size must be a multiple of 4 (word size)
    if ((n & 3) != 0) {
	return BOOT_E_SIZE;
    }
    n >>= 2;

    // perform size check and get install address
    if ((rv = up_install_init(ctx, n, (void**) &dst)) != BOOT_OK) {
	return rv;
    }

    // copy new firmware to destination
    if (install) {
	up_flash_unlock(ctx);
	while (n > 0) {
	    uint32_t buf[PB_WORDS];
	    int i, m = (n < PB_WORDS) ? n : PB_WORDS;
	    n -= m;
	    for (i = 0; i < m; i++) {
		buf[i] = *src++;
	    }
	    for (; i < PB_WORDS; i++) {
		buf[i] = 0; // pad last page with 0
	    }
	    up_flash_wr_page(ctx, dst, buf);
	    dst += PB_WORDS;
	}
	up_flash_lock(ctx);
    }
    return BOOT_OK;
}

uint32_t update (void* ctx, boot_uphdr* fwup, bool install) {
    // Note: The integrity of the update pointed to by fwup has
    // been verified at this point.

    switch (fwup->uptype) {
	case BOOT_UPTYPE_PLAIN:
	    return update_plain(ctx, fwup, install);
	default:
	    return BOOT_E_NOIMPL;
    }
}
