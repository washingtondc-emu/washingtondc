/*******************************************************************************
 *
 * Copyright 2017, 2019, 2020 snickerbockers
 * snickerbockers@washemu.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

#ifndef FIFO_H_
#define FIFO_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "washdc/log.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FIFO_DEREF(nodep, tp, memb)                     \
    (*((tp*)(((uint8_t*)nodep) - offsetof(tp, memb))))

#define FIFO_HEAD_INITIALIZER(name) { NULL /* first */, &name.first /* plast */ }

#define FIFO_FOREACH(head, curs) \
    for ((curs) = (head).first; (curs); (curs) = (curs)->next)

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

static inline struct fifo_node *fifo_peek(struct fifo_head *fifo) {
    return fifo->first;
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

static inline void fifo_erase(struct fifo_head *fifo, struct fifo_node *node) {
    struct fifo_node **cursp = &fifo->first;

    while (*cursp) {
        struct fifo_node *curs = *cursp;

        if (curs == node)
            break;
        cursp = &curs->next;
    }

    if (*cursp) {
        *cursp = node->next;
        node->next = NULL;
    } else {
        washdc_log_warn("WARNING: attempting to erase "
                        "non-present element from FIFO\n");
    }
}

#ifdef __cplusplus
}
#endif

#endif
