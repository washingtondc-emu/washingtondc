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

#include "maple_reg.hpp"

static const size_t N_MAPLE_REGS = ADDR_MAPLE_LAST - ADDR_MAPLE_FIRST + 1;
static reg32_t maple_regs[N_MAPLE_REGS];

struct maple_mapped_reg;

typedef int(*maple_reg_read_handler_t)(struct maple_mapped_reg const *reg_info,
                                       void *buf, addr32_t addr, unsigned len);
typedef int(*maple_reg_write_handler_t)(struct maple_mapped_reg const *reg_info,
                                        void const *buf, addr32_t addr,
                                        unsigned len);
static int
default_maple_reg_read_handler(struct maple_mapped_reg const *reg_info,
                               void *buf, addr32_t addr, unsigned len);
static int
default_maple_reg_write_handler(struct maple_mapped_reg const *reg_info,
                                void const *buf, addr32_t addr, unsigned len);

static int
warn_maple_reg_read_handler(struct maple_mapped_reg const *reg_info,
                            void *buf, addr32_t addr, unsigned len);
static int
warn_maple_reg_write_handler(struct maple_mapped_reg const *reg_info,
                             void const *buf, addr32_t addr, unsigned len);

static struct maple_mapped_reg {
    char const *reg_name;

    addr32_t addr;

    unsigned len;

    maple_reg_read_handler_t on_read;
    maple_reg_write_handler_t on_write;
} maple_reg_info[] = {
    { "SB_MDSTAR", 0x5f6c04, 4,
      warn_maple_reg_read_handler, warn_maple_reg_write_handler },
    { "SB_MDTSEL", 0x5f6c10, 4,
      warn_maple_reg_read_handler, warn_maple_reg_write_handler },
    { "SB_MDEN", 0x5f6c14, 4,
      warn_maple_reg_read_handler, warn_maple_reg_write_handler },
    { "SB_MDST", 0x5f6c18, 4,
      warn_maple_reg_read_handler, warn_maple_reg_write_handler },
    { "SB_MSYS", 0x5f6c80, 4,
      warn_maple_reg_read_handler, warn_maple_reg_write_handler },
    { "SB_MDAPRO", 0x5f6c8c, 4,
      warn_maple_reg_read_handler, warn_maple_reg_write_handler },
    { "SB_MMSEL", 0x5f6ce8, 4,
      warn_maple_reg_read_handler, warn_maple_reg_write_handler },
    { NULL }
};

int maple_reg_read(void *buf, size_t addr, size_t len) {
    struct maple_mapped_reg *curs = maple_reg_info;

    while (curs->reg_name) {
        if (curs->addr == addr) {
            if (curs->len == len) {
                return curs->on_read(curs, buf, addr, len);
            } else {
                BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                      errinfo_feature("Whatever happens when "
                                                      "you use an inapproriate "
                                                      "length while reading "
                                                      "from a maple "
                                                      "register") <<
                                      errinfo_guest_addr(addr) <<
                                      errinfo_length(len));
            }
        }
        curs++;
    }

    BOOST_THROW_EXCEPTION(UnimplementedError() <<
                          errinfo_feature("reading from one of the "
                                          "maple registers") <<
                          errinfo_guest_addr(addr));
}

int maple_reg_write(void const *buf, size_t addr, size_t len) {
    struct maple_mapped_reg *curs = maple_reg_info;

    while (curs->reg_name) {
        if (curs->addr == addr) {
            if (curs->len == len) {
                return curs->on_write(curs, buf, addr, len);
            } else {
                BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                      errinfo_feature("Whatever happens when "
                                                      "you use an inapproriate "
                                                      "length while writing to "
                                                      "a maple register") <<
                                      errinfo_guest_addr(addr) <<
                                      errinfo_length(len));
            }
        }
        curs++;
    }

    BOOST_THROW_EXCEPTION(UnimplementedError() <<
                          errinfo_feature("writing to one of the "
                                          "maple registers") <<
                          errinfo_guest_addr(addr));
}

static int
default_maple_reg_read_handler(struct maple_mapped_reg const *reg_info,
                               void *buf, addr32_t addr, unsigned len) {
    size_t idx = (addr - ADDR_MAPLE_FIRST) >> 2;
    memcpy(buf, idx + maple_regs, len);
    return 0;
}

static int
default_maple_reg_write_handler(struct maple_mapped_reg const *reg_info,
                                void const *buf, addr32_t addr, unsigned len) {
    size_t idx = (addr - ADDR_MAPLE_FIRST) >> 2;
    memcpy(idx + maple_regs, buf, len);
    return 0;
}

static int
warn_maple_reg_read_handler(struct maple_mapped_reg const *reg_info,
                            void *buf, addr32_t addr, unsigned len) {
    uint8_t val8;
    uint16_t val16;
    uint32_t val32;

    int ret_code = default_maple_reg_read_handler(reg_info, buf, addr, len);

    if (ret_code) {
        std::cerr << "WARNING: read from maple register " <<
            reg_info->reg_name << std::endl;
    } else {
        switch (reg_info->len) {
        case 1:
            memcpy(&val8, buf, sizeof(val8));
            std::cerr << "WARNING: read 0x" <<
                std::hex << std::setfill('0') << std::setw(2) <<
                unsigned(val8) << " from maple register " <<
                reg_info->reg_name << std::endl;
            break;
        case 2:
            memcpy(&val16, buf, sizeof(val16));
            std::cerr << "WARNING: read 0x" <<
                std::hex << std::setfill('0') << std::setw(4) <<
                unsigned(val16) << " from maple register " <<
                reg_info->reg_name << std::endl;
            break;
        case 4:
            memcpy(&val32, buf, sizeof(val32));
            std::cerr << "WARNING: read 0x" <<
                std::hex << std::setfill('0') << std::setw(8) <<
                unsigned(val32) << " from maple register " <<
                reg_info->reg_name << std::endl;
            break;
        default:
            std::cerr << "WARNING: read from maple register " <<
                reg_info->reg_name << std::endl;
        }
    }

    return 0;
}

static int
warn_maple_reg_write_handler(struct maple_mapped_reg const *reg_info,
                             void const *buf, addr32_t addr, unsigned len) {
    uint8_t val8;
    uint16_t val16;
    uint32_t val32;

    switch (reg_info->len) {
    case 1:
        memcpy(&val8, buf, sizeof(val8));
        std::cerr << "WARNING: writing 0x" <<
            std::hex << std::setfill('0') << std::setw(2) <<
            unsigned(val8) << " to maple register " <<
            reg_info->reg_name << std::endl;
        break;
    case 2:
        memcpy(&val16, buf, sizeof(val16));
        std::cerr << "WARNING: writing 0x" <<
            std::hex << std::setfill('0') << std::setw(4) <<
            unsigned(val16) << " to maple register " <<
            reg_info->reg_name << std::endl;
        break;
    case 4:
        memcpy(&val32, buf, sizeof(val32));
        std::cerr << "WARNING: writing 0x" <<
            std::hex << std::setfill('0') << std::setw(8) <<
            unsigned(val32) << " to maple register " <<
            reg_info->reg_name << std::endl;
        break;
    default:
        std::cerr << "WARNING: reading from maple register " <<
            reg_info->reg_name << std::endl;
    }

    return default_maple_reg_write_handler(reg_info, buf, addr, len);
}
