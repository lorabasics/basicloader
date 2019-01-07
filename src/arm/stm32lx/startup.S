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
    // Max. number of IRQn
#if defined(STM32L0)
#define MAX_IRQn		32	/* see PM0223, 2.3.4, pg. 29 */
#elif defined(STM32L1)
#define MAX_IRQn		68	/* see PM0056, 2.3.4, pg. 36 */
#else
#error "Unsupported MCU"
#endif


    // --------------------------------------------
    // interrupt vector
    .section .boot.isr_vector,"a",%progbits
    .type  isr_vector, %object
isr_vector:
    .word _estack
    .word reset_handler

    .rept 14
    .word default_handler
    .endr

    .rept MAX_IRQn
    .word default_handler
    .endr

    .size  isr_vector, .-isr_vector


    // --------------------------------------------
    // default handler
    .section .boot.default_handler,"ax",%progbits
    .thumb_func
default_handler:
	movs	r0, #4
	mov	r1, lr
	tst	r0, r1
	bne	1f
	mrs	r2, msp
	b	2f
    1:	mrs	r2, psp
    2:	ldr	r2, [r2, #0x18]		// return address
	movs	r0, BOOT_PANIC_TYPE_EXCEPTION
	mrs	r1, ipsr		// exception number
	b	boot_panic

    .size default_handler, .-default_handler


    // --------------------------------------------
    // reset handler (entry point)
    .section .boot.reset_handler,"ax",%progbits
    .thumb_func
reset_handler:
	ldr	r0, =_estack
	mov	sp, r0

	// call boot loader
	bl	bootloader

	// call entry point (returned from boot loader)
	mov	r1, r0
	ldr	r0, =boottab
	mov	lr, pc
	mov	pc, r1

	// should not be reached
	movs	r0, BOOT_PANIC_TYPE_BOOTLOADER
	movs	r1, BOOT_PANIC_REASON_FWRETURN
	movs	r2, #0			// no return address
	b	boot_panic

    .size reset_handler, .-reset_handler
