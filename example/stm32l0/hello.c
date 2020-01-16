// Copyright (C) 2016-2019 Semtech (International) AG. All rights reserved.
//
// This file is subject to the terms and conditions defined in file 'LICENSE',
// which is part of this source code package.

#include "stm32l072xx.h"
#include "bootloader.h"
#include "boottab.h"

void _start (boot_boottab* boottab); // forward declaration

// Firmware header
__attribute__((section(".fwhdr")))
const volatile boot_fwhdr fwhdr = {
    // CRC and size will be patched by external tool
    .crc	= 0,
    .size	= BOOT_MAGIC_SIZE,
    .entrypoint = (uint32_t) _start,
};

static void clock_init (void) {
    // System is clocked by MSI @2.1MHz at startup
    // We want to go to PLL(HSI16) @32MHz

    // 1a. HSI: Enable
    RCC->CR |= RCC_CR_HSION;
    // 1b. HSI: Wait for it
    while ((RCC->CR & RCC_CR_HSIRDY) == 0);

    // 2a. Flash: Enable prefetch buffer
    FLASH->ACR |= FLASH_ACR_PRFTEN;
    // 2b. Flash: Use 1 Wait state
    FLASH->ACR |= FLASH_ACR_LATENCY;
    while ((FLASH->ACR & FLASH_ACR_LATENCY) == 0);

    // 3a. Power: Enable clock
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    // 3b. Power: Select Vrange 1 (min. 1.71V!)
    PWR->CR = PWR_CR_VOS_0;
    // 3c. Power: Wait for regulator
    while ((PWR->CSR & PWR_CSR_VOSF) != 0);

    // 4a. PLL: Set source (HSI16), multiplier (4), divider (2)
    RCC->CFGR |= (RCC_CFGR_PLLSRC_HSI | RCC_CFGR_PLLMUL4 | RCC_CFGR_PLLDIV2);
    // 4b. PLL: Enable
    RCC->CR |= RCC_CR_PLLON;
    // 4c. PLL: Wait for it
    while ((RCC->CR & RCC_CR_PLLRDY) == 0);

    // 5a. System clock: Set source (PLL)
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    // 5b. System clock: Wait for it
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);

    // 6. Turn off MSI
    RCC->CR &= ~RCC_CR_MSION;
}

static void uart_init (void) {
    // Configure USART (115200/8N1, TX)
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    USART2->BRR = 278; // 115200 (APB1 clock @32MHz)
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE;

    // Configure GPIO (PA2 / USART2_TX / AF4)
    RCC->IOPENR |= RCC_IOPENR_GPIOAEN;
    GPIOA->AFR[0]  = (GPIOA->AFR[0]  & ~(7 << (4 * (2)))) | (4 << (4 * (2))); /* af 4            */
    GPIOA->MODER   = (GPIOA->MODER   & ~(3 << (2 * (2)))) | (2 << (2 * (2))); /* alternate func  */
    GPIOA->PUPDR   = (GPIOA->PUPDR   & ~(3 << (2 * (2)))) | (0 << (2 * (2))); /* no pull-up/down */
    GPIOA->OTYPER  = (GPIOA->OTYPER  & ~(1 << (1 * (2)))) | (0 << (1 * (2))); /* push-pull       */
    GPIOA->OSPEEDR = (GPIOA->OSPEEDR & ~(3 << (2 * (2)))) | (1 << (2 * (2))); /* medium speed    */
}

static void uart_print (char* s) {
    while (*s) {
	while ((USART2->ISR & USART_ISR_TXE) == 0);
	USART2->TDR = *s++;
    }
}

static void i2h (char* dst, uint32_t v) {
    dst[8] = '\0';
    for (int i = 0; i < 8; i++) {
	dst[7 - i] = "0123456789abcdef"[v & 0xf];
	v >>= 4;
    }
}

void _start (boot_boottab* boottab) {
    // We only use stack in this example. A real firmware
    // would need to do initialization at this point, e.g.
    // data / bss segments, ISR vector re-map, etc.

    char crcbuf[8+1];

    clock_init();
    uart_init();

    uart_print("----------------------\r\n");
    uart_print("Hello World!\r\n");

    uart_print("Build:      " __DATE__ " " __TIME__ "\r\n");

    i2h(crcbuf, boottab->version);
    uart_print("Bootloader: 0x");
    uart_print(crcbuf);
    uart_print("\r\n");

    i2h(crcbuf, fwhdr.crc);
    uart_print("Firmware:   0x");
    uart_print(crcbuf);
    uart_print("\r\n");

    while (1) __WFI(); // good night
}
