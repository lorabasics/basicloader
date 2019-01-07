// Copyright (C) 2016-2019 Semtech (International) AG. All rights reserved.
//
// This file is subject to the terms and conditions defined in file 'LICENSE',
// which is part of this source code package.

#ifndef _bootloader_impl_h_
#define _bootloader_impl_h_

#include "bootloader.h"
#include "boottab.h"

extern uint32_t _ebl;
#define BOOT_FW_BASE	((uint32_t) (&_ebl))

#define BOOT_CONFIG_BASE        0x30000000
#define BOOT_CONFIG_SZ		64


// ------------------------------------------------
// Bootloader configuration

typedef struct {
    uint32_t	fwupdate1;	// 0x00 pointer to valid update
    uint32_t	fwupdate2;	// 0x04 pointer to valid update
    hash32	hash;		// 0x08 SHA-256 hash of valid update

    uint8_t	rfu[24];	// 0x28 RFU
} boot_config;

#endif
