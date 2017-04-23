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

#include <stdlib.h>
#include <stdio.h>

#include "error.h"

#ifdef __cplusplus
#error oh bother
#endif

static char const *error_type_string(enum error_type tp);
static void print_attr(struct error_attr const *attr);

#ifdef ENABLE_DEBUGGER
static error_handler_t error_handler;
static void *error_handler_arg;
#endif

enum error_type error_type;

struct error_attr *first_attr;

#ifdef ENABLE_DEBUGGER
void set_error_handler(error_handler_t handler) {
    error_handler = handler;
}
#endif

void error_raise(enum error_type tp) {
    error_type = tp;

#ifdef ENABLE_DEBUGGER
    if (error_handler) {
        error_handler(tp, error_handler_arg);
        return;
    }
#endif

    error_print();
    exit(1);
}

void error_clear() {
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
    fprintf(stderr, "ERROR: %s\n", error_type_string(error_type));

    struct error_attr *curs = first_attr;
    while (curs) {
        print_attr(curs);

        curs = curs->next;
    }
}

static void print_attr(struct error_attr const *attr) {
    switch (attr->tp) {
    case ERROR_TYPE_STRING:
        fprintf(stderr, "[%s] = \"%s\"\n", attr->attr_name, attr->val.as_str);
        break;
    case ERROR_TYPE_INT:
        fprintf(stderr, "[%s] = %d\n", attr->attr_name, attr->val.as_int);
        break;
    case ERROR_TYPE_U32:
        fprintf(stderr, "[%s] = %x\n", attr->attr_name, (int)attr->val.as_u32);
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
    default:
        return "Unknown error (this shouldn\'t happen)";
    }
}

DEF_ERROR_INT_ATTR(line)
DEF_ERROR_STRING_ATTR(file)
DEF_ERROR_STRING_ATTR(feature)
DEF_ERROR_STRING_ATTR(param_name)
DEF_ERROR_U32_ATTR(address)
DEF_ERROR_INT_ATTR(length)
