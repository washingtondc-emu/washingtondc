/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2019 snickerbockers
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

#include <stdlib.h>
#include <stdio.h>

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#include <GL/gl.h>

#include "dreamcast.h"
#include "gfx/gfx_config.h"
#include "gfx/gfx_tex_cache.h"
#include "gfx/gfx.h"
#include "log.h"
#include "pix_conv.h"
#include "config_file.h"
#include "opengl_output.h"
#include "opengl_target.h"
#include "shader.h"

#include "opengl_renderer.h"

#define POSITION_SLOT          0
#define TRANS_MAT_SLOT         1
#define BASE_COLOR_SLOT        2
#define OFFS_COLOR_SLOT        3
#define TEX_COORD_SLOT         4

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

struct obj_tex_meta {
    unsigned width, height;

    GLenum format;   // internalformat and format parameter for glTexImage2D
    GLenum dat_type; // type parameter for glTexImage2D

    /*
     * if this is set, the OpenGL texture object will be re-initialized
     * regardless of the other parameters.
     */
    bool dirty;
};

// one texture object for each gfx_obj
static GLuint obj_tex_array[GFX_OBJ_COUNT];

static struct obj_tex_meta obj_tex_meta_array[GFX_OBJ_COUNT];

static DEF_ERROR_INT_ATTR(gfx_tex_fmt);

static GLenum tex_fmt_to_data_type(enum gfx_tex_fmt gfx_fmt);

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
 * done based on 1 / z instead of z.  On PVR2 a closer depth-value will
 * actually be larger, and a further depth value will be smaller.  Since we
 * convert 1/z to z (in pvr2_ta.c), we also need to invert the depth comparison.
 *
 * For example, guest software which configures the depth function as
 * PVR2_DEPTH_GREATER will expect fragments with larger ("greater") depth
 * values to be in front, but after the z-component is replaced by its own
 * reciprocal, fragments with larger z-values will now have smaller z-values,
 * and fragments with smaller z-values will now have larger z-values.
 *
 * TODO: one thing I'm not sure about is whether it's coorect to convert
 * LEQUAL to GREATER, and GEQUAL to LESSER.  Mathematically these functions are
 * inversions of one another, but I'm not sure if that's what I want to do if
 * all I'm doing is accounting for the reciprocal.
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

#define OIT_MAX_GROUPS (4*1024)

struct oit_group {
    float const *verts;
    unsigned n_verts;

    float avg_depth;

    struct gfx_rend_param rend_param;
};

static struct oit_state {
    unsigned tri_count;
    unsigned group_count;
    bool enabled;

    struct oit_group groups[OIT_MAX_GROUPS];

    struct gfx_rend_param cur_rend_param;
} oit_state;

// converts pixels from ARGB 4444 to RGBA 4444
static void render_conv_argb_4444(uint16_t *pixels, size_t n_pixels);

// converts pixels from ARGB 1555 to ABGR1555
static void render_conv_argb_1555(uint16_t *pixels, size_t n_pizels);

static void opengl_render_init(void);
static void opengl_render_cleanup(void);
static void opengl_renderer_update_tex(unsigned tex_obj);
static void opengl_renderer_release_tex(unsigned tex_obj);
static void opengl_renderer_set_blend_enable(bool enable);
static void opengl_renderer_set_rend_param(struct gfx_rend_param const *param);
static void opengl_renderer_draw_array(float const *verts, unsigned n_verts);
static void opengl_renderer_clear(float const bgcolor[4]);
static void opengl_renderer_set_screen_dim(unsigned width, unsigned height);
static void opengl_renderer_set_clip_range(float new_clip_min,
                                           float new_clip_max);
static void opengl_renderer_begin_sort_mode(void);
static void opengl_renderer_end_sort_mode(void);

struct rend_if const opengl_rend_if = {
    .init = opengl_render_init,
    .cleanup = opengl_render_cleanup,
    .update_tex = opengl_renderer_update_tex,
    .release_tex = opengl_renderer_release_tex,
    .set_blend_enable = opengl_renderer_set_blend_enable,
    .set_rend_param = opengl_renderer_set_rend_param,
    .draw_array = opengl_renderer_draw_array,
    .clear = opengl_renderer_clear,
    .set_screen_dim = opengl_renderer_set_screen_dim,
    .set_clip_range = opengl_renderer_set_clip_range,
    .begin_sort_mode = opengl_renderer_begin_sort_mode,
    .end_sort_mode = opengl_renderer_end_sort_mode,
    .target_bind_obj = opengl_target_bind_obj,
    .target_unbind_obj = opengl_target_unbind_obj,
    .target_begin = opengl_target_begin,
    .target_end = opengl_target_end,
    .video_get_fb = opengl_video_get_fb,
    .video_present = opengl_video_present,
    .video_new_framebuffer = opengl_video_new_framebuffer,
    .video_toggle_filter = opengl_video_toggle_filter
};

static char const * const pvr2_ta_vert_glsl =
    "#extension GL_ARB_explicit_uniform_location : enable\n"

    "layout (location = 0) in vec3 vert_pos;\n"
    "layout (location = 1) uniform mat4 trans_mat;\n"
    "layout (location = 2) in vec4 base_color;\n"
    "layout (location = 3) in vec4 offs_color;\n"

    "#ifdef TEX_ENABLE\n"
    "layout (location = 4) in vec2 tex_coord_in;\n"
    "#endif\n"

    "out vec4 vert_base_color, vert_offs_color;\n"
    "#ifdef TEX_ENABLE\n"
    "out vec2 st;\n"
    "#endif\n"

    /*
     * This function performs texture coordinate transformations if textures are\n"
     * enabled.\n"
     */
    "void tex_transform() {\n"
    "#ifdef TEX_ENABLE\n"
    "    st = tex_coord_in;\n"
    "#endif\n"
    "}\n"
    "\n"
    /*
     * translate coordinates from the Dreamcast's coordinate system (which is\n"
     * screen-coordinates with an origin in the upper-left) to OpenGL\n"
     * coordinates (which are bounded from -1.0 to 1.0, with the upper-left\n"
     * coordinate being at (-1.0, 1.0)\n"
     */
    "void modelview_project_transform() {\n"
    /*
     * Given that Dreamcast does all its vertex transformations in software on
     * the SH-4, you might think that it's alright to disregard the perspective
     * divide and just pass through 1.0 for the w coordinate...and you'd be
     * wrong for thinking that.
     *
     * OpenGL doesn't just use the w-coordinate for perspective divide, it also
     * uses it for perspect-correct texture-mapping later in the fragment stage.
     * If the w-coordinate for all vertices in a polygon is the same, then what
     * you get is effictively the same as affine texture-mapping.  Affine
     * texture mapping linearly-interpolates the u and v coordinates, and it
     * looks distorted for polygons where the orthonormal vector doesn't align
     * with the camera direction because it doesn't take the third-dimension
     * into account.  This is because fragments closer to the viewer should
     * sample texels that are closer together to each other than fragments
     * farther away will (i think), and the affine/linear transformation forces
     * them all to linearly sample texels that are the same distance from texels
     * sampled by adjacent fragments.
     *
     * ANYWAYS, perspective-correct texture mapping fixes this by taking the
     * depth-component into account, and it gets that from the w coordinate,
     * which is the value you divide by for perspective-divide; ergo I must use
     * the actual depth coordinate for the perspective divide.  Since the
     * perspective-divide will divide all components by w (which is actually z),
     * I have to multiply all of them by z.
     */
    "    vec4 pos = trans_mat * vec4(vert_pos, 1.0);\n"
    "    gl_Position = vec4(pos.x * vert_pos.z, pos.y * vert_pos.z, pos.z * vert_pos.z, vert_pos.z);\n"
    "}\n"

    "void color_transform() {\n"
    "#ifdef COLOR_DISABLE\n"
    "    vert_base_color = vec4(1.0, 1.0, 1.0, 1.0);\n"
    "    vert_offs_color = vec4(0.0, 0.0, 0.0, 0.0);\n"
    "#else\n"
    "    vert_base_color = base_color;\n"
    "    vert_offs_color = offs_color;\n"
    "#endif\n"
    "}\n"

    "void main() {\n"
    "    modelview_project_transform();\n"
    "    color_transform();\n"
    "    tex_transform();\n"
    "}\n";

static char const * const pvr2_ta_frag_glsl =
    "in vec4 vert_base_color, vert_offs_color;\n"
    "out vec4 color;\n"

    "#ifdef TEX_ENABLE\n"
    "in vec2 st;\n"
    "uniform sampler2D bound_tex;\n"
    "uniform int tex_inst;\n"
    "#endif\n"

    "void main() {\n"
    "#ifdef TEX_ENABLE\n"
    "    vec4 tex_color = texture(bound_tex, st);\n"

    // TODO: is the offset alpha color supposed to be used for anything?
    "    switch (tex_inst) {\n"
    "    default:\n"
    "    case 0:\n"
    // decal
    "        color.rgb = tex_color.rgb + vert_offs_color.rgb;\n"
    "        color.a = tex_color.a;\n"
    "        break;\n"
    "    case 1:\n"
    // modulate
    "        color.rgb = tex_color.rgb * vert_base_color.rgb + vert_offs_color.rgb;\n"
    "        color.a = tex_color.a;\n"
    "        break;\n"
    "    case 2:\n"
    // decal with alpha
    "        color.rgb = tex_color.rgb * tex_color.a +\n"
    "            vert_base_color.rgb * (1.0 - tex_color.a) + vert_offs_color.rgb;\n"
    "        color.a = vert_base_color.a;\n"
    "        break;\n"
    "    case 3:\n"
    // modulate with alpha
    "        color.rgb = tex_color.rgb * vert_base_color.rgb + vert_offs_color.rgb;\n"
    "        color.a = tex_color.a * vert_base_color.a;\n"
    "        break;\n"
    "    }\n"
    "#else\n"
    "    color = vert_base_color;\n"
    "#endif\n"
    "}\n";


static void opengl_render_init(void) {
    opengl_video_output_init();
    opengl_target_init();

    char const *oit_mode_str = cfg_get_node("gfx.rend.oit-mode");
    if (oit_mode_str) {
        if (strcmp(oit_mode_str, "per-group") == 0)
            gfx_config_oit_enable();
        else if (strcmp(oit_mode_str, "disabled") == 0)
            gfx_config_oit_disable();
        else
            gfx_config_oit_disable();
    } else {
        gfx_config_oit_enable();
    }

    shader_load_vert(&pvr_ta_shader, pvr2_ta_vert_glsl);
    shader_load_frag(&pvr_ta_shader, pvr2_ta_frag_glsl);
    shader_link(&pvr_ta_shader);

    shader_load_vert_with_preamble(&pvr_ta_tex_shader, pvr2_ta_vert_glsl,
                                   "#define TEX_ENABLE\n");
    shader_load_frag_with_preamble(&pvr_ta_tex_shader, pvr2_ta_frag_glsl,
                                   "#define TEX_ENABLE\n");
    shader_link(&pvr_ta_tex_shader);

    shader_load_vert_with_preamble(&pvr_ta_no_color_shader, pvr2_ta_vert_glsl,
                                   "#define COLOR_DISABLE\n");
    shader_load_frag_with_preamble(&pvr_ta_no_color_shader, pvr2_ta_frag_glsl,
                                   "#define COLOR_DISABLE\n");
    shader_link(&pvr_ta_no_color_shader);

    bound_tex_slot = glGetUniformLocation(pvr_ta_tex_shader.shader_prog_obj,
                                          "bound_tex");
    tex_inst_slot = glGetUniformLocation(pvr_ta_tex_shader.shader_prog_obj,
                                         "tex_inst");

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenTextures(GFX_OBJ_COUNT, obj_tex_array);

    memset(obj_tex_meta_array, 0, sizeof(obj_tex_meta_array));

    unsigned tex_no;
    for (tex_no = 0; tex_no < GFX_OBJ_COUNT; tex_no++) {
        obj_tex_meta_array[tex_no].dirty = true;

        /*
         * unconditionally set the texture wrapping mode to repeat.
         *
         * TODO: I know for sure that a lot of games need repeating texture,
         * coordinates but I don't know if there are any that need clamped
         * texture coordinates.  In the future I will need to determine if this
         * functionality exists in PVR2.
         */
        glBindTexture(GL_TEXTURE_2D, obj_tex_array[tex_no]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

static void opengl_render_cleanup(void) {
    glDeleteTextures(GFX_OBJ_COUNT, obj_tex_array);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    shader_cleanup(&pvr_ta_no_color_shader);
    shader_cleanup(&pvr_ta_tex_shader);
    shader_cleanup(&pvr_ta_shader);

    vao = 0;
    vbo = 0;
    memset(obj_tex_array, 0, sizeof(obj_tex_array));
}

static DEF_ERROR_INT_ATTR(max_length);

static void opengl_renderer_update_tex(unsigned tex_obj) {
    struct gfx_tex const *tex = gfx_tex_cache_get(tex_obj);
    struct gfx_obj *obj = gfx_obj_get(tex->obj_handle);

    // nothing to do here
    if (obj->state & GFX_OBJ_STATE_TEX)
        return;

    gfx_obj_alloc(obj);

    void const *tex_dat = obj->dat;
    GLenum format = tex->tex_fmt == GFX_TEX_FMT_RGB_565 ?
        GL_RGB : GL_RGBA;

    unsigned tex_w = tex->width;
    unsigned tex_h = tex->height;

    glBindTexture(GL_TEXTURE_2D, obj_tex_array[tex->obj_handle]);
    // TODO: maybe don't always set this to 1
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    /*
     * TODO: ideally I wouldn't need to copy ARGB_4444 and ARGB_1555 into a
     * separate buffer to do the pixel conversion.  The reason I do this is that
     * the tex-dump command in the cmd thread also sees the texture data in the
     * struct gfx_tex, so I don't want to modify that.  Maybe someday I'll
     * change things to remove this mostly-unnecessary buffering...
     */
    if (tex->tex_fmt == GFX_TEX_FMT_ARGB_4444) {
        size_t n_bytes = tex->width * tex->height * sizeof(uint16_t);
#ifdef INVARIANTS
        if (n_bytes > obj->dat_len) {
            error_set_length(n_bytes);
            error_set_max_length(obj->dat_len);
            RAISE_ERROR(ERROR_OVERFLOW);
        }
#endif
        uint16_t *tex_dat_conv = (uint16_t*)malloc(n_bytes);
        if (!tex_dat_conv)
            RAISE_ERROR(ERROR_FAILED_ALLOC);
        memcpy(tex_dat_conv, tex_dat, n_bytes);
        render_conv_argb_4444(tex_dat_conv, tex_w * tex_h);
        glTexImage2D(GL_TEXTURE_2D, 0, format, tex_w, tex_h, 0,
                     format, tex_fmt_to_data_type(GFX_TEX_FMT_ARGB_4444),
                     tex_dat_conv);
        opengl_renderer_tex_set_dims(tex->obj_handle, tex_w, tex_h);
        opengl_renderer_tex_set_format(tex->obj_handle, format);
        opengl_renderer_tex_set_dat_type(tex->obj_handle,
                                         tex_fmt_to_data_type(GFX_TEX_FMT_ARGB_4444));
        opengl_renderer_tex_set_dirty(tex->obj_handle, false);
        free(tex_dat_conv);
    } else if (tex->tex_fmt == GFX_TEX_FMT_ARGB_1555) {
        size_t n_bytes = tex->width * tex->height * sizeof(uint16_t);
#ifdef INVARIANTS
        if (n_bytes > obj->dat_len) {
            error_set_length(n_bytes);
            error_set_max_length(obj->dat_len);
            RAISE_ERROR(ERROR_OVERFLOW);
        }
#endif
        uint16_t *tex_dat_conv = (uint16_t*)malloc(n_bytes);
        if (!tex_dat_conv)
            RAISE_ERROR(ERROR_FAILED_ALLOC);
        memcpy(tex_dat_conv, tex_dat, n_bytes);
        render_conv_argb_1555(tex_dat_conv, tex_w * tex_h);
        glTexImage2D(GL_TEXTURE_2D, 0, format, tex_w, tex_h, 0,
                     format, tex_fmt_to_data_type(GFX_TEX_FMT_ARGB_1555),
                     tex_dat_conv);
        opengl_renderer_tex_set_dims(tex->obj_handle, tex_w, tex_h);
        opengl_renderer_tex_set_format(tex->obj_handle, format);
        opengl_renderer_tex_set_dat_type(tex->obj_handle,
                                         tex_fmt_to_data_type(GFX_TEX_FMT_ARGB_1555));
        opengl_renderer_tex_set_dirty(tex->obj_handle, false);
        free(tex_dat_conv);
    } else if (tex->tex_fmt == GFX_TEX_FMT_YUV_422) {
        uint8_t *tmp_dat =
            (uint8_t*)malloc(sizeof(uint8_t) * 3 * tex_w * tex_h);
        if (!tmp_dat)
            RAISE_ERROR(ERROR_FAILED_ALLOC);
        conv_yuv422_rgb888(tmp_dat, tex_dat, tex_w, tex_h);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tex_w, tex_h, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, tmp_dat);
        opengl_renderer_tex_set_dims(tex->obj_handle, tex_w, tex_h);
        opengl_renderer_tex_set_format(tex->obj_handle, GL_RGB);
        opengl_renderer_tex_set_dat_type(tex->obj_handle, GL_UNSIGNED_BYTE);
        opengl_renderer_tex_set_dirty(tex->obj_handle, false);
        free(tmp_dat);
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, format, tex_w, tex_h, 0,
                     format, tex_fmt_to_data_type(tex->tex_fmt), tex_dat);
        opengl_renderer_tex_set_dims(tex->obj_handle, tex_w, tex_h);
        opengl_renderer_tex_set_format(tex->obj_handle, format);
        opengl_renderer_tex_set_dat_type(tex->obj_handle,
                                         tex_fmt_to_data_type(tex->tex_fmt));
        opengl_renderer_tex_set_dirty(tex->obj_handle, false);
    }
    obj->state |= GFX_OBJ_STATE_TEX;
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void opengl_renderer_release_tex(unsigned tex_obj) {
    // do nothing
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

static void render_conv_argb_1555(uint16_t *pixels, size_t n_pixels) {
    for (size_t pix_no = 0; pix_no < n_pixels; pix_no++, pixels++) {
        uint16_t pix_current = *pixels;
        uint16_t b = (pix_current & 0x001f) >> 0;
        uint16_t g = (pix_current & 0x03e0) >> 5;
        uint16_t r = (pix_current & 0x7c00) >> 10;
        uint16_t a = (pix_current & 0x8000) >> 15;

        *pixels = (a << 15) | (b << 10) | (g << 5) | (r << 0);
    }
}

static void opengl_renderer_set_blend_enable(bool enable) {
    struct gfx_cfg rend_cfg = gfx_config_read();

    if (rend_cfg.blend_enable && enable)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
}

static float clip_min, clip_max;
static bool tex_enable;
static unsigned screen_width, screen_height;

static void opengl_renderer_set_rend_param(struct gfx_rend_param const *param) {
    if (oit_state.enabled) {
        /*
         * This gets flipped around to GL_LEQUAL when we set the actual OpenGL
         * depth function
         */
        oit_state.cur_rend_param.depth_func = PVR2_DEPTH_GREATER;
        oit_state.cur_rend_param = *param;
        return;
    }

    struct gfx_cfg rend_cfg = gfx_config_read();

    /*
     * TODO: currently disable color also disables textures; ideally these
     * would be two independent settings.
     */
    if (param->tex_enable && rend_cfg.tex_enable && rend_cfg.color_enable) {
        glUseProgram(pvr_ta_tex_shader.shader_prog_obj);

        if (gfx_tex_cache_get(param->tex_idx)->valid) {
            int obj_handle = gfx_tex_cache_get(param->tex_idx)->obj_handle;
            glBindTexture(GL_TEXTURE_2D, obj_tex_array[obj_handle]);
        } else {
            LOG_WARN("WARNING: attempt to bind invalid texture %u\n",
                     (unsigned)param->tex_idx);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        switch (param->tex_filter) {
        case TEX_FILTER_TRILINEAR_A:
        case TEX_FILTER_TRILINEAR_B:
            LOG_WARN("WARNING: trilinear filtering is not yet supported\n");
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
        GLenum tex_wrap_mode_gl[2];
        switch (param->tex_wrap_mode[0]) {
        case TEX_WRAP_REPEAT:
            tex_wrap_mode_gl[0] = GL_REPEAT;
            break;
        case TEX_WRAP_FLIP:
            tex_wrap_mode_gl[0] = GL_MIRRORED_REPEAT;
            break;
        case TEX_WRAP_CLAMP:
            tex_wrap_mode_gl[0] = GL_CLAMP_TO_EDGE;
            break;
        default:
            RAISE_ERROR(ERROR_INTEGRITY);
        }
        switch (param->tex_wrap_mode[1]) {
        case TEX_WRAP_REPEAT:
            tex_wrap_mode_gl[1] = GL_REPEAT;
            break;
        case TEX_WRAP_FLIP:
            tex_wrap_mode_gl[1] = GL_MIRRORED_REPEAT;
            break;
        case TEX_WRAP_CLAMP:
            tex_wrap_mode_gl[1] = GL_CLAMP_TO_EDGE;
            break;
        default:
            RAISE_ERROR(ERROR_INTEGRITY);
        }

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, tex_wrap_mode_gl[0]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, tex_wrap_mode_gl[1]);

        glUniform1i(bound_tex_slot, 0);
        glUniform1i(tex_inst_slot, param->tex_inst);
        glActiveTexture(GL_TEXTURE0);
    } else if (rend_cfg.color_enable) {
        glUseProgram(pvr_ta_shader.shader_prog_obj);
    } else {
        glUseProgram(pvr_ta_no_color_shader.shader_prog_obj);
    }

    glBlendFunc(src_blend_factors[(unsigned)param->src_blend_factor],
                dst_blend_factors[(unsigned)param->dst_blend_factor]);

    glDepthMask(param->enable_depth_writes ? GL_TRUE : GL_FALSE);
    glDepthFunc(depth_funcs[param->depth_func]);

    tex_enable = param->tex_enable;
}

static void opengl_renderer_draw_array(float const *verts, unsigned n_verts) {
    if (!n_verts)
        return;

    if (oit_state.enabled) {
        oit_state.tri_count += n_verts / 3;

        if (oit_state.group_count < OIT_MAX_GROUPS) {
            struct oit_group *grp = oit_state.groups + oit_state.group_count++;
            grp->rend_param = oit_state.cur_rend_param;
            grp->verts = verts;
            grp->n_verts = n_verts;

            float avg_depth = 0.0f;
            unsigned vert_no;
            for (vert_no = 0; vert_no < n_verts; vert_no++)
                avg_depth += verts[vert_no * GFX_VERT_LEN + 2];
            avg_depth /= n_verts;

            grp->avg_depth = avg_depth;
        } else {
            LOG_ERROR("OPENGL GFX: OIT BUFFER OVERFLOW!!!\n");
        }
        return;
    }

    float clip_min_actual = clip_min * 1.01f;
    float clip_max_actual = clip_max * 1.01f;

    GLfloat half_screen_dims[2] = {
        (GLfloat)(screen_width * 0.5),
        (GLfloat)(screen_height * 0.5)
    };

    GLfloat clip_delta = clip_max_actual - clip_min_actual;
    GLfloat trans_mat[16] = {
        1.0 / half_screen_dims[0], 0, 0, -1,
        0, -1.0 / half_screen_dims[1], 0, 1,
        0, 0, 2.0 / clip_delta, -2.0 * clip_min_actual / clip_delta - 1,
        0, 0, 0, 1
    };

    glUniformMatrix4fv(TRANS_MAT_SLOT, 1, GL_TRUE, trans_mat);

    // now draw the geometry itself
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 sizeof(float) * n_verts * GFX_VERT_LEN,
                 verts, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(POSITION_SLOT);
    glEnableVertexAttribArray(BASE_COLOR_SLOT);
    glEnableVertexAttribArray(OFFS_COLOR_SLOT);
    glVertexAttribPointer(POSITION_SLOT, 3, GL_FLOAT, GL_FALSE,
                          GFX_VERT_LEN * sizeof(float),
                          (GLvoid*)(GFX_VERT_POS_OFFSET * sizeof(float)));
    glVertexAttribPointer(BASE_COLOR_SLOT, 4, GL_FLOAT, GL_FALSE,
                          GFX_VERT_LEN * sizeof(float),
                          (GLvoid*)(GFX_VERT_BASE_COLOR_OFFSET * sizeof(float)));
    glVertexAttribPointer(OFFS_COLOR_SLOT, 4, GL_FLOAT, GL_FALSE,
                          GFX_VERT_LEN * sizeof(float),
                          (GLvoid*)(GFX_VERT_OFFS_COLOR_OFFSET * sizeof(float)));
    if (tex_enable) {
        glEnableVertexAttribArray(TEX_COORD_SLOT);
        glVertexAttribPointer(TEX_COORD_SLOT, 2, GL_FLOAT, GL_FALSE,
                              GFX_VERT_LEN * sizeof(float),
                              (GLvoid*)(GFX_VERT_TEX_COORD_OFFSET * sizeof(float)));
    }
    glDrawArrays(GL_TRIANGLES, 0, n_verts);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
}

static void opengl_renderer_clear(float const bgcolor[4]) {
    struct gfx_cfg rend_cfg = gfx_config_read();

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
        glClearColor(bgcolor[0], bgcolor[1],
                     bgcolor[2], bgcolor[3]);
    } else {
        glClearColor(0.0, 0.0, 0.0, 1.0);
    }
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (rend_cfg.depth_enable)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    /*
     * Strictly speaking, this isn't needed since we transform the
     * depth-component such that geo->clip_max maps to +1 and geo->clip_min
     * maps to -1, but we enable it just in case there are any floating-point
     * precision errors that push something to be greater than +1 or less
     * than -1.
     */
    glEnable(GL_DEPTH_CLAMP);
}

static void opengl_renderer_set_screen_dim(unsigned width, unsigned height) {
    screen_width = width;
    screen_height = height;
}

static void opengl_renderer_set_clip_range(float new_clip_min,
                                           float new_clip_max) {
    clip_min = new_clip_min;
    clip_max = new_clip_max;
}

GLuint opengl_renderer_tex(unsigned obj_no) {
    return obj_tex_array[obj_no];
}

unsigned opengl_renderer_tex_get_width(unsigned obj_no) {
    return obj_tex_meta_array[obj_no].width;
}

unsigned opengl_renderer_tex_get_height(unsigned obj_no) {
    return obj_tex_meta_array[obj_no].height;
}

void opengl_renderer_tex_set_dims(unsigned obj_no,
                                  unsigned width, unsigned height) {
    obj_tex_meta_array[obj_no].width = width;
    obj_tex_meta_array[obj_no].height = height;
}

void opengl_renderer_tex_set_format(unsigned obj_no, GLenum fmt) {
    obj_tex_meta_array[obj_no].format = fmt;
}

void opengl_renderer_tex_set_dat_type(unsigned obj_no, GLenum dat_tp) {
    obj_tex_meta_array[obj_no].dat_type = dat_tp;
}

void opengl_renderer_tex_set_dirty(unsigned obj_no, bool dirty) {
    obj_tex_meta_array[obj_no].dirty = dirty;
}

GLenum opengl_renderer_tex_get_format(unsigned obj_no) {
    return obj_tex_meta_array[obj_no].format;
}

GLenum opengl_renderer_tex_get_dat_type(unsigned obj_no) {
    return obj_tex_meta_array[obj_no].dat_type;
}

bool opengl_renderer_tex_get_dirty(unsigned obj_no) {
    return obj_tex_meta_array[obj_no].dirty;
}

static void opengl_renderer_begin_sort_mode(void) {
    if (oit_state.enabled)
        RAISE_ERROR(ERROR_INTEGRITY);

    if (gfx_config_read().depth_sort_enable) {
        LOG_INFO("SORT MODE ENABLE\n");
        oit_state.enabled = true;
        oit_state.tri_count = 0;
        oit_state.group_count = 0;
    }
}

static void opengl_renderer_end_sort_mode(void) {
    if (!gfx_config_read().depth_sort_enable)
        return;
    if (!oit_state.enabled)
        RAISE_ERROR(ERROR_INTEGRITY);

    LOG_INFO("SORT MODE DISABLE (%u triangles)\n", oit_state.tri_count);
    oit_state.enabled = false;

    // do an insertion sort because i'm a pleb
    unsigned src_idx, dst_idx;
    unsigned grp_cnt = oit_state.group_count;
    if (grp_cnt) {
        struct oit_group tmp;
        for (src_idx = 0; src_idx < grp_cnt - 1; src_idx++) {
            struct oit_group *grp_src = oit_state.groups + src_idx;
            for (dst_idx = src_idx + 1; dst_idx < grp_cnt; dst_idx++) {
                struct oit_group *grp_dst = oit_state.groups + dst_idx;
                if (grp_dst->avg_depth >= grp_src->avg_depth) {
                    tmp = *grp_src;
                    *grp_src = *grp_dst;
                    *grp_dst = tmp;
                }
            }
        }

        for (src_idx = 0; src_idx < grp_cnt; src_idx++) {
            struct oit_group *grp_src = oit_state.groups + src_idx;
            opengl_renderer_set_rend_param(&grp_src->rend_param);
            opengl_renderer_draw_array(grp_src->verts, grp_src->n_verts);
        }
    }
}

static GLenum tex_fmt_to_data_type(enum gfx_tex_fmt gfx_fmt) {
    switch (gfx_fmt) {
    case GFX_TEX_FMT_ARGB_1555:
        return GL_UNSIGNED_SHORT_1_5_5_5_REV;
    case GFX_TEX_FMT_RGB_565:
        return GL_UNSIGNED_SHORT_5_6_5;
    case GFX_TEX_FMT_ARGB_4444:
        return GL_UNSIGNED_SHORT_4_4_4_4;
    case GFX_TEX_FMT_ARGB_8888:
        return GL_UNSIGNED_BYTE;
    default:
        error_set_gfx_tex_fmt(gfx_fmt);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}
