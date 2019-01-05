/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017-2019 snickerbockers
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

#include "spg.h"
#include "pvr2_reg.h"
#include "pvr2_ta.h"
#include "pvr2_tex_cache.h"

#include "pvr2.h"

struct dc_clock *pvr2_clk;

void pvr2_init(struct pvr2 *pvr2, struct dc_clock *clk) {
    memset(pvr2, 0, sizeof(*pvr2));

    pvr2->clk = clk;
    pvr2_clk = clk;
    pvr2_reg_init(pvr2);
    spg_init(pvr2);
    pvr2_tex_cache_init();
    pvr2_ta_init(pvr2);
}

void pvr2_cleanup(struct pvr2 *pvr2) {
    pvr2_ta_cleanup(pvr2);
    pvr2_tex_cache_cleanup();
    spg_cleanup(pvr2);
    pvr2_reg_cleanup(pvr2);
}
