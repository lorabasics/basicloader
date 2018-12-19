ifeq ($(TOOLCHAIN),gcc)
    FLAGS	+= -MMD -MP
    FLAGS	+= -g
    CFLAGS	+= -std=gnu11
    LDFLAGS	+= -Wl,--gc-sections -Wl,-Map,$(basename $@).map
    CC		:= $(CROSS_COMPILE)gcc
    AS		:= $(CROSS_COMPILE)as
    LD		:= $(CROSS_COMPILE)gcc
    HEX		:= $(CROSS_COMPILE)objcopy -O ihex
    BIN		:= $(CROSS_COMPILE)objcopy -O binary
endif

CDEFS		+= $(DEFS)
ASDEFS		+= $(DEFS) ASSEMBLY
CFLAGS		+= $(addprefix -D,$(CDEFS))
ASFLAGS		+= $(addprefix -D,$(ASDEFS))

CFLAGS		+= $(FLAGS)
ASFLAGS		+= $(FLAGS)
