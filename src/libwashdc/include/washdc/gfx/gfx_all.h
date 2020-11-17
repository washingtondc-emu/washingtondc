/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2019, 2020 snickerbockers
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

#ifndef WASHDC_GFX_H_
#define WASHDC_GFX_H_

#include "config.h"
#include "def.h"
#include "washdc/gfx/gfx_il.h"

#ifdef __cplusplus
extern "C" {
#endif

struct gfx_rend_if {
    void (*init)(void);

    void (*cleanup)(void);

    void (*exec_gfx_il)(struct gfx_il_inst *cmd, unsigned n_cmd);
};

#ifdef __cplusplus
}
#endif

#endif
