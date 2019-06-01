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

#include "washdc/gameconsole.h"

void washdc_gameconsole_sndchan(struct washdc_gameconsole const *cons,
                                unsigned ch_no,
                                struct washdc_sndchan_stat *stat) {
    cons->snddev.get_chan(&cons->snddev, ch_no, stat);
}

void washdc_gameconsole_sndchan_var(struct washdc_gameconsole const *cons,
                                    struct washdc_sndchan_stat const *chan,
                                    unsigned var_no,
                                    struct washdc_var *var) {
    cons->snddev.get_var(&cons->snddev, chan, var_no, var);
}

void washdc_gameconsole_sndchan_mute(struct washdc_gameconsole const *cons,
                                     unsigned ch_no, bool mute) {
    cons->snddev.mute_chan(&cons->snddev, ch_no, mute);
}

void washdc_gameconsole_texinfo(struct washdc_gameconsole const *cons,
                                unsigned tex_no,
                                struct washdc_texinfo *texinfo) {
    cons->texcache.get_texinfo(&cons->texcache, tex_no,  texinfo);
}

void washdc_gameconsole_texinfo_var(struct washdc_gameconsole const *cons,
                                    struct washdc_texinfo const *texinfo,
                                    unsigned var_no, struct washdc_var *var) {
    cons->texcache.get_var(&cons->texcache, texinfo, var_no, var);
}
