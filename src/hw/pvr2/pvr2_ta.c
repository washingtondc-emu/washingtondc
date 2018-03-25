/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017, 2018 snickerbockers
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

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "error.h"
#include "gfx/gfx.h"
#include "hw/sys/holly_intc.h"
#include "pvr2_core_reg.h"
#include "pvr2_tex_mem.h"
#include "pvr2_tex_cache.h"
#include "framebuffer.h"
#include "host_branch_pred.h"
#include "log.h"
#include "dc_sched.h"
#include "dreamcast.h"
#include "gfx/gfx_il.h"

#include "pvr2_ta.h"

#define PVR2_CMD_MAX_LEN 64

#define TA_CMD_TYPE_SHIFT 29
#define TA_CMD_TYPE_MASK (0x7 << TA_CMD_TYPE_SHIFT)

#define TA_CMD_END_OF_STRIP_SHIFT 28
#define TA_CMD_END_OF_STRIP_MASK (1 << TA_CMD_END_OF_STRIP_SHIFT)

#define TA_CMD_DISP_LIST_SHIFT 24
#define TA_CMD_DISP_LIST_MASK (0x7 << TA_CMD_DISP_LIST_SHIFT)

/*
 * this has something to do with swapping out the ISP parameters
 * when modifier volumes are in use, I think
 */
#define TA_CMD_SHADOW_SHIFT 7
#define TA_CMD_SHADOW_MASK (1 << TA_CMD_SHADOW_SHIFT)

#define TA_CMD_TWO_VOLUMES_SHIFT 6
#define TA_CMD_TWO_VOLUMES_MASK (1 << TA_CMD_TWO_VOLUMES_SHIFT)

#define TA_CMD_COLOR_TYPE_SHIFT 4
#define TA_CMD_COLOR_TYPE_MASK (3 << TA_CMD_COLOR_TYPE_SHIFT)

#define TA_CMD_TEX_ENABLE_SHIFT 3
#define TA_CMD_TEX_ENABLE_MASK (1 << TA_CMD_TEX_ENABLE_SHIFT)

#define TA_CMD_OFFSET_COLOR_SHIFT 2
#define TA_CMD_OFFSET_COLOR_MASK (1 << TA_CMD_OFFSET_COLOR_SHIFT)

#define TA_CMD_GOURAD_SHADING_SHIFT 1
#define TA_CMD_GOURAD_SHADING_MASK (1 << TA_CMD_GOURAD_SHADING_SHIFT)

#define TA_CMD_16_BIT_TEX_COORD_SHIFT 0
#define TA_CMD_16_BIT_TEX_COORD_MASK (1 << TA_CMD_16_BIT_TEX_COORD_SHIFT)

enum ta_color_type {
    TA_COLOR_TYPE_PACKED,
    TA_COLOR_TYPE_FLOAT,
    TA_COLOR_TYPE_INTENSITY_MODE_1,
    TA_COLOR_TYPE_INTENSITY_MODE_2
};

#define TA_CMD_TYPE_END_OF_LIST 0x0
#define TA_CMD_TYPE_USER_CLIP   0x1
#define TA_CMD_TYPE_INPUT_LIST  0x2
// what is 3?
#define TA_CMD_TYPE_POLY_HDR    0x4
#define TA_CMD_TYPE_SPRITE_HDR  0x5
#define TA_CMD_TYPE_UNKNOWN     0x6  // I can't find any info on what this is
#define TA_CMD_TYPE_VERTEX      0x7

#define TA_COLOR_FMT_SHIFT 4
#define TA_COLOR_FMT_MASK (3 << TA_COLOR_FMT_SHIFT)

#define ISP_BACKGND_T_ADDR_SHIFT 1
#define ISP_BACKGND_T_ADDR_MASK (0x7ffffc << ISP_BACKGND_T_ADDR_SHIFT)

#define ISP_BACKGND_T_SKIP_SHIFT 24
#define ISP_BACKGND_T_SKIP_MASK (7 << ISP_BACKGND_T_SKIP_SHIFT)

#define TSP_WORD_SRC_ALPHA_FACTOR_SHIFT 29
#define TSP_WORD_SRC_ALPHA_FACTOR_MASK (7 << TSP_WORD_SRC_ALPHA_FACTOR_SHIFT)

#define TSP_WORD_DST_ALPHA_FACTOR_SHIFT 26
#define TSP_WORD_DST_ALPHA_FACTOR_MASK (7 << TSP_WORD_DST_ALPHA_FACTOR_SHIFT)

#define DEPTH_FUNC_SHIFT 29
#define DEPTH_FUNC_MASK (7 << DEPTH_FUNC_SHIFT)

#define DEPTH_WRITE_DISABLE_SHIFT 26
#define DEPTH_WRITE_DISABLE_MASK (1 << DEPTH_WRITE_DISABLE_SHIFT)

static uint8_t ta_fifo[PVR2_CMD_MAX_LEN];

static unsigned ta_fifo_byte_count = 0;


/*
 * There are five display lists:
 *
 * Opaque
 * Punch-through polygon
 * Opaque/punch-through modifier volume
 * Translucent
 * Translucent modifier volume
 *
 * They are rendered by the opengl backend in that order.
 */
enum display_list_type {
    DISPLAY_LIST_FIRST,
    DISPLAY_LIST_OPAQUE = DISPLAY_LIST_FIRST,
    DISPLAY_LIST_OPAQUE_MOD,
    DISPLAY_LIST_TRANS,
    DISPLAY_LIST_TRANS_MOD,
    DISPLAY_LIST_PUNCH_THROUGH,
    DISPLAY_LIST_LAST = DISPLAY_LIST_PUNCH_THROUGH,

    // These three list types are invalid, but I do see DISPLAY_LIST_7 sometimes
    DISPLAY_LIST_5,
    DISPLAY_LIST_6,
    DISPLAY_LIST_7,

    DISPLAY_LIST_COUNT,

    DISPLAY_LIST_NONE = -1
};

enum vert_type {
    VERT_NO_TEX_PACKED_COLOR,
    VERT_NO_TEX_FLOAT_COLOR,
    VERT_NO_TEX_INTENSITY,
    VERT_TEX_PACKED_COLOR,
    VERT_TEX_PACKED_COLOR_16_BIT_TEX_COORD,
    VERT_TEX_FLOATING_COLOR,
    VERT_TEX_FLOATING_COLOR_16_BIT_TEX_COORD,
    VERT_TEX_INTENSITY,
    VERT_TEX_INTENSITY_16_BIT_TEX_COORD,
    VERT_NO_TEX_PACKED_COLOR_TWO_VOLUMES,
    VERT_NO_TEX_INTENSITY_TWO_VOLUMES,
    VERT_TEX_PACKED_COLOR_TWO_VOLUMES,
    VERT_TEX_PACKED_COLOR_TWO_VOLUMES_16_BIT_TEX_COORD,
    VERT_TEX_INTENSITY_TWO_VOLUMES,
    VERT_TEX_INTENSITY_TWO_VOLUMES_16_BIT_TEX_COORD,

    N_VERT_TYPES
};

static DEF_ERROR_INT_ATTR(src_blend_factor);
static DEF_ERROR_INT_ATTR(dst_blend_factor);
static DEF_ERROR_INT_ATTR(display_list_index);
static DEF_ERROR_INT_ATTR(geo_buf_group_index);
static DEF_ERROR_INT_ATTR(ta_fifo_cmd)
static DEF_ERROR_INT_ATTR(pvr2_global_param)
static DEF_ERROR_INT_ATTR(ta_fifo_byte_count)
static DEF_ERROR_U32_ATTR(ta_fifo_word_0)
static DEF_ERROR_U32_ATTR(ta_fifo_word_1)
static DEF_ERROR_U32_ATTR(ta_fifo_word_2)
static DEF_ERROR_U32_ATTR(ta_fifo_word_3)
static DEF_ERROR_U32_ATTR(ta_fifo_word_4)
static DEF_ERROR_U32_ATTR(ta_fifo_word_5)
static DEF_ERROR_U32_ATTR(ta_fifo_word_6)
static DEF_ERROR_U32_ATTR(ta_fifo_word_7)
static DEF_ERROR_U32_ATTR(ta_fifo_word_8)
static DEF_ERROR_U32_ATTR(ta_fifo_word_9)
static DEF_ERROR_U32_ATTR(ta_fifo_word_a)
static DEF_ERROR_U32_ATTR(ta_fifo_word_b)
static DEF_ERROR_U32_ATTR(ta_fifo_word_c)
static DEF_ERROR_U32_ATTR(ta_fifo_word_d)
static DEF_ERROR_U32_ATTR(ta_fifo_word_e)
static DEF_ERROR_U32_ATTR(ta_fifo_word_f)

enum global_param {
    GLOBAL_PARAM_POLY = 4,
    GLOBAL_PARAM_SPRITE = 5
};

struct pvr2_ta_vert {
    float pos[3];
    float base_color[4];
    float offs_color[4];
    float tex_coord[2];
};

struct poly_hdr {
    bool tex_enable;
    uint32_t tex_addr;

    /*
     * this is the upper 2-bits (for 8BPP) or 6 bits (for 4BPP) of every
     * palette address referenced by this texture.  It needs to be shifted left
     * by 2 or 6 bits and ORed with pixel values to get palette addresses.
     *
     * this field only holds meaning if tex_fmt is TEX_CTRL_PIX_FMT_4_BPP_PAL
     * or TEX_CTRL_PIX_FMT_8_BPP_PAL; otherwise it is meaningless.
     */
    uint32_t tex_palette_start;

    unsigned tex_width_shift, tex_height_shift;
    bool tex_twiddle;
    bool stride_sel;
    bool tex_vq_compression;
    bool tex_mipmap;
    int tex_fmt;
    enum tex_inst tex_inst;
    enum tex_filter tex_filter;
    enum tex_wrap_mode tex_wrap_mode[2];

    unsigned ta_color_fmt;
    enum Pvr2BlendFactor src_blend_factor, dst_blend_factor;

    bool enable_depth_writes;
    enum Pvr2DepthFunc depth_func;

    bool shadow;
    bool two_volumes_mode;
    enum ta_color_type color_type;
    bool offset_color_enable;
    bool gourad_shading_enable;
    bool tex_coord_16_bit_enable;

    float poly_base_color_rgba[4];
    float poly_offs_color_rgba[4];

    float sprite_base_color_rgba[4];
    float sprite_offs_color_rgba[4];
};

static struct poly_state {
    enum global_param global_param;

    // used to store the previous two verts when we're rendering a triangle strip
    struct pvr2_ta_vert strip_vert_1;
    struct pvr2_ta_vert strip_vert_2;
    unsigned strip_len; // number of verts in the current triangle strip

    unsigned ta_color_fmt;

    bool tex_enable;

    // index into the texture cache
    unsigned tex_idx;

    // which display list is currently open
    enum display_list_type current_list;

    enum Pvr2BlendFactor src_blend_factor, dst_blend_factor;
    enum tex_wrap_mode tex_wrap_mode[2];

    bool enable_depth_writes;
    enum Pvr2DepthFunc depth_func;

    bool shadow;
    bool two_volumes_mode;
    enum ta_color_type color_type;
    bool offset_color_enable;
    bool gourad_shading_enable;
    bool tex_coord_16_bit_enable;

    enum tex_inst tex_inst;
    enum tex_filter tex_filter;

    // number of 4-byte quads per vertex
    unsigned vert_len;

    float poly_base_color_rgba[4];
    float poly_offs_color_rgba[4];

    float sprite_base_color_rgba[4];
    float sprite_offs_color_rgba[4];

    enum vert_type vert_type;
} poly_state = {
    .current_list = DISPLAY_LIST_NONE,
    .vert_len = 8
};

char const *display_list_names[DISPLAY_LIST_COUNT] = {
    "Opaque",
    "Opaque Modifier Volume",
    "Transparent",
    "Transparent Modifier Volume",
    "Punch-through Polygon",
    "Unknown Display list 6",
    "Unknown Display list 7"
};

// lengths of each type of vert, in terms of 32-bit integers
static const unsigned vert_lengths[N_VERT_TYPES] = {
    [VERT_NO_TEX_PACKED_COLOR] = 8,
    [VERT_NO_TEX_FLOAT_COLOR] = 8,
    [VERT_NO_TEX_INTENSITY] = 8,
    [VERT_TEX_PACKED_COLOR] = 8,
    [VERT_TEX_PACKED_COLOR_16_BIT_TEX_COORD] = 8,
    [VERT_TEX_FLOATING_COLOR] = 16,
    [VERT_TEX_FLOATING_COLOR_16_BIT_TEX_COORD] = 16,
    [VERT_TEX_INTENSITY] = 8,
    [VERT_TEX_INTENSITY_16_BIT_TEX_COORD] = 8,
    [VERT_NO_TEX_PACKED_COLOR_TWO_VOLUMES] = 8,
    [VERT_NO_TEX_INTENSITY_TWO_VOLUMES] = 8,
    [VERT_TEX_PACKED_COLOR_TWO_VOLUMES] = 16,
    [VERT_TEX_PACKED_COLOR_TWO_VOLUMES_16_BIT_TEX_COORD] = 16,
    [VERT_TEX_INTENSITY_TWO_VOLUMES] = 16,
    [VERT_TEX_INTENSITY_TWO_VOLUMES_16_BIT_TEX_COORD] = 16
};

bool list_submitted[DISPLAY_LIST_COUNT];

static void input_poly_fifo(uint8_t byte);

// this function gets called every time a full packet is received by the TA
static void on_packet_received(void);
static void on_polyhdr_received(void);
static void on_vertex_received(void);
static void on_sprite_received(void);
static void on_end_of_list_received(void);
static void on_user_clip_received(void);

static void render_frame_init(void);

static void finish_poly_group(enum display_list_type disp_list);
static void next_poly_group(enum display_list_type disp_list);

static void decode_poly_hdr(struct poly_hdr *hdr);

// call this whenever a packet has been processed
static void ta_fifo_finish_packet(void);

static enum vert_type classify_vert(void);

static void unpack_uv16(float *u_coord, float *v_coord, void const *input);

/*
 * the delay between when the STARTRENDER command is received and when the
 * RENDER_COMPLETE interrupt gets raised.
 * this value is arbitrary, for now it is 0 so it happens instantly.
 * TODO: This irq definitely should not be triggered immediately
 */
#define PVR2_RENDER_COMPLETE_INT_DELAY 0

static void pvr2_render_complete_int_event_handler(struct SchedEvent *event);

static struct SchedEvent pvr2_render_complete_int_event = {
    .handler = pvr2_render_complete_int_event_handler
};

static bool pvr2_render_complete_int_event_scheduled;

/*
 * the delay between when a list is rendered and when the list-complete
 * interrupt happens.
 * this value is arbitrary, for now it is 0 so it happens instantly.
 * TODO: In a real dreamcast this probably would not happen instantly
 */
#define PVR2_LIST_COMPLETE_INT_DELAY 0

static void pvr2_op_complete_int_event_handler(struct SchedEvent *event);
static void pvr2_op_mod_complete_int_event_handler(struct SchedEvent *event);
static void pvr2_trans_complete_int_event_handler(struct SchedEvent *event);
static void pvr2_trans_mod_complete_int_event_handler(struct SchedEvent *event);
static void pvr2_pt_complete_int_event_handler(struct SchedEvent *event);

static struct SchedEvent pvr2_op_complete_int_event = {
    .handler = pvr2_op_complete_int_event_handler
};

static struct SchedEvent pvr2_op_mod_complete_int_event = {
    .handler = pvr2_op_mod_complete_int_event_handler
};

static struct SchedEvent pvr2_trans_complete_int_event = {
    .handler = pvr2_trans_complete_int_event_handler
};

static struct SchedEvent pvr2_trans_mod_complete_int_event = {
    .handler = pvr2_trans_mod_complete_int_event_handler
};

static struct SchedEvent pvr2_pt_complete_int_event = {
    .handler = pvr2_pt_complete_int_event_handler
};

static bool pvr2_op_complete_int_event_scheduled,
    pvr2_op_mod_complete_int_event_scheduled,
    pvr2_trans_complete_int_event_scheduled,
    pvr2_trans_mod_complete_int_event_scheduled,
    pvr2_pt_complete_int_event_scheduled;

#define PVR2_TA_VERT_BUF_LEN (1024 * 1024)
static float *pvr2_ta_vert_buf;
static unsigned pvr2_ta_vert_buf_count;
static unsigned pvr2_ta_vert_cur_group;

#define PVR2_GFX_IL_INST_BUF_LEN (1024 * 256)
struct gfx_il_inst_chain {
    struct gfx_il_inst cmd;
    struct gfx_il_inst_chain *next;
};

struct gfx_il_inst_chain *disp_list_begin[DISPLAY_LIST_COUNT];
struct gfx_il_inst_chain *disp_list_end[DISPLAY_LIST_COUNT];

static struct gfx_il_inst_chain *gfx_il_inst_buf;
static unsigned gfx_il_inst_buf_count;

// the 4-component color that gets sent to glClearColor
static float pvr2_bgcolor[4];

static float clip_min, clip_max;

static bool open_group;

static unsigned next_frame_stamp;

void pvr2_ta_init(void) {
    pvr2_ta_vert_buf = (float*)malloc(PVR2_TA_VERT_BUF_LEN *
                                      sizeof(float) * GFX_VERT_LEN);
    if (!pvr2_ta_vert_buf)
        RAISE_ERROR(ERROR_FAILED_ALLOC);
    gfx_il_inst_buf = (struct gfx_il_inst_chain*)malloc(PVR2_GFX_IL_INST_BUF_LEN *
                                                        sizeof(struct gfx_il_inst_chain));
    if (!gfx_il_inst_buf)
        RAISE_ERROR(ERROR_FAILED_ALLOC);

    pvr2_ta_vert_buf_count = 0;
    pvr2_ta_vert_cur_group = 0;

    render_frame_init();
}

void pvr2_ta_cleanup(void) {
    free(gfx_il_inst_buf);
    free(pvr2_ta_vert_buf);
    pvr2_ta_vert_buf = NULL;
    pvr2_ta_vert_buf_count = 0;
    pvr2_ta_vert_cur_group = 0;
}

static inline void pvr2_ta_push_vert(struct pvr2_ta_vert vert) {
    if (pvr2_ta_vert_buf_count >= PVR2_TA_VERT_BUF_LEN) {
        LOG_WARN("PVR2 TA vertex buffer overflow\n");
        return;
    }

    float *outp = pvr2_ta_vert_buf + GFX_VERT_LEN * pvr2_ta_vert_buf_count++;
    outp[GFX_VERT_POS_OFFSET + 0] = vert.pos[0];
    outp[GFX_VERT_POS_OFFSET + 1] = vert.pos[1];
    outp[GFX_VERT_POS_OFFSET + 2] = vert.pos[2];
    outp[GFX_VERT_BASE_COLOR_OFFSET + 0] = vert.base_color[0];
    outp[GFX_VERT_BASE_COLOR_OFFSET + 1] = vert.base_color[1];
    outp[GFX_VERT_BASE_COLOR_OFFSET + 2] = vert.base_color[2];
    outp[GFX_VERT_BASE_COLOR_OFFSET + 3] = vert.base_color[3];
    outp[GFX_VERT_OFFS_COLOR_OFFSET + 0] = vert.offs_color[0];
    outp[GFX_VERT_OFFS_COLOR_OFFSET + 1] = vert.offs_color[1];
    outp[GFX_VERT_OFFS_COLOR_OFFSET + 2] = vert.offs_color[2];
    outp[GFX_VERT_OFFS_COLOR_OFFSET + 3] = vert.offs_color[3];
    outp[GFX_VERT_TEX_COORD_OFFSET + 0] = vert.tex_coord[0];
    outp[GFX_VERT_TEX_COORD_OFFSET + 1] = vert.tex_coord[1];
}

static inline void pvr2_ta_push_gfx_il(struct gfx_il_inst inst) {
    if (gfx_il_inst_buf_count >= PVR2_GFX_IL_INST_BUF_LEN)
        RAISE_ERROR(ERROR_OVERFLOW);

    struct gfx_il_inst_chain *ent = gfx_il_inst_buf + gfx_il_inst_buf_count++;
    ent->next = NULL;
    ent->cmd = inst;
    if (!disp_list_begin[poly_state.current_list])
        disp_list_begin[poly_state.current_list] = ent;
    if (disp_list_end[poly_state.current_list])
        disp_list_end[poly_state.current_list]->next = ent;
    disp_list_end[poly_state.current_list] = ent;
}

uint32_t pvr2_ta_fifo_poly_read_32(addr32_t addr) {
#ifdef PVR2_LOG_VERBOSE
    LOG_DBG(stderr, "WARNING: trying to read 4 bytes from the TA polygon FIFO "
            "(you get all 0s)\n");
#endif
    return 0;
}

void pvr2_ta_fifo_poly_write_32(addr32_t addr, uint32_t val) {
#ifdef PVR2_LOG_VERBOSE
    LOG_DBG("WARNING: writing 4 bytes to TA polygon FIFO: 0x%08x\n",
            (unsigned)val);
#endif
    uint8_t bytes[4] = {
        val & 0xff,
        (val >> 8) & 0xff,
        (val >> 16) & 0xff,
        (val >> 24) & 0xff
    };
    input_poly_fifo(bytes[0]);
    input_poly_fifo(bytes[1]);
    input_poly_fifo(bytes[2]);
    input_poly_fifo(bytes[3]);
}

uint16_t pvr2_ta_fifo_poly_read_16(addr32_t addr) {
#ifdef PVR2_LOG_VERBOSE
    LOG_DBG(stderr, "WARNING: trying to read 2 bytes from the TA polygon FIFO "
            "(you get all 0s)\n");
#endif
    return 0;
}

void pvr2_ta_fifo_poly_write_16(addr32_t addr, uint16_t val) {
#ifdef PVR2_LOG_VERBOSE
    LOG_DBG("WARNING: writing 2 bytes to TA polygon FIFO: 0x%04x\n",
            (unsigned)val);
#endif
    uint8_t bytes[2] = {
        val & 0xff,
        (val >> 8) & 0xff,
    };
    input_poly_fifo(bytes[0]);
    input_poly_fifo(bytes[1]);
}


uint8_t pvr2_ta_fifo_poly_read_8(addr32_t addr) {
#ifdef PVR2_LOG_VERBOSE
    LOG_DBG(stderr, "WARNING: trying to read 1 byte from the TA polygon FIFO "
            "(you get all 0s)\n");
#endif
    return 0;
}

void pvr2_ta_fifo_poly_write_8(addr32_t addr, uint8_t val) {
#ifdef PVR2_LOG_VERBOSE
    LOG_DBG("WARNING: writing 1 byte to TA polygon FIFO: 0x%02x\n",
            (unsigned)val);
#endif
    input_poly_fifo(val);
}

float pvr2_ta_fifo_poly_read_float(addr32_t addr) {
    uint32_t tmp = pvr2_ta_fifo_poly_read_32(addr);
    float ret;
    memcpy(&ret, &tmp, sizeof(ret));
    return ret;
}

void pvr2_ta_fifo_poly_write_float(addr32_t addr, float val) {
    uint32_t tmp;
    memcpy(&tmp, &val, sizeof(tmp));
    pvr2_ta_fifo_poly_write_32(addr, tmp);
}

double pvr2_ta_fifo_poly_read_double(addr32_t addr) {
    error_set_length(8);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

void pvr2_ta_fifo_poly_write_double(addr32_t addr, double val) {
    error_set_length(8);
    error_set_address(addr);
    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void input_poly_fifo(uint8_t byte) {
    ta_fifo[ta_fifo_byte_count++] = byte;

    if (!(ta_fifo_byte_count % 32)) {
        on_packet_received();
    }
}

static void on_packet_received(void) {
    uint32_t const *ta_fifo32 = (uint32_t const*)ta_fifo;
    unsigned cmd_tp = (ta_fifo32[0] & TA_CMD_TYPE_MASK) >> TA_CMD_TYPE_SHIFT;

    switch(cmd_tp) {
    case TA_CMD_TYPE_VERTEX:
        /*
         * Crazyy Taxi seems to send headers for all 8 possible polygon lists
         * even though only 5 lists actually exist.  It never submits vertex
         * data for the three which don't actually exist.  Here we panic if it
         * does try to send vertex data to one of those three lists.
         */
        if (unlikely(poly_state.current_list < DISPLAY_LIST_FIRST ||
                     poly_state.current_list > DISPLAY_LIST_LAST)) {
            error_set_feature("unknown display list type");
            error_set_display_list_index(poly_state.current_list);
            error_set_ta_fifo_byte_count(ta_fifo_byte_count);
            error_set_ta_fifo_word_0(ta_fifo32[0]);
            error_set_ta_fifo_word_1(ta_fifo32[1]);
            error_set_ta_fifo_word_2(ta_fifo32[2]);
            error_set_ta_fifo_word_3(ta_fifo32[3]);
            error_set_ta_fifo_word_4(ta_fifo32[4]);
            error_set_ta_fifo_word_5(ta_fifo32[5]);
            error_set_ta_fifo_word_6(ta_fifo32[6]);
            error_set_ta_fifo_word_7(ta_fifo32[7]);
            error_set_ta_fifo_word_8(ta_fifo32[8]);
            error_set_ta_fifo_word_9(ta_fifo32[9]);
            error_set_ta_fifo_word_a(ta_fifo32[10]);
            error_set_ta_fifo_word_b(ta_fifo32[11]);
            error_set_ta_fifo_word_c(ta_fifo32[12]);
            error_set_ta_fifo_word_d(ta_fifo32[13]);
            error_set_ta_fifo_word_e(ta_fifo32[14]);
            error_set_ta_fifo_word_f(ta_fifo32[15]);
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }
        if (poly_state.global_param == GLOBAL_PARAM_POLY) {
            on_vertex_received();
        } else if (poly_state.global_param == GLOBAL_PARAM_SPRITE) {
            on_sprite_received();
        } else {
            error_set_feature("some unknown vertex type");
            error_set_pvr2_global_param(poly_state.global_param);
            RAISE_ERROR(ERROR_UNIMPLEMENTED);
        }
        break;
    case TA_CMD_TYPE_POLY_HDR:
    case TA_CMD_TYPE_SPRITE_HDR:
        on_polyhdr_received();
        break;
    case TA_CMD_TYPE_END_OF_LIST:
        on_end_of_list_received();
        break;
    case TA_CMD_TYPE_USER_CLIP:
        on_user_clip_received();
        break;
    case TA_CMD_TYPE_INPUT_LIST:
        // I only semi-understand what this is
        LOG_DBG("TA_CMD_TYPE_INPUT_LIST received on pvr2 ta fifo!\n");
        ta_fifo_finish_packet();
        break;
    case TA_CMD_TYPE_UNKNOWN:
        LOG_DBG("WARNING: TA_CMD_TYPE_UNKNOWN received on pvr2 ta fifo!\n");
        ta_fifo_finish_packet();
        break;
    default:
        LOG_ERROR("UNKNOWN CMD TYPE 0x%x\n", cmd_tp);
        error_set_feature("PVR2 command type");
        error_set_ta_fifo_cmd(cmd_tp);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }
}

static void decode_poly_hdr(struct poly_hdr *hdr) {
    uint32_t const *ta_fifo32 = (uint32_t const*)ta_fifo;

    hdr->tex_enable = (bool)(ta_fifo32[0] & TA_CMD_TEX_ENABLE_MASK);
    hdr->ta_color_fmt = (ta_fifo32[0] & TA_COLOR_FMT_MASK) >>
        TA_COLOR_FMT_SHIFT;

    hdr->tex_fmt = (ta_fifo32[3] & TEX_CTRL_PIX_FMT_MASK) >>
        TEX_CTRL_PIX_FMT_SHIFT;

    /* if (hdr->tex_fmt == TEX_CTRL_PIX_FMT_BUMP_MAP || */
    /*     hdr->tex_fmt == TEX_CTRL_PIX_FMT_4_BPP_PAL || */
    /*     hdr->tex_fmt == TEX_CTRL_PIX_FMT_YUV_422 || */
    /*     hdr->tex_fmt >= TEX_CTRL_PIX_FMT_INVALID) */
    if (hdr->tex_fmt == TEX_CTRL_PIX_FMT_YUV_422)
        RAISE_ERROR(ERROR_UNIMPLEMENTED);

    hdr->tex_width_shift = 3 +
        ((ta_fifo32[2] & TSP_TEX_WIDTH_MASK) >> TSP_TEX_WIDTH_SHIFT);
    hdr->tex_height_shift = 3 +
        ((ta_fifo32[2] & TSP_TEX_HEIGHT_MASK) >> TSP_TEX_HEIGHT_SHIFT);
    hdr->tex_inst = (ta_fifo32[2] & TSP_TEX_INST_MASK) >>
        TSP_TEX_INST_SHIFT;

    if (hdr->tex_fmt != TEX_CTRL_PIX_FMT_4_BPP_PAL &&
        hdr->tex_fmt != TEX_CTRL_PIX_FMT_8_BPP_PAL) {
        hdr->tex_twiddle = !(bool)(TEX_CTRL_NOT_TWIDDLED_MASK & ta_fifo32[3]);
        if (!hdr->tex_twiddle)
            hdr->stride_sel = (bool)(TEX_CTRL_STRIDE_SEL_MASK & ta_fifo32[3]);
        else
            hdr->stride_sel = false;
        hdr->tex_palette_start = 0xdeadbeef;
    } else {
        hdr->tex_twiddle = true;
        hdr->stride_sel = false;
        hdr->tex_palette_start = (ta_fifo32[3] & TEX_CTRL_PALETTE_START_MASK) >>
            TEX_CTRL_PALETTE_START_SHIFT;
    }

    hdr->tex_vq_compression = (bool)(TEX_CTRL_VQ_MASK & ta_fifo32[3]);
    hdr->tex_mipmap = (bool)(TEX_CTRL_MIP_MAPPED_MASK & ta_fifo32[3]);
    hdr->tex_addr = ((ta_fifo32[3] & TEX_CTRL_TEX_ADDR_MASK) >>
                     TEX_CTRL_TEX_ADDR_SHIFT) << 3;
    hdr->tex_filter = (ta_fifo32[2] & TSP_TEX_INST_FILTER_MASK) >>
        TSP_TEX_INST_FILTER_SHIFT;

    hdr->src_blend_factor =
        (ta_fifo32[2] & TSP_WORD_SRC_ALPHA_FACTOR_MASK) >>
        TSP_WORD_SRC_ALPHA_FACTOR_SHIFT;
    hdr->dst_blend_factor =
        (ta_fifo32[2] & TSP_WORD_DST_ALPHA_FACTOR_MASK) >>
        TSP_WORD_DST_ALPHA_FACTOR_SHIFT;

    if (ta_fifo32[2] & (2 << TSP_TEX_CLAMP_SHIFT))
        hdr->tex_wrap_mode[0] = TEX_WRAP_CLAMP;
    else if (ta_fifo32[2] & (2 << TSP_TEX_FLIP_SHIFT))
        hdr->tex_wrap_mode[0] = TEX_WRAP_FLIP;
    else
        hdr->tex_wrap_mode[0] = TEX_WRAP_REPEAT;

    if (ta_fifo32[2] & (1 << TSP_TEX_CLAMP_SHIFT))
        hdr->tex_wrap_mode[1] = TEX_WRAP_CLAMP;
    else if (ta_fifo32[2] & (1 << TSP_TEX_FLIP_SHIFT))
        hdr->tex_wrap_mode[1] = TEX_WRAP_FLIP;
    else
        hdr->tex_wrap_mode[1] = TEX_WRAP_REPEAT;

    hdr->enable_depth_writes =
        !((ta_fifo32[0] & DEPTH_WRITE_DISABLE_MASK) >>
          DEPTH_WRITE_DISABLE_SHIFT);
    hdr->depth_func =
        (ta_fifo32[0] & DEPTH_FUNC_MASK) >> DEPTH_FUNC_SHIFT;

    hdr->shadow = (bool)(ta_fifo32[0] & TA_CMD_SHADOW_MASK);
    hdr->two_volumes_mode = (bool)(ta_fifo32[0] & TA_CMD_TWO_VOLUMES_MASK);
    hdr->color_type =
        (enum ta_color_type)((ta_fifo32[0] & TA_CMD_COLOR_TYPE_MASK) >>
                             TA_CMD_COLOR_TYPE_SHIFT);
    hdr->offset_color_enable = (bool)(ta_fifo32[0] & TA_CMD_OFFSET_COLOR_MASK);
    hdr->gourad_shading_enable =
        (bool)(ta_fifo32[0] & TA_CMD_GOURAD_SHADING_MASK);
    hdr->tex_coord_16_bit_enable =
        (bool)(ta_fifo32[0] & TA_CMD_16_BIT_TEX_COORD_MASK);

    if (((ta_fifo32[0] & TA_CMD_TYPE_MASK) >> TA_CMD_TYPE_SHIFT) ==
        TA_CMD_TYPE_SPRITE_HDR) {
        hdr->tex_coord_16_bit_enable = true; // force this on
    }

    // unpack the sprite color
    uint32_t base_color = ta_fifo32[4];
    uint32_t offset_color = ta_fifo32[5];
    unsigned base_r = (base_color & 0x00ff0000) >> 16;
    unsigned base_g = (base_color & 0x0000ff00) >> 8;
    unsigned base_b = base_color & 0x000000ff;
    unsigned base_a = (base_color & 0xff000000) >> 24;
    unsigned offset_r = (offset_color & 0x00ff0000) >> 16;
    unsigned offset_g = (offset_color & 0x0000ff00) >> 8;
    unsigned offset_b = offset_color & 0x000000ff;
    unsigned offset_a = (offset_color & 0xff000000) >> 24;

    hdr->sprite_base_color_rgba[0] = base_r / 255.0f;
    hdr->sprite_base_color_rgba[1] = base_g / 255.0f;
    hdr->sprite_base_color_rgba[2] = base_b / 255.0f;
    hdr->sprite_base_color_rgba[3] = base_a / 255.0f;

    if (hdr->offset_color_enable) {
        hdr->sprite_offs_color_rgba[0] = offset_r / 255.0f;
        hdr->sprite_offs_color_rgba[1] = offset_g / 255.0f;
        hdr->sprite_offs_color_rgba[2] = offset_b / 255.0f;
        hdr->sprite_offs_color_rgba[3] = offset_a / 255.0f;
    } else {
        memset(hdr->sprite_offs_color_rgba, 0,
               sizeof(hdr->sprite_offs_color_rgba));
    }

    if (hdr->color_type == TA_COLOR_TYPE_INTENSITY_MODE_1) {
        if (hdr->offset_color_enable) {
            memcpy(hdr->poly_base_color_rgba, ta_fifo32 + 9, 3 * sizeof(float));
            memcpy(hdr->poly_base_color_rgba + 3, ta_fifo32 + 8, sizeof(float));
            memcpy(hdr->poly_offs_color_rgba, ta_fifo32 + 13, 3 * sizeof(float));
            memcpy(hdr->poly_offs_color_rgba + 3, ta_fifo32 + 12, sizeof(float));
        } else {
            memcpy(hdr->poly_base_color_rgba, ta_fifo32 + 5, 3 * sizeof(float));
            memcpy(hdr->poly_base_color_rgba + 3, ta_fifo32 + 4, sizeof(float));
            memset(hdr->poly_offs_color_rgba, 0, sizeof(float) * 4);
        }
    } else {
        memset(hdr->poly_base_color_rgba, 0, sizeof(hdr->poly_base_color_rgba));
        memset(hdr->poly_offs_color_rgba, 0, sizeof(hdr->poly_offs_color_rgba));
    }
}

static void on_polyhdr_received(void) {
    uint32_t const *ta_fifo32 = (uint32_t const*)ta_fifo;
    enum display_list_type list =
        (enum display_list_type)((ta_fifo32[0] & TA_CMD_DISP_LIST_MASK) >>
                                 TA_CMD_DISP_LIST_SHIFT);
    struct poly_hdr hdr;

    if (list >= DISPLAY_LIST_COUNT || list < 0) {
        error_set_feature("correct response for invalid display list indices");
        error_set_display_list_index(list);
        error_set_ta_fifo_byte_count(ta_fifo_byte_count);
        error_set_ta_fifo_word_0(ta_fifo32[0]);
        error_set_ta_fifo_word_1(ta_fifo32[1]);
        error_set_ta_fifo_word_2(ta_fifo32[2]);
        error_set_ta_fifo_word_3(ta_fifo32[3]);
        error_set_ta_fifo_word_4(ta_fifo32[4]);
        error_set_ta_fifo_word_5(ta_fifo32[5]);
        error_set_ta_fifo_word_6(ta_fifo32[6]);
        error_set_ta_fifo_word_7(ta_fifo32[7]);
        error_set_ta_fifo_word_8(ta_fifo32[8]);
        error_set_ta_fifo_word_9(ta_fifo32[9]);
        error_set_ta_fifo_word_a(ta_fifo32[10]);
        error_set_ta_fifo_word_b(ta_fifo32[11]);
        error_set_ta_fifo_word_c(ta_fifo32[12]);
        error_set_ta_fifo_word_d(ta_fifo32[13]);
        error_set_ta_fifo_word_e(ta_fifo32[14]);
        error_set_ta_fifo_word_f(ta_fifo32[15]);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    decode_poly_hdr(&hdr);

    /*
     * XXX It seems that intensity mode 1 is 64 bits, but mode 2 is only 32.
     * This is most likely because the point of intensity mode 2 is to reuse
     * the face color from the previous intensity mode 1 polygon.  I'm not 100%
     * clear on what the format of an intensity mode 2 header is, and I'm also
     * not 100% clear on whether or not it has its own offset header.  That
     * said, I am confident that intensity mode 2 is 32 bits.
     */
    if (hdr.color_type == TA_COLOR_TYPE_INTENSITY_MODE_1 &&
        hdr.offset_color_enable && ta_fifo_byte_count != 64) {
        // need 64 bytes not, 32.
        return;
    }

    if ((poly_state.current_list == DISPLAY_LIST_NONE) &&
        list_submitted[list]) {
        LOG_WARN("WARNING: unable to open list %s because it is already "
                 "closed\n", display_list_names[list]);
        goto the_end;
    }

    if ((poly_state.current_list != DISPLAY_LIST_NONE) &&
        (poly_state.current_list != list)) {
        LOG_WARN("WARNING: attempting to input poly header for list %s without "
                 "first closing %s\n", display_list_names[list],
                 display_list_names[poly_state.current_list]);
        goto the_end;
    }

    /*
     * next_poly_group will finish the current poly_group (if there is one),
     * and that will reference the poly_state.  Ergo, next_poly_group must be
     * called BEFORE any poly_state changes are made.
     */
    if (poly_state.current_list != DISPLAY_LIST_NONE &&
        poly_state.current_list != list) {
        // finish the last poly group of the current list

#ifdef INVARIANTS
        if (poly_state.current_list < 0 || poly_state.current_list >= DISPLAY_LIST_COUNT) {
            LOG_ERROR("ERROR: poly_state.current_list is 0x%08x\n",
                   (unsigned)poly_state.current_list);
            RAISE_ERROR(ERROR_INTEGRITY);
        }
#endif

        if (open_group)
            finish_poly_group(poly_state.current_list);
    }

    if ((poly_state.current_list != list) &&
        !list_submitted[list]) {

        LOG_DBG("Opening display list %s\n", display_list_names[list]);
        poly_state.current_list = list;
        list_submitted[list] = true;
    }

    next_poly_group(poly_state.current_list);

    // reset triangle strips
    poly_state.strip_len = 0;

    poly_state.ta_color_fmt = hdr.ta_color_fmt;

    if (hdr.tex_enable) {
        poly_state.tex_enable = true;
        LOG_DBG("texture enabled\n");

        LOG_DBG("the texture format is %d\n", hdr.tex_fmt);
        LOG_DBG("The texture address ix 0x%08x\n", hdr.tex_addr);

        if (hdr.tex_twiddle)
            LOG_DBG("not twiddled\n");
        else
            LOG_DBG("twiddled\n");

        struct pvr2_tex *ent =
            pvr2_tex_cache_find(hdr.tex_addr, hdr.tex_palette_start,
                                hdr.tex_width_shift,
                                hdr.tex_height_shift,
                                hdr.tex_fmt, hdr.tex_twiddle,
                                hdr.tex_vq_compression,
                                hdr.tex_mipmap,
                                hdr.stride_sel);

        LOG_DBG("texture dimensions are (%u, %u)\n",
               1 << hdr.tex_width_shift,
               1 << hdr.tex_height_shift);
        if (ent) {
            LOG_DBG("Texture 0x%08x found in cache\n",
                    hdr.tex_addr);
        } else {
            LOG_DBG("Adding 0x%08x to texture cache...\n",
                    hdr.tex_addr);
            ent = pvr2_tex_cache_add(hdr.tex_addr, hdr.tex_palette_start,
                                     hdr.tex_width_shift,
                                     hdr.tex_height_shift,
                                     hdr.tex_fmt,
                                     hdr.tex_twiddle,
                                     hdr.tex_vq_compression,
                                     hdr.tex_mipmap,
                                     hdr.stride_sel);
        }

        if (!ent) {
            LOG_WARN("WARNING: failed to add texture 0x%08x to "
                    "the texture cache\n", hdr.tex_addr);
            poly_state.tex_enable = false;
        } else {
            poly_state.tex_idx = pvr2_tex_cache_get_idx(ent);
        }
    } else {
        LOG_DBG("textures are NOT enabled\n");
        poly_state.tex_enable = false;
    }
    poly_state.src_blend_factor = hdr.src_blend_factor;
    poly_state.dst_blend_factor = hdr.dst_blend_factor;
    memcpy(poly_state.tex_wrap_mode, hdr.tex_wrap_mode,
           sizeof(poly_state.tex_wrap_mode));

    poly_state.enable_depth_writes = hdr.enable_depth_writes;
    poly_state.depth_func = hdr.depth_func;

    poly_state.shadow = hdr.shadow;
    poly_state.two_volumes_mode = hdr.two_volumes_mode;
    poly_state.color_type = hdr.color_type;
    poly_state.offset_color_enable = hdr.offset_color_enable;
    poly_state.gourad_shading_enable = hdr.gourad_shading_enable;
    poly_state.tex_coord_16_bit_enable = hdr.tex_coord_16_bit_enable;

    poly_state.vert_type = classify_vert();
    poly_state.vert_len = vert_lengths[poly_state.vert_type];

    poly_state.tex_inst = hdr.tex_inst;
    poly_state.tex_filter = hdr.tex_filter;

    if (hdr.color_type == TA_COLOR_TYPE_INTENSITY_MODE_1) {
        memcpy(poly_state.poly_base_color_rgba, hdr.poly_base_color_rgba,
               sizeof(poly_state.poly_base_color_rgba));
        memcpy(poly_state.poly_offs_color_rgba, hdr.poly_offs_color_rgba,
               sizeof(poly_state.poly_offs_color_rgba));
    }

    memcpy(poly_state.sprite_base_color_rgba, hdr.sprite_base_color_rgba,
           sizeof(poly_state.sprite_base_color_rgba));
    memcpy(poly_state.sprite_offs_color_rgba, hdr.sprite_offs_color_rgba,
           sizeof(poly_state.sprite_offs_color_rgba));

    poly_state.global_param =
        (enum global_param)((ta_fifo32[0] & TA_CMD_TYPE_MASK) >>
                            TA_CMD_TYPE_SHIFT);

    LOG_DBG("POLY HEADER PACKET!\n");

the_end:
    ta_fifo_finish_packet();
}

// unpack 16-bit texture coordinates into two floats
static void unpack_uv16(float *u_coord, float *v_coord, void const *input) {
    uint32_t val = *(uint32_t*)input;
    uint32_t u_val = val & 0xffff0000;
    uint32_t v_val = val << 16;

    memcpy(u_coord, &u_val, sizeof(*u_coord));
    memcpy(v_coord, &v_val, sizeof(*v_coord));
}

static void on_sprite_received(void) {
    /*
     * if the vertex is not long enough, return and make input_poly_fifo call
     * us again later when there is more data.  Practically, this means that we
     * are expecting 64 bytes, but we only have 32 bytes so far.
     */
    if (ta_fifo_byte_count != 64)
        return;

    if (poly_state.current_list < 0) {
        LOG_WARN("WARNING: unable to render sprite because no display lists "
                 "are open\n");
        ta_fifo_finish_packet();
        return;
    }

    if (!open_group) {
        LOG_WARN("WARNING: unable to render sprite because I'm still waiting "
                 "to see a polygon header\n");
        ta_fifo_finish_packet();
        return;
    }

    float const *ta_fifo_float = (float const*)ta_fifo;

    /*
     * four quadrilateral vertices.  the z-coordinate of p4 is determined
     * automatically by the PVR2 so it is not possible to specify a non-coplanar
     * set of vertices.
     */
    float p1[3] = { ta_fifo_float[1], ta_fifo_float[2], 1.0 / ta_fifo_float[3] };
    float p2[3] = { ta_fifo_float[4], ta_fifo_float[5], 1.0 / ta_fifo_float[6] };
    float p3[3] = { ta_fifo_float[7], ta_fifo_float[8], 1.0 / ta_fifo_float[9] };
    float p4[3] = { ta_fifo_float[10], ta_fifo_float[11] };

    /*
     * unpack the texture coordinates.  The third vertex's coordinate is the
     * scond vertex's coordinate plus the two side-vectors.  We do this
     * unconditionally even if textures are disabled.  If textures are disabled
     * then the output of this texture-coordinate algorithm is undefined but it
     * does not matter because the rendering code won't be using it anyways.
     */
    float uv[4][2];

    unpack_uv16(uv[0], uv[0] + 1, ta_fifo_float + 13);
    unpack_uv16(uv[1], uv[1] + 1, ta_fifo_float + 14);
    unpack_uv16(uv[2], uv[2] + 1, ta_fifo_float + 15);

    float uv_vec[2][2] = {
        { uv[0][0] - uv[1][0], uv[0][1] - uv[1][1] },
        { uv[2][0] - uv[1][0], uv[2][1] - uv[1][1] }
    };

    uv[3][0] = uv[1][0] + uv_vec[0][0] + uv_vec[1][0];
    uv[3][1] = uv[1][1] + uv_vec[0][1] + uv_vec[1][1];

    /*
     * any three non-colinear points will define a 2-dimensional hyperplane in
     * 3-dimensionl space.  The hyperplane consists of all points where the
     * following relationship is true:
     *
     * dot(n, p) + d == 0
     *
     * where n is a vector orthogonal to the hyperplane, d is the translation
     * from the origin to the hyperplane along n, and p is any point on the
     * plane.
     *
     * n is usually a normalized vector, but for our purposes that is not
     * necessary because d will scale accordingly.
     *
     * If the magnitude of n is zero, then all three points are colinear (or
     * coincidental) and they do not define a single hyperplane because there
     * are inifinite hyperplanes which contain all three points.  In this case
     * the quadrilateral is considered degenerate and should not be rendered.
     *
     * Because the three existing vertices are coplanar, the fourth vertex's
     * z-coordinate can be determined based on the hyperplane defined by the
     * other three points.
     *
     * dot(n, p) + d == 0
     * n.x * p.x + n.y * p.y + n.z * p.z + d == 0
     * n.z * p.z = -(d + n.x * p.x + n.y * p.y)
     * p.z = -(d + n.x * p.x + n.y * p.y) / n.z
     *
     * In the case where n.z is 0, the hyperplane is oriented orthogonally with
     * respect to the observer.  The only dimension on which the quadrilateral
     * is visible is the one which is infinitely thin, so it should not be
     * rendered.
     */

    // side-vectors
    float v1[3] = { p2[0] - p1[0], p2[1] - p1[1], p2[2] - p1[2] };
    float v2[3] = { p3[0] - p1[0], p3[1] - p1[1], p3[2] - p1[2] };

    // hyperplane normal
    float norm[3] = {
        v1[1] * v2[2] - v1[2] * v2[1],
        v1[2] * v2[0] - v1[0] * v2[2],
        v1[0] * v2[1] - v1[1] * v2[0]
    };

    /*
     * return early if the quad is degenerate or it is oriented orthoganally to
     * the viewer.
     *
     * TODO: consider using a floating-point tolerance instead of comparing to
     * zero directly.
     */
    if ((norm[2] == 0.0f) ||
        (norm[0] * norm[0] + norm[1] * norm[1] + norm[2] * norm[2] == 0.0f)) {
        ta_fifo_finish_packet();
        return;
    }

    // hyperplane translation
    float dist = -norm[0] * p1[0] - norm[1] * p1[1] - norm[2] * p1[2];

    p4[2] = -1.0f * (dist + norm[0] * p4[0] + norm[1] * p4[1]) / norm[2];

    float const base_col[] = {
        poly_state.sprite_base_color_rgba[0],
        poly_state.sprite_base_color_rgba[1],
        poly_state.sprite_base_color_rgba[2],
        poly_state.sprite_base_color_rgba[3]
    };

    float const offs_col[] = {
        poly_state.sprite_offs_color_rgba[0],
        poly_state.sprite_offs_color_rgba[1],
        poly_state.sprite_offs_color_rgba[2],
        poly_state.sprite_offs_color_rgba[3]
    };

    struct pvr2_ta_vert vert1 = {
        .pos = { p1[0], p1[1], p1[2] },
        .base_color = { base_col[0], base_col[1], base_col[2], base_col[3] },
        .offs_color = { offs_col[0], offs_col[1], offs_col[2], offs_col[3] },
        .tex_coord = { uv[0][0], uv[0][1] }
    };

    struct pvr2_ta_vert vert2 = {
        .pos = { p2[0], p2[1], p2[2] },
        .base_color = { base_col[0], base_col[1], base_col[2], base_col[3] },
        .offs_color = { offs_col[0], offs_col[1], offs_col[2], offs_col[3] },
        .tex_coord = { uv[1][0], uv[1][1] }
    };

    struct pvr2_ta_vert vert3 = {
        .pos = { p3[0], p3[1], p3[2] },
        .base_color = { base_col[0], base_col[1], base_col[2], base_col[3] },
        .offs_color = { offs_col[0], offs_col[1], offs_col[2], offs_col[3] },
        .tex_coord = { uv[2][0], uv[2][1] }
    };

    struct pvr2_ta_vert vert4 = {
        .pos = { p4[0], p4[1], p4[2] },
        .base_color = { base_col[0], base_col[1], base_col[2], base_col[3] },
        .offs_color = { offs_col[0], offs_col[1], offs_col[2], offs_col[3] },
        .tex_coord = { uv[3][0], uv[3][1] }
    };

    pvr2_ta_push_vert(vert1);
    pvr2_ta_push_vert(vert2);
    pvr2_ta_push_vert(vert3);

    pvr2_ta_push_vert(vert1);
    pvr2_ta_push_vert(vert3);
    pvr2_ta_push_vert(vert4);

    if (p1[2] < clip_min)
        clip_min = p1[2];
    if (p1[2] > clip_max)
        clip_max = p1[2];

    if (p2[2] < clip_min)
        clip_min = p2[2];
    if (p2[2] > clip_max)
        clip_max = p2[2];

    if (p3[2] < clip_min)
        clip_min = p3[2];
    if (p3[2] > clip_max)
        clip_max = p3[2];

    if (p4[2] < clip_min)
        clip_min = p4[2];
    if (p4[2] > clip_max)
        clip_max = p4[2];

    ta_fifo_finish_packet();
}

static void on_vertex_received(void) {
    uint32_t const *ta_fifo32 = (uint32_t const*)ta_fifo;
    float const *ta_fifo_float = (float const*)ta_fifo;

    /*
     * if the vertex is not long enough, return and make input_poly_fifo call
     * us again later when there is more data.  Practically, this means that we
     * are expecting 64 bytes, but we only have 32 bytes so far.
     */
    if (ta_fifo_byte_count != (poly_state.vert_len * 4))
        return;

#ifdef PVR2_LOG_VERBOSE
    LOG_DBG("vertex received!\n");
#endif
    if (poly_state.current_list < 0) {
        LOG_WARN("WARNING: unable to render vertex because no display lists "
                 "are open\n");
        ta_fifo_finish_packet();
        return;
    }

    if (!open_group) {
        LOG_WARN("ERROR: unable to render vertex because I'm still waiting to "
                 "see a polygon header\n");
        ta_fifo_finish_packet();
        return;
    }

    /*
     * un-strip triangle strips by duplicating the previous two vertices.
     *
     * TODO: obviously it would be best to preserve the triangle strips and
     * send them to OpenGL via GL_TRIANGLE_STRIP in the rendering backend, but
     * then I need to come up with some way to signal the renderer to stop and
     * re-start strips.  It might also be possible to stitch separate strips
     * together with degenerate triangles...
     */
    if (poly_state.strip_len >= 3) {
        pvr2_ta_push_vert(poly_state.strip_vert_1);
        pvr2_ta_push_vert(poly_state.strip_vert_2);
    }

    // first update the clipping planes
    /*
     * TODO: there are FPU instructions on x86 that can do this without
     * branching
     */
    float z_recip = 1.0 / ta_fifo_float[3];
    if (z_recip < clip_min)
        clip_min = z_recip;
    if (z_recip > clip_max)
        clip_max = z_recip;

    struct pvr2_ta_vert vert;

    vert.pos[0] = ta_fifo_float[1];
    vert.pos[1] = ta_fifo_float[2];
    vert.pos[2] = z_recip;

    if (poly_state.tex_enable) {
        float uv[2];

        if (poly_state.tex_coord_16_bit_enable) {
            unpack_uv16(uv, uv + 1, ta_fifo_float + 4);
        } else {
            uv[0] = ta_fifo_float[4];
            uv[1] = ta_fifo_float[5];
        }

        vert.tex_coord[0] = uv[0];
        vert.tex_coord[1] = uv[1];
    }

    float base_color_r, base_color_g, base_color_b, base_color_a;
    float offs_color_r = 0.0f, offs_color_g = 0.0f,
        offs_color_b = 0.0f, offs_color_a = 0.0f;
    float base_intensity, offs_intensity;

    switch (poly_state.ta_color_fmt) {
    case TA_COLOR_TYPE_PACKED:
        base_color_a = (float)((ta_fifo32[6] & 0xff000000) >> 24) / 255.0f;
        base_color_r = (float)((ta_fifo32[6] & 0x00ff0000) >> 16) / 255.0f;
        base_color_g = (float)((ta_fifo32[6] & 0x0000ff00) >> 8) / 255.0f;
        base_color_b = (float)((ta_fifo32[6] & 0x000000ff) >> 0) / 255.0f;
        break;
    case TA_COLOR_TYPE_FLOAT:
        memcpy(&base_color_a, ta_fifo32 + 4, sizeof(base_color_a));
        memcpy(&base_color_r, ta_fifo32 + 5, sizeof(base_color_r));
        memcpy(&base_color_g, ta_fifo32 + 6, sizeof(base_color_g));
        memcpy(&base_color_b, ta_fifo32 + 7, sizeof(base_color_b));
        break;
    case TA_COLOR_TYPE_INTENSITY_MODE_1:
    case TA_COLOR_TYPE_INTENSITY_MODE_2:
        base_color_a = poly_state.poly_base_color_rgba[3];

        memcpy(&base_intensity, ta_fifo32 + 6, sizeof(float));
        memcpy(&offs_intensity, ta_fifo32 + 7, sizeof(float));

        base_color_r = base_intensity * poly_state.poly_base_color_rgba[0];
        base_color_g = base_intensity * poly_state.poly_base_color_rgba[1];
        base_color_b = base_intensity * poly_state.poly_base_color_rgba[2];

        if (poly_state.offset_color_enable) {
            offs_color_r =
                offs_intensity * poly_state.poly_offs_color_rgba[0];
            offs_color_g =
                offs_intensity * poly_state.poly_offs_color_rgba[1];
            offs_color_b =
                offs_intensity * poly_state.poly_offs_color_rgba[2];
            offs_color_a =
                offs_intensity * poly_state.poly_offs_color_rgba[3];
        }
        break;
    default:
        base_color_r = base_color_g = base_color_b = base_color_a = 1.0f;
        LOG_WARN("WARNING: unknown TA color format %u\n",
                 poly_state.ta_color_fmt);
    }

    vert.base_color[0] = base_color_r;
    vert.base_color[1] = base_color_g;
    vert.base_color[2] = base_color_b;
    vert.base_color[3] = base_color_a;

    vert.offs_color[0] = offs_color_r;
    vert.offs_color[1] = offs_color_g;
    vert.offs_color[2] = offs_color_b;
    vert.offs_color[3] = offs_color_a;

    pvr2_ta_push_vert(vert);

    if (ta_fifo32[0] & TA_CMD_END_OF_STRIP_MASK) {
        /*
         * TODO: handle degenerate cases where the user sends an
         * end-of-strip on the first or second vertex
         */
        poly_state.strip_len = 0;
    } else {
        /*
         * shift the new vert into strip_vert2 and
         * shift strip_vert2 into strip_vert1
         */
        poly_state.strip_vert_1 = poly_state.strip_vert_2;
        poly_state.strip_vert_2 = vert;
        poly_state.strip_len++;
    }

    ta_fifo_finish_packet();
}

static void
pvr2_op_complete_int_event_handler(struct SchedEvent *event) {
    pvr2_op_complete_int_event_scheduled = false;
    holly_raise_nrm_int(HOLLY_REG_ISTNRM_PVR_OPAQUE_COMPLETE);
}

static void
pvr2_op_mod_complete_int_event_handler(struct SchedEvent *event) {
    pvr2_op_mod_complete_int_event_scheduled = false;
    holly_raise_nrm_int(HOLLY_REG_ISTNRM_PVR_OPAQUE_MOD_COMPLETE);
}

static void
pvr2_trans_complete_int_event_handler(struct SchedEvent *event) {
    pvr2_trans_complete_int_event_scheduled = false;
    holly_raise_nrm_int(HOLLY_REG_ISTNRM_PVR_TRANS_COMPLETE);
}

static void
pvr2_trans_mod_complete_int_event_handler(struct SchedEvent *event) {
    pvr2_trans_mod_complete_int_event_scheduled = false;
    holly_raise_nrm_int(HOLLY_REG_ISTNRM_PVR_TRANS_MOD_COMPLETE);
}

static void
pvr2_pt_complete_int_event_handler(struct SchedEvent *event) {
    pvr2_pt_complete_int_event_scheduled = false;
    holly_raise_nrm_int(HOLLY_NRM_INT_ISTNRM_PVR_PUNCH_THROUGH_COMPLETE);
}

static void on_end_of_list_received(void) {
    LOG_DBG("END-OF-LIST PACKET!\n");

    finish_poly_group(poly_state.current_list);

    if (poly_state.current_list != DISPLAY_LIST_NONE) {
        LOG_DBG("Display list \"%s\" closed\n",
                display_list_names[poly_state.current_list]);
    } else {
        LOG_WARN("Unable to close the current display list because no display "
                 "list has been opened\n");
        goto the_end;
    }

    dc_cycle_stamp_t int_when = dc_cycle_stamp() + PVR2_LIST_COMPLETE_INT_DELAY;
    switch (poly_state.current_list) {
    case DISPLAY_LIST_OPAQUE:
        if (!pvr2_op_complete_int_event_scheduled) {
            pvr2_op_complete_int_event_scheduled = true;
            pvr2_op_complete_int_event.when = int_when;
            sched_event(&pvr2_op_complete_int_event);
        }
        break;
    case DISPLAY_LIST_OPAQUE_MOD:
        if (!pvr2_op_mod_complete_int_event_scheduled) {
            pvr2_op_mod_complete_int_event_scheduled = true;
            pvr2_op_mod_complete_int_event.when = int_when;
            sched_event(&pvr2_op_mod_complete_int_event);
        }
        break;
    case DISPLAY_LIST_TRANS:
        if (!pvr2_trans_complete_int_event_scheduled) {
            pvr2_trans_complete_int_event_scheduled = true;
            pvr2_trans_complete_int_event.when = int_when;
            sched_event(&pvr2_trans_complete_int_event);
        }
        break;
    case DISPLAY_LIST_TRANS_MOD:
        if (!pvr2_trans_mod_complete_int_event_scheduled) {
            pvr2_trans_mod_complete_int_event_scheduled = true;
            pvr2_trans_mod_complete_int_event.when = int_when;
            sched_event(&pvr2_trans_mod_complete_int_event);
        }
        break;
    case DISPLAY_LIST_PUNCH_THROUGH:
        if (!pvr2_pt_complete_int_event_scheduled) {
            pvr2_pt_complete_int_event_scheduled = true;
            pvr2_pt_complete_int_event.when = int_when;
            sched_event(&pvr2_pt_complete_int_event);
        }
        break;
    default:
        /*
         * this can never actually happen because this
         * functionshould have returned early above
         */
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    poly_state.current_list = DISPLAY_LIST_NONE;

the_end:
    ta_fifo_finish_packet();
}

static void on_user_clip_received(void) {
    LOG_WARN("PVR2 WARNING: UNIMPLEMENTED USER TILE CLIP PACKET RECEIVED!\n");

    // TODO: implement tile clipping

    ta_fifo_finish_packet();
}

static void pvr2_render_complete_int_event_handler(struct SchedEvent *event) {
    pvr2_render_complete_int_event_scheduled = false;
    holly_raise_nrm_int(HOLLY_REG_ISTNRM_PVR_RENDER_COMPLETE);
}

void pvr2_ta_startrender(void) {
    LOG_DBG("STARTRENDER requested!\n");

    struct gfx_il_inst cmd;

    unsigned tile_w = get_glob_tile_clip_x() << 5;
    unsigned tile_h = get_glob_tile_clip_y() << 5;
    unsigned x_clip_min = get_fb_x_clip_min();
    unsigned x_clip_max = get_fb_x_clip_max();
    unsigned y_clip_min = get_fb_y_clip_min();
    unsigned y_clip_max = get_fb_y_clip_max();

    unsigned x_min = x_clip_min;
    unsigned y_min = y_clip_min;
    unsigned x_max = tile_w < x_clip_max ? tile_w : x_clip_max;
    unsigned y_max = tile_h < y_clip_max ? tile_h : y_clip_max;
    unsigned width = x_max - x_min + 1;
    unsigned height = y_max - y_min + 1;

    /*
     * backgnd_info points to a structure containing some ISP/TSP parameters
     * and three vertices (potentially including texture coordinate and
     * color data).  These are used to draw a background plane.  isp_backgnd_d
     * contians some sort of depth value which is used in auto-sorting mode (I
     * think?).
     *
     * Obviously, I I don't actually understand how this works, nor do I
     * understand why the vertex coordinates are relevant when it's just going
     * to draw an infinite plane, so I just save the background color from the
     * first vertex in the geo_buf so the renderer can use it to glClear.  I
     * also save the depth value from isp_backgnd_d even though I don't have
     * auto-sorting implemented yet.
     *
     * This hack inspired by MAME's powervr2 code.
     */
    uint32_t backgnd_tag = get_isp_backgnd_t();
    addr32_t backgnd_info_addr = (backgnd_tag & ISP_BACKGND_T_ADDR_MASK) >>
        ISP_BACKGND_T_ADDR_SHIFT;
    uint32_t backgnd_skip = ((ISP_BACKGND_T_SKIP_MASK & backgnd_tag) >>
                             ISP_BACKGND_T_SKIP_SHIFT) + 3;
    uint32_t const *backgnd_info = (uint32_t*)(pvr2_tex32_mem + backgnd_info_addr);

    /* printf("background skip is %d\n", (int)backgnd_skip); */

    /* printf("ISP_BACKGND_D is %f\n", (double)frak); */
    /* printf("ISP_BACKGND_T is 0x%08x\n", (unsigned)backgnd_tag); */

    uint32_t bg_color_src = *(backgnd_info + 3 + 0 * backgnd_skip + 3);

    float bg_color_a = (float)((bg_color_src & 0xff000000) >> 24) / 255.0f;
    float bg_color_r = (float)((bg_color_src & 0x00ff0000) >> 16) / 255.0f;
    float bg_color_g = (float)((bg_color_src & 0x0000ff00) >> 8) / 255.0f;
    float bg_color_b = (float)((bg_color_src & 0x000000ff) >> 0) / 255.0f;
    pvr2_bgcolor[0] = bg_color_r;
    pvr2_bgcolor[1] = bg_color_g;
    pvr2_bgcolor[2] = bg_color_b;
    pvr2_bgcolor[3] = bg_color_a;

    /* uint32_t backgnd_depth_as_int = get_isp_backgnd_d(); */
    /* memcpy(&geo->bgdepth, &backgnd_depth_as_int, sizeof(float)); */

    pvr2_tex_cache_xmit();

    finish_poly_group(poly_state.current_list);

    framebuffer_set_render_target();

    // set up rendering context
    cmd.op = GFX_IL_BEGIN_REND;
    cmd.arg.begin_rend.screen_width = width;
    cmd.arg.begin_rend.screen_height = height;
    rend_exec_il(&cmd, 1);

    cmd.op = GFX_IL_SET_CLIP_RANGE;
    cmd.arg.set_clip_range.clip_min = clip_min;
    cmd.arg.set_clip_range.clip_max = clip_max;
    rend_exec_il(&cmd, 1);

    // initial rendering settings
    cmd.op = GFX_IL_CLEAR;
    cmd.arg.clear.bgcolor[0] = pvr2_bgcolor[0];
    cmd.arg.clear.bgcolor[1] = pvr2_bgcolor[1];
    cmd.arg.clear.bgcolor[2] = pvr2_bgcolor[2];
    cmd.arg.clear.bgcolor[3] = pvr2_bgcolor[3];
    rend_exec_il(&cmd, 1);

    // execute queued gfx_il commands
    enum display_list_type list;
    for (list = DISPLAY_LIST_FIRST; list <= DISPLAY_LIST_LAST; list++) {
        struct gfx_il_inst_chain *chain = disp_list_begin[list];
        while (chain) {
            rend_exec_il(&chain->cmd, 1);
            chain = chain->next;
        }
    }

    // tear down rendering context
    cmd.op = GFX_IL_END_REND;
    rend_exec_il(&cmd, 1);

    next_frame_stamp++;
    render_frame_init();

    // TODO: This irq definitely should not be triggered immediately
    if (!pvr2_render_complete_int_event_scheduled) {
        pvr2_render_complete_int_event_scheduled = true;
        pvr2_render_complete_int_event.when = dc_cycle_stamp() +
            PVR2_RENDER_COMPLETE_INT_DELAY;
        sched_event(&pvr2_render_complete_int_event);
    }
}

void pvr2_ta_reinit(void) {
    memset(list_submitted, 0, sizeof(list_submitted));
}

static void finish_poly_group(enum display_list_type disp_list) {
    struct gfx_il_inst cmd;

    if (disp_list < 0) {
        LOG_DBG("%s - no lists are open\n", __func__);
        return;
    }

    if (!open_group) {
        LOG_WARN("%s - still waiting for a polygon header to be opened!\n",
               __func__);
        return;
    }

    // filter out modifier volumes from being rendered
    if (disp_list == DISPLAY_LIST_OPAQUE_MOD ||
        disp_list == DISPLAY_LIST_TRANS_MOD)
        return;

    cmd.op = GFX_IL_SET_REND_PARAM;
    if (poly_state.tex_enable) {
        LOG_DBG("tex_enable should be true\n");
        cmd.arg.set_rend_param.param.tex_enable = true;
        cmd.arg.set_rend_param.param.tex_idx = poly_state.tex_idx;
    } else {
        LOG_DBG("tex_enable should be false\n");
        cmd.arg.set_rend_param.param.tex_enable = false;
    }

    cmd.arg.set_rend_param.param.src_blend_factor = poly_state.src_blend_factor;
    cmd.arg.set_rend_param.param.dst_blend_factor = poly_state.dst_blend_factor;
    cmd.arg.set_rend_param.param.tex_wrap_mode[0] = poly_state.tex_wrap_mode[0];
    cmd.arg.set_rend_param.param.tex_wrap_mode[1] = poly_state.tex_wrap_mode[1];

    cmd.arg.set_rend_param.param.enable_depth_writes =
        poly_state.enable_depth_writes;
    cmd.arg.set_rend_param.param.depth_func = poly_state.depth_func;

    cmd.arg.set_rend_param.param.tex_inst = poly_state.tex_inst;
    cmd.arg.set_rend_param.param.tex_filter = poly_state.tex_filter;

    // enqueue the configuration command
    pvr2_ta_push_gfx_il(cmd);

    /*
     * this check is a little silly, but I get segfaults sometimes when
     * indexing into src_blend_factors and dst_blend_factors and I don't know
     * why.
     *
     * TODO: this was (hopefully) fixed in commit
     * 92059fe4f1714b914cec75fd2f91e676127d3097 but I am keeping the INVARIANTS
     * test here just in case.  It should be safe to delete after a couple of
     * months have gone by without this INVARIANTS test ever failing.
     */
    if (((unsigned)cmd.arg.set_rend_param.param.src_blend_factor >= PVR2_BLEND_FACTOR_COUNT) ||
        ((unsigned)cmd.arg.set_rend_param.param.dst_blend_factor >= PVR2_BLEND_FACTOR_COUNT)) {
        error_set_src_blend_factor(cmd.arg.set_rend_param.param.src_blend_factor);
        error_set_dst_blend_factor(cmd.arg.set_rend_param.param.dst_blend_factor);
        error_set_display_list_index((unsigned)disp_list);
        /* error_set_geo_buf_group_index(group - list->groups); */
        RAISE_ERROR(ERROR_INTEGRITY);
    }

    // TODO: this only needs to be done once per list, not once per polygon group
    cmd.op = GFX_IL_SET_BLEND_ENABLE;
    cmd.arg.set_blend_enable.do_enable = (disp_list == DISPLAY_LIST_TRANS);
    pvr2_ta_push_gfx_il(cmd);

    cmd.op = GFX_IL_DRAW_ARRAY;
    cmd.arg.draw_array.n_verts = pvr2_ta_vert_buf_count - pvr2_ta_vert_cur_group;
    cmd.arg.draw_array.verts = pvr2_ta_vert_buf + pvr2_ta_vert_cur_group * GFX_VERT_LEN;
    pvr2_ta_push_gfx_il(cmd);

    pvr2_ta_vert_cur_group = pvr2_ta_vert_buf_count;

    open_group = false;
}

static void next_poly_group(enum display_list_type disp_list) {
    if (disp_list < 0) {
        LOG_WARN("%s - no lists are open\n", __func__);
        return;
    }

    if (open_group)
        finish_poly_group(disp_list);
    open_group = true;

    pvr2_ta_vert_cur_group = pvr2_ta_vert_buf_count;
}

static enum vert_type classify_vert(void) {
    if (poly_state.tex_enable) {
        if (poly_state.two_volumes_mode) {
            if (poly_state.tex_coord_16_bit_enable) {
                if (poly_state.color_type == TA_COLOR_TYPE_PACKED)
                    return VERT_TEX_PACKED_COLOR_TWO_VOLUMES_16_BIT_TEX_COORD;
                if ((poly_state.color_type == TA_COLOR_TYPE_INTENSITY_MODE_1) ||
                    (poly_state.color_type == TA_COLOR_TYPE_INTENSITY_MODE_2))
                    return VERT_TEX_INTENSITY_TWO_VOLUMES_16_BIT_TEX_COORD;
            } else {
                if (poly_state.color_type == TA_COLOR_TYPE_PACKED)
                    return VERT_TEX_PACKED_COLOR_TWO_VOLUMES;
                if ((poly_state.color_type == TA_COLOR_TYPE_INTENSITY_MODE_1) ||
                    (poly_state.color_type == TA_COLOR_TYPE_INTENSITY_MODE_2))
                    return VERT_TEX_INTENSITY_TWO_VOLUMES;
            }
        } else {
            if (poly_state.tex_coord_16_bit_enable) {
                if (poly_state.color_type == TA_COLOR_TYPE_PACKED)
                    return VERT_TEX_PACKED_COLOR_16_BIT_TEX_COORD;
                if (poly_state.color_type == TA_COLOR_TYPE_FLOAT)
                    return VERT_TEX_FLOATING_COLOR_16_BIT_TEX_COORD;
                if ((poly_state.color_type == TA_COLOR_TYPE_INTENSITY_MODE_1) ||
                    (poly_state.color_type == TA_COLOR_TYPE_INTENSITY_MODE_2))
                    return VERT_TEX_INTENSITY_16_BIT_TEX_COORD;
            } else {
                if (poly_state.color_type == TA_COLOR_TYPE_PACKED)
                    return VERT_TEX_PACKED_COLOR;
                if (poly_state.color_type == TA_COLOR_TYPE_FLOAT)
                    return VERT_TEX_FLOATING_COLOR;
                if ((poly_state.color_type == TA_COLOR_TYPE_INTENSITY_MODE_1) ||
                    (poly_state.color_type == TA_COLOR_TYPE_INTENSITY_MODE_2))
                    return VERT_TEX_INTENSITY;
            }
        }
    } else {
        if (poly_state.two_volumes_mode) {
            if (poly_state.color_type == TA_COLOR_TYPE_PACKED)
                return VERT_NO_TEX_PACKED_COLOR_TWO_VOLUMES;
            if ((poly_state.color_type == TA_COLOR_TYPE_INTENSITY_MODE_1) ||
                (poly_state.color_type == TA_COLOR_TYPE_INTENSITY_MODE_2))
                return VERT_NO_TEX_INTENSITY_TWO_VOLUMES;
        } else {
            if (poly_state.color_type == TA_COLOR_TYPE_PACKED)
                return VERT_NO_TEX_PACKED_COLOR;
            if (poly_state.color_type == TA_COLOR_TYPE_FLOAT)
                return VERT_NO_TEX_FLOAT_COLOR;
            if ((poly_state.color_type == TA_COLOR_TYPE_INTENSITY_MODE_1) ||
                (poly_state.color_type == TA_COLOR_TYPE_INTENSITY_MODE_2))
                return VERT_NO_TEX_INTENSITY;
        }
    }

    RAISE_ERROR(ERROR_UNIMPLEMENTED);
}

static void ta_fifo_finish_packet(void) {
    ta_fifo_byte_count = 0;
}

static void render_frame_init(void) {
    // free vertex arrays
    pvr2_ta_vert_buf_count = 0;
    pvr2_ta_vert_cur_group = 0;

    open_group = false;

    // free up gfx_il commands
    gfx_il_inst_buf_count = 0;
    enum display_list_type list;
    for (list = DISPLAY_LIST_FIRST; list < DISPLAY_LIST_COUNT; list++) {
        disp_list_begin[list] = NULL;
        disp_list_end[list] = NULL;
    }

    clip_min = -1.0f;
    clip_max = 1.0f;

    memset(list_submitted, 0, sizeof(list_submitted));
    poly_state.current_list = DISPLAY_LIST_NONE;
}

unsigned get_cur_frame_stamp(void) {
    return next_frame_stamp;
}
