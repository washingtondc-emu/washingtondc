/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2018 snickerbockers
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
#include <errno.h>
#include <err.h>
#include <stdbool.h>
#include <png.h> // for saving screenshots to PNG files

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#include <GL/gl.h>

#include "glfw/window.h"
#include "dreamcast.h"
#include "gfx/opengl/opengl_output.h"
#include "gfx/opengl/opengl_target.h"
#include "gfx/opengl/font/font.h"
#include "gfx/rend_common.h"
#include "gfx/gfx_tex_cache.h"
#include "log.h"
#include "config.h"

// for the palette_tp stuff
#include "hw/pvr2/pvr2_core_reg.h"

#include "gfx/gfx.h"

static unsigned win_width, win_height;

static unsigned frame_counter;

// Only call gfx_thread_signal and gfx_thread_wait when you hold the lock.
static void gfx_do_init(void);

static void gfx_auto_screenshot(void);

void gfx_init(unsigned width, unsigned height) {
    win_width = width;
    win_height = height;

    LOG_INFO("GFX: rendering graphics from within the main emulation thread\n");
    gfx_do_init();
}

void gfx_cleanup(void) {
    font_cleanup();
}

void gfx_expose(void) {
    opengl_video_present();
    win_update();
}

static void gfx_do_init(void) {
    win_make_context_current();

    glewExperimental = GL_TRUE;
    glewInit();
    glViewport(0, 0, win_width, win_height);

    opengl_target_init();
    opengl_video_output_init();
    gfx_tex_cache_init();
    rend_init();

    font_init();

    glClear(GL_COLOR_BUFFER_BIT);
}

void gfx_read_framebuffer(void *dat, unsigned n_bytes) {
    opengl_target_grab_pixels(dat, n_bytes);
}

static uint32_t *fb_screengrab;
static size_t fb_screengrab_w, fb_screengrab_h;

void gfx_post_framebuffer(uint32_t const *fb_new,
                          unsigned fb_new_width,
                          unsigned fb_new_height) {
    opengl_video_new_framebuffer(fb_new, fb_new_width, fb_new_height);

    // save a copy of fb_new for screengrabs
    if ((fb_new_width * fb_new_height) != (fb_screengrab_w * fb_screengrab_h)) {
        fb_screengrab_w = fb_new_width;
        fb_screengrab_h = fb_new_height;
        size_t n_bytes = fb_screengrab_w * fb_screengrab_h * 4;
        void *fb_tmp = realloc(fb_screengrab, n_bytes);
        if (!fb_tmp) {
            LOG_WARN("Unable to preserve screengrab: failed reallocation of "
                     "%llu bytes\n", (unsigned long long)n_bytes);
            fb_screengrab_w = 0;
            fb_screengrab_h = 0;
            free(fb_screengrab);
            fb_screengrab = NULL;
            return;
        }
        fb_screengrab = fb_tmp;
    }

    memcpy(fb_screengrab, fb_new, fb_screengrab_w * fb_screengrab_h * 4);
    if (config_get_enable_auto_screenshot())
        gfx_auto_screenshot();
    frame_counter++;
}

/*
 * XXX The way I've implemented screenshots is a little suboptimal.  Instead of
 * copying the framebuffer back from the GPU when the user requests a
 * screenshot, I make a copy of the framebuffer on the CPU-side every time a
 * new framebuffer is posted.  Then it makes another copy when it's actually
 * saving the PNG file.  I don't expect this to impact performance in a
 * noticeable way, but if it does I can always go back and change it later.
 *
 * I should also probably save the screenshots from the IO thread instead of
 * doing it in the gfx code.
 */

void gfx_grab_screen(uint32_t **fb_out, unsigned *fb_width_out,
                     unsigned *fb_height_out) {
    if (!fb_screengrab) {
        *fb_out = NULL;
        *fb_width_out = 0;
        *fb_height_out = 0;
        return;
    }

    size_t n_words = fb_screengrab_w * fb_screengrab_h;
    uint32_t *fb = (uint32_t*)malloc(n_words * sizeof(uint32_t));

    memcpy(fb, fb_screengrab, n_words * sizeof(uint32_t));

    *fb_out = fb;
    *fb_width_out = fb_screengrab_w;
    *fb_height_out = fb_screengrab_h;
}

int gfx_save_screenshot(char const *path) {
    int err_val = 0;
    uint32_t *fb_tmp;
    unsigned fb_width, fb_height;

    gfx_grab_screen(&fb_tmp, &fb_width, &fb_height);

    if (!fb_tmp) {
        LOG_WARN("Unable to save screenshot to %s due to failure to obtain "
                 "screengrab\n", path);
        err_val = -1;
        goto free_screengrab;
    }

    FILE *stream = fopen(path, "wb");

    if (!stream) {
        err_val = -1;
        goto free_screengrab;
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                                  NULL, NULL, NULL);
    if (!png_ptr) {
        err_val = -1;
        goto close_file;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);

    if (!info_ptr) {
        err_val = -1;
        goto cleanup_png;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        err_val = -1;
        goto cleanup_png;
    }

    png_init_io(png_ptr, stream);

    png_bytepp row_pointers = (png_bytepp)calloc(fb_height, sizeof(png_bytep));
    if (!row_pointers)
        goto cleanup_png;

    unsigned row, col;
    for (row = 0; row < fb_height; row++) {
        png_bytep cur_row = row_pointers[row] =
            (png_bytep)malloc(sizeof(png_byte) * fb_width * 3);

        for (col = 0; col < fb_width; col++) {
            unsigned pix_idx = row * fb_width + col;
            uint32_t in_px = fb_tmp[pix_idx];
            unsigned red = in_px & 0xff;
            unsigned green = (in_px >> 8) & 0xff;
            unsigned blue = (in_px >> 16) & 0xff;
            cur_row[col * 3] = (png_byte)red;
            cur_row[col * 3 + 1] = (png_byte)green;
            cur_row[col * 3 + 2] = (png_byte)blue;
        }
    }

    png_set_IHDR(png_ptr, info_ptr, fb_width, fb_height, 8,
                 PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);
    png_write_image(png_ptr, row_pointers);
    png_write_end(png_ptr, info_ptr);

    for (row = 0; row < fb_height; row++)
        free(row_pointers[row]);
    free(row_pointers);
 cleanup_png:
    png_destroy_write_struct(&png_ptr, &info_ptr);
 close_file:
    fclose(stream);
 free_screengrab:
    free(fb_tmp);
    return err_val;
}

#define AUTO_SCREEN_PATH_MAX 128
static void gfx_auto_screenshot(void) {
    static char path[AUTO_SCREEN_PATH_MAX];

    memset(path, 0, sizeof(path));

    snprintf(path, AUTO_SCREEN_PATH_MAX, "%s/frame_%u.png",
             config_get_auto_screenshot_dir(), frame_counter);

    path[AUTO_SCREEN_PATH_MAX - 1] = '\0';

    LOG_INFO("saving a screenshot to \"%s\"\n", path);
    gfx_save_screenshot(path);
}
