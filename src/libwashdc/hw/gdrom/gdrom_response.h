/*******************************************************************************
 *
 * Copyright 2017 snickerbockers
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
