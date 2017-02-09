/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017 snickerbockers
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
#include <iostream>

#include "sh4_excp.hpp"
#include "sh4.hpp"

// lookup table for TCR register indices
static sh4_reg_idx_t const chan_tcr[3] = {
    SH4_REG_TCR0, SH4_REG_TCR1, SH4_REG_TCR2
};

// lookup table for TCNT register indices
static sh4_reg_idx_t const chan_tcnt[3] = {
    SH4_REG_TCNT0, SH4_REG_TCNT1, SH4_REG_TCNT2
};

// lookup table for TCOR register indices
static sh4_reg_idx_t const chan_tcor[3] = {
    SH4_REG_TCOR0, SH4_REG_TCOR1, SH4_REG_TCOR2
};

void sh4_tmu_init(sh4_tmu *tmu) {
    memset(tmu, 0, sizeof(*tmu));
}

void sh4_tmu_cleanup(sh4_tmu *tmu) {
}

void sh4_tmu_tick(Sh4 *sh4) {
    unsigned ticks_per_countdown;

    sh4->tmu.last_tick = sh4->cycle_stamp;

    for (int chan = 0; chan < 3; chan++) {

        if (!(sh4->reg[SH4_REG_TSTR] & (1 << chan)))
            continue;

        sh4->tmu.tchan_accum[chan]++;

        switch(sh4->reg[chan_tcr[chan]] & SH4_TCR_TPSC_MASK) {
        case 0:
            ticks_per_countdown = 4;
            break;
        case 1:
            ticks_per_countdown = 16;
            break;
        case 2:
            ticks_per_countdown = 64;
            break;
        case 3:
            ticks_per_countdown = 256;
            break;
        case 5:
            ticks_per_countdown = 1024;
            break;
        default:
            // software shouldn't be doing this anyways
            return;
        }

        if (sh4->tmu.tchan_accum[chan] >= ticks_per_countdown) {
            sh4->tmu.tchan_accum[chan] = 0;

            // now actually do a single countdown
            // possible corner-case to watch out for: what if TCOR == 0?
            sh4->reg[chan_tcnt[chan]]--;
            if (!sh4->reg[chan_tcnt[chan]]) {
                sh4->reg[chan_tcnt[chan]] = sh4->reg[chan_tcor[chan]];

                sh4->reg[chan_tcr[chan]] |= SH4_TCR_UNF_MASK;

                if (sh4->reg[chan_tcr[chan]] & SH4_TCR_UNIE_MASK) {
                    Sh4ExceptionCode code;
                    switch (chan) {
                    case 0:
                        code = SH4_EXCP_TMU0_TUNI0;
                        break;
                    case 1:
                        code = SH4_EXCP_TMU1_TUNI1;
                        break;
                    case 2:
                        code = SH4_EXCP_TMU2_TUNI2;
                        break;
                    default:
                        break;
                    }

                    sh4_set_exception(sh4, code);
                }
            }
        }
    }
}

int sh4_tmu_tocr_read_handler(Sh4 *sh4, void *buf,
                              struct Sh4MemMappedReg const *reg_info) {
    uint8_t val = 1;
    memcpy(buf, &val, reg_info->len);
    return 0;
}

int sh4_tmu_tocr_write_handler(Sh4 *sh4, void const *buf,
                               struct Sh4MemMappedReg const *reg_info) {
    // sh4 spec says you can only write to the least-significant bit.
    // Dreamcast documents say this is always 1.
    sh4->reg[SH4_REG_TOCR] = 1;

    return 0;
}

int sh4_tmu_tstr_read_handler(Sh4 *sh4, void *buf,
                              struct Sh4MemMappedReg const *reg_info) {
    memcpy(buf, sh4->reg + SH4_REG_TSTR, reg_info->len);

    return 0;
}

int sh4_tmu_tstr_write_handler(Sh4 *sh4, void const *buf,
                               struct Sh4MemMappedReg const *reg_info) {
    uint8_t tmp;
    memcpy(&tmp, buf, reg_info->len);
    tmp &= 7;

    if (!(sh4->reg[SH4_REG_TSTR] & SH4_TSTR_CHAN0_MASK) ^
        (tmp & SH4_TSTR_CHAN0_MASK))
        sh4->tmu.tchan_accum[0] = 0;
    if (!(sh4->reg[SH4_REG_TSTR] & SH4_TSTR_CHAN1_MASK) ^
        (tmp & SH4_TSTR_CHAN1_MASK))
        sh4->tmu.tchan_accum[1] = 0;
    if (!(sh4->reg[SH4_REG_TSTR] & SH4_TSTR_CHAN2_MASK) ^
        (tmp & SH4_TSTR_CHAN2_MASK))
        sh4->tmu.tchan_accum[2] = 0;

    memcpy(sh4->reg + SH4_REG_TSTR, &tmp, reg_info->len);

    return 0;
}

int sh4_tmu_tcr_read_handler(Sh4 *sh4, void *buf,
                             struct Sh4MemMappedReg const *reg_info) {
    memcpy(buf, sh4->reg + reg_info->reg_idx, sizeof(uint16_t));

    return 0;
}

int sh4_tmu_tcr_write_handler(Sh4 *sh4, void const *buf,
                              struct Sh4MemMappedReg const *reg_info) {
    uint16_t new_val;
    memcpy(&new_val, buf, sizeof(new_val));

    uint16_t old_val = sh4->reg[reg_info->reg_idx];

    if ((new_val & SH4_TCR_ICPF_MASK) && !(old_val & SH4_TCR_ICPF_MASK))
        new_val &= ~SH4_TCR_ICPF_MASK;

    if ((new_val & SH4_TCR_UNF_MASK) && !(old_val & SH4_TCR_UNF_MASK))
        new_val &= ~SH4_TCR_UNF_MASK;

    if ((old_val & SH4_TCR_TPSC_MASK) != (new_val & SH4_TCR_TPSC_MASK)) {
        // changing clock source; clear accumulated ticks
        switch (reg_info->reg_idx) {
        case SH4_REG_TCR0:
            sh4->tmu.tchan_accum[0] = 0;
            break;
        case SH4_REG_TCR1:
            sh4->tmu.tchan_accum[1] = 0;
            break;
        case SH4_REG_TCR2:
            sh4->tmu.tchan_accum[2] = 0;
            break;
        default:
            BOOST_THROW_EXCEPTION(InvalidParamError());
        }
    }

    memcpy(sh4->reg + reg_info->reg_idx, &new_val, sizeof(uint16_t));

    return 0;
}
