/*******************************************************************************
 *
 * Copyright 2017, 2019 snickerbockers
 * snickerbockers@washemu.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
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

#endif
