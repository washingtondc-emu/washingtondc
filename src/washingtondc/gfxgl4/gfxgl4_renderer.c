/*******************************************************************************
 *
 * Copyright 2017-2020 snickerbockers
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#include <dlfcn.h> // for loading renderdoc API
#endif

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#include <GL/gl.h>

#include "washdc/error.h"
#include "washdc/gfx/config.h"
#include "washdc/gfx/tex_cache.h"
#include "washdc/gfx/def.h"
#include "washdc/gfx/obj.h"
#include "washdc/pix_conv.h"
#include "washdc/win.h"

#include "gfxgl4_output.h"
#include "gfxgl4_target.h"
#include "../shader.h"
#include "../shader_cache.h"
#include "gfxgl4_renderer.h"
#include "tex_cache.h"
#include "../gfx_obj.h"
#include "../config_file.h"

#include "../renderdoc_app.h"

#define POSITION_SLOT          0
#define BASE_COLOR_SLOT        1
#define OFFS_COLOR_SLOT        2
#define TEX_COORD_SLOT         3

static struct shader_cache shader_cache;
static GLint trans_mat_slot = -1;

static GLuint vbo, vao;

static struct renderer_callbacks const *switch_table;

static float clip_min, clip_max;
static bool tex_enable;
static unsigned screen_width, screen_height;
static enum gfx_user_clip_mode user_clip_mode;
static GLfloat user_clip[4];
static GLint user_clip_slot = -1;

enum {
      /*
       * OpenGL buffer that holds the atomic_uint used to
       * count how many nodes there are for per-pixel OIT
       */
      OIT_BUFFER_NODE_COUNT,

      /*
       * OpenGL buffer that stores the SSBO that backs the oit_nodes array in
       * the fragment shaders
       */
      OIT_BUFFER_NODES_SSBO,

      N_OIT_BUFFERS
};

static GLuint oit_buffers[N_OIT_BUFFERS];

static GLuint oit_heads_tex;

static GLuint oit_color_tex;

static struct shader oit_sort_shader;
static GLint oit_sort_shader_color_accum_slot;

/*
 * just a quad covering the entire screen-space that we need for the OIT sort
 * shader execution.
 */
static GLuint oit_quad_vao, oit_quad_vbo;

// size of each struct oit_node in the GLSL fragment shader
#define OIT_NODE_SIZE (4 * sizeof(GLfloat) + sizeof(GLfloat) + sizeof(GLuint))

/*
 * arbitrary limit - here we set it to 32 * typical screen dims
 *
 * TODO: when we finally implement high-resolution rendering, will probably want
 * to scale this with the screen resolution.  otherwise we'll be more likely to
 * run out of oit nodes in higher resolutions
 */
#define MAX_OIT_NODES (640 * 480 * 32)

#define OIT_NODE_COUNT_BINDING 0

#define OIT_BUFFER_NODES_BINDING 0

#define OIT_HEADS_BINDING 0

/*******************************************************************************
 *
 * RENDERDOC API SUPPORT
 *
 * RenderDoc is an open-source graphics debugger that comes in handy every now
 * and again.  WashingtonDC's rendering pipeline always renders everything to an
 * off-screen buffer and then renders that onto the screen as a textured quad
 * when it's time to present; this can cause problems with RenderDoc because the
 * debugger will only see us rendering the textured quad instead of the texture
 * that went onto the quad.  We fix this by using RenderDoc's API to show it
 * where each capture needs to begin and end.
 *
 * The capture key is set from the wash.ctrl.renderdoc-capture keybind; default
 * binding is f10.  YOU MUST PRESS THIS KEY, NOT THE KEY THAT RENDERDOC TELLS
 * YOU TO PRESS.  Otherwise, the capture will be triggered externally instead
 * of being triggered by WashingtonDC via renderdoc's API and renderdoc will
 * just show you WashingtonDC presenting a textured quad as described above.
 *
 ******************************************************************************/
static RENDERDOC_API_1_4_1 *rdoc_api = NULL;
static bool renderdoc_capture_requested, renderdoc_capture_in_progress;

static void capture_renderdoc(void) {
    renderdoc_capture_requested = true;
}

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

struct oit_group {
    float const *verts;
    unsigned n_verts;

    float avg_depth;

    GLfloat user_clip[4];

    struct gfx_rend_param rend_param;
};

static struct oit_state {
    bool enabled;
} oit_state;

// converts pixels from ARGB 4444 to RGBA 4444
static void render_conv_argb_4444(uint16_t *pixels, size_t n_pixels);

// converts pixels from ARGB 1555 to ABGR1555
static void render_conv_argb_1555(uint16_t *pixels, size_t n_pizels);

static void opengl_render_init(void);
static void opengl_render_cleanup(void);
static void gfxgl4_renderer_set_blend_enable(struct gfx_il_inst *cmd);
static void gfxgl4_renderer_set_rend_param(struct gfx_il_inst *cmd);
static void gfxgl4_renderer_draw_array(struct gfx_il_inst *cmd);
static void gfxgl4_renderer_clear(struct gfx_il_inst *cmd);
static void gfxgl4_renderer_set_screen_dim(unsigned width, unsigned height);
static void gfxgl4_renderer_set_clip_range(struct gfx_il_inst *cmd);
static void gfxgl4_renderer_begin_sort_mode(struct gfx_il_inst *cmd);
static void gfxgl4_renderer_end_sort_mode(struct gfx_il_inst *cmd);
static void gfxgl4_renderer_bind_tex(struct gfx_il_inst *cmd);
static void gfxgl4_renderer_unbind_tex(struct gfx_il_inst *cmd);
static void gfxgl4_renderer_obj_init(struct gfx_il_inst *cmd);
static void gfxgl4_renderer_obj_write(struct gfx_il_inst *cmd);
static void gfxgl4_renderer_obj_read(struct gfx_il_inst *cmd);
static void gfxgl4_renderer_obj_free(struct gfx_il_inst *cmd);
static void gfxgl4_renderer_grab_framebuffer(struct gfx_il_inst *cmd);
static void gfxgl4_renderer_post_framebuffer(struct gfx_il_inst *cmd);
static void gfxgl4_renderer_begin_rend(struct gfx_il_inst *cmd);
static void gfxgl4_renderer_end_rend(struct gfx_il_inst *cmd);

static void
gfxgl4_renderer_exec_gfx_il(struct gfx_il_inst *cmd, unsigned n_cmd);

static void do_set_rend_param(struct gfx_rend_param const *param);
static void do_draw_array(float const *verts, unsigned n_verts);

static void set_callbacks(struct renderer_callbacks const *callbacks);

struct gfx_rend_if const gfxgl4_rend_if = {
    .init = opengl_render_init,
    .cleanup = opengl_render_cleanup,
    .exec_gfx_il = gfxgl4_renderer_exec_gfx_il
};

struct renderer const gfxgl4_renderer = {
    .rend_if = &gfxgl4_rend_if,
    .set_callbacks = set_callbacks,
    .video_present = gfxgl4_video_present,
    .toggle_video_filter = gfxgl4_video_toggle_filter,
    .capture_renderdoc = capture_renderdoc
};

static void init_renderdoc_api(void);
static void cleanup_renderdoc_api(void);
static bool is_renderdoc_enabled(void);

static char const * const pvr2_ta_vert_glsl =
    "layout (location = 0) in vec3 vert_pos;\n"
    "layout (location = 1) in vec4 base_color;\n"
    "layout (location = 2) in vec4 offs_color;\n"

    "#ifdef TEX_ENABLE\n"
    "layout (location = 3) in vec2 tex_coord_in;\n"
    "#endif\n"

    "uniform mat4 trans_mat;\n"

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
    "    gl_Position = vec4(pos.x * pos.w, pos.y * pos.w, pos.z * pos.w, pos.w);\n"
    "}\n"

    "void color_transform() {\n"
    "#ifdef COLOR_ENABLE\n"
    "    vert_base_color = base_color;\n"
    "    vert_offs_color = offs_color;\n"
    "#else\n"
    "    vert_base_color = vec4(1.0, 1.0, 1.0, 1.0);\n"
    "    vert_offs_color = vec4(0.0, 0.0, 0.0, 0.0);\n"
    "#endif\n"
    "}\n"

    "void main() {\n"
    "    modelview_project_transform();\n"
    "    color_transform();\n"
    "    tex_transform();\n"
    "}\n";

#define OIT_NODE_GLSL_DEF                                               \
    "#define OIT_NODE_INVALID 0xffffffff\n"                             \
                                                                        \
    "struct oit_pixel {\n"                                              \
    "    vec4 color;\n"                                                 \
    "    float depth;\n"                                                \
    "    unsigned int src_blend_factor, dst_blend_factor;\n"            \
    "};\n"                                                              \
                                                                        \
    "struct oit_node {\n"                                               \
    "    struct oit_pixel pix;\n"                                       \
    "    unsigned int next_node;\n"                                     \
    "};\n"                                                              \
                                                                        \
    "layout (binding = 0) uniform atomic_uint node_count;\n"            \
    "layout(std430, binding = 0) coherent buffer oit_shared_data {\n"   \
    "    oit_node oit_nodes[];\n"                                       \
    "};\n"

static char const * const pvr2_ta_frag_glsl =
    "#define TEX_INST_DECAL 0\n"
    "#define TEX_INST_MOD 1\n"
    "#define TEX_INST_DECAL_ALPHA 2\n"
    "#define TEX_INST_MOD_ALPHA 3\n"

    "in vec4 vert_base_color, vert_offs_color;\n"
    "out vec4 out_color;\n"

    "#ifdef OIT_ENABLE\n"

    "layout(early_fragment_tests) in;\n"
    "uniform int MAX_OIT_NODES;\n"
    "uniform int src_blend_factor, dst_blend_factor;\n"

    OIT_NODE_GLSL_DEF

    /*
     * each pixel in oit_heads >= 0 points to an index in oit_node that is the
     * beginning of that pixel's linked-list.
     */
    "layout(r32ui, binding = 0) uniform coherent uimage2D oit_heads;\n"

    "#endif\n"

    "#ifdef TEX_ENABLE\n"
    "in vec2 st;\n"
    "uniform sampler2D bound_tex;\n"
    "#endif\n"

    "#ifdef USER_CLIP_ENABLE\n"
    /*
     * user_clip.x - x_min
     * user_clip.y - y_min
     * user_clip.z - x_max
     * user_clip.w - y_max
     */
    "uniform vec4 user_clip;\n"

    "void user_clip_test() {\n"
    "    bool in_rect = gl_FragCoord.x >= user_clip[0] &&\n"
    "        gl_FragCoord.x <= user_clip[2] &&\n"
    "        gl_FragCoord.y >= user_clip[1] &&\n"
    "        gl_FragCoord.y <= user_clip[3];\n"
    "#ifdef USER_CLIP_INVERT\n"
    "    if (in_rect)\n"
    "        discard;\n"
    "#else\n"
    "    if (!in_rect)\n"
    "        discard;\n"
    "#endif\n"
    "}\n"
    "#endif\n"

    "#ifdef PUNCH_THROUGH_ENABLE\n"
    "uniform int pt_alpha_ref;\n"

    "void punch_through_test(float alpha) {\n"
    "    if (int(alpha * 255) < pt_alpha_ref)\n"
    "        discard;\n"
    "}\n"
    "#endif\n"

    "#ifdef TEX_ENABLE\n"
    "vec4 eval_tex_inst() {\n"
    "    vec4 tex_color = texture(bound_tex, st);\n"
    "    vec4 color;\n"
    // TODO: is the offset alpha color supposed to be used for anything?
    "#if TEX_INST == TEX_INST_DECAL\n"
    "        color.rgb = tex_color.rgb + vert_offs_color.rgb;\n"
    "        color.a = tex_color.a;\n"
    "#elif TEX_INST == TEX_INST_MOD\n"
    "        color.rgb = tex_color.rgb * vert_base_color.rgb + vert_offs_color.rgb;\n"
    "        color.a = tex_color.a;\n"
    "#elif TEX_INST == TEX_INST_DECAL_ALPHA\n"
    "        color.rgb = tex_color.rgb * tex_color.a +\n"
    "            vert_base_color.rgb * (1.0 - tex_color.a) + vert_offs_color.rgb;\n"
    "        color.a = vert_base_color.a;\n"
    "#elif TEX_INST == TEX_INST_MOD_ALPHA\n"
    "        color.rgb = tex_color.rgb * vert_base_color.rgb + vert_offs_color.rgb;\n"
    "        color.a = tex_color.a * vert_base_color.a;\n"
    "#else\n"
    "#error unknown TEX_INST\n"
    "#endif\n"
    "    return color;\n"
    "}\n"
    "#endif\n"

    "void main() {\n"

    "#ifdef USER_CLIP_ENABLE\n"
    "    user_clip_test();\n"
    "#endif\n"

    "    vec4 color;\n"
    "#ifdef TEX_ENABLE\n"
    "    color = eval_tex_inst();\n"
    "#else\n"
    "    color = vert_base_color;\n"
    "#endif\n"

    /*
     * adding the vertex offset color can cause color to become out-of-bounds
     * we need to correct it here so it's right later on when we blend the
     * pixels together during the sorting pass.
     */
    "color = clamp(color, 0.0, 1.0);\n"

    "#ifdef PUNCH_THROUGH_ENABLE\n"
    "    punch_through_test(color.a);\n"
    "#endif\n"

    "#ifdef OIT_ENABLE\n"
    "    unsigned int node_idx = atomicCounterIncrement(node_count);\n"
    "    if (node_idx < MAX_OIT_NODES) {\n"
    "        oit_nodes[node_idx].pix.color = color;\n"
    "        oit_nodes[node_idx].pix.depth = gl_FragCoord.z;\n"
    "        oit_nodes[node_idx].pix.src_blend_factor = src_blend_factor;\n"
    "        oit_nodes[node_idx].pix.dst_blend_factor = dst_blend_factor;\n"

    "        oit_nodes[node_idx].next_node =\n"
    "            imageAtomicExchange(oit_heads, ivec2(gl_FragCoord.xy), node_idx);\n"
    "    }\n"
    "#endif\n"

    "    // when OIT_ENABLE is true, color and depth writes\n"
    "    // are both masked so this does nothing.\n"
    "    out_color = color;\n"
    "}\n";

static char const * const oit_sort_vert_shader =
    "#extension GL_ARB_explicit_uniform_location : enable\n"
    "layout (location = 0) in vec4 vert_pos;\n"
    "void main() { gl_Position = vert_pos; }\n";

static char const * const oit_sort_frag_shader =
    OIT_NODE_GLSL_DEF

    "out vec4 out_color;\n"

    "layout(r32ui, binding = 0) uniform coherent uimage2D oit_heads;\n"
    "uniform sampler2D color_accum;\n"

    "vec4 eval_src_blend_factor(unsigned int factor, vec4 src, vec4 dst) {\n"
    "    switch (factor) {\n"
    "    default:\n"
    "    case 0:\n"
    "        return vec4(0, 0, 0, 0);\n"
    "    case 1:\n"
    "        return vec4(1, 1, 1, 1);\n"
    "    case 2:\n"
    "        return dst;\n"
    "    case 3:\n"
    "        return vec4(1.0 - dst.r, 1.0 - dst.g, 1.0 - dst.b, 1.0 - dst.a);\n"
    "    case 4:\n"
    "        return vec4(src.a, src.a, src.a, src.a);\n"
    "    case 5:\n"
    "        return vec4(1.0 - src.a, 1.0 - src.a, 1.0 - src.a, 1.0 - src.a);\n"
    "    case 6:\n"
    "        return vec4(dst.a, dst.a, dst.a, dst.a);\n"
    "    case 7:\n"
    "        return vec4(1.0 - dst.a, 1.0 - dst.a, 1.0 - dst.a, 1.0 - dst.a);\n"
    "    }\n"
    "}\n"

    "vec4 eval_dst_blend_factor(unsigned int factor, vec4 src, vec4 dst) {\n"
    "    switch (factor) {\n"
    "    default:\n"
    "    case 0:\n"
    "        return vec4(0, 0, 0, 0);\n"
    "    case 1:\n"
    "        return vec4(1, 1, 1, 1);\n"
    "    case 2:\n"
    "        return src;\n"
    "    case 3:\n"
    "        return vec4(1.0 - src.r, 1.0 - src.g, 1.0 - src.b, 1.0 - src.a);\n"
    "    case 4:\n"
    "        return vec4(src.a, src.a, src.a, src.a);\n"
    "    case 5:\n"
    "        return vec4(1.0 - src.a, 1.0 - src.a, 1.0 - src.a, 1.0 - src.a);\n"
    "    case 6:\n"
    "        return vec4(dst.a, dst.a, dst.a, dst.a);\n"
    "    case 7:\n"
    "        return vec4(1.0 - dst.a, 1.0 - dst.a, 1.0 - dst.a, 1.0 - dst.a);\n"
    "    }\n"
    "}\n"

    "void swap_oit_nodes(uint node1, uint node2) {\n"
    "    oit_pixel pix_tmp = oit_nodes[node1].pix;\n"
    "    oit_nodes[node1].pix = oit_nodes[node2].pix;\n"
    "    oit_nodes[node2].pix = pix_tmp;\n"
    "}\n"


    "// it's my favorite algorithm, the insertion sort!\n"
    "void sort_list(unsigned int index) {\n"
    "    unsigned int src_idx = index;"
    "    while (src_idx != OIT_NODE_INVALID) {\n"
    "        unsigned int cmp_idx = oit_nodes[src_idx].next_node;\n"
    "        while (cmp_idx != OIT_NODE_INVALID) {\n"
    "            if (oit_nodes[cmp_idx].pix.depth >= oit_nodes[src_idx].pix.depth)\n"
    "                swap_oit_nodes(src_idx, cmp_idx);\n"
    "            cmp_idx = oit_nodes[cmp_idx].next_node;\n"
    "        }\n"
    "        src_idx = oit_nodes[src_idx].next_node;\n"
    "    }\n"
    "}\n"

    "void main() {\n"
    "    unsigned int head = imageLoad(oit_heads, ivec2(gl_FragCoord.xy))[0];\n"
    "    sort_list(head);\n"

    "    unsigned int cur_node = head;\n"
    "    vec4 color = texture(color_accum, gl_FragCoord.xy / textureSize(color_accum, 0));\n"

    /*
     * XXX this is actually wrong because gl_FragCoord.z samples from  the
     * fullscreen quad, *not* the original fragment value.  The reason why that
     * doesn't matter is because we discard when there are no transparent pixels
     * to render, which will preserve the original depth value.
     */
    "    float depth = gl_FragCoord.z;\n"

    "    // skip fragments that have no transparent pixels to render\n"
    "    if (cur_node == OIT_NODE_INVALID)\n"
    "        discard;\n"

    "    while (cur_node != OIT_NODE_INVALID) {\n"
    "        oit_pixel pix_in = oit_nodes[cur_node].pix;\n"
    "        vec4 color_in = pix_in.color;\n"
    "        vec4 src_factor = eval_src_blend_factor(pix_in.src_blend_factor, color_in, color);\n"
    "        vec4 dst_factor = eval_dst_blend_factor(pix_in.dst_blend_factor, color_in, color);\n"
    "        color = clamp(src_factor * color_in + dst_factor * color, 0, 1);\n"
    "        depth = oit_nodes[cur_node].pix.depth;\n"
    "        cur_node = oit_nodes[cur_node].next_node;\n"
    "    }\n"
    "    gl_FragDepth = depth;\n"
    "    out_color = color;\n"
    "}"
    ;

static struct shader_cache_ent* create_shader(shader_key key) {
    #define PREAMBLE_LEN 256
    static char preamble[PREAMBLE_LEN];
    bool tex_en = key & SHADER_KEY_TEX_ENABLE_BIT;
    bool color_en = key & SHADER_KEY_COLOR_ENABLE_BIT;
    bool punchthrough = key & SHADER_KEY_PUNCH_THROUGH_BIT;
    int tex_inst = key & SHADER_KEY_TEX_INST_MASK;
    bool user_clip_en = key & SHADER_KEY_USER_CLIP_ENABLE_BIT;
    bool user_clip_invert = key & SHADER_KEY_USER_CLIP_INVERT_BIT;
    bool oit_en = key & SHADER_KEY_OIT_BIT;

    char const *tex_inst_str = "";
    if (tex_en) {
        switch (tex_inst) {
        case SHADER_KEY_TEX_INST_DECAL_BIT:
            tex_inst_str = "#define TEX_INST TEX_INST_DECAL\n";
            break;
        case SHADER_KEY_TEX_INST_MOD_BIT:
            tex_inst_str = "#define TEX_INST TEX_INST_MOD\n";
            break;
        case SHADER_KEY_TEX_INST_DECAL_ALPHA_BIT:
            tex_inst_str = "#define TEX_INST TEX_INST_DECAL_ALPHA\n";
            break;
        case SHADER_KEY_TEX_INST_MOD_ALPHA_BIT:
            tex_inst_str = "#define TEX_INST TEX_INST_MOD_ALPHA\n";
            break;
        default:
            /*
             * this ought to be impossible since SHADER_KEY_TEX_INST_MASK
             * is two bits
             */
            fprintf(stderr, "Unknown tex_inst %d\n", tex_inst);
            tex_en = false;
        }
    }

    snprintf(preamble, PREAMBLE_LEN, "%s%s%s%s%s%s%s",
             tex_en ? "#define TEX_ENABLE\n" : "",
             color_en ? "#define COLOR_ENABLE\n" : "",
             punchthrough ? "#define PUNCH_THROUGH_ENABLE\n" : "",
             user_clip_en ? "#define USER_CLIP_ENABLE\n" : "",
             user_clip_invert ? "#define USER_CLIP_INVERT\n" : "",
             oit_en ? "#define OIT_ENABLE\n" : "",
             tex_inst_str);
    preamble[PREAMBLE_LEN - 1] = '\0';

    struct shader_cache_ent *ent = shader_cache_add_ent(&shader_cache, key);

    if (!ent) {
        fprintf(stderr, "Failure to create shader cache for key 0x%08x\n!",
                (int)key);
        return NULL;
    }

    shader_load_vert_with_preamble(&ent->shader, SHADER_VER_430,
                                   pvr2_ta_vert_glsl, preamble);
    shader_load_frag_with_preamble(&ent->shader, SHADER_VER_430,
                                   pvr2_ta_frag_glsl, preamble);
    shader_link(&ent->shader);

    /*
     * not all of these are valid for every shader.  This is alright because
     * glGetUniformLocation will return -1 for invalid uniform handles.
     * When -1 is passed as a uniform location to glUniform*, it will silently
     * fail without error.
     */
    ent->slots[SHADER_CACHE_SLOT_BOUND_TEX] =
        glGetUniformLocation(ent->shader.shader_prog_obj, "bound_tex");
    ent->slots[SHADER_CACHE_SLOT_PT_ALPHA_REF] =
        glGetUniformLocation(ent->shader.shader_prog_obj, "pt_alpha_ref");
    ent->slots[SHADER_CACHE_SLOT_TRANS_MAT] =
        glGetUniformLocation(ent->shader.shader_prog_obj, "trans_mat");
    ent->slots[SHADER_CACHE_SLOT_USER_CLIP] =
        glGetUniformLocation(ent->shader.shader_prog_obj, "user_clip");
    ent->slots[SHADER_CACHE_SLOT_MAX_OIT_NODES] =
        glGetUniformLocation(ent->shader.shader_prog_obj, "MAX_OIT_NODES");
    ent->slots[SHADER_CACHE_SLOT_SRC_BLEND_FACTOR] =
        glGetUniformLocation(ent->shader.shader_prog_obj, "src_blend_factor");
    ent->slots[SHADER_CACHE_SLOT_DST_BLEND_FACTOR] =
        glGetUniformLocation(ent->shader.shader_prog_obj, "dst_blend_factor");

    return ent;
}

static DEF_ERROR_INT_ATTR(shader_cache_key)

static struct shader_cache_ent* fetch_shader(shader_key key) {
    struct shader_cache_ent *shader_ent =
        shader_cache_find(&shader_cache, key);
    if (shader_ent)
        return shader_ent;
    shader_ent = create_shader(key);
    if (shader_ent)
        return shader_ent;

    error_set_shader_cache_key(key);
    RAISE_ERROR(ERROR_FAILED_ALLOC);
}

static void set_callbacks(struct renderer_callbacks const *callbacks) {
    switch_table = callbacks;
}

static void init_renderdoc_api(void) {
#ifdef _WIN32
    HMODULE renderdoc_dll = GetModuleHandleA("renderdoc.dll");
    if (renderdoc_dll) {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI =
            (pRENDERDOC_GetAPI)GetProcAddress(renderdoc_dll, "RENDERDOC_GetAPI");
        if (RENDERDOC_GetAPI) {
            if (RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_4_1,
                                 (void **)&rdoc_api) != 1) {
                rdoc_api = NULL;
            }
        }
    }
#else
    void *renderdoc_so = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD);
    if (renderdoc_so) {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI =
            (pRENDERDOC_GetAPI)dlsym(renderdoc_so, "RENDERDOC_GetAPI");
        if (RENDERDOC_GetAPI) {
            if (RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_4_1,
                                 (void **)&rdoc_api) != 1) {
                rdoc_api = NULL;
            }
        }
    }
#endif

    if (is_renderdoc_enabled())
        printf("OpenGL renderer: renderdoc API is enabled\n");
    else
        printf("OpenGL renderer: renderdoc API is disabled\n");
}


static void cleanup_renderdoc_api(void) {
}

static bool is_renderdoc_enabled(void) {
    return rdoc_api && rdoc_api->StartFrameCapture;
}

static void opengl_render_init(void) {
    user_clip_slot = -1;

    init_renderdoc_api();

    gfxgl4_tex_cache_init();

    win_make_context_current();
    glewExperimental = GL_TRUE;
    glewInit();

    gfxgl4_video_output_init();
    gfxgl4_target_init();

    gfx_config_oit_enable();

    shader_cache_init(&shader_cache);

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

    glClear(GL_COLOR_BUFFER_BIT);

    // initialize oit-related things
    glGenBuffers(N_OIT_BUFFERS, oit_buffers);

    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, oit_buffers[OIT_BUFFER_NODE_COUNT]);
    glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(GLuint),
                 NULL, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, oit_buffers[OIT_BUFFER_NODES_SSBO]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, OIT_NODE_SIZE * MAX_OIT_NODES,
                 NULL, GL_DYNAMIC_DRAW);

    glGenTextures(1, &oit_heads_tex);
    glGenTextures(1, &oit_color_tex);

    shader_load_vert(&oit_sort_shader, SHADER_VER_430, oit_sort_vert_shader);
    shader_load_frag(&oit_sort_shader, SHADER_VER_430, oit_sort_frag_shader);
    shader_link(&oit_sort_shader);
    oit_sort_shader_color_accum_slot =
        glGetUniformLocation(oit_sort_shader.shader_prog_obj, "color_accum");

    GLfloat quad_dat[16] = {
        -1.0f, -1.0f, 0.0f, 1.0f,
        1.0f, -1.0f, 0.0f, 1.0f,
        -1.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 0.0f, 1.0f
    };
    glGenVertexArrays(1, &oit_quad_vao);
    glGenBuffers(1, &oit_quad_vbo);
    glBindVertexArray(oit_quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, oit_quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_dat), quad_dat, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

static void opengl_render_cleanup(void) {
    glDeleteVertexArrays(1, &oit_quad_vao);
    glDeleteBuffers(1, &oit_quad_vbo);

    shader_cleanup(&oit_sort_shader);

    glDeleteTextures(1, &oit_color_tex);
    glDeleteTextures(1, &oit_heads_tex);

    glDeleteBuffers(N_OIT_BUFFERS, oit_buffers);
    memset(oit_buffers, 0, sizeof(oit_buffers));

    glDeleteTextures(GFX_OBJ_COUNT, obj_tex_array);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);

    shader_cache_cleanup(&shader_cache);

    vao = 0;
    vbo = 0;
    memset(obj_tex_array, 0, sizeof(obj_tex_array));

    gfxgl4_tex_cache_cleanup();

    cleanup_renderdoc_api();

    user_clip_slot = -1;
}

static DEF_ERROR_INT_ATTR(max_length);

void gfxgl4_renderer_update_tex(unsigned tex_obj) {
    struct gfxgl4_tex const *tex = gfx_gfxgl4_tex_cache_get(tex_obj);
    struct gfx_obj *obj = gfx_obj_get(tex->obj_handle);

    // nothing to do here
    if (obj->state & GFX_OBJ_STATE_TEX)
        return;

    gfx_obj_alloc(obj);

    void const *tex_dat = obj->dat;

    GLenum internal_format, format;
    switch (tex->tex_fmt) {
    case GFX_TEX_FMT_RGB_565:
        internal_format = GL_RGB;
        format = GL_RGB;
        break;
    case GFX_TEX_FMT_ARGB_8888:
        internal_format = GL_RGBA;
        format = GL_BGRA;
        break;
    default:
        internal_format = GL_RGBA;
        format = GL_RGBA;
    }

    unsigned tex_w = tex->width;
    unsigned tex_h = tex->height;

    glBindTexture(GL_TEXTURE_2D, obj_tex_array[tex->obj_handle]);
    // TODO: maybe don't always set this to 1
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    /*
     * TODO: ideally I wouldn't need to copy ARGB_4444 and ARGB_1555 into a
     * separate buffer to do the pixel conversion.  The reason I do this is that
     * the tex-dump command in the cmd thread also sees the texture data in the
     * struct gfxgl4_tex, so I don't want to modify that.  Maybe someday I'll
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
        gfxgl4_renderer_tex_set_dims(tex->obj_handle, tex_w, tex_h);
        gfxgl4_renderer_tex_set_format(tex->obj_handle, format);
        gfxgl4_renderer_tex_set_dat_type(tex->obj_handle,
                                         tex_fmt_to_data_type(GFX_TEX_FMT_ARGB_4444));
        gfxgl4_renderer_tex_set_dirty(tex->obj_handle, false);
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
        gfxgl4_renderer_tex_set_dims(tex->obj_handle, tex_w, tex_h);
        gfxgl4_renderer_tex_set_format(tex->obj_handle, format);
        gfxgl4_renderer_tex_set_dat_type(tex->obj_handle,
                                         tex_fmt_to_data_type(GFX_TEX_FMT_ARGB_1555));
        gfxgl4_renderer_tex_set_dirty(tex->obj_handle, false);
        free(tex_dat_conv);
    } else if (tex->tex_fmt == GFX_TEX_FMT_YUV_422) {
        uint8_t *tmp_dat =
            (uint8_t*)malloc(sizeof(uint8_t) * 4 * tex_w * tex_h);
        if (!tmp_dat)
            RAISE_ERROR(ERROR_FAILED_ALLOC);
        washdc_conv_yuv422_rgba8888(tmp_dat, tex_dat, tex_w, tex_h);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_w, tex_h, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, tmp_dat);
        gfxgl4_renderer_tex_set_dims(tex->obj_handle, tex_w, tex_h);
        gfxgl4_renderer_tex_set_format(tex->obj_handle, GL_RGBA);
        gfxgl4_renderer_tex_set_dat_type(tex->obj_handle, GL_UNSIGNED_BYTE);
        gfxgl4_renderer_tex_set_dirty(tex->obj_handle, false);
        free(tmp_dat);
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, tex_w, tex_h, 0,
                     format, tex_fmt_to_data_type(tex->tex_fmt), tex_dat);
        gfxgl4_renderer_tex_set_dims(tex->obj_handle, tex_w, tex_h);
        gfxgl4_renderer_tex_set_format(tex->obj_handle, format);
        gfxgl4_renderer_tex_set_dat_type(tex->obj_handle,
                                         tex_fmt_to_data_type(tex->tex_fmt));
        gfxgl4_renderer_tex_set_dirty(tex->obj_handle, false);
    }
    obj->state |= GFX_OBJ_STATE_TEX;
    glBindTexture(GL_TEXTURE_2D, 0);
}

void gfxgl4_renderer_release_tex(unsigned tex_obj) {
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

static void gfxgl4_renderer_set_blend_enable(struct gfx_il_inst *cmd) {
    bool enable = cmd->arg.set_blend_enable.do_enable;
    struct gfx_cfg rend_cfg = gfx_config_read();

    if (rend_cfg.blend_enable && enable)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
}

static void gfxgl4_renderer_set_rend_param(struct gfx_il_inst *cmd) {
    struct gfx_rend_param const *param = &cmd->arg.set_rend_param.param;
    do_set_rend_param(param);
}

static void do_set_rend_param(struct gfx_rend_param const *param) {
    struct gfx_cfg rend_cfg = gfx_config_read();

    /*
     * TODO: currently disable color also disables textures; ideally these
     * would be two independent settings.
     */
    shader_key shader_cache_key;
    if (param->tex_enable && rend_cfg.tex_enable && rend_cfg.color_enable) {
        shader_cache_key =
            SHADER_KEY_TEX_ENABLE_BIT | SHADER_KEY_COLOR_ENABLE_BIT;

        switch (param->tex_inst) {
        case TEX_INST_DECAL:
            shader_cache_key |= SHADER_KEY_TEX_INST_DECAL_BIT;
            break;
        case TEX_INST_MOD:
            shader_cache_key |= SHADER_KEY_TEX_INST_MOD_BIT;
            break;
        case TEXT_INST_DECAL_ALPHA:
            shader_cache_key |= SHADER_KEY_TEX_INST_DECAL_ALPHA_BIT;
            break;
        case TEX_INST_MOD_ALPHA:
            shader_cache_key |= SHADER_KEY_TEX_INST_MOD_ALPHA_BIT;
            break;
        }

        if (gfx_gfxgl4_tex_cache_get(param->tex_idx)->valid) {
            int obj_handle = gfx_gfxgl4_tex_cache_get(param->tex_idx)->obj_handle;
            glBindTexture(GL_TEXTURE_2D, obj_tex_array[obj_handle]);
        } else {
            fprintf(stderr, "WARNING: attempt to bind invalid texture %u\n",
                    (unsigned)param->tex_idx);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        switch (param->tex_filter) {
        case TEX_FILTER_TRILINEAR_A:
        case TEX_FILTER_TRILINEAR_B:
            fprintf(stderr,
                    "WARNING: trilinear filtering is not yet supported\n");
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

        glActiveTexture(GL_TEXTURE0);
    } else if (rend_cfg.color_enable) {
        shader_cache_key = SHADER_KEY_COLOR_ENABLE_BIT;
    } else {
        shader_cache_key = 0;
    }

    if (param->pt_mode && rend_cfg.pt_enable)
        shader_cache_key |= SHADER_KEY_PUNCH_THROUGH_BIT;

    user_clip_mode = param->user_clip_mode;

    if (user_clip_mode == GFX_USER_CLIP_INSIDE) {
        shader_cache_key |= SHADER_KEY_USER_CLIP_ENABLE_BIT;
    } else if (user_clip_mode == GFX_USER_CLIP_OUTSIDE) {
        shader_cache_key |= SHADER_KEY_USER_CLIP_ENABLE_BIT |
            SHADER_KEY_USER_CLIP_INVERT_BIT;
    }

    if (oit_state.enabled)
        shader_cache_key |= SHADER_KEY_OIT_BIT;

    struct shader_cache_ent *shader_ent = fetch_shader(shader_cache_key);
    if (!shader_ent) {
        fprintf(stderr, "%s Failure to set render parameter: unable to find "
                "texture with key 0x%08x\n", __func__, (int)shader_cache_key);
        return;
    }
    glUseProgram(shader_ent->shader.shader_prog_obj);
    glUniform1i(shader_ent->slots[SHADER_CACHE_SLOT_BOUND_TEX], 0);
    glUniform1i(shader_ent->slots[SHADER_CACHE_SLOT_PT_ALPHA_REF],
                param->pt_ref - 1);
    trans_mat_slot = shader_ent->slots[SHADER_CACHE_SLOT_TRANS_MAT];
    user_clip_slot = shader_ent->slots[SHADER_CACHE_SLOT_USER_CLIP];
    glUniform4f(user_clip_slot, user_clip[0], user_clip[1],
                user_clip[2], user_clip[3]);
    glUniform1i(shader_ent->slots[SHADER_CACHE_SLOT_MAX_OIT_NODES],
                MAX_OIT_NODES);

    enum Pvr2BlendFactor cur_src_blend_factor = param->src_blend_factor;
    enum Pvr2BlendFactor cur_dst_blend_factor = param->dst_blend_factor;
    glUniform1i(shader_ent->slots[SHADER_CACHE_SLOT_SRC_BLEND_FACTOR],
                cur_src_blend_factor);
    glUniform1i(shader_ent->slots[SHADER_CACHE_SLOT_DST_BLEND_FACTOR],
                cur_dst_blend_factor);

    if (oit_state.enabled) {
        glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER,
                         OIT_NODE_COUNT_BINDING,
                         oit_buffers[OIT_BUFFER_NODE_COUNT]);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                         OIT_BUFFER_NODES_BINDING,
                         oit_buffers[OIT_BUFFER_NODES_SSBO]);
        glBindImageTexture(OIT_HEADS_BINDING, oit_heads_tex, 0,
                           GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
    }

    glBlendFunc(src_blend_factors[(unsigned)cur_src_blend_factor],
                dst_blend_factors[(unsigned)cur_dst_blend_factor]);

    /*
     * TODO: is it correct to unconditionally disable depth writes whenever oit
     * is being used?  Maybe need to preserve the value of enable_depth_writes
     * for the sort shader somehow.  This can effect the punch-throughs since
     * they get drawn last
     */
    glDepthMask(param->enable_depth_writes && !oit_state.enabled ?
                GL_TRUE : GL_FALSE);

    if (oit_state.enabled)
        glDepthFunc(depth_funcs[PVR2_DEPTH_GREATER]);
    else
        glDepthFunc(depth_funcs[param->depth_func]);

    tex_enable = param->tex_enable;
}

static void gfxgl4_renderer_draw_array(struct gfx_il_inst *cmd) {
    unsigned n_verts = cmd->arg.draw_array.n_verts;
    float const *verts = cmd->arg.draw_array.verts;

    unsigned vert_no;
    float *cur_vert = verts;
    for (vert_no = 0; vert_no < n_verts; vert_no++) {
        cur_vert[GFX_VERT_POS_OFFSET + 2] =
            1.0f / cur_vert[GFX_VERT_POS_OFFSET + 2];
        cur_vert += GFX_VERT_LEN;
    }

    do_draw_array(verts, n_verts);
}

static void do_draw_array(float const *verts, unsigned n_verts) {
    if (!n_verts)
        return;

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
        0, 0, 1, 0
    };

    glUniformMatrix4fv(trans_mat_slot, 1, GL_TRUE, trans_mat);

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

static void gfxgl4_renderer_clear(struct gfx_il_inst *cmd) {
    float const *bgcolor = cmd->arg.clear.bgcolor;
    struct gfx_cfg rend_cfg = gfx_config_read();

    if (!rend_cfg.wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    } else {
        glLineWidth(1);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

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

static void gfxgl4_renderer_set_screen_dim(unsigned width, unsigned height) {
    screen_width = width;
    screen_height = height;
    glViewport(0, 0, width, height);
}

static void gfxgl4_renderer_set_clip_range(struct gfx_il_inst *cmd) {
    clip_min = cmd->arg.set_clip_range.clip_min;
    clip_max = cmd->arg.set_clip_range.clip_max;
}

GLuint gfxgl4_renderer_tex(unsigned obj_no) {
    return obj_tex_array[obj_no];
}

unsigned gfxgl4_renderer_tex_get_width(unsigned obj_no) {
    return obj_tex_meta_array[obj_no].width;
}

unsigned gfxgl4_renderer_tex_get_height(unsigned obj_no) {
    return obj_tex_meta_array[obj_no].height;
}

void gfxgl4_renderer_tex_set_dims(unsigned obj_no,
                                  unsigned width, unsigned height) {
    obj_tex_meta_array[obj_no].width = width;
    obj_tex_meta_array[obj_no].height = height;
}

void gfxgl4_renderer_tex_set_format(unsigned obj_no, GLenum fmt) {
    obj_tex_meta_array[obj_no].format = fmt;
}

void gfxgl4_renderer_tex_set_dat_type(unsigned obj_no, GLenum dat_tp) {
    obj_tex_meta_array[obj_no].dat_type = dat_tp;
}

void gfxgl4_renderer_tex_set_dirty(unsigned obj_no, bool dirty) {
    obj_tex_meta_array[obj_no].dirty = dirty;
}

GLenum gfxgl4_renderer_tex_get_format(unsigned obj_no) {
    return obj_tex_meta_array[obj_no].format;
}

GLenum gfxgl4_renderer_tex_get_dat_type(unsigned obj_no) {
    return obj_tex_meta_array[obj_no].dat_type;
}

bool gfxgl4_renderer_tex_get_dirty(unsigned obj_no) {
    return obj_tex_meta_array[obj_no].dirty;
}

static void gfxgl4_renderer_begin_sort_mode(struct gfx_il_inst *cmd) {
    if (gfx_config_read().wireframe)
        return;

    if (oit_state.enabled)
        RAISE_ERROR(ERROR_INTEGRITY);

    oit_state.enabled = true;

    // per-pixel oit
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, oit_buffers[OIT_BUFFER_NODE_COUNT]);
    GLuint new_val = 0;
    glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(new_val), &new_val);
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

    /*
     * reset the oit_heads texture to all -1
     *
     * TODO: there has got to be a better way to do this than the way I'm doing
     * it now.
     */
    size_t oit_heads_len = screen_width * screen_height * sizeof(GLint);
    void *oit_reset_data = malloc(oit_heads_len);
    if (!oit_reset_data)
        abort();
    memset(oit_reset_data, 0xff, oit_heads_len);
    glBindTexture(GL_TEXTURE_2D, oit_heads_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI,
                 screen_width, screen_height, 0,
                 GL_RED_INTEGER, GL_UNSIGNED_INT, oit_reset_data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    free(oit_reset_data);

    glBindTexture(GL_TEXTURE_2D, oit_color_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 screen_width, screen_height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_FALSE);
}

static void gfxgl4_renderer_end_sort_mode(struct gfx_il_inst *cmd) {
    if (gfx_config_read().wireframe)
        return;

    glNamedFramebufferReadBuffer(gfxgl4_tgt_fbo, GL_COLOR_ATTACHMENT0);
    glBindTexture(GL_TEXTURE_2D, oit_color_tex);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, screen_width, screen_height, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (!oit_state.enabled)
        RAISE_ERROR(ERROR_INTEGRITY);

    oit_state.enabled = false;

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_ALWAYS);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
                    GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    glUseProgram(oit_sort_shader.shader_prog_obj);
    glBindVertexArray(oit_quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, oit_quad_vbo);
    glEnableVertexAttribArray(0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                     OIT_BUFFER_NODES_BINDING,
                     oit_buffers[OIT_BUFFER_NODES_SSBO]);
    glBindImageTexture(OIT_HEADS_BINDING, oit_heads_tex, 0,
                       GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);

    glBindTexture(GL_TEXTURE_2D, oit_color_tex);
    glUniform1i(oit_sort_shader_color_accum_slot, 0);
    glActiveTexture(GL_TEXTURE0);

    /*
     * disale  blending.  the fragment shader we just loaded will be doing
     * blending itself so we don't want OpenGL to also be blending.
     */
    glDisable(GL_BLEND);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
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

static void gfxgl4_renderer_bind_tex(struct gfx_il_inst *cmd) {
    unsigned tex_no = cmd->arg.bind_tex.tex_no;
    int obj_handle = cmd->arg.bind_tex.gfx_obj_handle;
    enum gfx_tex_fmt pix_fmt = cmd->arg.bind_tex.pix_fmt;
    int width = cmd->arg.bind_tex.width;
    int height = cmd->arg.bind_tex.height;

    gfxgl4_tex_cache_bind(tex_no, obj_handle, width, height, pix_fmt);
}

static void gfxgl4_renderer_unbind_tex(struct gfx_il_inst *cmd) {
    gfxgl4_tex_cache_unbind(cmd->arg.unbind_tex.tex_no);
}

static void gfxgl4_renderer_obj_init(struct gfx_il_inst *cmd) {
    int obj_no = cmd->arg.init_obj.obj_no;
    size_t n_bytes = cmd->arg.init_obj.n_bytes;
    gfx_obj_init(obj_no, n_bytes);
}

static void gfxgl4_renderer_obj_write(struct gfx_il_inst *cmd) {
    int obj_no = cmd->arg.write_obj.obj_no;
    size_t n_bytes = cmd->arg.write_obj.n_bytes;
    void const *dat = cmd->arg.write_obj.dat;
    gfx_obj_write(obj_no, dat, n_bytes);
}

static void gfxgl4_renderer_obj_read(struct gfx_il_inst *cmd) {
    int obj_no = cmd->arg.read_obj.obj_no;
    size_t n_bytes = cmd->arg.read_obj.n_bytes;
    void *dat = cmd->arg.read_obj.dat;
    gfx_obj_read(obj_no, dat, n_bytes);
}

static void gfxgl4_renderer_obj_free(struct gfx_il_inst *cmd) {
    int obj_no = cmd->arg.free_obj.obj_no;
    gfx_obj_free(obj_no);
}

static void gfxgl4_renderer_grab_framebuffer(struct gfx_il_inst *cmd) {
    int handle;
    unsigned width, height;
    bool do_flip;

    if (gfxgl4_video_get_fb(&handle, &width, &height, &do_flip) != 0) {
        cmd->arg.grab_framebuffer.fb->valid = false;
        return;
    }

    struct gfx_obj *obj = gfx_obj_get(handle);
    if (!obj) {
        cmd->arg.grab_framebuffer.fb->valid = false;
        return;
    }

    size_t n_bytes = obj->dat_len;
    void *dat = malloc(n_bytes);
    if (!dat) {
        cmd->arg.grab_framebuffer.fb->valid = false;
        return;
    }

    gfx_obj_read(handle, dat, n_bytes);

    cmd->arg.grab_framebuffer.fb->valid = true;
    cmd->arg.grab_framebuffer.fb->width = width;
    cmd->arg.grab_framebuffer.fb->height = height;
    cmd->arg.grab_framebuffer.fb->dat = dat;
    cmd->arg.grab_framebuffer.fb->flip = do_flip;
}

static void gfxgl4_renderer_post_framebuffer(struct gfx_il_inst *cmd) {
    unsigned width = cmd->arg.post_framebuffer.width;
    unsigned height = cmd->arg.post_framebuffer.height;
    int obj_handle = cmd->arg.post_framebuffer.obj_handle;
    bool do_flip = cmd->arg.post_framebuffer.vert_flip;
    bool interlace = cmd->arg.post_framebuffer.interlaced;

    gfxgl4_video_new_framebuffer(obj_handle, width, height,
                                 do_flip, interlace);
    gfxgl4_video_present();

    if (switch_table) {
        if (switch_table->overlay_draw)
            switch_table->overlay_draw();

        if (switch_table->win_update)
            switch_table->win_update();
    }
}

static void gfxgl4_renderer_begin_rend(struct gfx_il_inst *cmd) {
    if (!renderdoc_capture_in_progress && renderdoc_capture_requested) {
        if (is_renderdoc_enabled()) {
            rdoc_api->StartFrameCapture(NULL, NULL);
            renderdoc_capture_in_progress = true;
        }
        renderdoc_capture_requested = false;
    }

    gfxgl4_target_begin(cmd->arg.begin_rend.screen_width,
                        cmd->arg.begin_rend.screen_height,
                        cmd->arg.begin_rend.rend_tgt_obj);

    unsigned *clip = cmd->arg.begin_rend.clip;

    // flip y-coordinates of clip rectangle
    clip[1] = screen_height - 1 - clip[1];
    clip[3] = screen_height - 1 - clip[3];

    glEnable(GL_SCISSOR_TEST);
    glScissor(clip[0], clip[3], clip[2] - clip[0] + 1, clip[1] - clip[3] + 1);
    gfxgl4_renderer_set_screen_dim(cmd->arg.begin_rend.screen_width,
                                   cmd->arg.begin_rend.screen_height);
}

static void gfxgl4_renderer_end_rend(struct gfx_il_inst *cmd) {
    glDisable(GL_SCISSOR_TEST);
    gfxgl4_target_end(cmd->arg.end_rend.rend_tgt_obj);

    if (renderdoc_capture_in_progress && is_renderdoc_enabled()) {
        rdoc_api->EndFrameCapture(NULL, NULL);
        renderdoc_capture_in_progress = false;
    }
}

static void
gfxgl4_renderer_exec_gfx_il(struct gfx_il_inst *cmd, unsigned n_cmd) {
    while (n_cmd--) {
        switch (cmd->op) {
        case GFX_IL_BIND_TEX:
            gfxgl4_renderer_bind_tex(cmd);
            break;
        case GFX_IL_UNBIND_TEX:
            gfxgl4_renderer_unbind_tex(cmd);
            break;
        case GFX_IL_BIND_RENDER_TARGET:
            gfxgl4_target_bind_obj(cmd);
            break;
        case GFX_IL_UNBIND_RENDER_TARGET:
            gfxgl4_target_unbind_obj(cmd);
            break;
        case GFX_IL_BEGIN_REND:
            gfxgl4_renderer_begin_rend(cmd);
            break;
        case GFX_IL_END_REND:
            gfxgl4_renderer_end_rend(cmd);
            break;
        case GFX_IL_CLEAR:
            gfxgl4_renderer_clear(cmd);
            break;
        case GFX_IL_SET_BLEND_ENABLE:
            gfxgl4_renderer_set_blend_enable(cmd);
            break;
        case GFX_IL_SET_REND_PARAM:
            gfxgl4_renderer_set_rend_param(cmd);
            break;
        case GFX_IL_SET_CLIP_RANGE:
            gfxgl4_renderer_set_clip_range(cmd);
            break;
        case GFX_IL_DRAW_ARRAY:
            gfxgl4_renderer_draw_array(cmd);
            break;
        case GFX_IL_INIT_OBJ:
            gfxgl4_renderer_obj_init(cmd);
            break;
        case GFX_IL_WRITE_OBJ:
             gfxgl4_renderer_obj_write(cmd);
            break;
        case GFX_IL_READ_OBJ:
            gfxgl4_renderer_obj_read(cmd);
            break;
        case GFX_IL_FREE_OBJ:
            gfxgl4_renderer_obj_free(cmd);
            break;
        case GFX_IL_POST_FRAMEBUFFER:
            gfxgl4_renderer_post_framebuffer(cmd);
            break;
        case GFX_IL_GRAB_FRAMEBUFFER:
            gfxgl4_renderer_grab_framebuffer(cmd);
            break;
        case GFX_IL_BEGIN_DEPTH_SORT:
            gfxgl4_renderer_begin_sort_mode(cmd);
            break;
        case GFX_IL_END_DEPTH_SORT:
            gfxgl4_renderer_end_sort_mode(cmd);
            break;
        case GFX_IL_SET_USER_CLIP:
            user_clip[0] = cmd->arg.set_user_clip.x_min;

            if (cmd->arg.set_user_clip.y_max <= screen_height - 1)
                user_clip[1] = screen_height - 1 - cmd->arg.set_user_clip.y_max;
            else
                user_clip[1] = 0;
            user_clip[2] = cmd->arg.set_user_clip.x_max;

            if (cmd->arg.set_user_clip.y_min <= screen_height - 1)
                user_clip[3] = screen_height - 1 - cmd->arg.set_user_clip.y_min;
            else
                user_clip[3] = 0;

            glUniform4f(user_clip_slot, user_clip[0], user_clip[1],
                        user_clip[2], user_clip[3]);
            break;
        default:
            fprintf(stderr, "ERROR: UNKNOWN GFX IL COMMAND %02X\n",
                    (unsigned)cmd->op);
        }
        cmd++;
    }
}
