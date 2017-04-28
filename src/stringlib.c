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
#include <string.h>

#include "error.h"

#include "stringlib.h"

void string_init(struct string *str) {
    size_t len;
    
    memset(str, 0, sizeof(*str));
}

void string_cleanup(struct string *str) {
    if (str->c_str)
        free(str->c_str);

    memset(str, 0, sizeof(*str));
}

void string_set(struct string *str, char const *txt) {
    size_t len;

    if (txt && (len = strlen(txt))) {
        size_t bytes_alloc = len + 1;
        str->c_str = (char*)realloc(str->c_str, sizeof(char) * bytes_alloc);

        if (!str->c_str)
            RAISE_ERROR(ERROR_FAILED_ALLOC);

        strcpy(str->c_str, txt);
        str->alloc = bytes_alloc;
    } else {
        if (str->c_str)
            free(str->c_str);
        memset(str, 0, sizeof(*str));
    }
}

size_t string_length(struct string const *str) {
    if (str->c_str)
        return strlen(str->c_str);
    return 0;
}

char const *string_get(struct string const *str) {
    if (str->c_str)
        return str->c_str;
    return "";
}

void string_copy(struct string *dst, struct string const *src) {
    // lazy implementation -_-
    string_set(dst, string_get(src));
}

void string_append(struct string *dst, char const *src) {
    size_t src_len = strlen(src);
    size_t dst_len = string_length(dst);
    size_t new_alloc_sz = src_len + dst_len + 1;

    char *new_alloc = (char*)malloc(new_alloc_sz * sizeof(char));
    if (!new_alloc)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    if (dst->c_str && dst_len)
        strcpy(new_alloc, dst->c_str);
    strcpy(new_alloc + dst_len, src);

    if (dst->c_str)
        free(dst->c_str);

    dst->c_str = new_alloc;
    dst->alloc = new_alloc_sz;
}

void string_append_char(struct string *dst, char ch) {
    char tmp[2];

    tmp[0] = ch;
    tmp[1] = '\0';

    string_append(dst, tmp);
}
