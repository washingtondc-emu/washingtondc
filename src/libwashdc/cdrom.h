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

#ifndef CDROM_H_
#define CDROM_H_

/*
 * the purpose of this file is to hold vadious
 * platform-agnostic CD-related defines
 */

// size of a single frame on a CD-ROM (or CDDA) including metadata
#define CDROM_FRAME_SIZE 2352

// size of a data block on most CD-ROM data modes
#define CDROM_FRAME_DATA_SIZE 2048

// offset to the data from the beginning of a frame
#define CDROM_MODE1_DATA_OFFSET 16
#define CDROM_MODE2_DATA_OFFSET 24

// convert between FAD and LBA.  this is a 150-byte offset
unsigned cdrom_lba_to_fad(unsigned lba);
unsigned cdrom_fad_to_lba(unsigned fad);

#endif
