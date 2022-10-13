#version 430

/*******************************************************************************
 *
 *
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


layout(early_fragment_tests) in;
uniform int MAX_OIT_NODES;
uniform int src_blend_factor, dst_blend_factor;

/*
  * BEGINNING OF COPY/PASTED SEGMENT
  * i need this to be in both gfxgl4_oit_first_pass.frag and
  * oit_sort.frag, however glsl lacks a #include statement which
  * necessitates that I copy/paste the code between these two files.
  *
  * i could paste the two strings together at run-time and that is
  * what WashingtonDC used to do, but then I wouldn't be able to
  * validate the shaders with glslangValidator.
  *
  * needless to say, the versions of this in
  * gfxgl4_oit_first_pass.frag and oit_sort.frag need to be consistent
  * with each other.
  */
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
// END OF COPY/PASTED SEGMENT

/*
 * each pixel in oit_heads >= 0 points to an index in oit_node that is the
 * beginning of that pixel's linked-list.
 */
layout(r32ui, binding = 0) uniform coherent uimage2D oit_heads;

void add_pixel_to_oit_list(vec4 color, float depth) {
    unsigned int node_idx = atomicCounterIncrement(node_count);
    if (node_idx < MAX_OIT_NODES) {
        oit_nodes[node_idx].pix.color = color;
        oit_nodes[node_idx].pix.depth = depth;
        oit_nodes[node_idx].pix.src_blend_factor = src_blend_factor;
        oit_nodes[node_idx].pix.dst_blend_factor = dst_blend_factor;

        oit_nodes[node_idx].next_node =
            imageAtomicExchange(oit_heads, ivec2(gl_FragCoord.xy), node_idx);
    }
}
