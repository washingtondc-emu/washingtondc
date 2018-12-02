/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018 snickerbockers
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
#include <string.h>
#include <ctype.h>

#include "title.h"

#define TITLE_LEN 128

#define CONTENT_LEN 64

static char content[CONTENT_LEN];
static unsigned xres, yres;
static double fps_internal;

void title_set_content(char const *new_content) {
    strncpy(content, new_content, sizeof(content));
    content[CONTENT_LEN - 1] = '\0';

    // trim trailing whitespace
    int idx;
    for (idx = strlen(content) - 1; (idx >= 0) && isspace(content[idx]); idx--)
        content[idx] = '\0';
}

void title_set_resolution(unsigned width, unsigned height) {
    xres = width;
    yres = height;
}

void title_set_fps_internal(double fps) {
    fps_internal = fps;
}

// return the window title
char const *title_get(void) {
    static char title[TITLE_LEN];

    if (strlen(content)) {
        snprintf(title, TITLE_LEN, "WashingtonDC - %s (%ux%u, %.2f Hz)",
                 content, xres, yres, fps_internal);
    } else {
        snprintf(title, TITLE_LEN, "WashingtonDC (%ux%u, %.2f Hz)",
                 xres, yres, fps_internal);
    }

    title[TITLE_LEN - 1] = '\0';

    return title;
}
