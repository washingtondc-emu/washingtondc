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

#ifndef SH4_REG_FLAGS_H_
#define SH4_REG_FLAGS_H_

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 *
 * SH4 status register
 *
 ******************************************************************************/

// true/false condition or carry/borrow bit
#define SH4_SR_FLAG_T_SHIFT 0
#define SH4_SR_FLAG_T_MASK (1 << SH4_SR_FLAG_T_SHIFT)

// saturation operation for MAC instructions
#define SH4_SR_FLAG_S_SHIFT 1
#define SH4_SR_FLAG_S_MASK (1 << SH4_SR_FLAG_S_SHIFT)

// interrupt mask level
#define SH4_SR_IMASK_SHIFT 4
#define SH4_SR_IMASK_MASK (0xf << SH4_SR_IMASK_SHIFT)

#define SH4_SR_Q_SHIFT 8
#define SH4_SR_Q_MASK (1 << SH4_SR_Q_SHIFT)

#define SH4_SR_M_SHIFT 9
#define SH4_SR_M_MASK (1 << SH4_SR_M_SHIFT)

// FPU disable bit
#define SH4_SR_FD_SHIFT 15
#define SH4_SR_FD_MASK (1 << SH4_SR_FD_SHIFT)

// IRQ mask (1 =masked)
#define SH4_SR_BL_SHIFT 28
#define SH4_SR_BL_MASK (1 << SH4_SR_BL_SHIFT)

// general register bank switch
#define SH4_SR_RB_SHIFT 29
#define SH4_SR_RB_MASK (1 << SH4_SR_RB_SHIFT)

// processor mode (0 user, 1 priveleged)
#define SH4_SR_MD_SHIFT 30
#define SH4_SR_MD_MASK (1 << SH4_SR_MD_SHIFT)

// floating-point rounding mode
#define SH4_FPSCR_RM_SHIFT 0
#define SH4_FPSCR_RM_MASK (3 << SH4_FPSCR_RM_SHIFT)

/*******************************************************************************
 *
 * SH4 floating-point status/control register
 *
 ******************************************************************************/

// FPU exception flags
#define SH4_FPSCR_FLAG_SHIFT 2
#define SH4_FPSCR_FLAG_MASK (0x1f << SH4_FPSCR_FLAG_SHIFT)

// FPU invalid operation
#define SH4_FPSCR_FLAG_V_SHIFT 6
#define SH4_FPSCR_FLAG_V_MASK (1 << SH4_FPSCR_FLAG_V_SHIFT)

// FPU divide by zero
#define SH4_FPSCR_FLAG_Z_SHIFT 5
#define SH4_FPSCR_FLAG_Z_MASK (1 << SH4_FPSCR_FLAG_Z_MASK)

// FPU overflow
#define SH4_FPSCR_FLAG_O_SHIFT 4
#define SH4_FPSCR_FLAG_O_MASK (1 << SH4_FPSCR_FLAG_O_SHIFT)

// FPU underflow
#define SH4_FPSCR_FLAG_U_SHIFT 3
#define SH4_FPSCR_FLAG_U_MASK (1 << SH4_FPSCR_FLAG_U_SHIFT)

// FPU inexact
#define SH4_FPSCR_FLAG_I_SHIFT 2
#define SH4_FPSCR_FLAG_I_MASK (1 << SH4_FPSCR_FLAG_I_SHIFT)

// FPU exception enable
#define SH4_FPSCR_ENABLE_SHIFT 7
#define SH4_FPSCR_ENABLE_MASK (0x1f << SH4_FPSCR_FLAG_SHIFT)

// FPU overflow
#define SH4_FPSCR_ENABLE_V_SHIFT 11
#define SH4_FPSCR_ENABLE_V_MASK (1 << SH4_FPSCR_ENABLE_V_SHIFT)

// FPU divide by zero
#define SH4_FPSCR_ENABLE_Z_SHIFT 10
#define SH4_FPSCR_ENABLE_Z_MASK (1 << SH4_FPSCR_ENABLE_Z_SHIFT)

// FPU overflow
#define SH4_FPSCR_ENABLE_O_SHIFT 9
#define SH4_FPSCR_ENABLE_O_MASK (1 << SH4_FPSCR_ENABLE_O_SHIFT)

// FPU underflow
#define SH4_FPSCR_ENABLE_U_SHIFT 8
#define SH4_FPSCR_ENABLE_U_MASK (1 << SH4_FPSCR_ENABLE_U_SHIFT)

// FPU inexact
#define SH4_FPSCR_ENABLE_I_SHIFT 7
#define SH4_FPSCR_ENABLE_I_MASK (1 << SH4_FPSCR_ENABLE_I_SHIFT)

// FPU exception cause
#define SH4_FPSCR_CAUSE_SHIFT 12
#define SH4_FPSCR_CAUSE_MASK (0x3f << SH4_FPSCR_CAUSE_SHIFT)

// FPU error
#define SH4_FPSCR_CAUSE_E_SHIFT 17
#define SH4_FPSCR_CAUSE_E_MASK (1 << SH4_FPSCR_CAUSE_E_SHIFT)

// FPU invalid operation
#define SH4_FPSCR_CAUSE_V_SHIFT 16
#define SH4_FPSCR_CAUSE_V_MASK (1 << SH4_FPSCR_CAUSE_V_SHIFT)

// FPU divide by zero
#define SH4_FPSCR_CAUSE_Z_SHIFT 15
#define SH4_FPSCR_CAUSE_Z_MASK (1 << SH4_FPSCR_CAUSE_Z_SHIFT)

// FPU overflow
#define SH4_FPSCR_CAUSE_O_SHIFT 14
#define SH4_FPSCR_CAUSE_O_MASK (1 << SH4_FPSCR_CAUSE_O_SHIFT)

// FPU underflow
#define SH4_FPSCR_CAUSE_U_SHIFT 13
#define SH4_FPSCR_CAUSE_U_MASK (1 << SH4_FPSCR_CAUSE_U_SHIFT)

// FPU inexact
#define SH4_FPSCR_CAUSE_I_SHIFT 12
#define SH4_FPSCR_CAUSE_I_MASK (1 << SH4_FPSCR_CAUSE_I_SHIFT)

// FPU Denormalization mode
#define SH4_FPSCR_DN_SHIFT 18
#define SH4_FPSCR_DN_MASK (1 << SH4_FPSCR_DN_SHIFT)

// FPU Precision mode
#define SH4_FPSCR_PR_SHIFT 19
#define SH4_FPSCR_PR_MASK (1 << SH4_FPSCR_PR_SHIFT)

// FPU Transfer size mode
#define SH4_FPSCR_SZ_SHIFT 20
#define SH4_FPSCR_SZ_MASK (1 << SH4_FPSCR_SZ_SHIFT)

// FPU bank switch
#define SH4_FPSCR_FR_SHIFT 21
#define SH4_FPSCR_FR_MASK (1 << SH4_FPSCR_FR_SHIFT)

/*******************************************************************************
 *
 * SH4 cache-control register
 *
 ******************************************************************************/

// IC index enable
#define SH4_CCR_IIX_SHIFT 15
#define SH4_CCR_IIX_MASK (1 << SH4_CCR_IIX_SHIFT)

// IC invalidation
#define SH4_CCR_ICI_SHIFT 11
#define SH4_CCR_ICI_MASK (1 << SH4_CCR_ICI_SHIFT)

// IC enable
#define SH4_CCR_ICE_SHIFT 8
#define SH4_CCR_ICE_MASK (1 << SH4_CCR_ICE_SHIFT)

// OC index enable
#define SH4_CCR_OIX_SHIFT 7
#define SH4_CCR_OIX_MASK (1 << SH4_CCR_OIX_SHIFT)

// OC RAM enable
#define SH4_CCR_ORA_SHIFT 5
#define SH4_CCR_ORA_MASK (1 << SH4_CCR_ORA_SHIFT)

// OC invalidation
#define SH4_CCR_OCI_SHIFT 3
#define SH4_CCR_OCI_MASK (1 << SH4_CCR_OCI_SHIFT)

// copy-back enable
#define SH4_CCR_CB_SHIFT 2
#define SH4_CCR_CB_MASK (1 << SH4_CCR_CB_SHIFT)

// Write-through
#define SH4_CCR_WT_SHIFT 1
#define SH4_CCR_WT_MASK (1 << SH4_CCR_WT_SHIFT)

// OC enable
#define SH4_CCR_OCE_SHIFT 0
#define SH4_CCR_OCE_MASK (1 << SH4_CCR_OCE_SHIFT)

/*******************************************************************************
 *
 * SH4 exception stuff
 *
 ******************************************************************************/

// exception code in the expevt register
#define SH4_EXPEVT_CODE_SHIFT 0
#define SH4_EXPEVT_CODE_MASK (0xfff << SH4_EXPEVT_CODE_SHIFT)

// exception code in the intevt register
#define SH4_INTEVT_CODE_SHIFT 0
#define SH4_INTEVT_CODE_MASK (0xfff << SH4_INTEVT_CODE_SHIFT)

// immediate value in the tra register
#define SH4_TRA_IMM_SHIFT 2
#define SH4_TRA_IMM_MASK (0xff << SH4_TRA_IMM_SHIFT)

/*******************************************************************************
 *
 * SH4 TMU
 *
 ******************************************************************************/

/* input capture control flag */
#define SH4_TCR_ICPF_SHIFT 9
#define SH4_TCR_ICPF_MASK (1 << 9)

/* underflow flag */
#define SH4_TCR_UNF_SHIFT 8
#define SH4_TCR_UNF_MASK (1 << SH4_TCR_UNF_SHIFT)

/* input capture control */
#define SH4_TCR_ICPE_SHIFT 6
#define SH4_TCR_ICPE_MASK (3 << SH4_TCR_ICPE_SHIFT)

/* underflow interrupt enable */
#define SH4_TCR_UNIE_SHIFT 5
#define SH4_TCR_UNIE_MASK (1 << SH4_TCR_UNIE_SHIFT)

/* clock edge selector, I don't think this matters for internal bus clock */
#define SH4_TCR_CKEG_SHIFT 3
#define SH4_TCR_CKEG_MASK (1 << SH4_TCR_CKEG_SHIFT)

/* timer prescaler */
#define SH4_TCR_TPSC_SHIFT 0
#define SH4_TCR_TPSC_MASK (7 << SH4_TCR_TPSC_SHIFT)

/* TSTR channel 0 enable */
#define SH4_TSTR_CHAN0_SHIFT 0
#define SH4_TSTR_CHAN0_MASK (1 << SH4_TSTR_CHAN0_SHIFT)

/* TSTR channel 1 enable */
#define SH4_TSTR_CHAN1_SHIFT 0
#define SH4_TSTR_CHAN1_MASK (1 << SH4_TSTR_CHAN1_SHIFT)

/* TSTR channel 2 enable */
#define SH4_TSTR_CHAN2_SHIFT 0
#define SH4_TSTR_CHAN2_MASK (1 << SH4_TSTR_CHAN2_SHIFT)

/*******************************************************************************
 *
 * SH4 Interrupt Controller
 *
 ******************************************************************************/
#define SH4_ICR_NMIL_SHIFT 15
#define SH4_ICR_NMIL_MASK (1 << SH4_ICR_NMIL_SHIFT)

#define SH4_ICR_MAI_SHIFT 14
#define SH4_ICR_MAI_MASK (1 << SH4_ICR_MAI_SHIFT)

#define SH4_ICR_NMIB_SHIFT 9
#define SH4_ICR_NMIB_MASK (1 << SH4_ICR_NMIB_SHIFT)

#define SH4_ICR_NMIE_SHIFT 8
#define SH4_ICR_NMIE_MASK (1 << SH4_ICR_NMIE_SHIFT)

#define SH4_ICR_IRLM_SHIFT 7
#define SH4_ICR_IRLM_MASK (1 << SH4_ICR_IRLM_SHIFT)

#define SH4_IPRA_TMU0_SHIFT 12
#define SH4_IPRA_TMU0_MASK (0xf << SH4_IPRA_TMU0_SHIFT)

#define SH4_IPRA_TMU1_SHIFT 8
#define SH4_IPRA_TMU1_MASK (0xf << SH4_IPRA_TMU1_SHIFT)

#define SH4_IPRA_TMU2_SHIFT 4
#define SH4_IPRA_TMU2_MASK (0xf << SH4_IPRA_TMU2_SHIFT)

#define SH4_IPRA_RTC_SHIFT 0
#define SH4_IPRA_RTC_MASK (0xf << SH4_IPRA_RTC_SHIFT)

#define SH4_IPRB_WDT_SHIFT 12
#define SH4_IPRB_WDT_MASK (0xf << SH4_IPRB_WDT_SHIFT)

#define SH4_IPRB_REF_SHIFT 8
#define SH4_IPRB_REF_MASK (0xf << SH4_IPRB_REF_SHIFT)

#define SH4_IPRB_SCI1_SHIFT 4
#define SH4_IPRB_SCI1_MASK (0xf << SH4_IPRB_SCI1_SHIFT)

#define SH4_IPRC_GPIO_SHIFT 12
#define SH4_IPRC_GPIO_MASK (0xf << SH4_IPRC_GPIO_SHIFT)

#define SH4_IPRC_DMAC_SHIFT 8
#define SH4_IPRC_DMAC_MASK (0xf << SH4_IPRC_DMAC_SHIFT)

#define SH4_IPRC_SCIF_SHIFT 4
#define SH4_IPRC_SCIF_MASK (0xf << SH4_IPRC_SCIF_SHIFT)

#define SH4_IPRC_HUDI_SHIFT 0
#define SH4_IPRC_HUDI_MASK (0xf << SH4_IPRC_HUDI_SHIFT)

#define SH4_IPRD_IRL0_SHIFT 12
#define SH4_IPRD_IRL0_MASK (0xf << SH4_IPRD_IRL0_SHIFT)

#define SH4_IPRD_IRL1_SHIFT 8
#define SH4_IPRD_IRL1_MASK (0xf << SH4_IPRD_IRL1_MASK)

#define SH4_IPRD_IRL2_SHIFT 4
#define SH4_IPRD_IRL2_MASK (0xf << SH4_IPRD_IRL2_SHIFT)

#define SH4_IPRD_IRL3_SHIFT 0
#define SH4_IPRD_IRL3_MASK (0xf << SH4_IPRD_IRL3_SHIFT)

/*******************************************************************************
 *
 * Store Queue Addr Control
 *
 ******************************************************************************/
#define SH4_QACR_SHIFT 2
#define SH4_QACR_MASK (0x7 << SH4_QACR_SHIFT)

#define SH4_MMUCR_AT_SHIFT 0
#define SH4_MMUCR_AT_MASK (1 << SH4_MMUCR_AT_SHIFT)

#define SH4_MMUCR_TI_SHIFT 2
#define SH4_MMUCR_TI_MASK (1 << SH4_MMUCR_TI_SHIFT)

/*******************************************************************************
 *
 * MMU Control Register
 *
 ******************************************************************************/
// Single (=1)/Multiple(=0) Virtual Memory switch bit
#define SH4_MMUCR_SV_SHIFT 8
#define SH4_MMUCR_SV_MASK (1 << SH4_MMUCR_SV_SHIFT)

#define SH4_MMUCR_SQMD_SHIFT 9
#define SH4_MMUCR_SQMD_MASK (1 << SH4_MMUCR_SQMD_SHIFT)

#define SH4_MMUCR_URC_SHIFT 10
#define SH4_MMUCR_URC_MASK (0x3f << SH4_MMUCR_URC_SHIFT)

#define SH4_MMUCR_URB_SHIFT 18
#define SH4_MMUCR_URB_MASK (0x3f << SH4_MMUCR_URB_SHIFT)

#define SH4_MMUCR_LRUI_SHIFT 26
#define SH4_MMUCR_LRUI_MASK (0x3f << SH4_MMUCR_LRUI_SHIFT)

/*******************************************************************************
 *
 * SCIF (Serial Port with FIFO)
 *
 ******************************************************************************/

// Transmit Interrupt Enable
#define SH4_SCSCR2_TIE_SHIFT 7
#define SH4_SCSCR2_TIE_MASK (1 << SH4_SCSCR2_TIE_SHIFT)

// Receive Interrupt Enable
#define SH4_SCSCR2_RIE_SHIFT 6
#define SH4_SCSCR2_RIE_MASK (1 << SH4_SCSCR2_RIE_SHIFT)

// Transmit Enable
#define SH4_SCSCR2_TE_SHIFT 5
#define SH4_SCSCR2_TE_MASK (1 << SH4_SCSCR2_TE_SHIFT)

// Receive Enable
#define SH4_SCSCR2_RE_SHIFT 4
#define SH4_SCSCR2_RE_MASK (1 << SH4_SCSCR2_RE_SHIFT)

// Receive Error Interrupt Enable
#define SH4_SCSCR2_REIE_SHIFT 3
#define SH4_SCSCR2_REIE_MASK (1 << SH4_SCSCR2_REIE_SHIFT)

// Clock Enable
#define SH4_SCSCR2_CKE1_SHIFT 1
#define SH4_SCSCR2_CKE1_MASK (1 << SH4_SCSCR2_CKE1_SHIFT)

// Number of Parity Errors
#define SH4_SCFSR2_N_PER_SHIFT 12
#define SH4_SCFSR2_N_PER_MASK (0xf << SH4_SCFSR2_N_PER_SHIFT)

// Number of Framing Errors
#define SH4_SCFSR2_N_FER_SHIFT 8
#define SH4_SCFSR2_N_FER_MASK (0xf << SH4_SCFSR2_N_FER_SHIFT)

// Receive Error flag
#define SH4_SCFSR2_ER_SHIFT 7
#define SH4_SCFSR2_ER_MASK (1 << SH4_SCFSR2_ER_SHIFT)

// Transmit End
#define SH4_SCFSR2_TEND_SHIFT 6
#define SH4_SCFSR2_TEND_MASK (1 << SH4_SCFSR2_TEND_SHIFT)

// Transmit FIFO Data Empty
#define SH4_SCFSR2_TDFE_SHIFT 5
#define SH4_SCFSR2_TDFE_MASK (1 << SH4_SCFSR2_TDFE_SHIFT)

// Break Detect
#define SH4_SCFSR2_BRK_SHIFT 4
#define SH4_SCFSR2_BRK_MASK (1 << SH4_SCFSR2_BRK_SHIFT)

// Framing Error
#define SH4_SCFSR2_FER_SHIFT 3
#define SH4_SCFSR2_FER_MASK (1 << SH4_SCFSR2_FER_SHIFT)

// Parity Error
#define SH4_SCFSR2_PER_SHIFT 2
#define SH4_SCFSR2_PER_MASK (1 << SH4_SCFSR2_PER_SHIFT)

// Receive FIFO Data Full
#define SH4_SCFSR2_RDF_SHIFT 1
#define SH4_SCFSR2_RDF_MASK (1 << SH4_SCFSR2_RDF_SHIFT)

// Receive Data Ready
#define SH4_SCFSR2_DR_SHIFT 0
#define SH4_SCFSR2_DR_MASK (1 << SH4_SCFSR2_DR_SHIFT)

// RTS2 Output Active Trigger
#define SH4_SCFCR2_RSTRG_SHIFT 8
#define SH4_SCFCR2_RSTRG_MASK (0x7 << SH4_SCFCR2_RSTRG_SHIFT)

// Receive FIFO Data Number Trigger
#define SH4_SCFCR2_RTRG_SHIFT 6
#define SH4_SCFCR2_RTRG_MASK (0x3 << SH4_SCFCR2_RTRG_SHIFT)

// Transmit FIFO Data Number Trigger
#define SH4_SCFCR2_TTRG_SHIFT 4
#define SH4_SCFCR2_TTRG_MASK (0x3 << SH4_SCFCR2_TTRG_SHIFT)

// Modem Control Enable
#define SH4_SCFCR2_MCE_SHIFT 3
#define SH4_SCFCR2_MCE_MASK (1 << SH4_SCFCR2_MCE_SHIFT)

// Transmit FIFO Data Register Reset
#define SH4_SCFCR2_TFRST_SHIFT 2
#define SH4_SCFCR2_TFRST_MASK (1 << SH4_SCFCR2_TFRST_SHIFT)

// Receive FIFO Data Register Reset
#define SH4_SCFCR2_RFRST_SHIFT 1
#define SH4_SCFCR2_RFRST_MASK (1 << SH4_SCFCR2_RFRST_SHIFT)

// Loopback Test
#define SH4_SCFCR2_LOOP_SHIFT 0
#define SH4_SCFCR2_LOOP_MASK (1 << SH4_SCFCR2_LOOP_SHIFT)

/*******************************************************************************
 *
 * SH-4 Standby Control Register
 *
 ******************************************************************************/

// Standby mode
#define SH4_STBCR_STBY_SHIFT 7
#define SH4_STBCR_STBY_MASK (1 << SH4_STBCR_STBY_SHIFT)

// Peripheral Module Pin High Impedance Control
#define SH4_STBCR_PHZ_SHIFT 6
#define SH4_STBCR_PHZ_MASK (1 << SH4_STBCR_PHZ_SHIFT)

// Peripheral Module Pin Pull-Up Control
#define SH4_STBCR_PPU_SHIFT 5
#define SH4_STBCR_PPU_MASK (1 << SH4_STBCR_PPU_SHIFT)

// Module Stop 4
#define SH4_STBCR_MSTP4_SHIFT 4
#define SH4_STBCR_MSTP4_MASK (1 << SH4_STBCR_MSTP4_SHIFT)

// Module Stop 3
#define SH4_STBCR_MSTP3_SHIFT 3
#define SH4_STBCR_MSTP3_MASK (1 << SH4_STBCR_MSTP3_SHIFT)

// Module Stop 2
#define SH4_STBCR_MSTP2_SHIFT 2
#define SH4_STBCR_MSTP2_MASK (1 << SH4_STBCR_MSTP2_SHIFT)

// Module Stop 1
#define SH4_STBCR_MSTP1_SHIFT 1
#define SH4_STBCR_MSTP1_MASK (1 << SH4_STBCR_MSTP1_SHIFT)

// Module Stop 0
#define SH4_STBCR_MSTP0_SHIFT 0
#define SH4_STBCR_MSTP0_MASK (1 << SH4_STBCR_MSTP0_SHIFT)

#ifdef __cplusplus
}
#endif

#endif
