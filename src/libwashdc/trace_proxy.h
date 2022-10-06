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

#ifndef TRACE_PROXY_H_
#define TRACE_PROXY_H_

#include <stdint.h>

/*
 * This is a memory_interface that proxies another memory_interface
 * and records all write operations to a file so they can be viewed
 * later for debugging or reverse-engineering purposes.
 */

#include "washdc/MemoryMap.h"
#include "washdc/hostfile.h"

struct trace_proxy {
    washdc_hostfile outfile;
    uint32_t mask; // address mask used when we're writing to the backing
    struct memory_interface const *proxied_intf;
    void *proxied_ctxt;
};

void
trace_proxy_create(struct trace_proxy *ctxt, washdc_hostfile outfile,
                   uint32_t mask, struct memory_interface const *intf,
                   void *proxied_ctxt);

extern struct memory_interface trace_proxy_memory_interface;

/*
 * TRACE_PROXY PACKET FORMAT
 *
 * All data is in little-endian format since that is the byte-order
 * used by Dreamcast.
 *
 * 4 bytes - packet type; this is always 1
 *     later there will be other packet types to represent things like
 *     VBLANK interrupt but for now all we log are writes to proxied
 *     memory
 * 4 bytes - the address that the data was written to
 * 4 bytes - length of the write in bytes
 *     this is 4 bytes because stuff like DMA transactions can
 *     transfer lots of data at once.  There will be an "optimizer"
 *     program that will combine adjacent writes to adjacent memory
 *     locations into larger blocks to handle things like writes to
 *     texture memory
 * VARIABLE-NUMBER bytes: the data
 *     the length of this is set by the previous field.
 * UP TO 3 BYTES of padding.  this should all be zero.  This way each
 *     packet will be aligned to a 4-byte boundary.  the lenght field
 *     above should not include this padding.  the program reading the
 *     capture will assume the padding is there if the length of the
 *     data is not a multiple of 4.
 */

#endif
