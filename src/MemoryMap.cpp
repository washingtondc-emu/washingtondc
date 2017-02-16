/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016, 2017 snickerbockers
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
#include "hw/sys/sys_block.hpp"
#include "hw/maple/maple_reg.hpp"
#include "hw/g1/g1_reg.hpp"
#include "hw/g2/g2_reg.hpp"
#include "hw/pvr2/pvr2_reg.hpp"

#include "MemoryMap.hpp"

static BiosFile *bios;
static struct Memory *mem;

void memory_map_init(BiosFile *bios_new, struct Memory *mem_new) {
    memory_map_set_bios(bios_new);
    memory_map_set_mem(mem_new);
}

void memory_map_set_bios(BiosFile *bios_new) {
    bios = bios_new;
}

void memory_map_set_mem(struct Memory *mem_new) {
    mem = mem_new;
}

int memory_map_read(void *buf, size_t addr, size_t len) {
    try {
        // check RAM first because that's the case we want to optimize for
        if (addr >= ADDR_RAM_FIRST && addr <= ADDR_RAM_LAST) {
            return memory_read(mem, buf, addr - ADDR_RAM_FIRST, len);
        } else if (addr <= ADDR_BIOS_LAST) {
            /*
             * XXX In case you were wondering: we don't check to see if
             * addr >= ADDR_BIOS_FIRST because ADDR_BIOS_FIRST is 0
             */
            if ((addr - 1 + len) > ADDR_BIOS_LAST) {
                BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                      errinfo_feature("proper response for "
                                                      "when the guest reads "
                                                      "past a memory map's "
                                                      "end") <<
                                      errinfo_length(len));
            }
            return bios->read(buf, addr - ADDR_BIOS_FIRST, len);
        } else if (addr >= ADDR_G1_FIRST && addr <= ADDR_G1_LAST) {
            if (addr + len > ADDR_G1_LAST) {
                BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                      errinfo_feature("proper response for "
                                                      "when the guest reads "
                                                      "past a memory map's "
                                                      "end") <<
                                      errinfo_length(len));
            }
            return g1_reg_read(buf, addr - ADDR_G1_FIRST, len);
        } else if (addr >= ADDR_SYS_FIRST && addr <= ADDR_SYS_LAST) {
            if (addr + len > ADDR_SYS_LAST) {
                BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                      errinfo_feature("proper response for "
                                                      "when the guest reads "
                                                      "past a memory map's "
                                                      "end") <<
                                      errinfo_length(len));
            }
            return sys_block_read(buf, addr, len);
        } else if (addr >= ADDR_MAPLE_FIRST && addr <= ADDR_MAPLE_LAST) {
            if (addr + len > ADDR_MAPLE_LAST) {
                BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                      errinfo_feature("proper response for "
                                                      "when the guest reads "
                                                      "past a memory map's "
                                                      "end") <<
                                      errinfo_length(len));
            }
            return maple_reg_read(buf, addr, len);
        } else if (addr >= ADDR_G2_FIRST && addr <= ADDR_G2_LAST) {
            if (addr + len > ADDR_G2_LAST) {
                BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                      errinfo_feature("proper response for "
                                                      "when the guest reads "
                                                      "past a memory map's "
                                                      "end") <<
                                      errinfo_length(len));
            }
            return g2_reg_read(buf, addr, len);
        } else if (addr >= ADDR_PVR2_FIRST && addr <= ADDR_PVR2_LAST) {
            if (addr + len > ADDR_PVR2_LAST) {
                BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                      errinfo_feature("proper response for "
                                                      "when the guest reads "
                                                      "past a memory map's "
                                                      "end") <<
                                      errinfo_length(len));
            }
            return pvr2_reg_read(buf, addr, len);
        }

        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("memory mapping") <<
                              errinfo_guest_addr(addr));
    } catch(BaseException& exc) {
        exc << errinfo_guest_addr(addr);
        exc << errinfo_op_type("read");
        throw;
    }
}

int memory_map_write(void const *buf, size_t addr, size_t len) {
    try {
        // check RAM first because that's the case we want to optimize for
        if (addr >= ADDR_RAM_FIRST && addr <= ADDR_RAM_LAST) {
            return memory_write(mem, buf, addr - ADDR_RAM_FIRST, len);
        } else if (addr <= ADDR_BIOS_LAST) {
            /*
             * XXX In case you were wondering: we don't check to see if
             * addr >= ADDR_BIOS_FIRST because ADDR_BIOS_FIRST is 0
             */
            BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                  errinfo_feature("Proper response for when "
                                                  "the guest tries to write to "
                                                  "read-only memory") <<
                                  errinfo_length(len));
        } else if (addr >= ADDR_G1_FIRST && addr <= ADDR_G1_LAST) {
            if (addr + len > ADDR_G1_LAST) {
                BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                      errinfo_feature("proper response for "
                                                      "when the guest writes "
                                                      "past a memory map's "
                                                      "end") <<
                                      errinfo_length(len));
            }
            return g1_reg_write(buf, addr, len);
        } else if (addr >= ADDR_SYS_FIRST && addr <= ADDR_SYS_LAST) {
            if (addr + len > ADDR_SYS_LAST) {
                BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                      errinfo_feature("proper response for "
                                                      "when the guest writes "
                                                      "past a memory map's "
                                                      "end") <<
                                      errinfo_length(len));
            }
            return sys_block_write(buf, addr, len);
        } else if (addr >= ADDR_MAPLE_FIRST && addr <= ADDR_MAPLE_LAST) {
            if (addr + len > ADDR_MAPLE_LAST) {
                BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                      errinfo_feature("proper response for "
                                                      "when the guest writes "
                                                      "past a memory map's "
                                                      "end") <<
                                      errinfo_length(len));
            }
            return maple_reg_write(buf, addr, len);
        } else if (addr >= ADDR_G2_FIRST && addr <= ADDR_G2_LAST) {
            if (addr + len > ADDR_G2_LAST) {
                BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                      errinfo_feature("proper response for "
                                                      "when the guest writes "
                                                      "past a memory map's "
                                                      "end") <<
                                      errinfo_length(len));
            }
            return g2_reg_write(buf, addr, len);
        } else if (addr >= ADDR_PVR2_FIRST && addr <= ADDR_PVR2_LAST) {
            if (addr + len > ADDR_PVR2_LAST) {
                BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                      errinfo_feature("proper response for "
                                                      "when the guest writes "
                                                      "past a memory map's "
                                                      "end") <<
                                      errinfo_length(len));
            }
            return pvr2_reg_write(buf, addr, len);
        }
    } catch(BaseException& exc) {
        exc << errinfo_guest_addr(addr);
        exc << errinfo_op_type("write");

        switch (len) {
        case 1:
            exc << errinfo_val8(*(uint8_t*)buf);
            break;
        case 2:
            exc << errinfo_val16(*(uint16_t*)buf);
            break;
        case 4:
            exc << errinfo_val32(*(uint32_t*)buf);
            break;
        case 8:
            exc << errinfo_val64(*(uint64_t*)buf);
            break;
        default:
            break; // do nothing
        }

        throw;
    }

    BOOST_THROW_EXCEPTION(UnimplementedError() <<
                          errinfo_feature("memory mapping") <<
                          errinfo_guest_addr(addr));
}
