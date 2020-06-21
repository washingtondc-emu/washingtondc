/*
 * Copyright 2020 snickerbockers
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
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AICA_ADPCM_H_
#define AICA_ADPCM_H_

#include <stdint.h>

#include "aica.h"

static inline int32_t adpcm_yamaha_expand_nibble(struct aica_chan *c, uint8_t nibble)
{
    /*
     * TODO: support for Yamaha's ADPCM audio format goes here.
     *
     * This is used in several games and also the dreamcast's boot animation.
     * This doesn't seem like too complicated of a sound format, but I was
     * never able to find any docs explaining exactly *how* it works so the only option
     * was to copy over code from another open-source project.  There are a few
     * magic numbers involved so I suspect that there's a document somewhere
     * that explains it all but I've never been able to find it.
     *
     * Previously this file contained LGPL-licensed code from FFMPEG.  since this is
     * obviously not compatible with the BSD re-licensing I've had to remove it and
     * replace it with this stub which does nothing.
     *
     * MAME, Reicast, and older versions of redream from back when it was
     * MIT-licensed ought to have implementations which can be put in here
     * without violating their licenses.
     *
     * Back when it had FFMPEG code this function was less than 20 lines long,
     * so It's really not that hard to implement, I just never knew where the
     * math was coming from which is why I couldn't do it myself.
     */

    return 0;
}

#endif
