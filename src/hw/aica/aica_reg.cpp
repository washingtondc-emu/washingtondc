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

#include <iostream>

#include "BaseException.hpp"
#include "MemoryMap.hpp"

#include "aica_reg.hpp"

static const size_t N_AICA_REGS = ADDR_AICA_LAST - ADDR_AICA_FIRST + 1;
static reg32_t aica_regs[N_AICA_REGS];

struct aica_mapped_reg;

typedef int(*aica_reg_read_handler_t)(struct aica_mapped_reg const *reg_info,
                                      void *buf, addr32_t addr, unsigned len);
typedef int(*aica_reg_write_handler_t)(struct aica_mapped_reg const *reg_info,
                                       void const *buf, addr32_t addr,
                                       unsigned len);
static int
default_aica_reg_read_handler(struct aica_mapped_reg const *reg_info,
                              void *buf, addr32_t addr, unsigned len);
static int
default_aica_reg_write_handler(struct aica_mapped_reg const *reg_info,
                               void const *buf, addr32_t addr, unsigned len);

static int
warn_aica_reg_read_handler(struct aica_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len);
static int
warn_aica_reg_write_handler(struct aica_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len);

static struct aica_mapped_reg {
    char const *reg_name;

    addr32_t addr;

    unsigned len;

    aica_reg_read_handler_t on_read;
    aica_reg_write_handler_t on_write;
} aica_reg_info[] = {
    /*
     * two-byte register containing VREG and some other weird unrelated stuff
     * that is part of AICA for reasons which I cannot fathom
     */
    { "AICA_00702C00", 0x00702c00, 4,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { NULL }
};

int aica_reg_read(void *buf, size_t addr, size_t len) {
    struct aica_mapped_reg *curs = aica_reg_info;

    while (curs->reg_name) {
        if (curs->addr == addr) {
            if (curs->len == len) {
                return curs->on_read(curs, buf, addr, len);
            } else {
                BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                      errinfo_feature("Whatever happens when "
                                                      "you use an inapproriate "
                                                      "length while reading "
                                                      "from a aica "
                                                      "register") <<
                                      errinfo_guest_addr(addr) <<
                                      errinfo_length(len));
            }
        }
        curs++;
    }

    BOOST_THROW_EXCEPTION(UnimplementedError() <<
                          errinfo_feature("reading from one of the "
                                          "aica registers") <<
                          errinfo_guest_addr(addr));
}

int aica_reg_write(void const *buf, size_t addr, size_t len) {
    struct aica_mapped_reg *curs = aica_reg_info;

    while (curs->reg_name) {
        if (curs->addr == addr) {
            if (curs->len == len) {
                return curs->on_write(curs, buf, addr, len);
            } else {
                BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                      errinfo_feature("Whatever happens when "
                                                      "you use an inapproriate "
                                                      "length while writing to "
                                                      "a aica register") <<
                                      errinfo_guest_addr(addr) <<
                                      errinfo_length(len));
            }
        }
        curs++;
    }

    BOOST_THROW_EXCEPTION(UnimplementedError() <<
                          errinfo_feature("writing to one of the "
                                          "aica registers") <<
                          errinfo_guest_addr(addr));
}

static int
default_aica_reg_read_handler(struct aica_mapped_reg const *reg_info,
                              void *buf, addr32_t addr, unsigned len) {
    size_t idx = addr - ADDR_AICA_FIRST;
    memcpy(buf, idx + aica_regs, len);
    return 0;
}

static int
default_aica_reg_write_handler(struct aica_mapped_reg const *reg_info,
                               void const *buf, addr32_t addr, unsigned len) {
    size_t idx = addr - ADDR_AICA_FIRST;
    memcpy(idx + aica_regs, buf, len);
    return 0;
}

static int
warn_aica_reg_read_handler(struct aica_mapped_reg const *reg_info,
                           void *buf, addr32_t addr, unsigned len) {
    uint8_t val8;
    uint16_t val16;
    uint32_t val32;

    int ret_code = default_aica_reg_read_handler(reg_info, buf, addr, len);

    if (ret_code) {
        std::cerr << "WARNING: read from aica register " <<
            reg_info->reg_name << std::endl;
    } else {
        switch (reg_info->len) {
        case 1:
            memcpy(&val8, buf, sizeof(val8));
            std::cerr << "WARNING: read 0x" <<
                std::hex << std::setfill('0') << std::setw(2) <<
                unsigned(val8) << " from aica register " <<
                reg_info->reg_name << std::endl;
            break;
        case 2:
            memcpy(&val16, buf, sizeof(val16));
            std::cerr << "WARNING: read 0x" <<
                std::hex << std::setfill('0') << std::setw(4) <<
                unsigned(val16) << " from aica register " <<
                reg_info->reg_name << std::endl;
            break;
        case 4:
            memcpy(&val32, buf, sizeof(val32));
            std::cerr << "WARNING: read 0x" <<
                std::hex << std::setfill('0') << std::setw(8) <<
                unsigned(val32) << " from aica register " <<
                reg_info->reg_name << std::endl;
            break;
        default:
            std::cerr << "WARNING: read from aica register " <<
                reg_info->reg_name << std::endl;
        }
    }

    return 0;
}

static int
warn_aica_reg_write_handler(struct aica_mapped_reg const *reg_info,
                            void const *buf, addr32_t addr, unsigned len) {
    uint8_t val8;
    uint16_t val16;
    uint32_t val32;

    switch (reg_info->len) {
    case 1:
        memcpy(&val8, buf, sizeof(val8));
        std::cerr << "WARNING: writing 0x" <<
            std::hex << std::setfill('0') << std::setw(2) <<
            unsigned(val8) << " to aica register " <<
            reg_info->reg_name << std::endl;
        break;
    case 2:
        memcpy(&val16, buf, sizeof(val16));
        std::cerr << "WARNING: writing 0x" <<
            std::hex << std::setfill('0') << std::setw(4) <<
            unsigned(val16) << " to aica register " <<
            reg_info->reg_name << std::endl;
        break;
    case 4:
        memcpy(&val32, buf, sizeof(val32));
        std::cerr << "WARNING: writing 0x" <<
            std::hex << std::setfill('0') << std::setw(8) <<
            unsigned(val32) << " to aica register " <<
            reg_info->reg_name << std::endl;
        break;
    default:
        std::cerr << "WARNING: reading from aica register " <<
            reg_info->reg_name << std::endl;
    }

    return default_aica_reg_write_handler(reg_info, buf, addr, len);
}
