/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2018 snickerbockers
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

#ifndef AICA_H_
#define AICA_H_

#include <stdint.h>

#include "dc_sched.h"
#include "aica_wave_mem.h"

struct arm7;

#define AICA_SYS_LEN 0x8000
#define AICA_SYS_MASK (AICA_SYS_LEN - 1)

enum ringbuffer_size {
    RINGBUFFER_SIZE_8K,
    RINGBUFFER_SIZE_16K,
    RINGBUFFER_SIZE_32K,
    RINGBUFFER_SIZE_64K
};

#define AICA_CHAN_COUNT 64
#define AICA_CHAN_LEN 128

enum aica_fmt {
    AICA_FMT_16_BIT_SIGNED,
    AICA_FMT_8_BIT_SIGNED,
    AICA_FMT_4_BIT_ADPCM,
    AICA_FMT_INVALID
};

enum aica_env_state {
    AICA_ENV_ATTACK = 0,
    AICA_ENV_DECAY = 1,
    AICA_ENV_SUSTAIN = 2,
    AICA_ENV_RELEASE = 3
};

enum aica_afsel {
    AICA_AFSEL_ATTEN,
    AICA_AFSEL_FILTER
};

// It's AICA's cute younger sister, aica-chan!
struct aica_chan {
    uint8_t raw[AICA_CHAN_LEN];

    /*
     * attenuation (referred to as envelope level in Neil Corlett's document)
     *
     * This is negative gain, expressed in a notation similar to floating point.
     * I don't completely understand the documentation, but I think bits 0-5 are
     * the mantissa and bits 6-9 are the logarithm.  According to corlett's
     * notes, the maximum value is 0x3bf, and anything above that is changed to
     * by 0x1fff.  The PlayStatus register allocated 13 bits for this, but only
     * 10 bits are used (unless it gets clamped to 0x1fff).
     */
    unsigned atten;

    // the state of the amplitude envelope in the PlayStatus register
    enum aica_env_state atten_env_state;

    /*
     * the loop-end flag in the PlayStatus register.  This gets cleared every
     * time it is read from.
     */
    bool loop_end_playstatus_flag;

    enum aica_fmt fmt;

    uint32_t addr_start, addr_cur;
    uint32_t loop_start, loop_end;

    /*
     * bit 15 of the play control register will execute a key-on (if set) or
     * key-off (if cleared) for every channel which has bit 14 set.  Bit 15
     * effects every channel which has bit 14 set, not just the channel that
     * was actually written to.
     */
    bool ready_keyon; // bit 14 of play control
    bool playing;     // if true, this channel is playing
    bool keyon;       // bit 15 of play control

    unsigned step_no;
    unsigned sample_no;

    // from the AmpEnv1 register
    unsigned attack_rate, decay_rate, sustain_rate;

    // from the AmpEnv2 register
    unsigned krs;
    unsigned decay_level;
    unsigned release_rate;

    // from the SampleRatePitch register
    unsigned octave;
    unsigned fns;
};

struct aica_timer {
    struct SchedEvent evt;

    bool scheduled;

    unsigned counter;
    unsigned prescale_log;
};

struct aica {
    struct aica_wave_mem mem;
    struct arm7 *arm7;

    // interrupt enable (SCIEB)
    uint32_t int_enable;

    // pending interrupts (SCIPD)
    uint32_t int_pending;

    // AICA_MCIEB
    uint32_t int_enable_sh4;

    // AICA_MCIPD
    uint32_t int_pending_sh4;

    bool irq_line;

    unsigned ringbuffer_addr;
    enum ringbuffer_size ringbuffer_size;
    bool ringbuffer_bit15;

    /*
     * Selects the channel that the PlayStatus register refers to
     *
     * (mslc bit in the ChnInfoReq register).
     */
    unsigned chan_sel;

    /*
     * selects whether PlayStatus refers to the amplitude envelope or the
     * filter envelope
     *
     * (afsel bit in the ChnInfoReq register)
     */
    enum aica_afsel afsel;

    /*
     * This is backing for registers that allow reads/writes to go through
     * transparently. There is space for every register here, but not every
     * register will use it.  Some register reads/writes will go to other
     * members of this struct instead.
     *
     * the purpose of the + 8 is to prevent buffer over-runs.  Bounds-checking
     * code in this AICA implementation tends to not take the length of the
     * read/write into account (not for any good reason, that's just the way I
     * wrote it...), so extending it by sizeof(double) (== 8 bytes) prevents
     * buffer overruns.
     */
    uint32_t sys_reg[(AICA_SYS_LEN / 4) + 8];

    struct aica_chan channels[AICA_CHAN_COUNT];

    dc_cycle_stamp_t last_sample_sync;

    // timerA, timerB, timerC
    struct aica_timer timers[3];

    struct dc_clock *clk;
};

void aica_init(struct aica *aica, struct arm7 *arm7, struct dc_clock *clk);
void aica_cleanup(struct aica *aica);

extern struct memory_interface aica_sys_intf;

extern bool aica_log_verbose_val;

#endif
