/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2020, 2022 snickerbockers
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

// main system memory
#define ADDR_AREA3_FIRST   0x0c000000
#define ADDR_AREA3_LAST    0x0fffffff
#define ADDR_AREA3_MASK    0x00ffffff

/*
 * This is where all the I/O registers that aren't in the SH4 go, as well as
 * the boot rom and system flash.
 */
#define ADDR_AREA0_FIRST 0x00000000
#define ADDR_AREA0_LAST  0x03ffffff
#define ADDR_AREA0_MASK  0x01ffffff

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
#define ADDR_PVR2_LAST   0x5f9fff

// yep, it's the modem.  And probably the broadband adapter, too.
#define ADDR_MODEM_FIRST     0x600000
#define ADDR_MODEM_LAST      0x60048c

// AICA registers
#define ADDR_AICA_CHANNEL_FIRST 0x00700000
#define ADDR_AICA_CHANNEL_LAST 0x007027ff

#define ADDR_AICA_COMMON_FIRST 0x00702800
#define ADDR_AICA_COMMON_LAST  0x00702fff

#define ADDR_AICA_DSP_FIRST    0x00703000
#define ADDR_AICA_DSP_LAST     0x00707fff

#define ADDR_AICA_SYS_FIRST 0x00700000
#define ADDR_AICA_SYS_LAST  0x00707fff

#define ADDR_AICA_RTC_FIRST  0x00710000
#define ADDR_AICA_RTC_LAST   0x0071000b

#define ADDR_AICA_WAVE_FIRST 0x00800000
#define ADDR_AICA_WAVE_LAST  0x00ffffff
#define ADDR_AICA_WAVE_MASK  0x001fffff

#define ADDR_EXT_DEV_FIRST 0x01000000
#define ADDR_EXT_DEV_LAST  0x01ffffff

/*
 * texture memory.
 *
 * VRAM can be accessed through one of two areas: the 64-bit area or
 * the 32-bit area. Both areas are backed by the same physical
 * memory.  Physical VRAM consists of two separate 4MB modules, and
 * the difference between the 32-bit and 64-bit areas is in how those
 * two modules are mapped to addresses. The 32-bit area is used to
 * store the framebuffer, and the 64-bit area is used to store textures.
 *
 * Although these two areas are referred to as "32-bit" and "64-bit"
 * areas, there is no restriction on what sizes may be used for read and
 * write operations. The names reflect the fact that the 64-bit area's
 * interleaving allows it to be accessed faster than the 32-bit area
 * since each consecutive set of four bytes comes from alternating RAM modules.
 *
 * The 32-bit area allows for sequential access across all 8MB of VRAM,
 * with the entirety of the second 4MB module placed after the first 4MB
 * module. The 64-bit memory interleaves the first 4MB module with the
 * second 4MB module, alternating between the two modules every four
 * bytes. So, every second set of four bytes in 64-bit area is offset
 * by 4MB in the 32-bit area as illustrated in the table below.
 */
#define ADDR_TEX64_FIRST       0x04000000
#define ADDR_TEX64_LAST        0x047fffff
#define ADDR_TEX32_FIRST       0x05000000
#define ADDR_TEX32_LAST        0x057fffff

/*
 * mirror images of the texture memory areas in ADDR_TEX32_FIRST/ADDR_TEX32_LAST
 * and ADDR_TEX64_FIRST/ADDR_TEX64_LAST.  These get used for channel-2 dma to
 * texture memory.
 *
 * These two regions can both map to either the 32-bit bus or the 64-bit bus.
 * the LMMODE0 (0xa05f6884) and LMMODE1 (0xa05f6888) registers control which bus
 * each range maps to.  When one of those registers is 0, the corresponding
 * memory range maps to the 64-bit bus.  When one of those registers is 1, the
 * corresponding memory range maps to the 32-bit bus.
 */
#define ADDR_AREA4_TEX_REGION_0_FIRST 0x11000000
#define ADDR_AREA4_TEX_REGION_0_LAST  0x117fffff
#define ADDR_AREA4_TEX_REGION_1_FIRST 0x11800000
#define ADDR_AREA4_TEX_REGION_1_LAST  0x11ffffff

// area 4 is used by the tile accelerator
#define ADDR_AREA4_FIRST 0x10000000
#define ADDR_AREA4_LAST  0x13ffffff

#define ADDR_TA_FIFO_POLY_FIRST 0x10000000
#define ADDR_TA_FIFO_POLY_LAST  0x107fffff

#define ADDR_TA_FIFO_YUV_FIRST 0x10800000
#define ADDR_TA_FIFO_YUV_LAST  0x10ffffff

#endif
