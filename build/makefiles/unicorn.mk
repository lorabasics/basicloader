include $(MKDIR)/arm.mk

VPATH		+= $(SRCDIR)/arm/unicorn
VPATH		+= $(SRCDIR)/common
VPATH		+= $(SRCDIR)/common/micro-ecc

SRCS		+= bootloader.c

SRCS		+= update.c
#SRCS		+= lz4.c
#SRCS		+= sha2.c
#SRCS		+= uECC.c

#DEFS		+= LZ4_FLASHWRITE
#DEFS		+= LZ4_PAGEBUFFER_SZ=64
DEFS		+= UP_PAGEBUFFER_SZ=64

FLAGS		+= -mcpu=cortex-m0plus
FLAGS		+= -I$(SRCDIR)/common

CFLAGS		+= -Wall
CFLAGS		+= -Os

LDFLAGS		+= -T$(SRCDIR)/arm/unicorn/ld/mem.ld
LDFLAGS		+= -T$(SRCDIR)/arm/unicorn/ld/bootloader.ld

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
