/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2019, 2020, 2022 snickerbockers
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
#include <time.h>
#include <string.h>
#include <stdint.h>

#include "washdc/hostfile.h"
#include "washdc/gfx/gfx_il.h"
#include "log.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

static int do_save_screenshot(washdc_hostfile stream, char const *path);
static int grab_screen(uint32_t **fb_out, unsigned *fb_width_out,
                       unsigned *fb_height_out, bool *do_flip_out);

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

    static char filename[PATH_LEN];
    snprintf(filename, PATH_LEN, "%s.png", timestr);
    washdc_hostfile stream;
    int idx;
    for (idx = 0; idx < 16; idx++) {
        stream =
            washdc_hostfile_open_screenshot(filename, WASHDC_HOSTFILE_WRITE |
                                            WASHDC_HOSTFILE_BINARY |
                                            WASHDC_HOSTFILE_DONT_OVERWRITE);
        if (stream != WASHDC_HOSTFILE_INVALID)
            break;
        else
            snprintf(filename, PATH_LEN, "%s_%d.png", timestr, idx);
    }
    if (stream == WASHDC_HOSTFILE_INVALID)
        return -1;

    int ret = do_save_screenshot(stream, filename);
    washdc_hostfile_close(stream);
    return ret;
}

static void write_png_callback(void *ctxt, void *data, int size) {
    washdc_hostfile fp = (washdc_hostfile)ctxt;
    washdc_hostfile_write(fp, data, size);
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

    unsigned char *img_data = malloc(fb_width * fb_height * 3);
    if (!img_data) {
        err_val = -1;
        goto cleanup_png;
    }

    unsigned row, col;
    for (row = 0; row < fb_height; row++) {
        for (col = 0; col < fb_width; col++) {
            unsigned char *outp = img_data + (row * fb_width + col) * 3;
            unsigned pix_idx;
            if (!do_flip)
                pix_idx = (fb_height - 1 - row) * fb_width + col;
            else
                pix_idx = row * fb_width + col;
            uint32_t in_px = fb_tmp[pix_idx];
            unsigned red = in_px & 0xff;
            unsigned green = (in_px >> 8) & 0xff;
            unsigned blue = (in_px >> 16) & 0xff;
            outp[0] = (unsigned char)red;
            outp[1] = (unsigned char)green;
            outp[2] = (unsigned char)blue;
        }
    }

    err_val = stbi_write_png_to_func(write_png_callback, stream, fb_width,
                                     fb_height, 3, img_data, 3 * fb_width);

    free(img_data);
 cleanup_png:
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
