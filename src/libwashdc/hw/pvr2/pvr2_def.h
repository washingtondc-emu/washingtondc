/*******************************************************************************
 *
 * Copyright 2020 snickerbockers
 * snickerbockers@washemu.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

/*
 * The purpose of this format is to hold definitions which are needed by more
 * than one pvr2 header file.
 *
 * Specifically it's intended for stuff that's shared between pvr2_ta.h and
 * pvr2_core.h, but really anything pvr-related can go here.
 */

#include <stdint.h>
#include <string.h>

#ifndef PVR2_DEF_H_
#define PVR2_DEF_H_

#define PVR2_TRACE(msg, ...)                                            \
    do {                                                                \
        LOG_DBG("PVR2: ");                                              \
        LOG_DBG(msg, ##__VA_ARGS__);                                    \
    } while (0)

/*
 * pixel formats for the texture control word.
 *
 * PAL here means "palette", not the European video standard.
 *
 * Also TEX_CTRL_PIX_FMT_INVALID is treated as TEX_CTRL_PIX_FMT_ARGB_1555 even
 * though it's still invalid.
 */
enum TexCtrlPixFmt {
    TEX_CTRL_PIX_FMT_ARGB_1555,
    TEX_CTRL_PIX_FMT_RGB_565,
    TEX_CTRL_PIX_FMT_ARGB_4444,
    TEX_CTRL_PIX_FMT_YUV_422,
    TEX_CTRL_PIX_FMT_BUMP_MAP,
    TEX_CTRL_PIX_FMT_4_BPP_PAL,
    TEX_CTRL_PIX_FMT_8_BPP_PAL,
    TEX_CTRL_PIX_FMT_INVALID,

    TEX_CTRL_PIX_FMT_COUNT // obviously this is not a real pixel format
};

enum pvr2_hdr_tp {
    PVR2_HDR_TRIANGLE_STRIP,
    PVR2_HDR_QUAD
};

/*
 * There are five polygon types:
 *
 * Opaque
 * Punch-through polygon
 * Opaque/punch-through modifier volume
 * Translucent
 * Translucent modifier volume
 *
 * They are rendered by the opengl backend in that order.
 */
enum pvr2_poly_type {
    PVR2_POLY_TYPE_FIRST,
    PVR2_POLY_TYPE_OPAQUE = PVR2_POLY_TYPE_FIRST,
    PVR2_POLY_TYPE_OPAQUE_MOD,
    PVR2_POLY_TYPE_TRANS,
    PVR2_POLY_TYPE_TRANS_MOD,
    PVR2_POLY_TYPE_PUNCH_THROUGH,
    PVR2_POLY_TYPE_LAST = PVR2_POLY_TYPE_PUNCH_THROUGH,

    // These three list types are invalid, but I do see PVR2_POLY_TYPE_7 sometimes
    PVR2_POLY_TYPE_5,
    PVR2_POLY_TYPE_6,
    PVR2_POLY_TYPE_7,

    PVR2_POLY_TYPE_COUNT,

    PVR2_POLY_TYPE_NONE = -1
};

enum ta_color_type {
    TA_COLOR_TYPE_PACKED,
    TA_COLOR_TYPE_FLOAT,
    TA_COLOR_TYPE_INTENSITY_MODE_1,
    TA_COLOR_TYPE_INTENSITY_MODE_2
};

// determines how the user tile clip rectangle is to be used
enum pvr2_user_clip_mode {
    PVR2_USER_CLIP_DISABLE,
    PVR2_USER_CLIP_RESERVED,// error
    PVR2_USER_CLIP_INSIDE,
    PVR2_USER_CLIP_OUTSIDE
};

// unpack 16-bit texture coordinates into two floats
inline static void
unpack_uv16(float *u_coord, float *v_coord, void const *input) {
    uint32_t val = *(uint32_t*)input;
    uint32_t u_val = val & 0xffff0000;
    uint32_t v_val = val << 16;

    memcpy(u_coord, &u_val, sizeof(*u_coord));
    memcpy(v_coord, &v_val, sizeof(*v_coord));
}

#endif
