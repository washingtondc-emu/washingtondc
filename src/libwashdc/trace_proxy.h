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

#endif
