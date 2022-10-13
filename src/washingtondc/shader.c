/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2019, 2020, 2022 snickerbockers
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

#ifdef _WIN32
#include "i_hate_windows.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#include <GL/gl.h>

#include "washdc/error.h"

#include "shader.h"

static char *read_txt(char const *path);

#define LOG_LEN_GLSL 1024
GLchar shader_log[LOG_LEN_GLSL];

void
shader_load_vert(struct shader *out, char const *name,
                 char const *vert_shader_src) {
    shader_load_vert_with_preamble(out, name, vert_shader_src, NULL);
}

void
shader_load_frag(struct shader *out, char const *name,
                 char const *frag_shader_src) {
    shader_load_frag_with_preamble(out, name, frag_shader_src, NULL);
}

void shader_load_vert_with_preamble(struct shader *out,
                                    char const *name,
                                    char const *vert_shader_src,
                                    ...) {
#define MAX_SHADER_STRINGS 32
    va_list arg_ptr;
    int n_shader_strings = 0;
    char const *shader_strings[MAX_SHADER_STRINGS] = { };

    va_start(arg_ptr, vert_shader_src);
    char const *next_str;
    while ((next_str = va_arg(arg_ptr, char const *)))
        if (n_shader_strings < MAX_SHADER_STRINGS - 1)
            shader_strings[n_shader_strings++] = next_str;
        else
            RAISE_ERROR(ERROR_OVERFLOW);
    va_end(arg_ptr);

    shader_strings[n_shader_strings++] = vert_shader_src;

    GLuint vert_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert_shader, n_shader_strings, shader_strings, NULL);
    glCompileShader(vert_shader);

    GLint shader_success;
    glGetShaderiv(vert_shader, GL_COMPILE_STATUS, &shader_success);
    if (!shader_success) {
        glGetShaderInfoLog(vert_shader, LOG_LEN_GLSL, NULL, shader_log);

        glDeleteShader(vert_shader);

        fprintf(stderr, "Error compiling vertex shader \"%s\": %s\n",
                name, shader_log);
        exit(1);
    }

    if (out->vs_count >= SHADER_MAX)
        RAISE_ERROR(ERROR_OVERFLOW);
    out->vert_shader[out->vs_count++] = vert_shader;
}

void shader_load_frag_with_preamble(struct shader *out,
                                    char const *name,
                                    char const *frag_shader_src,
                                    ...) {
#define MAX_SHADER_STRINGS 32
    va_list arg_ptr;
    int n_shader_strings = 0;
    char const *shader_strings[MAX_SHADER_STRINGS] = { };

    va_start(arg_ptr, frag_shader_src);
    char const *next_str;
    while ((next_str = va_arg(arg_ptr, char const *)))
        if (n_shader_strings < MAX_SHADER_STRINGS - 1)
            shader_strings[n_shader_strings++] = next_str;
        else
            RAISE_ERROR(ERROR_OVERFLOW);
    va_end(arg_ptr);

    shader_strings[n_shader_strings++] = frag_shader_src;

    GLuint frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag_shader, n_shader_strings, shader_strings, NULL);
    glCompileShader(frag_shader);

    GLint shader_success;
    glGetShaderiv(frag_shader, GL_COMPILE_STATUS, &shader_success);
    if (!shader_success) {
        glGetShaderInfoLog(frag_shader, LOG_LEN_GLSL, NULL, shader_log);

        fprintf(stderr, "Error compiling fragment shader \"%s\": %s\n",
                name, shader_log);
        exit(1);
    }

    if (out->fs_count >= SHADER_MAX)
        RAISE_ERROR(ERROR_OVERFLOW);
    out->frag_shader[out->fs_count++] = frag_shader;
}

void shader_load_vert_from_file_with_preamble(struct shader *out,
                                              char const *name,
                                              char const *vert_shader_path,
                                              char const *preamble) {
    char *vert_shader_src;

    vert_shader_src = read_txt(vert_shader_path);

    shader_load_vert_with_preamble(out, name, vert_shader_src, preamble, NULL);

    free(vert_shader_src);
}

void shader_load_frag_from_file_with_preamble(struct shader *out,
                                              char const *name,
                                              char const *frag_shader_path,
                                              char const *preamble) {
    char *frag_shader_src;

    frag_shader_src = read_txt(frag_shader_path);

    shader_load_frag_with_preamble(out, name,
                                   frag_shader_src, preamble, NULL);

    free(frag_shader_src);
}

void shader_load_vert_from_file(struct shader *out, char const *name,
                                char const *vert_shader_path) {
    shader_load_vert_from_file_with_preamble(out, name, vert_shader_path, NULL);
}

void shader_load_frag_from_file(struct shader *out, char const *name,
                                char const *frag_shader_path) {
    shader_load_frag_from_file_with_preamble(out, name, frag_shader_path, NULL);
}

void shader_link(struct shader *out) {
    GLuint shader_obj = glCreateProgram();
    int idx;
    for (idx = 0; idx < out->vs_count; idx++)
        glAttachShader(shader_obj, out->vert_shader[idx]);
    for (idx = 0; idx < out->fs_count; idx++)
        glAttachShader(shader_obj, out->frag_shader[idx]);
    glLinkProgram(shader_obj);

    GLint shader_success;
    glGetProgramiv(shader_obj, GL_LINK_STATUS, &shader_success);
    if (!shader_success) {
        glGetProgramInfoLog(shader_obj, LOG_LEN_GLSL, NULL, shader_log);

        for (idx = 0; idx < out->vs_count; idx++)
            glDeleteShader(out->vert_shader[idx]);
        for (idx = 0; idx < out->fs_count; idx++)
            glDeleteShader(out->frag_shader[idx]);
        glDeleteProgram(shader_obj);

        memset(out->vert_shader, 0, sizeof(out->vert_shader));
        memset(out->frag_shader, 0, sizeof(out->frag_shader));

        fprintf(stderr, "Error linking shader.\n%s\n", shader_log);
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
    unsigned idx;
    glDeleteProgram(shader->shader_prog_obj);
    for (idx = 0; idx < shader->fs_count; idx++)
        glDeleteShader(shader->frag_shader[idx]);
    for (idx = 0; idx < shader->vs_count; idx++)
        glDeleteShader(shader->vert_shader[idx]);

    memset(shader, 0, sizeof(*shader));
}
