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

void title_set_content(char const *new_content) {
    strncpy(content, new_content, sizeof(content));
    content[CONTENT_LEN - 1] = '\0';
}

// return the window title
char const *title_get(void) {
    static char title[TITLE_LEN];

    if (strlen(content))
        snprintf(title, TITLE_LEN, "WashingtonDC - %s", content);
    else
        strncpy(title, "WashingtonDC", TITLE_LEN);

    title[TITLE_LEN - 1] = '\0';

    // trim trailing whitespace
    int idx;
    for (idx = strlen(title) - 1; (idx >= 0) && isspace(title[idx]); idx--)
        title[idx] = '\0';

    return title;
}
