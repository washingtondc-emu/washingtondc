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
#define TEX_INST_DECAL 0
#define TEX_INST_MOD 1
#define TEX_INST_DECAL_ALPHA 2
#define TEX_INST_MOD_ALPHA 3

in vec4 vert_base_color, vert_offs_color;
out vec4 out_color;

in float w_coord;

#ifdef TEX_ENABLE
in vec2 st;
uniform sampler2D bound_tex;
#endif

#ifdef USER_CLIP_ENABLE
/*
 * user_clip.x - x_min
 * user_clip.y - y_min
 * user_clip.z - x_max
 * user_clip.w - y_max
 */
uniform vec4 user_clip;

void user_clip_test() {
    bool in_rect = gl_FragCoord.x >= user_clip[0] &&
        gl_FragCoord.x <= user_clip[2] &&
        gl_FragCoord.y >= user_clip[1] &&
        gl_FragCoord.y <= user_clip[3];
#ifdef USER_CLIP_INVERT
    if (in_rect)
        discard;
#else
    if (!in_rect)
        discard;
#endif
}
#endif

#ifdef PUNCH_THROUGH_ENABLE
uniform int pt_alpha_ref;

void punch_through_test(float alpha) {
    if (int(alpha * 255) < pt_alpha_ref)
        discard;
}
#endif

#ifdef TEX_ENABLE
vec4 eval_tex_inst() {
    /*
     * division by w_coord makes it perspective-correct when combined
     * with multiplication by vert_pos.z in the vertex shader.
     */
    vec4 base_color = vert_base_color / w_coord;
    vec4 offs_color = vert_offs_color / w_coord;
    vec4 tex_color = texture(bound_tex, st / w_coord);
    vec4 color;
// TODO: is the offset alpha color supposed to be used for anything?
#if TEX_INST == TEX_INST_DECAL
        color.rgb = tex_color.rgb + offs_color.rgb;
        color.a = tex_color.a;
#elif TEX_INST == TEX_INST_MOD
        color.rgb = tex_color.rgb * base_color.rgb + offs_color.rgb;
        color.a = tex_color.a;
#elif TEX_INST == TEX_INST_DECAL_ALPHA
        color.rgb = tex_color.rgb * tex_color.a +
            base_color.rgb * (1.0 - tex_color.a) + offs_color.rgb;
        color.a = base_color.a;
#elif TEX_INST == TEX_INST_MOD_ALPHA
        color.rgb = tex_color.rgb * base_color.rgb + offs_color.rgb;
        color.a = tex_color.a * base_color.a;
#else
#error unknown TEX_INST
#endif
    return color;
}
#endif

void main() {

#ifdef USER_CLIP_ENABLE
    user_clip_test();
#endif

    vec4 color;
#ifdef TEX_ENABLE
    color = eval_tex_inst();
#else
    // divide by w_coord for perspective correction
    color = vert_base_color / w_coord;
#endif

#ifdef PUNCH_THROUGH_ENABLE
    punch_through_test(color.a);
#endif

    out_color = color;
}