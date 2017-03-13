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
#include "hw/gdrom/gdrom_reg.hpp"

#include "sys_block.hpp"

static const size_t N_SYS_REGS = ADDR_SYS_LAST - ADDR_SYS_FIRST + 1;
static reg32_t sys_regs[N_SYS_REGS];

struct sys_mapped_reg;

typedef int(*sys_reg_read_handler_t)(struct sys_mapped_reg const *reg_info,
                                     void *buf, addr32_t addr, unsigned len);
typedef int(*sys_reg_write_handler_t)(struct sys_mapped_reg const *reg_info,
                                      void const *buf, addr32_t addr,
                                      unsigned len);

static int default_sys_reg_read_handler(struct sys_mapped_reg const *reg_info,
                                        void *buf, addr32_t addr, unsigned len);
static int default_sys_reg_write_handler(struct sys_mapped_reg const *reg_info,
                                         void const *buf, addr32_t addr,
                                         unsigned len);
static int warn_sys_reg_read_handler(struct sys_mapped_reg const *reg_info,
                                     void *buf, addr32_t addr, unsigned len);
static int warn_sys_reg_write_handler(struct sys_mapped_reg const *reg_info,
                                      void const *buf, addr32_t addr,
                                      unsigned len);

/* write handler for registers that should be read-only */
static int sys_read_only_reg_write_handler(struct sys_mapped_reg const *reg_info,
                                           void const *buf, addr32_t addr,
                                           unsigned len);

static int ignore_sys_reg_write_handler(struct sys_mapped_reg const *reg_info,
                                        void const *buf, addr32_t addr,
                                        unsigned len);

static int sys_reg_istext_read_handler(struct sys_mapped_reg const *reg_info,
                                       void *buf, addr32_t addr, unsigned len);

static struct sys_mapped_reg {
    char const *reg_name;

    addr32_t addr;

    unsigned len;

    sys_reg_read_handler_t on_read;
    sys_reg_write_handler_t on_write;
} sys_reg_info[] = {
    { "SB_C2DSTAT", 0x005f6800, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },
    { "SB_C2DLEN", 0x005f6804, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },
    { "SB_C2DST", 0x005f6808, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },
    { "SB_SDSTAW", 0x5f6810, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },
    { "SB_SDBAAW", 0x5f6814, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },
    { "SB_SDWLT", 0x5f6818, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },
    { "SB_SDLAS", 0x5f681c, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },
    { "SB_SDST", 0x5f6820, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },
    { "SB_DBREQM", 0x5f6840, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },
    { "SB_BAVLWC", 0x5f6844, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },
    { "SB_C2DPRYC", 0x5f6848, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },
    /* TODO: spec says default val if SB_C2DMAXL is 1, but bios writes 0 ? */
    { "SB_C2DMAXL", 0x5f684c, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },
    { "SB_LMMODE0", 0x5f6884, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },
    { "SB_LMMODE1", 0x5f6888, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },
    { "SB_FFST", 0x5f688c, 4,
      default_sys_reg_read_handler, sys_read_only_reg_write_handler },
    /* TODO: spec says default val if SB_RBSPLT's MSB is 0, but bios writes 1 */
    { "SB_RBSPLT", 0x5f68a0, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },
    /* I can't  seem to find any info on what the register at 0x5f68a4 is */
    { "UNKNOWN_REG_5f68a4", 0x5f68a4, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },
    /* I can't  seem to find any info on what the register at 0x5f68ac is */
    { "UNKNOWN_REG_5f68ac", 0x5f68ac, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },

    { "SB_IML2NRM", 0x5f6910, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },
    { "SB_IML2EXT", 0x5f6914, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },
    { "SB_IML2ERR", 0x5f6918, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },
    { "SB_IML4NRM", 0x5f6920, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },
    { "SB_IML4EXT", 0x5f6924, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },
    { "SB_IML4ERR", 0x5f6928, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },
    { "SB_IML6NRM", 0x5f6930, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },
    { "SB_IML6EXT", 0x5f6934, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },
    { "SB_IML6ERR", 0x5f6938, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },

    { "SB_PDTNRM", 0x5f6940, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },
    { "SB_PDTEXT", 0x5f6944, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },

    /* arguably these ones should go into their own hw/g2 subdirectory... */
    { "SB_G2DTNRM", 0x5f6950, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },
    { "SB_G2DTEXT", 0x5f6954, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },

    { "SB_ISTNRM", 0x5f6900, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },
    { "SB_ISTEXT", 0x5f6904, 4,
      sys_reg_istext_read_handler, ignore_sys_reg_write_handler },
    { "SB_ISTERR", 0x5f6908, 4,
      warn_sys_reg_read_handler, warn_sys_reg_write_handler },

    { NULL }
};

static int default_sys_reg_read_handler(struct sys_mapped_reg const *reg_info,
                                        void *buf, addr32_t addr,
                                        unsigned len) {
    size_t idx = (addr - ADDR_SYS_FIRST) >> 2;
    memcpy(buf, idx + sys_regs, len);
    return 0;
}

static int default_sys_reg_write_handler(struct sys_mapped_reg const *reg_info,
                                         void const *buf, addr32_t addr,
                                         unsigned len) {
    size_t idx = (addr - ADDR_SYS_FIRST) >> 2;
    memcpy(idx + sys_regs, buf, len);
    return 0;
}

static int warn_sys_reg_read_handler(struct sys_mapped_reg const *reg_info,
                                     void *buf, addr32_t addr, unsigned len) {
    uint8_t val8;
    uint16_t val16;
    uint32_t val32;

    int ret_code = default_sys_reg_read_handler(reg_info, buf, addr, len);

    if (ret_code) {
        std::cerr << "WARNING: read from system register " <<
            reg_info->reg_name << std::endl;
    } else {
        switch (reg_info->len) {
        case 1:
            memcpy(&val8, buf, sizeof(val8));
            std::cerr << "WARNING: read 0x" <<
                std::hex << std::setfill('0') << std::setw(2) <<
                unsigned(val8) << " from system register " <<
                reg_info->reg_name << std::endl;
            break;
        case 2:
            memcpy(&val16, buf, sizeof(val16));
            std::cerr << "WARNING: read 0x" <<
                std::hex << std::setfill('0') << std::setw(4) <<
                unsigned(val16) << " from system register " <<
                reg_info->reg_name << std::endl;
            break;
        case 4:
            memcpy(&val32, buf, sizeof(val32));
            std::cerr << "WARNING: read 0x" <<
                std::hex << std::setfill('0') << std::setw(8) <<
                unsigned(val32) << " from system register " <<
                reg_info->reg_name << std::endl;
            break;
        default:
            std::cerr << "WARNING: read from system register " <<
                reg_info->reg_name << std::endl;
        }
    }

    return 0;
}

static int warn_sys_reg_write_handler(struct sys_mapped_reg const *reg_info,
                                      void const *buf, addr32_t addr,
                                      unsigned len) {
    uint8_t val8;
    uint16_t val16;
    uint32_t val32;

    switch (reg_info->len) {
    case 1:
        memcpy(&val8, buf, sizeof(val8));
        std::cerr << "WARNING: writing 0x" <<
            std::hex << std::setfill('0') << std::setw(2) <<
            unsigned(val8) << " to system register " <<
            reg_info->reg_name << std::endl;
        break;
    case 2:
        memcpy(&val16, buf, sizeof(val16));
        std::cerr << "WARNING: writing 0x" <<
            std::hex << std::setfill('0') << std::setw(4) <<
            unsigned(val16) << " to system register " <<
            reg_info->reg_name << std::endl;
        break;
    case 4:
        memcpy(&val32, buf, sizeof(val32));
        std::cerr << "WARNING: writing 0x" <<
            std::hex << std::setfill('0') << std::setw(8) <<
            unsigned(val32) << " to system register " <<
            reg_info->reg_name << std::endl;
        break;
    default:
        std::cerr << "WARNING: writing to system register " <<
            reg_info->reg_name << std::endl;
    }

    return default_sys_reg_write_handler(reg_info, buf, addr, len);
}

int sys_block_read(void *buf, size_t addr, size_t len) {
    struct sys_mapped_reg *curs = sys_reg_info;

    while (curs->reg_name) {
        if (curs->addr == addr) {
            if (curs->len == len) {
                return curs->on_read(curs, buf, addr, len);
            } else {
                BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                      errinfo_feature("Whatever happens when "
                                                      "you use an inapproriate "
                                                      "length while writing to "
                                                      "a system register") <<
                                      errinfo_guest_addr(addr) <<
                                      errinfo_length(len));
            }
        }
        curs++;
    }

    BOOST_THROW_EXCEPTION(UnimplementedError() <<
                          errinfo_feature("accessing one of the "
                                          "system block registers") <<
                          errinfo_guest_addr(addr));
}

int sys_block_write(void const *buf, size_t addr, size_t len) {
    struct sys_mapped_reg *curs = sys_reg_info;

    while (curs->reg_name) {
        if (curs->addr == addr) {
            if (curs->len == len) {
                return curs->on_write(curs, buf, addr, len);
            } else {
                BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                      errinfo_feature("Whatever happens when "
                                                      "you use an inapproriate "
                                                      "length while reading "
                                                      "from a system "
                                                      "register") <<
                                      errinfo_guest_addr(addr) <<
                                      errinfo_length(len));
            }
        }
        curs++;
    }

    BOOST_THROW_EXCEPTION(UnimplementedError() <<
                          errinfo_feature("accessing one of the "
                                          "system block registers") <<
                          errinfo_guest_addr(addr));
}

static int sys_read_only_reg_write_handler(struct sys_mapped_reg const *reg_info,
                                           void const *buf, addr32_t addr,
                                           unsigned len) {
    BOOST_THROW_EXCEPTION(UnimplementedError() <<
                          errinfo_feature("Whatever happens when you try to "
                                          "write to a read-only system-block "
                                          "register") <<
                          errinfo_guest_addr(addr) <<
                          errinfo_length(len));
}

static int ignore_sys_reg_write_handler(struct sys_mapped_reg const *reg_info,
                                        void const *buf, addr32_t addr,
                                        unsigned len) {
    return 0;
}

static int sys_reg_istext_read_handler(struct sys_mapped_reg const *reg_info,
                                       void *buf, addr32_t addr, unsigned len) {
    reg32_t val = gdrom_irq() ? 1 : 0;

    memcpy(buf, &val, len < sizeof(val) ? len : sizeof(val));

    std::cout << "reading " << std::hex << val << " from ISTEXT" << std::endl;

    return 0;
}
