// Copyright (C) 2016-2019 Semtech (International) AG. All rights reserved.
//
// This file is subject to the terms and conditions defined in file 'LICENSE',
// which is part of this source code package.

#ifndef _bootloader_h_
#define _bootloader_h_


// This is the public "API" of the bootloader,
// i.e. the way the firmware and outside world
// interact with it. Since a bootloader doesn't
// change once installed and deployed, utmost
// care must be taken when modifying this API!


// Panic types
#define BOOT_PANIC_TYPE_EXCEPTION	0	// Exception handler
#define BOOT_PANIC_TYPE_BOOTLOADER	1	// Bootloader (reason codes see below)
#define BOOT_PANIC_TYPE_FIRMWARE	2	// Firmware (reason codes are application defined)


// Panic reason codes for type bootloader
#define BOOT_PANIC_REASON_FWRETURN	0	// firmware returned unexpectedly
#define BOOT_PANIC_REASON_CRC		1	// firmware CRC verification failed
#define BOOT_PANIC_REASON_FLASH		2	// error writing flash
#define BOOT_PANIC_REASON_UPDATE	3	// error updating firmware


// Update type codes
#define BOOT_UPTYPE_PLAIN		0	// plain update
#define BOOT_UPTYPE_LZ4			1	// lz4-compressed self-contained update
#define BOOT_UPTYPE_LZ4DELTA		2	// lz4-compressed block-delta update


// Magic numbers
#define BOOT_MAGIC_SIZE			0xff1234ff	// place-holder for firmware size


#ifndef ASSEMBLY

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>


// Bootloader return values (don't change values!)
enum {
    BOOT_OK,
    BOOT_E_GENERAL,		// general error
    BOOT_E_NOIMPL,		// not implemented error
    BOOT_E_SIZE,		// size error
};


// SHA-256 hash
typedef union {
    uint8_t b[32];
    uint32_t w[8];
} hash32;

_Static_assert(sizeof(hash32) == 32, "sizeof(hash32) must be 32");


// Firmware header
typedef struct {
    uint32_t	crc;		// firmware CRC
    uint32_t	size;		// firmware size (in bytes, including this header)
    /* -- everything below until end (size-8) is included in CRC -- */
    uint32_t	entrypoint;	// address of entrypoint
} boot_fwhdr;

_Static_assert(sizeof(boot_fwhdr) == 12, "sizeof(boot_fwhdr) must be 12");


// Hardware identifier (EUI-48, native byte order)
typedef union {
    struct __attribute__((packed)) {
	uint32_t a;
	uint16_t b;
    };
    uint8_t bytes[6];
} eui48;

_Static_assert(sizeof(eui48) == 6, "sizeof(eui48) must be 6");

static inline uint64_t eui2int (eui48* eui) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return ((uint64_t) eui->b << 32) | eui->a;
#else
    return ((uint64_t) eui->a << 16) | eui->b;
#endif
}


// Update header
typedef struct {
    uint32_t	crc;		// update CRC
    uint32_t	size;		// update size (in bytes, including this header)
    /* -- everything below until end (size-8) is included in CRC -- */
    uint32_t	fwcrc;		// firmware CRC (once unpacked)
    uint32_t	fwsize;		// firmware size (in bytes, including header)
    eui48	hwid;		// hardware target
    uint8_t	uptype;		// update type
    uint8_t	rfu;		// RFU
} boot_uphdr;

_Static_assert(sizeof(boot_uphdr) == 24, "sizeof(boot_uphdr) must be 24");

// Update delta header
typedef struct {
    uint32_t	refcrc;		// referenced firmware CRC
    uint32_t	refsize;	// referenced firmware size
    uint32_t    blksize;        // block size (multiple of flash page size, e.g. 4096)
} boot_updeltahdr;

_Static_assert(sizeof(boot_updeltahdr) == 12, "sizeof(boot_updeltahdr) must be 12");

// Update delta block
typedef struct __attribute__((packed)) {
    uint32_t    hash[2];        // block hash (sha256[0-7])
    uint8_t     blkidx;         // block number
    uint8_t     dictidx;        // dictionary block number
    uint16_t    dictlen;        // length of dictionary data (in bytes)
    uint16_t    lz4len;         // length of lz4-compressed block data (in bytes, up to block size)
    uint8_t     lz4data[];      // lz4-compressed block data
} boot_updeltablk;

_Static_assert(sizeof(boot_updeltablk) == 14, "sizeof(boot_updeltablk) must be 14");

#endif
#endif
