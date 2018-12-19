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

_estack = ORIGIN(RAM) + LENGTH(RAM);
_ebl = ORIGIN(BLFLASH) + LENGTH(BLFLASH);

SECTIONS {
    .boot : {
	. = ALIGN(4);
	KEEP(*(.boot.isr_vector))
	. = ALIGN(4);
	*(.boot*)
	*(.text*)
	*(.rodata*)
    } >BLFLASH
}
