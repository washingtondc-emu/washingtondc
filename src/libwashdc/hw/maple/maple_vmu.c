/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2020 snickerbockers
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
#include <stdlib.h>

#include "maple.h"
#include "maple_device.h"
#include "washdc/error.h"
#include "log.h"
#include "maple_vmu.h"
#include "washdc/hostfile.h"

static void vmu_dev_cleanup(struct maple_device *dev);
static void vmu_dev_info(struct maple_device *dev,
                         struct maple_devinfo *output);
static void vmu_dev_get_cond(struct maple_device *dev,
                             struct maple_cond *cond);
static void vmu_dev_set_cond(struct maple_device *dev,
                             struct maple_setcond *cond);
static void
vmu_dev_bwrite(struct maple_device *dev, struct maple_bwrite *bwrite);
static void
vmu_dev_bread(struct maple_device *dev, struct maple_bread *bread);
static void
vmu_dev_bsync(struct maple_device *dev, struct maple_bsync *bsync);
static void
vmu_dev_meminfo(struct maple_device *dev, struct maple_meminfo *meminfo);

static void create_vmufs(uint8_t *datp);

static void flush_vmu(struct maple_device *dev);

/*
 * TODO: need to verify these on real hardware since I don't have access to any
 * of my dreamcasts right now
 *
 * I'm very confident "Visual Memory" is the correct identifier based on old
 * logs captured from real hardware, but the license string may or may not be
 * correct; I'm just assuming that it matches the string on Dreamcast
 * controller.
 */
#define MAPLE_VMU_STRING "Visual Memory                "
#define MAPLE_VMU_LICENSE                                      \
    "Produced By or Under License From SEGA ENTERPRISES,LTD.    "

struct maple_switch_table maple_vmu_switch_table = {
    .device_type = "vmu",
    .dev_cleanup = vmu_dev_cleanup,
    .dev_info = vmu_dev_info,
    .dev_get_cond = vmu_dev_get_cond,
    .dev_set_cond = vmu_dev_set_cond,
    .dev_bwrite = vmu_dev_bwrite,
    .dev_bread = vmu_dev_bread,
    .dev_bsync = vmu_dev_bsync,
    .dev_meminfo = vmu_dev_meminfo
};

int maple_vmu_init(struct maple_device *dev, char const *backing_path) {
    if (!(dev->enable && (dev->tp == MAPLE_DEVICE_VMU)))
        RAISE_ERROR(ERROR_INTEGRITY);

    memset(&dev->ctxt, 0, sizeof(dev->ctxt));

    dev->ctxt.vmu.datp = calloc(MAPLE_VMU_DAT_LEN, 1);

    dev->tp = MAPLE_DEVICE_VMU;
    dev->ctxt.vmu.backing_path = strdup(backing_path);

    LOG_INFO("VMU image path is \"%s\"\n", dev->ctxt.vmu.backing_path);

    washdc_hostfile file = washdc_hostfile_open(dev->ctxt.vmu.backing_path,
                                                WASHDC_HOSTFILE_READ |
                                                WASHDC_HOSTFILE_BINARY);
    if (file == WASHDC_HOSTFILE_INVALID) {
        LOG_INFO("Unable to open VMU image; creating new one.\n");
        create_vmufs(dev->ctxt.vmu.datp);
        flush_vmu(dev);
    } else {
        if (washdc_hostfile_read(file, dev->ctxt.vmu.datp,
                                 MAPLE_VMU_DAT_LEN) != MAPLE_VMU_DAT_LEN) {
            LOG_ERROR("ERROR READING FROM VMU; GAME WILL NOT BE SAVED\n");
            free(dev->ctxt.vmu.backing_path);
            dev->ctxt.vmu.backing_path = NULL;
            create_vmufs(dev->ctxt.vmu.datp);
        }
        washdc_hostfile_close(file);
    }

    return 0;
}

static void vmu_dev_cleanup(struct maple_device *dev) {
    if (!(dev->enable && (dev->tp == MAPLE_DEVICE_VMU)))
        RAISE_ERROR(ERROR_INTEGRITY);

    flush_vmu(dev);

    free(dev->ctxt.vmu.datp);
    free(dev->ctxt.vmu.backing_path);
}

static void flush_vmu(struct maple_device *dev) {
    if (!dev->ctxt.vmu.backing_path)
        return;

    washdc_hostfile file = washdc_hostfile_open(dev->ctxt.vmu.backing_path,
                                                WASHDC_HOSTFILE_WRITE |
                                                WASHDC_HOSTFILE_BINARY);
    if (file == WASHDC_HOSTFILE_INVALID) {
        LOG_ERROR("Unable to open VMU image file \"%s\"\n",
                  dev->ctxt.vmu.backing_path);
        return;
    } else {
        if (washdc_hostfile_write(file, dev->ctxt.vmu.datp,
                                  MAPLE_VMU_DAT_LEN) != MAPLE_VMU_DAT_LEN) {
            LOG_ERROR("Unable to write to VMU image file \"%s\"\n",
                      dev->ctxt.vmu.backing_path);
        }
        washdc_hostfile_close(file);
    }
}

static void create_vmufs(uint8_t *datp) {
    uint32_t fat_block[128];
    uint32_t root_block[128];

    memset(root_block, 0, sizeof(root_block));

    root_block[0] = 0x55555555;
    root_block[1] = 0x55555555;
    root_block[2] = 0x55555555;
    root_block[3] = 0x55555555;
    root_block[4] = 0xffffff01;
    root_block[5] = 0xff;
    root_block[12] = 0x27119819;
    root_block[13] = 0x4140000;
    root_block[16] = 0xff;
    root_block[17] = 0xfe00ff;
    root_block[18] = 0xf10001;
    root_block[19] = 0xd;
    root_block[20] = 0xc8;
    root_block[21] = 0x800000;

    unsigned idx;
    for (idx = 0; idx < 120; idx++)
        fat_block[idx] = 0xfffcfffc;
    fat_block[120] = 0xfdfffc;
    fat_block[121] = 0xf2fffa;
    fat_block[122] = 0xf400f3;
    fat_block[123] = 0xf600f5;
    fat_block[124] = 0xf800f7;
    fat_block[125] = 0xfa00f9;
    fat_block[126] = 0xfc00fb;
    fat_block[127] = 0xfffafffa;

    memcpy(datp + MAPLE_VMU_BLOCK_SZ * 254, fat_block, MAPLE_VMU_BLOCK_SZ);
    memcpy(datp + MAPLE_VMU_BLOCK_SZ * 255, root_block, MAPLE_VMU_BLOCK_SZ);
}

static void vmu_dev_info(struct maple_device *dev,
                         struct maple_devinfo *output) {
    if (!(dev->enable && (dev->tp == MAPLE_DEVICE_VMU)))
        RAISE_ERROR(ERROR_INTEGRITY);

    memset(output, 0, sizeof(*output));

    output->func = MAPLE_FUNC_MEMCARD | MAPLE_FUNC_LCD | MAPLE_FUNC_CLOCK;

    strncpy(output->dev_name, MAPLE_VMU_STRING, MAPLE_DEV_NAME_LEN);
    output->dev_name[MAPLE_DEV_NAME_LEN - 1] = '\0';
    strncpy(output->license, MAPLE_VMU_LICENSE, MAPLE_DEV_LICENSE_LEN);
    output->license[MAPLE_DEV_LICENSE_LEN - 1] = '\0';

    // TODO: verify on real hardware!
    output->func_data[0] = 0x403f7e7e;
    output->func_data[1] = 0x00100500;
    output->func_data[2] = 0x00410f00;
    output->area_code = 0xff;
    output->dir = 0;
    output->standby_power = 0x01ae;
    output->max_power = 0x01f4;
}

static void vmu_dev_get_cond(struct maple_device *dev,
                             struct maple_cond *cond) {
    if (!(dev->enable && (dev->tp == MAPLE_DEVICE_VMU)))
        RAISE_ERROR(ERROR_INTEGRITY);

    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void vmu_dev_set_cond(struct maple_device *dev,
                             struct maple_setcond *cond) {
    if (!(dev->enable && (dev->tp == MAPLE_DEVICE_VMU)))
        RAISE_ERROR(ERROR_INTEGRITY);

    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void
vmu_dev_bwrite(struct maple_device *dev, struct maple_bwrite *bwrite) {
    if (!(dev->enable && (dev->tp == MAPLE_DEVICE_VMU)))
        RAISE_ERROR(ERROR_INTEGRITY);

    if (bwrite->n_dwords >= 2) {
        if (bwrite->dat[0] == MAPLE_FUNC_MEMCARD) {
            unsigned blkid = maple_convert_endian(bwrite->dat[1]);

            /*
             * VMU blocks are nominally 512 bytes, but the bwrite command only
             * writes 1/4 of a block at a time.  phase is the index of which 1/4
             * to write.
             */
            unsigned block = blkid & 0xff;
            unsigned phase = (blkid >> 16) & 3;
            LOG_INFO("%s - request to write to block %02X phase %u\n", __func__, block, phase);

            if (bwrite->n_dwords != 34) {
                /*
                 * AFAIK it should only be possible to write
                 * 1/4 of a block, no more or less.
                 */
                error_set_feature("unsupported VMU write length");
                RAISE_ERROR(ERROR_UNIMPLEMENTED);
            }

            unsigned byteoffs = MAPLE_VMU_BLOCK_SZ * block + 128 * phase;
            memcpy(dev->ctxt.vmu.datp + sizeof(uint8_t) * byteoffs,
                   bwrite->dat + 2,
                   128);
        } else {
            LOG_ERROR("%s - malformed request (unknown function %08X)\n", __func__, bwrite->dat[0]);
        }
    } else {
        LOG_ERROR("%s - malformed request (not enough data)\n", __func__);
    }
}

static void
vmu_dev_bread(struct maple_device *dev, struct maple_bread *bread) {
    if (!(dev->enable && (dev->tp == MAPLE_DEVICE_VMU)))
        RAISE_ERROR(ERROR_INTEGRITY);

    if (bread->n_dwords_in >= 2) {
        if (bread->dat_in[0] == MAPLE_FUNC_MEMCARD) {
            unsigned block = maple_convert_endian(bread->dat_in[1]);
            LOG_INFO("%s - request to read block %02X\n", __func__, block);

            bread->n_dwords_out = MAPLE_BLOCK_N_DWORDS;
            bread->func_out = MAPLE_FUNC_MEMCARD;
            bread->block_out = bread->dat_in[1];

            if (block >= MAPLE_VMU_N_BLOCKS)
                RAISE_ERROR(ERROR_UNIMPLEMENTED);
            memcpy(bread->dat_out,
                   dev->ctxt.vmu.datp + sizeof(uint8_t) * MAPLE_VMU_BLOCK_SZ * block,
                   sizeof(bread->dat_out));
        } else {
            LOG_ERROR("%s - malformed request (unknown function %08X)\n", __func__, bread->dat_in[0]);
        }
    } else {
        LOG_ERROR("%s - malformed request (not enough data)\n", __func__);
    }
}

static void
vmu_dev_bsync(struct maple_device *dev, struct maple_bsync *bsync) {
    if (!(dev->enable && (dev->tp == MAPLE_DEVICE_VMU)))
        RAISE_ERROR(ERROR_INTEGRITY);

    flush_vmu(dev);
}

static void
vmu_dev_meminfo(struct maple_device *dev, struct maple_meminfo *meminfo) {
    // TODO: verify this on real hardware

    meminfo->func = MAPLE_FUNC_MEMCARD;

    meminfo->blkmax = 255;
    meminfo->blkmin = 0;
    meminfo->infpos = 255;
    meminfo->fatpos = 254;
    meminfo->fatsz = 1;
    meminfo->dirpos = 241;
    meminfo->dirsz = 13;
    meminfo->icon = 0;
    meminfo->datasz = 200;
}
