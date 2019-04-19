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

#ifndef ERROR_H_
#define ERROR_H_

/* this is not thread safe - only the emulation thread can use it for now */

#include <stdint.h>

#include "washdc/fifo.h"

enum error_type {
    ERROR_NONE = 0,

    /* unable to continue due to unimplemented functionality */
    ERROR_UNIMPLEMENTED,

    /* invalid parameter */
    ERROR_INVALID_PARAM,

    /* memory access failed because the address was out-of-bounds */
    ERROR_MEM_OUT_OF_BOUNDS,

    /* unable to allocate memory */
    ERROR_FAILED_ALLOC,

    /* error on some file operation */
    ERROR_FILE_IO,

    /* sh4 interpreter encountered an unknown exception code */
    ERROR_UNKNOWN_EXCP_CODE,

    /* shouldn't be possible ? */
    ERROR_INTEGRITY,

    /* some parameter is beyond the maximum allowed limits */
    ERROR_TOO_BIG,

    /* some parameter is below the minimum allowed limits */
    ERROR_TOO_SMALL,

    /* some parameter was provided two or more times */
    ERROR_DUPLICATE_DATA,

    /* some mandatory parameter was not provided */
    ERROR_MISSING_DATA,

    /* more data than we can handle */
    ERROR_OVERFLOW,

    /* something beyond my control (library, system call, etc) failed. */
    ERROR_EXT_FAILURE,

    ERROR_INVALID_FILE_LEN
};

enum error_attr_type {
    ERROR_TYPE_STRING,
    ERROR_TYPE_INT,
    ERROR_TYPE_U32,
    ERROR_TYPE_U64
};

union error_dat {
    char const *as_str;
    int as_int;
    uint32_t as_u32;
    uint64_t as_u64;
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

/*
 * somehow using the C11 noreturn specifier here is breaking the C++ testing
 * code over in unit_tests/ even though that's SUPPOSED to work since it's
 * built using the C++11 standard which SHOULD be compatible with C11.  So I
 * have to go with the non-standard gcc __attribute__ extension instead.
 * Whatever.
 */
__attribute__((__noreturn__)) void error_raise(enum error_type tp);
enum error_type error_check();
void error_clear();

void error_add_attr(struct error_attr *attr);

void error_print();

/*
 * some subsystems will set error attributes but not call RAISE_ERROR if
 * there's a problem.  This is mostly for the benefit of the debugger, because
 * we don't want WashingtonDC to crash solely because the user punched in a bad
 * memory address or something.
 *
 * the pending error is used by functions within subsystems to store what
 * error should be raised if the caller cannot handle the error.
 *
 * If a function call returns that an error happened, and the caller cannot
 * handle the error, it should call RAISE_ERROR(get_error_pending())
 *
 * OR
 *
 * it can pass the buck along to its caller by returning some status code so
 * that it will know there's a recommended error pending.
 *
 * OR
 *
 * it can handle the error and then call error_clear so that the reccomended
 * error code and all of the attributes are unset.
 *
 * Note that it is not safe for a function to set new error attributes when
 * there is a recommended error pending.  This is because the error-handling
 * code is not smart enough to detect when an attribute is already set, so by
 * setting an attribute while an error is pending you risk setting the same
 * attribute twice, which corrupts the error attribute list.
 */
enum error_type get_error_pending(void);
void set_error_pending(enum error_type tp);

/*
 * error callbacks are invoked at the beginning of error processing to set
 * attributes.  They are not supposed to attempt to handle the error in any way.
 */
struct error_callback {
    void(*callback_fn)(void *arg);
    void *arg;

    struct fifo_node node;
};

void error_add_callback(struct error_callback *cb);
void error_rm_callback(struct error_callback *cb);

#define ERROR_STRING_ATTR(the_attr_name)                                \
    __attribute__((unused)) void                                        \
    error_set_##the_attr_name(char const *attr_val)

#define ERROR_INT_ATTR(the_attr_name)                   \
    __attribute__((unused)) void                        \
    error_set_##the_attr_name(int attr_val)

#define ERROR_U32_ATTR(the_attr_name)                   \
    __attribute__((unused)) void                        \
    error_set_##the_attr_name(uint32_t attr_val)

#define ERROR_U64_ATTR(the_attr_name)                   \
    __attribute__((unused)) void                        \
    error_set_##the_attr_name(uint64_t attr_val)

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
        attr.tp = ERROR_TYPE_U32;                       \
        attr.val.as_u32 = attr_val;                     \
                                                        \
        error_add_attr(&attr);                          \
    }                                                   \

#define DEF_ERROR_U64_ATTR(the_attr_name)               \
    ERROR_U64_ATTR(the_attr_name) {                     \
        static struct error_attr attr;                  \
                                                        \
        attr.attr_name = #the_attr_name;                \
        attr.tp = ERROR_TYPE_INT;                       \
        attr.val.as_u64 = attr_val;                     \
                                                        \
        error_add_attr(&attr);                          \
    }                                                   \

ERROR_INT_ATTR(line);
ERROR_STRING_ATTR(file);
ERROR_STRING_ATTR(function);

ERROR_INT_ATTR(pending_error_line);
ERROR_STRING_ATTR(pending_error_file);
ERROR_STRING_ATTR(pending_error_function);

ERROR_STRING_ATTR(feature);
ERROR_STRING_ATTR(param_name);

ERROR_U32_ATTR(address);

ERROR_INT_ATTR(length);

ERROR_U32_ATTR(value);

ERROR_INT_ATTR(errno_val);

ERROR_U32_ATTR(expected_length);

ERROR_STRING_ATTR(wtf);

ERROR_STRING_ATTR(advice);

ERROR_STRING_ATTR(file_path);

ERROR_INT_ATTR(max_val);

ERROR_INT_ATTR(index);

#define RAISE_ERROR(tp)                         \
    do {                                        \
        error_set_line(__LINE__);               \
        error_set_file(__FILE__);               \
        error_set_function(__func__);           \
        error_raise(tp);                        \
    } while (0)

#define PENDING_ERROR(tp)                               \
    do {                                                \
        error_set_pending_error_line(__LINE__);         \
        error_set_pending_error_file(__FILE__);         \
        error_set_pending_error_function(__func__);     \
        set_error_pending(tp);                          \
    } while (0)

#endif
