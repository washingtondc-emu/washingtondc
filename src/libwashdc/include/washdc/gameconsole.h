/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2019 snickerbockers
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

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WASHDC_VAR_NAME_LEN 16

enum washdc_var_type {
    WASHDC_VAR_INVALID,
    WASHDC_VAR_BOOL
};

union washdc_var_val {
    bool as_bool;
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
};

struct washdc_gameconsole {
    char const *name;

    struct washdc_snddev snddev;
};

void washdc_gameconsole_sndchan(struct washdc_gameconsole const *cons,
                                unsigned ch_no,
                                struct washdc_sndchan_stat *stat);
void washdc_gameconsole_sndchan_var(struct washdc_gameconsole const *cons,
                                    struct washdc_sndchan_stat const *chan,
                                    unsigned var_no,
                                    struct washdc_var *var);

#ifdef __cplusplus
}
#endif

#endif
