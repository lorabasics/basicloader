// Copyright (C) 2016-2019 Semtech (International) AG. All rights reserved.
//
// This file is subject to the terms and conditions defined in file 'LICENSE',
// which is part of this source code package.

#ifndef _sha2_h_
#define _sha2_h_

#include <stdint.h>

void sha256 (uint32_t* hash, const uint8_t* msg, uint32_t len);

#endif

