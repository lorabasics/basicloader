// Copyright (C) 2016-2019 Semtech (International) AG. All rights reserved.
//
// This file is subject to the terms and conditions defined in file 'LICENSE',
// which is part of this source code package.

#ifndef _bootloader_hw_h_
#define _bootloader_hw_h_

// ------------------------------------------------
#if defined(STM32L0)

#include "stm32l0xx.h"

#define FLASH_SZ()		(*((uint16_t*) 0x1FF8007C) << 10)	// flash size register (L0x1 RM0377 28.1.1; L0x2 RM0376 33.1.1)
#define FLASH_PAGE_SZ		128
#define ROUND_PAGE_SZ(sz)       (((sz) + (FLASH_PAGE_SZ - 1)) & ~(FLASH_PAGE_SZ - 1))
#define ISMULT_PAGE_SZ(sz)      (((sz) & (FLASH_PAGE_SZ - 1)) == 0)

#define GPIO_RCC_ENR	RCC->IOPENR
#define GPIO_RCC_ENB(p)	(((p) == 0) ? RCC_IOPENR_GPIOAEN \
	: ((p) == 1) ? RCC_IOPENR_GPIOBEN \
	: ((p) == 2) ? RCC_IOPENR_GPIOCEN \
	: 0)


// ------------------------------------------------
#elif defined(STM32L1)

#include "stm32l1xx.h"

#define GPIO_RCC_ENR	RCC->AHBENR
#define GPIO_RCC_ENB(p)	(((p) == 0) ? RCC_AHBENR_GPIOAEN \
	: ((p) == 1) ? RCC_AHBENR_GPIOBEN \
	: ((p) == 2) ? RCC_AHBENR_GPIOCEN \
	: 0)


// ------------------------------------------------
#else
#error "Unsupported MCU"
#endif


// ------------------------------------------------
// GPIO definition
#define GPIO(p,n,flags)	((((p)-'A') << 8) | (n) | (flags))

#define GPIO_F_ACTLOW	(1 << 16)

// GPIO access
#define GPIOx(pn)	((GPIO_TypeDef*) (GPIOA_BASE + (pn) * (GPIOB_BASE - GPIOA_BASE)))
#define PORTN(gpio)	((gpio) >> 8 & 0xff)
#define PORT(gpio)	GPIOx(PORTN(gpio))
#define PIN(gpio)	((gpio) & 0xff)

#define SET_PIN(gpio, state) do { \
    PORT(gpio)->BSRR |= (1 << (PIN(gpio) + ((state) ? 0 : 16))); \
} while (0)

#define GPIO_ENABLE(p)	do { GPIO_RCC_ENR |= GPIO_RCC_ENB(p); } while (0)
#define GPIO_DISABLE(p)	do { GPIO_RCC_ENR &= ~GPIO_RCC_ENB(p); } while (0)

#endif
