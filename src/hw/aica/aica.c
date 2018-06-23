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

#define AICA_MASTER_VOLUME 0x2800

#define AICA_ARM7_RST 0x2c00

#define AICA_RINGBUFFER_ADDRESS 0x2804

#define AICA_UNKNOWN_2880 0x2880

#define AICA_TIMERA_CTRL 0x2890
#define AICA_TIMERB_CTRL 0x2894
#define AICA_TIMERC_CTRL 0x2898

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

static uint32_t aica_sys_reg_read(struct aica *aica, addr32_t addr,
                                  bool from_sh4);
static void aica_sys_reg_write(struct aica *aica, addr32_t addr,
                               uint32_t val, bool from_sh4);

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

static bool aica_check_irq(void *ctxt);

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

void aica_init(struct aica *aica, struct arm7 *arm7) {
    memset(aica, 0, sizeof(*aica));

    aica->arm7 = arm7;
    arm7->check_irq = aica_check_irq;
    arm7->check_irq_dat = aica;

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

static uint32_t aica_sys_reg_read(struct aica *aica, addr32_t addr, bool from_sh4) {
#ifdef INVARANTS
    if (addr <= 0x7fff) {
        error_set_address(addr);
        RAISE_ERROR(ERROR_INTEGRITY);
    }
#endif

    switch (addr) {
    case AICA_MASTER_VOLUME:
        // Neill Corlett's AICA notes say this is always 16 when you read from it
        return 16;
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
        return aica->int_pending;
    case AICA_SCIEB:
        return aica->int_enable;
    case AICA_MCIEB:
        return aica->int_enable_sh4;
    case AICA_MCIPD:
        return aica->int_pending_sh4;
    case AICA_MIDI_INPUT:
        /*
         * The MIDI interface, as far as I know, only exists on development
         * systems and not on retail Dreamcasts.  The value hardcoded below will
         * hopefully convince programs that the MIDI is empty (see the Corlett
         * doc).
         */
        return  (1 << 11) | (1 << 8);
    default:
#ifdef AICA_PEDANTIC
        error_set_address(addr);
        error_set_length(4);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
#endif
        break;
    }

    return aica->sys_reg[addr / 4];
}

static void aica_sys_reg_write(struct aica *aica, addr32_t addr,
                               uint32_t val, bool from_sh4) {
#ifdef INVARANTS
    if (addr <= 0x7fff) {
        error_set_address(addr);
        RAISE_ERROR(ERROR_INTEGRITY);
    }
#endif

    switch (addr) {
    case AICA_MASTER_VOLUME:
        LOG_DBG("Writing 0x%08x to AICA_MASTER_VOLUME\n", (unsigned)val);
        break;
    case AICA_ARM7_RST:
        if (from_sh4) {
            arm7_reset(aica->arm7, !(val & 1));
        } else {
            printf("ARM7 suicide unimplemented\n");
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }
        break;
    case AICA_SCIRE:
        aica->int_pending &= ~val;
        aica_update_interrupts(aica);
        break;
    case AICA_MCIRE:
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
        aica->int_enable = val;
        aica_update_interrupts(aica);
        break;
    case AICA_MCIEB:
        if (val & ~AICA_INT_CPU_MASK)
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        aica->int_enable_sh4 = val;
        break;
    case AICA_RINGBUFFER_ADDRESS:
        aica->ringbuffer_addr = (val & BIT_RANGE(0, 11)) << 11;
        aica->ringbuffer_size = (val & BIT_RANGE(13, 14)) >> 13;
        aica->ringbuffer_bit15 = (bool)(val & (1 << 15));
        LOG_DBG("Writing 0x%08x to AICA_RINGBUFFER_ADDRESS\n", (unsigned)val);
        break;
    case AICA_UNKNOWN_2880:
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
        LOG_DBG("Writing 0x%08x to AICA_TIMERA_CTRL\n", (unsigned)val);
        break;
    case AICA_TIMERB_CTRL:
        LOG_DBG("Writing 0x%08x to AICB_TIMERA_CTRL\n", (unsigned)val);
        break;
    case AICA_TIMERC_CTRL:
        LOG_DBG("Writing 0x%08x to AICC_TIMERA_CTRL\n", (unsigned)val);
        break;

    default:
#ifdef AICA_PEDANTIC
        error_set_address(addr);
        error_set_length(4);
        error_set_value(val);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
#endif
        break;
    }
    aica->sys_reg[addr / 4] = val;
}

static uint32_t aica_sys_read_32(addr32_t addr, void *ctxt) {
    struct aica *aica = (struct aica*)ctxt;
    bool from_sh4 = (addr & 0x00f00000) == 0x00700000;

    addr &= AICA_SYS_MASK;

    if (addr < 0x1fff) {
        // Channel registers
        uint32_t val = aica->sys_reg[addr / 4];
        if (aica_log_verbose_val) {
            LOG_DBG("AICA CHANNEL DATA: Reading 0x%08x from 0x%04x\n",
                    (unsigned)val, (unsigned)addr);
        }
        return val;
    }

    if (addr <= 0x2044) {
        // DSP mixer
        uint32_t val = aica->sys_reg[addr / 4];
        if (aica_log_verbose_val) {
            LOG_DBG("AICA DSP MIXER: Reading 0x%08x from 0x%04x\n",
                    (unsigned)val, (unsigned)addr);
        }
        return val;
    }

    if (addr >= 0x3000 && addr <= 0x7fff) {
        // DSP registers
        uint32_t val = aica->sys_reg[addr / 4];
        if (aica_log_verbose_val) {
            LOG_DBG("AICA DSP: Reading 0x%08x from 0x%04x\n",
                    (unsigned)val, (unsigned)addr);
        }
        return val;
    }

    if (addr >= 0x2800 && addr <= 0x2fff)
        return aica_sys_reg_read(aica, addr, from_sh4);

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
        aica->sys_reg[addr / 4] = val;
        if (aica_log_verbose_val) {
            LOG_DBG("AICA CHANNEL DATA: Writing 0x%08x to 0x%04x\n",
                    (unsigned)val, (unsigned)addr);
        }
        return;
    }

    if (addr <= 0x2044) {
        // DSP mixer
        aica->sys_reg[addr / 4] = val;
        if (aica_log_verbose_val) {
            LOG_DBG("AICA DSP MIXER: Writing 0x%08x to 0x%04x\n",
                    (unsigned)val, (unsigned)addr);
        }
        return;
    }

    if (addr >= 0x3000 && addr <= 0x7fff) {
        // DSP registers
        aica->sys_reg[addr / 4] = val;
        if (aica_log_verbose_val) {
            LOG_DBG("AICA DSP: Writing 0x%08x to 0x%04x\n",
                    (unsigned)val, (unsigned)addr);
        }
        return;
    }

    if (addr >= 0x2800 && addr <= 0x2fff) {
        aica_sys_reg_write(aica, addr, val, from_sh4);
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

    if (addr >= 0x2800 && addr <= 0x2fff)
        return aica_sys_reg_read(aica, addr, from_sh4);

    error_set_address(addr);
    error_set_length(2);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void aica_sys_write_16(addr32_t addr, uint16_t val, void *ctxt) {
    struct aica *aica = (struct aica*)ctxt;
    bool from_sh4 = (addr & 0x00f00000) == 0x00700000;

    addr &= AICA_SYS_MASK;

    if (addr >= 0x2800 && addr <= 0x2fff) {
        aica_sys_reg_write(aica, addr, val, from_sh4);
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

    if (addr >= 0x2800 && addr <= 0x2fff)
        return aica_sys_reg_read(aica, addr, from_sh4);

    error_set_address(addr);
    error_set_length(1);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void aica_sys_write_8(addr32_t addr, uint8_t val, void *ctxt) {
    struct aica *aica = (struct aica*)ctxt;
    bool from_sh4 = (addr & 0x00f00000) == 0x00700000;

    addr &= AICA_SYS_MASK;

    if (addr >= 0x2800 && addr <= 0x2fff) {
        aica_sys_reg_write(aica, addr, val, from_sh4);
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
}

static bool aica_check_irq(void *ctxt) {
    struct aica *aica = (struct aica*)ctxt;

    return (bool)(aica->int_enable & aica->int_pending & AICA_ALL_INT_MASK);
}
