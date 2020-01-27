include $(MKDIR)/arm.mk

VPATH		+= $(SRCDIR)/arm/stm32lx
VPATH		+= $(SRCDIR)/common
VPATH		+= $(SRCDIR)/common/micro-ecc

SRCS		+= bootloader.c
SRCS		+= util.S
SRCS		+= startup.S

SRCS		+= update.c
SRCS		+= sha2.c
SRCS		+= lz4.c

STM32		:= $(shell echo $(MCU) | sed -E 's/^STM32(L[01])([0-9][0-9])R?([8BZ])$$/ok t\/\1 v\/\2 s\/\3/')
ifneq (ok,$(firstword $(STM32)))
    $(error Could not parse MCU: $(MCU))
endif
STM32_T		:= $(notdir $(filter t/%,$(STM32)))
STM32_V		:= $(notdir $(filter v/%,$(STM32)))
STM32_S		:= $(notdir $(filter s/%,$(STM32)))


DEFS		+= STM32$(STM32_T)
DEFS		+= STM32$(STM32_T)$(STM32_V)xx

DEFS		+= LZ4_PAGEBUFFER_SZ=128
DEFS		+= UP_PAGEBUFFER_SZ=128

FLAGS		+= -mcpu=cortex-m0plus
FLAGS		+= -I$(SRCDIR)/common

CFLAGS		+= -Wall
CFLAGS		+= -Os
CFLAGS		+= -I$(SRCDIR)/arm/CMSIS/Device/ST/STM32$(STM32_T)xx/Include

LDFLAGS		+= -mcpu=cortex-m0plus
LDFLAGS		+= -T$(SRCDIR)/arm/stm32lx/ld/STM32$(STM32_T)xx$(STM32_S).ld
LDFLAGS		+= -T$(SRCDIR)/arm/stm32lx/ld/STM32$(STM32_T).ld

OBJS		= $(addsuffix .o,$(basename $(SRCS)))

bootloader: $(OBJS)

bootloader.hex: bootloader
	$(HEX) $< $@

default: bootloader.hex


clean:
	rm -f *.o *.d *.map bootloader bootloader.hex bootloader.bin

.PHONY: clean default


MAKE_DEPS       := $(MAKEFILE_LIST)     # before we include all the *.d files

-include $(OBJS:.o=.d)

$(OBJS): $(MAKE_DEPS)
