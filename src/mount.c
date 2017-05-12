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

#include <string.h>

#include "error.h"

#include "mount.h"

static bool mounted;
static struct mount img;

void mount_insert(struct mount_ops const *ops, void *ptr) {
    if (img.state)
        mount_eject();

    img.ops = ops;
    img.state = ptr;
    mounted = true;
}

void mount_eject(void) {
    if (img.ops->cleanup)
        img.ops->cleanup(&img);

    memset(&img, 0, sizeof(img));
    mounted = false;
}

bool mount_check(void) {
    return mounted;
}

unsigned mount_session_count(void) {
    if (mounted) {
        if (img.ops->session_count)
            return img.ops->session_count(&img);
        return 0;
    }

    error_set_wtf("calling mount_session_count when there's nothing mounted");
    RAISE_ERROR(ERROR_INTEGRITY);
}

int mount_read_toc(struct mount_toc* out, unsigned session) {
    if (mounted) {
        if (session < mount_session_count() && img.ops->read_toc)
            return img.ops->read_toc(&img, out, session);
        return -1;
    } else {
        error_set_wtf("calling mount_read_toc when there's nothing mounted");
        RAISE_ERROR(ERROR_INTEGRITY);
    }
}
