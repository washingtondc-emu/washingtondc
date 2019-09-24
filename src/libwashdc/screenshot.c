/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2019 snickerbockers
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
#include <stdbool.h>
#include <png.h>
#include <time.h>
#include <string.h>

#include "washdc/hostfile.h"
#include "gfx/gfx_il.h"
#include "log.h"

static int do_save_screenshot(washdc_hostfile stream, char const *path);
static int grab_screen(uint32_t **fb_out, unsigned *fb_width_out,
                       unsigned *fb_height_out, bool *do_flip_out);

static void write_wrapper_png(png_structp png, png_bytep dat, png_size_t len);
static void flush_wrapper_png(png_structp png);

int save_screenshot(char const *path) {
    washdc_hostfile stream =
        washdc_hostfile_open(path,
                             WASHDC_HOSTFILE_WRITE | WASHDC_HOSTFILE_BINARY);
    if (stream == WASHDC_HOSTFILE_INVALID)
        return -1;
    int ret = do_save_screenshot(stream, path);
    washdc_hostfile_close(stream);
    return ret;
}

#define TIMESTR_LEN 32
#define PATH_LEN 1024

int save_screenshot_dir(void) {
    time_t rawtime;
    struct tm *timeinfo;
    char timestr[TIMESTR_LEN];

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(timestr, TIMESTR_LEN, "%F-%H-%M-%S", timeinfo);
    timestr[TIMESTR_LEN - 1] = '\0';

    char const *screenshot_dir = washdc_hostfile_screenshot_dir();
    static char filename[PATH_LEN];
    static char path[PATH_LEN];
    snprintf(filename, PATH_LEN, "%s.png", timestr);
    washdc_hostfile stream;
    int idx;
    for (idx = 0; idx < 16; idx++) {
        strncpy(path, screenshot_dir, PATH_LEN);
        path[PATH_LEN - 1] = '\0';
        washdc_hostfile_path_append(path, filename, PATH_LEN);
        stream =
            washdc_hostfile_open(path, WASHDC_HOSTFILE_WRITE |
                                 WASHDC_HOSTFILE_BINARY |
                                 WASHDC_HOSTFILE_DONT_OVERWRITE);
        if (stream != WASHDC_HOSTFILE_INVALID)
            break;
        else
            snprintf(filename, PATH_LEN, "%s_%d.png", timestr, idx);
    }
    if (stream == WASHDC_HOSTFILE_INVALID)
        return -1;

    int ret = do_save_screenshot(stream, path);
    washdc_hostfile_close(stream);
    return ret;
}

static int do_save_screenshot(washdc_hostfile stream, char const *path) {
    int err_val = 0;
    uint32_t *fb_tmp = NULL;
    unsigned fb_width, fb_height;
    bool do_flip;

    if (grab_screen(&fb_tmp, &fb_width, &fb_height, &do_flip) < 0) {
        LOG_ERROR("%s - Failed to capture screenshot\n", __func__);
        return -1;
    }

    if (!fb_tmp) {
        LOG_WARN("Unable to save screenshot to %s due to failure to obtain "
                 "screengrab\n", path);
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

    png_set_write_fn(png_ptr, stream, write_wrapper_png, flush_wrapper_png);

    png_bytepp row_pointers = (png_bytepp)calloc(fb_height, sizeof(png_bytep));
    if (!row_pointers)
        goto cleanup_png;

    unsigned row, col;
    for (row = 0; row < fb_height; row++) {
        png_bytep cur_row = row_pointers[row] =
            (png_bytep)malloc(sizeof(png_byte) * fb_width * 3);

        for (col = 0; col < fb_width; col++) {
            unsigned pix_idx;
            if (!do_flip)
                pix_idx = (fb_height - 1 - row) * fb_width + col;
            else
                pix_idx = row * fb_width + col;
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
 free_screengrab:
    free(fb_tmp);
    return err_val;
}

static int grab_screen(uint32_t **fb_out, unsigned *fb_width_out,
                       unsigned *fb_height_out, bool *do_flip_out) {
    struct gfx_framebuffer fb;
    struct gfx_il_inst cmd = {
        .op = GFX_IL_GRAB_FRAMEBUFFER,
        .arg = {
            .grab_framebuffer = {
                .fb = &fb
            }
        }
    };
    rend_exec_il(&cmd, 1);

    if (fb.valid) {
        *fb_out = fb.dat;
        *fb_width_out = fb.width;
        *fb_height_out = fb.height;
        *do_flip_out = fb.flip;
        return 0;
    }

    return -1;
}

static void write_wrapper_png(png_structp png, png_bytep dat, png_size_t len) {
    washdc_hostfile fp = (washdc_hostfile)png_get_io_ptr(png);
    washdc_hostfile_write(fp, dat, len);
}

static void flush_wrapper_png(png_structp png) {
    washdc_hostfile fp = (washdc_hostfile)png_get_io_ptr(png);
    washdc_hostfile_flush(fp);
}
