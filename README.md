# WashingtonDC Dreamcast Emulator

[![Build Status](https://travis-ci.org/washingtondc-emu/washingtondc.svg?branch=master)](https://travis-ci.org/washingtondc-emu/washingtondc)

Someday this is going to be a Sega Dreamcast emulator.  For now, it's just a
partially-complete SH4 interpreter.


# Instructions

* run washingtondc:
    ./washingtondc -b <path to Dreamcast BIOS file>

if debugging is enabled (it is by default):
    Awaiting remote GDB connection on port 1999...

* open gdb in another window
* enter the following commands:
    set architecture sh4
    set step-mode on
    target remote localhost:1999

At this point you will have a live debugger session you can use to watch
washingtondc load the dreamcast bios and eventually crash.
