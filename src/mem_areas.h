/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017 snickerbockers
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

#ifndef MEM_AREAS_H_
#define MEM_AREAS_H_

#ifdef __cplusplus
extern "C" {
#endif

// System Boot ROM
#define ADDR_BIOS_FIRST  0
#define ADDR_BIOS_LAST   0x001fffff

// flash memory
#define ADDR_FLASH_FIRST 0x00200000
#define ADDR_FLASH_LAST  0x0021ffff

// main system memory
#define ADDR_RAM_FIRST   0x0c000000
#define ADDR_RAM_LAST    0x0cffffff

// G1 bus control registers
#define ADDR_G1_FIRST    0x005F7400
#define ADDR_G1_LAST     0x005F74FF

// system block registers
#define ADDR_SYS_FIRST   0x005f6800
#define ADDR_SYS_LAST    0x005F69FF

// maple bus registers
#define ADDR_MAPLE_FIRST 0x5f6c00
#define ADDR_MAPLE_LAST  0x5f6cff

// G2 bus control registers
#define ADDR_G2_FIRST    0x5f7800
#define ADDR_G2_LAST     0x5f78ff

// GD-ROM drive control registers
#define ADDR_GDROM_FIRST 0x5f7000
#define ADDR_GDROM_LAST  0x5f70ff

// NEC PowerVR 2 control registers
#define ADDR_PVR2_FIRST  0x5f7c00
#define ADDR_PVR2_LAST   0x5f7cff

#define ADDR_PVR2_CORE_FIRST 0x5f8000
#define ADDR_PVR2_CORE_LAST  0x5f9fff

// yep, it's the modem.  And probably the broadband adapter, too.
#define ADDR_MODEM_FIRST     0x600000
#define ADDR_MODEM_LAST      0x60048c

// AICA registers
#define ADDR_AICA_FIRST      0x00700000
#define ADDR_AICA_LAST       0x00707FFF

#define ADDR_AICA_RTC_FIRST  0x00710000
#define ADDR_AICA_RTC_LAST   0x0071000b

#define ADDR_AICA_WAVE_FIRST 0x00800000
#define ADDR_AICA_WAVE_LAST  0x00ffffff

// texture memory.
// This represents both the 32-bit area and the 64-bit area because
// I don't know what the difference between them is supposed to be
#define ADDR_TEX_FIRST       0x05000000
#define ADDR_TEX_LAST        0x057fffff

#ifdef __cplusplus
}
#endif

#endif
