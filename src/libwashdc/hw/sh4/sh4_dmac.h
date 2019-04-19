/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2019 snickerbockers
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

#ifndef SH4_DMAC_H_
#define SH4_DMAC_H_

struct Sh4;
#include "washdc/types.h"

struct sh4_dmac {
    /*
     * 1 register per channel.  Channel 0 is inaccessible to guest programs and
     * therefore practically non-existant, but I still include it here for
     * posterity's sake.  Plus, by including it the indices of the registers
     * match the channel numbers.
     */
    reg32_t sar[4];
    reg32_t dar[4];
    reg32_t dmatcr[4];
    reg32_t chcr[4];

    reg32_t dmaor;
};

sh4_reg_val
sh4_dmac_sar_reg_read_handler(Sh4 *sh4,
                              struct Sh4MemMappedReg const *reg_info);
void sh4_dmac_sar_reg_write_handler(Sh4 *sh4,
                                    struct Sh4MemMappedReg const *reg_info,
                                    sh4_reg_val val);
sh4_reg_val
sh4_dmac_dar_reg_read_handler(Sh4 *sh4,
                              struct Sh4MemMappedReg const *reg_info);
void sh4_dmac_dar_reg_write_handler(Sh4 *sh4,
                                    struct Sh4MemMappedReg const *reg_info,
                                    sh4_reg_val val);
sh4_reg_val sh4_dmac_dmatcr_reg_read_handler(Sh4 *sh4,
                                             struct Sh4MemMappedReg const *reg_info);
void sh4_dmac_dmatcr_reg_write_handler(Sh4 *sh4,
                                       struct Sh4MemMappedReg const *reg_info,
                                       sh4_reg_val val);
sh4_reg_val
sh4_dmac_chcr_reg_read_handler(Sh4 *sh4,
                               struct Sh4MemMappedReg const *reg_info);
void sh4_dmac_chcr_reg_write_handler(Sh4 *sh4,
                                     struct Sh4MemMappedReg const *reg_info,
                                     sh4_reg_val val);
sh4_reg_val
sh4_dmac_dmaor_reg_read_handler(Sh4 *sh4,
                                struct Sh4MemMappedReg const *reg_info);
void sh4_dmac_dmaor_reg_write_handler(Sh4 *sh4,
                                      struct Sh4MemMappedReg const *reg_info,
                                      sh4_reg_val val);

/*
 * perform a DMA transfer from some external device to memory.
 * This completes the transfer immediately instead of
 * modeling the cycle-steal/burst transfer characteristics.
 *
 * this function does not raise any interrupts.
 */
void sh4_dmac_transfer_to_mem(Sh4 *sh4, addr32_t transfer_dst, size_t unit_sz,
                              size_t n_units, void const *dat);

/*
 * perform a DMA transfer to some external device from memory.
 * This completes the transfer immediately instead of
 * modeling the cycle-steal/burst transfer characteristics.
 *
 * this function does not raise any interrupts.
 */
void sh4_dmac_transfer_from_mem(Sh4 *sh4, addr32_t transfer_src, size_t unit_sz,
                                size_t n_units, void *dat);

void sh4_dmac_transfer(Sh4 *sh4, addr32_t transfer_src,
                       addr32_t transfer_dst, size_t n_bytes);

// perform a DMA transfer using channel 2's settings
void sh4_dmac_channel2(Sh4 *sh4, addr32_t transfer_dst, unsigned n_bytes);

#endif
