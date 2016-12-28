/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016 snickerbockers
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

#ifdef ENABLE_SH4_ICACHE
#include "Icache.hpp"
#endif

#ifdef ENABLE_SH4_OCACHE
#include "Ocache.hpp"
#endif

#include "BaseException.hpp"

#include "sh4.hpp"

typedef boost::error_info<struct tag_excp_code_error_info,
                          enum Sh4::ExceptionCode> excp_code_error_info;

class UnknownExcpCodeException : public BaseException {
public:
    char const *what() const throw() {
        return "unrecognized sh4 exception code";
    }
};

Sh4::Sh4(Memory *mem) {
    this->mem = mem;

    reg_area = new uint8_t[P4_REGEND - P4_REGSTART];

#ifdef ENABLE_SH4_MMU
    memset(utlb, 0, sizeof(utlb));
    memset(itlb, 0, sizeof(itlb));
#endif

    memset(&reg, 0, sizeof(reg));
    memset(&mmu, 0, sizeof(mmu));
    memset(&cache_reg, 0, sizeof(cache_reg));

#ifdef ENABLE_SH4_ICACHE
    this->inst_cache = new Icache(this, mem);
#endif

#ifdef ENABLE_SH4_OCACHE
    this->op_cache = new Ocache(this, mem);
#endif

    init_regs();

    compile_instructions();

    on_hard_reset();
}

Sh4::~Sh4() {
#ifdef ENABLE_SH4_OCACHE
    delete op_cache;
#endif

#ifdef ENABLE_SH4_ICACHE
    delete inst_cache;
#endif

    delete[] reg_area;
}

void Sh4::on_hard_reset() {
    reg.sr = SR_MD_MASK | SR_RB_MASK | SR_BL_MASK | SR_FD_MASK | SR_IMASK_MASK;
    reg.vbr = 0;
    reg.pc = 0xa0000000;

    reg.fpscr = 0x41;
}

reg32_t Sh4::get_pc() const {
    return reg.pc;
}

void Sh4::set_exception(unsigned excp_code) {
    excp_reg.expevt = (excp_code << EXPEVT_CODE_SHIFT) & EXPEVT_CODE_MASK;

    enter_exception((ExceptionCode)excp_code);
}

void Sh4::set_interrupt(unsigned intp_code) {
    excp_reg.intevt = (intp_code << INTEVT_CODE_SHIFT) & INTEVT_CODE_MASK;

    enter_exception((ExceptionCode)intp_code);
}

Sh4::RegFile Sh4::get_regs() const {
    return reg;
}

void Sh4::set_regs(const Sh4::RegFile& src) {
    this->reg = src;
}

const Sh4::ExcpMeta Sh4::excp_meta[Sh4::EXCP_COUNT] = {
    // exception code                         prio_level   prio_order   offset
    { EXCP_POWER_ON_RESET,                    1,           1,           0      },
    { EXCP_MANUAL_RESET,                      1,           2,           0      },
    { EXCP_HUDI_RESET,                        1,           1,           0      },
    { EXCP_INST_TLB_MULT_HIT,                 1,           3,           0      },
    { EXCP_DATA_TLB_MULT_HIT,                 1,           4,           0      },
    { EXCP_USER_BREAK_BEFORE,                 2,           0,           0x100  },
    { EXCP_INST_ADDR_ERR,                     2,           1,           0x100  },
    { EXCP_INST_TLB_MISS,                     2,           2,           0x400  },
    { EXCP_INST_TLB_PROT_VIOL,                2,           3,           0x100  },
    { EXCP_GEN_ILLEGAL_INST,                  2,           4,           0x100  },
    { EXCP_SLOT_ILLEGAL_INST,                 2,           4,           0x100  },
    { EXCP_GEN_FPU_DISABLE,                   2,           4,           0x100  },
    { EXCP_SLOT_FPU_DISABLE,                  2,           4,           0x100  },
    { EXCP_DATA_ADDR_READ,                    2,           5,           0x100  },
    { EXCP_DATA_ADDR_WRITE,                   2,           5,           0x100  },
    { EXCP_DATA_TLB_READ_MISS,                2,           6,           0x400  },
    { EXCP_DATA_TLB_WRITE_MISS,               2,           6,           0x400  },
    { EXCP_DATA_TLB_READ_PROT_VIOL,           2,           7,           0x100  },
    { EXCP_DATA_TLB_WRITE_PROT_VIOL,          2,           7,           0x100  },
    { EXCP_FPU,                               2,           8,           0x100  },
    { EXCP_INITIAL_PAGE_WRITE,                2,           9,           0x100  },
    { EXCP_UNCONDITIONAL_TRAP,                2,           4,           0x100  },
    { EXCP_USER_BREAK_AFTER,                  2,          10,           0x100  },
    { EXCP_NMI,                               3,           0,           0x600  },
    { EXCP_EXT_0,                             4,           2,           0x600  },
    { EXCP_EXT_1,                             4,           2,           0x600  },
    { EXCP_EXT_2,                             4,           2,           0x600  },
    { EXCP_EXT_3,                             4,           2,           0x600  },
    { EXCP_EXT_4,                             4,           2,           0x600  },
    { EXCP_EXT_5,                             4,           2,           0x600  },
    { EXCP_EXT_6,                             4,           2,           0x600  },
    { EXCP_EXT_7,                             4,           2,           0x600  },
    { EXCP_EXT_8,                             4,           2,           0x600  },
    { EXCP_EXT_9,                             4,           2,           0x600  },
    { EXCP_EXT_A,                             4,           2,           0x600  },
    { EXCP_EXT_B,                             4,           2,           0x600  },
    { EXCP_EXT_C,                             4,           2,           0x600  },
    { EXCP_EXT_D,                             4,           2,           0x600  },
    { EXCP_EXT_E,                             4,           2,           0x600  },
    { EXCP_TMU0_TUNI0,                        4,           2,           0x600  },
    { EXCP_TMU1_TUNI1,                        4,           2,           0x600  },
    { EXCP_TMU2_TUNI2,                        4,           2,           0x600  },
    { EXCP_TMU2_TICPI2,                       4,           2,           0x600  },
    { EXCP_RTC_ATI,                           4,           2,           0x600  },
    { EXCP_RTC_PRI,                           4,           2,           0x600  },
    { EXCP_RTC_CUI,                           4,           2,           0x600  },
    { EXCP_SCI_ERI,                           4,           2,           0x600  },
    { EXCP_SCI_RXI,                           4,           2,           0x600  },
    { EXCP_SCI_TXI,                           4,           2,           0x600  },
    { EXCP_SCI_TEI,                           4,           2,           0x600  },
    { EXCP_WDT_ITI,                           4,           2,           0x600  },
    { EXCP_REF_RCMI,                          4,           2,           0x600  },
    { EXCP_REF_ROVI,                          4,           2,           0x600  },
    { EXCP_GPIO_GPIOI,                        4,           2,           0x600  },
    { EXCP_DMAC_DMTE0,                        4,           2,           0x600  },
    { EXCP_DMAC_DMTE1,                        4,           2,           0x600  },
    { EXCP_DMAC_DMTE2,                        4,           2,           0x600  },
    { EXCP_DMAC_DMTE3,                        4,           2,           0x600  },
    { EXCP_DMAC_DMAE,                         4,           2,           0x600  },
    { EXCP_SCIF_ERI,                          4,           2,           0x600  },
    { EXCP_SCIF_RXI,                          4,           2,           0x600  },
    { EXCP_SCIF_BRI,                          4,           2,           0x600  },
    { EXCP_SCIF_TXI,                          4,           2,           0x600  }
};

void Sh4::enter_exception(enum ExceptionCode vector) {
    struct ExcpMeta const *meta = NULL;

    for (unsigned idx = 0; idx < EXCP_COUNT; idx++) {
        if (excp_meta[idx].code == vector) {
            meta = excp_meta + idx;
            break;
        }
    }

    if (!meta)
        BOOST_THROW_EXCEPTION(UnknownExcpCodeException() <<
                              excp_code_error_info(vector));

    reg.spc = reg.pc;
    reg.ssr = reg.sr;
    reg.sgr = reg.rgen[7];

    reg.sr |= SR_BL_MASK;
    reg.sr |= SR_MD_MASK;
    reg.sr |= SR_RB_MASK;
    reg.sr &= ~SR_FD_MASK;

    if (vector == EXCP_POWER_ON_RESET ||
        vector == EXCP_MANUAL_RESET ||
        vector == EXCP_HUDI_RESET ||
        vector == EXCP_INST_TLB_MULT_HIT ||
        vector == EXCP_INST_TLB_MULT_HIT) {
        reg.pc = 0xa0000000;
    } else if (vector == EXCP_USER_BREAK_BEFORE ||
               vector == EXCP_USER_BREAK_AFTER) {
        // TODO: check brcr.ubde and use DBR instead of VBR if it is set
        reg.pc = reg.vbr + meta->offset;
    } else {
        reg.pc = reg.vbr + meta->offset;
    }
}
