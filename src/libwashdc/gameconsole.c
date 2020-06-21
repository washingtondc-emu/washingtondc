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

void washdc_gameconsole_inject_irq(struct washdc_gameconsole const *cons,
                                   char const *irq_id) {
    if (cons->do_inject_irq)
        cons->do_inject_irq(irq_id);
}
