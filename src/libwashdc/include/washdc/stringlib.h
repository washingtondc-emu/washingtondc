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

#ifndef STRINGLIB_H_
#define STRINGLIB_H_

/*
 * high-level string implementaiton.
 * this takes care of memory management and delivers
 * functionality similar to what you'd get from the string types
 * found in most other high-level languages.
 */

#include <stdbool.h>
#include <stddef.h>

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
};

/*
 * initialize the string
 * the string must not be already initialized
 */
void string_init(struct string *str);

/*
 * initialize the string  with txt.
 * You still have to call string_cleanup after this
 */
void string_init_txt(struct string *str, char const *txt);

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
 * set the contents of string from the given file.
 *
 * the string must have been previously initialized via string_init.
 *
 * the file position after calling this function is undefined
 */
void string_load_stdio(struct string *str, FILE *fp);

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

/*
 * tokenizer API.
 *
 * first you call string_tok_begin to initialize your cursor
 * after that, you loop calling string_tok_next to get each
 * token until eventually it returns false.
 */
struct string_curs {
    size_t next_idx;
};
void string_tok_begin(struct string_curs *curs);

/*
 * get the next token from str (if there is a next token) and store it in tok.
 * tok must already be initialized by string_init.  curs must already be
 * initialized by string_tok_begin.  curs will be advanced to point to the next
 * token.
 *
 * If there are no more tokens, then this function will return false and the
 * contents of tok will be left unchanged.  curs will not be advanced in this
 * case.
 *
 * any one of the characters in delim will be considered a token.
 *
 * For the sake of this function, a token is defined as a substring with a
 * delimiter on either side.  This means that if the first character is a
 * delimiter or if there are two delimiters in a row, this function will return
 * an empty string ,so be aware of that.  This also means that if there are no
 * delimiters, then this function will return a single string.
 *
 * The delimters are never included in the strings returned by this function
 */
bool string_tok_next(struct string *tok,
                     struct string_curs *curs,
                     char const *str,
                     char const *delim);

/*
 * clear dst then copy len chars from src to dst.
 *
 * if first_idx or last_idx is less than 0, the result will be the same as if
 * it was 0.
 *
 * if first_idx or last_idx is greater than or equal to string_length,
 * the returned substring will be empty.
 *
 * if last_idx is less than 0, the returned substring will be empty.
 *
 * if last_idx is greater than (string_length - 1), the result will be the
 * same as if it was (string_length - 1).
 */
void string_substr(struct string *dst, struct string const *src,
                   int first_idx, int last_idx);

/*
 * returns the nth grouping of characters separated by the characters in delim.
 *
 * Although this function may seem like it serves a similar purpose to
 * string_tok_next, the behavior is slightly different because this function is
 * designed to fill a different niche.  This function will never return an
 * empty string (unless there's an error, but then the returned string is
 * considered invalid anyways), and it will treat any leading or trailing
 * delimiter characters as if they aren't there; the first column begins with
 * the first non-delim character and the last column ends with the last non
 * delim character.
 *
 * Additionally, this function will not split a token which is bound by
 * double-quotes (") characters.
 *
 * the return value of this function is 0 on success and non-zero on failure.
 * a failure means that the number of columns in the source string is less than
 * or equal to col_no.
 */
int string_get_col(struct string *dst, struct string const *src,
                   unsigned col_no, char const *delim);

/*
 * returns the index of the first instance of *any* part of delim in src.
 *
 * if no such characters could be found, this function returns -1.
 */
int string_find_first_of(struct string const *src, char const *delim);

/*
 * returns the index of the last instance of *any* part of delim in src.
 *
 * if no such characters could be found, this function returns -1.
 */
int string_find_last_of(struct string const *src, char const *delim);

/*
 * return true if the first n_chars if str match the first n_chars of cmp;
 * else return false.
 *
 * if one or both strings are less than n_chars and they are not equal length,
 * then this function returns false.
 *
 * if both strings are less than n_chars and they are of equal length, this
 * function will return true if they match eatch other and false otherwise.
 */
bool string_eq_n(struct string const *str, char const *cmp, int n_chars);

// append the given value to str as an 8-digit hexadecimal value.
void string_append_hex32(struct string *str, uint32_t val);

/*
 * read a 32-bit int from str starting at start_idx.  This function will stop
 * reading after it has encountered a non-hex character, after it is reached
 * the end of the string, or after it has read 8 hex-chars.
 */
uint32_t string_read_hex32(struct string const *str, int first_idx);

// dst must be initialized prior to calling this
void string_dirname(struct string *dst, char const *input);

#ifdef __cplusplus
}
#endif

#endif
