/*******************************************************************************
 *
 * Copyright 2020 snickerbockers
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
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

#include <stdint.h>
#include <string.h>
#include <math.h>

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#include <GL/gl.h>

#include "washdc/gfx/gfx_all.h"
#include "washdc/gfx/def.h"
#include "../gfx_obj.h"
#include "../shader.h"

#include "soft_gfx.h"

static struct soft_gfx_callbacks const *switch_table;

static void soft_gfx_init(void);
static void soft_gfx_cleanup(void);
static void soft_gfx_exec_gfx_il(struct gfx_il_inst *cmd, unsigned n_cmd);

static void init_poly();

#define FB_WIDTH 640
#define FB_HEIGHT 480

// vertex position (x, y, z)
#define OUTPUT_SLOT_VERT_POS 0

// vertex texture coordinates (s, t)
#define OUTPUT_SLOT_VERT_ST 1

#define OUTPUT_SLOT_TRANS_MAT 2

#define OUTPUT_SLOT_TEX_MAT 3

// number of floats per vertex.
// that's 3 floats for the position and 2 for the texture coords
#define FB_VERT_LEN 5
#define FB_VERT_COUNT 4
static GLfloat fb_quad_verts[FB_VERT_LEN * FB_VERT_COUNT] = {
    // position            // texture coordinates
    -1.0f,  1.0f, 0.0f,    0.0f, 1.0f,
    -1.0f, -1.0f, 0.0f,    0.0f, 0.0f,
     1.0f, -1.0f, 0.0f,    1.0f, 0.0f,
     1.0f,  1.0f, 0.0f,    1.0f, 1.0f
};

#define FB_QUAD_IDX_COUNT 4
static GLuint fb_quad_idx[FB_QUAD_IDX_COUNT] = {
    1, 0, 2, 3
};

static GLfloat trans_mat[16] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};

static GLfloat tex_mat[9] = {
    1.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 1.0f
};

/*
 * container for the poly's vertex array and its associated buffer objects.
 * this is created by fb_init_poly and never modified.
 *
 * The tex_obj, on the other hand, is modified frequently, as it is OpenGL's
 * view of our framebuffer.
 */
static struct fb_poly {
    GLuint vbo; // vertex buffer object
    GLuint vao; // vertex array object
    GLuint ebo; // element buffer object
} fb_poly;

static uint32_t fb[FB_WIDTH * FB_HEIGHT];
static GLuint fb_tex;
static struct shader fb_shader;
static int render_tgt = -1;
static int screen_width, screen_height;
static bool wireframe_mode;

static void rot90(float out[2], float const in[2]);

struct rend_if const soft_gfx_if = {
    .init = soft_gfx_init,
    .cleanup = soft_gfx_cleanup,
    .exec_gfx_il = soft_gfx_exec_gfx_il
};

static void soft_gfx_init(void) {
    glewExperimental = GL_TRUE;
    glewInit();

    static char const * const final_vert_glsl =
        "#extension GL_ARB_explicit_uniform_location : enable\n"

        "layout (location = 0) in vec3 vert_pos;\n"
        "layout (location = 1) in vec2 tex_coord;\n"
        "layout (location = 2) uniform mat4 trans_mat;\n"
        "layout (location = 3) uniform mat3 tex_mat;\n"

        "out vec2 st;\n"

        "void main() {\n"
        "    gl_Position = trans_mat * vec4(vert_pos.x, vert_pos.y, vert_pos.z, 1.0);\n"
        "    st = (tex_mat * vec3(tex_coord.x, tex_coord.y, 1.0)).xy;\n"
        "}\n";

    static char const * const final_frag_glsl =
        "in vec2 st;\n"
        "out vec4 color;\n"

        "uniform sampler2D fb_tex;\n"

        "void main() {\n"
        "    vec4 sample = texture(fb_tex, st);\n"
        "    color = sample;\n"
        "}\n";

    shader_load_vert(&fb_shader, final_vert_glsl);
    shader_load_frag(&fb_shader, final_frag_glsl);
    shader_link(&fb_shader);

    glGenTextures(1, &fb_tex);
    glBindTexture(GL_TEXTURE_2D, fb_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    memset(fb, 0, sizeof(fb));

    init_poly();
}

static void soft_gfx_cleanup(void) {
    glDeleteTextures(1, &fb_tex);
}

static void init_poly() {
    GLuint vbo, vao, ebo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 FB_VERT_LEN * FB_VERT_COUNT * sizeof(GLfloat),
                 fb_quad_verts, GL_STATIC_DRAW);
    glVertexAttribPointer(OUTPUT_SLOT_VERT_POS, 3, GL_FLOAT, GL_FALSE,
                          FB_VERT_LEN * sizeof(GLfloat),
                          (GLvoid*)0);
    glEnableVertexAttribArray(OUTPUT_SLOT_VERT_POS);
    glVertexAttribPointer(OUTPUT_SLOT_VERT_ST, 2, GL_FLOAT, GL_FALSE,
                          FB_VERT_LEN * sizeof(GLfloat),
                          (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(OUTPUT_SLOT_VERT_ST);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, FB_QUAD_IDX_COUNT * sizeof(GLuint),
                 fb_quad_idx, GL_STATIC_DRAW);

    glBindVertexArray(0);

    fb_poly.vbo = vbo;
    fb_poly.vao = vao;
    fb_poly.ebo = ebo;
}

void soft_gfx_set_callbacks(struct soft_gfx_callbacks const *callbacks) {
    switch_table = callbacks;
}

static void soft_gfx_obj_init(struct gfx_il_inst *cmd) {
    int obj_no = cmd->arg.init_obj.obj_no;
    size_t n_bytes = cmd->arg.init_obj.n_bytes;
    gfx_obj_init(obj_no, n_bytes);
    printf("\tinitialize object %d\n", obj_no);
}

static void soft_gfx_obj_write(struct gfx_il_inst *cmd) {
    int obj_no = cmd->arg.write_obj.obj_no;
    size_t n_bytes = cmd->arg.write_obj.n_bytes;
    void const *dat = cmd->arg.write_obj.dat;
    gfx_obj_write(obj_no, dat, n_bytes);
}

static void soft_gfx_obj_read(struct gfx_il_inst *cmd) {
    int obj_no = cmd->arg.read_obj.obj_no;
    size_t n_bytes = cmd->arg.read_obj.n_bytes;
    void *dat = cmd->arg.read_obj.dat;
    gfx_obj_read(obj_no, dat, n_bytes);
}

static void soft_gfx_obj_free(struct gfx_il_inst *cmd) {
    int obj_no = cmd->arg.free_obj.obj_no;
    gfx_obj_free(obj_no);
}

static void soft_gfx_bind_render_target(struct gfx_il_inst *cmd) {
    int obj_handle = cmd->arg.bind_render_target.gfx_obj_handle;
    struct gfx_obj *obj = gfx_obj_get(obj_handle);

    gfx_obj_alloc(obj);
}

static void soft_gfx_post_fb(struct gfx_il_inst *cmd) {
    int obj_handle = cmd->arg.post_framebuffer.obj_handle;
    struct gfx_obj *obj = gfx_obj_get(obj_handle);
    bool do_flip = cmd->arg.post_framebuffer.vert_flip;

    printf("\tpost object %d\n", obj_handle);

    if (obj->dat_len && obj->dat){
        size_t n_bytes = obj->dat_len < sizeof(fb) ? obj->dat_len : sizeof(fb);
        if (do_flip) {
            unsigned src_width = cmd->arg.post_framebuffer.width;
            unsigned src_height = cmd->arg.post_framebuffer.height;

            unsigned copy_width = src_width < FB_WIDTH ? src_width : FB_WIDTH;
            unsigned copy_height = src_height < FB_HEIGHT ? src_height : FB_HEIGHT;

            unsigned row;
            for (row = 0; row < copy_height; row++) {
                uint32_t *dstp = fb + row * FB_WIDTH;
                char *srcp = ((char*)obj->dat) + 4 * (src_height - 1 - row) * src_width;
                memcpy(dstp, srcp, copy_width * 4);
            }
        } else {
            memcpy(fb, obj->dat, n_bytes);
        }
    }

    glViewport(0, 0, FB_WIDTH, FB_HEIGHT);
    glUseProgram(fb_shader.shader_prog_obj);

    glBindTexture(GL_TEXTURE_2D, fb_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, FB_WIDTH, FB_HEIGHT,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, fb);

    glUniform1i(glGetUniformLocation(fb_shader.shader_prog_obj, "fb_tex"), 0);
    glUniformMatrix4fv(OUTPUT_SLOT_TRANS_MAT, 1, GL_TRUE, trans_mat);
    glUniformMatrix3fv(OUTPUT_SLOT_TEX_MAT, 1, GL_TRUE, tex_mat);

    glUseProgram(fb_shader.shader_prog_obj);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(fb_poly.vao);
    glDrawElements(GL_TRIANGLE_STRIP, FB_QUAD_IDX_COUNT, GL_UNSIGNED_INT, 0);

    switch_table->win_update();
}

static int clamp_int(int val, int min, int max) {
    if (val < min)
        return min;
    else if (val > max)
        return max;
    else
        return val;
}

static void soft_gfx_clear(struct gfx_il_inst *cmd) {
    if (render_tgt < 0) {
        fprintf(stderr, "ERROR: no render target bound for %s\n", __func__);
        return;
    }
    struct gfx_obj *obj = gfx_obj_get(render_tgt);

    uint32_t as_32;
    if (wireframe_mode) {
        as_32 = 0;
    } else {
        float const *bgcolor = cmd->arg.clear.bgcolor;
        int rgba[4] = {
                       clamp_int(bgcolor[0] * 255, 0, 255),
                       clamp_int(bgcolor[1] * 255, 0, 255),
                       clamp_int(bgcolor[2] * 255, 0, 255),
                       clamp_int(bgcolor[3] * 255, 0, 255)
        };
        as_32 =
            rgba[0]       |
            rgba[1] << 8  |
            rgba[2] << 16 |
            rgba[3] << 24;
    }

    if (obj->dat_len && !obj->dat) {
        fprintf(stderr, "ERROR: %s: object has no data pointer!\n", __func__);
        return;
    }

    if (obj->dat_len % sizeof(as_32)) {
        fprintf(stderr, "ERROR: %s: obj not aligned by four!\n", __func__);
        return;
    }

    size_t n_dwords = obj->dat_len / sizeof(as_32);
    unsigned idx;
    char *datp = (char*)obj->dat;
    for (idx = 0; idx < n_dwords; idx++) {
        memcpy(datp, &as_32, sizeof(as_32));
        datp += sizeof(as_32);
    }
}

static void soft_gfx_begin_rend(struct gfx_il_inst *cmd) {
    screen_width = cmd->arg.begin_rend.screen_width;
    screen_height = cmd->arg.begin_rend.screen_height;
    int obj_handle = cmd->arg.begin_rend.rend_tgt_obj;

    if (obj_handle < 0) {
        fprintf(stderr, "%s - invalid render target handle %d\n",
                __func__, obj_handle);
        return;
    }

    if (render_tgt != -1) {
        fprintf(stderr, "%s - %d still bound as render target!\n",
                __func__, render_tgt);
    }

    struct gfx_obj *obj = gfx_obj_get(obj_handle);
    if (!obj->dat || obj->dat_len < screen_width * screen_height * 4) {
        fprintf(stderr, "%s - invalid object %d data length %llu or NULL "
                "pointer for %ux%u; has it been bound as a render target "
                "yet?\n",
                __func__, obj_handle, (unsigned long long)obj->dat_len,
                screen_width, screen_height);
        return;
    }

    render_tgt = obj_handle;

    // frontend rendering parameters
    wireframe_mode = gfx_config_read().wireframe;
}

static void soft_gfx_end_rend(struct gfx_il_inst *cmd) {
    if (render_tgt < 0)
        fprintf(stderr, "%s - no render target bound!\n", __func__);
    render_tgt = -1;
}

#if 0
static void draw_pt(void *dat, int x_pos, int y_pos, int side_len) {
    int pix_y;
    int x_l = x_pos - side_len;
    int x_r = x_pos + side_len;
    int y_t = y_pos - side_len;
    int y_b = y_pos + side_len;

    if (x_l < 0)
        x_l = 0;
    else if (x_l >= screen_width)
        return;

    if (x_r >= screen_width)
        x_r = screen_width - 1;
    else if (x_r < 0)
        return;

    if (y_t < 0)
        y_t = 0;
    else if (y_t >= screen_height)
        return;

    if (y_b >= screen_height)
        y_b = screen_height - 1;
    else if (y_b < 0)
        return;

    size_t n_bytes = sizeof(uint32_t) * (x_r - x_l + 1);
    for (pix_y = y_t; pix_y <= y_b; pix_y++) {
        memset(((char*)dat) + (pix_y * screen_width + x_l) * sizeof(uint32_t),
               0xff, n_bytes);
    }
}
#endif

static inline void
put_pix(void *dat, int x_pix, int y_pix, uint32_t color) {

    if (x_pix < 0 || y_pix < 0 ||
        x_pix >= screen_width || y_pix >= screen_height) {
        fprintf(stderr, "%s - ERROR out of bounds (%d, %d)\n",
                __func__, x_pix, y_pix);
        abort();
    }

    y_pix = screen_height - 1 - y_pix;
    memcpy((char*)dat + (y_pix * screen_width + x_pix) * sizeof(uint32_t),
           &color, sizeof(uint32_t));
}

static void
draw_line(void *dat, int x1, int y1, int x2, int y2, uint32_t color) {
    if ((x1 < 0 && x2 < 0) ||
        (x1 >= screen_width && x2 >= screen_width) ||
        (y1 < 0 && y2 < 0) ||
        (y1 >= screen_height && y2 >= screen_height)) {
        return;
    }

    x1 = clamp_int(x1, 0, screen_width - 1);
    x2 = clamp_int(x2, 0, screen_width - 1);
    y1 = clamp_int(y1, 0, screen_height - 1);
    y2 = clamp_int(y2, 0, screen_height - 1);


    int delta_y = y2 - y1;
    int delta_x = x2 - x1;

    // use bresenham's line algorithm
    if (abs(delta_x) >= abs(delta_y)) {
        if ((delta_x >= 0 && delta_y >= 0) ||
            (delta_x <= 0 && delta_y <= 0)) {
            /*
             * angle is either between 0 and 45 degrees,
             * or between 180 and 225 degrees
             */
            if (delta_x < 0) {
                /*
                 * angle is between 180 and 225, so swap direction to make
                 * it between 0 and 45
                 */
                int tmp_x = x1;
                x1 = x2;
                x2 = tmp_x;

                int tmp_y = y1;
                y1 = y2;
                y2 = tmp_y;

                delta_x = -delta_x;
                delta_y = -delta_y;
            }

            // draw the line
            int x_pos = x1, y_pos = y1;
            int error = 0;
            do {
                put_pix(dat, x_pos, y_pos, color);
                error += delta_y;
                if (2 * error >= delta_x) {
                    y_pos++;
                    error -= delta_x;
                }
            } while (x_pos++ != x2);
        } else {
            /*
             * angle is either between 135 and 180 degrees,
             * or between 315 and 360 degrees
             */
            if (delta_x < 0) {
                /*
                 * angle is between 135 and 180 degrees, so swap direction to make
                 * it between 0 and 45
                 */
                int tmp_x = x1;
                x1 = x2;
                x2 = tmp_x;

                int tmp_y = y1;
                y1 = y2;
                y2 = tmp_y;

                delta_x = -delta_x;
                delta_y = -delta_y;
            }

            // draw the line
            int x_pos = x1, y_pos = y1;
            int error = 0;
            do {
                put_pix(dat, x_pos, y_pos, color);
                error += delta_y;
                if (2 * error < -delta_x) {
                    y_pos--;
                    error += delta_x;
                }
            } while (x_pos++ != x2);
        }
    } else {
        if ((delta_x >= 0 && delta_y >= 0) ||
            (delta_x <= 0 && delta_y <= 0)) {
            /*
             * angle is either between 45 and 90 degrees,
             * or between 225 and 270 degrees
             */
            if (delta_y < 0) {
                /*
                 * angle is between 225 and 270 degrees, so swap direction to make
                 * it between 0 and 45
                 */
                int tmp_x = x1;
                x1 = x2;
                x2 = tmp_x;

                int tmp_y = y1;
                y1 = y2;
                y2 = tmp_y;

                delta_x = -delta_x;
                delta_y = -delta_y;
            }

            // draw the line
            int x_pos = x1, y_pos = y1;
            int error = 0;
            do {
                put_pix(dat, x_pos, y_pos, color);
                error += delta_x;
                if (2 * error >= delta_y) {
                    x_pos++;
                    error -= delta_y;
                }
            } while (y_pos++ != y2);
        } else {
            /*
             * angle is either between either 90 and 135 degrees,
             * or between 270 and 315 degrees
             */
            if (delta_y < 0) {
                /*
                 * angle is between 270 and 315 degrees, so swap direction to make
                 * it between 90 and 135
                 */
                int tmp_x = x1;
                x1 = x2;
                x2 = tmp_x;

                int tmp_y = y1;
                y1 = y2;
                y2 = tmp_y;

                delta_x = -delta_x;
                delta_y = -delta_y;
            }

            // draw the line
            int x_pos = x1, y_pos = y1;
            int error = 0;
            do {
                put_pix(dat, x_pos, y_pos, color);
                error += delta_x;
                if (2 * error < -delta_y) {
                    x_pos--;
                    error += delta_y;
                }
            } while (y_pos++ != y2);
        }
    }
}

static void rot90(float out[2], float const in[2]) {
    // be careful in case in == out
    float new_x = -in[1];
    float new_y = in[0];
    out[0] = new_x;
    out[1] = new_y;
}

/* static float dot(float const v1[2], float const v2[2]) { */
/*     return v1[0] * v2[0] + v1[1] * v2[1]; */
/* } */

/*
 * dot product of v1 rotated by 90 degrees and v2
 *
 * this is a scalar with the magnitude and sign of the three-dimensional cross
 * product, ie |v1| * |v2| * sin(angle_between_v1_and_v2)
 */
static float ortho_dot(float const v1[2], float const v2[2]) {
    return -v1[1] * v2[0] + v1[0] * v2[1];
}

/*
 * return 2-dimensional bounding-box of given triangle
 * bounds[0] is x_min
 * bounds[1] is y_min
 * bounds[2] is x_max
 * bounds[3] is y_max
 */
static void
tri_bbox(float bounds[4], float const p1[2],
         float const p2[2], float const p3[2]) {
    bounds[0] = fminf(fminf(p1[0], p2[0]), p3[0]);
    bounds[1] = fminf(fminf(p1[1], p2[1]), p3[1]);
    bounds[2] = fmaxf(fmaxf(p1[0], p2[0]), p3[0]);
    bounds[3] = fmaxf(fmaxf(p1[1], p2[1]), p3[1]);
}

static void line_coeff(float coeff[3], float const p1[2], float const p2[2]) {
    float vec[2] = {
        p2[0] - p1[0],
        p2[1] - p1[1]
    };

    rot90(coeff, vec);
    coeff[2] = -(coeff[0] * p1[0] + coeff[1] * p1[1]);
}

static float tri_area(float const v1[2], float const v2[2], float const v3[2]) {
    float vec1[2] = { v2[0] - v1[0], v2[1] - v1[1] };
    float vec2[2] = { v3[0] - v1[0], v3[1] - v1[1] };

    return 0.5f * fabsf(ortho_dot(vec1, vec2));
}

static void soft_gfx_draw_array(struct gfx_il_inst *cmd) {
    if (render_tgt < 0) {
        fprintf(stderr, "%s - no render target bound!\n", __func__);
        return;
    }

    struct gfx_obj *obj = gfx_obj_get(render_tgt);
    unsigned n_verts = cmd->arg.draw_array.n_verts;
    float const *verts = cmd->arg.draw_array.verts;

    if (wireframe_mode) {
        /*
         * draw triangles as white lines with no depth testing or
         * per-vertex attributes
         */
        unsigned vert_no;
        for (vert_no = 0; vert_no < n_verts; vert_no += 3) {
            float const *p1 = verts + (vert_no + 0) * GFX_VERT_LEN;
            float const *p2 = verts + (vert_no + 1) * GFX_VERT_LEN;
            float const *p3 = verts + (vert_no + 2) * GFX_VERT_LEN;

            draw_line(obj->dat, p1[0], p1[1], p2[0], p2[1], 0xffffffff);
            draw_line(obj->dat, p2[0], p2[1], p3[0], p3[1], 0xffffffff);
            draw_line(obj->dat, p3[0], p3[1], p1[0], p1[1], 0xffffffff);
        }
    } else {
        unsigned vert_no;
        for (vert_no = 0; vert_no < n_verts; vert_no += 3) {
            float const *p1 = verts + (vert_no + 0) * GFX_VERT_LEN;
            float const *p2 = verts + (vert_no + 1) * GFX_VERT_LEN;
            float const *p3 = verts + (vert_no + 2) * GFX_VERT_LEN;

            float v1[3] = { p2[0] - p1[0], p2[1] - p1[1], p2[2] - p1[2] };
            float v2[3] = { p3[0] - p1[0], p3[1] - p1[1], p3[2] - p1[2] };

            /*
             * positive is counter-clockwise and negative is clockwise
             *
             * except the y-coordinate is inverted so really it's the other way
             * around.
             */
            int sign = ortho_dot(v1, v2) < 0.0f ? -1 : 1;

            float bbox_float[4];
            tri_bbox(bbox_float, p1, p2, p3);

            int bbox[4] = { bbox_float[0], bbox_float[1],
                            bbox_float[2], bbox_float[3] };

            if (bbox[0] < 0)
                bbox[0] = 0;
            else if (bbox[0] >= screen_width)
                continue;
            if (bbox[1] < 0)
                bbox[1] = 0;
            else if (bbox[1] >= screen_height)
                continue;
            if (bbox[2] >= screen_width)
                bbox[2] = screen_width - 1;
            else if (bbox[2] < 0)
                continue;
            if (bbox[3] >= screen_height)
                bbox[3] = screen_height - 1;
            else if (bbox[3] < 0)
                continue;

            /*
             * edge line coefficients
             * ax + by + c == 0
             *
             * index 0 - a
             * index 1 - b
             * index 2 - c
             */
            float e1[3], e2[3], e3[3];
            line_coeff(e1, p1, p2);
            line_coeff(e2, p2, p3);
            line_coeff(e3, p3, p1);

            float area = tri_area(p1, p2, p3);
            float area_recip = 1.0f / area;

            // perspective-correct base color
            float p1_base_col[4] = {
                p1[GFX_VERT_BASE_COLOR_OFFSET] * p1[2],
                p1[GFX_VERT_BASE_COLOR_OFFSET + 1] * p1[2],
                p1[GFX_VERT_BASE_COLOR_OFFSET + 2] * p1[2],
                p1[GFX_VERT_BASE_COLOR_OFFSET + 3] * p1[2]
            };
            float p2_base_col[4] = {
                p2[GFX_VERT_BASE_COLOR_OFFSET] * p2[2],
                p2[GFX_VERT_BASE_COLOR_OFFSET + 1] * p2[2],
                p2[GFX_VERT_BASE_COLOR_OFFSET + 2] * p2[2],
                p2[GFX_VERT_BASE_COLOR_OFFSET + 3] * p2[2]
            };
            float p3_base_col[4] = {
                p3[GFX_VERT_BASE_COLOR_OFFSET] * p3[2],
                p3[GFX_VERT_BASE_COLOR_OFFSET + 1] * p3[2],
                p3[GFX_VERT_BASE_COLOR_OFFSET + 2] * p3[2],
                p3[GFX_VERT_BASE_COLOR_OFFSET + 3] * p3[2]
            };

            int x_pos, y_pos;
            for (y_pos = bbox[1]; y_pos <= bbox[3]; y_pos++) {
                for (x_pos = bbox[0]; x_pos <= bbox[2]; x_pos++) {
                    float pos[2] = { x_pos, y_pos };
                    float dist[3] = {
                        e1[0] * pos[0] + e1[1] * pos[1] + e1[2],
                        e2[0] * pos[0] + e2[1] * pos[1] + e2[2],
                        e3[0] * pos[0] + e3[1] * pos[1] + e3[2]
                    };

                    if ((sign == -1 && dist[0] <= 0.0f &&
                         dist[1] <= 0.0f && dist[2] <= 0.0f) ||
                        (sign == 1 && dist[0] >= 0.0f &&
                         dist[1] >= 0.0f && dist[2] >= 0.0f)) {

                        // barycentric coordinates
                        float bary[3] = {
                            tri_area(p2, p3, pos) / area,
                            tri_area(p3, p1, pos) / area,
                            tri_area(p1, p2, pos) / area
                        };

                        // reciprocal depth
                        float w_coord =
                            p1[2] * bary[0] + p2[2] * bary[1] + p3[2] * bary[2];

                        float base_col[4] = {
                            (p1_base_col[0] * bary[0] + p2_base_col[0] * bary[1] +
                             p3_base_col[0] * bary[2]) / w_coord,

                            (p1_base_col[1] * bary[0] + p2_base_col[1] * bary[1] +
                             p3_base_col[1] * bary[2]) / w_coord,

                            (p1_base_col[2] * bary[0] + p2_base_col[2] * bary[1] +
                             p3_base_col[2] * bary[2]) / w_coord,

                            (p1_base_col[3] * bary[0] + p2_base_col[3] * bary[1] +
                             p3_base_col[3] * bary[2]) / w_coord
                        };

                        int rgba[4] = {
                            clamp_int(base_col[0] * 255, 0, 255),
                            clamp_int(base_col[1] * 255, 0, 255),
                            clamp_int(base_col[2] * 255, 0, 255),
                            clamp_int(base_col[3] * 255, 0, 255)
                        };

                        put_pix(obj->dat, x_pos, y_pos,
                                rgba[0]          |
                                (rgba[1] << 8)   |
                                (rgba[2] << 16)  |
                                (rgba[3] << 24));
                    }
                }
            }
        }
    }
}

static void soft_gfx_exec_gfx_il(struct gfx_il_inst *cmd, unsigned n_cmd) {
    while (n_cmd--) {
        switch (cmd->op) {
        case GFX_IL_BIND_TEX:
            printf("GFX_IL_BIND_TEX\n");
            break;
        case GFX_IL_UNBIND_TEX:
            printf("GFX_IL_UNBIND_TEX\n");
            break;
        case GFX_IL_BIND_RENDER_TARGET:
            printf("GFX_IL_BIND_RENDER_TARGET\n");
            soft_gfx_bind_render_target(cmd);
            break;
        case GFX_IL_UNBIND_RENDER_TARGET:
            printf("GFX_IL_UNBIND_RENDER_TARGET\n");
            break;
        case GFX_IL_BEGIN_REND:
            printf("GFX_IL_BEGIN_REND\n");
            soft_gfx_begin_rend(cmd);
            break;
        case GFX_IL_END_REND:
            printf("GFX_IL_END_REND\n");
            soft_gfx_end_rend(cmd);
            break;
        case GFX_IL_CLEAR:
            printf("GFX_IL_CLEAR\n");
            soft_gfx_clear(cmd);
            break;
        case GFX_IL_SET_BLEND_ENABLE:
            printf("GFX_IL_SET_BLEND_ENABLE\n");
            break;
        case GFX_IL_SET_REND_PARAM:
            printf("GFX_IL_SET_REND_PARAM\n");
            break;
        case GFX_IL_SET_CLIP_RANGE:
            printf("GFX_IL_SET_CLIP_RANGE\n");
            break;
        case GFX_IL_DRAW_ARRAY:
            printf("GFX_IL_DRAW_ARRAY\n");
            soft_gfx_draw_array(cmd);
            break;
        case GFX_IL_INIT_OBJ:
            printf("GFX_IL_INIT_OBJ\n");
            soft_gfx_obj_init(cmd);
            break;
        case GFX_IL_WRITE_OBJ:
            printf("GFX_IL_WRITE_OBJ\n");
            soft_gfx_obj_write(cmd);
            break;
        case GFX_IL_READ_OBJ:
            printf("GFX_IL_READ_OBJ\n");
            soft_gfx_obj_read(cmd);
            break;
        case GFX_IL_FREE_OBJ:
            printf("GFX_IL_FREE_OBJ\n");
            soft_gfx_obj_free(cmd);
            break;
        case GFX_IL_POST_FRAMEBUFFER:
            printf("GFX_IL_POST_FRAMEBUFFER\n");
            soft_gfx_post_fb(cmd);
            break;
        case GFX_IL_GRAB_FRAMEBUFFER:
            printf("GFX_IL_GRAB_FRAMEBUFFER\n");
            break;
        case GFX_IL_BEGIN_DEPTH_SORT:
            printf("GFX_IL_BEGIN_DEPTH_SORT\n");
            break;
        case GFX_IL_END_DEPTH_SORT:
            printf("GFX_IL_END_DEPTH_SORT\n");
            break;
        default:
            fprintf(stderr, "ERROR: UNKNOWN GFX IL COMMAND %02X\n",
                    (unsigned)cmd->op);
        }
        cmd++;
    }
}
