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
    unsigned n_elem;

    aica_reg_read_handler_t on_read;
    aica_reg_write_handler_t on_write;
} aica_reg_info[] = {
    /*
     * two-byte register containing VREG and some other weird unrelated stuff
     * that is part of AICA for reasons which I cannot fathom
     */
    { "AICA_00700000", 0x00700000, 4, 1,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_00700004", 0x00700004, 4, 1,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_00700008", 0x00700008, 4, 1,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_0070000c", 0x0070000c, 4, 1,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_00700010", 0x00700010, 4, 1,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_00700014", 0x00700014, 4, 1,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_00700018", 0x00700018, 4, 1,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_0070001c", 0x0070001c, 4, 1,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_00700020", 0x00700020, 4, 1,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_00700024", 0x00700024, 4, 1,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_00700028", 0x00700028, 4, 1,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_FLV0", 0x0070002c, 4, 1,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_FLV1", 0x00700030, 4, 1,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_FLV2", 0x00700034, 4, 1,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_FLV3", 0x00700038, 4, 1,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_FLV4", 0x0070003c, 4, 1,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_00700040", 0x00700040, 4, 1,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_00700044", 0x00700044, 4, 1,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_SLOT_CONTROL", 0x700080, 4, 0x7d2,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_DSP_OUT", 0x702000, 4, 18,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_COEF",     0x00703000, 4, 128,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_MADDRS", 0x703200, 4, 64,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_MPRO", 0x703400, 4, 4 * 128,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_TEMP", 0x704000, 4, 0x100,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_MEMS", 0x704400, 4, 0x40,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_MIXS", 0x704500, 4, 0x20,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_EFREG", 0x704580, 4, 0x10,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_EXTS", 0x7045c0, 4, 0x2,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_00702c00", 0x00702c00, 4, 1,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_00702800", 0x00702800, 4, 1,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_0070289c", 0x0070289c, 4, 1,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_007028a0", 0x007028a0, 4, 1,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_007028a4", 0x007028a4, 4, 1,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_007028b4", 0x007028b4, 4, 1,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },
    { "AICA_007028bc", 0x007028bc, 4, 1,
      warn_aica_reg_read_handler, warn_aica_reg_write_handler },

    { NULL }
};

int aica_reg_read(void *buf, size_t addr, size_t len) {
    struct aica_mapped_reg *curs = aica_reg_info;

    while (curs->reg_name) {
        if ((addr >= curs->addr) &&
            (addr < (curs->addr + curs->len * curs->n_elem))) {
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
                          errinfo_guest_addr(addr) <<
                          errinfo_length(len));
}

int aica_reg_write(void const *buf, size_t addr, size_t len) {
    struct aica_mapped_reg *curs = aica_reg_info;

    while (curs->reg_name) {
        if ((addr >= curs->addr) &&
            (addr < (curs->addr + curs->len * curs->n_elem))) {
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
                          errinfo_guest_addr(addr) <<
                          errinfo_length(len));
}

static int
default_aica_reg_read_handler(struct aica_mapped_reg const *reg_info,
                              void *buf, addr32_t addr, unsigned len) {
    size_t idx = (addr - ADDR_AICA_FIRST) >> 2;
    memcpy(buf, idx + aica_regs, len);
    return 0;
}

static int
default_aica_reg_write_handler(struct aica_mapped_reg const *reg_info,
                               void const *buf, addr32_t addr, unsigned len) {
    size_t idx = (addr - ADDR_AICA_FIRST) >> 2;
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
            reg_info->reg_name << "(offset is " <<
            (addr - reg_info->addr) / reg_info->len << ")" << std::endl;
    } else {
        switch (reg_info->len) {
        case 1:
            memcpy(&val8, buf, sizeof(val8));
            std::cerr << "WARNING: read 0x" <<
                std::hex << std::setfill('0') << std::setw(2) <<
                unsigned(val8) << " from aica register " <<
                reg_info->reg_name << "(offset is " <<
            (addr - reg_info->addr) / reg_info->len << ")" << std::endl;
            break;
        case 2:
            memcpy(&val16, buf, sizeof(val16));
            std::cerr << "WARNING: read 0x" <<
                std::hex << std::setfill('0') << std::setw(4) <<
                unsigned(val16) << " from aica register " <<
                reg_info->reg_name << "(offset is " <<
            (addr - reg_info->addr) / reg_info->len << ")" << std::endl;
            break;
        case 4:
            memcpy(&val32, buf, sizeof(val32));
            std::cerr << "WARNING: read 0x" <<
                std::hex << std::setfill('0') << std::setw(8) <<
                unsigned(val32) << " from aica register " <<
                reg_info->reg_name << "(offset is " <<
            (addr - reg_info->addr) / reg_info->len << ")" << std::endl;
            break;
        default:
            std::cerr << "WARNING: read from aica register " <<
                reg_info->reg_name << "(offset is " <<
            (addr - reg_info->addr) / reg_info->len << ")" << std::endl;
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
            reg_info->reg_name << "(offset is " <<
            (addr - reg_info->addr) / reg_info->len << ")" << std::endl;
        break;
    case 2:
        memcpy(&val16, buf, sizeof(val16));
        std::cerr << "WARNING: writing 0x" <<
            std::hex << std::setfill('0') << std::setw(4) <<
            unsigned(val16) << " to aica register " <<
            reg_info->reg_name << "(offset is " <<
            (addr - reg_info->addr) / reg_info->len << ")" << std::endl;
        break;
    case 4:
        memcpy(&val32, buf, sizeof(val32));
        std::cerr << "WARNING: writing 0x" <<
            std::hex << std::setfill('0') << std::setw(8) <<
            unsigned(val32) << " to aica register " <<
            reg_info->reg_name << "(offset is " <<
            (addr - reg_info->addr) / reg_info->len << ")" << std::endl;
        break;
    default:
        std::cerr << "WARNING: reading from aica register " <<
            reg_info->reg_name << "(offset is " <<
            (addr - reg_info->addr) / reg_info->len << ")" << std::endl;
    }

    return default_aica_reg_write_handler(reg_info, buf, addr, len);
}
