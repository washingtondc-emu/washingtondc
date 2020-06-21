/*******************************************************************************
 *
 * Copyright 2017, 2018, 2020 snickerbockers
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

#ifndef SH4_TMU_H_
#define SH4_TMU_H_

#include <stdint.h>

#include "sh4_reg.h"
#include "dc_sched.h"

struct Sh4;

typedef uint64_t tmu_cycle_t;

struct sh4_tmu {
    // this is the cycle count from the last time we updated the chan_accum values
    tmu_cycle_t stamp_last_sync[3];

    struct SchedEvent tmu_chan_event[3];

    unsigned chan_accum[3];

    bool chan_event_scheduled[3];
};

void sh4_tmu_init(Sh4 *sh4);
void sh4_tmu_cleanup(Sh4 *sh4);

sh4_reg_val
sh4_tmu_tocr_read_handler(Sh4 *sh4,
                          struct Sh4MemMappedReg const *reg_info);
void sh4_tmu_tocr_write_handler(Sh4 *sh4,
                                struct Sh4MemMappedReg const *reg_info,
                                sh4_reg_val val);
sh4_reg_val sh4_tmu_tstr_read_handler(Sh4 *sh4,
                                      struct Sh4MemMappedReg const *reg_info);
void sh4_tmu_tstr_write_handler(Sh4 *sh4,
                                struct Sh4MemMappedReg const *reg_info,
                                sh4_reg_val val);

sh4_reg_val sh4_tmu_tcr_read_handler(Sh4 *sh4,
                                     struct Sh4MemMappedReg const *reg_info);
void sh4_tmu_tcr_write_handler(Sh4 *sh4,
                               struct Sh4MemMappedReg const *reg_info,
                               sh4_reg_val val);

sh4_reg_val sh4_tmu_tcnt_read_handler(Sh4 *sh4,
                                      struct Sh4MemMappedReg const *reg_info);
void sh4_tmu_tcnt_write_handler(Sh4 *sh4,
                                struct Sh4MemMappedReg const *reg_info,
                                sh4_reg_val val);

void sh4_tmu_tick(SchedEvent *event);

#endif
