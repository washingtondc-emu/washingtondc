/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2019 snickerbockers
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

void shader_load_vert(struct shader *out, char const *vert_shader_src) {
    shader_load_vert_with_preamble(out, vert_shader_src, NULL);
}

void shader_load_frag(struct shader *out, char const *frag_shader_src) {
    shader_load_frag_with_preamble(out, frag_shader_src, NULL);
}

void shader_load_vert_with_preamble(struct shader *out,
                                    char const *vert_shader_src,
                                    char const *preamble) {
    int n_shader_strings;
    char const *shader_strings[3];

    if (preamble) {
        shader_strings[0] = "#version 330 core\n";
        shader_strings[1] = preamble;
        shader_strings[2] = vert_shader_src;
        n_shader_strings = 3;
    } else {
        shader_strings[0] = "#version 330 core\n";
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

        fprintf(stderr, "Error compiling shader: %s\n", shader_log);
        exit(1);
    }

    out->vert_shader = vert_shader;
}

void shader_load_frag_with_preamble(struct shader *out,
                                    char const *frag_shader_src,
                                    char const *preamble) {
    int n_shader_strings;
    char const *shader_strings[3];

    if (preamble) {
        shader_strings[0] = "#version 330 core\n";
        shader_strings[1] = preamble;
        shader_strings[2] = frag_shader_src;
        n_shader_strings = 3;
    } else {
        shader_strings[0] = "#version 330 core\n";
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

        fprintf(stderr, "Error compiling shader: %s\n", shader_log);
        exit(1);
    }

    out->frag_shader = frag_shader;
}

void shader_load_vert_from_file_with_preamble(struct shader *out,
                                              char const *vert_shader_path,
                                              char const *preamble) {
    char *vert_shader_src;

    vert_shader_src = read_txt(vert_shader_path);

    shader_load_vert_with_preamble(out, vert_shader_src, preamble);

    free(vert_shader_src);
}

void shader_load_frag_from_file_with_preamble(struct shader *out,
                                              char const *frag_shader_path,
                                              char const *preamble) {
    char *frag_shader_src;

    frag_shader_src = read_txt(frag_shader_path);

    shader_load_frag_with_preamble(out, frag_shader_src, preamble);

    free(frag_shader_src);
}

void shader_load_vert_from_file(struct shader *out, char const *vert_shader_path) {
    shader_load_vert_from_file_with_preamble(out, vert_shader_path, NULL);
}

void shader_load_frag_from_file(struct shader *out, char const *frag_shader_path) {
    shader_load_frag_from_file_with_preamble(out, frag_shader_path, NULL);
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
