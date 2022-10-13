/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2022 snickerbockers
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

#define OIT_NODE_INVALID 0xffffffff

struct oit_pixel {
    vec4 color;
    float depth;
    unsigned int src_blend_factor, dst_blend_factor;
};

struct oit_node {
    struct oit_pixel pix;
    unsigned int next_node;
};

layout (binding = 0) uniform atomic_uint node_count;
layout(std430, binding = 0) coherent buffer oit_shared_data {
    oit_node oit_nodes[];
};
