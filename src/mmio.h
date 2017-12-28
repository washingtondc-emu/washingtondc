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

#ifndef MMIO_H_
#define MMIO_H_

#include <stdint.h>
#include <stddef.h>

#include "types.h"

#define DECL_MMIO_REGION(name, len_bytes, beg_bytes, type)              \
    struct mmio_region_##name;                                          \
    typedef type(*mmio_region_##name##_read_handler)                    \
        (struct mmio_region_##name*,unsigned);                          \
    typedef void(*mmio_region_##name##_write_handler)                   \
    (struct mmio_region_##name*,unsigned,type);                         \
                                                                        \
    struct mmio_region_##name {                                         \
        mmio_region_##name##_read_handler                               \
        on_read[(len_bytes) / sizeof(type)];                            \
        mmio_region_##name##_write_handler                              \
        on_write[(len_bytes) / sizeof(type)];                           \
        type backing[(len_bytes) / sizeof(type)];                       \
        char const *names[(len_bytes) / sizeof(type)];                  \
    };                                                                  \

#define DEF_MMIO_REGION(name, len_bytes, beg_bytes, type)               \
    static struct mmio_region_##name mmio_region_##name;                \
                                                                        \
    __attribute__((unused)) static inline type                          \
    mmio_region_##name##_read(struct mmio_region_##name *region,        \
                              addr32_t addr) {                          \
        unsigned idx = (addr - (beg_bytes)) / sizeof(type);             \
        return region->on_read[idx](region, idx);                       \
    }                                                                   \
                                                                        \
    __attribute__((unused)) static inline void                          \
    mmio_region_##name##_write(struct mmio_region_##name *region,       \
                                  addr32_t addr, type val) {            \
        unsigned idx = (addr - beg_bytes) / sizeof(type);               \
        region->on_write[idx](region, idx, val);                        \
    }                                                                   \
                                                                        \
    __attribute__((unused)) static type                                 \
    mmio_region_##name##_read_error(struct mmio_region_##name *region,  \
                                    unsigned idx) {                     \
        error_set_length(sizeof(type));                                 \
        error_set_address(idx * sizeof(type));                          \
        error_set_feature("reading from some mmio register");           \
        RAISE_ERROR(ERROR_UNIMPLEMENTED);                               \
    }                                                                   \
                                                                        \
    __attribute__((unused)) static void                                 \
    mmio_region_##name##_write_error(struct mmio_region_##name *region, \
                                     unsigned idx, type val) {          \
        error_set_length(sizeof(type));                                 \
        error_set_address(idx * sizeof(type));                          \
        error_set_feature("writing to some mmio register");             \
        RAISE_ERROR(ERROR_UNIMPLEMENTED);                               \
    }                                                                   \
                                                                        \
    __attribute__((unused)) static void                                 \
    mmio_region_##name##_readonly_write_error(                          \
        struct mmio_region_##name *region,                              \
        unsigned idx, type val) {                                       \
        error_set_length(sizeof(type));                                 \
        error_set_address(idx * sizeof(type));                          \
        error_set_feature("proper response for writing to a "           \
                          "read-only register");                        \
        RAISE_ERROR(ERROR_UNIMPLEMENTED);                               \
    }                                                                   \
                                                                        \
    __attribute__((unused)) static type                                 \
    mmio_region_##name##_writeonly_read_error(                          \
        struct mmio_region_##name *region, unsigned idx) {              \
        error_set_length(sizeof(type));                                 \
        error_set_address(idx * sizeof(type));                          \
        error_set_feature("proper response for reading from a "         \
                          "write-only register");                       \
        RAISE_ERROR(ERROR_UNIMPLEMENTED);                               \
    }                                                                   \
                                                                        \
    __attribute__((unused)) static type                                 \
    mmio_region_##name##_warn_read_handler(struct mmio_region_##name *region, \
                                           unsigned idx) {              \
        type ret = region->backing[idx];                                \
        LOG_DBG("Read from \"%s\": 0x%08x\n",                           \
                region->names[idx], (unsigned)ret);                     \
        return ret;                                                     \
    }                                                                   \
                                                                        \
    __attribute__((unused)) static void                                 \
    mmio_region_##name##_warn_write_handler(struct mmio_region_##name *region, \
                                            unsigned idx, type val) {   \
        LOG_DBG("Write to \"%s\": 0x%08x\n",                            \
                region->names[idx], (unsigned)val);                     \
        region->backing[idx] = val;                                     \
    }                                                                   \
                                                                        \
    __attribute__((unused)) static type                                 \
    mmio_region_##name##_silent_read_handler(                           \
        struct mmio_region_##name *region,                              \
        unsigned idx) {                                                 \
        return region->backing[idx];                                    \
    }                                                                   \
                                                                        \
    __attribute__((unused)) static void                                 \
    mmio_region_##name##_silent_write_handler(                          \
        struct mmio_region_##name *region,                              \
        unsigned idx, type val) {                                       \
        region->backing[idx] = val;                                     \
    }                                                                   \
                                                                        \
    __attribute__((unused))                                             \
    static void init_mmio_region_##name(struct mmio_region_##name *region) { \
        memset(region, 0, sizeof(*region));                             \
        size_t cell_no;                                                 \
        for (cell_no = 0; cell_no < ((len_bytes) / sizeof(type)); cell_no++) { \
            region->names[cell_no] = "UNKNOWN_REGISTER";                \
            region->on_read[cell_no] = mmio_region_##name##_read_error; \
            region->on_write[cell_no] = mmio_region_##name##_write_error; \
        }                                                               \
    }                                                                   \
                                                                        \
    __attribute__((unused))                                             \
    static void cleanup_mmio_region_##name(                             \
        struct mmio_region_##name *region) {                            \
        memset(region, 0, sizeof(*region));                             \
    }                                                                   \
                                                                        \
    __attribute__((unused)) static void                                 \
    mmio_region_##name##_init_cell(struct mmio_region_##name *region,   \
                                   char const *name, addr32_t addr,     \
                                   mmio_region_##name##_read_handler on_read, \
                                   mmio_region_##name##_write_handler   \
                                   on_write) {                          \
        unsigned idx = (addr - (beg_bytes)) / sizeof(type);             \
        region->names[idx] = name;                                      \
        region->on_read[idx] = on_read;                                 \
        region->on_write[idx] = on_write;                               \
    }

#endif
