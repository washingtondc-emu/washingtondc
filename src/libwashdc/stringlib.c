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
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <libgen.h>

#include "washdc/error.h"

#include "washdc/stringlib.h"

static DEF_ERROR_INT_ATTR(character);

static bool is_hex_digit(char c);
static unsigned get_hex_val(char c);

void string_init(struct string *str) {
    memset(str, 0, sizeof(*str));
}

void string_init_txt(struct string *str, char const *txt) {
    string_init(str);
    string_set(str, txt);
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
    } else {
        if (str->c_str)
            free(str->c_str);
        memset(str, 0, sizeof(*str));
    }
}

void string_load_stdio(struct string *str, FILE *fp) {
    long file_sz, buf_sz;

    if (fseek(fp, 0, SEEK_END) != 0) {
        error_set_errno_val(errno);
        RAISE_ERROR(ERROR_FILE_IO);
    }

    if ((file_sz = ftell(fp)) < 0) {
        error_set_errno_val(errno);
        RAISE_ERROR(ERROR_FILE_IO);
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        error_set_errno_val(errno);
        RAISE_ERROR(ERROR_FILE_IO);
    }

    buf_sz = file_sz + 1;
    str->c_str = (char*)realloc(str->c_str, sizeof(char) * buf_sz);
    if (!str->c_str) {
        error_set_errno_val(errno);
        RAISE_ERROR(ERROR_FAILED_ALLOC);
    }

    if (fread(str->c_str, sizeof(char), file_sz, fp) != file_sz) {
        error_set_errno_val(errno);
        RAISE_ERROR(ERROR_FILE_IO);
    }
    str->c_str[buf_sz - 1] = '\0';
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
    assert(src != dst);
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
}

void string_append_char(struct string *dst, char ch) {
    char tmp[2];

    tmp[0] = ch;
    tmp[1] = '\0';

    string_append(dst, tmp);
}

void string_tok_begin(struct string_curs *curs) {
    curs->next_idx = 0;
}

bool string_tok_next(struct string *tok,
                     struct string_curs *curs,
                     char const *str,
                     char const *delim) {
    size_t pos = curs->next_idx;

    if (!str || !str[curs->next_idx])
        return false;

    string_set(tok, "");

    while (str[pos]) {
        char const *delim_p = delim;

        while (*delim_p) {
            if (*delim_p == str[pos]) {
                // end of token
                curs->next_idx = pos + 1;
                return true;
            }
            delim_p++;
        }

        string_append_char(tok, str[pos]);
        pos++;
    }

    // end of string
    curs->next_idx = pos;
    return true;
}

void string_substr(struct string *dst, struct string const *src,
                   int first_idx, int last_idx) {
    assert(src != dst);

    int src_len = (int)string_length(src);

    string_set(dst, "");

    if (!src_len)
        return;

    if (first_idx < 0)
        first_idx = 0;
    if (last_idx < 0)
        return;
    if (first_idx >= src_len)
        return;
    if (last_idx >= src_len)
        last_idx = src_len - 1;

    char const *src_c_str = string_get(src);

    string_set(dst, "");

    int idx = first_idx;
    while (idx <= last_idx) {
        string_append_char(dst, src_c_str[idx]);
        idx++;
    }
}

int string_find_first_of(struct string const *src, char const *delim) {
    char const *str_beg = string_get(src);
    char const *str = str_beg;

    while (*str) {
        char const *delim_cur = delim;
        while (*delim_cur) {
            if (*str == *delim_cur)
                return str - str_beg;
            delim_cur++;
        }
        str++;
    }

    return -1;
}

int string_find_last_of(struct string const *src, char const *delim) {
    char const *str_beg = string_get(src);
    char const *str = str_beg + (strlen(str_beg) - 1);

    if (!strlen(str_beg))
        return -1;

    do {
        char const *delim_cur = delim;
        while (*delim_cur) {
            if (*str == *delim_cur)
                return str - str_beg;
            delim_cur++;
        }
    } while (str-- != str_beg);

    return -1;
}

bool string_eq_n(struct string const *str, char const *cmp, int n_chars) {
    char const *c_str = string_get(str);

    int idx = 0;

    while ((*c_str && *cmp) && (idx < n_chars)) {
        if (*c_str != *cmp)
            return false;

        c_str++;
        cmp++;
        idx++;
    }

    if (idx == n_chars)
        return true;

    // we got to the end of one or both strings before reaching the nth char
    return *c_str == *cmp;
}

void string_append_hex32(struct string *str, uint32_t val) {
    static const char hex_tbl[16] = {
        '0', '1', '2', '3',
        '4', '5', '6', '7',
        '8', '9', 'a', 'b',
        'c', 'd', 'e', 'f'
    };

    unsigned digit;
    for (digit = 0; digit < 8; digit++) {
        unsigned shift = 4 * (7 - digit);
        uint32_t mask = 0xf << shift;

        string_append_char(str, hex_tbl[(mask & val) >> shift]);
    }
}

static bool is_hex_digit(char c) {
    return (c >= 'a' && c <= 'f') ||
        (c >= 'A' && c <= 'F') ||
        (c >= '0' && c <= '9');
}

static unsigned get_hex_val(char c) {
    if (c >= 'a' && c <= 'f')
        return (c - 'a') + 10;
    if (c >= 'A' && c <= 'F')
        return (c - 'A') + 10;
    if (c >= '0' && c <= '9')
        return c - '0';

    error_set_character((int)c);
    RAISE_ERROR(ERROR_INVALID_PARAM);
    return 15; // never happens
}

/*
 * read a 32-bit hex-int from str starting at start_idx.  This function will stop
 * reading after it has encountered a non-hex character, after it is reached
 * the end of the string, or after it has read 8 hex-chars.
 *
 * the hex-int is expected to be in big-endian format.  The uint32_t returned
 * will be in host byte order.
 *
 * if str is empty, then this function will return 0.
 */
uint32_t string_read_hex32(struct string const *str, int start_idx) {
    int n_bytes = 0;
    int idx = start_idx;
    char const *str_ptr = string_get(str);
    uint32_t val = 0;
    char dat[8];

    if (idx >= strlen(str_ptr))
        return 0;

    /*
     * is_hex_digit will catch the NULL terminator for us in addtion to any
     * non-hex characters.
     */
    while (n_bytes < 8 && is_hex_digit(*str_ptr))
        dat[n_bytes++] = get_hex_val(*str_ptr++);

    for (idx = 0; idx < n_bytes; idx++)
        val = (val << 4) | dat[idx];

    return val;
}

static bool check_char_class(char c, char const *class) {
    while (*class)
        if (c == *class++)
            return true;
    return false;
}

int string_get_col(struct string *dst, struct string const *src,
                   unsigned col_no, char const *delim) {
    unsigned cur_col = 0;
    char const *strp = string_get(src);

#ifdef INVARIANTS
    /*
     * make sure the caller didn't accidentally make quotes a delimiter - this
     * would not work out because we use the quotes character as a way to
     * enclose columns which might contain delimiters that should not separate
     * them.
     */
    char const *delim_curs = delim;
    while (*delim_curs)
        if (*delim_curs++ == '"')
            RAISE_ERROR(ERROR_INTEGRITY);
#endif

    for (;;) {
        // advance to the beginning of the column
        while (check_char_class(*strp, delim) && *strp)
            strp++;
        if (!*strp)
            return -1;

        if (cur_col == col_no)
            break;

        if (*strp == '"') {
            /*
             * columns that have quotes around them are delimited by the second
             * quote character rather than one of the delimters.  Reicast's gdi
             * code considers any filename enclosed in quotes to be a single
             * entity regardless of whatever whitespace is inside of it, and
             * WashingtonDC needs to follow the same conventions that other
             * emulators follow.
             *
             * Currently the gdi code is the only user of this function.  If
             * anything else needs this function in the future, then it might
             * be best to make this feature optional or alternatively allow the
             * caller to replace the quotes with a diffferent "grouping"
             * character.
             */
            do {
                strp++;
            } while (*strp && *strp != '"');
            if (!*strp)
                return -1;
            strp++; // make it point to the first char *after* the quote
        } else {
            // advance to the end of the column
            while (!check_char_class(*strp, delim) && *strp)
                strp++;
            if (!*strp)
                return -1;
        }

        cur_col++;
    }

    // at this point, strp points to the beginning of the column
    string_set(dst, "");

    if (*strp == '"') {
        // handle quotes
        if (strp[1] == '"' || !strp[1])
            return -1; // no empty quotes or unbound allowed

        // Check to make sure the quotes end *before* making any changes to dst
        char const *last;
        char const *first = strp + 1;
        do {
            last = strp;
            strp++;
        } while (*strp && *strp != '"');

        if (!*strp)
            return -1; // unbound quotes

        while (first <= last)
            string_append_char(dst, *first++);
    } else {
        while (*strp && !check_char_class(*strp, delim))
            string_append_char(dst, *strp++);
    }

    return 0;
}

void string_dirname(struct string *dst, char const *input) {
    /*
     * this function is kinda wordy because the posix standard for dirname is
     * so wonky.  dirname *might* edit the string you send it is a parameter,
     * so we have to make a copy before we call dirname, and it *might* return
     * that modified string or it *might* return a pointer to some static
     * memory, so we have to immediately make another copy.
     *
     * And don't get me started on basename, on GNU platforms that one actually
     * changes behavior depending on whether or not libgen.h was included.
     *
     * (/rant)
     */
    char *input_tmp = strdup(input);

    if (!input_tmp)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    if (dst->c_str)
        free(dst->c_str);

    dst->c_str = strdup(dirname(input_tmp));

    free(input_tmp);
}
