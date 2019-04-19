/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2019 snickerbockers
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

#include <stdlib.h>
#include <stdio.h>

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

__attribute__((__noreturn__))
void error_raise(enum error_type tp) {
    error_type = tp;

    struct fifo_node *cursor;

    FIFO_FOREACH(err_callbacks, cursor) {
        struct error_callback *cb =
            &FIFO_DEREF(cursor, struct error_callback, node);

        cb->callback_fn(cb->arg);
    }

    dc_print_perf_stats();

    error_print();
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
