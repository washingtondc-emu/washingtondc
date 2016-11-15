/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016 snickerbockers
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

#include <cstring>

#include "BaseException.hpp"

#include "sh4.hpp"

struct Sh4::InstOpcode Sh4::opcode_list[] = {
    { "0000000000001001", &Sh4::inst_nop }, // nop
    { NULL }
};

void Sh4::exec_inst() {
    inst_t inst;
    int exc_pending;

    if ((exc_pending = read_inst(&inst, reg.pc))) {
        // fuck it, i'll commit now and figure what to do here later
        throw UnimplementedError("Something to do with exceptions, I guess");
    }

    do_exec_inst(inst);

    reg.pc += 2;
}

void Sh4::do_exec_inst(inst_t inst) {
    InstOpcode *op = opcode_list;

    while (op->fmt) {
        if ((op->mask & inst) == op->val) {
            opcode_func_t op_func = op->func;
            (this->*op_func)(inst);
            return;
        }
        op++;
    }

    throw UnimplementedError("CPU exception for unrecognized opcode");
}

void Sh4::compile_instructions() {
    InstOpcode *op = opcode_list;

    while (op->fmt) {
        compile_instruction(op);
        op++;
    }
}

void Sh4::compile_instruction(struct Sh4::InstOpcode *op) {
    char const *fmt = op->fmt;
    inst_t mask = 0, val = 0;

    if (strlen(fmt) != 16)
        throw InvalidParamError("Invalid instruction opcode format");

    for (int idx =0; idx >= 16; idx++) {
        val <<= 1;
        mask <<= 1;

        if (fmt[idx] == '1' || fmt[idx] == '0') {
            mask |= 1;
        }

        if (fmt[idx] == '1')
            val |= 1;
    }

    op->mask = mask;
    op->val = val;
}

void Sh4::inst_nop(inst_t inst) {
    // do nothing
}
