// Copyright (C) 2016-2019 Semtech (International) AG. All rights reserved.
//
// This file is subject to the terms and conditions defined in file 'LICENSE',
// which is part of this source code package.

#include <string.h>

#include "update.h"
#include "boottab.h"
#include "sha2.h"


// ------------------------------------------------
// Memory

extern uint32_t _estack;
extern uint32_t _ebl;

#define RAM_BASE        0x10000000
#define RAM_SIZE        (16 * 1024)
#define FLASH_BASE      0x20000000
#define FLASH_SIZE      (128 * 1024)
#define EEPROM_BASE     0x30000000
#define EEPROM_SIZE     (8 * 1024)

#define FLASH_PAGE_SZ       128
#define ROUND_PAGE_SZ(sz)   (((sz) + (FLASH_PAGE_SZ - 1)) & ~(FLASH_PAGE_SZ - 1))
#define ISMULT_PAGE_SZ(sz)  (((sz) & (FLASH_PAGE_SZ - 1)) == 0)

#define FW_BASE         ((uint32_t) (&_ebl))
#define CONFIG_BASE	EEPROM_BASE


// ------------------------------------------------
// CRC-32

static void crc32 (uint32_t* pcrc, unsigned char* buf, uint32_t len) {
    int i;
    uint32_t byte, crc, mask;

    crc = ~(*pcrc);
    while (len-- != 0) {
	byte = *buf++;
	crc = crc ^ byte;
	for (i = 7; i >= 0; i--) {
	    mask = -(crc & 1);
	    crc = (crc >> 1) ^ (0xEDB88320 & mask);
	}
    }
    *pcrc = ~crc;
}

static uint32_t boot_crc32 (void* buf, uint32_t nwords) {
    uint32_t crc = 0;
    crc32(&crc, buf, nwords * 4);
    return crc;
}


// ------------------------------------------------
// Panic

static void svc (uint32_t id, uint32_t p1, uint32_t p2, uint32_t p3); // fwd decl

__attribute__((noreturn, naked, noinline))
static void fw_panic (uint32_t reason, uint32_t addr) {
    svc(BOOT_SVC_PANIC, BOOT_PANIC_TYPE_FIRMWARE, reason, addr);
    __builtin_unreachable();
}

__attribute__((noreturn, naked, noinline))
static void boot_panic (uint32_t reason) {
    svc(BOOT_SVC_PANIC, BOOT_PANIC_TYPE_BOOTLOADER, reason, 0);
    __builtin_unreachable();
}


// ------------------------------------------------
// Supervisor call

__attribute__((naked, noinline))
static void svc (uint32_t id, uint32_t p1, uint32_t p2, uint32_t p3) {
    asm("svc 0");
    // SVC call should not return, but might if not handled correctly
    boot_panic(0xdeadbeef);
}


// ------------------------------------------------
// Flash functions

void wr_flash (uint32_t* dst, const uint32_t* src, uint32_t nwords, bool erase) {
    if( ((uintptr_t) dst & 3) == 0 ) {
        while( nwords > 0 ) {
            if( erase && (((uintptr_t) dst) & (FLASH_PAGE_SZ-1)) == 0
                    && (uintptr_t) dst >= FLASH_BASE && (uintptr_t) dst < (FLASH_BASE + FLASH_SIZE) ) {
                memset(dst, 0, FLASH_PAGE_SZ);
            }
            int wtw;
            if( nwords > (FLASH_PAGE_SZ >> 2)) {
                wtw = (FLASH_PAGE_SZ >> 2);
            } else {
                wtw = nwords;
            }
            if( src ) {
                memcpy(dst, src, wtw << 2);
                src += wtw;
            }
            dst += wtw;
            nwords -= wtw;
        }
    }
}


// ------------------------------------------------
// Update glue functions

typedef struct {
    boot_uphdr* fwup;
    bool unlocked;
} up_ctx;

uint32_t up_install_init (void* ctx, uint32_t fwsize, void** pfwdst, uint32_t tmpsize, void** ptmpdst, boot_fwhdr** pcurrentfw) {
    up_ctx* uc = ctx;
    if (!ISMULT_PAGE_SZ(fwsize) || fwsize > ((uintptr_t) uc->fwup - FW_BASE)) {
	// new firmware is not multiple of page size or would overwrite update
	return BOOT_E_SIZE;
    }

    // assume dependency on current firmware when temp storage is requested
    if (tmpsize) {
	boot_fwhdr* fwhdr = (boot_fwhdr*) FW_BASE;
	uint32_t fwmax = (fwsize > fwhdr->size) ? fwsize : fwhdr->size;
	if (!ISMULT_PAGE_SZ(tmpsize) || fwmax + ROUND_PAGE_SZ(tmpsize) > ((uintptr_t) uc->fwup - FW_BASE)) {
	    return BOOT_E_SIZE;
	}
    }

    // set installation address for new firmware
    *pfwdst = (void*) FW_BASE;

    // set address for temporary storage
    if (tmpsize && ptmpdst) {
	*ptmpdst = (unsigned char*) uc->fwup - tmpsize;
    }

    // set pointer to current firmware header
    if (pcurrentfw) {
	*pcurrentfw = (boot_fwhdr*) FW_BASE;
    }

    return BOOT_OK;
}

void up_flash_wr_page (void* ctx, void* dst, void* src) {
    up_ctx* uc = ctx;
    if( uc->unlocked ) {
        wr_flash(dst, src, FLASH_PAGE_SZ >> 2, true);
    }
}

void up_flash_unlock (void* ctx) {
    up_ctx* uc = ctx;
    uc->unlocked = true;
}

void up_flash_lock (void* ctx) {
    up_ctx* uc = ctx;
    uc->unlocked = false;
}

static void ee_write (uint32_t* dst, uint32_t val) {
    if( (uintptr_t) dst >= EEPROM_BASE && (uintptr_t) dst < (EEPROM_BASE + EEPROM_SIZE) ) {
	*dst = val;
    }
}

// ------------------------------------------------
// Update functions

typedef struct {
    uint32_t	fwupdate1;	// 0x00 pointer to valid update
    uint32_t	fwupdate2;	// 0x04 pointer to valid update
    hash32	hash;		// 0x08 SHA-256 hash of valid update

    uint8_t	rfu[24];	// 0x28 RFU
} boot_config;

static void do_install (boot_uphdr* fwup) {
    up_ctx uc = {
	.fwup = fwup,
    };
    if (update(&uc, fwup, true) != BOOT_OK) {
	boot_panic(BOOT_PANIC_REASON_UPDATE);
    }
}

static bool check_update (boot_uphdr* fwup) {
    uint32_t flash_sz = FLASH_SIZE;

    return ( ((intptr_t) fwup & 3) == 0
	     && (intptr_t) fwup >= FLASH_BASE
	     && sizeof(boot_uphdr) <= flash_sz - ((intptr_t) fwup - FLASH_BASE)
	     && fwup->size >= sizeof(boot_uphdr)
	     && (fwup->size & 3) == 0
	     && fwup->size <= flash_sz - ((intptr_t) fwup - FLASH_BASE)
	     && boot_crc32(((unsigned char*) fwup) + 8, (fwup->size - 8) >> 2) == fwup->crc
	     && true /* TODO hardware id match */ );
}

static uint32_t set_update (void* ptr, hash32* hash) {
    uint32_t rv;
    if( ptr == NULL ) {
	rv = BOOT_OK;
    } else {
        up_ctx uc = {
            .fwup = ptr,
        };
	rv = check_update((boot_uphdr*) ptr) ? update(&uc, ptr, false) : BOOT_E_SIZE;
    }
    if( rv == BOOT_OK ) {
	boot_config* cfg = (boot_config*) CONFIG_BASE;
	if( hash ) {
	    for( int i = 0; i < 8; i++ ) {
		ee_write(&cfg->hash.w[i], hash->w[i]);
	    }
	}
	// set update pointer
	ee_write(&cfg->fwupdate1, (uint32_t) ptr);
	ee_write(&cfg->fwupdate2, (uint32_t) ptr);
    }
    return rv;
}


// ------------------------------------------------
// Bootloader information table

static const boot_boottab boottab = {
    .version	= 0x108,
    .update	= set_update,
    .panic	= fw_panic,
    .crc32      = boot_crc32,
    .svc        = svc,
    .wr_flash   = wr_flash,
    .sha256     = sha256,
};

// ------------------------------------------------
// Bootloader main entry point

void* bootloader (void) {
    boot_fwhdr* fwh = (boot_fwhdr*) FW_BASE;
    boot_config* cfg = (boot_config*) CONFIG_BASE;

    // check presence and integrity of firmware update
    if (cfg->fwupdate1 == cfg->fwupdate2) {
	boot_uphdr* fwup = (boot_uphdr*) cfg->fwupdate1;
	if (fwup != NULL && check_update(fwup)) {
	    do_install(fwup);
	}
    }

    // verify integrity of current firmware
    if (fwh->size < sizeof(boot_fwhdr)
	    || fwh->size > (FLASH_SIZE - (FW_BASE - FLASH_BASE))
	    || boot_crc32(((unsigned char*) fwh) + 8, (fwh->size - 8) >> 2) != fwh->crc) {
	boot_panic(BOOT_PANIC_REASON_CRC);
    }

    // clear fwup pointer in EEPROM if set
    if (cfg->fwupdate1 != 0 || cfg->fwupdate2 != 0) {
	set_update(NULL, NULL);
    }

    // call entry point
    ((void (*) (const boot_boottab*)) fwh->entrypoint)(&boottab);

    // not reached
    boot_panic(BOOT_PANIC_REASON_FWRETURN);
}


// ------------------------------------------------
// Bootloader header

__attribute__((section(".boot.header"))) const struct {
    uint32_t init_sp;
    uint32_t init_pc;
} boothdr = {
    .init_sp = (uint32_t) &_estack,
    .init_pc = (uint32_t) bootloader,
};
