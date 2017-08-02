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

#include <GL/gl.h>

#ifdef __cplusplus
extern "C" {
#endif

struct shader {
    GLuint vert_shader;
    GLuint frag_shader;
    GLuint shader_prog_obj;
};

/*
 * the preamble is a string that will get prepended to the beginning of the
 * shader.  The intended purpose of this is to define preprocessor macros.
 *
 * it is safe to send NULL if no preamble is needed.
 */
void shader_load_vert_with_preamble(struct shader *out,
                                    char const *vert_shader_src,
                                    char const *preamble);
void shader_load_frag_with_preamble(struct shader *out,
                                    char const *frag_shader_src,
                                    char const *preamble);

/*
 * In these versions, the preamble is expected to be a string, not a path to
 * a file.
 */
void shader_load_vert_from_file_with_preamble(struct shader *out,
                                              char const *vert_shader_path,
                                              char const *preamble);
void shader_load_frag_from_file_with_preamble(struct shader *out,
                                              char const *frag_shader_path,
                                              char const *preamble);

/*
 * these functions are equivalent to calling the _with_preamble versions with
 * a NULL preamble (ie no preamble).
 */
void shader_load_vert_from_file(struct shader *out, char const *vert_shader_path);
void shader_load_frag_from_file(struct shader *out, char const *frag_shader_path);
void shader_load_vert(struct shader *out, char const *vert_shader_src);
void shader_load_frag(struct shader *out, char const *frag_shader_src);

void shader_link(struct shader *out);

void shader_cleanup(struct shader *shader);

#ifdef __cplusplus
}
#endif
