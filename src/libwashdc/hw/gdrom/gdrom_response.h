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

#ifndef GDROM_RESPONSE_H_
#define GDROM_RESPONSE_H_

#include <stdint.h>

// harcoded GD-ROM response packets

/*
 * response to command packet 0x11 (REQ_MODE).  A couple of these fields
 * are supposed to be user-editable via the 0x12 (SET_MODE) packet.  Mostly
 * it's just irrelevant text used to get the drive's firmware version.  For
 * now none of these fields can be changed because I haven't implemented
 * that yet.
 */
#define GDROM_REQ_MODE_RESP_LEN 32
extern uint8_t const gdrom_req_mode_resp[GDROM_REQ_MODE_RESP_LEN];

#define GDROM_PKT_71_RESP_LEN 960
extern uint8_t const pkt71_resp[GDROM_PKT_71_RESP_LEN];

#define GDROM_IDENT_RESP_LEN 80
extern uint8_t const gdrom_ident_resp[GDROM_IDENT_RESP_LEN];

#endif
