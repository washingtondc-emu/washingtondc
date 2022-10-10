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

out vec4 out_color;

layout(r32ui, binding = 0) uniform coherent uimage2D oit_heads;
uniform sampler2D color_accum;

vec4 eval_src_blend_factor(unsigned int factor, vec4 src, vec4 dst) {
    switch (factor) {
    default:
    case 0:
        return vec4(0, 0, 0, 0);
    case 1:
        return vec4(1, 1, 1, 1);
    case 2:
        return dst;
    case 3:
        return vec4(1.0 - dst.r, 1.0 - dst.g, 1.0 - dst.b, 1.0 - dst.a);
    case 4:
        return vec4(src.a, src.a, src.a, src.a);
    case 5:
        return vec4(1.0 - src.a, 1.0 - src.a, 1.0 - src.a, 1.0 - src.a);
    case 6:
        return vec4(dst.a, dst.a, dst.a, dst.a);
    case 7:
        return vec4(1.0 - dst.a, 1.0 - dst.a, 1.0 - dst.a, 1.0 - dst.a);
    }
}

vec4 eval_dst_blend_factor(unsigned int factor, vec4 src, vec4 dst) {
    switch (factor) {
    default:
    case 0:
        return vec4(0, 0, 0, 0);
    case 1:
        return vec4(1, 1, 1, 1);
    case 2:
        return src;
    case 3:
        return vec4(1.0 - src.r, 1.0 - src.g, 1.0 - src.b, 1.0 - src.a);
    case 4:
        return vec4(src.a, src.a, src.a, src.a);
    case 5:
        return vec4(1.0 - src.a, 1.0 - src.a, 1.0 - src.a, 1.0 - src.a);
    case 6:
        return vec4(dst.a, dst.a, dst.a, dst.a);
    case 7:
        return vec4(1.0 - dst.a, 1.0 - dst.a, 1.0 - dst.a, 1.0 - dst.a);
    }
}

void swap_oit_nodes(uint node1, uint node2) {
    oit_pixel pix_tmp = oit_nodes[node1].pix;
    oit_nodes[node1].pix = oit_nodes[node2].pix;
    oit_nodes[node2].pix = pix_tmp;
}


// it's my favorite algorithm, the insertion sort!
void sort_list(unsigned int index) {
    unsigned int src_idx = index;
    while (src_idx != OIT_NODE_INVALID) {
        unsigned int cmp_idx = oit_nodes[src_idx].next_node;
        while (cmp_idx != OIT_NODE_INVALID) {
            if (oit_nodes[cmp_idx].pix.depth <= oit_nodes[src_idx].pix.depth)
                swap_oit_nodes(src_idx, cmp_idx);
            cmp_idx = oit_nodes[cmp_idx].next_node;
        }
        src_idx = oit_nodes[src_idx].next_node;
    }
}

void main() {
    unsigned int head = imageLoad(oit_heads, ivec2(gl_FragCoord.xy))[0];
    sort_list(head);

    unsigned int cur_node = head;
    vec4 color = texture(color_accum, gl_FragCoord.xy / textureSize(color_accum, 0));

/*
 * XXX this is actually wrong because gl_FragCoord.z samples from  the
 * fullscreen quad, *not* the original fragment value.  The reason why that
 * doesn't matter is because we discard when there are no transparent pixels
 * to render, which will preserve the original depth value.
 */
    float depth = gl_FragCoord.z;

    // skip fragments that have no transparent pixels to render
    if (cur_node == OIT_NODE_INVALID)
        discard;

    while (cur_node != OIT_NODE_INVALID) {
        oit_pixel pix_in = oit_nodes[cur_node].pix;
        vec4 color_in = pix_in.color;
        vec4 src_factor = eval_src_blend_factor(pix_in.src_blend_factor, color_in, color);
        vec4 dst_factor = eval_dst_blend_factor(pix_in.dst_blend_factor, color_in, color);
        color = clamp(src_factor * color_in + dst_factor * color, 0, 1);
        depth = oit_nodes[cur_node].pix.depth;
        cur_node = oit_nodes[cur_node].next_node;
    }
    gl_FragDepth = depth;
    out_color = color;
}
