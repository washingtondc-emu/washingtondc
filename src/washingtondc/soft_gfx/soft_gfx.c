/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2020, 2022 snickerbockers
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

static struct renderer_callbacks const *switch_table;

static void soft_gfx_init(void);
static void soft_gfx_cleanup(void);
static void soft_gfx_exec_gfx_il(struct gfx_il_inst *cmd, unsigned n_cmd);

static void init_poly();

static inline void
put_pix(struct gfx_obj *obj, int x_pix, int y_pix, uint32_t color);
static inline void
put_pix_blended(struct gfx_obj *obj, int x_pix, int y_pix, uint32_t color,
                enum Pvr2BlendFactor src_blend_factor,
                enum Pvr2BlendFactor dst_blend_factor);

static bool user_clip_test(int x_pix, int y_pix);
static bool clip_test(int x_pix, int y_pix);

static void soft_gfx_set_callbacks(struct renderer_callbacks const *callbacks);

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

static float *w_buffer = NULL;

// used for per-pixel order-independent transparency if sort_mode==true
#define MAX_OIT_PIXELS (FB_WIDTH * FB_HEIGHT * 32)
static struct oit_pixel {
    int rgba[4];
    float w_coord;
    int next_pix_idx; // if less than 0, then there is no next index
    enum Pvr2BlendFactor src_blend_factor, dst_blend_factor;
} oit_pixels[MAX_OIT_PIXELS];
static unsigned n_oit_pixels;

/*
 * 1 index into oit_pixels per pixel.  If less than 0 then there is nothing
 * there.
 */
static int oit_buf[FB_WIDTH * FB_HEIGHT];

static void sort_oit_pix_list(int first_idx);

static bool sort_mode_enable;
static bool blend_enable;
static struct gfx_rend_param rend_param;

static GLuint fb_tex;
static struct shader fb_shader;
static int render_tgt = -1;
static int screen_width, screen_height;
static bool wireframe_mode;

// pixel-space clip rectangle, for OpenGL-style scissor test
static unsigned clip[4];

/*
 * second pixel-space clip-rectangle which can be selectively enabled/disabled,
 * and also optionally can be inverted.
 */
static unsigned user_clip[4];

static float *vert_array;
unsigned vert_array_len;

struct tex {
    int obj_no;
    unsigned width, height;
    enum gfx_tex_fmt fmt;
};
// maps texture objects to gfx objects
static struct tex textures[GFX_TEX_CACHE_SIZE];

static void rot90(float out[2], float const in[2]);

struct gfx_rend_if const soft_gfx_if = {
    .init = soft_gfx_init,
    .cleanup = soft_gfx_cleanup,
    .exec_gfx_il = soft_gfx_exec_gfx_il
};

struct renderer const soft_gfx_renderer = {
    .rend_if = &soft_gfx_if,
    .set_callbacks = soft_gfx_set_callbacks
};

static void soft_gfx_init(void) {
    glewExperimental = GL_TRUE;
    glewInit();

    static char const * const final_vert_glsl =
        "#version 330\n"
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
        "#version 330\n"
        "in vec2 st;\n"
        "out vec4 color;\n"

        "uniform sampler2D fb_tex;\n"

        "void main() {\n"
        "    color = texture(fb_tex, st);\n"
        "}\n";

    shader_load_vert(&fb_shader, "final_vert_glsl", final_vert_glsl);
    shader_load_frag(&fb_shader, "final_frag_glsl", final_frag_glsl);
    shader_link(&fb_shader);

    glGenTextures(1, &fb_tex);
    glBindTexture(GL_TEXTURE_2D, fb_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    memset(fb, 0, sizeof(fb));

    unsigned idx;
    for (idx = 0; idx < GFX_TEX_CACHE_SIZE; idx++)
        textures[idx].obj_no = -1;
    render_tgt = -1;
    screen_width = 0;
    screen_height = 0;
    w_buffer = NULL;
    vert_array = NULL;
    vert_array_len = 0;

    n_oit_pixels = 0;
    for (idx = 0; idx < FB_WIDTH * FB_HEIGHT; idx++)
        oit_buf[idx] = -1;

    init_poly();
}

static void soft_gfx_cleanup(void) {
    glDeleteTextures(1, &fb_tex);

    free(w_buffer);
    w_buffer = NULL;

    free(vert_array);
    vert_array = NULL;
    vert_array_len = 0;
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

static void soft_gfx_set_callbacks(struct renderer_callbacks const *callbacks) {
    switch_table = callbacks;
}

static void soft_gfx_obj_init(struct gfx_il_inst *cmd) {
    int obj_no = cmd->arg.init_obj.obj_no;
    size_t n_bytes = cmd->arg.init_obj.n_bytes;
    gfx_obj_init(obj_no, n_bytes);
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

    /*
     * TODO: write directly to the framebuffer instead of calling put_pix.
     *
     * It should be faster that way, and we can skip the per-pixel clip
     * rectangle check by looping across the range of the rectangle.
     */
    unsigned row, col;
    for (row = 0; row <= screen_height-1; row++)
        for (col = 0; col <= screen_width-1; col++)
            put_pix(obj, col, row, as_32);

    /*
     * clear depth buffer
     *
     * XXX not entirely sure what the best default value here should be since
     * there are several different depth tests that games can configure.
     * Greater/Greater-or-equal seem to be the most popular ones (and the only
     * one supports for order-independent transparency) so -INFINITY works well
     * here.  Ideally we would be implementing the depth test on a per-tile
     * basis using the same algorithm as the actual PVR2 hardware instead of
     * using a persistent depth buffer like high-level APIs do.
     */
    unsigned idx;
    for (idx = 0; idx < screen_width * screen_height; idx++)
        w_buffer[idx] = -INFINITY;
}

static void soft_gfx_begin_rend(struct gfx_il_inst *cmd) {
    int old_screen_width = screen_width;
    int old_screen_height = screen_height;

    screen_width = cmd->arg.begin_rend.screen_width;
    screen_height = cmd->arg.begin_rend.screen_height;

    memcpy(clip, cmd->arg.begin_rend.clip, sizeof(clip));

    if (screen_width != old_screen_width ||
        screen_height != old_screen_height) {
        float *w_buf_new = realloc(w_buffer,
                                   sizeof(float) *
                                   screen_width * screen_height);
        if (!w_buf_new) {
            fprintf(stderr, "ERROR: %s - failure to allocate new w_buffer\n",
                    __func__);
            abort();
        }
        w_buffer = w_buf_new;
    }

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
put_pix(struct gfx_obj *obj, int x_pix, int y_pix, uint32_t color) {
    y_pix = screen_height - 1 - y_pix;
    unsigned byte_offs = (y_pix * screen_width + x_pix) * sizeof(uint32_t);

    if (x_pix < 0 || y_pix < 0 ||
        x_pix >= screen_width || y_pix >= screen_height) {
        return;
    } else if (byte_offs + (sizeof(uint32_t) - 1) >= obj->dat_len) {
        fprintf(stderr, "%s - ERROR out of bounds (%d, %d)\n",
                __func__, x_pix, y_pix);
        fflush(stdout);
        fflush(stderr);
        abort();
    }

    memcpy(((char*)obj->dat) + byte_offs, &color, sizeof(uint32_t));
}

static inline void
put_pix_blended(struct gfx_obj *obj, int x_pix, int y_pix, uint32_t color,
                enum Pvr2BlendFactor src_blend_factor,
                enum Pvr2BlendFactor dst_blend_factor) {
    y_pix = screen_height - 1 - y_pix;
    unsigned byte_offs = (y_pix * screen_width + x_pix) * sizeof(uint32_t);

    if (x_pix < 0 || y_pix < 0 ||
        x_pix >= screen_width || y_pix >= screen_height ||
        byte_offs + (sizeof(uint32_t) - 1) >= obj->dat_len) {
        fprintf(stderr, "%s - ERROR out of bounds (%d, %d)\n",
                __func__, x_pix, y_pix);
        fflush(stdout);
        fflush(stderr);
        abort();
    }

    uint32_t dst_val;
    memcpy(&dst_val, ((char*)obj->dat) + byte_offs, sizeof(dst_val));

    float dst_rgba[4] = {
        (dst_val & 0xff) / 255.0f,
        ((dst_val >> 8) & 0xff) / 255.0f,
        ((dst_val >> 16) & 0xff) / 255.0f,
        ((dst_val >> 24) & 0xff) / 255.0f
    };

    float src_rgba[4] = {
        (color & 0xff) / 255.0f,
        ((color >> 8) & 0xff) / 255.0f,
        ((color >> 16) & 0xff) / 255.0f,
        ((color >> 24) & 0xff) / 255.0f
    };

    float src_fact[4], dst_fact[4];

    switch (src_blend_factor) {
    default:
        fprintf(stderr, "ERROR: src Unknown blend factor\n");
    case PVR2_BLEND_ZERO:
        src_fact[0] = 0.0f;
        src_fact[1] = 0.0f;
        src_fact[2] = 0.0f;
        src_fact[3] = 0.0f;
        break;
    case PVR2_BLEND_ONE:
        src_fact[0] = 1.0f;
        src_fact[1] = 1.0f;
        src_fact[2] = 1.0f;
        src_fact[3] = 1.0f;
        break;
    case PVR2_BLEND_OTHER:
        src_fact[0] = dst_rgba[0];
        src_fact[1] = dst_rgba[1];
        src_fact[2] = dst_rgba[2];
        src_fact[3] = dst_rgba[3];
        break;
    case PVR2_BLEND_ONE_MINUS_OTHER:
        src_fact[0] = 1.0f - dst_rgba[0];
        src_fact[1] = 1.0f - dst_rgba[1];
        src_fact[2] = 1.0f - dst_rgba[2];
        src_fact[3] = 1.0f - dst_rgba[3];
        break;
    case PVR2_BLEND_SRC_ALPHA:
        src_fact[0] = src_rgba[3];
        src_fact[1] = src_rgba[3];
        src_fact[2] = src_rgba[3];
        src_fact[3] = src_rgba[3];
        break;
    case PVR2_BLEND_ONE_MINUS_SRC_ALPHA:
        src_fact[0] = 1.0f - src_rgba[3];
        src_fact[1] = 1.0f - src_rgba[3];
        src_fact[2] = 1.0f - src_rgba[3];
        src_fact[3] = 1.0f - src_rgba[3];
        break;
    case PVR2_BLEND_DST_ALPHA:
        src_fact[0] = dst_rgba[3];
        src_fact[1] = dst_rgba[3];
        src_fact[2] = dst_rgba[3];
        src_fact[3] = dst_rgba[3];
        break;
    case PVR2_BLEND_ONE_MINUS_DST_ALPHA:
        src_fact[0] = 1.0f - dst_rgba[3];
        src_fact[1] = 1.0f - dst_rgba[3];
        src_fact[2] = 1.0f - dst_rgba[3];
        src_fact[3] = 1.0f - dst_rgba[3];
        break;
    }

    switch (dst_blend_factor) {
    default:
        fprintf(stderr, "ERROR: dst Unknown blend factor\n");
    case PVR2_BLEND_ZERO:
        dst_fact[0] = 0.0f;
        dst_fact[1] = 0.0f;
        dst_fact[2] = 0.0f;
        dst_fact[3] = 0.0f;
        break;
    case PVR2_BLEND_ONE:
        dst_fact[0] = 1.0f;
        dst_fact[1] = 1.0f;
        dst_fact[2] = 1.0f;
        dst_fact[3] = 1.0f;
        break;
    case PVR2_BLEND_OTHER:
        dst_fact[0] = src_rgba[0];
        dst_fact[1] = src_rgba[1];
        dst_fact[2] = src_rgba[2];
        dst_fact[3] = src_rgba[3];
        break;
    case PVR2_BLEND_ONE_MINUS_OTHER:
        dst_fact[0] = 1.0f - src_rgba[0];
        dst_fact[1] = 1.0f - src_rgba[1];
        dst_fact[2] = 1.0f - src_rgba[2];
        dst_fact[3] = 1.0f - src_rgba[3];
        break;
    case PVR2_BLEND_SRC_ALPHA:
        dst_fact[0] = src_rgba[3];
        dst_fact[1] = src_rgba[3];
        dst_fact[2] = src_rgba[3];
        dst_fact[3] = src_rgba[3];
        break;
    case PVR2_BLEND_ONE_MINUS_SRC_ALPHA:
        dst_fact[0] = 1.0f - src_rgba[3];
        dst_fact[1] = 1.0f - src_rgba[3];
        dst_fact[2] = 1.0f - src_rgba[3];
        dst_fact[3] = 1.0f - src_rgba[3];
        break;
    case PVR2_BLEND_DST_ALPHA:
        dst_fact[0] = dst_rgba[3];
        dst_fact[1] = dst_rgba[3];
        dst_fact[2] = dst_rgba[3];
        dst_fact[3] = dst_rgba[3];
        break;
    case PVR2_BLEND_ONE_MINUS_DST_ALPHA:
        dst_fact[0] = 1.0f - dst_rgba[3];
        dst_fact[1] = 1.0f - dst_rgba[3];
        dst_fact[2] = 1.0f - dst_rgba[3];
        dst_fact[3] = 1.0f - dst_rgba[3];
        break;
    }

    src_rgba[0] *= src_fact[0];
    src_rgba[1] *= src_fact[1];
    src_rgba[2] *= src_fact[2];
    src_rgba[3] *= src_fact[3];

    dst_rgba[0] *= dst_fact[0];
    dst_rgba[1] *= dst_fact[1];
    dst_rgba[2] *= dst_fact[2];
    dst_rgba[3] *= dst_fact[3];

    float out_rgba[4] = {
        src_rgba[0] + dst_rgba[0],
        src_rgba[1] + dst_rgba[1],
        src_rgba[2] + dst_rgba[2],
        src_rgba[3] + dst_rgba[3]
    };

    uint32_t out32 = clamp_int(out_rgba[0] * 255, 0, 255) |
        (clamp_int(out_rgba[1] * 255, 0, 255) << 8)       |
        (clamp_int(out_rgba[2] * 255, 0, 255) << 16)      |
        (clamp_int(out_rgba[3] * 255, 0, 255) << 24);

    memcpy(((char*)obj->dat) + byte_offs, &out32, sizeof(out32));
}

static void
draw_line(struct gfx_obj *obj, int x1, int y1, int x2, int y2, uint32_t color) {
    if ((x1 < 0 && x2 < 0) ||
        (x1 >= screen_width && x2 >= screen_width) ||
        (y1 < 0 && y2 < 0) ||
        (y1 >= screen_height && y2 >= screen_height)) {
        return;
    }

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
                if (user_clip_test(x_pos, y_pos) && clip_test(x_pos, y_pos))
                    put_pix(obj, x_pos, y_pos, color);
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
                if (user_clip_test(x_pos, y_pos) && clip_test(x_pos, y_pos))
                    put_pix(obj, x_pos, y_pos, color);
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
                if (user_clip_test(x_pos, y_pos) && clip_test(x_pos, y_pos))
                    put_pix(obj, x_pos, y_pos, color);
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
                if (user_clip_test(x_pos, y_pos) && clip_test(x_pos, y_pos))
                    put_pix(obj, x_pos, y_pos, color);
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

/*
 * returns 2 * triangle's area
 *
 * it's alright for it to return it multiplied by 2 because the division
 * works out so that it doesn't actually matter since the *2 part ends up
 * in both the dividend and the divisor.
 */
static float
tri_area2_signed(float const v1[2], float const v2[2], float const v3[2]) {
    float vec1[2] = { v2[0] - v1[0], v2[1] - v1[1] };
    float vec2[2] = { v3[0] - v1[0], v3[1] - v1[1] };

    return -vec1[1] * vec2[0] + vec1[0] * vec2[1];
}

static bool depth_test(int x_pos, int y_pos, float w_coord) {
    float w_ref = w_buffer[y_pos * screen_width + x_pos];
    if (sort_mode_enable)
        return w_coord >= w_ref;
    switch (rend_param.depth_func) {
    case PVR2_DEPTH_NEVER:
        return false;
    case PVR2_DEPTH_LESS:
        return w_coord < w_ref;
    case PVR2_DEPTH_EQUAL:
        return w_coord == w_ref;
    case PVR2_DEPTH_LEQUAL:
        return w_coord <= w_ref;
    case PVR2_DEPTH_GREATER:
        return w_coord > w_ref;
    case PVR2_DEPTH_NOTEQUAL:
        return w_coord != w_ref;
    case PVR2_DEPTH_GEQUAL:
        return w_coord >= w_ref;
    case PVR2_DEPTH_ALWAYS:
        return true;
    default:
        fprintf(stderr, "Unknown depth function %d!\n",
                (int)rend_param.depth_func);
        return true;
    }
}

static bool user_clip_test(int x_pix, int y_pix) {
    switch (rend_param.user_clip_mode) {
    case GFX_USER_CLIP_INSIDE:
        if (x_pix < user_clip[0] ||
            x_pix > user_clip[2] ||
            y_pix < user_clip[1] ||
            y_pix > user_clip[3])
            return false;
        break;
    case GFX_USER_CLIP_OUTSIDE:
        if (!(x_pix < user_clip[0] ||
              x_pix > user_clip[2] ||
              y_pix < user_clip[1] ||
              y_pix > user_clip[3]))
            return false;
        break;
    case GFX_USER_CLIP_DISABLE:
    default:
        break;
    }

    return true;
}

static bool clip_test(int x_pix, int y_pix) {
    if (x_pix < clip[0] ||
        x_pix > clip[2] ||
        y_pix < clip[1] ||
        y_pix > clip[3])
        return false;
    return true;
}

static void
tex_sample(struct tex const *texp, float rgba[4], int const texcoord[2]) {
    if (texp->obj_no < 0) {
        fprintf(stderr, "%s - invalid texture/object binding %d\n", __func__, texp->obj_no);
        rgba[0] = 1.0f;
        rgba[1] = 1.0f;
        rgba[2] = 1.0f;
        rgba[3] = 1.0f;
        return;
    }

    struct gfx_obj *obj = gfx_obj_get(texp->obj_no);

    int uv[2];

    switch (rend_param.tex_wrap_mode[0]) {
    case TEX_WRAP_CLAMP:
        uv[0] = clamp_int(texcoord[0], 0, texp->width - 1);
        break;
    case TEX_WRAP_REPEAT:
        uv[0] = texcoord[0] % texp->width;
        break;
    case TEX_WRAP_FLIP:
        if ((texcoord[0] / texp->width) % 2 == 0)
            uv[0] = texcoord[0] % texp->width;
        else
            uv[0] = texp->width - 1 - (texcoord[0] % texp->width);
        break;
    default:
        fprintf(stderr, "%s - invalid tex clamp mode\n", __func__);
        rgba[0] = 1.0f;
        rgba[1] = 1.0f;
        rgba[2] = 1.0f;
        rgba[3] = 1.0f;
        return;
    }

    switch (rend_param.tex_wrap_mode[1]) {
    case TEX_WRAP_CLAMP:
        uv[1] = clamp_int(texcoord[1], 0, texp->height - 1);
        break;
    case TEX_WRAP_REPEAT:
        uv[1] = texcoord[1] % texp->height;
        break;
    case TEX_WRAP_FLIP:
        if ((texcoord[1] / texp->height) % 2 == 0)
            uv[1] = texcoord[1] % texp->height;
        else
            uv[1] = texp->height - 1 - (texcoord[1] % texp->height);
        break;
    default:
        fprintf(stderr, "%s - invalid tex clamp mode\n", __func__);
        rgba[0] = 1.0f;
        rgba[1] = 1.0f;
        rgba[2] = 1.0f;
        rgba[3] = 1.0f;
        return;
    }

    unsigned tex_idx = uv[1] * texp->width + uv[0];

    switch (texp->fmt) {
    case GFX_TEX_FMT_ARGB_1555:
        {
            if (tex_idx * sizeof(uint16_t) + (sizeof(uint16_t) - 1) >=
                obj->dat_len || !obj->dat) {
                fprintf(stderr, "%s - buffer overflow\n", __func__);
                fprintf(stderr, "\tdat_len %llu\n",
                        (unsigned long long)obj->dat_len);
                fprintf(stderr, "\ttex_idx: %u\n", tex_idx);
                rgba[0] = 1.0f;
                rgba[1] = 1.0f;
                rgba[2] = 1.0f;
                rgba[3] = 1.0f;
                return;
            }
            uint16_t val;
            memcpy(&val, ((char*)obj->dat) + sizeof(uint16_t) * tex_idx,
                   sizeof(val));

            rgba[0] = ((val >> 10) & 0x1f) / 31.0f;
            rgba[1] = ((val >> 5) & 0x1f) / 31.0f;
            rgba[2] = ((val >> 0) & 0x1f) / 31.0f;
            rgba[3] = val & 0x8000 ? 1.0f : 0.0f;
        }
        return;
    case GFX_TEX_FMT_ARGB_4444:
        {
            if (tex_idx * sizeof(uint16_t) + (sizeof(uint16_t) - 1) >=
                obj->dat_len || !obj->dat) {
                fprintf(stderr, "%s - buffer overflow\n", __func__);
                fprintf(stderr, "\tdat_len %llu\n",
                        (unsigned long long)obj->dat_len);
                fprintf(stderr, "\ttex_idx: %u\n", tex_idx);
                rgba[0] = 1.0f;
                rgba[1] = 1.0f;
                rgba[2] = 1.0f;
                rgba[3] = 1.0f;
                return;
            }
            uint16_t val;
            memcpy(&val, ((char*)obj->dat) + sizeof(uint16_t) * tex_idx,
                   sizeof(val));

            rgba[0] = ((val >> 8) & 0xf) / 15.0f;
            rgba[1] = ((val >> 4) & 0xf) / 15.0f;
            rgba[2] = ((val >> 0) & 0xf) / 15.0f;
            rgba[3] = ((val >> 12) & 0xf) / 15.0f;
        }
        return;
    case GFX_TEX_FMT_RGB_565:
        {
            if (tex_idx * sizeof(uint16_t) + (sizeof(uint16_t) - 1) >=
                obj->dat_len || !obj->dat) {
                fprintf(stderr, "%s - buffer overflow\n", __func__);
                fprintf(stderr, "\tdat_len %llu\n",
                        (unsigned long long)obj->dat_len);
                fprintf(stderr, "\ttex_idx: %u\n", tex_idx);
                rgba[0] = 1.0f;
                rgba[1] = 1.0f;
                rgba[2] = 1.0f;
                rgba[3] = 1.0f;
                return;
            }
            uint16_t val;
            memcpy(&val, ((char*)obj->dat) + sizeof(uint16_t) * tex_idx,
                   sizeof(val));

            rgba[0] = ((val >> 11) & 0x1f) / 31.0f;
            rgba[1] = ((val >> 5) & 0x3f) / 63.0f;
            rgba[2] = ((val >> 0) & 0x1f) / 31.0f;
            rgba[3] = 1.0f;
        }
        return;
    case GFX_TEX_FMT_YUV_422:
        {
            if ((tex_idx / 2) * sizeof(uint32_t) + (sizeof(uint32_t) - 1) >=
                obj->dat_len || !obj->dat) {
                fprintf(stderr, "%s - buffer overflow\n", __func__);
                fprintf(stderr, "\tdat_len %llu\n",
                        (unsigned long long)obj->dat_len);
                fprintf(stderr, "\ttex_idx: %u\n", tex_idx);
                rgba[0] = 1.0f;
                rgba[1] = 1.0f;
                rgba[2] = 1.0f;
                rgba[3] = 1.0f;
                return;
            }

            uint32_t val;
            memcpy(&val, ((char*)obj->dat) + sizeof(uint32_t) * (tex_idx / 2),
                   sizeof(val));

            unsigned lum;
            int chrom_b = val & 0xff, chrom_r = (val >> 16) & 0xff;

            chrom_b -= 128;
            chrom_r -= 128;

            if (texcoord[0] % 2)
                lum = (val >> 24) & 0xff;
            else
                lum = (val >> 8) & 0xff;

            int adds[3] = {
                           (0x16000  * chrom_r) >> 16,
                           -((0x5800 * chrom_b + 0xb000 * chrom_r) >> 16),
                           (0x1b800 * chrom_b) >> 16
            };
            rgba[0] = clamp_int(lum + adds[0], 0, 255) / 255.0f;
            rgba[1] = clamp_int(lum + adds[1], 0, 255) / 255.0f;
            rgba[2] = clamp_int(lum + adds[2], 0, 255) / 255.0f;
            rgba[3] = 1.0f;
        }
        return;
    case GFX_TEX_FMT_ARGB_8888:
        {
            if (tex_idx * sizeof(uint32_t) + (sizeof(uint32_t) - 1) >=
                obj->dat_len || !obj->dat) {
                fprintf(stderr, "%s - buffer overflow\n", __func__);
                fprintf(stderr, "\tdat_len %llu\n",
                        (unsigned long long)obj->dat_len);
                fprintf(stderr, "\ttex_idx: %u\n", tex_idx);
                rgba[0] = 1.0f;
                rgba[1] = 1.0f;
                rgba[2] = 1.0f;
                rgba[3] = 1.0f;
                return;
            }
            uint32_t val;
            memcpy(&val, ((char*)obj->dat) + sizeof(uint32_t) * tex_idx,
                   sizeof(val));

            rgba[0] = ((val >> 16) & 0xff) / 255.0f;
            rgba[1] = ((val >> 8) & 0xff) / 255.0f;
            rgba[2] = ((val >> 0) & 0xff) / 255.0f;
            rgba[3] = ((val >> 24) & 0xff) / 255.0f;
        }
        return;
    default:
        fprintf(stderr, "%s - unimplemented tex format %d\n",
                __func__, (int)texp->fmt);
        rgba[0] = 1.0f;
        rgba[1] = 1.0f;
        rgba[2] = 1.0f;
        rgba[3] = 1.0f;

        abort();

        return;
    }
}

/*
 * per-vertex attribute interpolated across a triangle
 *
 * TODO: find a way to use float here and in draw_tri instead
 * of double without causing a bunch of really obnoxious texture
 * sampling artifacts.  float's lack of precision has noticeable
 * effects here.  Daytona USA 2001 is a good test-case
 * (look closely at menus and dev logos).
 */
struct vert_attr {
    double init, ystep, xstep;
};

static double vert_attr_val(struct vert_attr const *attr, int y_pos, int x_pos) {
    return attr->init + y_pos * attr->ystep + x_pos * attr->xstep;
}

static void
draw_tri(struct gfx_obj *obj, float const *p1,
         float const *p2, float const *p3) {
    float bbox_float[4];
    tri_bbox(bbox_float, p1, p2, p3);

    int bbox[4] = { bbox_float[0], bbox_float[1],
                    bbox_float[2], bbox_float[3] };

    if (bbox[0] < 0)
        bbox[0] = 0;
    else if (bbox[0] >= screen_width)
        return;
    if (bbox[1] < 0)
        bbox[1] = 0;
    else if (bbox[1] >= screen_height)
        return;
    if (bbox[2] >= screen_width)
        bbox[2] = screen_width - 1;
    else if (bbox[2] < 0)
        return;
    if (bbox[3] >= screen_height)
        bbox[3] = screen_height - 1;
    else if (bbox[3] < 0)
        return;

    /*
     * positive is counter-clockwise and negative is clockwise
     *
     * except the y-coordinate is inverted so really it's the other way
     * around.
     */
    float area = tri_area2_signed(p1, p2, p3);
    if (area < 0.0f) {
        float const *tmp = p3;
        p3 = p2;
        p2 = tmp;
        area = -area;
    }

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

    // perspective-correct base color
    double p1_base_col[4] = {
        p1[GFX_VERT_BASE_COLOR_OFFSET] * p1[2],
        p1[GFX_VERT_BASE_COLOR_OFFSET + 1] * p1[2],
        p1[GFX_VERT_BASE_COLOR_OFFSET + 2] * p1[2],
        p1[GFX_VERT_BASE_COLOR_OFFSET + 3] * p1[2]
    };
    double p2_base_col[4] = {
        p2[GFX_VERT_BASE_COLOR_OFFSET] * p2[2],
        p2[GFX_VERT_BASE_COLOR_OFFSET + 1] * p2[2],
        p2[GFX_VERT_BASE_COLOR_OFFSET + 2] * p2[2],
        p2[GFX_VERT_BASE_COLOR_OFFSET + 3] * p2[2]
    };
    double p3_base_col[4] = {
        p3[GFX_VERT_BASE_COLOR_OFFSET] * p3[2],
        p3[GFX_VERT_BASE_COLOR_OFFSET + 1] * p3[2],
        p3[GFX_VERT_BASE_COLOR_OFFSET + 2] * p3[2],
        p3[GFX_VERT_BASE_COLOR_OFFSET + 3] * p3[2]
    };

    // perspective-correct offset color
    double p1_offs_col[4] = {
        p1[GFX_VERT_OFFS_COLOR_OFFSET] * p1[2],
        p1[GFX_VERT_OFFS_COLOR_OFFSET + 1] * p1[2],
        p1[GFX_VERT_OFFS_COLOR_OFFSET + 2] * p1[2],
        p1[GFX_VERT_OFFS_COLOR_OFFSET + 3] * p1[2]
    };
    double p2_offs_col[4] = {
        p2[GFX_VERT_OFFS_COLOR_OFFSET] * p2[2],
        p2[GFX_VERT_OFFS_COLOR_OFFSET + 1] * p2[2],
        p2[GFX_VERT_OFFS_COLOR_OFFSET + 2] * p2[2],
        p2[GFX_VERT_OFFS_COLOR_OFFSET + 3] * p2[2]
    };
    double p3_offs_col[4] = {
        p3[GFX_VERT_OFFS_COLOR_OFFSET] * p3[2],
        p3[GFX_VERT_OFFS_COLOR_OFFSET + 1] * p3[2],
        p3[GFX_VERT_OFFS_COLOR_OFFSET + 2] * p3[2],
        p3[GFX_VERT_OFFS_COLOR_OFFSET + 3] * p3[2]
    };

    double texmat[4] = {
        rend_param.tex_transform[0],
        rend_param.tex_transform[1],
        rend_param.tex_transform[2],
        rend_param.tex_transform[3]
    };

    // perspective-correct texture coordinates
    double p1_texcoord[2] = {
        (p1[GFX_VERT_TEX_COORD_OFFSET] * texmat[0] +
         p1[GFX_VERT_TEX_COORD_OFFSET + 1] * texmat[1]) * p1[2],

        (p1[GFX_VERT_TEX_COORD_OFFSET] * texmat[2] +
         p1[GFX_VERT_TEX_COORD_OFFSET + 1] * texmat[3]) * p1[2]
    };
    double p2_texcoord[2] = {
        (p2[GFX_VERT_TEX_COORD_OFFSET] * texmat[0] +
         p2[GFX_VERT_TEX_COORD_OFFSET + 1] * texmat[1]) * p2[2],

        (p2[GFX_VERT_TEX_COORD_OFFSET] * texmat[2] +
         p2[GFX_VERT_TEX_COORD_OFFSET + 1] * texmat[3]) * p2[2],
    };
    double p3_texcoord[2] = {
        (p3[GFX_VERT_TEX_COORD_OFFSET] * texmat[0] +
         p3[GFX_VERT_TEX_COORD_OFFSET + 1] * texmat[1]) * p3[2],

        (p3[GFX_VERT_TEX_COORD_OFFSET] * texmat[2] +
         p3[GFX_VERT_TEX_COORD_OFFSET + 1] * texmat[3]) * p3[2],
    };

    struct tex *texp = NULL;
    if (rend_param.tex_enable) {
        if (rend_param.tex_idx < GFX_TEX_CACHE_SIZE) {
            if (textures[rend_param.tex_idx].obj_no >= 0 &&
                textures[rend_param.tex_idx].obj_no < GFX_OBJ_COUNT)
                texp = textures + rend_param.tex_idx;
            else
                fprintf(stderr, "%s - texture %d not bound to object\n",
                        __func__, rend_param.tex_idx);
        } else {
            fprintf(stderr, "%s - invalid tex_idx %u\n",
                    __func__, rend_param.tex_idx);
        }
    }

    float dist_xstep[3] = { e1[0], e2[0], e3[0] };
    float dist_ystep[3] = { e1[1], e2[1], e3[1] };
    float dist_init[3] = {
        e1[0] * bbox[0] + e1[1] * bbox[1] + e1[2],
        e2[0] * bbox[0] + e2[1] * bbox[1] + e2[2],
        e3[0] * bbox[0] + e3[1] * bbox[1] + e3[2]
    };

    /*
     * barycentric coordinate * area of entire triangle
     *
     * this is basically a pseudo-attribute, we don't need it for anything
     * but it's a compoment in several other variables.
     */
    double bary_area_xstep[3] = {
        p2[1] - p3[1],
        p3[1] - p1[1],
        p1[1] - p2[1]
    };
    double bary_area_ystep[3] = {
        p3[0] - p2[0],
        p1[0] - p3[0],
        p2[0] - p1[0]
    };
    double bary_area_init[3] = {
        (bbox[0] - p2[0]) * bary_area_xstep[0] +
        (bbox[1] - p2[1]) * bary_area_ystep[0],
        (bbox[0] - p3[0]) * bary_area_xstep[1] +
        (bbox[1] - p3[1]) * bary_area_ystep[1],
        (bbox[0] - p1[0]) * bary_area_xstep[2] +
        (bbox[1] - p1[1]) * bary_area_ystep[2]
    };

    double w_coord_xstep =
        bary_area_xstep[0] * (p1[2] / area) +
        bary_area_xstep[1] * (p2[2] / area) +
        bary_area_xstep[2] * (p3[2] / area);
    double w_coord_ystep =
        bary_area_ystep[0] * (p1[2] / area) +
        bary_area_ystep[1] * (p2[2] / area) +
        bary_area_ystep[2] * (p3[2] / area);
    double w_coord_init =
        bary_area_init[0] * (p1[2] / area) +
        bary_area_init[1] * (p2[2] / area) +
        bary_area_init[2] * (p3[2] / area);

    struct vert_attr w_coord_area_attr = {
        // init
        bary_area_init[0] * p1[2] +
        bary_area_init[1] * p2[2] +
        bary_area_init[2] * p3[2],

        // ystep
        bary_area_ystep[0] * p1[2] +
        bary_area_ystep[1] * p2[2] +
        bary_area_ystep[2] * p3[2],

        // xstep
        bary_area_xstep[0] * p1[2] +
        bary_area_xstep[1] * p2[2] +
        bary_area_xstep[2] * p3[2]
    };

    struct vert_attr texcoord_attr[2] = {
        {
            // init
            bary_area_init[0] * p1_texcoord[0] +
            bary_area_init[1] * p2_texcoord[0] +
            bary_area_init[2] * p3_texcoord[0],

            // ystep
            bary_area_ystep[0] * p1_texcoord[0] +
            bary_area_ystep[1] * p2_texcoord[0] +
            bary_area_ystep[2] * p3_texcoord[0],

            // xstep
            bary_area_xstep[0] * p1_texcoord[0] +
            bary_area_xstep[1] * p2_texcoord[0] +
            bary_area_xstep[2] * p3_texcoord[0],
        },
        {
            // init
            bary_area_init[0] * p1_texcoord[1] +
            bary_area_init[1] * p2_texcoord[1] +
            bary_area_init[2] * p3_texcoord[1],

            // ystep
            bary_area_ystep[0] * p1_texcoord[1] +
            bary_area_ystep[1] * p2_texcoord[1] +
            bary_area_ystep[2] * p3_texcoord[1],

            // xstep
            bary_area_xstep[0] * p1_texcoord[1] +
            bary_area_xstep[1] * p2_texcoord[1] +
            bary_area_xstep[2] * p3_texcoord[1]
        }
    };

    struct vert_attr base_col_attr[4] = {
        {
            // init
            bary_area_init[0] * p1_base_col[0] +
            bary_area_init[1] * p2_base_col[0] +
            bary_area_init[2] * p3_base_col[0],

            // ystep
            bary_area_ystep[0] * p1_base_col[0] +
            bary_area_ystep[1] * p2_base_col[0] +
            bary_area_ystep[2] * p3_base_col[0],

            // xstep
            bary_area_xstep[0] * p1_base_col[0] +
            bary_area_xstep[1] * p2_base_col[0] +
            bary_area_xstep[2] * p3_base_col[0]
        },
        {
            // init
            bary_area_init[0] * p1_base_col[1] +
            bary_area_init[1] * p2_base_col[1] +
            bary_area_init[2] * p3_base_col[1],

            // ystep
            bary_area_ystep[0] * p1_base_col[1] +
            bary_area_ystep[1] * p2_base_col[1] +
            bary_area_ystep[2] * p3_base_col[1],

            // xstep
            bary_area_xstep[0] * p1_base_col[1] +
            bary_area_xstep[1] * p2_base_col[1] +
            bary_area_xstep[2] * p3_base_col[1]
        },
        {
            // init
            bary_area_init[0] * p1_base_col[2] +
            bary_area_init[1] * p2_base_col[2] +
            bary_area_init[2] * p3_base_col[2],

            // ystep
            bary_area_ystep[0] * p1_base_col[2] +
            bary_area_ystep[1] * p2_base_col[2] +
            bary_area_ystep[2] * p3_base_col[2],

            // xstep
            bary_area_xstep[0] * p1_base_col[2] +
            bary_area_xstep[1] * p2_base_col[2] +
            bary_area_xstep[2] * p3_base_col[2]
        },
        {
            // init
            bary_area_init[0] * p1_base_col[3] +
            bary_area_init[1] * p2_base_col[3] +
            bary_area_init[2] * p3_base_col[3],

            // ystep
            bary_area_ystep[0] * p1_base_col[3] +
            bary_area_ystep[1] * p2_base_col[3] +
            bary_area_ystep[2] * p3_base_col[3],

            // xstep
            bary_area_xstep[0] * p1_base_col[3] +
            bary_area_xstep[1] * p2_base_col[3] +
            bary_area_xstep[2] * p3_base_col[3]
        }
    };

    struct vert_attr offs_col_attr[4] = {
        {
            // init
            bary_area_init[0] * p1_offs_col[0] +
            bary_area_init[1] * p2_offs_col[0] +
            bary_area_init[2] * p3_offs_col[0],

            // ystep
            bary_area_ystep[0] * p1_offs_col[0] +
            bary_area_ystep[1] * p2_offs_col[0] +
            bary_area_ystep[2] * p3_offs_col[0],

            // xstep
            bary_area_xstep[0] * p1_offs_col[0] +
            bary_area_xstep[1] * p2_offs_col[0] +
            bary_area_xstep[2] * p3_offs_col[0],
        },
        {
            // init
            bary_area_init[0] * p1_offs_col[1] +
            bary_area_init[1] * p2_offs_col[1] +
            bary_area_init[2] * p3_offs_col[1],

            // ystep
            bary_area_ystep[0] * p1_offs_col[1] +
            bary_area_ystep[1] * p2_offs_col[1] +
            bary_area_ystep[2] * p3_offs_col[1],

            // xstep
            bary_area_xstep[0] * p1_offs_col[1] +
            bary_area_xstep[1] * p2_offs_col[1] +
            bary_area_xstep[2] * p3_offs_col[1],
        },
        {
            // init
            bary_area_init[0] * p1_offs_col[2] +
            bary_area_init[1] * p2_offs_col[2] +
            bary_area_init[2] * p3_offs_col[2],

            // ystep
            bary_area_ystep[0] * p1_offs_col[2] +
            bary_area_ystep[1] * p2_offs_col[2] +
            bary_area_ystep[2] * p3_offs_col[2],

            // xstep
            bary_area_xstep[0] * p1_offs_col[2] +
            bary_area_xstep[1] * p2_offs_col[2] +
            bary_area_xstep[2] * p3_offs_col[2],
        },
        {
            // init
            bary_area_init[0] * p1_offs_col[3] +
            bary_area_init[1] * p2_offs_col[3] +
            bary_area_init[2] * p3_offs_col[3],

            // ystep
            bary_area_ystep[0] * p1_offs_col[3] +
            bary_area_ystep[1] * p2_offs_col[3] +
            bary_area_ystep[2] * p3_offs_col[3],

            // xstep
            bary_area_xstep[0] * p1_offs_col[3] +
            bary_area_xstep[1] * p2_offs_col[3] +
            bary_area_xstep[2] * p3_offs_col[3],
        }
    };

    int x_pos, y_pos;
    for (y_pos = bbox[1]; y_pos <= bbox[3]; y_pos++) {
        int y_offs = y_pos - bbox[1];
        float dist_row_val[3] = {
            dist_init[0] + y_offs * dist_ystep[0],
            dist_init[1] + y_offs * dist_ystep[1],
            dist_init[2] + y_offs * dist_ystep[2]
        };
        for (x_pos = bbox[0]; x_pos <= bbox[2]; x_pos++) {
            int x_offs = x_pos - bbox[0];

            if (x_offs * dist_xstep[0] >= -dist_row_val[0] &&
                x_offs * dist_xstep[1] >= -dist_row_val[1] &&
                x_offs * dist_xstep[2] >= -dist_row_val[2]) {

                // reciprocal depth * area
                double w_coord_area =
                    vert_attr_val(&w_coord_area_attr, y_offs, x_offs);

                // reciprocal depth
                float w_coord = w_coord_init +
                    y_offs * w_coord_ystep +
                    x_offs * w_coord_xstep;

                if ((!sort_mode_enable &&
                     !depth_test(x_pos, y_pos, w_coord)) ||
                    !user_clip_test(x_pos, y_pos) ||
                    !clip_test(x_pos, y_pos))
                    continue;

                if (rend_param.enable_depth_writes && !sort_mode_enable)
                    w_buffer[y_pos * screen_width + x_pos] = w_coord;

                double base_col[4] = {
                    vert_attr_val(base_col_attr + 0, y_offs, x_offs),
                    vert_attr_val(base_col_attr + 1, y_offs, x_offs),
                    vert_attr_val(base_col_attr + 2, y_offs, x_offs),
                    vert_attr_val(base_col_attr + 3, y_offs, x_offs)
                };

                base_col[0] /= w_coord_area;
                base_col[1] /= w_coord_area;
                base_col[2] /= w_coord_area;
                base_col[3] /= w_coord_area;

                double offs_col[4] = {
                    vert_attr_val(offs_col_attr + 0, y_offs, x_offs),
                    vert_attr_val(offs_col_attr + 1, y_offs, x_offs),
                    vert_attr_val(offs_col_attr + 2, y_offs, x_offs),
                    vert_attr_val(offs_col_attr + 3, y_offs, x_offs)
                };

                offs_col[0] /= w_coord_area;
                offs_col[1] /= w_coord_area;
                offs_col[2] /= w_coord_area;
                offs_col[3] /= w_coord_area;

                double pix_color[4];

                if (texp) {
                    double texcoord[2] = {
                        vert_attr_val(texcoord_attr + 0, y_offs, x_offs),
                        vert_attr_val(texcoord_attr + 1, y_offs, x_offs),
                    };

                    texcoord[0] /= w_coord_area;
                    texcoord[1] /= w_coord_area;

                    float sample[4];
                    switch (rend_param.tex_filter) {
                    case TEX_FILTER_TRILINEAR_A:
                    case TEX_FILTER_TRILINEAR_B:
                        // TODO: TRILINEAR FILTERING
                    case TEX_FILTER_BILINEAR:
                        // TODO: BILINEAR FILTERING
                    case TEX_FILTER_NEAREST:
                        {
                            int texcoord_pix[2] = {
                                texcoord[0] * texp->width,
                                texcoord[1] * texp->height
                            };

                            tex_sample(texp, sample, texcoord_pix);
                        }
                        break;
                    default:
                        fprintf(stderr, "%s - invalid texture filter %d\n",
                                __func__, (int)rend_param.tex_filter);
                        abort();
                    }

                    switch (rend_param.tex_inst) {
                    case TEX_INST_DECAL:
                        pix_color[0] = sample[0] + offs_col[0];
                        pix_color[1] = sample[1] + offs_col[1];
                        pix_color[2] = sample[2] + offs_col[2];
                        pix_color[3] = sample[3];
                        break;
                    case TEX_INST_MOD:
                        pix_color[0] = sample[0] * base_col[0] + offs_col[0];
                        pix_color[1] = sample[1] * base_col[1] + offs_col[1];
                        pix_color[2] = sample[2] * base_col[2] + offs_col[2];
                        pix_color[3] = sample[3];
                        break;
                    case TEXT_INST_DECAL_ALPHA:
                        pix_color[0] = sample[0] * sample[3] +
                            base_col[0] * (1.0 - sample[3]) + offs_col[0];
                        pix_color[1] = sample[1] * sample[3] +
                            base_col[1] * (1.0 - sample[3]) + offs_col[1];
                        pix_color[2] = sample[2] * sample[3] +
                            base_col[2] * (1.0 - sample[3]) + offs_col[2];
                        pix_color[3] = base_col[3];
                        break;
                    case TEX_INST_MOD_ALPHA:
                        pix_color[0] = sample[0] * base_col[0] + offs_col[0];
                        pix_color[1] = sample[1] * base_col[1] + offs_col[1];
                        pix_color[2] = sample[2] * base_col[2] + offs_col[2];
                        pix_color[3] = sample[3] * base_col[3];
                        break;
                    default:
                        fprintf(stderr, "unknown texture inst %d\n",
                                (int)rend_param.tex_inst);
                        pix_color[0] = 1.0;
                        pix_color[1] = 1.0;
                        pix_color[2] = 1.0;
                        pix_color[3] = 1.0;
                    }
                } else {
                    memcpy(pix_color, base_col, sizeof(pix_color));
                }

                int rgba[4] = {
                    clamp_int(pix_color[0] * 255, 0, 255),
                    clamp_int(pix_color[1] * 255, 0, 255),
                    clamp_int(pix_color[2] * 255, 0, 255),
                    clamp_int(pix_color[3] * 255, 0, 255)
                };

                if (sort_mode_enable) {
                    if (x_pos < 0 || x_pos >= FB_WIDTH ||
                        y_pos < 0 || y_pos >= FB_HEIGHT ||
                        n_oit_pixels >= MAX_OIT_PIXELS)
                        continue;
                    unsigned oit_node_idx = n_oit_pixels++;
                    struct oit_pixel *pix = oit_pixels + oit_node_idx;
                    memcpy(pix->rgba, rgba, sizeof(pix->rgba));
                    pix->w_coord = w_coord;
                    pix->src_blend_factor = rend_param.src_blend_factor;
                    pix->dst_blend_factor = rend_param.dst_blend_factor;
                    pix->next_pix_idx = oit_buf[y_pos * FB_WIDTH + x_pos];
                    oit_buf[y_pos * FB_WIDTH + x_pos] = oit_node_idx;
                } else if (blend_enable) {
                    put_pix_blended(obj, x_pos, y_pos,
                                    rgba[0]          |
                                    (rgba[1] << 8)   |
                                    (rgba[2] << 16)  |
                                    (rgba[3] << 24),
                                    rend_param.src_blend_factor,
                                    rend_param.dst_blend_factor);
                } else {
                    put_pix(obj, x_pos, y_pos,
                            rgba[0]          |
                            (rgba[1] << 8)   |
                            (rgba[2] << 16)  |
                            (rgba[3] << 24));
                }
            }
        }
    }
}

static void soft_gfx_set_vert_array(struct gfx_il_inst *cmd) {
    if (render_tgt < 0) {
        fprintf(stderr, "%s - no render target bound!\n", __func__);
        return;
    }

    unsigned n_verts = cmd->arg.set_vert_array.n_verts;
    float const *verts = cmd->arg.set_vert_array.verts;

    if (!n_verts) {
        free(vert_array);
        vert_array = NULL;
        vert_array_len = 0;
        return;
    }
    size_t bytes_per_vert = sizeof(float) * (size_t)GFX_VERT_LEN;
    if (SIZE_MAX / n_verts < bytes_per_vert) {
        // overflow
        free(vert_array);
        vert_array = NULL;
        vert_array_len = 0;
        return;
    }
    float *new_vert_array = realloc(vert_array, bytes_per_vert * n_verts);
    if (!new_vert_array) {
        // failed alloc
        vert_array_len = 0;
        free(vert_array);
        vert_array = NULL;
        return;
    }
    vert_array = new_vert_array;
    memcpy(vert_array, verts, bytes_per_vert * n_verts);
    vert_array_len = n_verts;
}

static void soft_gfx_draw_vert_array(struct gfx_il_inst *cmd) {
    unsigned n_verts = cmd->arg.draw_vert_array.n_verts;
    unsigned first_idx = cmd->arg.draw_vert_array.first_idx;
    unsigned last_idx = first_idx + (n_verts - 1);

    if (!n_verts || !vert_array || last_idx >= vert_array_len)
        return;

    struct gfx_obj *obj = gfx_obj_get(render_tgt);
    unsigned cur_idx;
    float const *tri_buf[2];
    unsigned tri_buf_len = 0;

    if (wireframe_mode) {
        /*
         * draw triangles as white lines with no depth testing or
         * per-vertex attributes
         */
        for (cur_idx = first_idx; cur_idx <= last_idx; cur_idx++) {
            if (tri_buf_len == 2) {
                float const *newvert = vert_array + cur_idx * GFX_VERT_LEN;

                draw_line(obj, tri_buf[0][0], tri_buf[0][1], tri_buf[1][0], tri_buf[1][1], 0xffffffff);
                draw_line(obj, tri_buf[1][0], tri_buf[1][1], newvert[0], newvert[1], 0xffffffff);
                draw_line(obj, newvert[0], newvert[1], tri_buf[0][0], tri_buf[0][1], 0xffffffff);

                tri_buf[0] = tri_buf[1];
                tri_buf[1] = newvert;
            } else {
                tri_buf[tri_buf_len++] = vert_array + cur_idx * GFX_VERT_LEN;
            }
        }
    } else {
        bool odd = false;
        for (cur_idx = first_idx; cur_idx <= last_idx; cur_idx++) {
            if (tri_buf_len == 2) {
                /*
                 * reverse winding order on every other triangle so that they all
                 * have the same winding order.
                 *
                 * This is not strictly necessary since draw_tri can handle either
                 * winding order but I want to keep things consistent for when I
                 * eventually implement culling.
                 */
                float const *newvert = vert_array + cur_idx * GFX_VERT_LEN;
                if (odd)
                    draw_tri(obj, tri_buf[1], tri_buf[0], newvert);
                else
                    draw_tri(obj, tri_buf[0], tri_buf[1], newvert);
                odd = !odd;

                tri_buf[0] = tri_buf[1];
                tri_buf[1] = newvert;
            } else {
                tri_buf[tri_buf_len++] = vert_array + cur_idx * GFX_VERT_LEN;
            }
        }
    }
}

static void soft_gfx_bind_tex(struct gfx_il_inst *cmd) {
    unsigned tex_no = cmd->arg.bind_tex.tex_no;
    int obj_handle = cmd->arg.bind_tex.gfx_obj_handle;
    unsigned width = cmd->arg.bind_tex.width;
    unsigned height = cmd->arg.bind_tex.height;
    enum gfx_tex_fmt pix_fmt = cmd->arg.bind_tex.pix_fmt;

    if (tex_no >= GFX_TEX_CACHE_SIZE) {
        fprintf(stderr, "%s - invalid texture handle %u\n", __func__, tex_no);
    } else {
        struct tex *texp = textures + tex_no;

        texp->obj_no = obj_handle;
        texp->width = width;
        texp->height = height;
        texp->fmt = pix_fmt;
    }
}

static void soft_gfx_unbind_tex(struct gfx_il_inst *cmd) {
    unsigned tex_no = cmd->arg.unbind_tex.tex_no;
    if (tex_no >= GFX_TEX_CACHE_SIZE) {
        fprintf(stderr, "%s - invalid texture handle %u\n", __func__, tex_no);
    } else {
        textures[tex_no].obj_no = -1;
    }
}

static void sort_oit_pix_list(int first_idx) {
    int src_idx = first_idx;
    struct oit_pixel *srcp = oit_pixels + src_idx;
    struct oit_pixel *cmpp = srcp;
    int cmp_idx;

    while ((cmp_idx = cmpp->next_pix_idx) >= 0) {
        cmpp = oit_pixels + cmp_idx;
        if (cmpp->w_coord < srcp->w_coord) {
            int rgba_tmp[4];
            memcpy(rgba_tmp, cmpp->rgba, sizeof(rgba_tmp));
            memcpy(cmpp->rgba, srcp->rgba, sizeof(cmpp->rgba));
            memcpy(srcp->rgba, rgba_tmp, sizeof(srcp->rgba));
            float w_tmp;
            w_tmp = cmpp->w_coord;
            cmpp->w_coord = srcp->w_coord;
            srcp->w_coord = w_tmp;

            enum Pvr2BlendFactor src_blend_tmp, dst_blend_tmp;
            src_blend_tmp = cmpp->src_blend_factor;
            dst_blend_tmp = cmpp->dst_blend_factor;
            cmpp->src_blend_factor = srcp->src_blend_factor;
            cmpp->dst_blend_factor = srcp->dst_blend_factor;
            srcp->src_blend_factor = src_blend_tmp;
            srcp->dst_blend_factor = dst_blend_tmp;
        }
    }

    if (srcp->next_pix_idx >= 0)
        sort_oit_pix_list(srcp->next_pix_idx);
}

static void soft_gfx_exec_gfx_il(struct gfx_il_inst *cmd, unsigned n_cmd) {
    while (n_cmd--) {
        switch (cmd->op) {
        case GFX_IL_BIND_TEX:
            soft_gfx_bind_tex(cmd);
            break;
        case GFX_IL_UNBIND_TEX:
            soft_gfx_unbind_tex(cmd);
            break;
        case GFX_IL_BIND_RENDER_TARGET:
            soft_gfx_bind_render_target(cmd);
            break;
        case GFX_IL_UNBIND_RENDER_TARGET:
            break;
        case GFX_IL_BEGIN_REND:
            soft_gfx_begin_rend(cmd);
            break;
        case GFX_IL_END_REND:
            soft_gfx_end_rend(cmd);
            break;
        case GFX_IL_CLEAR:
            soft_gfx_clear(cmd);
            break;
        case GFX_IL_SET_BLEND_ENABLE:
            blend_enable = cmd->arg.set_blend_enable.do_enable;
            break;
        case GFX_IL_SET_REND_PARAM:
            rend_param = cmd->arg.set_rend_param.param;
            break;
        case GFX_IL_SET_CLIP_RANGE:
            break;
        case GFX_IL_SET_VERT_ARRAY:
            soft_gfx_set_vert_array(cmd);
            break;
        case GFX_IL_DRAW_VERT_ARRAY:
            soft_gfx_draw_vert_array(cmd);
            break;
        case GFX_IL_INIT_OBJ:
            soft_gfx_obj_init(cmd);
            break;
        case GFX_IL_WRITE_OBJ:
            soft_gfx_obj_write(cmd);
            break;
        case GFX_IL_READ_OBJ:
            soft_gfx_obj_read(cmd);
            break;
        case GFX_IL_FREE_OBJ:
            soft_gfx_obj_free(cmd);
            break;
        case GFX_IL_POST_FRAMEBUFFER:
            soft_gfx_post_fb(cmd);
            break;
        case GFX_IL_GRAB_FRAMEBUFFER:
            fprintf(stderr, "ERROR: GFX_IL_GRAB_FRAMEBUFFER not implemented for soft_gfx\n");
            abort(); // we can't give the emulator what it needs here
            break;
        case GFX_IL_BEGIN_DEPTH_SORT:
            sort_mode_enable = true;

            // re-initialize
            n_oit_pixels = 0;
            {
                unsigned idx;
                for (idx = 0; idx < FB_WIDTH * FB_HEIGHT; idx++)
                    oit_buf[idx] = -1;
            }
            break;
        case GFX_IL_END_DEPTH_SORT:
            // sort pixels and render back-to-front
            {
                int row, col;
                for (row = 0; row < FB_HEIGHT; row++)
                    for (col = 0; col < FB_WIDTH; col++) {
                        int pix_idx = oit_buf[row * FB_WIDTH + col];
                        if (pix_idx >= 0) {
                            sort_oit_pix_list(pix_idx);
                            struct gfx_obj *obj = gfx_obj_get(render_tgt);
                            do {
                                struct oit_pixel *pix = oit_pixels + pix_idx;
                                if (depth_test(col, row, pix->w_coord)) {
                                    put_pix_blended(obj, col, row,
                                                    pix->rgba[0]          |
                                                    (pix->rgba[1] << 8)   |
                                                    (pix->rgba[2] << 16)  |
                                                    (pix->rgba[3] << 24),
                                                    pix->src_blend_factor,
                                                    pix->dst_blend_factor);
                                    w_buffer[row * screen_width + col] = pix->w_coord;
                                }
                                pix_idx = pix->next_pix_idx;
                            } while (pix_idx >= 0);
                        }
                    }
            }
            sort_mode_enable = false;
            break;
        case GFX_IL_SET_USER_CLIP:
            user_clip[0] = cmd->arg.set_user_clip.x_min;
            user_clip[1] = cmd->arg.set_user_clip.y_min;
            user_clip[2] = cmd->arg.set_user_clip.x_max;
            user_clip[3] = cmd->arg.set_user_clip.y_max;
            break;
        default:
            fprintf(stderr, "ERROR: UNKNOWN GFX IL COMMAND %02X\n",
                    (unsigned)cmd->op);
        }
        cmd++;
    }
}
