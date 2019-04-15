/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018, 2019 snickerbockers
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

#include <cstring>
#include <cstdio>

#include <GL/gl.h>

#ifdef NOTYET
#include "dreamcast.h"
#include "hw/pvr2/pvr2.h"
#endif
#include "font/font.hpp"
#include "washdc/win.h"
#include "washdc/gfx/gl/shader.h"
#include "washdc/washdc.h"

#include "overlay.hpp"

static double framerate, virt_framerate;
static bool not_hidden;

static struct shader ui_shader;

static void shader_init(void);

void overlay_show(bool do_show) {
    not_hidden = do_show;
}

void overlay_draw(void) {
    if (not_hidden) {
        unsigned screen_width = win_get_width();
        unsigned screen_height = win_get_height();

        char tmp[64];
        snprintf(tmp, sizeof(tmp), "%.2f / %.2f", framerate, virt_framerate);
        tmp[63] = '\0';

        font_render(tmp, 0, 0, screen_width, screen_height);

        struct washdc_pvr2_stat stat;
        washdc_get_pvr2_stat(&stat);

        /*
         * TODO: put in list names when we have a font that can display text
         * characters
         */
        snprintf(tmp, sizeof(tmp), "%u",
                 stat.poly_count[WASHDC_PVR2_POLY_GROUP_OPAQUE]);
        tmp[63] = '\0';
        font_render(tmp, 0, 1, screen_width, screen_height);

        snprintf(tmp, sizeof(tmp), "%u",
                 stat.poly_count[WASHDC_PVR2_POLY_GROUP_OPAQUE_MOD]);
        tmp[63] = '\0';
        font_render(tmp, 0, 2, screen_width, screen_height);

        snprintf(tmp, sizeof(tmp), "%u",
                 stat.poly_count[WASHDC_PVR2_POLY_GROUP_TRANS]);
        tmp[63] = '\0';
        font_render(tmp, 0, 3, screen_width, screen_height);

        snprintf(tmp, sizeof(tmp), "%u",
                 stat.poly_count[WASHDC_PVR2_POLY_GROUP_TRANS_MOD]);
        tmp[63] = '\0';
        font_render(tmp, 0, 4, screen_width, screen_height);

        snprintf(tmp, sizeof(tmp), "%u",
                 stat.poly_count[WASHDC_PVR2_POLY_GROUP_PUNCH_THROUGH]);
        tmp[63] = '\0';
        font_render(tmp, 0, 5, screen_width, screen_height);
    }
}

void overlay_set_fps(double fps) {
    framerate = fps;
}

void overlay_set_virt_fps(double fps) {
    virt_framerate = fps;
}

void overlay_init(void) {
    font_init();
}

void overlay_cleanup(void) {
    font_cleanup();
}

static void shader_init(void) {
    static char const * const final_vert_glsl =
        "#extension GL_ARB_explicit_uniform_location : enable\n"

        "layout (location = 0) in vec3 vert_pos;\n"
        "layout (location = 1) in vec2 tex_coord;\n"
        "layout (location = 2) uniform mat4 trans_mat;\n"
        "layout (location = 3) uniform mat3 tex_mat;\n"

        "out vec2 st;\n"

        "void main() {\n"
        "    gl_Position = trans_mat * vec4(vert_pos.x, vert_pos.y, vert_pos.z, 1.0);\n"
        "    st = (tex_mat * vec3(tex_coord.x, tex_coord.y, 1.0)).xy;\n"
        "}\n";

    static char const * const final_frag_glsl =
        "in vec2 st;\n"
        "out vec4 color;\n"

        "uniform sampler2D fb_tex;\n"

        "void main() {\n"
        "    vec4 sample = texture(fb_tex, st);\n"
        "    color = sample;\n"
        "}\n";

    shader_load_vert(&ui_shader, final_vert_glsl);
    shader_load_frag(&ui_shader, final_frag_glsl);
    shader_link(&ui_shader);
}
