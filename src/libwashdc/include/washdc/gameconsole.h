/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2019-2021 snickerbockers
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

#ifndef WASHDC_GAMECONSOLE_H_
#define WASHDC_GAMECONSOLE_H_

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WASHDC_VAR_NAME_LEN 32
#define WASHDC_VAR_STR_LEN 32

enum washdc_var_type {
    WASHDC_VAR_INVALID,
    WASHDC_VAR_BOOL,

    /*
     * WASHDC_VAR_INT and WASHDC_VAR_HEX both use the as_int member of
     * washdc_var_val.  The point of WASHDC_VAR_HEX is to provide a hint to
     * frontends about how this should be displayed, but really it's the same
     * as WASHDC_VAR_INT.
     */
    WASHDC_VAR_INT,
    WASHDC_VAR_HEX,

    WASHDC_VAR_STR,

    WASHDC_VAR_DOUBLE
};

union washdc_var_val {
    char as_str[WASHDC_VAR_STR_LEN];
    bool as_bool;
    int as_int;
    double as_double;
};

struct washdc_var {
    char name[WASHDC_VAR_NAME_LEN];
    enum washdc_var_type tp;
    union washdc_var_val val;
};

struct washdc_sndchan_stat {
    unsigned ch_idx;

    /*
     * This variable is treated as a special-case so that frontends can sue it
     * to filter out channels that aren't playing.  Otherwise, it would be a
     * washdc_var like everything else is.
     */
    bool playing;

    unsigned n_vars;
};

struct washdc_snddev {
    char const *name;
    unsigned const n_channels;

    void (*get_chan)(struct washdc_snddev const *dev,
                     unsigned ch_no,
                     struct washdc_sndchan_stat *stat);
    void (*get_var)(struct washdc_snddev const *dev,
                    struct washdc_sndchan_stat const *chan,
                    unsigned var_no, struct washdc_var *var);
    void (*mute_chan)(struct washdc_snddev const *dev,
                      unsigned chan_no, bool do_mute);
};

enum washdc_tex_fmt {
    WASHDC_TEX_FMT_ARGB_1555,
    WASHDC_TEX_FMT_RGB_565,
    WASHDC_TEX_FMT_ARGB_4444,
    WASHDC_TEX_FMT_ARGB_8888,
    WASHDC_TEX_FMT_YUV_422,

    WASHDC_TEX_FMT_COUNT
};

struct washdc_texinfo {
    unsigned idx;
    unsigned n_vars;
    bool valid;

    void *tex_dat;
    size_t n_tex_bytes;
    unsigned width, height;
    enum washdc_tex_fmt fmt;
};

struct washdc_texcache {
    unsigned sz;

    void (*get_texinfo)(struct washdc_texcache const* cache,
                        unsigned tex_no,
                        struct washdc_texinfo *texinfo);
    void (*get_var)(struct washdc_texcache const *cache,
                    struct washdc_texinfo const *texinfo,
                    unsigned var_no,
                    struct washdc_var *var);
};

struct washdc_gameconsole {
    char const *name;

    struct washdc_snddev snddev;
    struct washdc_texcache texcache;

    void (*do_inject_irq)(char const*);
};

void washdc_gameconsole_sndchan(struct washdc_gameconsole const *cons,
                                unsigned ch_no,
                                struct washdc_sndchan_stat *stat);
void washdc_gameconsole_sndchan_var(struct washdc_gameconsole const *cons,
                                    struct washdc_sndchan_stat const *chan,
                                    unsigned var_no,
                                    struct washdc_var *var);
void washdc_gameconsole_sndchan_mute(struct washdc_gameconsole const *cons,
                                     unsigned ch_no, bool mute);

void washdc_gameconsole_texinfo(struct washdc_gameconsole const *cons,
                                unsigned tex_no,
                                struct washdc_texinfo *texinfo);
void washdc_gameconsole_texinfo_var(struct washdc_gameconsole const *cons,
                                    struct washdc_texinfo const *texinfo,
                                    unsigned var_no, struct washdc_var *var);

void washdc_gameconsole_inject_irq(struct washdc_gameconsole const *cons,
                                   char const *irq_id);

void washdc_dump_main_memory(char const *path);
void washdc_dump_aica_memory(char const *path);

#ifdef __cplusplus
}
#endif

#endif
