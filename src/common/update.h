// Copyright (C) 2016-2019 Semtech (International) AG. All rights reserved.
//
// This file is subject to the terms and conditions defined in file 'LICENSE',
// which is part of this source code package.

#ifndef _update_h_
#define _update_h_

#include "bootloader.h"

uint32_t update (void* ctx, boot_uphdr* fwup, bool install);

// glue functions
extern uint32_t up_install_init (void* ctx, uint32_t fwsize, void** pfwdst, uint32_t tmpsize, void** ptmpdst, boot_fwhdr** pcurrentfw);
extern void up_flash_wr_page (void* ctx, void* dst, void* src);
extern void up_flash_unlock (void* ctx);
extern void up_flash_lock (void* ctx);

#endif
