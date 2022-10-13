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

// OIT_NODE_GLSL_DEF

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
