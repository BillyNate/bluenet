# Installation

## Manual

Rather than going through this page, it is also possible to go through the SDK, which has a nice installation manual. It has been test driven by around 50 students, so it pretty thorougly captures most of the edge cases:

* [Crownstone SDK manual](https://docs.google.com/document/d/1W-UfzLD4jOh_F5iSbDKcMxwTOVEVfT2QVTTjQlZCXrc/edit)

## Prerequisites

The installation should not be hard when you have the Nordic SDK. Get this from their website after buying a development kit. You also need a cross-compiler for ARM. You need the JLink utilities from Segger. And you need cmake for the build process.

* [Nordic nRF51822 SDK 8.1](https://developer.nordicsemi.com/nRF5_SDK/nRF51_SDK_v8.x.x/)
* [Nordic S110 Softdevice 8.0](http://www.nordicsemi.com/eng/Products/S110-SoftDevice-v8.0) or [Nordic S1130 Softdevice 1.0](https://www.nordicsemi.com/eng/Products/S130-SoftDevice)
* [JLink Software](http://www.segger.com/jlink-software.html), there is a [.deb file](https://www.segger.com/jlink-software.html?step=1&file=JLinkLinuxDEB64_4.96.4)
* sudo aptitude install cmake

A cross-compiler for ARM is the `GCC` cross-compiler which is maintained by the ARM folks on [Launchpad](https://launchpad.net/gcc-arm-embedded).

    curl -v -O https://launchpad.net/gcc-arm-embedded/4.8/4.8-2014-q3-update/+download/gcc-arm-none-eabi-4_8-2014q3-20140805-linux.tar.bz2
    tar -xvf gcc-arm-none-eabi-4_8-2014q3-20140805-linux.tar.bz2 -C /opt

This is a 32-bit application, so you will need some dependencies:

    sudo apt-get install libstdc++6:i386 libncurses5:i386

If the cross-compiler does not work, make sure you check if all its dependencies are met:

    ldd /opt/gcc-arm-none-eabi-4_8-2014q3/bin/arm-none-eabi-gcc

Unpack the Nordic files to for example the `/opt/softdevices` and `/opt/nrf51_sdk` directories.

It is a `cmake` build system, so you will need it:

    sudo apt-get install cmake

You can now just type `make` in the main directory. Or you can build using the scripts (see below). Before that you'll have to adjust the default configuration settings (see below as well).

### Bugs

There is a bug in the following softdevice files, namely `nrf_svc.h`:

    /opt/softdevices/s110_nrf51_8.0.0_API/include
    /opt/softdevices/s130_nrf51_1.0.0_API/include

Change the assembly line:

        "bx r14" : : "I" (number) : "r0" \

Into:

        "bx r14" : : "I" ((uint16_t)number) : "r0" \

## Usage

You will have to attach a programmer/debugger somehow. Towards that you only need four pins. On the RFduino this is `GND`, `3V`, `RESET`, and `FACTORY` and they are subsequent pins on that side of the RFduino where there are most pins (the other side has the antenna stealing a bit of space for eventual pins). The pin layout of the JLink connector is written out on the [DoBots blog](http://dobots.nl/2014/03/05/rfduino-without-rfduino-code/).


### Configuration

Fork the code by clicking on:

* Fork [https://github.com/dobots/bluenet/fork](https://github.com/dobots/bluenet/fork).
* `git clone https://github.com/${YOUR_GITHUB_USERNAME}/bluenet`
* let us call this directory $BLUENET_DIR

Now you will have to set all fields in the configuration file:

* cp CMakeBuild.config.default CMakeBuild.config
* adjust the `NRF51822_DIR` to wherever you installed the Nordic SDK (it should have `/Include` and `/Source` subdirectories
* adjust the `SOFTDEVICE_DIR` to wherever you unzipped the SoftDevice from Nordic
* adjust the `SOFTDEVICE_SERIES` to for example `110` or `130` (SoftDevice name starts with it, without the `s`)
* adjust major accordingly `SOFTDEVICE_MAJOR=8`
* adjust minor accordingly `SOFTDEVICE_MINOR=0`
* adjust the `SOFTDEVICE_DIR_API` to the directory with the SoftDevice include files
* set `SOFTDEVICE_NO_SEPARATE_UICR_SECTION=1` if you use one of the earlier s110 softdevices with a separate UICR section
* adjust the type `SOFTDEVICE` accordingly (basename of file without `_softdevice.hex`)
* set the `APPLICATION_START_ADDRESS` to start of application in FLASH (called `CODE_R1_BASE` in Nordic documentation)
* set the `APPLICATION_LENGTH` to what remains of FLASH
* set `RAM_R1_BASE` to the start of RAM that is available (SoftDevice S130 v0.5 uses a staggering 10kB from the 16kB!)
* set `RAM_APPLICATION_AMOUNT` to what remains for the application in RAM
* adjust the `COMPILER_PATH` and `COMPILER_TYPE` to your compiler (it will be used as `$COMPILER_PATH\bin\$COMPILER_TYPE-gcc`)
* adjust `JLINK` to the full name of the JLink utility (JLinkExe on Linux)
* adjust `JLINK_GDB_SERVER` to the full name of the JLink utility that supports gdb (JLinkGDBServer on Linux)
* set `BLUETOOTH_NAME` to something you like, but make sure it's short.
* adjust `INDOOR_SERVICE` to `1` if you want to enable it, the same is true for the other services
* adjust `CHAR_MESHING` to `1` if you want to enable meshing functionality, the same is true for other characteristics
* adjust `HARDWARE_BOARD` to the correct number for your board. This determines the pin layout.
* adjust `HARDWARE_VERSION` to the correct version of the NRF51 chip you have. Use script/hardware_version.sh to check your version.
* adjust `SERIAL_VERBOSITY` to the value you prefer. Set it to None to disable all logging over serial. The default is 1 (info).
* adjust `MASTER_BUFFER_SIZE` if you want to have a larger buffer (this buffer is reused by all characteristics that use more than a single byte or string)

Let us now install the SoftDevice on the nRF51822:

    cd $BLUENET_DIR/scripts
    ./softdevice.sh build
    ./softdevice.sh upload

Now we can build our own software:

    cd $BLUENET_DIR
    make

Alternatively, you can build it using our script:

    cd $BLUENET_DIR/scripts
    ./firmware.sh build crownstone

There is also an upload and debug script. The first uses `JLink` to upload, the second uses `gdb` to attach itself
to the running process:

    ./firmware.sh upload crownstone
    ./firmware.sh debug crownstone

You can also run everything in sequence:

    ./firmware.sh all crownstone

And there you go. There are some more utility scripts, such as `reboot.sh`. Use as you wish.

## Flashing with the ST-Link

The above assumes you have the J-Link programmer from Nordic. If you do not have that device, you can still program something like the RFduino or the Crownstone, by using an ST-Link. A full explanation can be found on <https://dobots.nl/2015/01/23/programming-the-nrf51822-with-the-st-link/>.

### Combine softdevice and firmware

First of all you should combine all the required binaries into one big binary. This is done by the script combine.sh. Before you use it, you will need to install srec_cat on your system.

    sudo apt-get install srecord

If you call the script it basically just runs srec_cat:

    ./combine.sh

And you will see that it runs something like this:

    srec_cat /opt/softdevices/s110_nrf51822_7.0.0_softdevice.hex -intel crownstone.bin -binary -offset 0x00016000 -o combined.hex -intel

You have to adjust that file on the moment manually to switch between softdevices or to add/remove the bootloader, sorry! Note that the result is a `.hex` file. Such a file does haveinformation across multiple memory sections. If you upload a `.bin` file often configuration bits/bytes will not be set!

### Upload with OpenOCD

Rather than downloading `openocd` from the Ubuntu repositories, it is recommended to get the newest software from the source:

    cd /opt
    git clone https://github.com/ntfreak/openocd
    sudo aptitude install libtool automake libusb-1.0-0-dev expect
    cd openocd
    ./bootstrap
    ./configure --enable-stlink
    make
    sudo make install

Also, make sure you can use the USB ST-Link device without sudo rights:

    sudo cp scripts/openocd/49-stlinkv2.rules /etc/udev/rules.d
    sudo restart udev

You can now use the `flash_openocd.sh` script in the `scripts` directory:

    ./flash_openocd.sh init

And in another console:

    ./flash_openocd.sh upload combined.bin

Here the binary `combined.bin` is the softdevice and application combined.

## Meshing

The meshion functionality is the one we are currently integrating on the moment. So, this is a moving target. In addition, the mesh code is relatively memory intensive, so you will need a 32kB version to run it. Set
`CHAR_MESHING` to `1` if you want to use it.

For the meshing functionality we use https://github.com/NordicSemiconductor/nRF51-ble-bcast-mesh written by a
Trond Einar Snekvik, department of Engineering Cybernetics at Norwegian University of Science and Technology (and
Nordic Semiconductors).

## UART

Currently UART for debugging. In case you happen to have the nRFgo Motherboard (nRF6310, strongly recommended) you can
easily connect the pints at P2.0 and P2.1 to respectively the pins RXD and TXD on P15 on the board. Do not forget to
switch on the RS232 switch. Subsequently you will need some RS232 to USB cable if you haven't an extremely old laptop.
The current set baudrate you can find in `src/serial.cpp` and is `38400` baud. To read from serial, my personal
favorite application is  `minicom`, but feel free to use any other program.

    (sudo) minicom -c on -s -D /dev/ttyUSB0

The sudo rights are only necessary if your `udev` rights are not properly set. The above flags just set colors to `on`
and define the right usb port to use (if you've multiple).

## Bootloader

To upload a new program when the Crownstone is embedded in a wall socket is cumbersome. For that reason for deployment
we recommend to add a bootloader. We adjusted the Nordic bootloader so that it works with the bluenet code:

    git clone https://github.com/dobots/nrf51-dfu-bootloader-for-gcc-compiler
    cd scripts
    ./all.sh

Make sure to choose the correct branch.

You will have to set some fields such that the bootloader is loaded rather than the application directly. If you use the `J-Link` this script will install bootloader, softdevice, and application:

    ./all.sh

And you should be good to upload binaries, for example with the following python script:

    git clone https://github.com/dobots/nrf51_dfu_linux
    python dfu.py -f crownstone.hex -a CD:E3:4A:47:1C:E4

### Debugging bootloader

Make sure the bootloader is actually loaded and the proper address for the application is set. If you use the `J-Link` you can use `./general_command.sh` to read individual memory locations:

    mem 0x10001014 4

This should be equal to `BOOTLOADER_REGION_START` if you use the bootloader. If it is `0xFFFF` the application will be loaded from the application start address.

If the bootloader does not find a valid app, there might indeed not be an app available, o its configuration field that tells it that the app is correct isn't set properly:

    mem 3FC00 10

This should return `0000 0001 0000 0000 0000 00FE 0000 0000`. If it isn't set correctly, make sure you have uploaded the `bootloader.hex` file (and not only the `bootloader.bin` file).

## iBeacon

There is iBeacon functionality, if you want to try that out. For that you will need to set a define in the main file.

    #define IBEACON

This is not the normal operation mode of the Crownstones, and it is not guaranteed to stay.

