TOPDIR	:= ../../..
MKDIR	:= $(TOPDIR)/build/makefiles
SRCDIR	:= $(TOPDIR)/src

-include Makefile.local
-include $(MKDIR)/Makefile.local

FLAVOR	?= $(error FLAVOR not set)

default:

include $(MKDIR)/$(FLAVOR).mk
