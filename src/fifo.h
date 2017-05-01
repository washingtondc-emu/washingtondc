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

#ifndef FIFO_H_
#define FIFO_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FIFO_DEREF(nodep, tp, memb) \
    (*((tp*)(((uint8_t*)nodep) - offsetof(tp, memb))))

struct fifo_node {
    struct fifo_node *next;
};

struct fifo_head {
    struct fifo_node *first;
    struct fifo_node **plast;
};

static inline void fifo_init(struct fifo_head *fifo) {
    fifo->first = NULL;
    fifo->plast = &fifo->first;
}

static inline void fifo_push(struct fifo_head *fifo, struct fifo_node *node) {
    *fifo->plast = node;
    node->next = NULL;
    fifo->plast = &node->next;
}

static inline struct fifo_node* fifo_pop(struct fifo_head *fifo) {
    struct fifo_node *first = fifo->first;

    if (first) {
        fifo->first = fifo->first->next;

        if (fifo->plast == &first->next)
            fifo->plast = &fifo->first;

        first->next = NULL;
    }

    return first;
}

static inline size_t fifo_len(struct fifo_head *fifo) {
    size_t len = 0;
    struct fifo_node *nodep = fifo->first;

    while (nodep) {
        len++;
        nodep = nodep->next;
    }
    return len;
}

static inline bool fifo_empty(struct fifo_head *fifo) {
    return fifo->first == NULL;
}

#ifdef __cplusplus
}
#endif

#endif
