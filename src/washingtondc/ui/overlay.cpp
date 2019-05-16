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
#include <sstream>

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#include <GL/gl.h>

#include "washdc/win.h"
#include "washdc/gfx/gl/shader.h"
#include "washdc/washdc.h"
#include "washdc/gameconsole.h"
#include "imgui.h"
#include "renderer.hpp"
#include "../window.hpp"
#include "../sound.hpp"
#include "../washingtondc.hpp"

#include "overlay.hpp"

// see main.cpp
extern struct washdc_gameconsole *console;

static double framerate, virt_framerate;
static bool not_hidden;
static bool en_perf_win = true;
static bool en_demo_win = false;
static bool en_aica_win = true;
static bool show_nonplaying_channels = true;
static bool have_debugger;

static unsigned n_chans;
static bool *sndchan_mute;

static std::unique_ptr<renderer> ui_renderer;

namespace overlay {
static void show_perf_win(void);
static void show_aica_win(void);
static std::string var_as_str(struct washdc_var const *var);
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

    bool mute_old = sound::is_muted();
    bool do_mute_audio = mute_old;

    // main menu bar
    if (ImGui::BeginMainMenuBar()) {

        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Quit", "Ctrl+Q"))
                washdc_kill();
            ImGui::EndMenu();
        }

        if (!have_debugger && ImGui::BeginMenu("Execution")) {
            if (washdc_is_paused()) {
                if (ImGui::MenuItem("Resume"))
                    do_resume();
                if (ImGui::MenuItem("Run one frame"))
                    do_run_one_frame();
            } else {
                if (ImGui::MenuItem("Pause"))
                    do_pause();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Audio")) {
            ImGui::Checkbox("mute", &do_mute_audio);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window")) {
            ImGui::Checkbox("Performance", &en_perf_win);
            ImGui::Checkbox("AICA", &en_aica_win);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("About")) {
            ImGui::Checkbox("ImGui demo window", &en_demo_win);
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    // Performance Window
    if (en_perf_win)
        show_perf_win();

    if (en_demo_win)
        ImGui::ShowDemoWindow(&en_demo_win);
    if (en_aica_win)
        show_aica_win();

    if (mute_old != do_mute_audio)
        sound::mute(do_mute_audio);

    ImGui::Render();
    ui_renderer->do_render(ImGui::GetDrawData());
}

static void overlay::show_perf_win(void) {
    struct washdc_pvr2_stat stat;
    washdc_get_pvr2_stat(&stat);

    ImGui::Begin("Performance", &en_perf_win);
    ImGui::Text("Framerate: %.2f / %.2f (%.2f%%)", framerate, virt_framerate, 100.0 * (framerate / virt_framerate));
    ImGui::Text("%u frames rendered\n", washdc_get_frame_count());
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

static void overlay::show_aica_win(void) {
    ImGui::Begin("AICA", &en_aica_win);
    ImGui::BeginChild("Scrolling");
    ImGui::Checkbox("Show non-playing channels", &show_nonplaying_channels);

    for (unsigned idx = 0; idx < console->snddev.n_channels; idx++) {
        ImGui::PushID(idx);

        struct washdc_sndchan_stat ch_stat;
        washdc_gameconsole_sndchan(console, idx, &ch_stat);

        if (!show_nonplaying_channels && !ch_stat.playing)
            continue;

        std::stringstream ss;
        ss << "channel " << idx;
        if (ImGui::CollapsingHeader(ss.str().c_str())) {
            if (idx >= n_chans) {
                fprintf(stderr, "ERROR BUFFER OVERFLOW\n");
                continue;
            }

            ImGui::Checkbox("mute", sndchan_mute + idx);
            washdc_gameconsole_sndchan_mute(console, idx, sndchan_mute[idx]);

            std::stringstream playing_ss;
            playing_ss << "Playing: " << (ch_stat.playing ? "True" : "False");
            ImGui::Text(playing_ss.str().c_str());

            unsigned n_vars = ch_stat.n_vars;
            for (unsigned var_no = 0; var_no < n_vars; var_no++) {
                struct washdc_var var;
                washdc_gameconsole_sndchan_var(console, &ch_stat, var_no, &var);
                if (var.tp != WASHDC_VAR_INVALID) {
                    std::stringstream var_ss;
                    var_ss << var.name << ": " << var_as_str(&var);
                    ImGui::Text(var_ss.str().c_str());
                }
            }
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::End();
}

void overlay::set_fps(double fps) {
    framerate = fps;
}

void overlay::set_virt_fps(double fps) {
    virt_framerate = fps;
}

void overlay::init(bool enable_debugger) {
    n_chans = console->snddev.n_channels;
    sndchan_mute = new bool[n_chans];
    std::fill(sndchan_mute, sndchan_mute + n_chans, false);

    en_perf_win = true;
    not_hidden = false;
    have_debugger = enable_debugger;

    ImGui::CreateContext();

    ui_renderer = std::make_unique<renderer>();
}

void overlay::cleanup() {
    ui_renderer.reset();

    ImGui::DestroyContext();

    delete[] sndchan_mute;
}

void overlay::update() {
    ui_renderer->update();
}

static std::string overlay::var_as_str(struct washdc_var const *var) {
    std::stringstream ss;
    switch (var->tp) {
    case WASHDC_VAR_BOOL:
        if (var->val.as_bool)
            return "TRUE";
        else
            return "FALSE";
    default:
    case WASHDC_VAR_INT:
        ss << var->val.as_int;
        return ss.str();
    case WASHDC_VAR_HEX:
        ss << "0x" << std::hex << var->val.as_int;
        return ss.str();
    case WASHDC_VAR_STR:
        return var->val.as_str;
    case WASHDC_VAR_INVALID:
        return "INVALID";
    }
}
