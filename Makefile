BOARDDIRS=$(dir $(wildcard build/boards/*/Makefile))

default:

%:
	for BOARDDIR in $(BOARDDIRS); do $(MAKE) -C $${BOARDDIR} $@; done
