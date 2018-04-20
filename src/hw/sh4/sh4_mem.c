/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016-2018 snickerbockers
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

#include <stdlib.h>

#include "sh4_excp.h"
#include "sh4_mem.h"
#include "sh4_ocache.h"
#include "sh4.h"
#include "dreamcast.h"
#include "MemoryMap.h"
#include "mem_code.h"

#ifdef ENABLE_DEBUGGER
#include "debugger.h"
#endif

/*
 * TODO: need to adequately return control to the debugger when there's a memory
 * error and the debugger has its error-handler set up.  longjmp is the obvious
 * solution, but until all the codebase is out of C++ I don't want to risk that.
 */

#define SH4_DO_WRITE_P4_TMPL(type, postfix)                             \
    void sh4_do_write_p4_##postfix(Sh4 *sh4, addr32_t addr, type val) { \
        if ((addr & SH4_SQ_AREA_MASK) == SH4_SQ_AREA_VAL) {             \
            sh4_sq_write_##postfix(sh4, addr, val);                     \
        } else if (addr >= SH4_P4_REGSTART && addr < SH4_P4_REGEND) {   \
            sh4_write_mem_mapped_reg_##postfix(sh4, addr, val);         \
        } else if (addr >= SH4_OC_ADDR_ARRAY_FIRST &&                   \
                   addr <= SH4_OC_ADDR_ARRAY_LAST) {                    \
            sh4_ocache_write_addr_array_##postfix(sh4, addr, val);  \
        } else {                                                        \
            error_set_address(addr);                                    \
            error_set_length(sizeof(val));                              \
            error_set_feature("writing to part of the P4 memory region"); \
            RAISE_ERROR(ERROR_UNIMPLEMENTED);                           \
        }                                                               \
    }

SH4_DO_WRITE_P4_TMPL(uint8_t, 8)
SH4_DO_WRITE_P4_TMPL(uint16_t, 16)
SH4_DO_WRITE_P4_TMPL(uint32_t, 32)
SH4_DO_WRITE_P4_TMPL(float, float)
SH4_DO_WRITE_P4_TMPL(double, double)

#define SH4_DO_READ_P4_TMPL(type, postfix)                              \
    type sh4_do_read_p4_##postfix(Sh4 *sh4, addr32_t addr) {            \
        type tmp_val;                                                   \
                                                                        \
        if ((addr & SH4_SQ_AREA_MASK) == SH4_SQ_AREA_VAL) {             \
            return sh4_sq_read_##postfix(sh4, addr);                    \
        } else if (addr >= SH4_P4_REGSTART && addr < SH4_P4_REGEND) {   \
            return sh4_read_mem_mapped_reg_##postfix(sh4, addr);        \
        } else if (addr >= SH4_OC_ADDR_ARRAY_FIRST &&                   \
                   addr <= SH4_OC_ADDR_ARRAY_LAST) {                    \
            return sh4_ocache_read_addr_array_##postfix(sh4, addr);     \
        } else {                                                        \
            error_set_length(sizeof(type));                             \
            error_set_address(addr);                                    \
            error_set_feature("reading from part of the P4 memory region"); \
            RAISE_ERROR(ERROR_UNIMPLEMENTED);                           \
        }                                                               \
                                                                        \
        return tmp_val;                                                 \
    }

SH4_DO_READ_P4_TMPL(uint8_t, 8)
SH4_DO_READ_P4_TMPL(uint16_t, 16)
SH4_DO_READ_P4_TMPL(uint32_t, 32)
SH4_DO_READ_P4_TMPL(float, float)
SH4_DO_READ_P4_TMPL(double, double)
