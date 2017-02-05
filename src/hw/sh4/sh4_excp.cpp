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

#include "BaseException.hpp"
#include "sh4.hpp"
#include "sh4_excp.hpp"

typedef boost::error_info<struct tag_excp_code_error_info,
                          enum Sh4ExceptionCode> sh4_excp_code_error_info;

class UnknownExcpCodeException : public BaseException {
public:
    char const *what() const throw() {
        return "unrecognized sh4 exception code";
    }
};

static Sh4ExcpMeta const sh4_excp_meta[SH4_EXCP_COUNT] = {
    // exception code                         prio_level   prio_order   offset
    { SH4_EXCP_POWER_ON_RESET,                    1,           1,           0      },
    { SH4_EXCP_MANUAL_RESET,                      1,           2,           0      },
    { SH4_EXCP_HUDI_RESET,                        1,           1,           0      },
    { SH4_EXCP_INST_TLB_MULT_HIT,                 1,           3,           0      },
    { SH4_EXCP_DATA_TLB_MULT_HIT,                 1,           4,           0      },
    { SH4_EXCP_USER_BREAK_BEFORE,                 2,           0,           0x100  },
    { SH4_EXCP_INST_ADDR_ERR,                     2,           1,           0x100  },
    { SH4_EXCP_INST_TLB_MISS,                     2,           2,           0x400  },
    { SH4_EXCP_INST_TLB_PROT_VIOL,                2,           3,           0x100  },
    { SH4_EXCP_GEN_ILLEGAL_INST,                  2,           4,           0x100  },
    { SH4_EXCP_SLOT_ILLEGAL_INST,                 2,           4,           0x100  },
    { SH4_EXCP_GEN_FPU_DISABLE,                   2,           4,           0x100  },
    { SH4_EXCP_SLOT_FPU_DISABLE,                  2,           4,           0x100  },
    { SH4_EXCP_DATA_ADDR_READ,                    2,           5,           0x100  },
    { SH4_EXCP_DATA_ADDR_WRITE,                   2,           5,           0x100  },
    { SH4_EXCP_DATA_TLB_READ_MISS,                2,           6,           0x400  },
    { SH4_EXCP_DATA_TLB_WRITE_MISS,               2,           6,           0x400  },
    { SH4_EXCP_DATA_TLB_READ_PROT_VIOL,           2,           7,           0x100  },
    { SH4_EXCP_DATA_TLB_WRITE_PROT_VIOL,          2,           7,           0x100  },
    { SH4_EXCP_FPU,                               2,           8,           0x100  },
    { SH4_EXCP_INITIAL_PAGE_WRITE,                2,           9,           0x100  },
    { SH4_EXCP_UNCONDITIONAL_TRAP,                2,           4,           0x100  },
    { SH4_EXCP_USER_BREAK_AFTER,                  2,          10,           0x100  },
    { SH4_EXCP_NMI,                               3,           0,           0x600  },
    { SH4_EXCP_EXT_0,                             4,           2,           0x600  },
    { SH4_EXCP_EXT_1,                             4,           2,           0x600  },
    { SH4_EXCP_EXT_2,                             4,           2,           0x600  },
    { SH4_EXCP_EXT_3,                             4,           2,           0x600  },
    { SH4_EXCP_EXT_4,                             4,           2,           0x600  },
    { SH4_EXCP_EXT_5,                             4,           2,           0x600  },
    { SH4_EXCP_EXT_6,                             4,           2,           0x600  },
    { SH4_EXCP_EXT_7,                             4,           2,           0x600  },
    { SH4_EXCP_EXT_8,                             4,           2,           0x600  },
    { SH4_EXCP_EXT_9,                             4,           2,           0x600  },
    { SH4_EXCP_EXT_A,                             4,           2,           0x600  },
    { SH4_EXCP_EXT_B,                             4,           2,           0x600  },
    { SH4_EXCP_EXT_C,                             4,           2,           0x600  },
    { SH4_EXCP_EXT_D,                             4,           2,           0x600  },
    { SH4_EXCP_EXT_E,                             4,           2,           0x600  },
    { SH4_EXCP_TMU0_TUNI0,                        4,           2,           0x600  },
    { SH4_EXCP_TMU1_TUNI1,                        4,           2,           0x600  },
    { SH4_EXCP_TMU2_TUNI2,                        4,           2,           0x600  },
    { SH4_EXCP_TMU2_TICPI2,                       4,           2,           0x600  },
    { SH4_EXCP_RTC_ATI,                           4,           2,           0x600  },
    { SH4_EXCP_RTC_PRI,                           4,           2,           0x600  },
    { SH4_EXCP_RTC_CUI,                           4,           2,           0x600  },
    { SH4_EXCP_SCI_ERI,                           4,           2,           0x600  },
    { SH4_EXCP_SCI_RXI,                           4,           2,           0x600  },
    { SH4_EXCP_SCI_TXI,                           4,           2,           0x600  },
    { SH4_EXCP_SCI_TEI,                           4,           2,           0x600  },
    { SH4_EXCP_WDT_ITI,                           4,           2,           0x600  },
    { SH4_EXCP_REF_RCMI,                          4,           2,           0x600  },
    { SH4_EXCP_REF_ROVI,                          4,           2,           0x600  },
    { SH4_EXCP_GPIO_GPIOI,                        4,           2,           0x600  },
    { SH4_EXCP_DMAC_DMTE0,                        4,           2,           0x600  },
    { SH4_EXCP_DMAC_DMTE1,                        4,           2,           0x600  },
    { SH4_EXCP_DMAC_DMTE2,                        4,           2,           0x600  },
    { SH4_EXCP_DMAC_DMTE3,                        4,           2,           0x600  },
    { SH4_EXCP_DMAC_DMAE,                         4,           2,           0x600  },
    { SH4_EXCP_SCIF_ERI,                          4,           2,           0x600  },
    { SH4_EXCP_SCIF_RXI,                          4,           2,           0x600  },
    { SH4_EXCP_SCIF_BRI,                          4,           2,           0x600  },
    { SH4_EXCP_SCIF_TXI,                          4,           2,           0x600  }
};

void sh4_enter_exception(Sh4 *sh4, enum Sh4ExceptionCode vector) {
    struct Sh4ExcpMeta const *meta = NULL;
    reg32_t *reg = sh4->reg;

    for (unsigned idx = 0; idx < SH4_EXCP_COUNT; idx++) {
        if (sh4_excp_meta[idx].code == vector) {
            meta = sh4_excp_meta + idx;
            break;
        }
    }

    if (!meta)
        BOOST_THROW_EXCEPTION(UnknownExcpCodeException() <<
                              sh4_excp_code_error_info(vector));

    reg[SH4_REG_SPC] = reg[SH4_REG_PC];
    reg[SH4_REG_SSR] = reg[SH4_REG_SR];
    reg[SH4_REG_SGR] = reg[SH4_REG_R15];

    reg[SH4_REG_SR] |= Sh4::SR_BL_MASK;
    reg[SH4_REG_SR] |= Sh4::SR_MD_MASK;
    reg[SH4_REG_SR] |= Sh4::SR_RB_MASK;
    reg[SH4_REG_SR] &= ~Sh4::SR_FD_MASK;

    if (vector == SH4_EXCP_POWER_ON_RESET ||
        vector == SH4_EXCP_MANUAL_RESET ||
        vector == SH4_EXCP_HUDI_RESET ||
        vector == SH4_EXCP_INST_TLB_MULT_HIT ||
        vector == SH4_EXCP_INST_TLB_MULT_HIT) {
        reg[SH4_REG_PC] = 0xa0000000;
    } else if (vector == SH4_EXCP_USER_BREAK_BEFORE ||
               vector == SH4_EXCP_USER_BREAK_AFTER) {
        // TODO: check brcr.ubde and use DBR instead of VBR if it is set
        reg[SH4_REG_PC] = reg[SH4_REG_VBR] + meta->offset;
    } else {
        reg[SH4_REG_PC] = reg[SH4_REG_VBR] + meta->offset;
    }
}

void sh4_set_exception(Sh4 *sh4, unsigned excp_code) {
    sh4->reg[SH4_REG_EXPEVT] = (excp_code << Sh4::EXPEVT_CODE_SHIFT) &
        Sh4::EXPEVT_CODE_MASK;

    sh4_enter_exception(sh4, (Sh4ExceptionCode)excp_code);
}

void sh4_set_interrupt(Sh4 *sh4, unsigned intp_code) {
    sh4->reg[SH4_REG_INTEVT] = (intp_code << Sh4::INTEVT_CODE_SHIFT) &
        Sh4::INTEVT_CODE_MASK;

    sh4_enter_exception(sh4, (Sh4ExceptionCode)intp_code);
}

int Sh4TraRegReadHandler(Sh4 *sh4, void *buf,
                         struct Sh4MemMappedReg const *reg_info) {
    memcpy(buf, &sh4->reg[SH4_REG_TRA], sizeof(sh4->reg[SH4_REG_TRA]));

    return 0;
}

int Sh4TraRegWriteHandler(Sh4 *sh4, void const *buf,
                          struct Sh4MemMappedReg const *reg_info) {
    memcpy(&sh4->reg[SH4_REG_TRA], buf, sizeof(sh4->reg[SH4_REG_TRA]));

    return 0;
}

int Sh4ExpevtRegReadHandler(Sh4 *sh4, void *buf,
                            struct Sh4MemMappedReg const *reg_info) {
    memcpy(buf, &sh4->reg[SH4_REG_EXPEVT], sizeof(sh4->reg[SH4_REG_EXPEVT]));

    return 0;
}

int Sh4ExpevtRegWriteHandler(Sh4 *sh4, void const *buf,
                             struct Sh4MemMappedReg const *reg_info) {
    memcpy(&sh4->reg[SH4_REG_EXPEVT], buf, sizeof(sh4->reg[SH4_REG_EXPEVT]));

    return 0;
}

int Sh4IntevtRegReadHandler(Sh4 *sh4, void *buf,
                            struct Sh4MemMappedReg const *reg_info) {
    memcpy(buf, &sh4->reg[SH4_REG_INTEVT], sizeof(sh4->reg[SH4_REG_INTEVT]));

    return 0;
}

int Sh4IntevtRegWriteHandler(Sh4 *sh4, void const *buf,
                             struct Sh4MemMappedReg const *reg_info) {
    memcpy(&sh4->reg[SH4_REG_INTEVT], buf, sizeof(sh4->reg[SH4_REG_INTEVT]));

    return 0;
}
