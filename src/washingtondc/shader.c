/*******************************************************************************
 *
 * Copyright 2017, 2019, 2020 snickerbockers
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

#ifdef _WIN32
#include "i_hate_windows.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#include <GL/gl.h>

#include "shader.h"

static char *read_txt(char const *path);

#define LOG_LEN_GLSL 1024
GLchar shader_log[LOG_LEN_GLSL];

void
shader_load_vert(struct shader *out, char const *verstr,
                 char const *vert_shader_src) {
    shader_load_vert_with_preamble(out, verstr, vert_shader_src, NULL);
}

void
shader_load_frag(struct shader *out, char const *verstr,
                 char const *frag_shader_src) {
    shader_load_frag_with_preamble(out, verstr, frag_shader_src, NULL);
}

void shader_load_vert_with_preamble(struct shader *out,
                                    char const *verstr,
                                    char const *vert_shader_src,
                                    char const *preamble) {
    int n_shader_strings;
    char const *shader_strings[3];

    if (preamble) {
        shader_strings[0] = verstr;
        shader_strings[1] = preamble;
        shader_strings[2] = vert_shader_src;
        n_shader_strings = 3;
    } else {
        shader_strings[0] = verstr;
        shader_strings[1] = vert_shader_src;
        shader_strings[2] = NULL;
        n_shader_strings = 2;
    }

    GLuint vert_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert_shader, n_shader_strings, shader_strings, NULL);
    glCompileShader(vert_shader);

    GLint shader_success;
    glGetShaderiv(vert_shader, GL_COMPILE_STATUS, &shader_success);
    if (!shader_success) {
        glGetShaderInfoLog(vert_shader, LOG_LEN_GLSL, NULL, shader_log);

        glDeleteShader(vert_shader);

        fprintf(stderr, "Error compiling vertex shader: %s\n", shader_log);
        exit(1);
    }

    out->vert_shader = vert_shader;
}

void shader_load_frag_with_preamble(struct shader *out,
                                    char const *verstr,
                                    char const *frag_shader_src,
                                    char const *preamble) {
    int n_shader_strings;
    char const *shader_strings[3];

    if (preamble) {
        shader_strings[0] = verstr;
        shader_strings[1] = preamble;
        shader_strings[2] = frag_shader_src;
        n_shader_strings = 3;
    } else {
        shader_strings[0] = verstr;
        shader_strings[1] = frag_shader_src;
        n_shader_strings = 2;
    }

    GLuint frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag_shader, n_shader_strings, shader_strings, NULL);
    glCompileShader(frag_shader);

    GLint shader_success;
    glGetShaderiv(frag_shader, GL_COMPILE_STATUS, &shader_success);
    if (!shader_success) {
        glGetShaderInfoLog(frag_shader, LOG_LEN_GLSL, NULL, shader_log);

        fprintf(stderr, "Error compiling fragment shader: %s\n", shader_log);
        exit(1);
    }

    out->frag_shader = frag_shader;
}

void shader_load_vert_from_file_with_preamble(struct shader *out,
                                              char const *verstr,
                                              char const *vert_shader_path,
                                              char const *preamble) {
    char *vert_shader_src;

    vert_shader_src = read_txt(vert_shader_path);

    shader_load_vert_with_preamble(out, verstr, vert_shader_src, preamble);

    free(vert_shader_src);
}

void shader_load_frag_from_file_with_preamble(struct shader *out,
                                              char const *verstr,
                                              char const *frag_shader_path,
                                              char const *preamble) {
    char *frag_shader_src;

    frag_shader_src = read_txt(frag_shader_path);

    shader_load_frag_with_preamble(out, verstr, frag_shader_src, preamble);

    free(frag_shader_src);
}

void shader_load_vert_from_file(struct shader *out, char const *verstr,
                                char const *vert_shader_path) {
    shader_load_vert_from_file_with_preamble(out, verstr, vert_shader_path, NULL);
}

void shader_load_frag_from_file(struct shader *out, char const *verstr,
                                char const *frag_shader_path) {
    shader_load_frag_from_file_with_preamble(out, verstr, frag_shader_path, NULL);
}

void shader_link(struct shader *out) {
    GLuint shader_obj = glCreateProgram();
    glAttachShader(shader_obj, out->vert_shader);
    glAttachShader(shader_obj, out->frag_shader);
    glLinkProgram(shader_obj);

    GLint shader_success;
    glGetProgramiv(shader_obj, GL_LINK_STATUS, &shader_success);
    if (!shader_success) {
        glGetProgramInfoLog(shader_obj, LOG_LEN_GLSL, NULL, shader_log);

        glDeleteShader(out->vert_shader);
        glDeleteShader(out->frag_shader);
        glDeleteProgram(shader_obj);

        out->vert_shader = out->frag_shader = 0;

        fprintf(stderr, "Error compiling shader: %s\n", shader_log);
        exit(1);
    }

    out->shader_prog_obj = shader_obj;
}

static char *read_txt(char const *path) {
    FILE *txt_fp;
    char *src;
    long src_len;

    if (!(txt_fp = fopen(path, "r"))) {
        fprintf(stderr, "Unable to open \"%s\": %s\n", path, strerror(errno));;
        exit(1);
    }

    if (fseek(txt_fp, 0, SEEK_END) < 0) {
        fprintf(stderr, "unable to seek \"%s\": %s\n", path, strerror(errno));
        exit(1);
    }
    if ((src_len = ftell(txt_fp)) < 0) {
        fprintf(stderr, "unable to obtain length of \"%s\": %s\n", path, strerror(errno));
        exit(1);
    }
    if (fseek(txt_fp, 0, SEEK_SET) < 0) {
        fprintf(stderr, "unable to seek \"%s\": %s\n", path, strerror(errno));
        exit(1);
    }

    src = (char*)malloc(sizeof(char) * (1 + src_len));
    if (fread(src, sizeof(char),
              src_len, txt_fp) != src_len) {
        fprintf(stderr, "unable to read from \"%s\": %s\n",
                path, strerror(errno));
        exit(1);
    }
    src[src_len - 1] = '\0';

    fclose(txt_fp);

    return src;
}

void shader_cleanup(struct shader *shader) {
    glDeleteProgram(shader->shader_prog_obj);
    glDeleteShader(shader->frag_shader);
    glDeleteShader(shader->vert_shader);

    memset(shader, 0, sizeof(*shader));
}
