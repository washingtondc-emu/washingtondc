/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016-2020 snickerbockers
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

#include "real_ticks.h"

// need this for usleep on posix, Sleep on windows
#ifdef _MSC_VER
#include "i_hate_windows.h"
#else
#include <unistd.h>
#endif

#include <errno.h>
#include <time.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>

#include "config.h"
#include "washdc/error.h"
#include "hw/flash_mem.h"
#include "dc_sched.h"
#include "hw/pvr2/spg.h"
#include "washdc/MemoryMap.h"
#include "gfx/gfx.h"
#include "hw/aica/aica_rtc.h"
#include "hw/gdrom/gdrom.h"
#include "hw/maple/maple.h"
#include "hw/maple/maple_device.h"
#include "hw/maple/maple_controller.h"
#include "hw/pvr2/framebuffer.h"
#include "hw/pvr2/pvr2_tex_mem.h"
#include "hw/pvr2/pvr2_ta.h"
#include "log.h"
#include "hw/sh4/sh4_read_inst.h"
#include "hw/sh4/sh4_jit.h"
#include "hw/pvr2/pvr2.h"
#include "hw/pvr2/pvr2_reg.h"
#include "hw/pvr2/pvr2_yuv.h"
#include "hw/sys/sys_block.h"
#include "hw/aica/aica.h"
#include "hw/aica/aica_rtc.h"
#include "hw/g1/g1.h"
#include "hw/g1/g1_reg.h"
#include "hw/g2/g2.h"
#include "hw/g2/g2_reg.h"
#include "hw/g2/modem.h"
#include "hw/g2/external_dev.h"
#include "hw/maple/maple_reg.h"
#include "jit/code_block.h"
#include "jit/jit_intp/code_block_intp.h"
#include "jit/code_cache.h"
#include "jit/jit.h"
#include "hw/boot_rom.h"
#include "hw/arm7/arm7.h"
#include "title.h"
#include "mount.h"
#include "gdi.h"
#include "cdi.h"
#include "washdc/win.h"
#include "washdc/sound_intf.h"
#include "sound.h"
#include "washdc/hostfile.h"
#include "hw/sys/holly_intc.h"

#ifdef ENABLE_TCP_SERIAL
#include "serial_server.h"
#endif

#ifdef ENABLE_JIT_X86_64
#include "jit/x86_64/native_dispatch.h"
#include "jit/x86_64/native_mem.h"
#include "jit/x86_64/exec_mem.h"
#endif

#include "dreamcast.h"

static struct Sh4 cpu;
static struct Memory dc_mem;
static struct memory_map mem_map;
static struct boot_rom firmware;
static struct flash_mem flash_mem;
static struct aica_rtc rtc;
static struct arm7 arm7;
static struct memory_map arm7_mem_map;
static struct aica aica;
static struct gdrom_ctxt gdrom;
static struct pvr2 dc_pvr2;
static struct maple maple;
static struct sys_block_ctxt sys_block;

static washdc_atomic_int is_running;
static washdc_atomic_int signal_exit_threads;

static bool frame_stop;
static bool init_complete;
static bool end_of_frame;

static bool using_debugger;

static washdc_real_time last_frame_realtime;
static dc_cycle_stamp_t last_frame_virttime;

static struct memory_interface sh4_unmapped_mem;
static struct memory_interface arm7_unmapped_mem;

#ifdef ENABLE_JIT_X86_64
static struct native_dispatch_meta sh4_native_dispatch_meta;
#endif

enum TermReason {
    TERM_REASON_NORM,   // normal program exit
    TERM_REASON_SIGINT, // received SIGINT
    TERM_REASON_ERROR   // usually this means somebody threw a c++ exception
};

// this stores the reason the dreamcast suspended execution
enum TermReason term_reason = TERM_REASON_NORM;

static enum dc_state dc_state = DC_STATE_NOT_RUNNING;

struct dc_clock arm7_clock;
struct dc_clock sh4_clock;

static unsigned frame_count;

static void dc_sigint_handler(int param);

static void *load_file(char const *path, long *len);

static void construct_sh4_mem_map(struct Sh4 *sh4, struct memory_map *map);
static void construct_arm7_mem_map(struct memory_map *map);

// Run until the next scheduled event (in dc_sched) should occur
static bool run_to_next_sh4_event(void *ctxt);
static bool run_to_next_sh4_event_jit(void *ctxt);

static bool run_to_next_arm7_event(void *ctxt);

static int lmmode0, lmmode1;

static double dc_framerate, dc_virt_framerate;

#ifdef ENABLE_JIT_X86_64
static bool run_to_next_sh4_event_jit_native(void *ctxt);
#endif

#ifdef ENABLE_DEBUGGER
// this must be called before run or not at all
static void dreamcast_enable_debugger(void);

// returns true if it's time to exit
static bool dreamcast_check_debugger(void);

static bool run_to_next_sh4_event_debugger(void *ctxt);

static bool run_to_next_arm7_event_debugger(void *ctxt);

#endif

// this must be called before run or not at all
static void dreamcast_enable_serial_server(void);

static void suspend_loop(void);

static void dc_inject_irq(char const *id);

void washdc_dump_main_memory(char const *path) {
    FILE *outfile = fopen(path, "wb");
    if (outfile) {
        fwrite(dc_mem.mem, sizeof(dc_mem.mem), 1, outfile);
        fclose(outfile);
    }
}

static uint32_t on_pdtra_read(struct Sh4*);
static void on_pdtra_write(struct Sh4*, uint32_t);

/*
 * XXX this used to be (SCHED_FREQUENCY / 10).  Now it's (SCHED_FREQUENCY / 100)
 * because programs that use the serial port (like KallistiOS) can timeout if
 * the serial port takes too long to reply.
 *
 * If the serial port is ever removed from the periodic event handler, this
 * should be increased back to (SCHED_FREQUENCY / 10) to save on host CPU
 * cycles.
 */
#define DC_PERIODIC_EVENT_PERIOD (SCHED_FREQUENCY / 100)

static void periodic_event_handler(struct SchedEvent *event);
static struct SchedEvent periodic_event;

static struct debug_frontend const *dbg_intf;
static struct serial_server_intf const *sersrv;

static void dc_get_sndchan_stat(struct washdc_snddev const *dev,
                                unsigned ch_no,
                                struct washdc_sndchan_stat *stat) {
    aica_get_sndchan_stat(&aica, ch_no, stat);
}

static void dc_get_sndchan_var(struct washdc_snddev const *dev,
                               struct washdc_sndchan_stat const *chan,
                               unsigned var_no, struct washdc_var *var) {
    aica_get_sndchan_var(&aica, chan, var_no, var);
}

static void dc_mute_sndchan(struct washdc_snddev const *dev,
                            unsigned chan_no, bool is_muted) {
    aica_mute_chan(&aica, chan_no, is_muted);
}

static inline void washdc_sleep_ms(unsigned n_ms) {
#ifdef _MSC_VER
    Sleep(n_ms);
#else
    usleep(n_ms * 1000);
#endif
}

static void dc_inject_irq(char const *id) {
    // TODO: add support for more than just Hollywood IRQs

    LOG_INFO("injecting IRQ %s\n", id);

    if (strcmp(id, "HBLANK") == 0)
        holly_raise_nrm_int(HOLLY_NRM_INT_HBLANK);
    else if (strcmp(id, "VBLANK-IN") == 0)
        holly_raise_nrm_int(HOLLY_NRM_INT_VBLANK_IN);
    else if (strcmp(id, "VBLANK-OUT") == 0)
        holly_raise_nrm_int(HOLLY_NRM_INT_VBLANK_OUT);
    else if (strcmp(id, "POLYGON EOL OPAQUE") == 0)
        holly_raise_nrm_int(HOLLY_REG_ISTNRM_PVR_OPAQUE_COMPLETE);
    else if (strcmp(id, "POLYGON EOL OPAQUE MOD") == 0)
        holly_raise_nrm_int(HOLLY_REG_ISTNRM_PVR_OPAQUE_MOD_COMPLETE);
    else if (strcmp(id, "POLYGON EOL TRANSPARENT") == 0)
        holly_raise_nrm_int(HOLLY_REG_ISTNRM_PVR_TRANS_COMPLETE);
    else if (strcmp(id, "POLYGON EOL TRANSPARENT MOD") == 0)
        holly_raise_nrm_int(HOLLY_REG_ISTNRM_PVR_TRANS_MOD_COMPLETE);
    else if (strcmp(id, "POLYGON EOL PUNCH-THROUGH") == 0)
        holly_raise_nrm_int(HOLLY_NRM_INT_ISTNRM_PVR_PUNCH_THROUGH_COMPLETE);
    else if (strcmp(id, "POWERVR2 RENDER COMPLETE") == 0)
        holly_raise_nrm_int(HOLLY_REG_ISTNRM_PVR_RENDER_COMPLETE);
    else if (strcmp(id, "POWERVR2 YUV CONVERSION COMPLETE") == 0)
        holly_raise_nrm_int(HOLLY_REG_ISTNRM_PVR_YUV_COMPLETE);
    else if (strcmp(id, "POWERVR2 DMA") == 0)
        holly_raise_nrm_int(HOLLY_REG_ISTNRM_CHANNEL2_DMA_COMPLETE);
    else if (strcmp(id, "MAPLE DMA") == 0)
        holly_raise_nrm_int(HOLLY_MAPLE_ISTNRM_DMA_COMPLETE);
    else if (strcmp(id, "AICA DMA") == 0)
        holly_raise_nrm_int(HOLLY_REG_ISTNRM_AICA_DMA_COMPLETE);
    else if (strcmp(id, "AICA (ARM7 TO SH4)") == 0)
        holly_raise_ext_int(HOLLY_EXT_INT_AICA);
    else if (strcmp(id, "GD-ROM") == 0)
        holly_raise_ext_int(HOLLY_EXT_INT_GDROM);
    else if (strcmp(id, "GD-DMA") == 0)
        holly_raise_nrm_int(HOLLY_REG_ISTNRM_GDROM_DMA_COMPLETE);
    else if (strcmp(id, "SORT DMA") == 0)
        holly_raise_nrm_int(HOLLY_REG_ISTNRM_SORT_DMA_COMPLETE);
    else
        LOG_ERROR("FAILURE TO INJECT IRQ \"%s\"\n", id);
}

static void dc_get_texinfo(struct washdc_texcache const *cache,
                           unsigned tex_no, struct washdc_texinfo *texinfo) {
    struct pvr2_tex_meta meta;
    if (tex_no < PVR2_TEX_CACHE_SIZE &&
        pvr2_tex_get_meta(&dc_pvr2, &meta, tex_no) == 0) {
        texinfo->idx = tex_no;
        texinfo->valid = true;
        texinfo->n_vars = 12;

        texinfo->width = meta.linestride;
        texinfo->height = 1 << meta.h_shift;

        switch (meta.pix_fmt) {
        case GFX_TEX_FMT_ARGB_1555:
            texinfo->fmt = WASHDC_TEX_FMT_ARGB_1555;
            break;
        case GFX_TEX_FMT_RGB_565:
            texinfo->fmt = WASHDC_TEX_FMT_RGB_565;
            break;
        case GFX_TEX_FMT_ARGB_4444:
            texinfo->fmt = WASHDC_TEX_FMT_ARGB_4444;
            break;
        case GFX_TEX_FMT_ARGB_8888:
            texinfo->fmt = WASHDC_TEX_FMT_ARGB_8888;
            break;
        case GFX_TEX_FMT_YUV_422:
            texinfo->fmt = WASHDC_TEX_FMT_YUV_422;
            break;
        default:
            // should never happen
            texinfo->valid = false;
            LOG_ERROR("%s - UNKNOWN TEXTURE FORMAT 0x%02x\n",
                      __func__, (int)meta.pix_fmt);
            return;
        }

        dc_tex_cache_read(&texinfo->tex_dat, &texinfo->n_tex_bytes, &meta);
    } else {
        texinfo->valid = false;
    }
}

static void dc_get_texinfo_var(struct washdc_texcache const *cache,
                               struct washdc_texinfo const *texinfo,
                               unsigned var_no,
                               struct washdc_var *var) {
    struct pvr2_tex_meta meta;
    if (!texinfo->valid ||
        pvr2_tex_get_meta(&dc_pvr2, &meta, texinfo->idx) != 0)
        goto inval;

    switch (var_no) {
    case 0:
        // addr_first
        strncpy(var->name, "addr_first", WASHDC_VAR_NAME_LEN);
        var->name[WASHDC_VAR_NAME_LEN - 1] = 0;
        var->tp = WASHDC_VAR_HEX;
        var->val.as_int = meta.addr_first;
        return;
    case 1:
        //addr_last
        strncpy(var->name, "addr_last", WASHDC_VAR_NAME_LEN);
        var->name[WASHDC_VAR_NAME_LEN - 1] = 0;
        var->tp = WASHDC_VAR_HEX;
        var->val.as_int = meta.addr_last;
        return;
    case 2:
        // x-res
        strncpy(var->name, "width", WASHDC_VAR_NAME_LEN);
        var->name[WASHDC_VAR_NAME_LEN - 1] = 0;
        var->tp = WASHDC_VAR_INT;
        var->val.as_int = 1 << meta.w_shift;
        return;
    case 3:
        // y-res
        strncpy(var->name, "height", WASHDC_VAR_NAME_LEN);
        var->name[WASHDC_VAR_NAME_LEN - 1] = 0;
        var->tp = WASHDC_VAR_INT;
        var->val.as_int = 1 << meta.h_shift;
        return;
    case 4:
        // pixel format
        strncpy(var->name, "pix_fmt", WASHDC_VAR_NAME_LEN);
        var->name[WASHDC_VAR_NAME_LEN - 1] = 0;
        var->tp = WASHDC_VAR_STR;
        switch (meta.pix_fmt) {
        case GFX_TEX_FMT_ARGB_1555:
            strncpy(var->val.as_str, "ARGB_1555", WASHDC_VAR_STR_LEN);
            break;
        case GFX_TEX_FMT_RGB_565:
            strncpy(var->val.as_str, "RGB_565", WASHDC_VAR_STR_LEN);
            break;
        case GFX_TEX_FMT_ARGB_4444:
            strncpy(var->val.as_str, "ARGB_4444", WASHDC_VAR_STR_LEN);
            break;
        case GFX_TEX_FMT_ARGB_8888:
            strncpy(var->val.as_str, "ARGB_8888", WASHDC_VAR_STR_LEN);
            break;
        case GFX_TEX_FMT_YUV_422:
            strncpy(var->val.as_str, "YUV_422", WASHDC_VAR_STR_LEN);
            break;
        default:
            strncpy(var->val.as_str, "UNKNOWN (error?)", WASHDC_VAR_STR_LEN);
        }
        var->val.as_str[WASHDC_VAR_STR_LEN - 1] = '\0';
        return;
    case 5:
        // tex format
        strncpy(var->name, "tex_fmt", WASHDC_VAR_NAME_LEN);
        var->name[WASHDC_VAR_NAME_LEN - 1] = 0;
        var->tp = WASHDC_VAR_STR;
        switch (meta.tex_fmt) {
        case TEX_CTRL_PIX_FMT_ARGB_1555:
            strncpy(var->val.as_str, "ARGB_1555", WASHDC_VAR_STR_LEN);
            break;
        case TEX_CTRL_PIX_FMT_RGB_565:
            strncpy(var->val.as_str, "RGB_565", WASHDC_VAR_STR_LEN);
            break;
        case TEX_CTRL_PIX_FMT_ARGB_4444:
            strncpy(var->val.as_str, "ARGB_4444", WASHDC_VAR_STR_LEN);
            break;
        case TEX_CTRL_PIX_FMT_YUV_422:
            strncpy(var->val.as_str, "YUV_422", WASHDC_VAR_STR_LEN);
            break;
        case TEX_CTRL_PIX_FMT_BUMP_MAP:
            strncpy(var->val.as_str, "BUMP_MAP", WASHDC_VAR_STR_LEN);
            break;
        case TEX_CTRL_PIX_FMT_4_BPP_PAL:
            strncpy(var->val.as_str, "4_BPP_PALETTE", WASHDC_VAR_STR_LEN);
            break;
        case TEX_CTRL_PIX_FMT_8_BPP_PAL:
            strncpy(var->val.as_str, "8_BPP_PALETTE", WASHDC_VAR_STR_LEN);
            break;
        default:
            strncpy(var->val.as_str, "UNKNOWN (error?)", WASHDC_VAR_STR_LEN);
        }
        var->val.as_str[WASHDC_VAR_STR_LEN - 1] = '\0';
        return;
    case 6:
        // tex_palette_start
        strncpy(var->name, "tex_palette_start", WASHDC_VAR_NAME_LEN);
        var->name[WASHDC_VAR_NAME_LEN - 1] = 0;
        var->tp = WASHDC_VAR_HEX;
        var->val.as_int = meta.tex_palette_start;
        return;
    case 8:
        // twiddled
        strncpy(var->name, "twiddled", WASHDC_VAR_NAME_LEN);
        var->name[WASHDC_VAR_NAME_LEN - 1] = '\0';
        var->tp = WASHDC_VAR_BOOL;
        var->val.as_bool = meta.twiddled;
        return;
    case 9:
        // stride_sel
        strncpy(var->name, "stride_sel", WASHDC_VAR_NAME_LEN);
        var->name[WASHDC_VAR_NAME_LEN - 1] = '\0';
        var->tp = WASHDC_VAR_BOOL;
        var->val.as_bool = meta.stride_sel;
        return;
    case 10:
        // vq_compression
        strncpy(var->name, "vq_compression", WASHDC_VAR_NAME_LEN);
        var->name[WASHDC_VAR_NAME_LEN - 1] = '\0';
        var->tp = WASHDC_VAR_BOOL;
        var->val.as_bool = meta.vq_compression;
        return;
    case 11:
        // mipmap
        strncpy(var->name, "mipmap", WASHDC_VAR_NAME_LEN);
        var->name[WASHDC_VAR_NAME_LEN - 1] = '\0';
        var->tp = WASHDC_VAR_BOOL;
        var->val.as_bool = meta.mipmap;
        return;
    default:
        goto inval;
    }
 inval:
    memset(var, 0, sizeof(*var));
    var->tp = WASHDC_VAR_INVALID;
}

static struct washdc_gameconsole dccons = {
    .name = "SEGA Dreamcast",
    .snddev = {
        .name = "AICA",
        .n_channels = AICA_CHAN_COUNT,
        .get_chan = dc_get_sndchan_stat,
        .get_var = dc_get_sndchan_var,
        .mute_chan = dc_mute_sndchan
    },
    .texcache = {
        .sz = PVR2_TEX_CACHE_SIZE,
        .get_texinfo = dc_get_texinfo,
        .get_var = dc_get_texinfo_var
    },
    .do_inject_irq = dc_inject_irq
};

static bool streq_case_insensitive(char const* str1, char const* str2) {
    if (strlen(str1) != strlen(str2))
        return false;
    while (*str1 && *str2)
        if (toupper(*str1++) != toupper(*str2++))
            return false;
    return true;
}

struct washdc_gameconsole const*
dreamcast_init(char const *gdi_path,
               struct gfx_rend_if const *gfx_if,
               struct debug_frontend const *dbg_frontend,
               struct serial_server_intf const *ser_intf,
               struct washdc_sound_intf const *snd_intf,
               bool flash_mem_writeable,
               struct washdc_controller_dev const controllers[4][3]) {

    frame_count = 0;

    dbg_intf = dbg_frontend;
    sersrv = ser_intf;

    log_init(config_get_log_stdout(), config_get_log_verbose());

    char const *title_content = NULL;
    struct mount_meta content_meta; // only valid if gdi_path is non-null

    if (gdi_path) {
        char const *ext = strrchr(gdi_path, '.');
        if (ext && streq_case_insensitive(ext, ".cdi"))
            mount_cdi(gdi_path);
        else if (ext && streq_case_insensitive(ext, ".gdi"))
            mount_gdi(gdi_path);
        else {
            LOG_ERROR("Unknown file type (need either GDI or CDI)!\n");
            exit(1);
        }

        if (mount_get_meta(&content_meta) == 0) {
            // dump meta to stdout and set the window title to the game title
            title_content = content_meta.title;

            LOG_INFO("GDI image %s mounted:\n", gdi_path);
            LOG_INFO("\thardware: %s\n", content_meta.hardware);
            LOG_INFO("\tmaker: %s\n", content_meta.maker);
            LOG_INFO("\tdevice info: %s\n", content_meta.dev_info);
            LOG_INFO("\tregion: %s\n", content_meta.region);
            LOG_INFO("\tperipheral support: %s\n", content_meta.periph_support);
            LOG_INFO("\tproduct id: %s\n", content_meta.product_id);
            LOG_INFO("\tproduct version: %s\n", content_meta.product_version);
            LOG_INFO("\trelease date: %s\n", content_meta.rel_date);
            LOG_INFO("\tboot file: %s\n", content_meta.boot_file);
            LOG_INFO("\tcompany: %s\n", content_meta.company);
            LOG_INFO("\ttitle: %s\n", content_meta.title);
        }
    }

    if (!(config_get_boot_mode() == DC_BOOT_DIRECT || gdi_path))
        title_content = "firmware";

    title_set_content(title_content);

    washdc_atomic_int_init(&signal_exit_threads, 0);
    washdc_atomic_int_init(&is_running, 1);

    /* if (cfg_get_int("win.external-res.x", &win_width) != 0 || win_width <= 0) */
    /*     win_width = 640; */
    /* if (cfg_get_int("win.external-res.y", &win_height) != 0 || win_height <= 0) */
    /*     win_height = 480; */

    // initialize host windowing, graphics and sound systems
    /* win_init(win_width, win_height); */
    gfx_init(gfx_if);
    dc_sound_init(snd_intf);

    memory_init(&dc_mem);
    flash_mem_init(&flash_mem, config_get_dc_flash_path(), flash_mem_writeable);
    boot_rom_init(&firmware, config_get_dc_bios_path());

    int boot_mode = config_get_boot_mode();
    if (boot_mode == (int)DC_BOOT_IP_BIN || boot_mode == (int)DC_BOOT_DIRECT) {
        long len_ip_bin;
        char const *ip_bin_path = config_get_ip_bin_path();
        if (ip_bin_path && strlen(ip_bin_path)) {
            void *dat_ip_bin = load_file(ip_bin_path, &len_ip_bin);
            if (!dat_ip_bin) {
                error_set_file_path(ip_bin_path);
                error_set_errno_val(errno);
                RAISE_ERROR(ERROR_FILE_IO);
            }

            memory_write(&dc_mem, dat_ip_bin, ADDR_IP_BIN & ADDR_AREA3_MASK,
                         len_ip_bin);
            free(dat_ip_bin);
        }

        long len_1st_read_bin;
        char const *exec_bin_path = config_get_exec_bin_path();
        if (exec_bin_path && strlen(exec_bin_path)) {
            void *dat_1st_read_bin = load_file(exec_bin_path, &len_1st_read_bin);
            if (!dat_1st_read_bin) {
                error_set_file_path(exec_bin_path);
                error_set_errno_val(errno);
                RAISE_ERROR(ERROR_FILE_IO);
            }
            memory_write(&dc_mem, dat_1st_read_bin,
                         ADDR_1ST_READ_BIN & ADDR_AREA3_MASK, len_1st_read_bin);
            free(dat_1st_read_bin);
        }

        char const *syscall_path = config_get_syscall_path();
        long syscall_len;
        void *dat_syscall = load_file(syscall_path, &syscall_len);

        if (!dat_syscall) {
            error_set_file_path(syscall_path);
            error_set_errno_val(errno);
            RAISE_ERROR(ERROR_FILE_IO);
        }

        if (syscall_len != LEN_SYSCALLS) {
            error_set_length(syscall_len);
            error_set_expected_length(LEN_SYSCALLS);
            RAISE_ERROR(ERROR_INVALID_FILE_LEN);
        }

        memory_write(&dc_mem, dat_syscall,
                     ADDR_SYSCALLS & ADDR_AREA3_MASK, syscall_len);
        free(dat_syscall);
    }

    dc_clock_init(&sh4_clock);
    dc_clock_init(&arm7_clock);
    sh4_init(&cpu, &sh4_clock);
    arm7_init(&arm7, &arm7_clock, &aica.mem);

#ifdef ENABLE_JIT_X86_64
    if (config_get_native_jit()) {
        jit_x86_64_backend_init();
        exec_mem_init();
        sh4_jit_set_native_dispatch_meta(&sh4_native_dispatch_meta);
        sh4_native_dispatch_meta.clk = &sh4_clock;
        native_dispatch_init(&sh4_native_dispatch_meta, &cpu);
        native_mem_init();
    }
#endif
    jit_init(&sh4_clock);

    g1_init();
    g2_init();
    aica_init(&aica, &arm7, &arm7_clock, &sh4_clock);
    pvr2_init(&dc_pvr2, &sh4_clock, &maple);
    sys_block_init(&sys_block, &sh4_clock, &cpu, &dc_mem, &dc_pvr2);
    gdrom_init(&gdrom, &sh4_clock);
    maple_init(&maple, &sh4_clock);

    unsigned port;
    for (port = 0; port < 4; port++) {
        struct washdc_controller_dev unit0 = controllers[port][0];
        if (unit0.tp == WASHDC_CONTROLLER_TP_CONTROLLER) {
            maple_device_init_controller(&maple, maple_addr_pack(port, 0));
        } else if (unit0.tp == WASHDC_CONTROLLER_TP_KEYBOARD_US) {
            maple_device_init_keyboard_us(&maple, maple_addr_pack(port, 0));
        }

        /*
         * it is theoretically possible to have a VMU, PuruPuru, etc plugged
         * into unit 0, but there is no way to do that without hacking up your
         * own custom hardware so it's not allowed in this emulator.
         */
        unsigned unit;
        for (unit = 1; unit <= 2; unit++) {
            if (controllers[port][unit].tp == WASHDC_CONTROLLER_TP_PURUPURU) {
                maple_device_init_purupuru(&maple, maple_addr_pack(port, unit));
            } else if (controllers[port][unit].tp == WASHDC_CONTROLLER_TP_VMU) {
                maple_device_init_vmu(&maple, maple_addr_pack(port, unit),
                                      controllers[port][unit].image_path);
            }
        }
    }

    // hook up the irl line
    sh4_register_irl_line(&cpu, holly_intc_irl_line_fn, NULL);

    // hook up pdtra read/write handlers
    sh4_register_pdtra_read_handler(&cpu, on_pdtra_read);
    sh4_register_pdtra_write_handler(&cpu, on_pdtra_write);

    memory_map_init(&mem_map);
    construct_sh4_mem_map(&cpu, &mem_map);
    sh4_set_mem_map(&cpu, &mem_map);

    memory_map_init(&arm7_mem_map);
    construct_arm7_mem_map(&arm7_mem_map);
    arm7_set_mem_map(&arm7, &arm7_mem_map);

#ifdef ENABLE_JIT_X86_64
    if (config_get_native_jit())
        native_mem_register(cpu.mem.map);
#endif

    /* set the PC to the booststrap code within IP.BIN */
    if (boot_mode == (int)DC_BOOT_DIRECT)
        cpu.reg[SH4_REG_PC] = ADDR_1ST_READ_BIN;
    else if (boot_mode == (int)DC_BOOT_IP_BIN)
        cpu.reg[SH4_REG_PC] = ADDR_BOOTSTRAP;

    if (boot_mode == (int)DC_BOOT_IP_BIN || boot_mode == (int)DC_BOOT_DIRECT) {
        /*
         * set the VBR to what it would have been after a BIOS boot.
         * This was obtained empirically on a real Dreamcast.
         *
         * XXX not sure if there should be a different value depending on
         * whether or not we skip IP.BIN.  All I do know is that this value is
         * correct when we do skip IP.BIN because I obtained it by running a
         * homebrew that prints the VBR value when it starts, which would be
         * immediately after IP.BIN is run.  It is possible that there's a
         * different value immediately before IP.BIN runs, and that the value
         * seen by 1ST_READ.BIN is set by IP.BIN.
         */
        cpu.reg[SH4_REG_VBR] = 0x8c00f400;
    }

    aica_rtc_init(&rtc, &sh4_clock, config_get_dc_path_rtc());

#ifdef ENABLE_DEBUGGER
    if (config_get_dbg_enable()) {
        dc_state_transition(DC_STATE_RUNNING, DC_STATE_NOT_RUNNING);
        goto init_complete;
    }
#endif

    // TODO: hold here when pausing on start gets implemented
    /* if (!cmd_session) */ {
        /*
         * if there's no debugging support and we have a remote cmd session
         * attached, then leave the system in DC_STATE_NOT_RUNNING until the
         * user executes the begin-execution command.
         */
        dc_state_transition(DC_STATE_RUNNING, DC_STATE_NOT_RUNNING);
    }

#ifdef ENABLE_DEBUGGER
init_complete:
#endif

    lmmode0 = 0;
    lmmode1 = 0;

    init_complete = true;

    return &dccons;
}

void dreamcast_cleanup() {
    init_complete = false;

#ifdef ENABLE_DEBUGGER
    LOG_INFO("Cleanup up debugger\n");
    debug_cleanup();
    LOG_INFO("debugger cleaned up\n");
#endif

    aica_rtc_cleanup(&rtc);

    memory_map_cleanup(&arm7_mem_map);
    memory_map_cleanup(&mem_map);

    // disconnect PDTRA read/write handlers
    sh4_register_pdtra_read_handler(&cpu, NULL);
    sh4_register_pdtra_write_handler(&cpu, NULL);


    // disconnect the irl line
    sh4_register_irl_line(&cpu, NULL, NULL);

    maple_cleanup(&maple);
    gdrom_cleanup(&gdrom);
    sys_block_cleanup(&sys_block);
    pvr2_cleanup(&dc_pvr2);
    aica_cleanup(&aica);
    g2_cleanup();
    g1_cleanup();

    jit_cleanup();
#ifdef ENABLE_JIT_X86_64
    if (config_get_native_jit()) {
        native_mem_cleanup();
        native_dispatch_cleanup(&sh4_native_dispatch_meta);
        exec_mem_cleanup();
        jit_x86_64_backend_cleanup();
    }
#endif

    arm7_cleanup(&arm7);
    sh4_cleanup(&cpu);
    dc_clock_cleanup(&arm7_clock);
    dc_clock_cleanup(&sh4_clock);
    boot_rom_cleanup(&firmware);
    flash_mem_cleanup(&flash_mem);
    memory_cleanup(&dc_mem);

    dc_sound_cleanup();
    gfx_cleanup();
    /* win_cleanup(); */

    if (mount_check())
        mount_eject();

    log_cleanup();
}

/*
 * this is used to store the irl timestamp right before execution begins.
 * This exists for performance profiling purposes only.
 */
static washdc_real_time start_time;

static void run_one_frame(void) {
    while (!end_of_frame) {
        if (dc_clock_run_timeslice(&sh4_clock))
            return;
        if (dc_clock_run_timeslice(&arm7_clock))
            return;
        if (config_get_jit())
            code_cache_gc();
    }
    end_of_frame = false;
}

unsigned dc_get_frame_count(void) {
    return frame_count;
}

static void main_loop_sched(void) {
    while (washdc_atomic_int_load(&is_running)) {
        run_one_frame();
        frame_count++;
        if (frame_stop) {
            frame_stop = false;
            if (dc_state == DC_STATE_RUNNING) {
                dc_state_transition(DC_STATE_SUSPEND, DC_STATE_RUNNING);
                suspend_loop();
            } else {
                LOG_WARN("Unable to suspend execution at frame stop: "
                         "system is not running\n");
            }
        }
    }
}

typedef bool(*cpu_backend_func)(void*);

static cpu_backend_func select_sh4_backend(void) {
#ifdef ENABLE_DEBUGGER
    bool use_debugger = config_get_dbg_enable();
    if (use_debugger)
        return run_to_next_sh4_event_debugger;
#endif

#ifdef ENABLE_JIT_X86_64
    bool const native_mode = config_get_native_jit();
    bool const jit = config_get_jit();

    if (jit) {
        if (native_mode)
            return run_to_next_sh4_event_jit_native;
        else
            return run_to_next_sh4_event_jit;
    } else {
        return run_to_next_sh4_event;
    }
#else
    bool const jit = config_get_jit();
    if (jit)
        return run_to_next_sh4_event_jit;
    else
        return run_to_next_sh4_event;
#endif
}

static cpu_backend_func select_arm7_backend(void) {
#ifdef ENABLE_DEBUGGER
    bool use_debugger = config_get_dbg_enable();
    if (use_debugger)
        return run_to_next_arm7_event_debugger;
#endif
    return run_to_next_arm7_event;
}

void dreamcast_run() {
    signal(SIGINT, dc_sigint_handler);

    if (config_get_ser_srv_enable())
        dreamcast_enable_serial_server();

#ifdef ENABLE_DEBUGGER
    debug_init();
    debug_init_context(DEBUG_CONTEXT_SH4, &cpu, &mem_map);
    debug_init_context(DEBUG_CONTEXT_ARM7, &arm7, &arm7_mem_map);
    if (config_get_dbg_enable())
        dreamcast_enable_debugger();
#endif

    periodic_event.when = clock_cycle_stamp(&sh4_clock) + DC_PERIODIC_EVENT_PERIOD;
    periodic_event.handler = periodic_event_handler;
    sched_event(&sh4_clock, &periodic_event);

    // back when cmd existed, this was where we'd wait for the user to begin-execution
    if (dc_get_state() == DC_STATE_NOT_RUNNING)
        RAISE_ERROR(ERROR_UNIMPLEMENTED);

    washdc_get_real_time(&start_time);
    washdc_get_real_time(&last_frame_realtime);

    sh4_clock.dispatch = select_sh4_backend();
    sh4_clock.dispatch_ctxt = &cpu;

    arm7_clock.dispatch = select_arm7_backend();
    arm7_clock.dispatch_ctxt = &arm7;

    main_loop_sched();

    dc_print_perf_stats();

    // tell the other threads it's time to clean up and exit
    int oldval = 0;
    washdc_atomic_int_compare_exchange(&signal_exit_threads, &oldval, 1);

    switch (term_reason) {
    case TERM_REASON_NORM:
        LOG_INFO("program execution ended normally\n");
        break;
    case TERM_REASON_ERROR:
        LOG_INFO("program execution ended due to an unrecoverable error\n");
        break;
    case TERM_REASON_SIGINT:
        LOG_INFO("program execution ended due to user-initiated interruption\n");
        break;
    default:
        LOG_INFO("program execution ended for unknown reasons\n");
        break;
    }
}

static bool run_to_next_arm7_event(void *ctxt) {
    dc_cycle_stamp_t tgt_stamp = clock_target_stamp(&arm7_clock);

    if (arm7.enabled) {
        dc_cycle_stamp_t cycles_after;
        for (;;) {
            int extra_cycles;
            arm7_inst inst = arm7_fetch_inst(&arm7, &extra_cycles);
            arm7_op_fn handler = arm7_decode(&arm7, inst);
            unsigned inst_cycles = handler(&arm7, inst);
            dc_cycle_stamp_t cycles_adv =
                (inst_cycles + extra_cycles) * ARM7_CLOCK_SCALE;

            if (cycles_adv >= clock_countdown(&arm7_clock)) {
                cycles_after = clock_target_stamp(&arm7_clock);
                break;
            }

            clock_countdown_sub(&arm7_clock, cycles_adv);
        }
        clock_set_cycle_stamp(&arm7_clock, cycles_after);
    } else {
        /*
         * XXX When the ARM7 is disabled, the PC is supposed to continue
         * incrementing until it's enabled just as if it was executing
         * instructions.  When the ARM7 is re-enabled, the PC is saved into
         * R14_svc, the CPSR is saved into SPSR_svc, and the PC is cleared to 0.
         *
         * This means it's possible for the SH4 to place arbitrary values into
         * R14_svc by timing its writes to the ARM7's nReset register.
         * I'm hoping that nothing ever uses this to set a specific value into
         * R14_svc.  TBH I think it would be hard to get the timing right even
         * on real hardware.
         */
        tgt_stamp = clock_target_stamp(&arm7_clock);
        clock_set_cycle_stamp(&arm7_clock, tgt_stamp);
    }

    return false;
}

#ifdef ENABLE_DEBUGGER
static bool run_to_next_arm7_event_debugger(void *ctxt) {
    dc_cycle_stamp_t tgt_stamp = clock_target_stamp(&arm7_clock);

    if (arm7.enabled) {
        debug_set_context(DEBUG_CONTEXT_ARM7);
        dc_cycle_stamp_t cycles_after;
        bool exit_now;

        if ((exit_now = dreamcast_check_debugger()))
            return exit_now;

        while (!(exit_now = dreamcast_check_debugger())) {
            int extra_cycles;
            arm7_inst inst = arm7_fetch_inst(&arm7, &extra_cycles);
            arm7_op_fn handler = arm7_decode(&arm7, inst);
            unsigned inst_cycles = handler(&arm7, inst);
            dc_cycle_stamp_t cycles_adv =
                (inst_cycles + extra_cycles) * ARM7_CLOCK_SCALE;

            if (cycles_adv >= clock_countdown(&arm7_clock)) {
                cycles_after = clock_target_stamp(&arm7_clock);
                break;
            }

            clock_countdown_sub(&arm7_clock, cycles_adv);
#ifdef ENABLE_DBG_COND
            debug_check_conditions(DEBUG_CONTEXT_ARM7);
#endif
        }
        if (!exit_now)
            clock_set_cycle_stamp(&arm7_clock, cycles_after);
    } else {
        /*
         * XXX When the ARM7 is disabled, the PC is supposed to continue
         * incrementing until it's enabled just as if it was executing
         * instructions.  When the ARM7 is re-enabled, the PC is saved into
         * R14_svc, the CPSR is saved into SPSR_svc, and the PC is cleared to 0.
         *
         * This means it's possible for the SH4 to place arbitrary values into
         * R14_svc by timing its writes to the ARM7's nReset register.
         * I'm hoping that nothing ever uses this to set a specific value into
         * R14_svc.  TBH I think it would be hard to get the timing right even
         * on real hardware.
         */
        tgt_stamp = clock_target_stamp(&arm7_clock);
        clock_set_cycle_stamp(&arm7_clock, tgt_stamp);
    }

    return false;
}
#endif

#ifdef ENABLE_DEBUGGER
static bool dreamcast_check_debugger(void) {
    /*
     * If the debugger is enabled, make sure we have its permission to
     * single-step; if we don't then  block until something interresting
     * happens, and then skip the rest of the loop.
     */
    debug_notify_inst();
    bool is_running;

    enum dc_state cur_state = dc_get_state();
    if ((is_running = dc_emu_thread_is_running()) &&
        cur_state == DC_STATE_DEBUG) {
        printf("cur_state is DC_STATE_DEBUG\n");
        do {
            // call debug_run_once 100 times per second
            win_check_events();
            debug_run_once();
            washdc_sleep_ms(1000 / 100);
        } while ((cur_state = dc_get_state()) == DC_STATE_DEBUG &&
                 (is_running = dc_emu_thread_is_running()));
    }
    return !is_running;
}

static bool run_to_next_sh4_event_debugger(void *ctxt) {
    Sh4 *sh4 = (void*)ctxt;
    bool exit_now;

    debug_set_context(DEBUG_CONTEXT_SH4);

    if ((exit_now = dreamcast_check_debugger()))
        return exit_now;

    dc_cycle_stamp_t cycles_after = clock_target_stamp(&sh4_clock);
    while (!(exit_now = dreamcast_check_debugger())) {
        dc_cycle_stamp_t cycles_adv = 0;

        cycles_adv +=
            (dc_cycle_stamp_t)sh4_do_exec_inst(sh4) * SH4_CLOCK_SCALE;
        if (cycles_adv >= clock_countdown(&sh4_clock)) {
            cycles_after = clock_target_stamp(&sh4_clock);
            break;
        }

        clock_countdown_sub(&sh4_clock, cycles_adv);

#ifdef ENABLE_DBG_COND
        debug_check_conditions(DEBUG_CONTEXT_SH4);
#endif
    }

    clock_set_cycle_stamp(&sh4_clock, cycles_after);

    return exit_now;
}

#endif

static bool run_to_next_sh4_event(void *ctxt) {
    dc_cycle_stamp_t cycles_after;

    Sh4 *sh4 = (void*)ctxt;

    for (;;) {
        dc_cycle_stamp_t cycles_adv = 0;

        cycles_adv +=
            (dc_cycle_stamp_t)sh4_do_exec_inst(sh4) * SH4_CLOCK_SCALE;
        if (cycles_adv >= clock_countdown(&sh4_clock)) {
            cycles_after = clock_target_stamp(&sh4_clock);
            break;
        }

        clock_countdown_sub(&sh4_clock, cycles_adv);
    }

    clock_set_cycle_stamp(&sh4_clock, cycles_after);

    return false;
}

#ifdef ENABLE_JIT_X86_64
static bool run_to_next_sh4_event_jit_native(void *ctxt) {
    Sh4 *sh4 = (Sh4*)ctxt;

    reg32_t newpc = sh4->reg[SH4_REG_PC];

    jit_hash hash =
        sh4_jit_hash(ctxt, newpc, sh4_fpscr_pr(sh4), sh4_fpscr_sz(sh4));
    newpc = sh4_native_dispatch_meta.entry(newpc, hash);

    sh4->reg[SH4_REG_PC] = newpc;

    return false;
}
#endif

static bool run_to_next_sh4_event_jit(void *ctxt) {
    Sh4 *sh4 = (Sh4*)ctxt;

    reg32_t newpc = sh4->reg[SH4_REG_PC];
    dc_cycle_stamp_t tgt_stamp = clock_target_stamp(&sh4_clock);

    do {
        addr32_t blk_addr = newpc;
        jit_hash code_hash =
            sh4_jit_hash(sh4, blk_addr, sh4_fpscr_pr(sh4), sh4_fpscr_sz(sh4));
        struct cache_entry *ent = code_cache_find(code_hash);

        struct jit_code_block *blk = &ent->blk;
        struct code_block_intp *intp_blk = &blk->intp;
        if (!ent->valid) {
            sh4_jit_compile_intp(sh4, blk, blk_addr);
            ent->valid = true;
        }

#ifdef JIT_PROFILE
        jit_profile_notify(&sh4->jit_profile, blk->profile);
#endif

        newpc = code_block_intp_exec(sh4, intp_blk);

        dc_cycle_stamp_t cycles_after = clock_cycle_stamp(&sh4_clock) +
            intp_blk->cycle_count;
        clock_set_cycle_stamp(&sh4_clock, cycles_after);
        tgt_stamp = clock_target_stamp(&sh4_clock);
    } while (tgt_stamp > clock_cycle_stamp(&sh4_clock));
    if (clock_cycle_stamp(&sh4_clock) > tgt_stamp)
        clock_set_cycle_stamp(&sh4_clock, tgt_stamp);

    sh4->reg[SH4_REG_PC] = newpc;

    return false;
}

void dc_print_perf_stats(void) {
    if (init_complete) {
        washdc_real_time end_time, delta_time;
        washdc_get_real_time(&end_time);

        washdc_real_time_diff(&delta_time, &end_time, &start_time);

        double seconds = washdc_real_time_to_seconds(&delta_time);
        LOG_INFO("Total elapsed time: %f seconds\n", seconds);

        LOG_INFO("%u SH4 CPU cycles executed\n",
                 (unsigned)sh4_get_cycles(&cpu));
        printf("%u SH4 CPU cycles executed\n",
               (unsigned)sh4_get_cycles(&cpu));

        double hz = (double)sh4_get_cycles(&cpu) / seconds;
        double hz_ratio = hz / (double)(200 * 1000 * 1000);

        LOG_INFO("Average Performance is %f MHz (%f%%)\n",
                 hz / 1000000.0, hz_ratio * 100.0);
        printf("Average Performance is %f MHz (%f%%)\n",
               hz / 1000000.0, hz_ratio * 100.0);
    } else {
        LOG_INFO("Program execution halted before WashingtonDC was completely "
                 "initialized.\n");
    }
}

void dreamcast_kill(void) {
    LOG_INFO("%s called - WashingtonDC will exit soon\n", __func__);
    int oldval = 1;
    washdc_atomic_int_compare_exchange(&is_running, &oldval, 0);
}

Sh4 *dreamcast_get_cpu() {
    return &cpu;
}

#ifdef ENABLE_DEBUGGER
static void dreamcast_enable_debugger(void) {
    if (dbg_intf) {
        using_debugger = true;
        debug_attach(dbg_intf);
    } else {
        using_debugger = false;
    }
}
#endif

static void dreamcast_enable_serial_server(void) {
#ifdef ENABLE_TCP_SERIAL
    serial_server_attach(sersrv, &cpu);
    sh4_scif_connect_server(&cpu);
#else
    LOG_ERROR("You must recompile with -DENABLE_TCP_SERIAL=On to use the tcp "
	      "serial server emulator.\n");
#endif
}

static void dc_sigint_handler(int param) {
    term_reason = TERM_REASON_SIGINT;
    dreamcast_kill();
}

static void *load_file(char const *path, long *len) {
    washdc_hostfile fp = washdc_hostfile_open(path, WASHDC_HOSTFILE_READ |
                                              WASHDC_HOSTFILE_BINARY);
    long file_sz;
    void *dat = NULL;

    if (fp == WASHDC_HOSTFILE_INVALID)
        return NULL;

    if (washdc_hostfile_seek(fp, 0, WASHDC_HOSTFILE_SEEK_END) < 0)
        goto close_fp;
    if ((file_sz = washdc_hostfile_tell(fp)) < 0)
        goto close_fp;
    if (washdc_hostfile_seek(fp, 0, WASHDC_HOSTFILE_SEEK_BEG) < 0)
        goto close_fp;

    dat = malloc(file_sz);
    if (washdc_hostfile_read(fp, dat, file_sz) != file_sz)
        goto free_dat;

    *len = file_sz;

    // success
    goto close_fp;

free_dat:
    free(dat);
    dat = NULL;
close_fp:
    washdc_hostfile_close(fp);
    return dat;
}

bool dc_is_running(void) {
    return !washdc_atomic_int_load(&signal_exit_threads);
}

bool dc_emu_thread_is_running(void) {
    return washdc_atomic_int_load(&is_running);
}

enum dc_state dc_get_state(void) {
    return dc_state;
}

void dc_state_transition(enum dc_state state_new, enum dc_state state_old) {
    if (state_old != dc_state)
        RAISE_ERROR(ERROR_INTEGRITY);
    dc_state = state_new;
}

bool dc_debugger_enabled(void) {
    return using_debugger;
}

static void suspend_loop(void) {
    enum dc_state cur_state = dc_get_state();
    if (cur_state == DC_STATE_SUSPEND) {
        do {
            win_run_once_on_suspend();
            /*
             * TODO: sleep on a pthread condition or something instead of
             * polling.
             */
            washdc_sleep_ms(1000 / 60);
        } while (dc_emu_thread_is_running() &&
                 ((cur_state = dc_get_state()) == DC_STATE_SUSPEND));
    }
}

/*
 * the purpose of this handler is to perform processing that needs to happen
 * occasionally but has no hard timing requirements.  The timing of this event
 * is *technically* deterministic, but users should not assume any determinism
 * because the frequency of this event is subject to change.
 */
static void periodic_event_handler(struct SchedEvent *event) {
    suspend_loop();

    sh4_periodic(&cpu);

    periodic_event.when = clock_cycle_stamp(&sh4_clock) + DC_PERIODIC_EVENT_PERIOD;
    sched_event(&sh4_clock, &periodic_event);
}

void dc_end_frame(void) {
    washdc_real_time timestamp, delta, virt_frametime_ns;
    dc_cycle_stamp_t virt_timestamp = clock_cycle_stamp(&sh4_clock);
    double framerate, virt_framerate, virt_frametime;

    end_of_frame = true;

    virt_frametime = (double)(virt_timestamp - last_frame_virttime);
    double virt_frametime_seconds = virt_frametime / (double)SCHED_FREQUENCY;
    washdc_real_time_from_seconds(&virt_frametime_ns, virt_frametime_seconds);

    washdc_get_real_time(&timestamp);
    washdc_real_time_diff(&delta, &timestamp, &last_frame_realtime);

    framerate = 1.0 / washdc_real_time_to_seconds(&delta);
    virt_framerate = (double)SCHED_FREQUENCY / virt_frametime;

    last_frame_realtime = timestamp;
    last_frame_virttime = virt_timestamp;
    dc_framerate = framerate;
    dc_virt_framerate = virt_framerate;

    title_set_fps_internal(virt_framerate);

    win_update_title();
    framebuffer_render(&dc_pvr2);
    win_check_events();
}

double dc_get_fps(void) {
    return dc_framerate;
}

double dc_get_virt_fps(void) {
    return dc_virt_framerate;
}

void dc_tex_cache_read(void **tex_dat_out, size_t *n_bytes_out,
                       struct pvr2_tex_meta const *meta) {
    pvr2_tex_cache_read(&dc_pvr2, tex_dat_out, n_bytes_out, meta);
}

static void construct_arm7_mem_map(struct memory_map *map) {
    /*
     * TODO: I'm not actually 100% sure that the aica wave mem should be
     * mirrored four times over here, but it is mirrored on the sh4-side of
     * things.
     */
    memory_map_add(map, 0x00000000, 0x007fffff,
                   0xffffffff, ADDR_AICA_WAVE_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &aica_wave_mem_intf, &aica.mem);
    memory_map_add(map, 0x00800000, 0x00807fff,
                   0xffffffff, 0xffffffff, MEMORY_MAP_REGION_UNKNOWN,
                   &aica_sys_intf, &aica);

    map->unmap = &arm7_unmapped_mem;
}

static void construct_sh4_mem_map(struct Sh4 *sh4, struct memory_map *map) {
    /*
     * I don't like the idea of putting SH4_AREA_P4 ahead of AREA3 (memory),
     * but this absolutely needs to be at the front of the list because the
     * only distinction between this and the other memory regions is that the
     * upper three bits of the address are all 1, and for the other regions the
     * upper three bits can be anything as long as they are not all 1.
     *
     * SH4_OC_RAM_AREA is also an SH4 on-chip component but as far as I know
     * nothing else in the dreamcast's memory map overlaps with it; this is why
     * have not also put it at the begging of the regions array.
     */
    memory_map_add(map, SH4_AREA_P4_FIRST, SH4_AREA_P4_LAST,
                   0xffffffff, 0xffffffff, MEMORY_MAP_REGION_UNKNOWN,
                   &sh4_p4_intf, sh4);

    // Main system memory.
    memory_map_add(map, 0x0c000000, 0x0cffffff,
                   0x1fffffff, ADDR_AREA3_MASK, MEMORY_MAP_REGION_RAM,
                   &ram_intf, &dc_mem);
    memory_map_add(map, 0x0d000000, 0x0dffffff,
                   0x1fffffff, ADDR_AREA3_MASK, MEMORY_MAP_REGION_RAM,
                   &ram_intf, &dc_mem);
    memory_map_add(map, 0x0e000000, 0x0effffff,
                   0x1fffffff, ADDR_AREA3_MASK, MEMORY_MAP_REGION_RAM,
                   &ram_intf, &dc_mem);
    memory_map_add(map, 0x0f000000, 0x0fffffff,
                   0x1fffffff, ADDR_AREA3_MASK, MEMORY_MAP_REGION_RAM,
                   &ram_intf, &dc_mem);


    /*
     * 64-bit and 32-bit texture memory.  I think these are actually supposed
     * to share the same backing, but with the data stored separately.  For now
     * they're implemented as two separate regions because I'm not sure how that
     * works.
     *
     * TODO: each of these has at least three additional mirrors.
     *
     * The 64-bit area has mirrors at 0x04800000-0x04ffffff,
     * 0x06000000-0x067fffff, and 0x06800000-0x06ffffff
     *
     * The 32-bit area has mirrors at 0x05800000-0x05ffffff,
     * 0x07000000-0x077fffff, and 0x07800000-0x07ffffff.
     *
     * There might be even more mirrors at 0x11000000-0x11ffffff and
     * 0x13000000-0x13ffffff, but I'm not sure.
     */
    memory_map_add(map, 0x04000000, 0x047fffff,
                   0x1fffffff, (8<<20)-1, MEMORY_MAP_REGION_UNKNOWN,
                   &pvr2_tex_mem_area64_intf, &dc_pvr2);
    memory_map_add(map, 0x05000000, 0x057fffff,
                   0x1fffffff, (8<<20)-1, MEMORY_MAP_REGION_UNKNOWN,
                   &pvr2_tex_mem_area32_intf, &dc_pvr2);
    memory_map_add(map, 0x06000000, 0x067fffff,
                   0x1fffffff, (8<<20)-1, MEMORY_MAP_REGION_UNKNOWN,
                   &pvr2_tex_mem_area64_intf, &dc_pvr2);
    memory_map_add(map, 0x07000000, 0x077fffff,
                   0x1fffffff, (8<<20)-1, MEMORY_MAP_REGION_UNKNOWN,
                   &pvr2_tex_mem_area32_intf, &dc_pvr2);


    memory_map_add(map, 0x10000000, 0x107fffff,
                   0x1fffffff, 0x1fffffff, MEMORY_MAP_REGION_UNKNOWN,
                   &pvr2_ta_fifo_intf, &dc_pvr2);
    memory_map_add(map, 0x10800000, 0x10ffffff,
                   0x1fffffff, 0x1fffffff, MEMORY_MAP_REGION_UNKNOWN,
                   &pvr2_ta_yuv_fifo_intf, &dc_pvr2);
    memory_map_add(map, 0x11000000, 0x117fffff,
                   0x1fffffff, 0x1fffffff, MEMORY_MAP_REGION_UNKNOWN,
                   &pvr2_ta_fifo_intf, &dc_pvr2);

    /*
     * TODO: YUV FIFO - apparently I made it a special case in the DMAC code
     * for some dumb reason...
     */

    memory_map_add(map, 0x7c000000, 0x7fffffff,
                   0xffffffff, 0xffffffff, MEMORY_MAP_REGION_UNKNOWN,
                   &sh4_ora_intf, sh4);


    memory_map_add(map, ADDR_BIOS_FIRST, ADDR_BIOS_LAST,
                   0x1fffffff, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &boot_rom_intf, &firmware);
    memory_map_add(map, ADDR_FLASH_FIRST, ADDR_FLASH_LAST,
                   0x1fffffff, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &flash_mem_intf, &flash_mem);
    memory_map_add(map, ADDR_G1_FIRST, ADDR_G1_LAST,
                   0x1fffffff, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &g1_intf, NULL);
    memory_map_add(map, ADDR_SYS_FIRST, ADDR_SYS_LAST,
                   0x1fffffff, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &sys_block_intf, &sys_block);
    memory_map_add(map, ADDR_MAPLE_FIRST, ADDR_MAPLE_LAST,
                   0x1fffffff, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &maple_intf, &maple);
    memory_map_add(map, ADDR_G2_FIRST, ADDR_G2_LAST,
                   0x1fffffff, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &g2_intf, NULL);
    memory_map_add(map, ADDR_PVR2_FIRST, ADDR_PVR2_LAST,
                   0x1fffffff, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &pvr2_reg_intf, &dc_pvr2);
    memory_map_add(map, ADDR_MODEM_FIRST, ADDR_MODEM_LAST,
                   0x1fffffff, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &modem_intf, NULL);
    /* memory_map_add(map, ADDR_PVR2_CORE_FIRST, ADDR_PVR2_CORE_LAST, */
    /*                0x1fffffff, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN, */
    /*                &pvr2_core_reg_intf, NULL); */
    memory_map_add(map, ADDR_AICA_WAVE_FIRST, ADDR_AICA_WAVE_LAST,
                   0x1fffffff, ADDR_AICA_WAVE_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &aica_wave_mem_intf, &aica.mem);
    memory_map_add(map, 0x00700000, 0x00707fff,
                   0x1fffffff, 0xffffffff, MEMORY_MAP_REGION_UNKNOWN,
                   &aica_sys_intf, &aica);
    memory_map_add(map, ADDR_AICA_RTC_FIRST, ADDR_AICA_RTC_LAST,
                   0x1fffffff, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &aica_rtc_intf, &rtc);
    memory_map_add(map, ADDR_GDROM_FIRST, ADDR_GDROM_LAST,
                   0x1fffffff, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &gdrom_reg_intf, &gdrom);
    memory_map_add(map, ADDR_EXT_DEV_FIRST, ADDR_EXT_DEV_LAST,
                   0x1fffffff, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &ext_dev_intf, NULL);

    memory_map_add(map, ADDR_BIOS_FIRST + 0x02000000, ADDR_BIOS_LAST + 0x02000000,
                   0x1fffffff, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &boot_rom_intf, &firmware);
    memory_map_add(map, ADDR_FLASH_FIRST + 0x02000000, ADDR_FLASH_LAST + 0x02000000,
                   0x1fffffff, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &flash_mem_intf, &flash_mem);
    memory_map_add(map, ADDR_G1_FIRST + 0x02000000, ADDR_G1_LAST + 0x02000000,
                   0x1fffffff, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &g1_intf, NULL);
    memory_map_add(map, ADDR_SYS_FIRST + 0x02000000, ADDR_SYS_LAST + 0x02000000,
                   0x1fffffff, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &sys_block_intf, NULL);
    memory_map_add(map, ADDR_MAPLE_FIRST + 0x02000000, ADDR_MAPLE_LAST + 0x02000000,
                   0x1fffffff, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &maple_intf, NULL);
    memory_map_add(map, ADDR_G2_FIRST + 0x02000000, ADDR_G2_LAST + 0x02000000,
                   0x1fffffff, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &g2_intf, NULL);
    memory_map_add(map, ADDR_PVR2_FIRST + 0x02000000, ADDR_PVR2_LAST + 0x02000000,
                   0x1fffffff, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &pvr2_reg_intf, &dc_pvr2);
    memory_map_add(map, ADDR_MODEM_FIRST + 0x02000000, ADDR_MODEM_LAST + 0x02000000,
                   0x1fffffff, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &modem_intf, NULL);
    /* memory_map_add(map, ADDR_PVR2_CORE_FIRST + 0x02000000, ADDR_PVR2_CORE_LAST + 0x02000000, */
    /*                0x1fffffff, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN, */
    /*                &pvr2_core_reg_intf, NULL); */
    memory_map_add(map, ADDR_AICA_WAVE_FIRST + 0x02000000, ADDR_AICA_WAVE_LAST + 0x02000000,
                   0x1fffffff, ADDR_AICA_WAVE_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &aica_wave_mem_intf, &aica.mem);
    memory_map_add(map, 0x00700000 + 0x02000000, 0x00707fff + 0x02000000,
                   0x1fffffff, 0xffffffff, MEMORY_MAP_REGION_UNKNOWN,
                   &aica_sys_intf, &aica);
    memory_map_add(map, ADDR_AICA_RTC_FIRST + 0x02000000, ADDR_AICA_RTC_LAST + 0x02000000,
                   0x1fffffff, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &aica_rtc_intf, &rtc);
    memory_map_add(map, ADDR_GDROM_FIRST + 0x02000000, ADDR_GDROM_LAST + 0x02000000,
                   0x1fffffff, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &gdrom_reg_intf, &gdrom);
    memory_map_add(map, ADDR_EXT_DEV_FIRST + 0x02000000, ADDR_EXT_DEV_LAST + 0x02000000,
                   0x1fffffff, ADDR_AREA0_MASK, MEMORY_MAP_REGION_UNKNOWN,
                   &ext_dev_intf, NULL);

    /*
     * More PowerVR2 texture regions - these don't get used much which is why
     * they're at the end (for performance reasons).
     */
    memory_map_add(map, 0x04800000, 0x04ffffff, 0x1fffffff, 0x1fffffff,
                   MEMORY_MAP_REGION_UNKNOWN, &pvr2_tex_mem_unused_intf, NULL);
    memory_map_add(map, 0x05800000, 0x05ffffff, 0x1fffffff, 0x1fffffff,
                   MEMORY_MAP_REGION_UNKNOWN, &pvr2_tex_mem_unused_intf, NULL);
    memory_map_add(map, 0x06800000, 0x06ffffff, 0x1fffffff, 0x1fffffff,
                   MEMORY_MAP_REGION_UNKNOWN, &pvr2_tex_mem_unused_intf, NULL);
    memory_map_add(map, 0x07800000, 0x07ffffff, 0x1fffffff, 0x1fffffff,
                   MEMORY_MAP_REGION_UNKNOWN, &pvr2_tex_mem_unused_intf, NULL);

    map->unmap = &sh4_unmapped_mem;
}

void dc_request_frame_stop(void) {
    frame_stop = true;
}

static DEF_ERROR_U32_ATTR(ch2_dma_xfer_src_first)
static DEF_ERROR_U32_ATTR(ch2_dma_xfer_src_last)
static DEF_ERROR_U32_ATTR(ch2_dma_xfer_dst_first)
static DEF_ERROR_U32_ATTR(ch2_dma_xfer_dst_last)

void dc_set_lmmode0(unsigned val) {
    lmmode0 = val;
}

void dc_set_lmmode1(unsigned val) {
    lmmode1 = val;
}

unsigned dc_get_lmmode0(void) {
    return lmmode0;
}

unsigned dc_get_lmmode1(void) {
    return lmmode1;
}


/*
 * this is like dc_ch2_dma_xfer, but it's slower because it evaluates the
 * memory mapping of each 4 bytes individually.  We use it as a fallback for
 * situations where dc_ch2_dma_xfer sees a transfer that crosses the boundary
 * between regions.  Mars Matrix needs this because it does a 512KB transfer
 * into PVR2 texture memory which starts at the end of one of the main system
 * memory mirrors and crosses over into the beginning of the next mirror.
 */
static void
dc_ch2_dma_xfer_slow(addr32_t xfer_src, addr32_t xfer_dst, unsigned n_words) {
    unsigned n_bytes = 4 * n_words;
    uint32_t xfer_src_first = xfer_src;
    uint32_t xfer_src_last = xfer_src + n_bytes - 4;
    uint32_t xfer_dst_first = xfer_dst;
    uint32_t xfer_dst_last = xfer_dst + n_bytes - 4;

    uint32_t src = xfer_src_first;
    uint32_t dst = xfer_dst_first;
    while (src <= xfer_src_last) {
        uint32_t val = memory_map_read_32(&mem_map, src);

        if ((dst >= ADDR_TA_FIFO_POLY_FIRST) &&
            (dst <= ADDR_TA_FIFO_POLY_LAST)) {
            pvr2_ta_fifo_poly_write_32(dst, val, &dc_pvr2);
        } else if ((dst >= ADDR_AREA4_TEX_REGION_0_FIRST) &&
                   (dst <= ADDR_AREA4_TEX_REGION_0_LAST)) {
            uint32_t dst_offs = dst - ADDR_AREA4_TEX_REGION_0_FIRST;
            if (dc_get_lmmode0() == 0)
                pvr2_tex_mem_64bit_write32(&dc_pvr2, dst_offs, val);
            else
                pvr2_tex_mem_32bit_write32(&dc_pvr2, dst_offs, val);
        } else if ((xfer_dst >= ADDR_AREA4_TEX_REGION_1_FIRST) &&
                   (xfer_dst <= ADDR_AREA4_TEX_REGION_1_LAST)) {
            uint32_t dst_offs = dst - ADDR_AREA4_TEX_REGION_1_FIRST;
            if (dc_get_lmmode1() == 0)
                pvr2_tex_mem_64bit_write32(&dc_pvr2, dst_offs, val);
            else
                pvr2_tex_mem_32bit_write32(&dc_pvr2, dst_offs, val);
        } else if (xfer_dst >= ADDR_TA_FIFO_YUV_FIRST &&
               xfer_dst <= ADDR_TA_FIFO_YUV_LAST) {
            pvr2_yuv_input_data(&dc_pvr2, &val, sizeof(val));
        } else {
            error_set_ch2_dma_xfer_src_last(xfer_src_last);
            error_set_ch2_dma_xfer_src_first(xfer_src_first);
            error_set_ch2_dma_xfer_dst_last(xfer_dst_last);
            error_set_ch2_dma_xfer_dst_first(xfer_dst_first);
            error_set_address(xfer_src);
            error_set_length(4);
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }

        src += 4;
        dst += 4;
    }
}

dc_cycle_stamp_t
dc_ch2_dma_xfer(addr32_t xfer_src, addr32_t xfer_dst, unsigned n_words) {
    struct memory_map_region *src_region = memory_map_get_region(&mem_map,
                                                                 xfer_src,
                                                                 n_words * 4);

    addr32_t xfer_dst_initial = xfer_dst;
    unsigned n_words_initial = n_words;

    if (!src_region) {
        /*
         * This could mean that the transfer crosses regions, so try switching
         * to the fallback implementation.
         */
        dc_ch2_dma_xfer_slow(xfer_src, xfer_dst, n_words);
        goto the_end;
    }

    memory_map_read32_func read32 = src_region->intf->read32;
    void *ctxt = src_region->ctxt;
    uint32_t mask = src_region->mask;
    if ((xfer_dst >= ADDR_TA_FIFO_POLY_FIRST) &&
        (xfer_dst <= ADDR_TA_FIFO_POLY_LAST)) {
        while (n_words--) {
            uint32_t buf = read32(xfer_src & mask, ctxt);
            pvr2_ta_fifo_poly_write_32(xfer_dst, buf, &dc_pvr2);
            xfer_dst += sizeof(buf);
            xfer_src += sizeof(buf);
        }
    } else if ((xfer_dst >= ADDR_AREA4_TEX_REGION_0_FIRST) &&
               (xfer_dst <= ADDR_AREA4_TEX_REGION_0_LAST)) {
        // TODO: do tex DMA transfers in large chuks instead of 4-byte increments
        xfer_dst -= ADDR_AREA4_TEX_REGION_0_FIRST;
        if (dc_get_lmmode0() == 0) {
            while (n_words--) {
                uint32_t buf = read32(xfer_src & mask, ctxt);
                pvr2_tex_mem_64bit_write32(&dc_pvr2, xfer_dst, buf);
                xfer_dst += sizeof(buf);
                xfer_src += sizeof(buf);
            }
        } else {
            while (n_words--) {
                uint32_t buf = read32(xfer_src & mask, ctxt);
                pvr2_tex_mem_32bit_write32(&dc_pvr2, xfer_dst, buf);
                xfer_dst += sizeof(buf);
                xfer_src += sizeof(buf);
            }
        }
    } else if ((xfer_dst >= ADDR_AREA4_TEX_REGION_1_FIRST) &&
               (xfer_dst <= ADDR_AREA4_TEX_REGION_1_LAST)) {
        // TODO: do tex DMA transfers in large chuks instead of 4-byte increments
        xfer_dst -= ADDR_AREA4_TEX_REGION_1_FIRST;
        if (dc_get_lmmode1() == 0) {
            while (n_words--) {
                uint32_t buf = read32(xfer_src & mask, ctxt);
                pvr2_tex_mem_64bit_write32(&dc_pvr2, xfer_dst, buf);
                xfer_dst += sizeof(buf);
                xfer_src += sizeof(buf);
            }
        } else {
            while (n_words--) {
                uint32_t buf = read32(xfer_src & mask, ctxt);
                pvr2_tex_mem_32bit_write32(&dc_pvr2, xfer_dst, buf);
                xfer_dst += sizeof(buf);
                xfer_src += sizeof(buf);
            }
        }
    } else if (xfer_dst >= ADDR_TA_FIFO_YUV_FIRST &&
               xfer_dst <= ADDR_TA_FIFO_YUV_LAST) {
        while (n_words--) {
            uint32_t in = read32(xfer_src & mask, ctxt);
            xfer_src += sizeof(in);
            pvr2_yuv_input_data(&dc_pvr2, &in, sizeof(in));
        }
    } else {
        error_set_address(xfer_dst);
        error_set_length(n_words * 4);
        error_set_feature("channel-2 DMA transfers to an unknown destination");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

 the_end:
    // calculate dma timing, we return this to the sh4 code below
    if ((xfer_dst_initial >= ADDR_TA_FIFO_POLY_FIRST) &&
        (xfer_dst_initial <= ADDR_TA_FIFO_POLY_LAST)) {
        /*
         * TODO: WE NEED TO BENCMARK THIS.  BELOW TIMING IS
         * INACCURATE BECAUSE IT IS COPIED FROM TEXTURE MEMORY DMA!
         */
        double n_secs =
            ((n_words_initial*4) * 0.019373669058526 + 10.9678657897639) /
            (50 * 1024*1024/4);
        return n_secs * SCHED_FREQUENCY;
    } else if ((xfer_dst_initial >= ADDR_AREA4_TEX_REGION_0_FIRST) &&
               (xfer_dst_initial <= ADDR_AREA4_TEX_REGION_0_LAST)) {
        if (dc_get_lmmode0() == 0) {
            // this is accurate because it was empirically measured using the pvr2_mem_test
            double n_secs =
                ((n_words_initial*4) * 0.019373669058526 + 10.9678657897639) /
                (50 * 1024*1024/4);
            return n_secs * SCHED_FREQUENCY;
        } else {
            double n_secs =
                ((n_words_initial*4) * 0.032643091507195 + 9.09723447094439) /
                (50 * 1024*1024/4);
            return n_secs * SCHED_FREQUENCY;
        }
    } else if ((xfer_dst_initial >= ADDR_AREA4_TEX_REGION_1_FIRST) &&
               (xfer_dst_initial <= ADDR_AREA4_TEX_REGION_1_LAST)) {
        if (dc_get_lmmode1() == 0) {
            // this is accurate because it was empirically measured using the pvr2_mem_test
            double n_secs =
                ((n_words_initial*4) * 0.019373669058526 + 10.9678657897639) /
                (50 * 1024*1024/4);
            return n_secs * SCHED_FREQUENCY;
        } else {
            double n_secs =
                ((n_words_initial*4) * 0.032643091507195 + 9.09723447094439) /
                (50 * 1024*1024/4);
            return n_secs * SCHED_FREQUENCY;
        }
    } else if (xfer_dst_initial >= ADDR_TA_FIFO_YUV_FIRST &&
               xfer_dst_initial <= ADDR_TA_FIFO_YUV_LAST) {
        /*
         * Like the normal DMA timings, this was obtained empirically with the
         * pvr2_mem_test test program.  Behavior does not appear to be
         * completely linear in this case, may need to move to a more
         * complicated model.  In addition to starting off with negative
         * latency (which gets truncated to zero here), this model also is slightly
         * slower than an actual Dreamcast in the byte-range where I expect
         * most YUV DMA conversions would take place.
         */
        double n_secs =
            ((n_words_initial*4) * 0.071500073245219 - 3428.54930631431) /
            (50 * 1024*1024/4);
        if (n_secs < 0.0)
            n_secs = 0.0;
        return n_secs * SCHED_FREQUENCY;
    } else {
        // should never happen because we would have gone into the identical case above
        error_set_address(xfer_dst_initial);
        error_set_length(n_words_initial * 4);
        error_set_feature("channel-2 DMA transfers to an unknown destination");
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
    RAISE_ERROR(ERROR_INTEGRITY); // this should be impossible
}

int dc_try_read32(uint32_t addr, uint32_t *valp) {
#ifdef ENABLE_MMU
    struct sh4_utlb_ent *ent =
        sh4_utlb_find_ent_associative(&cpu, addr);
    if (ent)
        addr = sh4_utlb_ent_translate_addr(ent, addr);
#endif

    return memory_map_try_read_32(&mem_map, addr, valp);
}

void dc_get_pvr2_stats(struct pvr2_stat *stats) {
    *stats = dc_pvr2.stat;
}

static uint32_t trans_bind_washdc_to_maple(uint32_t wash) {
    uint32_t ret = 0;

    if (wash & WASHDC_CONT_BTN_C_MASK)
        ret |= MAPLE_CONT_BTN_C_MASK;
    if (wash & WASHDC_CONT_BTN_B_MASK)
        ret |= MAPLE_CONT_BTN_B_MASK;
    if (wash & WASHDC_CONT_BTN_A_MASK)
        ret |= MAPLE_CONT_BTN_A_MASK;
    if (wash & WASHDC_CONT_BTN_START_MASK)
        ret |= MAPLE_CONT_BTN_START_MASK;
    if (wash & WASHDC_CONT_BTN_DPAD_UP_MASK)
        ret |= MAPLE_CONT_BTN_DPAD_UP_MASK;
    if (wash & WASHDC_CONT_BTN_DPAD_DOWN_MASK)
        ret |= MAPLE_CONT_BTN_DPAD_DOWN_MASK;
    if (wash & WASHDC_CONT_BTN_DPAD_LEFT_MASK)
        ret |= MAPLE_CONT_BTN_DPAD_LEFT_MASK;
    if (wash & WASHDC_CONT_BTN_DPAD_RIGHT_MASK)
        ret |= MAPLE_CONT_BTN_DPAD_RIGHT_MASK;
    if (wash & WASHDC_CONT_BTN_Z_MASK)
        ret |= MAPLE_CONT_BTN_Z_MASK;
    if (wash & WASHDC_CONT_BTN_Y_MASK)
        ret |= MAPLE_CONT_BTN_Y_MASK;
    if (wash & WASHDC_CONT_BTN_X_MASK)
        ret |= MAPLE_CONT_BTN_X_MASK;
    if (wash & WASHDC_CONT_BTN_D_MASK)
        ret |= MAPLE_CONT_BTN_D_MASK;
    if (wash & WASHDC_CONT_BTN_DPAD2_UP_MASK)
        ret |= MAPLE_CONT_BTN_DPAD2_UP_MASK;
    if (wash & WASHDC_CONT_BTN_DPAD2_DOWN_MASK)
        ret |= MAPLE_CONT_BTN_DPAD2_DOWN_MASK;
    if (wash & WASHDC_CONT_BTN_DPAD2_LEFT_MASK)
        ret |= MAPLE_CONT_BTN_DPAD2_LEFT_MASK;
    if (wash & WASHDC_CONT_BTN_DPAD2_RIGHT_MASK)
        ret |= MAPLE_CONT_BTN_DPAD2_RIGHT_MASK;

    return ret;
}

void dc_controller_press_buttons(unsigned port_no, uint32_t btns) {
    maple_controller_press_btns(&maple, port_no, trans_bind_washdc_to_maple(btns));
}

void dc_controller_release_buttons(unsigned port_no, uint32_t btns) {
    maple_controller_release_btns(&maple, port_no, trans_bind_washdc_to_maple(btns));
}

static int trans_axis_washdc_to_maple(int axis) {
    switch (axis) {
    case WASHDC_CONTROLLER_AXIS_R_TRIG:
        return MAPLE_CONTROLLER_AXIS_R_TRIG;
    case WASHDC_CONTROLLER_AXIS_L_TRIG:
        return MAPLE_CONTROLLER_AXIS_L_TRIG;
    case WASHDC_CONTROLLER_AXIS_JOY1_Y:
        return MAPLE_CONTROLLER_AXIS_JOY1_Y;
    case WASHDC_CONTROLLER_AXIS_JOY2_X:
        return MAPLE_CONTROLLER_AXIS_JOY2_X;
    case WASHDC_CONTROLLER_AXIS_JOY2_Y:
        return MAPLE_CONTROLLER_AXIS_JOY2_Y;
    default:
        LOG_ERROR("unknown axis %d\n", axis);
    case WASHDC_CONTROLLER_AXIS_JOY1_X:
        return MAPLE_CONTROLLER_AXIS_JOY1_X;
    }
}

void dc_controller_set_axis(unsigned port_no, unsigned axis, unsigned val) {
    maple_controller_set_axis(&maple, port_no,
                              trans_axis_washdc_to_maple(axis), val);
}

void dc_keyboard_set_key(unsigned port_no, unsigned btn_no, bool is_pressed) {
    maple_keyboard_press_key(&maple, port_no, btn_no, is_pressed);
}

void dc_keyboard_press_special(unsigned port_no,
                               enum washdc_keyboard_special_keys which) {
    enum maple_keyboard_special_keys spec = MAPLE_KEYBOARD_NONE;
    if (which & WASHDC_KEYBOARD_LEFT_CTRL)
        spec |= MAPLE_KEYBOARD_LEFT_CTRL;
    if (which & WASHDC_KEYBOARD_LEFT_SHIFT)
        spec |= MAPLE_KEYBOARD_LEFT_SHIFT;
    if (which & WASHDC_KEYBOARD_LEFT_ALT)
        spec |= MAPLE_KEYBOARD_LEFT_ALT;
    if (which & WASHDC_KEYBOARD_S1)
        spec |= MAPLE_KEYBOARD_S1;
    if (which & WASHDC_KEYBOARD_RIGHT_CTRL)
        spec |= MAPLE_KEYBOARD_RIGHT_CTRL;
    if (which & WASHDC_KEYBOARD_RIGHT_SHIFT)
        spec |= MAPLE_KEYBOARD_RIGHT_SHIFT;
    if (which & WASHDC_KEYBOARD_RIGHT_ALT)
        spec |= MAPLE_KEYBOARD_RIGHT_ALT;
    if (which & WASHDC_KEYBOARD_S2)
        spec |= MAPLE_KEYBOARD_S2;
    maple_keyboard_press_special(&maple, port_no, spec);
}

void dc_keyboard_release_special(unsigned port_no,
                                 enum washdc_keyboard_special_keys which) {
    enum maple_keyboard_special_keys spec = MAPLE_KEYBOARD_NONE;
    if (which & WASHDC_KEYBOARD_LEFT_CTRL)
        spec |= MAPLE_KEYBOARD_LEFT_CTRL;
    if (which & WASHDC_KEYBOARD_LEFT_SHIFT)
        spec |= MAPLE_KEYBOARD_LEFT_SHIFT;
    if (which & WASHDC_KEYBOARD_LEFT_ALT)
        spec |= MAPLE_KEYBOARD_LEFT_ALT;
    if (which & WASHDC_KEYBOARD_S1)
        spec |= MAPLE_KEYBOARD_S1;
    if (which & WASHDC_KEYBOARD_RIGHT_CTRL)
        spec |= MAPLE_KEYBOARD_RIGHT_CTRL;
    if (which & WASHDC_KEYBOARD_RIGHT_SHIFT)
        spec |= MAPLE_KEYBOARD_RIGHT_SHIFT;
    if (which & WASHDC_KEYBOARD_RIGHT_ALT)
        spec |= MAPLE_KEYBOARD_RIGHT_ALT;
    if (which & WASHDC_KEYBOARD_S2)
        spec |= MAPLE_KEYBOARD_S2;
    maple_keyboard_release_special(&maple, port_no, spec);
}

static float sh4_unmapped_readfloat(uint32_t addr, void *ctxt) {
    error_set_feature("memory mapping");
    error_set_address(addr);
    error_set_length(sizeof(float));
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static double sh4_unmapped_readdouble(uint32_t addr, void *ctxt) {
    error_set_feature("memory mapping");
    error_set_address(addr);
    error_set_length(sizeof(double));
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static uint32_t sh4_unmapped_read32(uint32_t addr, void *ctxt) {
    error_set_feature("memory mapping");
    error_set_address(addr);
    error_set_length(sizeof(uint32_t));
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static uint16_t sh4_unmapped_read16(uint32_t addr, void *ctxt) {
    error_set_feature("memory mapping");
    error_set_address(addr);
    error_set_length(sizeof(uint16_t));
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static uint8_t sh4_unmapped_read8(uint32_t addr, void *ctxt) {
    error_set_feature("memory mapping");
    error_set_address(addr);
    error_set_length(sizeof(uint8_t));
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void sh4_unmapped_writefloat(uint32_t addr, float val, void *ctxt) {
    uint32_t val_hex;
    memcpy(&val_hex, &val, sizeof(val_hex));
    error_set_feature("memory mapping");
    error_set_value(val_hex);
    error_set_address(addr);
    error_set_length(sizeof(float));
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void sh4_unmapped_writedouble(uint32_t addr, double val, void *ctxt) {
    uint64_t val_hex;
    memcpy(&val_hex, &val, sizeof(val_hex));
    error_set_feature("memory mapping");
    error_set_value(val_hex);
    error_set_address(addr);
    error_set_length(sizeof(double));
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void sh4_unmapped_write32(uint32_t addr, uint32_t val, void *ctxt) {
    /*
     * XXX I think maybe these addresses are intended to go to the SH4 ocache
     * ram since bits 0-28 of the addresses match up, but that doesn't seem
     * right since the spec makes it pretty clear that the ocache ram area goes
     * from 0x7c000000 to 0x7fffffff (ie the upper 3 bits should be important).
     */
    if (((addr >> 16) == 0xbc2d) && !val) {
        /*
         * HACK - this memory region is off-limits according to SH-4 documentation.
         * Star Wars Episode I Racer writes 0 (4-bytes) to the following addresses:
         * 0xbc2dca74, 0xbc2dcdc4, 0xbc2dd114, 0xbc2dd464, 0xbc2dd7b4, 0xbc2ddb04,
         * 0xbc2dde54, 0xbc2de1a4, 0xbc2de4f4, 0xbc2de844, 0xbc2deb94, 0xbc2deee4,
         * 0xbc2df234, 0xbc2df584, 0xbc2df8d4, 0xbc2dfc24.  Note that all values
         * are 0x350 apart.
         *
         * citation for this being off-limits is page 268 of sh7750.pdf (Hitachi
         * SH-4 hardware manual):
         *
         * "The area 7 address range, H'1C000000 to H'1FFFFFFFF, is a reserved
         * space and must not be used."
         *
         * I have confirmed via hardware test that this is *not* a mirror of
         * the main system ram.  I have also confirmed that on real hardware
         * writes to these addresses retain their values, so there must be some
         * sort of registers or memory backing these addresses.
         *
         * Without further information it's impossible to know what these
         * addresses are, so for now we'll allow writes of 0 to pass while still
         * failing on non-zero writes.  According to hardware tests, 0 is the
         * default value of all of these registers, anyways.
         */
        LOG_WARN("%s (PC=0x%08x) - allowing 4-byte write of 0x%08x to unmapped address "
                 "0x%08x\n", __func__, (unsigned)cpu.reg[SH4_REG_PC], (unsigned)val, (unsigned)addr);
    } else if ((((addr >> 16) == 0xbc52) || ((addr >> 16) == 0xbc53)) && !val) {
        // same situation as above, but this time it's Bangai-O
        LOG_WARN("%s (PC=0x%08x) - allowing 4-byte write of 0x%08x to unmapped address "
                 "0x%08x\n", __func__, (unsigned)cpu.reg[SH4_REG_PC], (unsigned)val, (unsigned)addr);
    } else {
        error_set_feature("memory mapping");
        error_set_value(val);
        error_set_address(addr);
        error_set_length(sizeof(uint32_t));
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

static void sh4_unmapped_write16(uint32_t addr, uint16_t val, void *ctxt) {
    error_set_feature("memory mapping");
    error_set_value(val);
    error_set_address(addr);
    error_set_length(sizeof(uint16_t));
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void sh4_unmapped_write8(uint32_t addr, uint8_t val, void *ctxt) {
    error_set_feature("memory mapping");
    error_set_value(val);
    error_set_address(addr);
    error_set_length(sizeof(uint8_t));
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static struct memory_interface sh4_unmapped_mem = {
    .readdouble = sh4_unmapped_readdouble,
    .readfloat = sh4_unmapped_readfloat,
    .read32 = sh4_unmapped_read32,
    .read16 = sh4_unmapped_read16,
    .read8 = sh4_unmapped_read8,

    .writedouble = sh4_unmapped_writedouble,
    .writefloat = sh4_unmapped_writefloat,
    .write32 = sh4_unmapped_write32,
    .write16 = sh4_unmapped_write16,
    .write8 = sh4_unmapped_write8
};

static uint32_t on_pdtra_read(struct Sh4* sh4) {
    /*
     * HACK - prevent infinite loop during bios boot at pc=0x8c00b94e
     * I'm not 100% sure what I'm doing here, I *think* PDTRA has something to
     * do with the display adapter.
     *
     * Basically, the boot rom writes a sequence of values to PDTRA (with
     * pctra's i/o selects toggling occasionally) and it expects a certain
     * sequence of values when it reads back from pdtra.  I mask in the values
     * it writes as outputs into the value of pdtra which is read back (because
     * according to the sh4 spec, output bits can be read as inputs and they
     * will have the value which was last written to them) and send it either 0
     * or 3 on the input bits based on the address in the PR register.
     * Hopefully this is good enough.
     *
     * If the boot rom doesn't get a value it wants to see after 10 attempts,
     * then it branches to GBR (0x8c000000), where it will put the processor to
     * sleep with interrupts disabled (ie forever).  Presumably this is all it
     * can due to handle an error at such an early stage in the boot process.
     */

    /*
     * n_pup = "not pullup"
     * n_input = "not input"
     */
    uint16_t n_pup_mask = 0, n_input_mask = 0;
    uint32_t pctra = sh4->reg[SH4_REG_PCTRA];

    /* parse out the PCTRA register */
    unsigned bit_no;
    for (bit_no = 0; bit_no < 16; bit_no++) {
        uint32_t n_input = (1 << (bit_no * 2) & pctra) >> (bit_no * 2);
        uint32_t n_pup = (1 << (bit_no * 2 + 1) & pctra) >> (bit_no * 2 + 1);

        n_pup_mask |= n_pup << bit_no;
        n_input_mask |= n_input << bit_no;
    }

    /*
     * Put the first byte to 0xe because that seems to be what it always is on
     * real hardware.
     */
    uint32_t out_val = 0xe0;

    out_val |= 0x0300; // hardocde cable type to composite NTSC video

    /*
     * The lower 4 bits of the output value appear to be important, but I don't
     * know what they represent.  The below table was dumped from an NTSC-U
     * Dreamcast connected to a TV via composite video.  If these values are
     * wrong, then the Dreamcast firmware will hang during early bootup.
     */
    unsigned const tbl[16][4] = {
        { 0x03, 0x03, 0x03, 0x03 },
        { 0x00, 0x03, 0x00, 0x03 },
        { 0x03, 0x03, 0x03, 0x03 },
        { 0x00, 0x03, 0x00, 0x03 },
        { 0x00, 0x00, 0x03, 0x03 },
        { 0x00, 0x01, 0x02, 0x03 },
        { 0x00, 0x00, 0x03, 0x03 },
        { 0x00, 0x01, 0x02, 0x03 },
        { 0x03, 0x03, 0x03, 0x03 },
        { 0x00, 0x03, 0x00, 0x03 },
        { 0x03, 0x03, 0x03, 0x03 },
        { 0x00, 0x03, 0x00, 0x03 },
        { 0x00, 0x00, 0x03, 0x03 },
        { 0x00, 0x01, 0x02, 0x03 },
        { 0x00, 0x00, 0x03, 0x03 },
        { 0x00, 0x01, 0x02, 0x03 }
    };

    out_val |= tbl[pctra & 0xf][sh4->reg[SH4_REG_PDTRA] & 3];

    /*
     * TODO:
     * I also need to add in a way to select the TV video type in bits 4:2.  For
     * now I leave those three bits at zero, which corresponds to NTSC.  For PAL
     * formats, some of those bits are supposed to be non-zero.
     *
     * ALSO TODO: What about the upper two bytes of PDTRA?
     */

    /*
     * Now combine this with the values previously written to PDTRA - remember
     * that bits set to output can be read back, and that they should have the
     * same values that were written to them.
     */
    out_val = (out_val & ~n_input_mask) |
        (sh4->reg[SH4_REG_PDTRA] & n_input_mask);

    /* I got my eye on you...*/
    LOG_DBG("reading 0x%04x from register PDTRa\n", (unsigned)out_val);

    return out_val;
}

static void on_pdtra_write(struct Sh4* sh4, uint32_t val) {
    WASHDC_UNUSED sh4_reg_val val_orig = val;

    /*
     * n_pup = "not pullup"
     * n_input = "not input"
     */
    uint16_t n_pup_mask = 0, n_input_mask = 0;
    uint32_t pctra = sh4->reg[SH4_REG_PCTRA];

    /* parse out the PCTRA register */
    unsigned bit_no;
    for (bit_no = 0; bit_no < 16; bit_no++) {
        uint32_t n_input = (1 << (bit_no * 2) & pctra) >> (bit_no * 2);
        uint32_t n_pup = (1 << (bit_no * 2 + 1) & pctra) >> (bit_no * 2 + 1);

        n_pup_mask |= n_pup << bit_no;
        n_input_mask |= n_input << bit_no;
    }

    /* I got my eye on you...*/
    LOG_DBG("WARNING: writing 0x%04x to register pdtra "
            "(attempted write was %x)\n",
            (unsigned)val, (unsigned)val_orig);

    sh4->reg[SH4_REG_PDTRA] = val;
}

/*
 * Evolution: The World of Sacred Device will attempt to read and write to
 * invalid addresses from the ARM7.  This behavior was also observed when I
 * tested it on MAME.  On real hardware, this does not fail.  The value returned
 * by read operations is all zeroes.  I have confirmed this behavior with a
 * hardware test.
 */

static float arm7_unmapped_readfloat(uint32_t addr, void *ctxt) {
    return 0.0f;
}

static double arm7_unmapped_readdouble(uint32_t addr, void *ctxt) {
    return 0.0;
}

static uint32_t arm7_unmapped_read32(uint32_t addr, void *ctxt) {
    return 0;
}

static uint16_t arm7_unmapped_read16(uint32_t addr, void *ctxt) {
    return 0;
}

static uint8_t arm7_unmapped_read8(uint32_t addr, void *ctxt) {
    return 0;
}

static void arm7_unmapped_writefloat(uint32_t addr, float val, void *ctxt) {
}

static void arm7_unmapped_writedouble(uint32_t addr, double val, void *ctxt) {
}

static void arm7_unmapped_write32(uint32_t addr, uint32_t val, void *ctxt) {
}

static void arm7_unmapped_write16(uint32_t addr, uint16_t val, void *ctxt) {
}

static void arm7_unmapped_write8(uint32_t addr, uint8_t val, void *ctxt) {
}

static struct memory_interface arm7_unmapped_mem = {
    .readdouble = arm7_unmapped_readdouble,
    .readfloat = arm7_unmapped_readfloat,
    .read32 = arm7_unmapped_read32,
    .read16 = arm7_unmapped_read16,
    .read8 = arm7_unmapped_read8,

    .writedouble = arm7_unmapped_writedouble,
    .writefloat = arm7_unmapped_writefloat,
    .write32 = arm7_unmapped_write32,
    .write16 = arm7_unmapped_write16,
    .write8 = arm7_unmapped_write8
};
