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

#ifndef GEO_BUF_H_
#define GEO_BUF_H_

/*
 * a geo_buf is a pre-allocated buffer used to pass data from the emulation
 * thread to the gfx_thread.  They are stored in a ringbuffer in which the
 * emulation code produces and the rendering code consumes.  Currently this code
 * supports only triangles, but it will eventually grow to encapsulate
 * everything.
 */

/*
 * max number of triangles for a single geo_buf.  Maybe it doesn't need to be
 * this big, or maybe it isn't big enough.  Who is John Galt?
 */
#define GEO_BUF_TRIANGLE_COUNT 131072
#define GEO_BUF_VERT_COUNT (GEO_BUF_TRIANGLE_COUNT * 3)

struct geo_buf {
    float verts[GEO_BUF_VERT_COUNT*3];
    unsigned n_verts;
    unsigned frame_stamp;
};

/*
 * return the next geo_buf to be consumed, or NULL if there are none.
 * This function never blocks.
 */
struct geo_buf *geo_buf_get_cons(void);

/*
 * return the next geo_buf to be produced.  This function never returns NULL.
 */
struct geo_buf *geo_buf_get_prod(void);

// consume the current geo_buf (which is the one returned by geo_buf_get_cons)
void geo_buf_consume(void);

// mark the current geo_buf as having been consumed
void geo_buf_produce(void);

// return the frame stamp of the last geo_buf to be produced
unsigned geo_buf_latest_frame_stamp(void);

#endif
