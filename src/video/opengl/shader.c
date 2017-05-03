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

#include <err.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#include <GL/gl.h>

#include "shader.h"

static char *read_txt(char const *path);

#define LOG_LEN_GLSL 1024
GLchar shader_log[LOG_LEN_GLSL];

void shader_init(struct shader *out,
                 char const *vert_shader_src,
                 char const *frag_shader_src) {
    GLuint vert_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert_shader, 1, &vert_shader_src, NULL);
    glCompileShader(vert_shader);

    GLint shader_success;
    glGetShaderiv(vert_shader, GL_COMPILE_STATUS, &shader_success);
    if (!shader_success) {
        glGetShaderInfoLog(vert_shader, LOG_LEN_GLSL, NULL, shader_log);

        glDeleteShader(vert_shader);

        errx(1, "Error compiling shader: %s\n", shader_log);
    }

    GLuint frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag_shader, 1, &frag_shader_src, NULL);
    glCompileShader(frag_shader);

    glGetShaderiv(frag_shader, GL_COMPILE_STATUS, &shader_success);
    if (!shader_success) {
        glGetShaderInfoLog(frag_shader, LOG_LEN_GLSL, NULL, shader_log);

        glDeleteShader(vert_shader);
        glDeleteShader(frag_shader);

        errx(1, "Error compiling shader: %s\n", shader_log);
    }

    GLuint shader_obj = glCreateProgram();
    glAttachShader(shader_obj, vert_shader);
    glAttachShader(shader_obj, frag_shader);
    glLinkProgram(shader_obj);

    glGetProgramiv(shader_obj, GL_LINK_STATUS, &shader_success);
    if (!shader_success) {
        glGetProgramInfoLog(shader_obj, LOG_LEN_GLSL, NULL, shader_log);

        glDeleteShader(vert_shader);
        glDeleteShader(frag_shader);
        glDeleteProgram(shader_obj);

        errx(1, "Error compiling shader: %s\n", shader_log);
    }

    out->vert_shader = vert_shader;
    out->frag_shader = frag_shader;
    out->shader_prog_obj = shader_obj;
}

static char *read_txt(char const *path) {
    FILE *txt_fp;
    char *src;
    long src_len;

    if (!(txt_fp = fopen(path, "r")))
        err(1, "Unable to open \"%s\"\n", path);

    if (fseek(txt_fp, 0, SEEK_END) < 0)
        err(1, "unable to seek \"%s\"\n", path);
    if ((src_len = ftell(txt_fp)) < 0)
        err(1, "unable to obtain length of \"%s\"\n", path);
    if (fseek(txt_fp, 0, SEEK_SET) < 0)
        err(1, "unable to seek \"%s\"\n", path);

    src = (char*)malloc(sizeof(char) * (1 + src_len));
    if (fread(src, sizeof(char),
              src_len, txt_fp) != src_len) {
        err(1, "unable to read from \"%s\"\n", path);
    }
    src[src_len - 1] = '\0';

    fclose(txt_fp);

    return src;
}

void shader_init_from_file(struct shader *out,
                           char const *vert_shader_path,
                           char const *frag_shader_path) {
    char *vert_shader_src, *frag_shader_src;

    vert_shader_src = read_txt(vert_shader_path);
    frag_shader_src = read_txt(frag_shader_path);

    shader_init(out, vert_shader_src, frag_shader_src);

    free(vert_shader_src);
    free(frag_shader_src);
}

void shader_cleanup(struct shader const *shader) {
    glDeleteProgram(shader->shader_prog_obj);
    glDeleteShader(shader->frag_shader);
    glDeleteShader(shader->vert_shader);
}
