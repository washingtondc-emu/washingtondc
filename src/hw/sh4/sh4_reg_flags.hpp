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

#ifndef SH4_REG_FLAGS_HPP_
#define SH4_REG_FLAGS_HPP_

/*******************************************************************************
 *
 * SH4 status register
 *
 ******************************************************************************/

// true/false condition or carry/borrow bit
static const unsigned SH4_SR_FLAG_T_SHIFT = 0;
static const unsigned SH4_SR_FLAG_T_MASK = 1 << SH4_SR_FLAG_T_SHIFT;

// saturation operation for MAC instructions
static const unsigned SH4_SR_FLAG_S_SHIFT = 1;
static const unsigned SH4_SR_FLAG_S_MASK = 1 << SH4_SR_FLAG_S_SHIFT;

// interrupt mask level
static const unsigned SH4_SR_IMASK_SHIFT = 4;
static const unsigned SH4_SR_IMASK_MASK = 0xf << SH4_SR_IMASK_SHIFT;

static const unsigned SH4_SR_Q_SHIFT = 8;
static const unsigned SH4_SR_Q_MASK = 1 << SH4_SR_Q_SHIFT;

static const unsigned SH4_SR_M_SHIFT = 9;
static const unsigned SH4_SR_M_MASK = 1 << SH4_SR_M_SHIFT;

// FPU disable bit
static const unsigned SH4_SR_FD_SHIFT = 15;
static const unsigned SH4_SR_FD_MASK = 1 << SH4_SR_FD_SHIFT;

// IRQ mask (1 == masked)
static const unsigned SH4_SR_BL_SHIFT = 28;
static const unsigned SH4_SR_BL_MASK = 1 << SH4_SR_BL_SHIFT;

// general register bank switch
static const unsigned SH4_SR_RB_SHIFT = 29;
static const unsigned SH4_SR_RB_MASK = 1 << SH4_SR_RB_SHIFT;

// processor mode (0 = user, 1 = priveleged)
static const unsigned SH4_SR_MD_SHIFT = 30;
static const unsigned SH4_SR_MD_MASK = 1 << SH4_SR_MD_SHIFT;

// floating-point rounding mode
static const unsigned SH4_FPSCR_RM_SHIFT = 0;
static const unsigned SH4_FPSCR_RM_MASK = 3 << SH4_FPSCR_RM_SHIFT;

/*******************************************************************************
 *
 * SH4 floating-point status/control register
 *
 ******************************************************************************/

// FPU exception flags
static const unsigned SH4_FPSCR_FLAG_SHIFT = 2;
static const unsigned SH4_FPSCR_FLAG_MASK = 0x1f << SH4_FPSCR_FLAG_SHIFT;

// FPU exception enable
static const unsigned SH4_FPSCR_ENABLE_SHIFT = 7;
static const unsigned SH4_FPSCR_ENABLE_MASK = 0x1f << SH4_FPSCR_FLAG_SHIFT;

// FPU exception cause
static const unsigned SH4_FPSCR_CAUSE_SHIFT = 12;
static const unsigned SH4_FPSCR_CAUSE_MASK = 0x1f << SH4_FPSCR_CAUSE_SHIFT;

// FPU Denormalization mode
static const unsigned SH4_FPSCR_DN_SHIFT = 18;
static const unsigned SH4_FPSCR_DN_MASK = 1 << SH4_FPSCR_DN_SHIFT;

// FPU Precision mode
static const unsigned SH4_FPSCR_PR_SHIFT = 19;
static const unsigned SH4_FPSCR_PR_MASK = 1 << SH4_FPSCR_PR_SHIFT;

// FPU Transfer size mode
static const unsigned SH4_FPSCR_SZ_SHIFT = 20;
static const unsigned SH4_FPSCR_SZ_MASK = 1 << SH4_FPSCR_SZ_SHIFT;

// FPU bank switch
static const unsigned SH4_FPSCR_FR_SHIFT = 21;
static const unsigned SH4_FPSCR_FR_MASK = 1 << SH4_FPSCR_FR_SHIFT;

/*******************************************************************************
 *
 * SH4 cache-control register
 *
 ******************************************************************************/

// IC index enable
static const unsigned SH4_CCR_IIX_SHIFT = 15;
static const unsigned SH4_CCR_IIX_MASK = 1 << SH4_CCR_IIX_SHIFT;

// IC invalidation
static const unsigned SH4_CCR_ICI_SHIFT = 11;
static const unsigned SH4_CCR_ICI_MASK = 1 << SH4_CCR_ICI_SHIFT;

// IC enable
static const unsigned SH4_CCR_ICE_SHIFT = 8;
static const unsigned SH4_CCR_ICE_MASK = 1 << SH4_CCR_ICE_SHIFT;

// OC index enable
static const unsigned SH4_CCR_OIX_SHIFT = 7;
static const unsigned SH4_CCR_OIX_MASK = 1 << SH4_CCR_OIX_SHIFT;

// OC RAM enable
static const unsigned SH4_CCR_ORA_SHIFT = 5;
static const unsigned SH4_CCR_ORA_MASK = 1 << SH4_CCR_ORA_SHIFT;

// OC invalidation
static const unsigned SH4_CCR_OCI_SHIFT = 3;
static const unsigned SH4_CCR_OCI_MASK = 1 << SH4_CCR_OCI_SHIFT;

// copy-back enable
static const unsigned SH4_CCR_CB_SHIFT = 2;
static const unsigned SH4_CCR_CB_MASK = 1 << SH4_CCR_CB_SHIFT;

// Write-through
static const unsigned SH4_CCR_WT_SHIFT = 1;
static const unsigned SH4_CCR_WT_MASK = 1 << SH4_CCR_WT_SHIFT;

// OC enable
static const unsigned SH4_CCR_OCE_SHIFT = 0;
static const unsigned SH4_CCR_OCE_MASK = 1 << SH4_CCR_OCE_SHIFT;

/*******************************************************************************
 *
 * SH4 exception stuff
 *
 ******************************************************************************/

// exception code in the expevt register
static const unsigned SH4_EXPEVT_CODE_SHIFT = 0;
static const unsigned SH4_EXPEVT_CODE_MASK = 0xfff << SH4_EXPEVT_CODE_SHIFT;

// exception code in the intevt register
static const unsigned SH4_INTEVT_CODE_SHIFT = 0;
static const unsigned SH4_INTEVT_CODE_MASK = 0xfff << SH4_INTEVT_CODE_SHIFT;

// immediate value in the tra register
static const unsigned SH4_TRA_IMM_SHIFT = 2;
static const unsigned SH4_TRA_IMM_MASK = 0xff << SH4_TRA_IMM_SHIFT;


#endif
