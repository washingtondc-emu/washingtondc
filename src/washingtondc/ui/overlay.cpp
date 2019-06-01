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
static bool en_tex_cache_win = true;
static bool show_nonplaying_channels = true;
static bool have_debugger;

enum exec_options {
    EXEC_OPT_PAUSED,
    EXEC_OPT_100P,
    EXEC_OPT_UNLIMITED
};
static enum exec_options exec_opt;

static unsigned n_chans;
static bool *sndchan_mute;

static std::unique_ptr<renderer> ui_renderer;

namespace overlay {
static void show_perf_win(void);
static void show_aica_win(void);
static void show_tex_cache_win(void);
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
                exec_opt = EXEC_OPT_PAUSED;
                if (ImGui::MenuItem("Resume (normal speed)")) {
                    sound::set_sync_mode(sound::SYNC_MODE_NORM);
                    exec_opt = EXEC_OPT_100P;
                    do_resume();
                }
                if (ImGui::MenuItem("Resume (unlimited speed)")) {
                    sound::set_sync_mode(sound::SYNC_MODE_UNLIMITED);
                    exec_opt = EXEC_OPT_UNLIMITED;
                    do_resume();
                }
                if (ImGui::MenuItem("Run one frame")) {
                    exec_opt = EXEC_OPT_100P;
                    do_run_one_frame();
                }
            } else {
                int choice = (int)exec_opt;
                ImGui::RadioButton("Pause", &choice, EXEC_OPT_PAUSED);
                ImGui::RadioButton("100% speed", &choice, EXEC_OPT_100P);
                ImGui::RadioButton("Unlimited speed", &choice, EXEC_OPT_UNLIMITED);

                if (choice != (int)exec_opt) {
                    exec_opt = (enum exec_options)choice;
                    switch (exec_opt) {
                    case EXEC_OPT_PAUSED:
                        do_pause();
                        break;
                    case EXEC_OPT_100P:
                        sound::set_sync_mode(sound::SYNC_MODE_NORM);
                        break;
                    case EXEC_OPT_UNLIMITED:
                        sound::set_sync_mode(sound::SYNC_MODE_UNLIMITED);
                        break;
                    }
                }
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
            ImGui::Checkbox("Texture Cache", &en_tex_cache_win);
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

    if (en_tex_cache_win)
        show_tex_cache_win();

    if (mute_old != do_mute_audio)
        sound::mute(do_mute_audio);

    ImGui::Render();
    ui_renderer->do_render(ImGui::GetDrawData());
}

static void overlay::show_perf_win(void) {
    static double best = -DBL_MAX;
    static double worst = DBL_MAX;
    static const int MAX_FRAMES = 60 * 60 * 10;
    static int n_frames = 0;
    static int frame_idx = 0;
    static double total = 0.0;
    static double buf[MAX_FRAMES];

    struct washdc_pvr2_stat stat;
    washdc_get_pvr2_stat(&stat);

    double framerate_ratio = framerate / virt_framerate;
    if (!washdc_is_paused()) {
        // update persistent stats
        if (framerate_ratio > best)
            best = framerate_ratio;
        if (framerate_ratio < worst)
            worst = framerate_ratio;

        if (n_frames < MAX_FRAMES)
            n_frames++;
        else
            total -= buf[frame_idx];

        total += framerate_ratio;
        buf[frame_idx] = framerate_ratio;
        frame_idx = (frame_idx + 1) % MAX_FRAMES;
    }

    ImGui::Begin("Performance", &en_perf_win);
    ImGui::Text("Framerate: %.2f / %.2f (%.2f%%)", framerate, virt_framerate, 100.0 * framerate_ratio);
    ImGui::Text("%u frames rendered\n", washdc_get_frame_count());

    ImGui::Text("Best: %f%%", 100.0 * best);
    ImGui::Text("Worst: %f%%", 100.0 * worst);
    if (n_frames < MAX_FRAMES)
        ImGui::Text("Average: %f%%", 100.0 * (total / n_frames));
    else
        ImGui::Text("Average: %f%% (last %d frames)\n",
                    100.0 * (total / n_frames), MAX_FRAMES);

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
                ImGui::PopID();
                continue;
            }

            ImGui::Checkbox("mute", sndchan_mute + idx);
            washdc_gameconsole_sndchan_mute(console, idx, sndchan_mute[idx]);

            std::stringstream playing_ss;
            playing_ss << "Playing: " << (ch_stat.playing ? "True" : "False");
            ImGui::Text("%s", playing_ss.str().c_str());

            unsigned n_vars = ch_stat.n_vars;
            for (unsigned var_no = 0; var_no < n_vars; var_no++) {
                struct washdc_var var;
                washdc_gameconsole_sndchan_var(console, &ch_stat, var_no, &var);
                if (var.tp != WASHDC_VAR_INVALID) {
                    std::stringstream var_ss;
                    var_ss << var.name << ": " << var_as_str(&var);
                    ImGui::Text("%s", var_ss.str().c_str());
                }
            }
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::End();
}

static void overlay::show_tex_cache_win(void) {
    ImGui::Begin("Texture Cache", &en_tex_cache_win);
    ImGui::BeginChild("Scrolling");

    for (unsigned idx = 0; idx < console->texcache.sz; idx++) {
        struct washdc_texinfo texinfo;
        washdc_gameconsole_texinfo(console, idx, &texinfo);
        if (!texinfo.valid)
            continue;

        ImGui::PushID(idx);

        std::stringstream title;
        title << "texture " << idx;
        ImGui::CollapsingHeader(title.str().c_str());
        for (unsigned var_no = 0; var_no < texinfo.n_vars; var_no++) {
            struct washdc_var var;
            washdc_gameconsole_texinfo_var(console, &texinfo, var_no, &var);
            if (var.tp != WASHDC_VAR_INVALID) {
                std::stringstream ss;
                ss << var.name << ": " << var_as_str(&var);
                ImGui::Text("%s", ss.str().c_str());
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
    exec_opt = EXEC_OPT_100P;

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
    case WASHDC_VAR_DOUBLE:
        ss << var->val.as_double;
        return ss.str();
    case WASHDC_VAR_INVALID:
        return "INVALID";
    }
}
