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

#ifndef G1_HPP_
#define G1_HPP_

#include <cstdlib>

#include "types.hpp"

class G1Bus {
public:
    G1Bus();
    ~G1Bus();

    int read(void *buf, size_t addr, size_t len);
    int write(void const *buf, size_t addr, size_t len);

private:
    /*
     * pointer to place where memory-mapped registers are stored.
     * RegReadHandlers and RegWriteHandlers do not need to use this as long as
     * they are consistent.
     */
    static const size_t REG_AREA_SZ = 256;
    uint8_t *reg_area;

    typedef int(G1Bus::*RegReadHandler)(void *buf, addr32_t addr, unsigned len);
    typedef int(G1Bus::*RegWriteHandler)(void const *buf, addr32_t addr,
                                         unsigned len);

    // default read/write handler callbacks
    int DefaultRegReadHandler(void *buf, addr32_t addr, unsigned len);
    int DefaultRegWriteHandler(void const *buf, addr32_t addr, unsigned len);

    static struct MemMappedReg {
        char const *reg_name;

        /*
         * Some registers can be referenced over a range of addresses.
         * To check for equality between this register and a given physical
         * address, AND the address with addr_mask and then check for equality
         * with addr
         */
        addr32_t addr;
        addr32_t addr_mask;

        unsigned len;

        G1Bus::RegReadHandler on_read;
        G1Bus::RegWriteHandler on_write;
    } mem_mapped_regs[];

    MemMappedReg *find_reg_by_addr(addr32_t addr);
};

#endif
