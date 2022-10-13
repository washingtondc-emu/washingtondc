/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2020, 2022 snickerbockers
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

#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#include <GL/gl.h>

#include "washdc/win.h"

#include "../config_file.h"
#include "../gfx_obj.h"
#include "gfxgl3_output.h"
#include "gfxgl3_renderer.h"
#include "../shader.h"

static void init_poly();

/*
 * this shader represents the final stage of output, where a single textured
 * quad is drawn covering the entirety of the screen.
 */
static struct shader fb_shader;

// If true, then the screen will be flipped vertically.
static bool do_flip;

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

static int bound_obj_handle;
static double bound_obj_w, bound_obj_h;
static bool bound_obj_interlace;
static GLenum min_filter = GL_NEAREST, mag_filter = GL_NEAREST;

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

static GLfloat bgcolor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

static void set_flip(bool flip);

static void
gfxgl3_video_update_framebuffer(int obj_handle,
                                unsigned fb_read_width,
                                unsigned fb_read_height,
                                bool interlace);

void gfxgl3_video_output_init(void) {
    char const *filter_str;

    filter_str = cfg_get_node("gfx.output.filter");

    if (filter_str) {
        if (strcmp(filter_str, "nearest") == 0) {
            min_filter = GL_NEAREST;
            mag_filter = GL_NEAREST;
        } else if (strcmp(filter_str, "linear") == 0) {
            min_filter = GL_LINEAR;
            mag_filter = GL_LINEAR;
        } else {
            min_filter = GL_LINEAR;
            mag_filter = GL_LINEAR;
        }
    } else {
        min_filter = GL_LINEAR;
        mag_filter = GL_LINEAR;
    }

    static char const * const final_vert_glsl =
#include "gfxgl3_final_vert.h"
        ;

    static char const * const final_frag_glsl =
#include "gfxgl3_final_frag.h"
        ;

    int rgb[3];
    if (cfg_get_rgb("ui.bgcolor", rgb, rgb + 1, rgb + 2) == 0) {
        bgcolor[0] = rgb[0] / 255.0f;
        bgcolor[1] = rgb[1] / 255.0f;
        bgcolor[2] = rgb[2] / 255.0f;
    }

    shader_load_vert(&fb_shader, "", final_vert_glsl);
    shader_load_frag(&fb_shader, "", final_frag_glsl);
    shader_link(&fb_shader);

    init_poly();
}

void gfxgl3_video_output_cleanup(void) {
    // TODO cleanup OpenGL stuff
}

void gfxgl3_video_new_framebuffer(int obj_handle,
                                  unsigned fb_new_width,
                                  unsigned fb_new_height,
                                  bool do_flip,
                                  bool interlace) {
    set_flip(do_flip);
    gfxgl3_video_update_framebuffer(obj_handle, fb_new_width, fb_new_height, interlace);
}

static void set_flip(bool flip) {
    do_flip = flip;
}

static void
gfxgl3_video_update_framebuffer(int obj_handle,
                                unsigned fb_read_width,
                                unsigned fb_read_height,
                                bool interlace) {
    if (obj_handle < 0)
        return;

    struct gfx_obj *obj = gfx_obj_get(obj_handle);
    GLuint tex_obj = gfxgl3_renderer_tex(obj_handle);

    glBindTexture(GL_TEXTURE_2D, tex_obj);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);

    if (!(obj->state & GFX_OBJ_STATE_TEX)) {
        if (obj->dat_len < fb_read_width * fb_read_height * sizeof(uint32_t)) {
            fprintf(stderr, "ERROR: INTEGRITY\n");
            abort();
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fb_read_width, fb_read_height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, obj->dat);

        gfxgl3_renderer_tex_set_dims(obj_handle, fb_read_width, fb_read_height);
        gfxgl3_renderer_tex_set_format(obj_handle, GL_RGBA);
        gfxgl3_renderer_tex_set_dat_type(obj_handle, GL_UNSIGNED_BYTE);
        gfxgl3_renderer_tex_set_dirty(obj_handle, false);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    bound_obj_handle = obj_handle;
    bound_obj_w = (double)fb_read_width;
    bound_obj_h = (double)fb_read_height;
    bound_obj_interlace = interlace;
}

void gfxgl3_video_present(void) {
    glClearColor(bgcolor[0], bgcolor[1], bgcolor[2], bgcolor[3]);
    glClear(GL_COLOR_BUFFER_BIT);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);

    int xres = win_get_width();
    int yres = win_get_height();

    double xres_dbl = (double)xres;
    double yres_dbl = (double)yres;

    /*
     * double up the height if we can.
     *
     * I'm not 100% clear on how 240p video ends up filling the entirety of my
     * 480-scanline TV screen, but it defintely does so I can only assume that
     * the hardware knows when it can double up the vertical resolution.
     */
    if (bound_obj_h * 2 <= 480)
        bound_obj_h *= 2;

    /*
     *
     * For interlace-scans, this would have already happened in the PowerVR2
     * emulation because the total scanline count including both fields is
     * actually twice the height of each individual field.  Progressive scans
     * are vertically stretched out by the TV, which is why the height gets
     * doubled here and not in the PowerVR2 emulation code.
     *
     * After doubling the height, it should be approximately 480 scanlines.  It is
     * common for games to make it slightly less than that, ie 476 scanlines.
     * The width is completely arbitrary since that's just an analog signal,
     * although it's usually either 320 or 240.
     *
     * So somehow, we need to divine what the intended aspect ratio is.  Analog
     * video didn't have a fixed horizontal resolution, it would just spit out
     * an analog signal and however many pixels the TV set had would be how the
     * horizontal resolution.  Scanlines are the only discrete element on an
     * analog video system, and thus the number of scanlines your TV set has is
     * fixed.
     *
     * Here we assume that the television set we're emulating has 480
     * scanlines, and 640 pixels per scanline.  If the game gives us a
     * framebuffer with less than 480 scanlines, then we'll treat the extra
     * scanlines in the emulated TV as being empty.  If the game gives us more
     * than 480 scanlines, then the excess scanlines won't be displayed.  No
     * matter how many horizontal pixels there are, the picture will always be
     * stretched or compressed to make the aspect ratio 4:3.
     */

    double clip_width, clip_height;
    if ((xres_dbl / yres_dbl) < (4.0 / 3.0)) {
        // output window is taller and narrower than 4:3
        clip_height = (3.0 / 4.0) * (xres_dbl / yres_dbl);
    } else {
        // output window is shorter and wider than 4:3
        clip_height = bound_obj_h < 480 ? bound_obj_h / 480.0 : 1.0;
    }

    /*
     * handle pictures that are smaller than 480 scanlines.
     *
     * XXX because of this, there will always be a little empty space on the
     * output window when the pictures are less than 480 scanlines.  It would
     * be possible to scale it up to fill either the width or height of the
     * screen.  Current behavior seems better since it leaves behind blank
     * parts of the screen like a real TV might, but arguably it's kinda
     * annoying since fullscreen mode isn't the entire screen.
     *
     * Also it might be more accurate to shift up so that the top scanline is
     * always at the top of the window.  Current implementation centers
     * everything, and I'm not sure if that's how a real CRT set would do it.
     */
    if (clip_height < 480)
        clip_height *= (bound_obj_h) / 480.0;

    clip_width = (4.0 / 3.0) * (yres_dbl / xres_dbl) * clip_height;

    // clip pictures that are bigger than 480 scanlines
    if (bound_obj_h > 480.0)
        tex_mat[4] = 480.0 / bound_obj_h;
    else
        tex_mat[4] = 1.0;

    trans_mat[0] = clip_width;
    trans_mat[5] = do_flip ? -clip_height : clip_height;

    glViewport(0, 0, xres, yres);
    glUseProgram(fb_shader.shader_prog_obj);
    glBindTexture(GL_TEXTURE_2D, gfxgl3_renderer_tex(bound_obj_handle));
    glUniform1i(glGetUniformLocation(fb_shader.shader_prog_obj, "fb_tex"), 0);
    glUniformMatrix4fv(OUTPUT_SLOT_TRANS_MAT, 1, GL_TRUE, trans_mat);
    glUniformMatrix3fv(OUTPUT_SLOT_TEX_MAT, 1, GL_TRUE, tex_mat);

    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(fb_poly.vao);
    glDrawElements(GL_TRIANGLE_STRIP, FB_QUAD_IDX_COUNT, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
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

void gfxgl3_video_toggle_filter(void) {
    if (min_filter == GL_NEAREST)
        min_filter = GL_LINEAR;
    else
        min_filter = GL_NEAREST;

    if (mag_filter == GL_NEAREST)
        mag_filter = GL_LINEAR;
    else
        mag_filter = GL_NEAREST;
}

int gfxgl3_video_get_fb(int *obj_handle_out, unsigned *width_out,
                        unsigned *height_out, bool *flip_out) {
    if (bound_obj_handle < 0)
        return -1;
    *obj_handle_out = bound_obj_handle;
    *width_out = gfxgl3_renderer_tex_get_width(bound_obj_handle);
    *height_out = gfxgl3_renderer_tex_get_height(bound_obj_handle);
    *flip_out = do_flip;
    return 0;
}
