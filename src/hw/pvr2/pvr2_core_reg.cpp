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

#include <cstring>
#include <iostream>

#include "types.hpp"
#include "MemoryMap.hpp"
#include "BaseException.hpp"

#include "pvr2_core_reg.hpp"

static const size_t N_PVR2_CORE_REGS =
    ADDR_PVR2_CORE_LAST - ADDR_PVR2_CORE_FIRST + 1;
static reg32_t pvr2_core_regs[N_PVR2_CORE_REGS];

struct pvr2_core_mem_mapped_reg;

typedef int(*pvr2_core_reg_read_handler_t)(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void *buf, addr32_t addr, unsigned len);
typedef int(*pvr2_core_reg_write_handler_t)(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void const *buf, addr32_t addr, unsigned len);

static int
default_pvr2_core_reg_read_handler(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void *buf, addr32_t addr, unsigned len);
static int
default_pvr2_core_reg_write_handler(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void const *buf, addr32_t addr, unsigned len);
static int
warn_pvr2_core_reg_read_handler(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void *buf, addr32_t addr, unsigned len);
static int
warn_pvr2_core_reg_write_handler(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void const *buf, addr32_t addr, unsigned len);
static int
pvr2_core_read_only_reg_write_handler(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void const *buf, addr32_t addr, unsigned len);
static int
pvr2_core_id_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                          void *buf, addr32_t addr, unsigned len);

static struct pvr2_core_mem_mapped_reg {
    char const *reg_name;

    addr32_t addr;

    unsigned len;
    unsigned n_elem;

    pvr2_core_reg_read_handler_t on_read;
    pvr2_core_reg_write_handler_t on_write;
} pvr2_core_reg_info[] = {
    { "ID", 0x5f8000, 4, 1,
      pvr2_core_id_read_handler, pvr2_core_read_only_reg_write_handler },

    { "SOFTRESET", 0x5f8008, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },

    { "SPAN_SORT_CFG", 0x5f8030, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "VO_BORDER_COL", 0x5f8040, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FB_R_CTRL", 0x5f8044, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FB_W_CTROL", 0x5f8048, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FB_W_LINESTRIDE", 0x5f804c, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FB_R_SOF1", 0x5f8050, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FB_R_SOF2", 0x5f8054, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FB_R_SIZE", 0x5f805c, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FB_W_SOF1", 0x5f8060, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FB_W_SOF2", 0x5f8064, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FB_X_CLIP", 0x5f8068, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FB_Y_CLIP", 0x5f806c, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FPU_SHAD_SCALE", 0x5f8074, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FPU_CULL_VAL", 0x5f8078, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FPU_PARAM_CFG", 0x5f807c, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "HALF_OFFSET", 0x5f8080, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FPU_PERP_VAL", 0x5f8084, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "ISP_BACKGND_D", 0x5f8088, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "ISP_BACKGND_T", 0x5f808c, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_CLAMP_MAX", 0x5f80bc, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_CLAMP_MIN", 0x5f80c0, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "TEXT_CONTROL", 0x5f80e4, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "SCALER_CTL", 0x5f80f4, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FB_BURSTXTRL", 0x5f8110, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "Y_COEFF", 0x5f8118, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "SDRAM_REFRESH", 0x5f80a0, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "SDRAM_CFG", 0x5f80a8, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_COL_RAM", 0x5f80b0, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_COL_VERT", 0x5f80b4, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "FOG_DENSITY", 0x5f80b8, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "SPG_HBLANK_INT", 0x5f80c8, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "SPG_VBLANK_INT", 0x5f80cc, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "SPG_CONTROL", 0x5f80d0, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "SPG_HBLANK", 0x5f80d4, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "SPG_LOAD", 0x5f80d8, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "SPG_VBLANK", 0x5f80dc, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "SPG_WIDTH", 0x5f80e0, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "VO_CONTROL", 0x5f80e8, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "VO_STARTX", 0x5f80ec, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "VO_STARTY", 0x5f80f0, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "TA_OL_BASE", 0x5f8124, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "TA_ISP_BASE", 0x5f8128, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "TA_ISP_LIMIT", 0x5f8130, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "TA_OL_LIMIT", 0x5f812c, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "TA_GLOB_TILE_CLIP", 0x5f813c, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "TA_ALLOC_CTRL", 0x5f8140, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "TA_NEXT_OPB_INIT", 0x5f8164, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },
    { "TA_LIST_INIT", 0x5f8144, 4, 1,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },

    { "FOG_TABLE", 0x5f8200, 4, 0x80,
      warn_pvr2_core_reg_read_handler, warn_pvr2_core_reg_write_handler },

    { NULL }
};

int pvr2_core_reg_read(void *buf, size_t addr, size_t len) {
    struct pvr2_core_mem_mapped_reg *curs = pvr2_core_reg_info;

    while (curs->reg_name) {
        if ((addr >= curs->addr) &&
            (addr < (curs->addr + curs->len * curs->n_elem))) {
            if (curs->len >= len) {
                return curs->on_read(curs, buf, addr, len);
            } else {
                BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                      errinfo_feature("Whatever happens when "
                                                      "you use an inapproriate "
                                                      "length while reading "
                                                      "from a pvr2 core "
                                                      "register") <<
                                      errinfo_guest_addr(addr) <<
                                      errinfo_length(len));
            }
        }
        curs++;
    }

    BOOST_THROW_EXCEPTION(UnimplementedError() <<
                          errinfo_feature("reading from one of the "
                                          "pvr2 core registers") <<
                          errinfo_guest_addr(addr));
}

int pvr2_core_reg_write(void const *buf, size_t addr, size_t len) {
    struct pvr2_core_mem_mapped_reg *curs = pvr2_core_reg_info;

    while (curs->reg_name) {
        if ((addr >= curs->addr) &&
            (addr < (curs->addr + curs->len * curs->n_elem))) {
            if (curs->len >= len) {
                return curs->on_write(curs, buf, addr, len);
            } else {
                BOOST_THROW_EXCEPTION(UnimplementedError() <<
                                      errinfo_feature("Whatever happens when "
                                                      "you use an inapproriate "
                                                      "length while writing to "
                                                      "a pvr2 core register") <<
                                      errinfo_guest_addr(addr) <<
                                      errinfo_length(len));
            }
        }
        curs++;
    }

    BOOST_THROW_EXCEPTION(UnimplementedError() <<
                          errinfo_feature("writing to one of the "
                                          "pvr2 core registers") <<
                          errinfo_guest_addr(addr));
}

static int
default_pvr2_core_reg_read_handler(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void *buf, addr32_t addr, unsigned len) {
    size_t idx = (addr - ADDR_PVR2_CORE_FIRST) >> 2;
    memcpy(buf, idx + pvr2_core_regs, len);
    return 0;
}

static int
default_pvr2_core_reg_write_handler(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void const *buf, addr32_t addr, unsigned len) {
    size_t idx = (addr - ADDR_PVR2_CORE_FIRST) >> 2;
    memcpy(idx + pvr2_core_regs, buf, len);
    return 0;
}

static int
warn_pvr2_core_reg_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                                void *buf, addr32_t addr, unsigned len) {
    uint8_t val8;
    uint16_t val16;
    uint32_t val32;

    int ret_code = default_pvr2_core_reg_read_handler(reg_info, buf, addr, len);

    if (ret_code) {
        std::cerr << "WARNING: read from pvr2 core register " <<
            reg_info->reg_name << std::endl;
    } else {
        switch (len) {
        case 1:
            memcpy(&val8, buf, sizeof(val8));
            std::cerr << "WARNING: read 0x" <<
                std::hex << std::setfill('0') << std::setw(2) <<
                unsigned(val8) << " from pvr2 core register " <<
                reg_info->reg_name << std::endl;
            break;
        case 2:
            memcpy(&val16, buf, sizeof(val16));
            std::cerr << "WARNING: read 0x" <<
                std::hex << std::setfill('0') << std::setw(4) <<
                unsigned(val16) << " from pvr2 core register " <<
                reg_info->reg_name << std::endl;
            break;
        case 4:
            memcpy(&val32, buf, sizeof(val32));
            std::cerr << "WARNING: read 0x" <<
                std::hex << std::setfill('0') << std::setw(8) <<
                unsigned(val32) << " from pvr2 core register " <<
                reg_info->reg_name << std::endl;
            break;
        default:
            std::cerr << "WARNING: read from pvr2 core register " <<
                reg_info->reg_name << std::endl;
        }
    }

    return 0;
}

static int
warn_pvr2_core_reg_write_handler(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void const *buf, addr32_t addr, unsigned len) {
    uint8_t val8;
    uint16_t val16;
    uint32_t val32;

    switch (len) {
    case 1:
        memcpy(&val8, buf, sizeof(val8));
        std::cerr << "WARNING: writing 0x" <<
            std::hex << std::setfill('0') << std::setw(2) <<
            unsigned(val8) << " to pvr2 core register " <<
            reg_info->reg_name << std::endl;
        break;
    case 2:
        memcpy(&val16, buf, sizeof(val16));
        std::cerr << "WARNING: writing 0x" <<
            std::hex << std::setfill('0') << std::setw(4) <<
            unsigned(val16) << " to pvr2 core register " <<
            reg_info->reg_name << std::endl;
        break;
    case 4:
        memcpy(&val32, buf, sizeof(val32));
        std::cerr << "WARNING: writing 0x" <<
            std::hex << std::setfill('0') << std::setw(8) <<
            unsigned(val32) << " to pvr2 core register " <<
            reg_info->reg_name << std::endl;
        break;
    default:
        std::cerr << "WARNING: reading from pvr2 core register " <<
            reg_info->reg_name << std::endl;
    }

    return default_pvr2_core_reg_write_handler(reg_info, buf, addr, len);
}

static int
pvr2_core_id_read_handler(struct pvr2_core_mem_mapped_reg const *reg_info,
                          void *buf, addr32_t addr, unsigned len) {
    /* hardcoded hardware ID */
    uint32_t tmp = 0x17fd11db;

    memcpy(buf, &tmp, len);

    return 0;
}

static int
pvr2_core_read_only_reg_write_handler(
    struct pvr2_core_mem_mapped_reg const *reg_info,
    void const *buf, addr32_t addr, unsigned len) {
    BOOST_THROW_EXCEPTION(UnimplementedError() <<
                          errinfo_feature("whatever happens when you write to "
                                          "a read-only register") <<
                          errinfo_guest_addr(addr) << errinfo_length(len));
}
