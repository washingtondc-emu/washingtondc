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
#include <memory>

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#include <GL/gl.h>

#include "washdc/win.h"
#include "washdc/gfx/gl/shader.h"
#include "washdc/washdc.h"
#include "imgui.h"
#include "renderer.hpp"
#include "../window.hpp"

#include "overlay.hpp"

static double framerate, virt_framerate;
static bool not_hidden;
static bool en_perf_win = true;

std::unique_ptr<renderer> ui_renderer;

namespace overlay {
static void show_perf_win(void);
}

void overlay::show(bool do_show) {
    not_hidden = do_show;
}

void overlay::draw() {
    if (!not_hidden)
        return;

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)win_glfw_get_width(),
                            (float)win_glfw_get_height());

    ImGui::NewFrame();

    // main menu bar
    if (ImGui::BeginMainMenuBar()) {

        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Quit", "Ctrl+Q"))
                washdc_kill();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window")) {
            ImGui::Checkbox("Performance", &en_perf_win);
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    // Performance Window
    if (en_perf_win)
        show_perf_win();

    ImGui::Render();
    ui_renderer->do_render(ImGui::GetDrawData());
}

static void overlay::show_perf_win(void) {
    struct washdc_pvr2_stat stat;
    washdc_get_pvr2_stat(&stat);

    ImGui::Begin("Performance", &en_perf_win);
    ImGui::Text("Framerate: %.2f / %.2f", framerate, virt_framerate);
    ImGui::Text("%u opaque polygons",
                stat.poly_count[WASHDC_PVR2_POLY_GROUP_OPAQUE]);
    ImGui::Text("%u opaque modifier polygons",
                stat.poly_count[WASHDC_PVR2_POLY_GROUP_OPAQUE_MOD]);
    ImGui::Text("%u transparent polygons",
                stat.poly_count[WASHDC_PVR2_POLY_GROUP_TRANS]);
    ImGui::Text("%u transparent modifier polygons",
                stat.poly_count[WASHDC_PVR2_POLY_GROUP_TRANS_MOD]);
    ImGui::Text("%u punch-through polygons",
                stat.poly_count[WASHDC_PVR2_POLY_GROUP_PUNCH_THROUGH]);
    ImGui::End();
}

void overlay::set_fps(double fps) {
    framerate = fps;
}

void overlay::set_virt_fps(double fps) {
    virt_framerate = fps;
}

void overlay::init() {
    en_perf_win = true;
    not_hidden = false;

    ImGui::CreateContext();

    ui_renderer = std::make_unique<renderer>();
}

void overlay::cleanup() {
    ui_renderer.reset();

    ImGui::DestroyContext();
}

void overlay::update() {
    ui_renderer->update();
}
