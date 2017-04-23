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

#ifndef ERROR_H_
#define ERROR_H_

/* this is not thread safe - only the emulation thread can use it for now */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum error_type {
    ERROR_NONE = 0,

    /* unable to continue due to unimplemented functionality */
    ERROR_UNIMPLEMENTED,

    /* invalid parameter */
    ERROR_INVALID_PARAM,

    /* memory access failed because the address was out-of-bounds */
    ERROR_MEM_OUT_OF_BOUNDS,
};

enum error_attr_type {
    ERROR_TYPE_STRING,
    ERROR_TYPE_INT,
    ERROR_TYPE_U32
};

union error_dat {
    char const *as_str;
    int as_int;
    uint32_t as_u32;
};

/*
 * error_attrs should always be global and/or static variables.
 * They should never be allocated on the stack or the heap.
 */
struct error_attr {
    char const *attr_name;

    enum error_attr_type tp;

    union error_dat val;

    struct error_attr *next;
    struct error_attr **pprev;
};

void error_raise(enum error_type tp);
enum error_type error_check();
void error_clear();

void error_add_attr(struct error_attr *attr);

void error_print();

#define ERROR_STRING_ATTR(the_attr_name)                \
    void error_set_##the_attr_name(char const *attr_val)

#define ERROR_INT_ATTR(the_attr_name)                   \
    void error_set_##the_attr_name(int attr_val)

#define ERROR_U32_ATTR(the_attr_name)                   \
    void error_set_##the_attr_name(uint32_t attr_val)

#define DEF_ERROR_STRING_ATTR(the_attr_name)            \
    ERROR_STRING_ATTR(the_attr_name) {                  \
        static struct error_attr attr;                  \
                                                        \
        attr.attr_name = #the_attr_name;                \
        attr.tp = ERROR_TYPE_STRING;                    \
        attr.val.as_str = attr_val;                     \
                                                        \
        error_add_attr(&attr);                          \
    }                                                   \

#define DEF_ERROR_INT_ATTR(the_attr_name)               \
    ERROR_INT_ATTR(the_attr_name) {                     \
        static struct error_attr attr;                  \
                                                        \
        attr.attr_name = #the_attr_name;                \
        attr.tp = ERROR_TYPE_INT;                       \
        attr.val.as_int = attr_val;                     \
                                                        \
        error_add_attr(&attr);                          \
    }                                                   \

#define DEF_ERROR_U32_ATTR(the_attr_name)               \
    ERROR_U32_ATTR(the_attr_name) {                     \
        static struct error_attr attr;                  \
                                                        \
        attr.attr_name = #the_attr_name;                \
        attr.tp = ERROR_TYPE_INT;                       \
        attr.val.as_u32 = attr_val;                     \
                                                        \
        error_add_attr(&attr);                          \
    }                                                   \

ERROR_INT_ATTR(line);
ERROR_STRING_ATTR(file);

ERROR_STRING_ATTR(feature);
ERROR_STRING_ATTR(param_name);

ERROR_U32_ATTR(address);

ERROR_INT_ATTR(length);

#define RAISE_ERROR(tp)                         \
    do {                                        \
        error_set_line(__LINE__);               \
        error_set_file(__FILE__);               \
        error_raise(tp);                        \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif
