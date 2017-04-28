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

#ifndef STRINGLIB_H_
#define STRINGLIB_H_

/*
 * high-level string implementaiton.
 * this takes care of memory management and delivers
 * functionality similar to what you'd get from the string types
 * found in most other high-level languages.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct string {
    /*
     * this is allocated on the heap.  It is null-terminated.
     *
     * If the string is empty, then c_str will be NULL.
     * if you want a string that behaves like a normal c string even when it's
     * empty, then call string_get
     */
    char *c_str;

    // this is the number of bytes allocated in c_str.
    // for the actual string length, call string_length.
    size_t alloc;
};

/*
 * initialize the string with txt.
 * the string must not be already initialized
 */
void string_init(struct string *str);

// release resources allocated to the string
void string_cleanup(struct string *str);

/*
 * set the string to contain txt.
 *
 * the string must have been previously initialized via
 * string_init
 */
void string_set(struct string *str, char const *txt);

/*
 * copy src into dst.
 *
 * dst must have been initialized by string_init prior to calling this function
 */
void string_copy(struct string *dst, struct string const *src);

/*
 * return the length of the given string.
 *
 * like strlen, this function does not count the NULL terminator
 */
size_t string_length(struct string const *str);

void string_append(struct string *dst, char const *src);

void string_append_char(struct string *dst, char ch);

/*
 * returns a c-style representation of the str.
 * the pointer returned is never NULL, even if str is empty
 */
char const *string_get(struct string const *str);

#ifdef __cplusplus
}
#endif

#endif
