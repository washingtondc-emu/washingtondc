/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2019, 2022 snickerbockers
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

#ifndef WASHDC_SHADER_H_
#define WASHDC_SHADER_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * intentionally avoid including OpenGL headers to make sure that this header
 * does not get included by code which is not OpenGL-specific.
 */

struct shader {
    GLuint vert_shader;
    GLuint frag_shader;
    GLuint shader_prog_obj;
};

#define SHADER_VER_330 "#version 330\n"
#define SHADER_VER_430 "#version 430\n"

/*
 * the preamble is a string or strings that will get prepended to the
 * beginning of the shader.  The intended purpose of this is to
 * define preprocessor macros.
 *
 * the last preamble should be NULL to signal the end.
 * if there are no preambles needed then just send NULL as the final parameter
 */
void shader_load_vert_with_preamble(struct shader *out,
                                    char const *verstr,
                                    char const *vert_shader_src,
                                    ... /* preambles go here */);
void shader_load_frag_with_preamble(struct shader *out,
                                    char const *verstr,
                                    char const *frag_shader_src,
                                    ... /* preambles go here */);

/*
 * In these versions, the preamble is expected to be a string, not a path to
 * a file.
 */
void shader_load_vert_from_file_with_preamble(struct shader *out,
                                              char const *verstr,
                                              char const *vert_shader_path,
                                              char const *preamble);
void shader_load_frag_from_file_with_preamble(struct shader *out,
                                              char const *verstr,
                                              char const *frag_shader_path,
                                              char const *preamble);

/*
 * these functions are equivalent to calling the _with_preamble versions with
 * a NULL preamble (ie no preamble).
 */
void shader_load_vert_from_file(struct shader *out, char const *verstr,
                                char const *vert_shader_path);
void shader_load_frag_from_file(struct shader *out, char const *verstr,
                                char const *frag_shader_path);
void shader_load_vert(struct shader *out, char const *verstr, char const *vert_shader_src);
void shader_load_frag(struct shader *out, char const *verstr, char const *frag_shader_src);

void shader_link(struct shader *out);

void shader_cleanup(struct shader *shader);

#ifdef __cplusplus
}
#endif

#endif
