# Basic Loader

Basic Loader is an update-capable bootloader that supports the unpacking and
installing of new firmware on an end-device.

Basic MAC uses this component to load its firmware, and to support firmware
updates over-the-air (FUOTA).

## Building Basic Loader

The reference hardware platform for Basic Loader is the B-L072Z-LRWAN1 STM32
LoRa™ Discovery kit.

### Prerequisites
It is recommended to use a recent Ubuntu distribution as build host. We use
`gcc-arm-embedded` from this PPA:
<https://launchpad.net/~team-gcc-arm-embedded/+archive/ubuntu/ppa>

To build, change into the target board's build directory and type make:

```
cd build/boards/B-L072Z-LRWAN1
make
```

The output of the build process is a file called `bootloader.hex` that can be
loaded onto the B-L072Z-LRWAN1 development board. If there is no valid firmware
installed, the LED LD2 will be flashing the corresponding error sequence
(SYNC-2-2-1).

## Release Notes

### Pre-release 2
15-Jan-2019

- Bootloader for STM32 makes Flash-write function available to firmware

### Pre-release 1
07-Jan-2019

- Verifies and loads firmware
- Tool for patching firmware image header
- Support for plain updates
