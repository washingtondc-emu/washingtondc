/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018-2021 snickerbockers
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

#ifdef _WIN32
#include "i_hate_windows.h"
#endif

#include <cstring>
#include <cstdio>
#include <memory>
#include <sstream>
#include <vector>
#include <iostream>
#include <memory>

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#include <GL/gl.h>

#include "washdc/win.h"
#include "washdc/washdc.h"
#include "washdc/gameconsole.h"
#include "washdc/pix_conv.h"
#include "imgui.h"
#include "renderer.hpp"
#include "../window.hpp"
#include "../sound.hpp"
#include "../washingtondc.hpp"
#include "../config_file.h"

#ifndef DISABLE_MEM_DUMP_UI
#include "imfilebrowser.h"
#endif

#include "overlay.hpp"

// see main.cpp
extern struct washdc_gameconsole const *console;

static bool not_hidden;
static bool en_perf_win = true;
static bool en_demo_win = false;
static bool en_aica_win = true;

// disabled by default due to poor performance
static bool en_tex_cache_win = false;

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
static void show_tex_win(unsigned idx);
static std::string var_as_str(struct washdc_var const *var);

#ifndef DISABLE_MEM_DUMP_UI
static std::unique_ptr<ImGui::FileBrowser> mem_dump_browser;
#endif

/*
 * This structure represents a texture in the texture-cache which the UI has a
 * separate copy of
 */
struct tex_stat {
    // OpenGL object that the ui's copy of the texture is
    GLuint tex_obj;

    // if true then the window for this texture will be shown
    bool show_window;

    double aspect_ratio;

    // if true an update is needed
    bool dirty;
};

static void update_tex_cache_ent(struct washdc_texinfo *texinfo, tex_stat stat);

static std::vector<tex_stat> textures;

}

void overlay::show(bool do_show) {
    not_hidden = do_show;
}

void overlay::draw(void) {
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

        if (ImGui::BeginMenu("Debug")) {
            if (ImGui::BeginMenu("IRQ injection")) {
                char const *irqstr = NULL;

                if (ImGui::MenuItem("HBLANK"))
                    irqstr = "HBLANK";
                else if (ImGui::MenuItem("VBLANK-IN"))
                    irqstr = "VBLANK-IN";
                else if (ImGui::MenuItem("VBLANK-OUT"))
                    irqstr = "VBLANK-OUT";
                else if (ImGui::MenuItem("POLYGON EOL OPAQUE"))
                    irqstr = "POLYGON EOL OPAQUE";
                else if (ImGui::MenuItem("POLYGON EOL OPAQUE MOD"))
                    irqstr = "POLYGON EOL OPAQUE MOD";
                else if (ImGui::MenuItem("POLYGON EOL TRANSPARENT"))
                    irqstr = "POLYGON EOL TRANSPARENT";
                else if (ImGui::MenuItem("POLYGON EOL TRANSPARENT MOD"))
                    irqstr = "POLYGON EOL TRANSPARENT MOD";
                else if (ImGui::MenuItem("POLYGON EOL PUNCH-THROUGH"))
                    irqstr = "POLYGON EOL PUNCH-THROUGH";
                else if (ImGui::MenuItem("POWERVR2 RENDER COMPLETE"))
                    irqstr = "POWERVR2 RENDER COMPLETE";
                else if (ImGui::MenuItem("POWERVR2 YUV CONVERSION COMPLETE"))
                    irqstr = "POWERVR2 YUV CONVERSION COMPLETE";
                else if (ImGui::MenuItem("POWERVR2 DMA"))
                    irqstr = "POWERVR2 DMA";
                else if (ImGui::MenuItem("MAPLE DMA"))
                    irqstr = "MAPLE DMA";
                else if (ImGui::MenuItem("AICA DMA"))
                    irqstr = "AICA DMA";
                else if (ImGui::MenuItem("AICA (ARM7 TO SH4)"))
                    irqstr = "AICA (ARM7 TO SH4)";
                else if (ImGui::MenuItem("GD-ROM"))
                    irqstr = "GD-ROM";
                else if (ImGui::MenuItem("GD-DMA"))
                    irqstr = "GD-DMA";
                else if (ImGui::MenuItem("SORT DMA"))
                    irqstr = "SORT DMA";
                else if (ImGui::MenuItem("AICA SAMPLE INTERVAL"))
                    irqstr = "AICA SAMPLE INTERVAL";
                else if (ImGui::MenuItem("AICA MIDI OUT"))
                    irqstr = "AICA MIDI OUT";
                else if (ImGui::MenuItem("AICA TIMER C"))
                    irqstr = "AICA TIMER C";
                else if (ImGui::MenuItem("AICA TIMER B"))
                    irqstr = "AICA TIMER B";
                else if (ImGui::MenuItem("AICA TIMER A"))
                    irqstr = "AICA TIMER A";
                else if (ImGui::MenuItem("SH4 => AICA"))
                    irqstr = "SH4 => AICA";
                else if (ImGui::MenuItem("AICA DMA"))
                    irqstr = "AICA DMA";
                else if (ImGui::MenuItem("AICA MIDI IN"))
                    irqstr = "AICA MIDI IN";
                else if (ImGui::MenuItem("AICA EXTERNAL"))
                    irqstr = "AICA EXTERNAL";

                if (irqstr)
                    washdc_gameconsole_inject_irq(console, irqstr);

                ImGui::EndMenu();
            }

#ifndef DISABLE_MEM_DUMP_UI
            if (ImGui::MenuItem("Dump Main Memory"))
                mem_dump_browser->Open();
#endif
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

    for (tex_stat& stat : textures)
        stat.dirty = true;

    if (en_tex_cache_win)
        show_tex_cache_win();

    for (unsigned tex_idx = 0; tex_idx < textures.size(); tex_idx++) {
        if (textures.at(tex_idx).show_window) {
            if (textures.at(tex_idx).dirty) {
                struct washdc_texinfo texinfo;
                textures.at(tex_idx).dirty = false;
                washdc_gameconsole_texinfo(console, tex_idx, &texinfo);
                if (!texinfo.valid) {
                    textures.at(tex_idx).show_window = false;
                    continue;
                }
                update_tex_cache_ent(&texinfo, textures.at(tex_idx));
                free(texinfo.tex_dat);
            }

            show_tex_win(tex_idx);
        }
    }

#ifndef DISABLE_MEM_DUMP_UI
    mem_dump_browser->Display();
    if (mem_dump_browser->HasSelected()) {
        std::filesystem::path sel = mem_dump_browser->GetSelected();
        mem_dump_browser->Close();
        washdc_dump_main_memory(sel.string().c_str());
    }
#endif

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

    double framerate = washdc_get_fps();
    double virt_framerate = washdc_get_virt_fps();

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

    ImGui::Text("%u opaque vertices",
                stat.vert_count[WASHDC_PVR2_POLY_GROUP_OPAQUE]);
    ImGui::Text("%u opaque modifier vertices",
                stat.vert_count[WASHDC_PVR2_POLY_GROUP_OPAQUE_MOD]);
    ImGui::Text("%u transparent vertices",
                stat.vert_count[WASHDC_PVR2_POLY_GROUP_TRANS]);
    ImGui::Text("%u transparent modifier vertices",
                stat.vert_count[WASHDC_PVR2_POLY_GROUP_TRANS_MOD]);
    ImGui::Text("%u punch-through vertices",
                stat.vert_count[WASHDC_PVR2_POLY_GROUP_PUNCH_THROUGH]);
    ImGui::Text("%u texture transmissions",
                stat.tex_xmit_count);
    ImGui::Text("%u texture invalidates",
                stat.tex_invalidate_count);
    ImGui::Text("%u paletted texture invalidates",
                stat.pal_tex_invalidate_count);
    ImGui::Text("%u texture overwrites", stat.texture_overwrite_count);
    ImGui::Text("%u fresh texture uploads", stat.fresh_texture_upload_count);
    ImGui::Text("%u texture cache evictions", stat.tex_eviction_count);
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

static void overlay::show_tex_win(unsigned idx) {
    std::stringstream title_stream;
    tex_stat const& stat = textures.at(idx);

    title_stream << "texture cache entry " << idx;

    ImGui::Begin(title_stream.str().c_str(), &textures.at(idx).show_window,
                 ImGuiWindowFlags_NoScrollbar);

    ImVec2 win_sz = ImGui::GetContentRegionAvail();
    ImVec2 img_sz;

    if (win_sz.x / win_sz.y < stat.aspect_ratio) {
        // fit to x
        img_sz.x = win_sz.x;
        img_sz.y = win_sz.x / stat.aspect_ratio;
    } else {
        // fit to y
        img_sz.y = win_sz.y;
        img_sz.x = win_sz.y * stat.aspect_ratio;
    }

    ImGui::Image((ImTextureID)(uintptr_t)stat.tex_obj, img_sz, ImVec2(0,0),
                 ImVec2(1,1), ImVec4(1,1,1,1), ImVec4(1,1,1,1));

    ImGui::End();
}

static void
overlay::update_tex_cache_ent(struct washdc_texinfo *texinfo,
                              struct overlay::tex_stat stat) {
    glBindTexture(GL_TEXTURE_2D, stat.tex_obj);

    unsigned tex_w = texinfo->width, tex_h = texinfo->height;
    void *dat = texinfo->tex_dat;

    unsigned n_colors;
    unsigned pvr2_pix_size;
    GLenum fmt;
    std::vector<uint8_t> dat_conv;
    switch (texinfo->fmt) {
    case WASHDC_TEX_FMT_ARGB_1555:
    case WASHDC_TEX_FMT_ARGB_4444:
        n_colors = 4;
        pvr2_pix_size = 2;
        fmt = GL_RGBA;
        break;
    case WASHDC_TEX_FMT_RGB_565:
        n_colors = 3;
        pvr2_pix_size = 2;
        fmt = GL_RGB;
        break;
    case WASHDC_TEX_FMT_ARGB_8888:
        n_colors = 4;
        pvr2_pix_size = 4;
        fmt = GL_RGBA;
        break;
    case WASHDC_TEX_FMT_YUV_422:
        n_colors = 4;
        pvr2_pix_size = 3;
        dat_conv.resize(n_colors * tex_w * tex_h);
        washdc_conv_yuv422_rgba8888(dat_conv.data(), dat, tex_w, tex_h);
        dat = dat_conv.data();
        fmt = GL_RGBA;
        break;
    default:
        glBindTexture(GL_TEXTURE_2D, 0);
        return;
    }

    std::vector<uint8_t> tmp_pix_buf(tex_w * tex_h * n_colors);

    unsigned row, col;
    for (row = 0; row < tex_h; row++) {
        uint8_t *cur_row = tmp_pix_buf.data() + tex_w * n_colors * row;

        for (col = 0; col < tex_w; col++) {
            unsigned pix_idx = row * tex_w + col;
            uint8_t src_pix[4];
            unsigned red, green, blue, alpha;

            memcpy(src_pix, ((uint8_t*)dat) + pix_idx * pvr2_pix_size, sizeof(src_pix));

            switch (texinfo->fmt) {
            case WASHDC_TEX_FMT_ARGB_1555:
                alpha = src_pix[1] & 0x80 ? 255 : 0;
                red = (src_pix[1] & 0x7c) >> 2;
                green = ((src_pix[1] & 0x03) << 3) | ((src_pix[0] & 0xe0) >> 5);
                blue = src_pix[0] & 0x1f;

                red <<= 3;
                green <<= 3;
                blue <<= 3;
                break;
            case WASHDC_TEX_FMT_ARGB_4444:
                blue = src_pix[0] & 0x0f;
                green = (src_pix[0] & 0xf0) >> 4;
                red = src_pix[1] & 0x0f;
                alpha = (src_pix[1] & 0xf0) >> 4;

                alpha <<= 4;
                red <<= 4;
                green <<= 4;
                blue <<= 4;
                break;
            case WASHDC_TEX_FMT_RGB_565:
                blue = src_pix[0] & 0x1f;
                green = ((src_pix[0] & 0xe0) >> 5) | ((src_pix[1] & 0x7) << 3);
                red = (src_pix[1] & 0xf1) >> 3;

                red <<= 3;
                green <<= 2;
                blue <<= 3;
                alpha = 255;
                break;
            case WASHDC_TEX_FMT_YUV_422:
                red = src_pix[0];
                green = src_pix[1];
                blue = src_pix[2];
                alpha = 255;
                break;
            case WASHDC_TEX_FMT_ARGB_8888:
                alpha = src_pix[0];
                red = src_pix[1];
                green = src_pix[2];
                blue = src_pix[3];
                break;
            default:
                glBindTexture(GL_TEXTURE_2D, 0);
                return;
            }

            cur_row[n_colors * col] = red;
            cur_row[n_colors * col + 1] = green;
            cur_row[n_colors * col + 2] = blue;
            if (n_colors == 4)
                cur_row[n_colors * col + 3] = alpha;
        }
    }

    glTexImage2D(GL_TEXTURE_2D, 0, fmt, tex_w, tex_h, 0, fmt, GL_UNSIGNED_BYTE, tmp_pix_buf.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, 0);
}


static void overlay::show_tex_cache_win(void) {
    ImGui::Begin("Texture Cache", &en_tex_cache_win);
    ImGui::BeginChild("Scrolling");

    for (unsigned idx = 0; idx < console->texcache.sz; idx++) {
        struct washdc_texinfo texinfo;

        washdc_gameconsole_texinfo(console, idx, &texinfo);
        if (!texinfo.valid) {
            textures.at(idx).show_window = false;
            continue;
        }

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

        if (textures.at(idx).dirty) {
            update_tex_cache_ent(&texinfo, textures.at(idx));
            textures.at(idx).dirty = false;
        }

        if (ImGui::ImageButton((ImTextureID)(uintptr_t)textures.at(idx).tex_obj, ImVec2(64, 64),
                               ImVec2(0, 0), ImVec2(1,1), -1, ImVec4(1,1,1,1),
                               ImVec4(1, 1, 1, 1))) {
            textures.at(idx).show_window = true;
            unsigned tex_w = texinfo.width, tex_h = texinfo.height;
            textures.at(idx).aspect_ratio = (double)tex_w / (double)tex_h;
        }

        free(texinfo.tex_dat);
        ImGui::PopID();
    }

    ImGui::EndChild();
    ImGui::End();
}

void overlay::init(bool enable_debugger) {
    char const *exec_mode_str = cfg_get_node("exec.speed");
    if (exec_mode_str == NULL || strcmp(exec_mode_str, "full") == 0) {
        exec_opt = EXEC_OPT_100P;
        sound::set_sync_mode(sound::SYNC_MODE_NORM);
    } else if (strcmp(exec_mode_str, "unlimited") == 0) {
        exec_opt = EXEC_OPT_UNLIMITED;
        sound::set_sync_mode(sound::SYNC_MODE_UNLIMITED);
    } else if (strcmp(exec_mode_str, "pause") == 0) {
        exec_opt = EXEC_OPT_PAUSED;
        do_pause();
    } else {
        exec_opt = EXEC_OPT_100P;
        sound::set_sync_mode(sound::SYNC_MODE_NORM);
        std::cerr << "Unrecognized execution mode \"" <<
            exec_mode_str << "\"" << std::endl;
    }

    n_chans = console->snddev.n_channels;
    sndchan_mute = new bool[n_chans];
    std::fill(sndchan_mute, sndchan_mute + n_chans, false);

    en_perf_win = true;
    not_hidden = false;
    have_debugger = enable_debugger;

    ImGui::CreateContext();

    ui_renderer = std::make_unique<renderer>();

    textures.resize(console->texcache.sz);
    for (tex_stat& stat : textures)
        glGenTextures(1, &stat.tex_obj);

#ifndef DISABLE_MEM_DUMP_UI
    ImGuiFileBrowserFlags browser_flags =
        ImGuiFileBrowserFlags_EnterNewFilename |
        ImGuiFileBrowserFlags_CreateNewDir;
    mem_dump_browser = std::make_unique<ImGui::FileBrowser>(browser_flags);
    mem_dump_browser->SetTitle("Save Main System Memory Dump");
    mem_dump_browser->SetTypeFilters({ ".bin" });
#endif
}

void overlay::cleanup(void) {
#ifndef DISABLE_MEM_DUMP_UI
    mem_dump_browser.reset(nullptr);
#endif

    for (tex_stat& stat : textures)
        glDeleteTextures(1, &stat.tex_obj);

    ui_renderer.reset(nullptr);

    ImGui::DestroyContext();

    delete[] sndchan_mute;
}

void overlay::update(void) {
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

void overlay::input_text(unsigned codepoint) {
    if (not_hidden) {
        ImGuiIO& io = ImGui::GetIO();
        io.AddInputCharacter(codepoint);
    }
}
