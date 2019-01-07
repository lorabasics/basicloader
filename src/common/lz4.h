// Copyright (C) 2016-2019 Semtech (International) AG. All rights reserved.
//
// This file is subject to the terms and conditions defined in file 'LICENSE',
// which is part of this source code package.

#ifndef _lz4_h_
#define _lz4_h_


#ifdef LZ4_FLASHWRITE
#include <stdint.h>
typedef void (*lz4_flash_wr_page) (uint32_t* dst, uint32_t* src);
#endif

int lz4_decompress (
#ifdef LZ4_FLASHWRITE
	lz4_flash_wr_page flash_wr_page,
#endif
	unsigned char* src, int srclen, unsigned char* dst, unsigned char* dict, int dictlen);

#endif
