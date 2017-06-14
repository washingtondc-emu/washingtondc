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

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "hw/sys/holly_intc.h"

#include "pvr2_ta.h"

#define PVR2_CMD_MAX_LEN 64

#define TA_CMD_TYPE_SHIFT 29
#define TA_CMD_TYPE_MASK (0x7 << TA_CMD_TYPE_SHIFT)

#define TA_CMD_DISP_LIST_SHIFT 24
#define TA_CMD_DISP_LIST_MASK (0x7 << TA_CMD_DISP_LIST_SHIFT)

#define TA_CMD_TYPE_END_OF_LIST 0x0
#define TA_CMD_TYPE_USER_CLIP   0x1
// what is 2?
// what is 3?
#define TA_CMD_TYPE_POLY_HDR    0x4
#define TA_CMD_TYPE_SPRITE      0x5
// what is 6?
#define TA_CMD_TYPE_VERTEX      0x7

static uint8_t ta_fifo[PVR2_CMD_MAX_LEN];

unsigned expected_ta_fifo_len = 32;
unsigned ta_fifo_byte_count = 0;

/*
 * vert_buf_len is in terms of verts, so the
 * actual number of elements used in vert_buf
 * is vert_buf_len*3
 */
#define MAX_VERTS (1 << 16)
float vert_buf[3 * MAX_VERTS];
unsigned vert_buf_len;

enum display_list {
    DISPLAY_LIST_OPAQUE,
    DISPLAY_LIST_OPAQUE_MOD,
    DISPLAY_LIST_TRANS,
    DISPLAY_LIST_TRANS_MOD,
    DISPLAY_LIST_PUNCH_THROUGH,

    DISPLAY_LIST_COUNT,

    DISPLAY_LIST_NONE = -1
};

char const *display_list_names[DISPLAY_LIST_COUNT] = {
    "Opaque",
    "Opaque Modifier Volume",
    "Transparent",
    "Transparent Modifier Volume",
    "Punch-through Polygon"
};

bool list_submitted[DISPLAY_LIST_COUNT];

// which display list is currently open
static enum display_list current_list = DISPLAY_LIST_NONE;

static void input_poly_fifo(uint8_t byte);

// this function gets called every time a full packet is received by the TA
static void on_packet_received(void);
static void on_polyhdr_received(void);
static void on_vertex_received(void);
static void on_end_of_list_received(void);

int pvr2_ta_fifo_poly_read(void *buf, size_t addr, size_t len) {
    fprintf(stderr, "WARNING: trying to read %u bytes from the TA polygon FIFO "
            "(you get all 0s)\n", (unsigned)len);
    memset(buf, 0, len);

    return 0;
}

int pvr2_ta_fifo_poly_write(void const *buf, size_t addr, size_t len) {
    fprintf(stderr, "WARNING: writing %u bytes to TA polygon FIFO:\n",
            (unsigned)len);

    unsigned len_copy = len;

    if (len_copy % 4 == 0) {
        uint32_t *ptr = (uint32_t*)buf;
        while (len_copy) {
            fprintf(stderr, "\t%08x\n", (unsigned)*ptr++);
            len_copy -= 4;
        }
    } else {
        uint8_t *ptr = (uint8_t*)buf;
        while (len_copy--)
            fprintf(stderr, "\t%02x\n", (unsigned)*ptr++);
    }

    uint8_t *ptr = (uint8_t*)buf;
    while (len--)
        input_poly_fifo(*ptr++);

    return 0;
}

static void input_poly_fifo(uint8_t byte) {
    ta_fifo[ta_fifo_byte_count++] = byte;

    if (ta_fifo_byte_count == expected_ta_fifo_len) {
        on_packet_received();
        expected_ta_fifo_len = 32;
        ta_fifo_byte_count = 0;
    }
}

static void on_packet_received(void) {
    uint32_t const *ta_fifo32 = (uint32_t const*)ta_fifo;
    unsigned cmd_tp = (ta_fifo32[0] & TA_CMD_TYPE_MASK) >> TA_CMD_TYPE_SHIFT;

    switch(cmd_tp) {
    case TA_CMD_TYPE_VERTEX:
        on_vertex_received();
        break;
    case TA_CMD_TYPE_POLY_HDR:
        on_polyhdr_received();
        break;
    case TA_CMD_TYPE_END_OF_LIST:
        on_end_of_list_received();
        break;
    default:
        printf("UNKNOWN CMD TYPE 0x%x\n", cmd_tp);
    }
}

static void on_polyhdr_received(void) {
    uint32_t const *ta_fifo32 = (uint32_t const*)ta_fifo;
    enum display_list list =
        (enum display_list)((ta_fifo32[0] & TA_CMD_DISP_LIST_MASK) >>
                            TA_CMD_DISP_LIST_SHIFT);

    if (current_list == DISPLAY_LIST_NONE) {
        if (!list_submitted[current_list]) {
            printf("Opening display list %s\n", display_list_names[list]);
            current_list = list;
            list_submitted[current_list] = true;
        } else {
            printf("WARNING: unable to open list %s because it is already "
                   "closed\n", display_list_names[list]);
        }
    } else if (current_list != list) {
        printf("WARNING: attempting to input poly header for list %s without "
               "first closing %s\n", display_list_names[list],
               display_list_names[current_list]);
        return;
    }

    printf("POLY HEADER PACKET!\n");
}

static void on_vertex_received(void) {
    uint32_t const *ta_fifo32 = (uint32_t const*)ta_fifo;
    float const *ta_fifo_float = (float const*)ta_fifo;
    enum display_list list =
        (enum display_list)((ta_fifo32[0] & TA_CMD_DISP_LIST_MASK) >>
                            TA_CMD_DISP_LIST_SHIFT);

    if (current_list == DISPLAY_LIST_NONE) {
        printf("Opening display list %s\n", display_list_names[list]);
        current_list = list;
    } else if (current_list != list) {
        printf("WARNING: attempting to input vertex for list %s without first "
               "closing %s\n", display_list_names[list],
               display_list_names[current_list]);
        return;
    }

    printf("vertex received!\n");
    vert_buf[3 * vert_buf_len + 0] = ta_fifo_float[1];
    vert_buf[3 * vert_buf_len + 1] = ta_fifo_float[2];
    vert_buf[3 * vert_buf_len + 2] = ta_fifo_float[3];
    vert_buf_len++;
}

static void on_end_of_list_received(void) {
    printf("END-OF-LIST PACKET!\n");
    printf("Display list \"%s\" closed\n", display_list_names[current_list]);

    // TODO: In a real dreamcast this probably would not happen instantly
    switch (current_list) {
    case DISPLAY_LIST_OPAQUE:
        holly_raise_nrm_int(HOLLY_REG_ISTNRM_PVR_OPAQUE_COMPLETE);
        break;
    case DISPLAY_LIST_OPAQUE_MOD:
        holly_raise_nrm_int(HOLLY_REG_ISTNRM_PVR_OPAQUE_MOD_COMPLETE);
        break;
    case DISPLAY_LIST_TRANS:
        holly_raise_nrm_int(HOLLY_REG_ISTNRM_PVR_TRANS_COMPLETE);
        break;
    case DISPLAY_LIST_TRANS_MOD:
        holly_raise_nrm_int(HOLLY_REG_ISTNRM_PVR_TRANS_MOD_COMPLETE);
        break;
    case DISPLAY_LIST_PUNCH_THROUGH:
        holly_raise_nrm_int(HOLLY_NRM_INT_ISTNRM_PVR_PUNCH_THROUGH_COMPLETE);
        break;
    default:
        printf("WARNING: not raising interrupt for closing of list type %d "
               "(invalid)\n", current_list);
    }

    current_list = DISPLAY_LIST_NONE;
 }

void pvr2_ta_startrender(void) {
    printf("STARTRENDER requested!\n");

    printf("Vertex dump:\n");
    unsigned vert_no = 0;

    for (vert_no = 0; vert_no < vert_buf_len; vert_no++) {
        float const *vertp = vert_buf + 3 * vert_no;
        printf("\t(%f, %f, %f)\n", vertp[1], vertp[2], vertp[3]);
    }

    vert_buf_len = 0;
    memset(list_submitted, 0, sizeof(list_submitted));
    current_list = DISPLAY_LIST_NONE;

    // TODO: This irq definitely should not be triggered immediately
    holly_raise_nrm_int(HOLLY_REG_ISTNRM_PVR_RENDER_COMPLETE);
}

void pvr2_ta_reinit(void) {
    memset(list_submitted, 0, sizeof(list_submitted));
}
