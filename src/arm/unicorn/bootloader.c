// Copyright (C) 2016-2019 Semtech (International) AG. All rights reserved.
//
// This file is subject to the terms and conditions defined in file 'LICENSE',
// which is part of this source code package.

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


// ------------------------------------------------
// Update glue functions

uint32_t up_install_init (void* ctx, uint32_t size, void** pdst) {
    return BOOT_OK;
}

void up_flash_wr_page (void* ctx, void* dst, void* src) {
}

void up_flash_unlock (void* ctx) {
}

void up_flash_lock (void* ctx) {
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
#if 0
    uint32_t funcbuf[WR_FL_HP_WORDS];
    up_ctx uc = {
	.wf_func = prep_wr_fl_hp(funcbuf),
	.fwup = fwup,
    };
    update(&uc, fwup, true);
#endif
}

static uint32_t set_update (void* ptr, hash32* hash) {
#if 0
    uint32_t rv;
    if (ptr == NULL) {
	rv = BOOT_OK;
    } else {
	rv = update(NULL, ptr, false);
    }
    if (rv == BOOT_OK) {
	boot_config* cfg = (boot_config*) BOOT_CONFIG_BASE;
	// unlock EEPROM
	FLASH->PEKEYR = 0x89ABCDEF; // FLASH_PEKEY1
	FLASH->PEKEYR = 0x02030405; // FLASH_PEKEY2
	// copy hash
	if (hash) {
	    for (int i = 0; i < 8; i++) {
		ee_write(&cfg->hash.w[i], hash->w[i]);
	    }
	}
	// set update pointer
	ee_write(&cfg->fwupdate1, (uint32_t) ptr);
	ee_write(&cfg->fwupdate2, (uint32_t) ptr);
	// relock EEPROM
	FLASH->PECR |= FLASH_PECR_PELOCK;
    }
    return rv;
#endif
    return 0;
}


// ------------------------------------------------
// Bootloader information table

static const boot_boottab boottab = {
    .version	= 0x102,
    .update	= set_update,
    .panic	= fw_panic,
    .crc32      = boot_crc32,
    .svc        = svc,
    .sha256     = sha256,
};

// ------------------------------------------------
// Bootloader main entry point

void* bootloader (void) {
    boot_fwhdr* fwh = (boot_fwhdr*) FW_BASE;
    boot_config* cfg = (boot_config*) CONFIG_BASE;

    uint32_t flash_sz = FLASH_SIZE;

    if (cfg->fwupdate1 == cfg->fwupdate2) {
	boot_uphdr* fwup = (boot_uphdr*) cfg->fwupdate1;
	if (fwup != NULL
		&& ((intptr_t) fwup & 3) == 0
		&& (intptr_t) fwup >= FLASH_BASE
		&& sizeof(boot_uphdr) <= flash_sz - ((intptr_t) fwup - FLASH_BASE)
		&& fwup->size >= sizeof(boot_uphdr)
		&& (fwup->size & 3) == 0
		&& fwup->size <= flash_sz - ((intptr_t) fwup - FLASH_BASE)
		&& boot_crc32(((unsigned char*) fwup) + 8, (fwup->size - 8) >> 2) == fwup->crc
		&& true /* TODO hardware id match */ ) {
	    do_install(fwup);
	}
    }

    // verify integrity of current firmware
    if (fwh->size < sizeof(boot_fwhdr)
	    || fwh->size > (flash_sz - (FW_BASE - FLASH_BASE))
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
