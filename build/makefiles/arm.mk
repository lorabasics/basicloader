TOOLCHAIN	:= gcc
CROSS_COMPILE	+= arm-none-eabi-

include $(MKDIR)/toolchain.mk

CMSIS		:= $(SRCDIR)/arm/CMSIS

CFLAGS		+= -fno-common -fno-builtin -fno-exceptions -ffunction-sections -fdata-sections -fomit-frame-pointer
CFLAGS		+= -I$(CMSIS)/Include

LDFLAGS		+= -nostartfiles
