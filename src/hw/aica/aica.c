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

/*
 * Yamaha AICA Super-Intelligent Sound Processor.
 *
 * This implementation is based on Neill Corlett's AICA notes and a little bit
 * of my own experimentation.
 */

#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "log.h"
#include "error.h"
#include "intmath.h"
#include "hw/arm7/arm7.h"

#include "aica.h"

/*
 * TODO: I'm really only assuming this is 44.1KHz because that's the standard,
 * I don't actually know if this is correct.
 */
#define AICA_SAMPLE_FREQ 44100

/*
 * TODO: SCHED_FREQUENCY is not an integer multiple of AICA_SAMPLE_FREQ, so
 * there will be some inaccuracies here.
 */
#define TICKS_PER_SAMPLE (SCHED_FREQUENCY / AICA_SAMPLE_FREQ)

#define AICA_CHAN_PLAY_CTRL 0x0000

#define AICA_MASTER_VOLUME 0x2800

#define AICA_ARM7_RST 0x2c00

#define AICA_RINGBUFFER_ADDRESS 0x2804

#define AICA_PLAYSTATUS 0x2810

#define AICA_PLAYPOS 0x2814

#define AICA_UNKNOWN_2880 0x2880

#define AICA_TIMERA_CTRL 0x2890
#define AICA_TIMERB_CTRL 0x2894
#define AICA_TIMERC_CTRL 0x2898

#define AICA_SCILV0 0x28a8
#define AICA_SCILV1 0x28ac
#define AICA_SCILV2 0x28b0

// interrupt enable
#define AICA_SCIEB 0x289c

// interrupt pending
#define AICA_SCIPD 0x28a0

// interrupt reset
#define AICA_SCIRE 0x28a4

// SH4 interrupt enable
#define AICA_MCIEB 0x28b4

// SH4 interrupt pending
#define AICA_MCIPD 0x28b8

// SH4 interrupt reset
#define AICA_MCIRE 0x28bc

#define AICA_MIDI_INPUT 0x2808

#define AICA_INTREQ 0x2d00

#define AICA_INTCLEAR 0x2d04

#define AICA_CHANINFOREQ 0x280c

#define AICA_INT_EXTERNAL_SHIFT 0
#define AICA_INT_EXTERNAL_MASK (1 << AICA_INT_EXTERNAL_SHIFT)

#define AICA_INT_MIDI_IN_SHIFT 3
#define AICA_INT_MIDI_IN_MASK (1 << AICA_INT_MIDI_IN_SHIFT)

#define AICA_INT_DMA_SHIFT 4
#define AICA_INT_DMA_MASK (1 << AICA_INT_DMA_SHIFT)

#define AICA_INT_CPU_SHIFT 5
#define AICA_INT_CPU_MASK (1 << AICA_INT_CPU_SHIFT)

#define AICA_INT_TIMA_SHIFT 6
#define AICA_INT_TIMA_MASK (1 << AICA_INT_TIMA_SHIFT)

#define AICA_INT_TIMB_SHIFT 7
#define AICA_INT_TIMB_MASK (1 << AICA_INT_TIMB_SHIFT)

#define AICA_INT_TIMC_SHIFT 8
#define AICA_INT_TIMC_MASK (1 << AICA_INT_TIMC_SHIFT)

#define AICA_INT_MIDI_OUT_SHIFT 9
#define AICA_INT_MIDI_OUT_MASK (1 << AICA_INT_MIDI_OUT_SHIFT)

#define AICA_INT_SAMPLE_INTERVAL_SHIFT 10
#define AICA_INT_SAMPLE_INTERVAL_MASK (1 << AICA_INT_SAMPLE_INTERVAL_SHIFT)

// Mask of all the interrupt bits that we care about
#define AICA_ALL_INT_MASK (AICA_INT_SAMPLE_INTERVAL_MASK |      \
                           AICA_INT_MIDI_OUT_MASK |             \
                           AICA_INT_TIMC_MASK |                 \
                           AICA_INT_TIMB_MASK |                 \
                           AICA_INT_TIMA_MASK |                 \
                           AICA_INT_CPU_MASK |                  \
                           AICA_INT_DMA_MASK |                  \
                           AICA_INT_MIDI_IN_MASK |              \
                           AICA_INT_EXTERNAL_MASK)

// If this is defined, WashingtonDC will panic on unrecognized AICA addresses.
#define AICA_PEDANTIC

static void aica_update_interrupts(struct aica *aica);

static void
aica_sys_reg_read(struct aica *aica, addr32_t addr,
                  void *out, unsigned n_bytes, bool from_sh4);
static void aica_sys_reg_write(struct aica *aica, addr32_t addr,
                               void const *val_in, unsigned n_bytes,
                               bool from_sh4);

static float aica_sys_read_float(addr32_t addr, void *ctxt);
static void aica_sys_write_float(addr32_t addr, float val, void *ctxt);
static double aica_sys_read_double(addr32_t addr, void *ctxt);
static void aica_sys_write_double(addr32_t addr, double val, void *ctxt);
static uint32_t aica_sys_read_32(addr32_t addr, void *ctxt);
static void aica_sys_write_32(addr32_t addr, uint32_t val, void *ctxt);
static uint16_t aica_sys_read_16(addr32_t addr, void *ctxt);
static void aica_sys_write_16(addr32_t addr, uint16_t val, void *ctxt);
static uint8_t aica_sys_read_8(addr32_t addr, void *ctxt);
static void aica_sys_write_8(addr32_t addr, uint8_t val, void *ctxt);

static void aica_sys_channel_read(struct aica *aica, void *dst,
                                  uint32_t addr, unsigned len);
static void aica_dsp_mixer_read(struct aica *aica, void *dst,
                                uint32_t addr, unsigned len);
static void aica_dsp_reg_read(struct aica *aica, void *dst,
                              uint32_t addr, unsigned len);
static void aica_sys_channel_write(struct aica *aica, void const *src,
                                   uint32_t addr, unsigned len);
static void aica_dsp_mixer_write(struct aica *aica, void const *src,
                                 uint32_t addr, unsigned len);
static void aica_dsp_reg_write(struct aica *aica, void const *src,
                               uint32_t addr, unsigned len);

static bool aica_check_irq(void *ctxt);

__attribute__((unused))
static void aica_unsched_all_timers(struct aica *aica);

__attribute__((unused))
static void aica_sched_all_timers(struct aica *aica);

static void aica_unsched_timer(struct aica *aica, unsigned tim_idx);
static void aica_sched_timer(struct aica *aica, unsigned tim_idx);

static void aica_sync_timer(struct aica *aica, unsigned tim_idx);

static void
aica_timer_a_handler(struct SchedEvent *evt);
static void
aica_timer_b_handler(struct SchedEvent *evt);
static void
aica_timer_c_handler(struct SchedEvent *evt);

static void aica_timer_handler(struct aica *aica, unsigned tim_idx);

static unsigned aica_read_sci(struct aica *aica, unsigned bit);

struct memory_interface aica_sys_intf = {
    .read32 = aica_sys_read_32,
    .read16 = aica_sys_read_16,
    .read8 = aica_sys_read_8,
    .readfloat = aica_sys_read_float,
    .readdouble = aica_sys_read_double,

    .write32 = aica_sys_write_32,
    .write16 = aica_sys_write_16,
    .write8 = aica_sys_write_8,
    .writefloat = aica_sys_write_float,
    .writedouble = aica_sys_write_double
};

void aica_init(struct aica *aica, struct arm7 *arm7, struct dc_clock *clk) {
    memset(aica, 0, sizeof(*aica));

    aica->clk = clk;
    aica->arm7 = arm7;

    // HACK
    aica->int_enable = AICA_TIMERA_CTRL;

    /*
     * The corlett docs say these are default values
     */
    aica->sys_reg[AICA_SCILV0 / 4] = 0x18;
    aica->sys_reg[AICA_SCILV1 / 4] = 0x50;
    aica->sys_reg[AICA_SCILV2 / 4] = 0x08;

    /*
     * TODO: I think this might actually be meant to be handled as FIQ, not IRQ.
     * I need to do more research to confirm.
     */
    arm7->check_fiq = aica_check_irq;
    arm7->check_fiq_dat = aica;

    aica->timers[0].evt.handler = aica_timer_a_handler;
    aica->timers[1].evt.handler = aica_timer_b_handler;
    aica->timers[2].evt.handler = aica_timer_c_handler;
    aica->timers[0].evt.arg_ptr = aica;
    aica->timers[1].evt.arg_ptr = aica;
    aica->timers[2].evt.arg_ptr = aica;

    aica_sched_all_timers(aica);

    aica_wave_mem_init(&aica->mem);
}

void aica_cleanup(struct aica *aica) {
    aica->arm7->check_irq = NULL;
    aica->arm7->check_irq_dat = NULL;

    aica_wave_mem_cleanup(&aica->mem);
}

static float aica_sys_read_float(addr32_t addr, void *ctxt) {
    addr &= AICA_SYS_MASK;

    error_set_address(addr);
    error_set_length(4);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void aica_sys_write_float(addr32_t addr, float val, void *ctxt) {
    addr &= AICA_SYS_MASK;

    error_set_address(addr);
    error_set_length(4);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static double aica_sys_read_double(addr32_t addr, void *ctxt) {
    addr &= AICA_SYS_MASK;

    error_set_address(addr);
    error_set_length(8);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void aica_sys_write_double(addr32_t addr, double val, void *ctxt) {
    addr &= AICA_SYS_MASK;

    error_set_address(addr);
    error_set_length(8);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void
aica_sys_reg_pre_read(struct aica *aica, unsigned idx, bool from_sh4) {
    uint32_t val;
    switch (4 * idx) {
    case AICA_MASTER_VOLUME:
        // Neill Corlett's AICA notes say this is always 16 when you read from it
        val = 16;
        memcpy(aica->sys_reg + idx, &val, sizeof(uint32_t));
        break;
    case AICA_ARM7_RST:
        if (!from_sh4) {
            printf("ARM7 suicide unimplemented\n");
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }
        break;
    case AICA_SCIRE:
        /*
         * Writing to this register clears interrupts, it's not clear what
         * would happen if it is read from.
         */
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
        break;
    case AICA_SCIPD:
        memcpy(aica->sys_reg + idx, &aica->int_pending, sizeof(uint32_t));
        break;
    case AICA_SCIEB:
        memcpy(aica->sys_reg + idx, &aica->int_enable, sizeof(uint32_t));
        break;
    case AICA_MCIEB:
        memcpy(aica->sys_reg + idx, &aica->int_enable_sh4, sizeof(uint32_t));
        break;
    case AICA_MCIPD:
        memcpy(aica->sys_reg + idx, &aica->int_pending_sh4, sizeof(uint32_t));
        break;
    case AICA_MIDI_INPUT:
        /*
         * The MIDI interface, as far as I know, only exists on development
         * systems and not on retail Dreamcasts.  The value hardcoded below will
         * hopefully convince programs that the MIDI is empty (see the Corlett
         * doc).
         */
        val = (1 << 11) | (1 << 8);
        memcpy(aica->sys_reg + idx, &val, sizeof(uint32_t));
        break;
    case AICA_PLAYPOS:
        LOG_DBG("Reading 0x%08x from AICA_PLAYPOS\n",
                (unsigned)aica->sys_reg[idx]);
        break;
    case AICA_PLAYSTATUS:
        LOG_DBG("Reading 0x%08x from AICA_PLAYSTATUS\n",
                (unsigned)aica->sys_reg[idx]);
        break;

    case AICA_TIMERA_CTRL:
        printf("read AICA_TIMERA_CTRL\n");
        aica_sync_timer(aica, 0);
        aica->sys_reg[AICA_TIMERA_CTRL / 4] =
            ((aica->timers[0].prescale & 0x7) << 8) |
            (aica->timers[0].counter & 0xf);
        break;
    case AICA_TIMERB_CTRL:
        printf("read AICA_TIMERA_CTRL\n");
        aica_sync_timer(aica, 1);
        aica->sys_reg[AICA_TIMERB_CTRL / 4] =
            ((aica->timers[1].prescale & 0x7) << 8) |
            (aica->timers[1].counter & 0xf);
        break;
    case AICA_TIMERC_CTRL:
        printf("read AICA_TIMERA_CTRL\n");
        aica_sync_timer(aica, 2);
        aica->sys_reg[AICA_TIMERC_CTRL / 4] =
            ((aica->timers[2].prescale & 0x7) << 8) |
            (aica->timers[2].counter & 0xf);
        break;
    case AICA_INTREQ:
        break;
    default:
#ifdef AICA_PEDANTIC
        error_set_address(4 * idx);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
#endif
        break;
    }
}

static void
aica_sys_reg_read(struct aica *aica, addr32_t addr,
                  void *out, unsigned n_bytes, bool from_sh4) {
#ifdef INVARANTS
    if (addr <= 0x7fff) {
        error_set_address(addr);
        RAISE_ERROR(ERROR_INTEGRITY);
    }
#endif

    aica_sys_reg_pre_read(aica, addr / 4, from_sh4);

    memcpy(out, ((uint8_t*)aica->sys_reg) + addr, n_bytes);
}

static void
aica_sys_reg_post_write(struct aica *aica, unsigned idx, bool from_sh4) {
    uint32_t val;

    switch (idx * 4) {
    case AICA_MASTER_VOLUME:
        LOG_DBG("Writing 0x%08x to AICA_MASTER_VOLUME\n", (unsigned)val);
        break;
    case AICA_ARM7_RST:
        memcpy(&val, aica->sys_reg + (AICA_ARM7_RST/4), sizeof(val));
        if (from_sh4) {
            arm7_reset(aica->arm7, !(val & 1));
        } else {
            printf("ARM7 suicide unimplemented\n");
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }
        break;
    case AICA_SCIRE:
        memcpy(&val, aica->sys_reg + (AICA_SCIRE/4), sizeof(val));
        if ((aica->int_pending & AICA_INT_TIMA_MASK) & (val & AICA_INT_TIMA_MASK)) {
            printf("clearing timerA interrupt\n");
        }
        aica->int_pending &= ~val;
        aica_update_interrupts(aica);
        break;
    case AICA_MCIRE:
        memcpy(&val, aica->sys_reg + (AICA_MCIRE/4), sizeof(val));
        aica->int_pending_sh4 &= ~val;
        aica_update_interrupts(aica);
        break;
    case AICA_SCIPD:
        /*
         * TODO: Neill Corlett's doc says that interrupt 5 (CPU interrupt) can
         * be manually triggered by writing to bit 5 of this register.
         */
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
        break;
    case AICA_MCIPD:
        /*
         * TODO: You can write to bit 5 (CPU interrupt) to send an interrupt to
         * the SH4.
         */
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
        break;
    case AICA_SCIEB:
        memcpy(&val, aica->sys_reg + (AICA_SCIEB/4), sizeof(val));
        aica->int_enable = val;
        aica_update_interrupts(aica);
        break;
    case AICA_MCIEB:
        memcpy(&val, aica->sys_reg + (AICA_MCIEB/4), sizeof(val));
        if (val & ~AICA_INT_CPU_MASK)
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        aica->int_enable_sh4 = val;
        break;
    case AICA_RINGBUFFER_ADDRESS:
        memcpy(&val, aica->sys_reg + (AICA_RINGBUFFER_ADDRESS/4), sizeof(val));
        aica->ringbuffer_addr = (val & BIT_RANGE(0, 11)) << 11;
        aica->ringbuffer_size = (val & BIT_RANGE(13, 14)) >> 13;
        aica->ringbuffer_bit15 = (bool)(val & (1 << 15));
        LOG_DBG("Writing 0x%08x to AICA_RINGBUFFER_ADDRESS\n", (unsigned)val);
        break;
    case AICA_UNKNOWN_2880:
        memcpy(&val, aica->sys_reg + (AICA_UNKNOWN_2880/4), sizeof(val));
        LOG_DBG("Writing 0x%08x to AICA_UNKNOWN_2880\n", (unsigned)val);
        break;

        /*
         * TODO: there are three timers in the AICA system.
         *
         * The lower byte of the timer register is a counter which increments
         * periodically and raises an interrupt when it overflows.
         *
         * bits 10-8 are the base-2 logarithm of how many samples occur per
         * timer increment.  Ostensibly "samples" refers to audio samples, but
         * I still don't understand AICA well enough to implement that so I
         * didn't implement that.
         */
    case AICA_TIMERA_CTRL:
        memcpy(&val, aica->sys_reg + (AICA_TIMERA_CTRL/4), sizeof(val));
        LOG_DBG("Writing 0x%08x to AICA_TIMERA_CTRL\n", (unsigned)val);
        break;
    case AICA_TIMERB_CTRL:
        memcpy(&val, aica->sys_reg + (AICA_TIMERB_CTRL/4), sizeof(val));
        LOG_DBG("Writing 0x%08x to AICB_TIMERA_CTRL\n", (unsigned)val);
        break;
    case AICA_TIMERC_CTRL:
        memcpy(&val, aica->sys_reg + (AICA_TIMERC_CTRL/4), sizeof(val));
        LOG_DBG("Writing 0x%08x to AICC_TIMERA_CTRL\n", (unsigned)val);
        break;

    case AICA_SCILV0:
        memcpy(&val, aica->sys_reg + (AICA_SCILV0/4), sizeof(val));
        LOG_DBG("Writing 0x%08x to AICA_SCILV0\n", (unsigned)val);
        break;
    case AICA_SCILV1:
        memcpy(&val, aica->sys_reg + (AICA_SCILV1/4), sizeof(val));
        LOG_DBG("Writing 0x%08x to AICA_SCILV1\n", (unsigned)val);
        break;
    case AICA_SCILV2:
        memcpy(&val, aica->sys_reg + (AICA_SCILV2/4), sizeof(val));
        LOG_DBG("Writing 0x%08x to AICA_SCILV2\n", (unsigned)val);
        break;

    case AICA_INTCLEAR:
        memcpy(&val, aica->sys_reg + (AICA_INTCLEAR/4), sizeof(val));
        LOG_DBG("Writing 0x%08x to AICA_INTCLEAR\n", (unsigned)val);
        break;

    case AICA_CHANINFOREQ:
        memcpy(&val, aica->sys_reg + (AICA_CHANINFOREQ/4), sizeof(val));
        LOG_DBG("Writing 0x%08x to AICA_CHANINFOREQ\n", (unsigned)val);
        break;

    default:
#ifdef AICA_PEDANTIC
        error_set_address(4 * idx);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
#endif
        break;
    }
}

static void aica_sys_reg_write(struct aica *aica, addr32_t addr,
                               void const *val_in, unsigned n_bytes,
                               bool from_sh4) {
#ifdef INVARANTS
    if (addr <= 0x7fff) {
        error_set_address(addr);
        RAISE_ERROR(ERROR_INTEGRITY);
    }
#endif

    memcpy(((uint8_t*)aica->sys_reg) + addr, val_in, n_bytes);
    aica_sys_reg_post_write(aica, addr / 4, from_sh4);
}

static void aica_sys_channel_read(struct aica *aica, void *dst,
                                  uint32_t addr, unsigned len) {
    uint32_t addr_first = addr;
    uint32_t addr_last = addr + (len - 1);
    if (addr_first > 0x1fff || addr_last > 0x1fff) {
        error_set_length(len);
        error_set_address(addr);
        RAISE_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }
    if (aica_log_verbose_val) {
        LOG_DBG("AICA CHANNEL DATA: Reading %u bytes from 0x%08x\n",
                len, (unsigned)addr);
    }

    unsigned chan_no = addr / AICA_CHAN_LEN;
    unsigned chan_reg = addr % AICA_CHAN_LEN;

    struct aica_chan *chan = aica->channels + chan_no;
    uint32_t tmp;

    switch (chan_reg / 4) {
    case AICA_CHAN_PLAY_CTRL:
        memcpy(&tmp, chan->raw + AICA_CHAN_PLAY_CTRL, sizeof(tmp));
        tmp &= ~(1 << 15);
        memcpy(chan->raw + AICA_CHAN_PLAY_CTRL, &tmp, sizeof(tmp));
        break;
    }

    memcpy(dst, chan->raw + chan_reg, len);
}

static void aica_dsp_mixer_read(struct aica *aica, void *dst,
                                uint32_t addr, unsigned len) {
    uint32_t addr_first = addr;
    uint32_t addr_last = addr + (len - 1);
    if (addr_first >= 0x2048 || addr_last >= 0x2048 ||
        addr_first <= 0x1fff || addr_last <= 0x1fff) {
        error_set_length(len);
        error_set_address(addr);
        RAISE_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }
    if (aica_log_verbose_val) {
        LOG_DBG("AICA DSP MIXER: Reading %u bytes from 0x%08x\n",
                len, (unsigned)addr);
    }
    memcpy(dst, ((uint8_t*)aica->sys_reg) + addr, len);
}

static void aica_dsp_reg_read(struct aica *aica, void *dst,
                              uint32_t addr, unsigned len) {
    uint32_t addr_first = addr;
    uint32_t addr_last = addr + (len - 1);
    if (addr_first >= 0x8000 || addr_last >= 0x8000 ||
        addr_first < 0x3000 || addr_last < 0x3000) {
        error_set_length(len);
        error_set_address(addr);
        RAISE_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }
    if (aica_log_verbose_val) {
        LOG_DBG("AICA DSP REG: Reading %u bytes from 0x%08x\n",
                len, (unsigned)addr);
    }
    memcpy(dst, ((uint8_t*)aica->sys_reg) + addr, len);
}

static void aica_do_keyon(struct aica *aica) {
    unsigned chan_no;
    for (chan_no = 0; chan_no < AICA_CHAN_COUNT; chan_no++) {
        struct aica_chan *chan = aica->channels + chan_no;
        if (chan->ready_keyon && !chan->playing) {
            chan->playing = true;
            printf("AICA channel %u key-on\n", chan_no);
        } else if (!chan->ready_keyon && chan->playing) {
            chan->playing = false;
            printf("AICA channel %u key-off\n", chan_no);
        }
    }
}

static void aica_chan_playctrl_write(struct aica *aica, unsigned chan_no) {
    uint32_t val;
    struct aica_chan *chan = aica->channels + chan_no;
    memcpy(&val, chan->raw + AICA_CHAN_PLAY_CTRL, sizeof(val));

    chan->ready_keyon = (bool)(val & (1 << 14));
    if (val & (1 << 15))
        aica_do_keyon(aica);
    chan->keyon = (bool)(val & (1 << 15));
}

static void aica_sys_channel_write(struct aica *aica, void const *src,
                                   uint32_t addr, unsigned len) {
    uint32_t addr_first = addr;
    uint32_t addr_last = addr + (len - 1);
    if (addr_first > 0x1fff || addr_last > 0x1fff) {
        error_set_length(len);
        error_set_address(addr);
        RAISE_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }

    unsigned chan_no = addr / AICA_CHAN_LEN;
    unsigned chan_reg = addr % AICA_CHAN_LEN;

    struct aica_chan *chan = aica->channels + chan_no;

    if (aica_log_verbose_val) {
        LOG_DBG("AICA CHANNEL DATA: Writing %u bytes from 0x%08x\n",
                len, (unsigned)addr);
    }
    memcpy(chan->raw + chan_reg, src, len);

    switch (chan_reg / 4) {
    case AICA_CHAN_PLAY_CTRL:
        aica_chan_playctrl_write(aica, chan_no);
        break;
    }
}

static void aica_dsp_mixer_write(struct aica *aica, void const *src,
                                uint32_t addr, unsigned len) {
    uint32_t addr_first = addr;
    uint32_t addr_last = addr + (len - 1);
    if (addr_first >= 0x2048 || addr_last >= 0x2048 ||
        addr_first <= 0x1fff || addr_last <= 0x1fff) {
        error_set_length(len);
        error_set_address(addr);
        RAISE_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }
    if (aica_log_verbose_val) {
        LOG_DBG("AICA DSP MIXER: Writing %u bytes from 0x%08x\n",
                len, (unsigned)addr);
    }
    memcpy(((uint8_t*)aica->sys_reg) + addr, src, len);
}

static void aica_dsp_reg_write(struct aica *aica, void const *src,
                               uint32_t addr, unsigned len) {
    uint32_t addr_first = addr;
    uint32_t addr_last = addr + (len - 1);
    if (addr_first >= 0x8000 || addr_last >= 0x8000 ||
        addr_first < 0x3000 || addr_last < 0x3000) {
        error_set_length(len);
        error_set_address(addr);
        RAISE_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }
    if (aica_log_verbose_val) {
        LOG_DBG("AICA DSP REG: Writing %u bytes from 0x%08x\n",
                len, (unsigned)addr);
    }
    memcpy(((uint8_t*)aica->sys_reg) + addr, src, len);
}

static uint32_t aica_sys_read_32(addr32_t addr, void *ctxt) {
    struct aica *aica = (struct aica*)ctxt;
    bool from_sh4 = (addr & 0x00f00000) == 0x00700000;

    addr &= AICA_SYS_MASK;

    if (addr < 0x1fff) {
        // Channel registers
        uint32_t ret;
        aica_sys_channel_read(aica, &ret, addr, sizeof(ret));
        return ret;
    }

    if (addr <= 0x2044) {
        // DSP mixer
        uint32_t ret;
        aica_dsp_mixer_read(aica, &ret, addr, sizeof(ret));
        return ret;
    }

    if (addr >= 0x3000 && addr <= 0x7fff) {
        // DSP registers
        uint32_t ret;
        aica_dsp_reg_read(aica, &ret, addr, sizeof(ret));
        return ret;
    }

    if (addr >= 0x2800 && addr <= 0x2fff) {
        uint32_t ret;
        aica_sys_reg_read(aica, addr, &ret, sizeof(ret), from_sh4);
        return ret;
    }

    error_set_address(addr);
    error_set_length(4);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void aica_sys_write_32(addr32_t addr, uint32_t val, void *ctxt) {
    struct aica *aica = (struct aica*)ctxt;
    bool from_sh4 = (addr & 0x00f00000) == 0x00700000;

    addr &= AICA_SYS_MASK;

    if (addr <= 0x1fff) {
        // channel data
        aica_sys_channel_write(aica, &val, addr, sizeof(val));
        return;
    }

    if (addr <= 0x2044) {
        // DSP mixer
        aica_dsp_mixer_write(aica, &val, addr, sizeof(val));
        return;
    }

    if (addr >= 0x3000 && addr <= 0x7fff) {
        // DSP registers
        aica_dsp_reg_write(aica, &val, addr, sizeof(val));
        return;
    }

    if (addr >= 0x2800 && addr <= 0x2fff) {
        aica_sys_reg_write(aica, addr, &val, sizeof(val), from_sh4);
    } else {
        error_set_address(addr);
        error_set_length(4);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

static uint16_t aica_sys_read_16(addr32_t addr, void *ctxt) {
    struct aica *aica = (struct aica*)ctxt;
    bool from_sh4 = (addr & 0x00f00000) == 0x00700000;

    addr &= AICA_SYS_MASK;

    if (addr < 0x1fff) {
        // Channel registers
        uint16_t ret;
        aica_sys_channel_read(aica, &ret, addr, sizeof(ret));
        return ret;
    }

    if (addr <= 0x2044) {
        // DSP mixer
        uint16_t ret;
        aica_dsp_mixer_read(aica, &ret, addr, sizeof(ret));
        return ret;
    }

    if (addr >= 0x3000 && addr <= 0x7fff) {
        // DSP registers
        uint16_t ret;
        aica_dsp_reg_read(aica, &ret, addr, sizeof(ret));
        return ret;
    }

    if (addr >= 0x2800 && addr <= 0x2fff) {
        uint16_t ret;
        aica_sys_reg_read(aica, addr, &ret, sizeof(ret), from_sh4);
        return ret;
    }

    error_set_address(addr);
    error_set_length(2);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void aica_sys_write_16(addr32_t addr, uint16_t val, void *ctxt) {
    struct aica *aica = (struct aica*)ctxt;
    bool from_sh4 = (addr & 0x00f00000) == 0x00700000;

    addr &= AICA_SYS_MASK;

    if (addr <= 0x1fff) {
        // channel data
        aica_sys_channel_write(aica, &val, addr, sizeof(val));
        return;
    }

    if (addr <= 0x2044) {
        // DSP mixer
        aica_dsp_mixer_write(aica, &val, addr, sizeof(val));
        return;
    }

    if (addr >= 0x3000 && addr <= 0x7fff) {
        // DSP registers
        aica_dsp_reg_write(aica, &val, addr, sizeof(val));
        return;
    }

    if (addr >= 0x2800 && addr <= 0x2fff) {
        aica_sys_reg_write(aica, addr, &val, sizeof(val), from_sh4);
    } else {
        error_set_address(addr);
        error_set_length(2);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

static uint8_t aica_sys_read_8(addr32_t addr, void *ctxt) {
    struct aica *aica = (struct aica*)ctxt;
    bool from_sh4 = (addr & 0x00f00000) == 0x00700000;

    addr &= AICA_SYS_MASK;

    if (addr < 0x1fff) {
        // Channel registers
        uint8_t ret;
        aica_sys_channel_read(aica, &ret, addr, sizeof(ret));
        return ret;
    }

    if (addr <= 0x2044) {
        // DSP mixer
        uint8_t ret;
        aica_dsp_mixer_read(aica, &ret, addr, sizeof(ret));
        return ret;
    }

    if (addr >= 0x3000 && addr <= 0x7fff) {
        // DSP registers
        uint8_t ret;
        aica_dsp_reg_read(aica, &ret, addr, sizeof(ret));
        return ret;
    }

    if (addr >= 0x2800 && addr <= 0x2fff) {
        uint8_t ret;
        aica_sys_reg_read(aica, addr, &ret, sizeof(ret), from_sh4);
        return ret;
    }

    error_set_address(addr);
    error_set_length(1);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void aica_sys_write_8(addr32_t addr, uint8_t val, void *ctxt) {
    struct aica *aica = (struct aica*)ctxt;
    bool from_sh4 = (addr & 0x00f00000) == 0x00700000;

    addr &= AICA_SYS_MASK;

    if (addr <= 0x1fff) {
        // channel data
        aica_sys_channel_write(aica, &val, addr, sizeof(val));
        return;
    }

    if (addr <= 0x2044) {
        // DSP mixer
        aica_dsp_mixer_write(aica, &val, addr, sizeof(val));
        return;
    }

    if (addr >= 0x3000 && addr <= 0x7fff) {
        // DSP registers
        aica_dsp_reg_write(aica, &val, addr, sizeof(val));
        return;
    }

    if (addr >= 0x2800 && addr <= 0x2fff) {
        aica_sys_reg_write(aica, addr, &val, sizeof(val), from_sh4);
    } else {
        error_set_address(addr);
        error_set_length(1);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

static void aica_update_interrupts(struct aica *aica) {
    /*
     * this is really just a placeholder in case I ever want to put some logging
     * in or something, this function doesn't actually need to be here.
     */
    printf("FIQ: aica->int_enable is now 0x%08x\n", (unsigned)aica->int_enable);
}

static bool aica_check_irq(void *ctxt) {
    struct aica *aica = (struct aica*)ctxt;

    bool ret_val = (bool)(aica->int_enable & aica->int_pending & AICA_ALL_INT_MASK);
    /* if (ret_val) */
    /*     printf("FIQ!\n"); */
    /* else if (aica->int_pending) */
    /*     printf("pending FIQ!\n"); */
    return ret_val;
    /* return (bool)(aica->int_enable & aica->int_pending & AICA_ALL_INT_MASK); */
}

static void aica_unsched_all_timers(struct aica *aica) {
    unsigned idx;
    for (idx = 0; idx < 3; idx++)
        aica_unsched_timer(aica, idx);
}

static void aica_sched_all_timers(struct aica *aica) {
    unsigned idx;
    for (idx = 0; idx < 3; idx++)
        aica_sched_timer(aica, idx);
}

static void aica_unsched_timer(struct aica *aica, unsigned tim_idx) {
    struct aica_timer *timer = aica->timers + tim_idx;
    if (timer->scheduled) {
        timer->scheduled = false;
        cancel_event(aica->clk, &timer->evt);
    }
}

static void aica_sched_timer(struct aica *aica, unsigned tim_idx) {
    struct aica_timer *timer = aica->timers + tim_idx;

    if (timer->scheduled)
        return;

    struct SchedEvent *evt = &timer->evt;
    struct dc_clock *clk = aica->clk;

    // TODO: implement the prescale
    unsigned samples_to_go = 256 - timer->counter;
    dc_cycle_stamp_t clk_ticks = TICKS_PER_SAMPLE * samples_to_go;

    evt->when = clock_cycle_stamp(clk) + clk_ticks;

    sched_event(aica->clk, evt);

    timer->scheduled = true;
}

/*
 * TODO: the most accurate way to handle the integer rounding error would be to
 * always evaluate the current sample based on the total number of clock ticks
 * that have occured since the emulator began.
 */
static void aica_sync_timer(struct aica *aica, unsigned tim_idx) {
    struct aica_timer *timer = aica->timers + tim_idx;
    dc_cycle_stamp_t cur_stamp = clock_cycle_stamp(aica->clk);
    dc_cycle_stamp_t delta = cur_stamp - timer->stamp_last_sync;
    unsigned sample_delta = delta / TICKS_PER_SAMPLE;

    if (sample_delta) {
        timer->counter += sample_delta;
        timer->counter %= 256;
    }

    timer->stamp_last_sync = cur_stamp;
}

static void
aica_timer_a_handler(struct SchedEvent *evt) {
    struct aica *aica = (struct aica*)evt->arg_ptr;
    aica_timer_handler(aica, 0);
}

static void
aica_timer_b_handler(struct SchedEvent *evt) {
    struct aica *aica = (struct aica*)evt->arg_ptr;
    aica_timer_handler(aica, 1);
}

static void
aica_timer_c_handler(struct SchedEvent *evt) {
    struct aica *aica = (struct aica*)evt->arg_ptr;
    aica_timer_handler(aica, 2);
}

static void aica_timer_handler(struct aica *aica, unsigned tim_idx) {
    struct aica_timer *timer = aica->timers + tim_idx;

    timer->scheduled = false;
    aica_sync_timer(aica, tim_idx);

    if (timer->counter) {
        LOG_ERROR("timer->counter is %u\n", timer->counter);
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    switch (tim_idx) {
    case 0:
        aica->int_pending |= AICA_INT_TIMA_MASK;
        aica->sys_reg[AICA_INTREQ/4] = aica_read_sci(aica, 6);
        break;
    case 1:
        aica->int_pending |= AICA_INT_TIMB_MASK;
        break;
    case 2:
        aica->int_pending |= AICA_INT_TIMC_MASK;
        break;
    }

    aica_sched_timer(aica, tim_idx);
}

static unsigned aica_read_sci(struct aica *aica, unsigned bit) {
    if (bit >= 8)
        RAISE_ERROR(ERROR_INTEGRITY);
    return (aica->sys_reg[AICA_SCILV2 / 4] << 2) |
        (aica->sys_reg[AICA_SCILV1 / 4] << 1) |
        aica->sys_reg[AICA_SCILV0 / 4];
}
