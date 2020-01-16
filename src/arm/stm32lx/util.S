// Copyright (C) 2016-2019 Semtech (International) AG. All rights reserved.
//
// This file is subject to the terms and conditions defined in file 'LICENSE',
// which is part of this source code package.

#include "bootloader.h"

    // --------------------------------------------
    // assembler settings
    .syntax unified
    .thumb


    // --------------------------------------------
    // void delay (uint32_t length)
    .section .boot.delay,"ax",%progbits
    .thumb_func
delay:
	lsls r0, r0, #16
     1: subs r0, #1
	bne 1b
	bx lr

    .size delay, .-delay
    .global delay


    // --------------------------------------------
    // void wr_fl_hp (uint32_t* dst, const uint32_t* src)
    // write flash half-page (DO NOT CALL DIRECTLY!)
    // r0: dst*, r1: src*, r2-r5: scratch
    .section .boot.wr_fl_hp,"ax",%progbits
wr_fl_hp_begin:
    .thumb_func
wr_fl_hp:
	push {r4-r5, lr}
	// copy aligned data from src (RAM) to dst (FLASH)
	ldmia r1!, {r2-r5}
	stmia r0!, {r2-r5}
	ldmia r1!, {r2-r5}
	stmia r0!, {r2-r5}
	ldmia r1!, {r2-r5}
	stmia r0!, {r2-r5}
	ldmia r1!, {r2-r5}
	stmia r0!, {r2-r5}
	// wait for flash busy flag to clear
	ldr r0, 2f
     1: ldr r1, [r0, #24]
	lsls r1, r1, #31
	bmi 1b
	// return
	pop {r4-r5, pc}
    .p2align(2)
     2: .word 0x40022000
wr_fl_hp_end:

    .size wr_fl_hp_begin, .-wr_fl_hp_begin
    .global wr_fl_hp_begin
    .global wr_fl_hp_end
