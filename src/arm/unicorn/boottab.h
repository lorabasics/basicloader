// Copyright (C) 2016-2019 Semtech (International) AG. All rights reserved.
//
// This file is subject to the terms and conditions defined in file 'LICENSE',
// which is part of this source code package.

#ifndef _boottab_h_
#define _boottab_h_

#include "bootloader.h"

// Bootloader information table on unicorn

typedef struct {
    uint32_t version;					// version of boot loader
    __attribute__((noreturn))
	void (*panic) (uint32_t reason, uint32_t addr);	// bootloader panic function 
    uint32_t (*update) (void* ptr, hash32* hash);	// function to set firmware update pointer
    uint32_t (*crc32) (void* buf, uint32_t nwords);     // calculate CRC32
    void* svc;                                          // supervisor call
    void (*wr_flash) (uint32_t* dst, const uint32_t* src, // write flash
            uint32_t nwords, bool erase);
    void (*sha256) (uint32_t* hash,                     // SHA-256
            const uint8_t* msg, uint32_t len);
} boot_boottab;


// Supervisor call IDs

enum {
    BOOT_SVC_PANIC,             // panic: p1=type, p2=reason, p3=addr

    BOOT_SVC_FWBASE = 0x80      // all supervisor call ids below are reserved for bootloader
};

#endif
