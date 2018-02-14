/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018 snickerbockers
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

#include "jit/jit_il.h"
#include "jit/code_block.h"
#include "jit/jit_mem.h"

#include "sh4.h"
#include "sh4_read_inst.h"
#include "sh4_disas.h"

enum reg_status {
    // the register resides in the sh4's reg array
    REG_STATUS_SH4,

    /*
     * the register resides in a slot, but it does not need to be written back
     * to the sh4's reg array because it has not been written to (yet).
     */
    REG_STATUS_SLOT_AND_SH4,

    /*
     * the register resides in a slot and the copy of the register in the sh4's
     * reg array is outdated.  The slot will need to be written back to the
     * sh4's reg array at some point before the current code block ends.
     */
    REG_STATUS_SLOT
};

struct residency {
    enum reg_status stat;
    int slot_no;

    /*
     * these track the value of inst_count (from the il_code_block) the last
     * time this slot was used by this register.  The idea is that the IL will
     * be able to use these to minimize the number of slots in use at any time
     * by writing slots back to the sh4 registers after they've been used for
     * the last time.  Currently that's not implemented, and slots are only
     * written back when they need to be.
     */
    unsigned last_write, last_read;
};

// this is a temporary space the il uses to map sh4 registers to slots
static struct residency reg_map[SH4_REGISTER_COUNT];

/*
 * this tells whether a given slot is in use.
 * It is safe to make MAX_SLOTS bigger if necessary
 */
#define MAX_SLOTS 512
static bool slot_status[MAX_SLOTS];

// this counts the number of slots that have been allocated
static unsigned n_slots;

// this counts how many slots are currently in use
static unsigned n_slots_in_use;

// this stores the maximum value of n_slots_in_use
static unsigned max_slots;

static void res_associate_reg(unsigned reg_no, unsigned slot_no);
static void res_disassociate_reg(struct il_code_block *block,
                                 unsigned reg_no);
static unsigned res_alloc_slot(struct il_code_block *block);
static void res_free_slot(struct il_code_block *block, unsigned slot_no);

/*
 * this will load the given register into a slot if it is not already in a slot
 * and then return the index of the slot it resides in.
 *
 * the register will be marked as REG_STATUS_SLOT_AND_SH4 if it its status is
 * REG_STATUS_SH4.  Otherwise the reg status will be left alone.
 */
static unsigned reg_slot(Sh4 *sh4, struct il_code_block *block, unsigned reg_no);

/*
 * return the slot index of a given register.  If the register is
 * REG_STATUS_SH4, then allocate a new slot for it, set the reg status to
 * REG_STATUS_SLOT and return the new slot.  If the reg status is
 * REG_STATUS_SLOT_AND_SH4, then the existing slot index will be returned but
 * the reg status will still be set to REG_STATUS_SLOT.
 *
 * This function will not load the register into the slot; instead it will set
 * the register residency to point to the slot without initializing the slot
 * contents.  This function is intended for situations in which the preexisting
 * contents of a given register are irrelevant because they will immediately be
 * overwritten.
 */
static unsigned
reg_slot_noload(Sh4 *sh4, struct il_code_block *block, unsigned reg_no);

static void res_drain_reg(struct il_code_block *block, unsigned reg_no) {
    Sh4 *sh4 = dreamcast_get_cpu();
    struct residency *res = reg_map + reg_no;
    if (res->stat == REG_STATUS_SLOT) {
        jit_store_slot(block, res->slot_no, sh4->reg + reg_no);
        res->stat = REG_STATUS_SLOT_AND_SH4;
    }
}

// this function emits il ops to move all data in slots into registers.
static void res_drain_all_regs(struct il_code_block *block) {
    unsigned reg_no;
    for (reg_no = 0; reg_no < SH4_REGISTER_COUNT; reg_no++)
        res_drain_reg(block, reg_no);
}

/*
 * mark the given register as REG_STATUS_SH4.
 * This does not write it back to the reg array.
 */
static void res_invalidate_reg(struct il_code_block *block, unsigned reg_no) {
    struct residency *res = reg_map + reg_no;
    if (res->stat != REG_STATUS_SH4) {
        res->stat = REG_STATUS_SH4;
        n_slots_in_use--;
        res_free_slot(block, res->slot_no);
    }
}

/*
 * mark all registers as REG_STATUS_SH4.
 * This does not write them back to the reg array.
 */
static void res_invalidate_all_regs(struct il_code_block *block) {
    unsigned reg_no;
    for (reg_no = 0; reg_no < SH4_REGISTER_COUNT; reg_no++)
        if (reg_map[reg_no].stat != REG_STATUS_SH4)
            res_invalidate_reg(block, reg_no);
}

void sh4_disas_new_block(void) {
    unsigned reg_no;
    for (reg_no = 0; reg_no < SH4_REGISTER_COUNT; reg_no++) {
        reg_map[reg_no].slot_no = -1;
        reg_map[reg_no].stat = REG_STATUS_SH4;
        reg_map[reg_no].last_read = 0;
        reg_map[reg_no].last_write = 0;
    }

    unsigned slot_no;
    for (slot_no = 0; slot_no < MAX_SLOTS; slot_no++)
        slot_status[slot_no] = false;

    n_slots_in_use = 0;
    n_slots = 0;
    max_slots = 0;
}

static void sh4_disas_delay_slot(struct il_code_block *block, unsigned pc) {
    inst_t inst = sh4_do_read_inst(pc);
    struct InstOpcode const *inst_op = sh4_decode_inst(inst);
    if (inst_op->pc_relative) {
        error_set_feature("illegal slot exceptions in the jit");
        error_set_address(pc);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

    if (!inst_op->disas(block, pc, inst_op, inst)) {
        /*
         * in theory, this will never happen because only branch instructions
         * can return true, and those all should have been filtered out by the
         * pc_relative check above.
         */
        printf("inst is 0x%04x\n", (unsigned)inst);
        RAISE_ERROR(ERROR_INTEGRITY);
    }
    block->cycle_count += sh4_count_inst_cycles(inst_op,
                                                &block->last_inst_type);
}

bool sh4_disas_inst(struct il_code_block *block, unsigned pc) {
    inst_t inst = sh4_do_read_inst(pc);
    struct InstOpcode const *inst_op = sh4_decode_inst(inst);
    block->cycle_count += sh4_count_inst_cycles(inst_op,
                                                &block->last_inst_type);
    return inst_op->disas(block, pc, inst_op, inst);
}

bool sh4_disas_fallback(struct il_code_block *block, unsigned pc,
                        struct InstOpcode const *op, inst_t inst) {
    struct jit_inst il_inst;

    res_drain_all_regs(block);
    res_invalidate_all_regs(block);

    il_inst.op = JIT_OP_FALLBACK;
    il_inst.immed.fallback.fallback_fn = op->func;
    il_inst.immed.fallback.inst.inst = inst;

    il_code_block_push_inst(block, &il_inst);

    return true;
}

bool sh4_disas_rts(struct il_code_block *block, unsigned pc,
                   struct InstOpcode const *op, inst_t inst) {
    unsigned slot_no = reg_slot(dreamcast_get_cpu(), block, SH4_REG_PR);
    res_disassociate_reg(block, SH4_REG_PR);

    sh4_disas_delay_slot(block, pc + 2);

    res_drain_all_regs(block);

    jit_jump(block, slot_no);

    res_free_slot(block, slot_no);

    return false;
}

bool sh4_disas_rte(struct il_code_block *block, unsigned pc,
                   struct InstOpcode const *op, inst_t inst) {
    unsigned slot_no = reg_slot(dreamcast_get_cpu(), block, SH4_REG_SPC);
    res_disassociate_reg(block, SH4_REG_SPC);

    /*
     * there are a few different ways editing the SR can cause side-effects (for
     * example by initiating a bank-switch) so we need to make sure everything
     * is committed to the reg array and we also need to make sure we reload any
     * registers referenced after the jit_restore_sr operation.
     */
    res_drain_all_regs(block);
    res_invalidate_all_regs(block);

    jit_restore_sr(block, reg_slot(dreamcast_get_cpu(), block, SH4_REG_SSR));

    sh4_disas_delay_slot(block, pc + 2);

    res_drain_all_regs(block);

    jit_jump(block, slot_no);

    res_free_slot(block, slot_no);

    return false;
}

bool sh4_disas_braf_rn(struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, inst_t inst) {
    unsigned reg_no = (inst >> 8) & 0xf;
    unsigned jump_offs = pc + 4;

    unsigned slot_no = reg_slot(dreamcast_get_cpu(), block, SH4_REG_R0 + reg_no);
    res_disassociate_reg(block, SH4_REG_R0 + reg_no);
    jit_add_const32(block, slot_no, jump_offs);

    sh4_disas_delay_slot(block, pc + 2);

    res_drain_all_regs(block);

    jit_jump(block, slot_no);

    res_free_slot(block, slot_no);

    return false;
}

bool sh4_disas_bsrf_rn(struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, inst_t inst) {
    unsigned reg_no = (inst >> 8) & 0xf;
    unsigned jump_offs = pc + 4;

    unsigned addr_slot_no = reg_slot(dreamcast_get_cpu(),
                                     block, SH4_REG_R0 + reg_no);
    res_disassociate_reg(block, SH4_REG_R0 + reg_no);
    jit_add_const32(block, addr_slot_no, jump_offs);


    unsigned pr_slot_no = reg_slot_noload(dreamcast_get_cpu(), block,
                                          SH4_REG_PR);
    jit_set_slot(block, pr_slot_no, pc + 4);

    sh4_disas_delay_slot(block, pc + 2);

    res_drain_all_regs(block);

    jit_jump(block, addr_slot_no);

    res_free_slot(block, addr_slot_no);

    return false;
}

bool sh4_disas_bf(struct il_code_block *block, unsigned pc,
                  struct InstOpcode const *op, inst_t inst) {
    int jump_offs = (int)((int8_t)(inst & 0x00ff)) * 2 + 4;

    unsigned slot_no = reg_slot(dreamcast_get_cpu(), block, SH4_REG_SR);
    res_disassociate_reg(block, SH4_REG_SR);

    res_drain_all_regs(block);

    unsigned jmp_addr_slot = res_alloc_slot(block);
    unsigned alt_jmp_addr_slot = res_alloc_slot(block);

    jit_set_slot(block, jmp_addr_slot, pc + jump_offs);
    jit_set_slot(block, alt_jmp_addr_slot, pc + 2);

    jit_jump_cond(block, slot_no, jmp_addr_slot, alt_jmp_addr_slot, 0);

    res_free_slot(block, alt_jmp_addr_slot);
    res_free_slot(block, jmp_addr_slot);

    res_free_slot(block, slot_no);

    return false;
}

bool sh4_disas_bt(struct il_code_block *block, unsigned pc,
                  struct InstOpcode const *op, inst_t inst) {
    int jump_offs = (int)((int8_t)(inst & 0x00ff)) * 2 + 4;

    unsigned slot_no = reg_slot(dreamcast_get_cpu(), block, SH4_REG_SR);
    res_disassociate_reg(block, SH4_REG_SR);

    res_drain_all_regs(block);

    unsigned jmp_addr_slot = res_alloc_slot(block);
    unsigned alt_jmp_addr_slot = res_alloc_slot(block);

    jit_set_slot(block, jmp_addr_slot, pc + jump_offs);
    jit_set_slot(block, alt_jmp_addr_slot, pc + 2);

    jit_jump_cond(block, slot_no, jmp_addr_slot, alt_jmp_addr_slot, 1);

    res_free_slot(block, alt_jmp_addr_slot);
    res_free_slot(block, jmp_addr_slot);

    res_free_slot(block, slot_no);

    return false;
}

bool sh4_disas_bfs(struct il_code_block *block, unsigned pc,
                   struct InstOpcode const *op, inst_t inst) {
    int jump_offs = (int)((int8_t)(inst & 0x00ff)) * 2 + 4;

    unsigned slot_no = reg_slot(dreamcast_get_cpu(), block, SH4_REG_SR);
    res_disassociate_reg(block, SH4_REG_SR);

    sh4_disas_delay_slot(block, pc + 2);

    res_drain_all_regs(block);

    unsigned jmp_addr_slot = res_alloc_slot(block);
    unsigned alt_jmp_addr_slot = res_alloc_slot(block);

    jit_set_slot(block, jmp_addr_slot, pc + jump_offs);
    jit_set_slot(block, alt_jmp_addr_slot, pc + 4);

    jit_jump_cond(block, slot_no, jmp_addr_slot, alt_jmp_addr_slot, 0);

    res_free_slot(block, alt_jmp_addr_slot);
    res_free_slot(block, jmp_addr_slot);

    res_free_slot(block, slot_no);

    return false;
}

bool sh4_disas_bts(struct il_code_block *block, unsigned pc,
                   struct InstOpcode const *op, inst_t inst) {
    int jump_offs = (int)((int8_t)(inst & 0x00ff)) * 2 + 4;

    unsigned slot_no = reg_slot(dreamcast_get_cpu(), block, SH4_REG_SR);
    res_disassociate_reg(block, SH4_REG_SR);

    sh4_disas_delay_slot(block, pc + 2);

    res_drain_all_regs(block);

    unsigned jmp_addr_slot = res_alloc_slot(block);
    unsigned alt_jmp_addr_slot = res_alloc_slot(block);

    jit_set_slot(block, jmp_addr_slot, pc + jump_offs);
    jit_set_slot(block, alt_jmp_addr_slot, pc + 4);

    jit_jump_cond(block, slot_no, jmp_addr_slot, alt_jmp_addr_slot, 1);

    res_free_slot(block, alt_jmp_addr_slot);
    res_free_slot(block, jmp_addr_slot);

    res_free_slot(block, slot_no);

    return false;
}

bool sh4_disas_bra(struct il_code_block *block, unsigned pc,
                   struct InstOpcode const *op, inst_t inst) {
    int32_t disp = inst & 0x0fff;
    if (disp & 0x0800)
        disp |= 0xfffff000;
    disp = disp * 2 + 4;

    sh4_disas_delay_slot(block, pc + 2);

    res_drain_all_regs(block);

    unsigned addr_slot = res_alloc_slot(block);
    jit_set_slot(block, addr_slot, pc + disp);

    jit_jump(block, addr_slot);

    res_free_slot(block, addr_slot);

    return false;
}

bool sh4_disas_bsr(struct il_code_block *block, unsigned pc,
                   struct InstOpcode const *op, inst_t inst) {
    int32_t disp = inst & 0x0fff;
    if (disp & 0x0800)
        disp |= 0xfffff000;
    disp = disp * 2 + 4;

    unsigned slot_no = reg_slot_noload(dreamcast_get_cpu(), block, SH4_REG_PR);
    jit_set_slot(block, slot_no, pc + 4);

    sh4_disas_delay_slot(block, pc + 2);

    res_drain_all_regs(block);

    unsigned addr_slot = res_alloc_slot(block);
    jit_set_slot(block, addr_slot, pc + disp);

    jit_jump(block, addr_slot);

    res_free_slot(block, addr_slot);

    return false;
}

bool sh4_disas_jmp_arn(struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, inst_t inst) {
    unsigned reg_no = (inst >> 8) & 0xf;

    unsigned slot_no = reg_slot(dreamcast_get_cpu(), block,
                                SH4_REG_R0 + reg_no);
    res_disassociate_reg(block, SH4_REG_R0 + reg_no);

    sh4_disas_delay_slot(block, pc + 2);

    res_drain_all_regs(block);

    jit_jump(block, slot_no);

    res_free_slot(block, slot_no);

    return false;
}

bool sh4_disas_jsr_arn(struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, inst_t inst) {
    unsigned reg_no = (inst >> 8) & 0xf;

    unsigned addr_slot_no = reg_slot(dreamcast_get_cpu(), block,
                                     SH4_REG_R0 + reg_no);
    res_disassociate_reg(block, SH4_REG_R0 + reg_no);

    unsigned pr_slot_no = reg_slot_noload(dreamcast_get_cpu(), block,
                                          SH4_REG_PR);
    jit_set_slot(block, pr_slot_no, pc + 4);

    sh4_disas_delay_slot(block, pc + 2);

    res_drain_all_regs(block);

    jit_jump(block, addr_slot_no);

    res_free_slot(block, addr_slot_no);

    return false;
}

// disassembles the "mov.w @(disp, pc), rn" instruction
bool sh4_disas_movw_a_disp_pc_rn(struct il_code_block *block, unsigned pc,
                                 struct InstOpcode const *op, inst_t inst) {
    unsigned reg_no = ((inst >> 8) & 0xf) + SH4_REG_R0;
    unsigned disp = inst & 0xff;
    addr32_t addr = disp * 2 + pc + 4;
    Sh4 *cpu = dreamcast_get_cpu();

    unsigned slot_no = reg_slot_noload(cpu, block, reg_no);

    jit_sh4_mem_read_constaddr_16(block, addr, slot_no);

    jit_sign_extend_16(block, slot_no);

    return true;
}

// disassembles the "mov.l @(disp, pc), rn" instruction
bool sh4_disas_movl_a_disp_pc_rn(struct il_code_block *block, unsigned pc,
                                 struct InstOpcode const *op, inst_t inst) {
    unsigned reg_no = (inst >> 8) & 0xf;
    unsigned disp = inst & 0xff;
    addr32_t addr = disp * 4 + (pc & ~3) + 4;
    Sh4 *cpu = dreamcast_get_cpu();

    unsigned slot_no = reg_slot_noload(cpu, block, reg_no);
    jit_sh4_mem_read_constaddr_32(block, addr, slot_no);

    return true;
}

bool sh4_disas_mova_a_disp_pc_r0(struct il_code_block *block, unsigned pc,
                                 struct InstOpcode const *op, inst_t inst) {
    unsigned disp = inst & 0xff;
    addr32_t addr = disp * 4 + (pc & ~3) + 4;

    unsigned slot_no = reg_slot_noload(dreamcast_get_cpu(), block, SH4_REG_R0);
    jit_set_slot(block, slot_no, addr);

    return true;
}

bool sh4_disas_nop(struct il_code_block *block, unsigned pc,
                   struct InstOpcode const *op, inst_t inst) {
    return true;
}

bool sh4_disas_ocbi_arn(struct il_code_block *block, unsigned pc,
                        struct InstOpcode const *op, inst_t inst) {
    return true;
}

bool sh4_disas_ocbp_arn(struct il_code_block *block, unsigned pc,
                        struct InstOpcode const *op, inst_t inst) {
    return true;
}

bool sh4_disas_ocbwb_arn(struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, inst_t inst) {
    return true;
}

// ADD Rm, Rn
// 0011nnnnmmmm1100
bool sh4_disas_add_rm_rn(struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, inst_t inst) {
    unsigned reg_src = (inst & 0x00f0) >> 4;
    unsigned reg_dst = (inst & 0x0f00) >> 8;

    unsigned slot_src = reg_slot(dreamcast_get_cpu(), block, reg_src);
    unsigned slot_dst = reg_slot(dreamcast_get_cpu(), block, reg_dst);

    jit_add(block, slot_src, slot_dst);

    reg_map[reg_dst].stat = REG_STATUS_SLOT;

    return true;
}

// ADD #imm, Rn
// 0111nnnniiiiiiii
bool sh4_disas_add_imm_rn(struct il_code_block *block, unsigned pc,
                          struct InstOpcode const *op, inst_t inst) {
    int32_t imm_val = (int32_t)(int8_t)(inst & 0xff);
    unsigned reg_dst = (inst & 0x0f00) >> 8;

    unsigned slot_dst = reg_slot(dreamcast_get_cpu(), block, reg_dst);

    jit_add_const32(block, slot_dst, imm_val);

    reg_map[reg_dst].stat = REG_STATUS_SLOT;

    return true;
}

// XOR Rm, Rn
// 0010nnnnmmmm1010
bool sh4_disas_xor_rm_rn(struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, inst_t inst) {
    unsigned reg_src = (inst & 0x00f0) >> 4;
    unsigned reg_dst = (inst & 0x0f00) >> 8;

    unsigned slot_src = reg_slot(dreamcast_get_cpu(), block, reg_src);
    unsigned slot_dst = reg_slot(dreamcast_get_cpu(), block, reg_dst);

    jit_xor(block, slot_src, slot_dst);

    reg_map[reg_dst].stat = REG_STATUS_SLOT;

    return true;
}

// MOV Rm, Rn
// 0110nnnnmmmm0011
bool sh4_disas_mov_rm_rn(struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, inst_t inst) {
    unsigned reg_src = (inst & 0x00f0) >> 4;
    unsigned reg_dst = (inst & 0x0f00) >> 8;

    unsigned slot_src = reg_slot(dreamcast_get_cpu(), block, reg_src);
    unsigned slot_dst = reg_slot(dreamcast_get_cpu(), block, reg_dst);

    jit_mov(block, slot_src, slot_dst);

    reg_map[reg_dst].stat = REG_STATUS_SLOT;

    return true;
}

// AND Rm, Rn
// 0010nnnnmmmm1001
bool sh4_disas_and_rm_rn(struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, inst_t inst) {
    unsigned reg_src = (inst & 0x00f0) >> 4;
    unsigned reg_dst = (inst & 0x0f00) >> 8;

    unsigned slot_src = reg_slot(dreamcast_get_cpu(), block, reg_src);
    unsigned slot_dst = reg_slot(dreamcast_get_cpu(), block, reg_dst);

    jit_and(block, slot_src, slot_dst);

    reg_map[reg_dst].stat = REG_STATUS_SLOT;

    return true;
}

// OR Rm, Rn
// 0010nnnnmmmm1011
bool sh4_disas_or_rm_rn(struct il_code_block *block, unsigned pc,
                        struct InstOpcode const *op, inst_t inst) {
    unsigned reg_src = (inst & 0x00f0) >> 4;
    unsigned reg_dst = (inst & 0x0f00) >> 8;

    unsigned slot_src = reg_slot(dreamcast_get_cpu(), block, reg_src);
    unsigned slot_dst = reg_slot(dreamcast_get_cpu(), block, reg_dst);

    jit_or(block, slot_src, slot_dst);

    reg_map[reg_dst].stat = REG_STATUS_SLOT;

    return true;
}

// SUB Rm, Rn
// 0011nnnnmmmm1000
bool sh4_disas_sub_rm_rn(struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, inst_t inst) {
    unsigned reg_src = (inst & 0x00f0) >> 4;
    unsigned reg_dst = (inst & 0x0f00) >> 8;

    unsigned slot_src = reg_slot(dreamcast_get_cpu(), block, reg_src);
    unsigned slot_dst = reg_slot(dreamcast_get_cpu(), block, reg_dst);

    jit_sub(block, slot_src, slot_dst);

    reg_map[reg_dst].stat = REG_STATUS_SLOT;

    return true;
}

// AND #imm, R0
// 11001001iiiiiiii
bool sh4_inst_binary_andb_imm_r0(struct il_code_block *block, unsigned pc,
                                 struct InstOpcode const *op, inst_t inst) {
    unsigned imm_val = inst & 0xff;
    unsigned slot_no = reg_slot(dreamcast_get_cpu(), block, SH4_REG_R0);

    jit_and_const32(block, slot_no, imm_val);
    reg_map[SH4_REG_R0].stat = REG_STATUS_SLOT;

    return true;
}

// OR #imm, R0
// 11001011iiiiiiii
bool sh4_disas_or_imm8_r0(struct il_code_block *block, unsigned pc,
                          struct InstOpcode const *op, inst_t inst) {
    unsigned imm_val = inst & 0xff;
    unsigned slot_no = reg_slot(dreamcast_get_cpu(), block, SH4_REG_R0);

    jit_or_const32(block, slot_no, imm_val);
    reg_map[SH4_REG_R0].stat = REG_STATUS_SLOT;

    return true;
}

// XOR #imm, R0
// 11001010iiiiiiii
bool sh4_disas_xor_imm8_r0(struct il_code_block *block, unsigned pc,
                           struct InstOpcode const *op, inst_t inst) {
    unsigned imm_val = inst & 0xff;
    unsigned slot_no = reg_slot(dreamcast_get_cpu(), block, SH4_REG_R0);

    jit_xor_const32(block, slot_no, imm_val);

    reg_map[SH4_REG_R0].stat = REG_STATUS_SLOT;

    return true;
}

// TST Rm, Rn
// 0010nnnnmmmm1000
bool sh4_disas_tst_rm_rn(struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, inst_t inst) {
    unsigned reg_dst = ((inst & 0x0f00) >> 8) + SH4_REG_R0;
    unsigned reg_src = ((inst & 0x00f0) >> 4) + SH4_REG_R0;

    unsigned slot_src = reg_slot(dreamcast_get_cpu(), block, reg_src);
    unsigned slot_dst = reg_slot(dreamcast_get_cpu(), block, reg_dst);
    unsigned slot_sr = reg_slot(dreamcast_get_cpu(), block, SH4_REG_SR);

    res_disassociate_reg(block, reg_dst);
    jit_and(block, slot_src, slot_dst);

    jit_slot_to_bool(block, slot_dst);
    jit_not(block, slot_dst);
    jit_and_const32(block, slot_dst, 1);

    jit_and_const32(block, slot_sr, ~1);
    jit_or(block, slot_dst, slot_sr);

    reg_map[SH4_REG_SR].stat = REG_STATUS_SLOT;

    res_free_slot(block, slot_dst);

    return true;
}

// MOV.L @Rm, Rn
// 0110nnnnmmmm0010
bool sh4_disas_movl_arm_rn(struct il_code_block *block, unsigned pc,
                           struct InstOpcode const *op, inst_t inst) {
    unsigned reg_src = ((inst & 0x00f0) >> 4) + SH4_REG_R0;
    unsigned reg_dst = ((inst & 0x0f00) >> 8) + SH4_REG_R0;

    unsigned slot_src = reg_slot(dreamcast_get_cpu(), block, reg_src);
    unsigned slot_dst = reg_slot(dreamcast_get_cpu(), block, reg_dst);

    jit_read_32_slot(block, slot_src, slot_dst);

    reg_map[reg_dst].stat = REG_STATUS_SLOT;

    return true;
}

static unsigned reg_slot(Sh4 *sh4, struct il_code_block *block, unsigned reg_no) {
    struct residency *res = reg_map + reg_no;

    if (res->stat == REG_STATUS_SH4) {
        // need to load it into an unused slot
        unsigned slot_no = res_alloc_slot(block);
        res_associate_reg(reg_no, slot_no);
        res->stat = REG_STATUS_SLOT_AND_SH4;
        res->slot_no = slot_no;
        n_slots_in_use++;
        if (n_slots_in_use > max_slots)
            max_slots = n_slots_in_use;
        // TODO: set res->last_read here
        jit_load_slot(block, slot_no, sh4->reg + reg_no);
    }

    return res->slot_no;
}

static unsigned reg_slot_noload(Sh4 *sh4,
                                struct il_code_block *block, unsigned reg_no) {
    struct residency *res = reg_map + reg_no;
    if (res->stat == REG_STATUS_SH4) {
        unsigned slot_no = res_alloc_slot(block);
        res_associate_reg(reg_no, slot_no);
        res->stat = REG_STATUS_SLOT;
        res->slot_no = slot_no;
        n_slots_in_use++;
        if (n_slots_in_use > max_slots)
            max_slots = n_slots_in_use;
        // TODO: set res->last_read here
    } else if (res->stat == REG_STATUS_SLOT_AND_SH4) {
        res->stat = REG_STATUS_SLOT;
    }
    return res->slot_no;
}

static unsigned res_alloc_slot(struct il_code_block *block) {
    if (n_slots >= MAX_SLOTS)
        RAISE_ERROR(ERROR_INTEGRITY);

    unsigned slot_no = n_slots++;
    slot_status[slot_no] = true;
    il_code_block_add_slot(block);

    return slot_no;
}

static void res_associate_reg(unsigned reg_no, unsigned slot_no) {
    struct residency *res = reg_map + reg_no;
    res->slot_no = slot_no;
}

/*
 * drain the given register and then set its status to REG_STATUS_SH4.  The
 * slot the register resided in is still valid and its value is unchanged, but
 * it is no longer associated with the given register.  The caller will need to
 * call res_free_slot on that slot when it is no longer needed.
 */
static void res_disassociate_reg(struct il_code_block *block,
                                 unsigned reg_no) {
    res_drain_reg(block, reg_no);
    struct residency *res = reg_map + reg_no;
    res->stat = REG_STATUS_SH4;
}

static void res_free_slot(struct il_code_block *block, unsigned slot_no) {
    slot_status[slot_no] = false;
    jit_discard_slot(block, slot_no);
}
