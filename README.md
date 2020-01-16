# Basic Loader

Basic Loader is an update-capable bootloader that supports the unpacking and
installing of new firmware on an end-device.

Basic MAC uses this component to load its firmware, and to support firmware
updates over-the-air (FUOTA).

## Building Basic Loader

The reference hardware platform for Basic Loader is the B-L072Z-LRWAN1 STM32
LoRa™ Discovery kit.

### Prerequisites
It is recommended to use a recent Ubuntu distribution as build host with the
`gcc-arm-embedded` package installed.

To build the bootloaders for all supported platforms, simply run `make` in
the toplevel directory.

Alternatively, change into a specific target board's build directory and run
`make` there:

```
cd build/boards/B-L072Z-LRWAN1
make
```

The output of the build process is a file named `bootloader.hex` that can be
loaded onto the B-L072Z-LRWAN1 development board. If there is no valid firmware
installed, the LED LD2 will be flashing the corresponding error sequence
(SYNC-2-2-1).

## Release Notes

### Release 4
16-Jan-2020

- Support for LZ4 in-place delta updates
- Bugfix: Flash writing routines
- Improved readability of LED blink sequence
- Option to blink LED during update process
- Improved out-of-tree build support
- Improved ZFW (firmware patching) tool

### Release 3
01-May-2019

- Bootloader for STM32 makes SHA-256 function available to firmware
- New ZFW-tool creates firmware archives with metadata
- Changed linker scripts to work around a recent [regression in GNU
  ld](https://sourceware.org/bugzilla/show_bug.cgi?id=24289)
- Bugfixes

### Pre-release 2
15-Jan-2019

- Bootloader for STM32 makes Flash-write function available to firmware

### Pre-release 1
07-Jan-2019

- Verifies and loads firmware
- Tool for patching firmware image header
- Support for plain updates
