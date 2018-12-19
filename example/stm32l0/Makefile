CC	:= arm-none-eabi-gcc

CFLAGS	+= -std=gnu11
CFLAGS	+= -Wall
CFLAGS	+= -g

CFLAGS	+= -mcpu=cortex-m0plus -fno-common -fno-builtin -fno-exceptions -ffunction-sections -fdata-sections -fomit-frame-pointer 

CFLAGS	+= -I../../src/common
CFLAGS	+= -I../../src/arm/stm32lx
CFLAGS	+= -I../../src/arm/CMSIS/Include
CFLAGS	+= -I../../src/arm/CMSIS/Device/ST/STM32L0xx/Include

LDFLAGS	+= -Wl,--gc-sections -Wl,-Map,$(basename $@).map
LDFLAGS	+= -nostdlib
LDFLAGS	+= -T../../src/arm/stm32lx/ld/STM32L0xxB.ld
LDFLAGS	+= -Tfw.ld

FWTOOL	:= python ../../tools/fwtool/fwtool.py

hello.hex: hello
	arm-none-eabi-objcopy -O ihex $< $@
	$(FWTOOL) patch $@

hello: hello.o

clean:
	rm -f *.o *.map hello hello.hex

.PHONY: clean

.DELETE_ON_ERROR:
