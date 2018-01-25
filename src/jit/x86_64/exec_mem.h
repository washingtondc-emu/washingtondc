/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018 snickerbockers
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

#ifndef EXEC_MEM_H_
#define EXEC_MEM_H_

#include <stddef.h>

void exec_mem_init(void);
void exec_mem_cleanup(void);

void *exec_mem_alloc(size_t len_req);
void exec_mem_free(void *ptr);

struct exec_mem_stats {
    size_t free_bytes;
    size_t total_bytes;
    unsigned n_allocations;
    unsigned n_free_chunks;
};

void exec_mem_get_stats(struct exec_mem_stats *stats);
void exec_mem_print_stats(struct exec_mem_stats const *stats);

#endif
