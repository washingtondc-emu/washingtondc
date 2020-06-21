/*******************************************************************************
 *
 * Copyright 2017-2020 snickerbockers
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

#ifndef SH4_DMAC_H_
#define SH4_DMAC_H_

#include <stdbool.h>

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

    /*
     * this is set to true whenever the corresponding chcr is read while its TE
     * bit is set.  TE will only be cleared if software writes 0 to the TE bit
     * while dma_ack is set.
     */
    bool dma_ack[4];

    /*
     * while we're waiting on a DMA xfer to end, sar_pending holds the final
     * value of sar that will be written after the xfer completes.
     */
    reg32_t sar_pending[4];

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

void sh4_dmac_transfer_words(Sh4 *sh4, addr32_t transfer_src,
                             addr32_t transfer_dst, size_t n_words);

// perform a DMA transfer using channel 2's settings
void sh4_dmac_channel2(Sh4 *sh4, addr32_t transfer_dst, unsigned n_bytes);

void sh4_dmac_init(Sh4 *sh4);
void sh4_dmac_cleanup(Sh4 *sh4);

#endif
