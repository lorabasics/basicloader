# The Bootloader

TrackNet is excited to contribute to the LoRaWAN™ ecosystem by open-sourcing
the firmware updates over-the-air (FUOTA) portion of its end-device LoRaWAN stack.

One important component of this stack is an update-capable bootloader. This loader
supports the unpacking and installing of new firmware on an end-device.

Over the next few days, we will start migrating our internal codebase to this
public repository, where we will then continue to evolve and maintain this component.
This file will be updated as this process continues.

## Update 1
*Tue, Mar 20 2018*

The bootloader skeleton and build environment have been migrated to this new
repository. We recommend the use of a recent Ubuntu distribution as build
host. We use `gcc-arm-embedded` from this PPA:
<https://launchpad.net/~team-gcc-arm-embedded/+archive/ubuntu/ppa>

To build, change into the target board's build directory and type make:

```
cd build/boards/B-L072Z-LRWAN1
make
```

You'll end up with a file called `bootloader.hex` that can be loaded onto the
B-L072Z-LRWAN1 STM32 LoRa™ Discovery kit. Since there is no valid firmware yet,
the LED LD2 will be flashing the corresponding error sequence (SYNC-2-2-1).

## Update 2
*Thu, Mar 22 2018*

To make this a bit more interesting, an example firmware has been added to the
repository. This simple application will just print "Hello World" and some
information about the bootloader and firmware to the UART (115200/8N1).

The bootloader expects the firmware to have a valid header with CRC and length.
Because this information cannot be produced by the complier/linker, a tool is
provided that can patch this information in the firmware hex file.

The patch tool is written in Python. It is tested with Python 3.6 and requires
the package `intelhex` package to be installed.

To build the application, change into the example's directory and type make:
```
cd example/stm32l0
make
```

This produces a file called `hello.hex` which can then be loaded onto the
B-L072Z-LRWAN1 STM32 LoRa™ Discovery kit. Make sure you have also loaded the
bootloader previously. Upon startup, you should something like this on the
serial port:
```
----------------------
Hello World!
Build:      Mar 22 2018 10:57:16
Bootloader: 0x00000100
Firmware:   0x64861573
```
