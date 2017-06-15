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

#include <stdio.h>

#include "hw/pvr2/geo_buf.h"

#include "opengl_renderer.h"

void render_next_geo_buf(void) {
    struct geo_buf *geo = geo_buf_get_cons();

    if (!geo) {
        printf("%s - erm...there's nothing to render here?\n", __func__);
        return;
    }

    printf("Vertex dump:\n");

    unsigned vert_no;
    for (vert_no = 0; vert_no < geo->n_verts; vert_no++) {
        float const *vertp = geo->verts + 3 * vert_no;
        printf("\t(%f, %f, %f)\n", vertp[1], vertp[2], vertp[3]);
    }

    geo_buf_consume();
}
