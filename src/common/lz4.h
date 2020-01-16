// Copyright (C) 2016-2019 Semtech (International) AG. All rights reserved.
//
// This file is subject to the terms and conditions defined in file 'LICENSE',
// which is part of this source code package.

#ifndef _lz4_h_
#define _lz4_h_

int lz4_decompress (void* ctx, unsigned char* src, int srclen, unsigned char* dst, unsigned char* dict, int dictlen);

#endif
