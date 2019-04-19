/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016-2019 snickerbockers
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

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include "mem_code.h"
#include "washdc/error.h"
#include "log.h"

#include "boot_rom.h"

static DEF_ERROR_U32_ATTR(max_length)

// consider yourself warned: these functions don't do bounds-checking
static uint8_t boot_rom_read_8(addr32_t addr, void *ctxt);
static uint16_t boot_rom_read_16(addr32_t addr, void *ctxt);
static uint32_t boot_rom_read_32(addr32_t addr, void *ctxt);
static float boot_rom_read_float(addr32_t addr, void *ctxt);
static double boot_rom_read_double(addr32_t addr, void *ctxt);

static void boot_rom_write_8(addr32_t addr, uint8_t val, void *ctxt);
static void boot_rom_write_16(addr32_t addr, uint16_t val, void *ctxt);
static void boot_rom_write_32(addr32_t addr, uint32_t val, void *ctxt);
static void boot_rom_write_float(addr32_t addr, float val, void *ctxt);
static void boot_rom_write_double(addr32_t addr, double val, void *ctxt);

static int boot_rom_try_read_8(addr32_t addr, uint8_t *valp, void *ctxt);
static int boot_rom_try_read_16(addr32_t addr, uint16_t *valp, void *ctxt);
static int boot_rom_try_read_32(addr32_t addr, uint32_t *valp, void *ctxt);
static int boot_rom_try_read_float(addr32_t addr, float *valp, void *ctxt);
static int boot_rom_try_read_double(addr32_t addr, double *valp, void *ctxt);

static int boot_rom_try_write_8(addr32_t addr, uint8_t val, void *ctxt);
static int boot_rom_try_write_16(addr32_t addr, uint16_t val, void *ctxt);
static int boot_rom_try_write_32(addr32_t addr, uint32_t val, void *ctxt);
static int boot_rom_try_write_float(addr32_t addr, float val, void *ctxt);
static int boot_rom_try_write_double(addr32_t addr, double val, void *ctxt);

struct memory_interface boot_rom_intf = {
    .readdouble = boot_rom_read_double,
    .readfloat = boot_rom_read_float,
    .read32 = boot_rom_read_32,
    .read16 = boot_rom_read_16,
    .read8 = boot_rom_read_8,

    .writedouble = boot_rom_write_double,
    .writefloat = boot_rom_write_float,
    .write32 = boot_rom_write_32,
    .write16 = boot_rom_write_16,
    .write8 = boot_rom_write_8,

    .try_readdouble = boot_rom_try_read_double,
    .try_readfloat = boot_rom_try_read_float,
    .try_read32 = boot_rom_try_read_32,
    .try_read16 = boot_rom_try_read_16,
    .try_read8 = boot_rom_try_read_8,

    .try_writedouble = boot_rom_try_write_double,
    .try_writefloat = boot_rom_try_write_float,
    .try_write32 = boot_rom_try_write_32,
    .try_write16 = boot_rom_try_write_16,
    .try_write8 = boot_rom_try_write_8,
};

void boot_rom_init(struct boot_rom *rom, char const *path) {
    FILE *fp = fopen(path, "rb");

    if (!fp) {
        error_set_errno_val(errno);
        RAISE_ERROR(ERROR_FILE_IO);
        return;
    }

    if (fseek(fp, 0, SEEK_END) < 0) {
        error_set_errno_val(errno);
        RAISE_ERROR(ERROR_FILE_IO);
        fclose(fp);
        return;
    }

    long file_len = ftell(fp);

    if (file_len <= 0) {
        error_set_errno_val(errno);
        RAISE_ERROR(ERROR_FILE_IO);
        fclose(fp);
        return;
    }

    if (fseek(fp, 0, SEEK_SET) < 0) {
        error_set_errno_val(errno);
        RAISE_ERROR(ERROR_FILE_IO);
        fclose(fp);
        return;
    }

    rom->dat = (uint8_t*)malloc(file_len * sizeof(uint8_t));
    if (!rom->dat) {
        RAISE_ERROR(ERROR_FAILED_ALLOC);
        return;
    }

    if (fread(rom->dat, sizeof(uint8_t), file_len, fp) != file_len) {
        error_set_errno_val(errno);
        RAISE_ERROR(ERROR_FILE_IO);
        free(rom->dat);
        fclose(fp);
        return;
    }

    fclose(fp);

    rom->dat_len = file_len;

    if (file_len != BIOS_SZ_EXPECT) {
        LOG_WARN("WARNING - unexpected bios size (expected %d, got %d).  This "
                 "BIOS will still be loaded but it could cause issues.\n",
                 BIOS_SZ_EXPECT, (int)file_len);
    }
}

void boot_rom_cleanup(struct boot_rom *rom) {
    if (rom->dat)
        free(rom->dat);

    memset(rom, 0, sizeof(*rom));
}

static uint8_t boot_rom_read_8(addr32_t addr, void *ctxt) {
    struct boot_rom *rom = (struct boot_rom*)ctxt;

#ifdef INVARIANTS
    if (sizeof(uint8_t) - 1 + addr >= rom->dat_len) {
        error_set_address(addr);
        error_set_length(sizeof(uint8_t));
        error_set_max_length(rom->dat_len);
        RAISE_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }
#endif

    return rom->dat[addr];
}

#define DEF_BOOT_ROM_TRY_READ_HANDLER(type, postfix)                    \
    static int                                                          \
    boot_rom_try_read_##postfix(addr32_t addr,                          \
                                type *valp, void *ctxt) {               \
        struct boot_rom *rom = (struct boot_rom*)ctxt;                  \
        type  const *ptr = (type const*)rom->dat;                       \
        if (sizeof(type) - 1 + addr >= rom->dat_len)                    \
            return -1;                                                  \
        *valp = ptr[addr / sizeof(type)];                               \
        return 0;                                                       \
    }

DEF_BOOT_ROM_TRY_READ_HANDLER(uint8_t, 8)
DEF_BOOT_ROM_TRY_READ_HANDLER(uint16_t, 16)
DEF_BOOT_ROM_TRY_READ_HANDLER(uint32_t, 32)
DEF_BOOT_ROM_TRY_READ_HANDLER(float, float)
DEF_BOOT_ROM_TRY_READ_HANDLER(double, double)

#define DEF_BOOT_ROM_TRY_WRITE_HANDLER(type, postfix)           \
    static int                                                  \
    boot_rom_try_write_##postfix(addr32_t addr,                 \
                                 type val, void *ctxt) {        \
        return -1;                                              \
    }

DEF_BOOT_ROM_TRY_WRITE_HANDLER(uint8_t, 8)
DEF_BOOT_ROM_TRY_WRITE_HANDLER(uint16_t, 16)
DEF_BOOT_ROM_TRY_WRITE_HANDLER(uint32_t, 32)
DEF_BOOT_ROM_TRY_WRITE_HANDLER(float, float)
DEF_BOOT_ROM_TRY_WRITE_HANDLER(double, double)

static uint16_t boot_rom_read_16(addr32_t addr, void *ctxt) {
    struct boot_rom *rom = (struct boot_rom*)ctxt;
    uint16_t const *bios16 = (uint16_t const*)rom->dat;

#ifdef INVARIANTS
    if (sizeof(uint16_t) - 1 + addr >= rom->dat_len) {
        error_set_address(addr);
        error_set_length(sizeof(uint16_t));
        error_set_max_length(rom->dat_len);
        RAISE_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }
#endif

    return bios16[addr / 2];
}

static uint32_t boot_rom_read_32(addr32_t addr, void *ctxt) {
    struct boot_rom *rom = (struct boot_rom*)ctxt;
    uint32_t const *bios32 = (uint32_t const*)rom->dat;

#ifdef INVARIANTS
    if (sizeof(uint32_t) - 1 + addr >= rom->dat_len) {
        error_set_address(addr);
        error_set_length(sizeof(uint32_t));
        error_set_max_length(rom->dat_len);
        RAISE_ERROR(ERROR_MEM_OUT_OF_BOUNDS);
    }
#endif

    return bios32[addr / 4];
}

static float boot_rom_read_float(addr32_t addr, void *ctxt) {
    uint32_t tmp = boot_rom_read_32(addr, ctxt);
    float ret;
    memcpy(&ret, &tmp, sizeof(ret));
    return ret;
}

static double boot_rom_read_double(addr32_t addr, void *ctxt) {
    error_set_address(addr);
    error_set_length(8);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void boot_rom_write_8(addr32_t addr, uint8_t val, void *ctxt) {
    /*
     * I'm not sure what the correct response is when guest software tries to
     * write to the boot rom...
     */
    error_set_feature("proper response for when the guest "
                      "tries to write to the bios");
    error_set_address(addr);
    error_set_length(1);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void boot_rom_write_16(addr32_t addr, uint16_t val, void *ctxt) {
    /*
     * I'm not sure what the correct response is when guest software tries to
     * write to the boot rom...
     */
    error_set_feature("proper response for when the guest "
                      "tries to write to the bios");
    error_set_address(addr);
    error_set_length(2);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void boot_rom_write_32(addr32_t addr, uint32_t val, void *ctxt) {
    /*
     * I'm not sure what the correct response is when guest software tries to
     * write to the boot rom...
     */
    error_set_feature("proper response for when the guest "
                      "tries to write to the bios");
    error_set_address(addr);
    error_set_length(4);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void boot_rom_write_float(addr32_t addr, float val, void *ctxt) {
    /*
     * I'm not sure what the correct response is when guest software tries to
     * write to the boot rom...
     */
    error_set_feature("proper response for when the guest "
                      "tries to write to the bios");
    error_set_address(addr);
    error_set_length(4);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void boot_rom_write_double(addr32_t addr, double val, void *ctxt) {
    /*
     * I'm not sure what the correct response is when guest software tries to
     * write to the boot rom...
     */
    error_set_feature("proper response for when the guest "
                      "tries to write to the bios");
    error_set_address(addr);
    error_set_length(8);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}
