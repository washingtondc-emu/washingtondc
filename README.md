# WashingtonDC

WashingtonDC is an open-source SEGA Dreamcast emulator for Linux.  It's still at
an early stage of development; currently it can only run the Dreamcast firmware,
Crazy Taxi, Power Stone and a handful of homebrew programs.

## GALLERY
![Alt text](media/washingtondc_power_stone_in_game.png "Power Stone")
![Alt text](media/washingtondc_crazy_taxi_in_game.png "Crazy Taxi")

## COMPILING
```
mkdir build
cd build
cmake [OPTIONS] ..
make

Available options for the cmake generation are:

ENABLE_SH4_MMU=On(default)/Off - emulate the sh4's Memory Management Unit (MMU)
ENABLE_DEBUGGER=On(default)/Off - Enable the remote gdb backend
ENABLE_DIRECT_BOOT=On(default)/Off - Enable direct boot mode (optionally skip
                                     boot rom)
DBG_EXIT_ON_UNDEFINED_OPCODE=Of/Off(default) - Bail out if the emulator hits an
                                               undefined opcode
INVARIANTS=On(default)/Off - runtime sanity checks that should never fail
DEEP_SYSCALL_TRACE=On/Off(default) - log system calls made by guest software.
```
## USAGE
```
./washingtondc -b dc_bios.bin -f dc_flash.bin [options] [IP.BIN 1ST_READ.BIN]

OPTIONS:
-b <bios_path> path to dreamcast boot ROM
-c enable development/debugging console access via TCP port 2000
-f <flash_path> path to dreamcast flash ROM image
-g enable remote GDB backend via TCP port 1999
-d enable direct boot (skip BIOS)
-u skip IP.BIN and boot straight to 1ST_READ.BIN (only valid for direct boot)
-m <gdi path> path to .gdi file which will be mounted in the GD-ROM drive
-s path to dreamcast system call image (only needed for direct boot)
-t establish serial server over TCP port 1998
-h display this message and exit

```
The emulator currently only supports one controller, and the controls cannot be
rebinded yet.  It must be controlled using a keyboard with a number pad.

The -b and -f options are mandatory because we need a firmware to boot.  To do a
direct-boot, the -s option is also needed to provide a system call image since
the firmware won't have had a chance to load one itself.

The -c command opens up a TCP on port 2000 that you can connect to via telnet to
control the emulator via a text-based command-line interface; this is the
closest thing to a UI that WashingtonDC has.

You can view online command documentation with the 'help' command.
'begin-execution' is the command to start the emulator.

The only games I know to work so far are Power Stone and Crazy Taxi.

```
    |============================|
    | keyboard   |     Dreamcast |
    |============================|
    | W          | UP    (D-PAD) |
    | S          | DOWN  (D-PAD) |
    | A          | LEFT  (D-PAD) |
    | D          | RIGHT (D-PAD) |
    | 2 (numpad) | A             |
    | 6 (numpad) | B             |
    | 4 (numpad) | X             |
    | 8 (numpad) | Y             |
    | Space      | Start         |
    |============================|

```
## EXAMPLES
load the firmware (dc_bios.bin) with no .gdi disc image mounted:
```
./washingtondc -b dc_bios.bin -f dc_flash.bin
```
load the firmware with a .gdi disc image mounted:
```
./washingtondc -b dc_bios.bin -f dc_flash.bin -m /path/to/disc.gdi
```
direct-boot a homebrew program (requires a system call table dump):
```
./washingtondc -b dc_bios.bin -f dc_flash.bin -s syscalls.bin -du IP.BIN 1st_read.bin
```
## CONTACT
You can reach me at my public-facing e-mail address, chimerasaurusrex@gmail.com
I'm also @sbockers on twitter if you can tolerate my lame sense of humor.
WashingtonDC's official website (such as it is) can be found at http://snickerbockers.github.io.
