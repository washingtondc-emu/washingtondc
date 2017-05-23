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

/*
 * XXX Currently the only memory area which has its Image Area implemented is
 * the main RAM.  I don't have it implemented for the texture memory yet because
 * I haven't determined if the 32-bit/64-bit texture access areas are supposed
 * to be images of each other, and therefore I don't yet know how the actual
 * Image Area for that area of memory will work.
 *
 * The reason why I have not implemented it for Area 0 is that I have not had a
 * chance to run a HW test to confirm that my understanding of its image area is
 * correct, and I don't currently have any games that I know use it.  When I
 * have a chance to run an hw test on this, I will implement it.
 *
 * As for the RAM's image area, I know of one game that uses it (Namco Museum),
 * and I have run a hardware test to verify that the same data gets read/written
 * for same offset in all four image areas.
 */

// System Boot ROM
#define ADDR_BIOS_FIRST  0
#define ADDR_BIOS_LAST   0x001fffff

// flash memory
#define ADDR_FLASH_FIRST 0x00200000
#define ADDR_FLASH_LAST  0x0021ffff

/*
 * main system memory - unlike the mappings, this one includes its own image
 * areas between ADD_RAM_FIRST and ADDR_RAM_LAST.  The mask is what you use to
 * get the offset from the beginning of RAM to a given memory address.
 */
#define ADDR_RAM_FIRST   0x0c000000
#define ADDR_RAM_LAST    0x0fffffff
#define ADDR_RAM_MASK    0x00ffffff

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
