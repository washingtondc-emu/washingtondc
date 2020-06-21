/*******************************************************************************
 *
 * Copyright 2019, 2020 snickerbockers
 * snickerbockers@washemu.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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


#ifdef __cplusplus
}
#endif

#endif
