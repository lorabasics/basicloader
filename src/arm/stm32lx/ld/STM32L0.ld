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
