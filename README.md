# WashingtonDC Dreamcast Emulator

[![Build Status](https://travis-ci.org/washingtondc-emu/washingtondc.svg?branch=master)](https://travis-ci.org/washingtondc-emu/washingtondc)

Someday this is going to be a Sega Dreamcast emulator.  For now, it's just a
partially-complete SH4 interpreter.


# Instructions

* run washingtondc:
    ./washingtondc -b \<path to Dreamcast BIOS file\>

Not much will happen because this is still in a very early stage of development

# remote debugging with gdb

* run washingtondc with the -g option:

    ./washingtondc -b \<path to Dreamcast BIOS file\>

    It will print:

    Awaiting remote GDB connection on port 1999...

* open gdb in another window
* enter the following commands:
    set architecture sh4
    set step-mode on
    target remote localhost:1999

At this point you will have a live debugger session you can use to watch
washingtondc load the dreamcast bios and eventually crash.
