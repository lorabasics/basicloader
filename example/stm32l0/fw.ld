/*     _____                _        __     _
 *    /__   \_ __ __ _  ___| | __ /\ \ \___| |_
 *      / /\/ '__/ _` |/ __| |/ //  \/ / _ \ __|
 *     / /  | | | (_| | (__|   '/ /\  /  __/ |_
 *     \_\  |_|  \__,_|\___|_|\_\_\ \_\\___|\__|
 *
 * Copyright (c) 2016-2018 Trackio International AG
 * All rights reserved.
 *
 * This file is subject to the terms and conditions
 * defined in file 'LICENSE', which is part of this
 * source code package.
 *
 */

SECTIONS {
    .text : {
	. = ALIGN(4);
	KEEP(*(.fwhdr))
	. = ALIGN(4);
	*(.text)
	*(.text*)
	. = ALIGN(4);
    } >FWFLASH

    .data : {
	. = ALIGN(4);
	_sdata = .;
	KEEP(*(.isr))
	*(.data)
	*(.data*)
	. = ALIGN(4);
	_edata = .;
    } >RAM AT>FWFLASH

    _sidata = LOADADDR(.data);
  
    .bss : {
	. = ALIGN(4);
	_sbss = .;
	*(.bss)
	*(.bss*)
	*(COMMON)
	. = ALIGN(4);
	_ebss = .;
    } >RAM

    .rodata : {
	. = ALIGN(4);
	*(.rodata)
	*(.rodata*)
	. = ALIGN(4);

	/* make sure flash image is a multiple of page size */
	FILL(0xffffffff)
	. = ALIGN(128);
	__fw_end__ = .;
    } >FWFLASH
}
