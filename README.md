![WashingtonDC](media/washingtondc_logo_640x320.png)

http://washemu.org

snickerbockers@washemu.org

Discord Server: https://discord.gg/2vmxMCd

WashingtonDC is an open-source SEGA Dreamcast emulator for Linux and
Microsoft Windows.

## COMPILING
```
mkdir build
cd build
cmake [OPTIONS] ..
make

The program executable will be at src/washingtondc/washingtondc (relative to
the root of the build directory)

Available options for the cmake generation are:

ENABLE_SH4_MMU=On(default)/Off - emulate the sh4's Memory Management Unit (MMU)
ENABLE_DEBUGGER=On(default)/Off - Enable the remote gdb backend
ENABLE_DIRECT_BOOT=On(default)/Off - Enable direct boot mode (optionally skip
                                     boot rom)
DBG_EXIT_ON_UNDEFINED_OPCODE=On/Off(default) - Bail out if the emulator hits an
                                               undefined opcode
INVARIANTS=On(default)/Off - runtime sanity checks that should never fail
DEEP_SYSCALL_TRACE=On/Off(default) - log system calls made by guest software.
```
## USAGE

src/washingtondc/washingtondc -b dc_bios.bin -f dc_flash.bin [options] -- [game]

[game] should be the path to a file of one of the below types:

|extension|type|
----------|----------------------------------|
| .gdi    | GDI-format GD-ROM image |
| .chd    | MAME CHD format GD-ROM image |
| .cdi    | DiscJuggler CD-ROM image representing a MIL-CD homebrew |
| .elf    | SH-4 executable program loaded to 8c010000 |
| .bin    | raw SH-4 binary loaded to 8c010000 |

If the game is a .elf or a .bin, then the emulator will automatically
load it to 8c010000 and begin execution from there.  The other three
types will be loaded into the emulated GD-ROM drive and the emulator
will begin executing the firmware at a0000000 just like a real
Dreamcast would.  In either case, the firmware image (-b option) and
flash image (-f option) are mandatory.

```
OPTIONS:
-b <bios_path>           path to dreamcast boot ROM
-f <flash_path>          path to dreamcast flash ROM image
-g gdb                   enable remote GDB backend on tcp port 1999
-g washdbg               enable remote WashDbg backend on tcp port 1999
-n                       don't do native memory inlining when the jit is enabled
-t                       establish serial server over TCP port 1998
-h                       display this message and exit
-p                       disable the dynamic recompiler and enable the
                             interpreter instead
-j                       disable the x86_64 backend and use the JIT IL
                             interpreter instead
-x                       enable the x86_64 dynamic recompiler backend (this is
                             enabled by default)
-w                       enable the experimental WashDbg debugger via text
                             stream over TCP port 1999
```

By default, only one controller will be supported.  You can enable
additional controllers by editing $HOME/.config/washdc/wash.cfg .

## CONTROLS

Control bindings are stored in $HOME/.config/washdc/wash.cfg.  This file is
automatically created the first time WashingtonDC is run.  Controls can be
edited by editing this file.

The default keyboard controls are listed below.  The default gamepad controls
are designed around the Logitech F310 in XInput mode, although you can remap
them to fit any other controller.

```
    |============================|
    | keyboard   |     Dreamcast |
    |============================|
    | W          | UP    (STICK) |
    | S          | DOWN  (STICK) |
    | A          | LEFT  (STICK) |
    | D          | RIGHT (STICK) |
    | Q          | LEFT TRIGGER  |
    | E          | RIGHT TRIGGER |
    | UP         | UP    (D-PAD) |
    | LEFT       | LEFT  (D-PAD) |
    | DOWN       | DOWN  (D-PAD) |
    | RIGHT      | RIGHT (D-PAD) |
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
src/washingtondc/washingtondc -b dc_bios.bin -f dc_flash.bin
```
load the firmware with a .gdi disc image mounted:
```
src/washingtondc/washingtondc -b dc_bios.bin -f dc_flash.bin -- /path/to/disc.gdi
```
direct-boot a homebrew program:
```
src/washingtondc/washingtondc -b dc_bios.bin -f dc_flash.bin -- my_game.elf
```
## LICENSE
WashingtonDC is licensed under the terms of the GNU GPLv3.  The
terms of this license can be found in COPYING.

WashingtonDC also makes use of several third-party libraries available under
various different licenses.

WashingtonDC makes use of nothings' stb_image_write, which is included in
src/libwashdc/stb_image_write.h .  The license can be found at the bottom of
that file.

WashingtonDC also makes use of the glfw library.  This is not included in this
source distribution, and is instead distributed using a git submodule.  The
license for this software can be found in external/glfw/LICENSE.md in the
source, or at LICENSE_glfw.txt in packaged builds.

WashingtonDC also makes use of the Capstone library.  This is not included in
this source distribution, and is instead distributed using a git submodule.  The
licenses for this software can be found in external/capstone/LICENSE.txt and
external/capstone/LICENSE_LLVM.txt in the source, or at LICENSE_capstone.txt and
LICENSE_llvm.txt in packaged builds.

WashingtonDC also makes use of the libevent library.  This is not included in
this source distribution, and is instead automatically downloaded by the build
system as a tarball.  The license for this software can be found in the tarball
at libevent-2.1.8-stable/LICENSE in the source, or at LICENSE_libevent.txt in
packaged builds.

WashingtonDC also makes use of version 2.1.0 of the glew library.  A copy of
this software with some unnecessary components removed is included in
external/glew.  The license for this software can be found at
external/glew/LICENSE.txt in the source, or at LICENSE_glew.txt in packaged
builds.

WashingtonDC uses the imgui library.  This software is included via a git
submodule.  The license for this software can be found at
external/imgui/LICENSE.txt in the source, or at LICENSE_imgui.txt in packaged
builds.

WashingtonDC uses the portaudio library.  This software is included via a git
submodule.  The license for this software can be found at
external/portaudio/LICENSE.txt in the source, or at LICENSE_portaudio.txt in
packaged builds.

WashingtonDC uses the libchdr library; this software is included via a git submodule.
The license for this software can be found at external/libchdr/LICENSE.txt.  Libchdr
is based upon source code from the MAME project; this license can be found in the
libchdr source in a comment line at the top of most source files, or alternatively
in external/external_licenses/libchdr_aaron_giles.

WashingtonDC uses the zlib library, which is included in the libchdr sources.
The license for this library can be found at external/libchdr/deps/zlib-1.2.11/README.

WashingtonDC uses the LZMA SDK, which is included in the libchdr sources.
The license for this library can be found at external/libchdr/deps/lzma-19.00/LICENSE.

Some code from FFmpeg was used to implement Yamaha's ADPCM format.  This code is
part of the FFmpeg library and is licensed under version 2.1 or greater of the
GNU Lesser General Public License (LGPL).  The GNU LGPL allows licensees to
optionally accept code under the terms of the GPL instead; I have chosen to do
that and accept it under the terms of the GNU GPL version 3.

WashingtonDC uses a processed version of the SDL gamecontrollerdb; the license
for this can be found in its source at src/washingtondc/sdl_gamecontrollerdb.h.

## CONTACT
You can reach me at my public-facing e-mail address, snickerbockers@washemu.org.

I'm also @sbockers on twitter if you can tolerate my lame sense of humor.

WashingtonDC's official website (such as it is) can be found at http://www.washemu.org.
