/*******************************************************************************
 *
 * Copyright 2017-2020 snickerbockers
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

#include <stdlib.h>
#include <stdio.h>

#include "compiler_bullshit.h"
#include "washdc/fifo.h"
#include "dreamcast.h"
#include "log.h"

#include "washdc/error.h"

static char const *error_type_string(enum error_type tp);
static void print_attr(struct error_attr const *attr);

static enum error_type error_pending = ERROR_NONE;

static enum error_type error_type;

static struct error_attr *first_attr;

static struct fifo_head err_callbacks = FIFO_HEAD_INITIALIZER(err_callbacks);

WASHDC_NORETURN void error_raise(enum error_type tp) {
    error_type = tp;

    struct fifo_node *cursor;

    FIFO_FOREACH(err_callbacks, cursor) {
        struct error_callback *cb =
            &FIFO_DEREF(cursor, struct error_callback, node);

        cb->callback_fn(cb->arg);
    }

    dc_print_perf_stats();

    error_print();

    bool dump_mem;
    if (config_get_dump_mem_on_error())
        washdc_dump_main_memory("washdc_error_dump.bin");

    fflush(stdout);
    fflush(stderr);
    log_flush();
    log_cleanup();
    abort(); // abort so we get a core-dump
}

void error_clear() {
    error_pending = ERROR_NONE;
    error_type = ERROR_NONE;
    first_attr = NULL;
}

enum error_type error_check() {
    return error_type;
}

void error_add_attr(struct error_attr *attr) {
    if (first_attr)
        first_attr->pprev = &attr->next;
    attr->next = first_attr;
    first_attr = attr;
}

void error_print() {
    LOG_ERROR("ERROR: %s\n", error_type_string(error_type));

    struct error_attr *curs = first_attr;
    while (curs) {
        print_attr(curs);

        curs = curs->next;
    }
}

static void print_attr(struct error_attr const *attr) {
    switch (attr->tp) {
    case ERROR_TYPE_STRING:
        LOG_ERROR("[%s] = \"%s\"\n", attr->attr_name, attr->val.as_str);
        break;
    case ERROR_TYPE_INT:
        LOG_ERROR("[%s] = %d\n", attr->attr_name, attr->val.as_int);
        break;
    case ERROR_TYPE_U32:
        LOG_ERROR("[%s] = %x\n", attr->attr_name, (int)attr->val.as_u32);
        break;
    case ERROR_TYPE_U64:
        LOG_ERROR("[%s] = %llx\n", attr->attr_name,
                  (long long)attr->val.as_u32);
        break;
    default:
        break;
    }
}

static char const *error_type_string(enum error_type tp) {
    switch (tp) {
    case ERROR_NONE:
        return "no error";
    case ERROR_UNIMPLEMENTED:
        return "unable to continue due to unimplemented functionality";
    case ERROR_INVALID_PARAM:
        return "invalid parameter value";
    case ERROR_MEM_OUT_OF_BOUNDS:
        return "memory access failed because the address was out-of-bounds";
    case ERROR_FAILED_ALLOC:
        return "unable to allocate memory";
    case ERROR_FILE_IO:
        return "error on some file operation";
    case ERROR_UNKNOWN_EXCP_CODE:
        return "sh4 interpreter encountered an unknown exception code";
    case ERROR_INTEGRITY:
        return "something that *should* be impossible just happened";
    case ERROR_INVALID_FILE_LEN:
        return "incorrect file length";
    case ERROR_TOO_BIG:
        return "some parameter is beyond the maximum allowed limits";
    case ERROR_TOO_SMALL:
        return "some parameter is below the minimum allowed limits";
    case ERROR_DUPLICATE_DATA:
        return "some parameter was provided two or more times";
    case ERROR_MISSING_DATA:
        return "some mandatory parameter was not provided";
    case ERROR_OVERFLOW:
        return "out of buffer space";
    case ERROR_EXT_FAILURE:
        return "a failure occurred in a component WashingtonDC depends upon";
    default:
        return "Unknown error (this shouldn\'t happen)";
    }
}

void error_add_callback(struct error_callback *cb) {
    fifo_push(&err_callbacks, &cb->node);
}

void error_rm_callback(struct error_callback *cb) {
    fifo_erase(&err_callbacks, &cb->node);
}

enum error_type get_error_pending(void) {
    return error_pending;
}

void set_error_pending(enum error_type tp) {
    error_pending = tp;
}

DEF_ERROR_INT_ATTR(line)
DEF_ERROR_STRING_ATTR(file)
DEF_ERROR_INT_ATTR(pending_error_line)
DEF_ERROR_STRING_ATTR(pending_error_file)
DEF_ERROR_STRING_ATTR(feature)
DEF_ERROR_STRING_ATTR(param_name)
DEF_ERROR_U32_ATTR(address)
DEF_ERROR_INT_ATTR(length)
DEF_ERROR_U32_ATTR(value)
DEF_ERROR_INT_ATTR(errno_val)
DEF_ERROR_U32_ATTR(expected_length)
DEF_ERROR_STRING_ATTR(wtf)
DEF_ERROR_STRING_ATTR(advice)
DEF_ERROR_STRING_ATTR(file_path)
DEF_ERROR_INT_ATTR(max_val)
DEF_ERROR_STRING_ATTR(function)
DEF_ERROR_STRING_ATTR(pending_error_function)
DEF_ERROR_INT_ATTR(index)
