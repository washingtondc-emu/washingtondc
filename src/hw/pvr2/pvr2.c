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

#include "spg.h"
#include "pvr2_reg.h"
#include "pvr2_core_reg.h"
#include "pvr2_ta.h"

#include "pvr2.h"

void pvr2_init(void) {
    pvr2_reg_init();
    pvr2_core_reg_init();
    spg_init();
    pvr2_ta_init();
}

void pvr2_cleanup(void) {
    pvr2_ta_cleanup();
    spg_cleanup();
    pvr2_core_reg_cleanup();
    pvr2_reg_cleanup();
}
