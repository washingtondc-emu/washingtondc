/*******************************************************************************
 *
 * Copyright 2018 snickerbockers
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

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "title.h"

#define TITLE_LEN 128
#define CONTENT_LEN 64
#define PIX_FMT_LEN 16

static char content[CONTENT_LEN];
static char pix_fmt[PIX_FMT_LEN];
static unsigned xres, yres;
static double fps_internal;
static bool interlaced;

void title_set_content(char const *new_content) {
    if (new_content) {
        strncpy(content, new_content, sizeof(content));
        content[CONTENT_LEN - 1] = '\0';

        // trim trailing whitespace
        int idx;
        for (idx = strlen(content) - 1; (idx >= 0) && isspace(content[idx]); idx--)
            content[idx] = '\0';
    } else {
        memset(content, 0, sizeof(content));
    }
}

void title_set_resolution(unsigned width, unsigned height) {
    xres = width;
    yres = height;
}

void title_set_fps_internal(double fps) {
    fps_internal = fps;
}

void title_set_pix_fmt(char const *fmt) {
    strncpy(pix_fmt, fmt, sizeof(pix_fmt));
    pix_fmt[PIX_FMT_LEN - 1] = '\0';
}

void title_set_interlace(bool intl) {
    interlaced = intl;
}

// return the window title
char const *title_get(void) {
    static char title[TITLE_LEN];

    if (strlen(content)) {
        snprintf(title, TITLE_LEN, "WashingtonDC - %s (%ux%u%c %s, %.2f Hz)",
                 content, xres, yres, interlaced ? 'i' : 'p', pix_fmt, fps_internal);
    } else {
        snprintf(title, TITLE_LEN, "WashingtonDC (%ux%u%c %s, %.2f Hz)",
                 xres, yres, interlaced ? 'i' : 'p', pix_fmt, fps_internal);
    }

    title[TITLE_LEN - 1] = '\0';

    return title;
}
