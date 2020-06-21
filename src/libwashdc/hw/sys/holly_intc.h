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

#ifndef HOLLY_INTC_H_
#define HOLLY_INTC_H_

#include <stdint.h>

#include "washdc/types.h"
#include "sys_block.h"

enum HollyExtInt {
    HOLLY_EXT_INT_GDROM,
    HOLLY_EXT_INT_AICA,

    HOLLY_EXT_INT_COUNT
};
typedef enum HollyExtInt HollyExtInt;

enum HollyNrmInt {
    HOLLY_NRM_INT_HBLANK,
    HOLLY_NRM_INT_VBLANK_OUT,
    HOLLY_NRM_INT_VBLANK_IN,
    HOLLY_NRM_INT_ISTNRM_PVR_PUNCH_THROUGH_COMPLETE,
    HOLLY_REG_ISTNRM_PVR_TRANS_MOD_COMPLETE,
    HOLLY_REG_ISTNRM_PVR_TRANS_COMPLETE,
    HOLLY_REG_ISTNRM_PVR_OPAQUE_MOD_COMPLETE,
    HOLLY_REG_ISTNRM_PVR_OPAQUE_COMPLETE,
    HOLLY_REG_ISTNRM_PVR_RENDER_COMPLETE,

    HOLLY_MAPLE_ISTNRM_DMA_COMPLETE,

    HOLLY_REG_ISTNRM_CHANNEL2_DMA_COMPLETE,

    HOLLY_REG_ISTNRM_AICA_DMA_COMPLETE,

    HOLLY_REG_ISTNRM_PVR_YUV_COMPLETE,

    HOLLY_REG_ISTNRM_GDROM_DMA_COMPLETE,

    HOLLY_REG_ISTNRM_SORT_DMA_COMPLETE,

    HOLLY_NRM_INT_COUNT
};
typedef enum HollyNrmInt HollyNrmInt;

// should be hooked up to the sh4 intc's irl line function
int holly_intc_irl_line_fn(void *ctx);

// when the punch-through polygon list has been successfully input
#define HOLLY_REG_ISTNRM_PVR_PUNCH_THROUGH_COMPLETE_SHIFT 21
#define HOLLY_REG_ISTNRM_PVR_PUNCH_THROUGH_COMPLETE_MASK \
    (1 << HOLLY_REG_ISTNRM_PVR_PUNCH_THROUGH_COMPLETE_SHIFT)

#define HOLLY_REG_ISTNRM_SORT_DMA_COMPLETE_SHIFT 20
#define HOLLY_REG_ISTNRM_SORT_DMA_COMPLETE_MASK     \
    (1 << HOLLY_REG_ISTNRM_SORT_DMA_COMPLETE_SHIFT)

#define HOLLY_REG_ISTNRM_CHANNEL2_DMA_COMPLETE_SHIFT 19
#define HOLLY_REG_ISTNRM_CHANNEL2_DMA_COMPLETE_MASK \
    (1 << HOLLY_REG_ISTNRM_CHANNEL2_DMA_COMPLETE_SHIFT)

#define HOLLY_REG_ISTNRM_AICA_DMA_COMPLETE_SHIFT 15
#define HOLLY_REG_ISTNRM_AICA_DMA_COMPLETE_MASK \
    (1 << HOLLY_REG_ISTNRM_AICA_DMA_COMPLETE_SHIFT)

#define HOLLY_REG_ISTNRM_GDROM_DMA_COMPLETE_SHIFT 14
#define HOLLY_REG_ISTNRM_GDROM_DMA_COMPLETE_MASK    \
    (1 << HOLLY_REG_ISTNRM_GDROM_DMA_COMPLETE_SHIFT)

#define HOLLY_REG_ISTNRM_MAPLE_DMA_COMPLETE_SHIFT 12
#define HOLLY_REG_ISTNRM_MAPLE_DMA_COMPLETE_MASK \
    (1 << HOLLY_REG_ISTNRM_MAPLE_DMA_COMPLETE_SHIFT)

// when the transparent polygon modifier list has been successfully input
#define HOLLY_REG_ISTNRM_PVR_TRANS_MOD_COMPLETE_SHIFT 10
#define HOLLY_REG_ISTNRM_PVR_TRANS_MOD_COMPLETE_MASK \
    (1 << HOLLY_REG_ISTNRM_PVR_TRANS_MOD_COMPLETE_SHIFT)

// when the transparent polygon list has been successfully input
#define HOLLY_REG_ISTNRM_PVR_TRANS_COMPLETE_SHIFT 9
#define HOLLY_REG_ISTNRM_PVR_TRANS_COMPLETE_MASK \
    (1 << HOLLY_REG_ISTNRM_PVR_TRANS_COMPLETE_SHIFT)

// when the opaque polygon modifier list has been successfully input
#define HOLLY_REG_ISTNRM_PVR_OPAQUE_MOD_COMPLETE_SHIFT 8
#define HOLLY_REG_ISTNRM_PVR_OPAQUE_MOD_COMPLETE_MASK \
    (1 << HOLLY_REG_ISTNRM_PVR_OPAQUE_MOD_COMPLETE_SHIFT)

// when the opaque polygon list has been successfully input
#define HOLLY_REG_ISTNRM_PVR_OPAQUE_COMPLETE_SHIFT 7
#define HOLLY_REG_ISTNRM_PVR_OPAQUE_COMPLETE_MASK \
    (1 << HOLLY_REG_ISTNRM_PVR_OPAQUE_COMPLETE_SHIFT)

#define HOLLY_REG_ISTNRM_PVR_YUV_COMPLETE_SHIFT 6
#define HOLLY_REG_ISTNRM_PVR_YUV_COMPLETE_MASK  \
    (1 << HOLLY_REG_ISTNRM_PVR_YUV_COMPLETE_SHIFT)

#define HOLLY_REG_ISTNRM_HBLANK_SHIFT 5
#define HOLLY_REG_ISTNRM_HBLANK_MASK (1 << HOLLY_REG_ISTNRM_HBLANK_SHIFT)

#define HOLLY_REG_ISTNRM_VBLANK_OUT_SHIFT 4
#define HOLLY_REG_ISTNRM_VBLANK_OUT_MASK  (1 << HOLLY_REG_ISTNRM_VBLANK_OUT_SHIFT)

#define HOLLY_REG_ISTNRM_VBLANK_IN_SHIFT 3
#define HOLLY_REG_ISTNRM_VBLANK_IN_MASK (1 << HOLLY_REG_ISTNRM_VBLANK_IN_SHIFT)

#define HOLLY_REG_ISTNRM_PVR_RENDER_COMPLETE_SHIFT 2
#define HOLLY_REG_ISTNRM_PVR_RENDER_COMPLETE_MASK \
    (1 << HOLLY_REG_ISTNRM_PVR_RENDER_COMPLETE_SHIFT)

#define HOLLY_REG_ISTEXT_GDROM_SHIFT 0
#define HOLLY_REG_ISTEXT_GDROM_MASK (1 << HOLLY_REG_ISTEXT_GDROM_SHIFT)

#define HOLLY_REG_ISTEXT_AICA_SHIFT 1
#define HOLLY_REG_ISTEXT_AICA_MASK (1 << HOLLY_REG_ISTEXT_AICA_SHIFT)

/*
 * these functionse should not be called from within an sh4 instruction handler
 * or from within a function that could possibly get called from within an sh4
 * instruction hander.  Best bet is to schedule an event and call it from there.
 */
void holly_raise_ext_int(HollyExtInt int_type);
void holly_raise_nrm_int(HollyNrmInt int_type);

void holly_clear_ext_int(HollyExtInt int_type);
void holly_clear_nrm_int(HollyNrmInt int_type);

uint32_t holly_reg_istnrm_mmio_read(struct mmio_region_sys_block *region,
                                    unsigned idx, void *ctxt);
void holly_reg_istnrm_mmio_write(struct mmio_region_sys_block *region,
                                 unsigned idx, uint32_t val, void *ctxt);
uint32_t holly_reg_istext_mmio_read(struct mmio_region_sys_block *region,
                                    unsigned idx, void *ctxt);
void holly_reg_istext_mmio_write(struct mmio_region_sys_block *region,
                                 unsigned idx, uint32_t val, void *ctxt);
uint32_t holly_reg_isterr_mmio_read(struct mmio_region_sys_block *region,
                                    unsigned idx, void *ctxt);
void holly_reg_isterr_mmio_write(struct mmio_region_sys_block *region,
                                 unsigned idx, uint32_t val, void *ctxt);
uint32_t holly_reg_iml2nrm_mmio_read(struct mmio_region_sys_block *region,
                                     unsigned idx, void *ctxt);
void holly_reg_iml2nrm_mmio_write(struct mmio_region_sys_block *region,
                                  unsigned idx, uint32_t val, void *ctxt);
uint32_t holly_reg_iml2err_mmio_read(struct mmio_region_sys_block *region,
                                     unsigned idx, void *ctxt);
void holly_reg_iml2err_mmio_write(struct mmio_region_sys_block *region,
                                  unsigned idx, uint32_t val, void *ctxt);
uint32_t holly_reg_iml2ext_mmio_read(struct mmio_region_sys_block *region,
                                     unsigned idx, void *ctxt);
void holly_reg_iml2ext_mmio_write(struct mmio_region_sys_block *region,
                                  unsigned idx, uint32_t val, void *ctxt);
uint32_t holly_reg_iml4nrm_mmio_read(struct mmio_region_sys_block *region,
                                     unsigned idx, void *ctxt);
void holly_reg_iml4nrm_mmio_write(struct mmio_region_sys_block *region,
                                  unsigned idx, uint32_t val, void *ctxt);
uint32_t holly_reg_iml4err_mmio_read(struct mmio_region_sys_block *region,
                                     unsigned idx, void *ctxt);
void holly_reg_iml4err_mmio_write(struct mmio_region_sys_block *region,
                                  unsigned idx, uint32_t val, void *ctxt);
uint32_t holly_reg_iml4ext_mmio_read(struct mmio_region_sys_block *region,
                                     unsigned idx, void *ctxt);
void holly_reg_iml4ext_mmio_write(struct mmio_region_sys_block *region,
                                  unsigned idx, uint32_t val, void *ctxt);
uint32_t holly_reg_iml6nrm_mmio_read(struct mmio_region_sys_block *region,
                                     unsigned idx, void *ctxt);
void holly_reg_iml6nrm_mmio_write(struct mmio_region_sys_block *region,
                                  unsigned idx, uint32_t val, void *ctxt);
uint32_t holly_reg_iml6err_mmio_read(struct mmio_region_sys_block *region,
                                     unsigned idx, void *ctxt);
void holly_reg_iml6err_mmio_write(struct mmio_region_sys_block *region,
                                  unsigned idx, uint32_t val, void *ctxt);
uint32_t holly_reg_iml6ext_mmio_read(struct mmio_region_sys_block *region,
                                     unsigned idx, void *ctxt);
void holly_reg_iml6ext_mmio_write(struct mmio_region_sys_block *region,
                                  unsigned idx, uint32_t val, void *ctxt);
#endif
