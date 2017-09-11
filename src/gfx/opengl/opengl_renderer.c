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

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#include <GL/gl.h>

#include "hw/pvr2/geo_buf.h"
#include "hw/pvr2/pvr2_tex_cache.h"
#include "shader.h"
#include "opengl_target.h"
#include "dreamcast.h"
#include "gfx/gfx_config.h"
#include "gfx/gfx_tex_cache.h"

#include "opengl_renderer.h"

#define POSITION_SLOT          0
#define TRANS_MAT_SLOT         1
#define COLOR_SLOT             2
#define TEX_COORD_SLOT         3

static GLuint bound_tex_slot;
static GLuint tex_inst_slot;

static struct shader pvr_ta_shader;
static struct shader pvr_ta_tex_shader;

/*
 * special shader for wireframe mode that ignores vertex colors and textures
 * TODO: this shader also ignores textures.  This is not desirable since
 * textures are a separate config, but ultimately it's not that big of a deal
 * since wireframe mode always disables textures anyways
 */
static struct shader pvr_ta_no_color_shader;

static GLuint vbo, vao;

static GLuint tex_cache[PVR2_TEX_CACHE_SIZE];

static struct gfx_cfg rend_cfg;

static const GLenum tex_formats[TEX_CTRL_PIX_FMT_COUNT] = {
    [TEX_CTRL_PIX_FMT_ARGB_1555] = GL_UNSIGNED_SHORT_1_5_5_5_REV,
    [TEX_CTRL_PIX_FMT_RGB_565] = GL_UNSIGNED_SHORT_5_6_5,
    [TEX_CTRL_PIX_FMT_ARGB_4444] = GL_UNSIGNED_SHORT_4_4_4_4,
};

static const GLenum src_blend_factors[PVR2_BLEND_FACTOR_COUNT] = {
    [PVR2_BLEND_ZERO]                = GL_ZERO,
    [PVR2_BLEND_ONE]                 = GL_ONE,
    [PVR2_BLEND_OTHER]               = GL_DST_COLOR,
    [PVR2_BLEND_ONE_MINUS_OTHER]     = GL_ONE_MINUS_DST_COLOR,
    [PVR2_BLEND_SRC_ALPHA]           = GL_SRC_ALPHA,
    [PVR2_BLEND_ONE_MINUS_SRC_ALPHA] = GL_ONE_MINUS_SRC_ALPHA,
    [PVR2_BLEND_DST_ALPHA]           = GL_DST_ALPHA,
    [PVR2_BLEND_ONE_MINUS_DST_ALPHA] = GL_ONE_MINUS_DST_ALPHA
};

static const GLenum dst_blend_factors[PVR2_BLEND_FACTOR_COUNT] = {
    [PVR2_BLEND_ZERO]                = GL_ZERO,
    [PVR2_BLEND_ONE]                 = GL_ONE,
    [PVR2_BLEND_OTHER]               = GL_SRC_COLOR,
    [PVR2_BLEND_ONE_MINUS_OTHER]     = GL_ONE_MINUS_SRC_COLOR,
    [PVR2_BLEND_SRC_ALPHA]           = GL_SRC_ALPHA,
    [PVR2_BLEND_ONE_MINUS_SRC_ALPHA] = GL_ONE_MINUS_SRC_ALPHA,
    [PVR2_BLEND_DST_ALPHA]           = GL_DST_ALPHA,
    [PVR2_BLEND_ONE_MINUS_DST_ALPHA] = GL_ONE_MINUS_DST_ALPHA
};

/*
 * the PVR2 and OpenGL depth functions are inverted because PVR2's versions are
 * done based on 1 / z instead of z.
 */
static const GLenum depth_funcs[PVR2_DEPTH_FUNC_COUNT] = {
    [PVR2_DEPTH_NEVER]               = GL_NEVER,
    [PVR2_DEPTH_LESS]                = GL_GEQUAL,
    [PVR2_DEPTH_EQUAL]               = GL_EQUAL,
    [PVR2_DEPTH_LEQUAL]              = GL_GREATER,
    [PVR2_DEPTH_GREATER]             = GL_LEQUAL,
    [PVR2_DEPTH_NOTEQUAL]            = GL_NOTEQUAL,
    [PVR2_DEPTH_GEQUAL]              = GL_LESS,
    [PVR2_DEPTH_ALWAYS]              = GL_ALWAYS
};

/*
 * draws the given geo_buf in whatever context is available (ie without setting
 * the shader, or the framebuffer).
 */
static void render_do_draw(struct geo_buf *geo);

// converts pixels from ARGB 4444 to RGBA 4444
static void render_conv_argb_4444(uint16_t *pixels, size_t n_pixels);

static void opengl_render_init(void);
static void opengl_render_cleanup(void);
static void opengl_renderer_update_tex(unsigned tex_obj);
static void opengl_renderer_release_tex(unsigned tex_obj);
static void opengl_renderer_do_draw_geo_buf(struct geo_buf *geo);

struct rend_if const opengl_rend_if = {
    .init = opengl_render_init,
    .cleanup = opengl_render_cleanup,
    .update_tex = opengl_renderer_update_tex,
    .release_tex = opengl_renderer_release_tex,
    .do_draw_geo_buf = opengl_renderer_do_draw_geo_buf
};

static void opengl_render_init(void) {
    shader_load_vert_from_file(&pvr_ta_shader, "pvr2_ta_vert.glsl");
    shader_load_frag_from_file(&pvr_ta_shader, "pvr2_ta_frag.glsl");
    shader_link(&pvr_ta_shader);

    shader_load_vert_from_file_with_preamble(&pvr_ta_tex_shader,
                                             "pvr2_ta_vert.glsl",
                                             "#define TEX_ENABLE\n");
    shader_load_frag_from_file_with_preamble(&pvr_ta_tex_shader,
                                             "pvr2_ta_frag.glsl",
                                             "#define TEX_ENABLE\n");
    shader_link(&pvr_ta_tex_shader);

    shader_load_vert_from_file_with_preamble(&pvr_ta_no_color_shader,
                                             "pvr2_ta_vert.glsl",
                                             "#define COLOR_DISABLE\n");
    shader_load_frag_from_file_with_preamble(&pvr_ta_no_color_shader,
                                             "pvr2_ta_frag.glsl",
                                             "#define COLOR_DISABLE\n");
    shader_link(&pvr_ta_no_color_shader);

    bound_tex_slot = glGetUniformLocation(pvr_ta_tex_shader.shader_prog_obj,
                                          "bound_tex");
    tex_inst_slot = glGetUniformLocation(pvr_ta_tex_shader.shader_prog_obj,
                                         "tex_inst");

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenTextures(PVR2_TEX_CACHE_SIZE, tex_cache);

    unsigned tex_no;
    for (tex_no = 0; tex_no < PVR2_TEX_CACHE_SIZE; tex_no++) {
        glBindTexture(GL_TEXTURE_2D, tex_no);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

static void opengl_render_cleanup(void) {
    glDeleteTextures(PVR2_TEX_CACHE_SIZE, tex_cache);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    shader_cleanup(&pvr_ta_no_color_shader);
    shader_cleanup(&pvr_ta_tex_shader);
    shader_cleanup(&pvr_ta_shader);

    vao = 0;
    vbo = 0;
    memset(tex_cache, 0, sizeof(tex_cache));
}

static void render_do_draw_group(struct geo_buf *geo,
                                 enum display_list_type disp_list,
                                 unsigned group_no) {
    struct poly_group *group = geo->lists[disp_list].groups + group_no;

    /*
     * TODO: currently disable color also disables textures; ideally these
     * would be two independent settings.
     */
    if (group->tex_enable && rend_cfg.tex_enable && rend_cfg.color_enable) {
        glUseProgram(pvr_ta_tex_shader.shader_prog_obj);
        glBindTexture(GL_TEXTURE_2D, tex_cache[group->tex_idx]);
        switch (group->tex_filter) {
        case TEX_FILTER_TRILINEAR_A:
        case TEX_FILTER_TRILINEAR_B:
            printf("WARNING: trilinear filtering is not yet supported\n");
            // intentional fall-through
        case TEX_FILTER_NEAREST:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            break;
        case TEX_FILTER_BILINEAR:
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            break;
        }
        glUniform1i(bound_tex_slot, 0);
        glUniform1i(tex_inst_slot, group->tex_inst);
        glActiveTexture(GL_TEXTURE0);
    } else if (rend_cfg.color_enable) {
        glUseProgram(pvr_ta_shader.shader_prog_obj);
    } else {
        glUseProgram(pvr_ta_no_color_shader.shader_prog_obj);
    }

#ifdef INVARIANTS
    /*
     * this check is a little silly, but I get segfaults sometimes when
     * indexing into src_blend_factors and dst_blend_factors and I don't know
     * why.
     *
     * TODO: this was (hopefully) fixed in commit
     * 92059fe4f1714b914cec75fd2f91e676127d3097 but I am keeping the INVARIANTS
     * test here just in case.  It should be safe to delete after a couple of
     * months have gone by without this INVARIANTS test ever failing.
     */
    if ((group->src_blend_factor < 0) ||
        (group->dst_blend_factor < 0) ||
        (group->src_blend_factor >= PVR2_BLEND_FACTOR_COUNT) ||
        (group->dst_blend_factor >= PVR2_BLEND_FACTOR_COUNT)) {
        error_set_src_blend_factor(group->src_blend_factor);
        error_set_dst_blend_factor(group->dst_blend_factor);
        error_set_display_list_index((unsigned)disp_list);
        error_set_geo_buf_group_index(group_no);
        RAISE_ERROR(ERROR_INTEGRITY);
    }
#endif

    glBlendFunc(src_blend_factors[(unsigned)group->src_blend_factor],
                dst_blend_factors[(unsigned)group->dst_blend_factor]);


    glDepthMask(group->enable_depth_writes ? GL_TRUE : GL_FALSE);
    glDepthFunc(depth_funcs[group->depth_func]);

    /*
     * Orthographic projection.  Map all coordinates into the (-1, -1, -1) to
     * (1, 1, 1).  Anything less than -half_screen_dims or greater than
     * half_screen_dims on the x/y axes or anything not between
     * clip_min_max[0]/clip_min_max[1] on the z-axis will be clipped.  Ideally
     * nothing should be clipped on the z-axis because clip_min_max is derived
     * from the minimum and maximum depths.
     */
    GLfloat half_screen_dims[2] = {
        (GLfloat)(geo->screen_width * 0.5),
        (GLfloat)(geo->screen_height * 0.5)
    };
    GLfloat trans_mat[16] = {
        1.0f / half_screen_dims[0], 0, 0, 0,
        0, -1.0 / half_screen_dims[1], 0, 0,
        0, 0, -1.0 / (geo->clip_max - geo->clip_min), 0,
        -1, 1, geo->clip_min / (geo->clip_max - geo->clip_min), 1
    };
    glUniformMatrix4fv(TRANS_MAT_SLOT, 1, GL_FALSE, trans_mat);

    // now draw the geometry itself
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(float) * group->n_verts * GEO_BUF_VERT_LEN,
                 group->verts, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(POSITION_SLOT);
    glEnableVertexAttribArray(COLOR_SLOT);
    glVertexAttribPointer(POSITION_SLOT, 3, GL_FLOAT, GL_FALSE,
                          GEO_BUF_VERT_LEN * sizeof(float),
                          (GLvoid*)(GEO_BUF_POS_OFFSET * sizeof(float)));
    glVertexAttribPointer(COLOR_SLOT, 4, GL_FLOAT, GL_FALSE,
                          GEO_BUF_VERT_LEN * sizeof(float),
                          (GLvoid*)(GEO_BUF_COLOR_OFFSET * sizeof(float)));
    if (group->tex_enable) {
        glEnableVertexAttribArray(TEX_COORD_SLOT);
        glVertexAttribPointer(TEX_COORD_SLOT, 2, GL_FLOAT, GL_FALSE,
                              GEO_BUF_VERT_LEN * sizeof(float),
                              (GLvoid*)(GEO_BUF_TEX_COORD_OFFSET * sizeof(float)));
    }
    glDrawArrays(GL_TRIANGLES, 0, group->n_verts);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
}

static void render_do_draw(struct geo_buf *geo) {
}

static void opengl_renderer_update_tex(unsigned tex_obj) {
    struct gfx_tex const *tex = gfx_tex_cache_get(tex_obj);
    int n_colors = tex->pvr2_pix_fmt == TEX_CTRL_PIX_FMT_RGB_565 ? 3 : 4;
    GLenum format = tex->pvr2_pix_fmt == TEX_CTRL_PIX_FMT_RGB_565 ?
        GL_RGB : GL_RGBA;

    unsigned tex_w = 1 << tex->w_shift;
    unsigned tex_h = 1 << tex->h_shift;

    if (tex->pvr2_pix_fmt == TEX_CTRL_PIX_FMT_ARGB_4444)
        render_conv_argb_4444((uint16_t*)tex->dat, tex_w * tex_h);

    glBindTexture(GL_TEXTURE_2D, tex_cache[tex_obj]);

    // TODO: maybe don't always set this to 1
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, format, tex_w, tex_h, 0,
                 format, tex_formats[tex->pvr2_pix_fmt], tex->dat);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void opengl_renderer_release_tex(unsigned tex_obj) {
    // do nothing
}

static void opengl_renderer_do_draw_geo_buf(struct geo_buf *geo) {
    gfx_config_read(&rend_cfg);

    if (!rend_cfg.wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    } else {
        glLineWidth(1);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    if (rend_cfg.tex_enable)
        glEnable(GL_TEXTURE_2D);
    else
        glDisable(GL_TEXTURE_2D);

    /*
     * first draw the background plane
     * TODO: I should actually draw a background plane instead
     * of just calling glClear
     */
    if (rend_cfg.bgcolor_enable) {
        glClearColor(geo->bgcolor[0], geo->bgcolor[1],
                     geo->bgcolor[2], geo->bgcolor[3]);
    } else {
        glClearColor(0.0, 0.0, 0.0, 1.0);
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (rend_cfg.depth_enable)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    unsigned group_no;
    enum display_list_type disp_list;
    for (disp_list = DISPLAY_LIST_FIRST; disp_list < DISPLAY_LIST_COUNT;
         disp_list++) {
        struct display_list *list = geo->lists + disp_list;

        if (rend_cfg.blend_enable) {
            if (list->blend_enable)
                glEnable(GL_BLEND);
            else
                glDisable(GL_BLEND);
        } else {
            glDisable(GL_BLEND);
        }

        for (group_no = 0; group_no < list->n_groups; group_no++)
            render_do_draw_group(geo, disp_list, group_no);
    }
}

static void render_conv_argb_4444(uint16_t *pixels, size_t n_pixels) {
    for (size_t pix_no = 0; pix_no < n_pixels; pix_no++, pixels++) {
        uint16_t pix_current = *pixels;
        uint16_t b = (pix_current & 0x000f) >> 0;
        uint16_t g = (pix_current & 0x00f0) >> 4;
        uint16_t r = (pix_current & 0x0f00) >> 8;
        uint16_t a = (pix_current & 0xf000) >> 12;

        *pixels = a | (b << 4) | (g << 8) | (r << 12);
    }
}
