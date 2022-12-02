/*******************************************************************************
 *
 *
 *    Copyright (C) 2022 snickerbockers
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 ******************************************************************************/

#ifndef AREA0_H_
#define AREA0_H_

#include "washdc/hostfile.h"
#include "washdc/MemoryMap.h"

struct boot_rom;
struct flash_mem;
struct sys_block_ctxt;
struct maple;
struct pvr2;
struct aica;
struct gdrom_ctxt;
struct aica_rtc;

/*
  need:
  boot rom
  flash memory
  system control reg
  maple control reg
  gdrom
  g1
  g2
  pvr control reg
  ta/pvr core reg
  modem
  g2 (again?)
  AICA
  ext dev
 */

struct area0 {
    struct memory_map map;

    struct boot_rom *bios;
    struct flash_mem *flash;
    struct sys_block_ctxt *sys_block;
    struct maple *maple;
    struct gdrom_ctxt *gdrom;
    // G1 is NULL
    // G2 is NULL
    struct pvr2 *pvr2;
    // modem is NULL
    struct aica *aica; // sys
    /* struct aica *aica;  // rtc */
    /* struct aica *aica;  // memory */
    // ext_dev is NULL
    struct aica_rtc *rtc;
};

void area0_init(struct area0 *area,
                struct boot_rom *bios,
                struct flash_mem *flash,
                struct sys_block_ctxt *sys_block,
                struct maple *maple,
                struct gdrom_ctxt *gdrom,
                struct pvr2 *pvr2,
                struct aica *aica,
                struct aica_rtc *rtc,
                washdc_hostfile pvr2_trace_file,
                washdc_hostfile aica_trace_file);
void area0_cleanup(struct area0 *area);

extern struct memory_interface area0_intf;

#endif
