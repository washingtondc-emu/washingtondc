/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018, 2019 snickerbockers
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

#include <stdio.h>

#include "sh4asm_core/disas.h"

#include "jit/jit_il.h"
#include "jit/code_block.h"
#include "jit/jit_mem.h"

#ifdef JIT_PROFILE
#include "jit/jit_profile.h"
#endif

#include "log.h"
#include "sh4.h"
#include "sh4_read_inst.h"
#include "sh4_jit.h"

struct native_dispatch_meta const sh4_native_dispatch_meta = {
#ifdef JIT_PROFILE
    .profile_notify = sh4_jit_profile_notify,
#endif
    .on_compile = sh4_jit_compile_native
};

void sh4_jit_set_native_dispatch_meta(struct native_dispatch_meta *meta) {
#ifdef JIT_PROFILE
    meta->profile_notify = sh4_jit_profile_notify;
#endif
    meta->on_compile = sh4_jit_compile_native;
}

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

static void sh4_jit_set_sr(void *ctx, uint32_t new_sr_val);

static void res_associate_reg(unsigned reg_no, unsigned slot_no);
static void res_disassociate_reg(Sh4 *sh4, struct il_code_block *block,
                                 unsigned reg_no);

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

#ifdef JIT_PROFILE
static void sh4_jit_profile_disas(FILE *out, uint32_t addr, void const *instp);
static void sh4_jit_profile_emit_fn(char ch);
#endif

void sh4_jit_init(struct Sh4 *sh4) {
#ifdef JIT_PROFILE
    jit_profile_ctxt_init(&sh4->jit_profile, sizeof(uint16_t));
    sh4->jit_profile.disas = sh4_jit_profile_disas;
#endif
}

void sh4_jit_cleanup(struct Sh4 *sh4) {
#ifdef JIT_PROFILE
    FILE *outfile = fopen("sh4_profile.txt", "w");
    if (outfile) {
        jit_profile_print(&sh4->jit_profile, outfile);
        fclose(outfile);
    } else {
        LOG_ERROR("Failure to open sh4_profile.txt for writing\n");
    }
    jit_profile_ctxt_cleanup(&sh4->jit_profile);
#endif
}

#ifdef JIT_PROFILE
static FILE *jit_profile_out;

static void sh4_jit_profile_disas(FILE *out, uint32_t addr, void const *instp) {
    jit_profile_out = out;

    uint16_t inst;
    memcpy(&inst, instp, sizeof(inst));
    sh4asm_disas_inst(inst, sh4_jit_profile_emit_fn);
    jit_profile_out = NULL;
}

static void sh4_jit_profile_emit_fn(char ch) {
    fputc(ch, jit_profile_out);
}
#endif

static void
res_drain_reg(Sh4 *sh4, struct il_code_block *block, unsigned reg_no) {
    struct residency *res = reg_map + reg_no;
    if (res->stat == REG_STATUS_SLOT) {
        jit_store_slot(block, res->slot_no, sh4->reg + reg_no);
        res->stat = REG_STATUS_SLOT_AND_SH4;
    }
}

// this function emits il ops to move all data in slots into registers.
static void res_drain_all_regs(Sh4 *sh4, struct il_code_block *block) {
    unsigned reg_no;
    for (reg_no = 0; reg_no < SH4_REGISTER_COUNT; reg_no++)
        res_drain_reg(sh4, block, reg_no);
}

/*
 * mark the given register as REG_STATUS_SH4.
 * This does not write it back to the reg array.
 */
static void res_invalidate_reg(struct il_code_block *block, unsigned reg_no) {
    struct residency *res = reg_map + reg_no;
    if (res->stat != REG_STATUS_SH4) {
        res->stat = REG_STATUS_SH4;
        free_slot(block, res->slot_no);
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

void sh4_jit_new_block(void) {
    unsigned reg_no;
    for (reg_no = 0; reg_no < SH4_REGISTER_COUNT; reg_no++) {
        reg_map[reg_no].slot_no = -1;
        reg_map[reg_no].stat = REG_STATUS_SH4;
        reg_map[reg_no].last_read = 0;
        reg_map[reg_no].last_write = 0;
    }
}

static void
sh4_jit_delay_slot(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                   struct il_code_block *block, unsigned pc) {
    cpu_inst_param inst = sh4_do_read_inst(sh4, pc);
    struct InstOpcode const *inst_op = sh4_decode_inst(inst);
    if (inst_op->pc_relative) {
        error_set_feature("illegal slot exceptions in the jit");
        error_set_address(pc);
        RAISE_ERROR(ERROR_UNIMPLEMENTED);
    }

#ifdef JIT_PROFILE
        uint16_t inst16 = inst;
        jit_profile_push_inst(&sh4->jit_profile, block->profile, &inst16);
#endif

    if (!inst_op->disas(sh4, ctx, block, pc, inst_op, inst)) {
        /*
         * in theory, this will never happen because only branch instructions
         * can return true, and those all should have been filtered out by the
         * pc_relative check above.
         */
        printf("inst is 0x%04x\n", (unsigned)inst);
        RAISE_ERROR(ERROR_INTEGRITY);
    }
    unsigned old_cycle_count = ctx->cycle_count;
    ctx->cycle_count += sh4_count_inst_cycles(inst_op,
                                              &ctx->last_inst_type);
    if (old_cycle_count > ctx->cycle_count)
        LOG_ERROR("*** JIT DETECTED CYCLE COUNT OVERFLOW ***\n");
}

bool
sh4_jit_compile_inst(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                     struct il_code_block *block, cpu_inst_param inst,
                     unsigned pc) {
    struct InstOpcode const *inst_op = sh4_decode_inst(inst);

    unsigned old_cycle_count = ctx->cycle_count;
    ctx->cycle_count += sh4_count_inst_cycles(inst_op,
                                              &ctx->last_inst_type);
    if (old_cycle_count > ctx->cycle_count)
        LOG_ERROR("*** JIT DETECTED CYCLE COUNT OVERFLOW ***\n");

    return inst_op->disas(sh4, ctx, block, pc, inst_op, inst);
}

bool
sh4_jit_fallback(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                 struct il_code_block *block, unsigned pc,
                 struct InstOpcode const *op, cpu_inst_param inst) {
    struct jit_inst il_inst;

    res_drain_all_regs(sh4, block);
    res_invalidate_all_regs(block);

    il_inst.op = JIT_OP_FALLBACK;
    il_inst.immed.fallback.fallback_fn = op->func;
    il_inst.immed.fallback.inst = inst;

    il_code_block_push_inst(block, &il_inst);

    return true;
}

bool sh4_jit_rts(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                 struct il_code_block *block, unsigned pc,
                 struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned slot_no = reg_slot(sh4, block, SH4_REG_PR);
    res_disassociate_reg(sh4, block, SH4_REG_PR);

    sh4_jit_delay_slot(sh4, ctx, block, pc + 2);

    res_drain_all_regs(sh4, block);

    jit_jump(block, slot_no);

    free_slot(block, slot_no);

    return false;
}

bool sh4_jit_rte(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                 struct il_code_block *block, unsigned pc,
                 struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned slot_no = reg_slot(sh4, block, SH4_REG_SPC);
    res_disassociate_reg(sh4, block, SH4_REG_SPC);

    /*
     * there are a few different ways editing the SR can cause side-effects (for
     * example by initiating a bank-switch) so we need to make sure everything
     * is committed to the reg array and we also need to make sure we reload any
     * registers referenced after the jit_restore_sr operation.
     */
    res_drain_all_regs(sh4, block);
    res_invalidate_all_regs(block);

    jit_call_func(block, sh4_jit_set_sr, reg_slot(sh4, block, SH4_REG_SSR));

    sh4_jit_delay_slot(sh4, ctx, block, pc + 2);

    res_drain_all_regs(sh4, block);

    jit_jump(block, slot_no);

    free_slot(block, slot_no);

    return false;
}

bool sh4_jit_braf_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                     struct il_code_block *block, unsigned pc,
                     struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_no = (inst >> 8) & 0xf;
    unsigned jump_offs = pc + 4;

    unsigned slot_no = reg_slot(sh4, block, SH4_REG_R0 + reg_no);
    res_disassociate_reg(sh4, block, SH4_REG_R0 + reg_no);
    jit_add_const32(block, slot_no, jump_offs);

    sh4_jit_delay_slot(sh4, ctx, block, pc + 2);

    res_drain_all_regs(sh4, block);

    jit_jump(block, slot_no);

    free_slot(block, slot_no);

    return false;
}

bool sh4_jit_bsrf_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                     struct il_code_block *block, unsigned pc,
                     struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_no = (inst >> 8) & 0xf;
    unsigned jump_offs = pc + 4;

    unsigned addr_slot_no = reg_slot(sh4, block, SH4_REG_R0 + reg_no);
    res_disassociate_reg(sh4, block, SH4_REG_R0 + reg_no);
    jit_add_const32(block, addr_slot_no, jump_offs);


    unsigned pr_slot_no = reg_slot_noload(sh4, block, SH4_REG_PR);
    jit_set_slot(block, pr_slot_no, pc + 4);

    sh4_jit_delay_slot(sh4, ctx, block, pc + 2);

    res_drain_all_regs(sh4, block);

    jit_jump(block, addr_slot_no);

    free_slot(block, addr_slot_no);

    return false;
}

bool sh4_jit_bf(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                struct il_code_block *block, unsigned pc,
                struct InstOpcode const *op, cpu_inst_param inst) {
    int jump_offs = (int)((int8_t)(inst & 0x00ff)) * 2 + 4;

    unsigned slot_no = reg_slot(sh4, block, SH4_REG_SR);
    res_disassociate_reg(sh4, block, SH4_REG_SR);

    res_drain_all_regs(sh4, block);

    unsigned jmp_addr_slot = alloc_slot(block);
    unsigned alt_jmp_addr_slot = alloc_slot(block);

    jit_set_slot(block, jmp_addr_slot, pc + jump_offs);
    jit_set_slot(block, alt_jmp_addr_slot, pc + 2);

    jit_jump_cond(block, slot_no, jmp_addr_slot, alt_jmp_addr_slot, 0);

    free_slot(block, alt_jmp_addr_slot);
    free_slot(block, jmp_addr_slot);

    free_slot(block, slot_no);

    return false;
}

bool sh4_jit_bt(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                struct il_code_block *block, unsigned pc,
                struct InstOpcode const *op, cpu_inst_param inst) {
    int jump_offs = (int)((int8_t)(inst & 0x00ff)) * 2 + 4;

    unsigned slot_no = reg_slot(sh4, block, SH4_REG_SR);
    res_disassociate_reg(sh4, block, SH4_REG_SR);

    res_drain_all_regs(sh4, block);

    unsigned jmp_addr_slot = alloc_slot(block);
    unsigned alt_jmp_addr_slot = alloc_slot(block);

    jit_set_slot(block, jmp_addr_slot, pc + jump_offs);
    jit_set_slot(block, alt_jmp_addr_slot, pc + 2);

    jit_jump_cond(block, slot_no, jmp_addr_slot, alt_jmp_addr_slot, 1);

    free_slot(block, alt_jmp_addr_slot);
    free_slot(block, jmp_addr_slot);

    free_slot(block, slot_no);

    return false;
}

bool sh4_jit_bfs(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                 struct il_code_block *block, unsigned pc,
                 struct InstOpcode const *op, cpu_inst_param inst) {
    int jump_offs = (int)((int8_t)(inst & 0x00ff)) * 2 + 4;

    unsigned slot_no = reg_slot(sh4, block, SH4_REG_SR);
    res_disassociate_reg(sh4, block, SH4_REG_SR);

    sh4_jit_delay_slot(sh4, ctx, block, pc + 2);

    res_drain_all_regs(sh4, block);

    unsigned jmp_addr_slot = alloc_slot(block);
    unsigned alt_jmp_addr_slot = alloc_slot(block);

    jit_set_slot(block, jmp_addr_slot, pc + jump_offs);
    jit_set_slot(block, alt_jmp_addr_slot, pc + 4);

    jit_jump_cond(block, slot_no, jmp_addr_slot, alt_jmp_addr_slot, 0);

    free_slot(block, alt_jmp_addr_slot);
    free_slot(block, jmp_addr_slot);

    free_slot(block, slot_no);

    return false;
}

bool sh4_jit_bts(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                 struct il_code_block *block, unsigned pc,
                 struct InstOpcode const *op, cpu_inst_param inst) {
    int jump_offs = (int)((int8_t)(inst & 0x00ff)) * 2 + 4;

    unsigned slot_no = reg_slot(sh4, block, SH4_REG_SR);
    res_disassociate_reg(sh4, block, SH4_REG_SR);

    sh4_jit_delay_slot(sh4, ctx, block, pc + 2);

    res_drain_all_regs(sh4, block);

    unsigned jmp_addr_slot = alloc_slot(block);
    unsigned alt_jmp_addr_slot = alloc_slot(block);

    jit_set_slot(block, jmp_addr_slot, pc + jump_offs);
    jit_set_slot(block, alt_jmp_addr_slot, pc + 4);

    jit_jump_cond(block, slot_no, jmp_addr_slot, alt_jmp_addr_slot, 1);

    free_slot(block, alt_jmp_addr_slot);
    free_slot(block, jmp_addr_slot);

    free_slot(block, slot_no);

    return false;
}

bool sh4_jit_bra(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                 struct il_code_block *block, unsigned pc,
                 struct InstOpcode const *op, cpu_inst_param inst) {
    int32_t disp = inst & 0x0fff;
    if (disp & 0x0800)
        disp |= 0xfffff000;
    disp = disp * 2 + 4;

    sh4_jit_delay_slot(sh4, ctx, block, pc + 2);

    res_drain_all_regs(sh4, block);

    unsigned addr_slot = alloc_slot(block);
    jit_set_slot(block, addr_slot, pc + disp);

    jit_jump(block, addr_slot);

    free_slot(block, addr_slot);

    return false;
}

bool sh4_jit_bsr(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                 struct il_code_block *block, unsigned pc,
                 struct InstOpcode const *op, cpu_inst_param inst) {
    int32_t disp = inst & 0x0fff;
    if (disp & 0x0800)
        disp |= 0xfffff000;
    disp = disp * 2 + 4;

    unsigned slot_no = reg_slot_noload(sh4, block, SH4_REG_PR);
    jit_set_slot(block, slot_no, pc + 4);

    sh4_jit_delay_slot(sh4, ctx, block, pc + 2);

    res_drain_all_regs(sh4, block);

    unsigned addr_slot = alloc_slot(block);
    jit_set_slot(block, addr_slot, pc + disp);

    jit_jump(block, addr_slot);

    free_slot(block, addr_slot);

    return false;
}

bool sh4_jit_jmp_arn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                     struct il_code_block *block, unsigned pc,
                     struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_no = (inst >> 8) & 0xf;

    unsigned slot_no = reg_slot(sh4, block, SH4_REG_R0 + reg_no);
    res_disassociate_reg(sh4, block, SH4_REG_R0 + reg_no);

    sh4_jit_delay_slot(sh4, ctx, block, pc + 2);

    res_drain_all_regs(sh4, block);

    jit_jump(block, slot_no);

    free_slot(block, slot_no);

    return false;
}

bool sh4_jit_jsr_arn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                     struct il_code_block *block, unsigned pc,
                     struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_no = (inst >> 8) & 0xf;

    unsigned addr_slot_no = reg_slot(sh4, block, SH4_REG_R0 + reg_no);
    res_disassociate_reg(sh4, block, SH4_REG_R0 + reg_no);

    unsigned pr_slot_no = reg_slot_noload(sh4, block, SH4_REG_PR);
    jit_set_slot(block, pr_slot_no, pc + 4);

    sh4_jit_delay_slot(sh4, ctx, block, pc + 2);

    res_drain_all_regs(sh4, block);

    jit_jump(block, addr_slot_no);

    free_slot(block, addr_slot_no);

    return false;
}

// disassembles the "mov.w @(disp, pc), rn" instruction
bool
sh4_jit_movw_a_disp_pc_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                          struct il_code_block *block, unsigned pc,
                          struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_no = ((inst >> 8) & 0xf) + SH4_REG_R0;
    unsigned disp = inst & 0xff;
    addr32_t addr = disp * 2 + pc + 4;

    unsigned slot_no = reg_slot_noload(sh4, block, reg_no);

    jit_mem_read_constaddr_16(sh4->mem.map, block, addr, slot_no);

    jit_sign_extend_16(block, slot_no);

    return true;
}

// disassembles the "mov.l @(disp, pc), rn" instruction
bool
sh4_jit_movl_a_disp_pc_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                          struct il_code_block *block, unsigned pc,
                          struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_no = (inst >> 8) & 0xf;
    unsigned disp = inst & 0xff;
    addr32_t addr = disp * 4 + (pc & ~3) + 4;

    unsigned slot_no = reg_slot_noload(sh4, block, reg_no);
    jit_mem_read_constaddr_32(sh4->mem.map, block, addr, slot_no);

    return true;
}

bool
sh4_jit_mova_a_disp_pc_r0(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                          struct il_code_block *block, unsigned pc,
                          struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned disp = inst & 0xff;
    addr32_t addr = disp * 4 + (pc & ~3) + 4;

    unsigned slot_no = reg_slot_noload(sh4, block, SH4_REG_R0);
    jit_set_slot(block, slot_no, addr);

    return true;
}

bool
sh4_jit_nop(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
            struct il_code_block *block, unsigned pc,
            struct InstOpcode const *op, cpu_inst_param inst) {
    return true;
}

bool sh4_jit_ocbi_arn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                      struct il_code_block *block, unsigned pc,
                      struct InstOpcode const *op, cpu_inst_param inst) {
    return true;
}

bool sh4_jit_ocbp_arn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                      struct il_code_block *block, unsigned pc,
                      struct InstOpcode const *op, cpu_inst_param inst) {
    return true;
}

bool sh4_jit_ocbwb_arn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                       struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, cpu_inst_param inst) {
    return true;
}

// ADD Rm, Rn
// 0011nnnnmmmm1100
bool sh4_jit_add_rm_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                       struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_src = (inst & 0x00f0) >> 4;
    unsigned reg_dst = (inst & 0x0f00) >> 8;

    unsigned slot_src = reg_slot(sh4, block, reg_src);
    unsigned slot_dst = reg_slot(sh4, block, reg_dst);

    jit_add(block, slot_src, slot_dst);

    reg_map[reg_dst].stat = REG_STATUS_SLOT;

    return true;
}

// ADD #imm, Rn
// 0111nnnniiiiiiii
bool sh4_jit_add_imm_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                        struct il_code_block *block, unsigned pc,
                        struct InstOpcode const *op, cpu_inst_param inst) {
    int32_t imm_val = (int32_t)(int8_t)(inst & 0xff);
    unsigned reg_dst = (inst & 0x0f00) >> 8;

    unsigned slot_dst = reg_slot(sh4, block, reg_dst);

    jit_add_const32(block, slot_dst, imm_val);

    reg_map[reg_dst].stat = REG_STATUS_SLOT;

    return true;
}

// XOR Rm, Rn
// 0010nnnnmmmm1010
bool sh4_jit_xor_rm_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                       struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_src = (inst & 0x00f0) >> 4;
    unsigned reg_dst = (inst & 0x0f00) >> 8;

    unsigned slot_src = reg_slot(sh4, block, reg_src);
    unsigned slot_dst = reg_slot(sh4, block, reg_dst);

    jit_xor(block, slot_src, slot_dst);

    reg_map[reg_dst].stat = REG_STATUS_SLOT;

    return true;
}

// MOV Rm, Rn
// 0110nnnnmmmm0011
bool sh4_jit_mov_rm_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                       struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_src = (inst & 0x00f0) >> 4;
    unsigned reg_dst = (inst & 0x0f00) >> 8;

    unsigned slot_src = reg_slot(sh4, block, reg_src);
    unsigned slot_dst = reg_slot(sh4, block, reg_dst);

    jit_mov(block, slot_src, slot_dst);

    reg_map[reg_dst].stat = REG_STATUS_SLOT;

    return true;
}

// AND Rm, Rn
// 0010nnnnmmmm1001
bool sh4_jit_and_rm_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                       struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_src = (inst & 0x00f0) >> 4;
    unsigned reg_dst = (inst & 0x0f00) >> 8;

    unsigned slot_src = reg_slot(sh4, block, reg_src);
    unsigned slot_dst = reg_slot(sh4, block, reg_dst);

    jit_and(block, slot_src, slot_dst);

    reg_map[reg_dst].stat = REG_STATUS_SLOT;

    return true;
}

// OR Rm, Rn
// 0010nnnnmmmm1011
bool sh4_jit_or_rm_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                      struct il_code_block *block, unsigned pc,
                      struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_src = (inst & 0x00f0) >> 4;
    unsigned reg_dst = (inst & 0x0f00) >> 8;

    unsigned slot_src = reg_slot(sh4, block, reg_src);
    unsigned slot_dst = reg_slot(sh4, block, reg_dst);

    jit_or(block, slot_src, slot_dst);

    reg_map[reg_dst].stat = REG_STATUS_SLOT;

    return true;
}

// SUB Rm, Rn
// 0011nnnnmmmm1000
bool sh4_jit_sub_rm_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                       struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_src = (inst & 0x00f0) >> 4;
    unsigned reg_dst = (inst & 0x0f00) >> 8;

    unsigned slot_src = reg_slot(sh4, block, reg_src);
    unsigned slot_dst = reg_slot(sh4, block, reg_dst);

    jit_sub(block, slot_src, slot_dst);

    reg_map[reg_dst].stat = REG_STATUS_SLOT;

    return true;
}

// AND #imm, R0
// 11001001iiiiiiii
bool
sh4_inst_binary_andb_imm_r0(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                            struct il_code_block *block, unsigned pc,
                            struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned imm_val = inst & 0xff;
    unsigned slot_no = reg_slot(sh4, block, SH4_REG_R0);

    jit_and_const32(block, slot_no, imm_val);
    reg_map[SH4_REG_R0].stat = REG_STATUS_SLOT;

    return true;
}

// OR #imm, R0
// 11001011iiiiiiii
bool sh4_jit_or_imm8_r0(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                        struct il_code_block *block, unsigned pc,
                        struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned imm_val = inst & 0xff;
    unsigned slot_no = reg_slot(sh4, block, SH4_REG_R0);

    jit_or_const32(block, slot_no, imm_val);
    reg_map[SH4_REG_R0].stat = REG_STATUS_SLOT;

    return true;
}

// XOR #imm, R0
// 11001010iiiiiiii
bool sh4_jit_xor_imm8_r0(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                         struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned imm_val = inst & 0xff;
    unsigned slot_no = reg_slot(sh4, block, SH4_REG_R0);

    jit_xor_const32(block, slot_no, imm_val);

    reg_map[SH4_REG_R0].stat = REG_STATUS_SLOT;

    return true;
}

// TST Rm, Rn
// 0010nnnnmmmm1000
bool sh4_jit_tst_rm_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                       struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_dst = ((inst & 0x0f00) >> 8) + SH4_REG_R0;
    unsigned reg_src = ((inst & 0x00f0) >> 4) + SH4_REG_R0;

    unsigned slot_src = reg_slot(sh4, block, reg_src);
    unsigned slot_dst = reg_slot(sh4, block, reg_dst);
    unsigned slot_sr = reg_slot(sh4, block, SH4_REG_SR);

    res_disassociate_reg(sh4, block, reg_dst);
    jit_and(block, slot_src, slot_dst);

    jit_slot_to_bool(block, slot_dst);
    jit_not(block, slot_dst);
    jit_and_const32(block, slot_dst, 1);

    jit_and_const32(block, slot_sr, ~1);
    jit_or(block, slot_dst, slot_sr);

    reg_map[SH4_REG_SR].stat = REG_STATUS_SLOT;

    free_slot(block, slot_dst);

    return true;
}

// TST #imm, R0
// 11001000iiiiiiii
bool sh4_jit_tst_imm8_r0(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                         struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned slot_r0 = reg_slot(sh4, block, SH4_REG_R0);
    unsigned slot_sr = reg_slot(sh4, block, SH4_REG_SR);

    res_disassociate_reg(sh4, block, SH4_REG_R0);
    jit_and_const32(block, slot_r0, inst & 0xff);

    jit_slot_to_bool(block, slot_r0);
    jit_not(block, slot_r0);
    jit_and_const32(block, slot_r0, 1);

    jit_and_const32(block, slot_sr, ~1);
    jit_or(block, slot_r0, slot_sr);

    reg_map[SH4_REG_SR].stat = REG_STATUS_SLOT;

    free_slot(block, slot_r0);

    return true;
}

// MOV.L @(R0, Rm), Rn
// 0000nnnnmmmm1110
bool sh4_jit_movl_a_r0_rm_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                             struct il_code_block *block, unsigned pc,
                             struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_src = ((inst & 0x00f0) >> 4) + SH4_REG_R0;
    unsigned reg_dst = ((inst & 0x0f00) >> 8) + SH4_REG_R0;

    unsigned slot_src = reg_slot(sh4, block, reg_src);
    unsigned slot_dst = reg_slot(sh4, block, reg_dst);
    unsigned slot_r0 = reg_slot(sh4, block, SH4_REG_R0);

    unsigned slot_srcaddr = alloc_slot(block);

    jit_mov(block, slot_src, slot_srcaddr);
    jit_add(block, slot_r0, slot_srcaddr);

    jit_read_32_slot(block, sh4->mem.map, slot_srcaddr, slot_dst);

    reg_map[reg_dst].stat = REG_STATUS_SLOT;

    free_slot(block, slot_srcaddr);

    return true;
}

// MOV.L @Rm, Rn
// 0110nnnnmmmm0010
bool sh4_jit_movl_arm_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                         struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_src = ((inst & 0x00f0) >> 4) + SH4_REG_R0;
    unsigned reg_dst = ((inst & 0x0f00) >> 8) + SH4_REG_R0;

    unsigned slot_src = reg_slot(sh4, block, reg_src);
    unsigned slot_dst = reg_slot(sh4, block, reg_dst);

    jit_read_32_slot(block, sh4->mem.map, slot_src, slot_dst);

    reg_map[reg_dst].stat = REG_STATUS_SLOT;

    return true;
}

// MOV.L Rm, @Rn
// 0010nnnnmmmm0010
bool sh4_jit_movl_rm_arn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                         struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_src = ((inst & 0x00f0) >> 4) + SH4_REG_R0;
    unsigned reg_dst = ((inst & 0x0f00) >> 8) + SH4_REG_R0;

    unsigned slot_src = reg_slot(sh4, block, reg_src);
    unsigned slot_dst = reg_slot(sh4, block, reg_dst);

    jit_write_32_slot(block, sh4->mem.map, slot_src, slot_dst);

    reg_map[reg_src].stat = REG_STATUS_SLOT;

    return true;
}

// MOV.L @(disp, Rm), Rn
// 0101nnnnmmmmdddd
bool
sh4_jit_movl_a_disp4_rm_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                           struct il_code_block *block, unsigned pc,
                           struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned disp = (inst & 0xf) << 2;
    unsigned reg_src = ((inst & 0x00f0) >> 4) + SH4_REG_R0;
    unsigned reg_dst = ((inst & 0x0f00) >> 8) + SH4_REG_R0;

    unsigned slot_src = reg_slot(sh4, block, reg_src);
    res_disassociate_reg(sh4, block, reg_src);
    jit_add_const32(block, slot_src, disp);

    unsigned slot_dst = reg_slot(sh4, block, reg_dst);

    jit_read_32_slot(block, sh4->mem.map, slot_src, slot_dst);

    reg_map[reg_dst].stat = REG_STATUS_SLOT;

    free_slot(block, slot_src);

    return true;
}

// MOV.L @(disp, GBR), R0
// 11000110dddddddd
bool
sh4_jit_movl_a_disp8_gbr_r0(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                            struct il_code_block *block, unsigned pc,
                            struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned disp = (inst & 0xff) << 2;
    unsigned reg_src = SH4_REG_GBR;
    unsigned reg_dst = SH4_REG_R0;

    unsigned slot_src = reg_slot(sh4, block, reg_src);
    res_disassociate_reg(sh4, block, reg_src);
    jit_add_const32(block, slot_src, disp);

    unsigned slot_dst = reg_slot(sh4, block, reg_dst);

    jit_read_32_slot(block, sh4->mem.map, slot_src, slot_dst);

    reg_map[reg_dst].stat = REG_STATUS_SLOT;

    free_slot(block, slot_src);

    return true;
}

// MOV.W @Rm+, Rn
// 0110nnnnmmmm0101
bool sh4_jit_movw_armp_rn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                          struct il_code_block *block, unsigned pc,
                          struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_src = ((inst >> 4) & 0xf) + SH4_REG_R0;
    unsigned reg_dst = ((inst >> 8) & 0xf) + SH4_REG_R0;

    unsigned slot_src = reg_slot(sh4, block, reg_src);
    unsigned slot_dst = reg_slot(sh4, block, reg_dst);

    jit_read_16_slot(block, sh4->mem.map, slot_src, slot_dst);
    jit_sign_extend_16(block, slot_dst);
    if (reg_src != reg_dst)
        jit_add_const32(block, slot_src, 2);

    reg_map[reg_dst].stat = REG_STATUS_SLOT;
    reg_map[reg_src].stat = REG_STATUS_SLOT;

    return true;
}

// MOV.L @Rm+, Rn
// 0110nnnnmmmm0110
bool sh4_jit_movl_armp_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                          struct il_code_block *block, unsigned pc,
                          struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_src = ((inst & 0x00f0) >> 4) + SH4_REG_R0;
    unsigned reg_dst = ((inst & 0x0f00) >> 8) + SH4_REG_R0;

    unsigned slot_src = reg_slot(sh4, block, reg_src);
    unsigned slot_dst = reg_slot(sh4, block, reg_dst);

    jit_read_32_slot(block, sh4->mem.map, slot_src, slot_dst);
    if (reg_src != reg_dst)
        jit_add_const32(block, slot_src, 4);

    reg_map[reg_dst].stat = REG_STATUS_SLOT;
    reg_map[reg_src].stat = REG_STATUS_SLOT;

    return true;
}

// MOV.L Rm, @-Rn
// 0010nnnnmmmm0110
bool sh4_jit_movl_rm_amrn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                          struct il_code_block *block, unsigned pc,
                          struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_src = ((inst & 0x00f0) >> 4) + SH4_REG_R0;
    unsigned reg_dst = ((inst & 0x0f00) >> 8) + SH4_REG_R0;

    unsigned slot_src = reg_slot(sh4, block, reg_src);
    unsigned slot_dst = reg_slot(sh4, block, reg_dst);

    jit_add_const32(block, slot_dst, -4);
    jit_write_32_slot(block, sh4->mem.map, slot_src, slot_dst);

    reg_map[reg_dst].stat = REG_STATUS_SLOT;
    reg_map[reg_src].stat = REG_STATUS_SLOT;

    return true;
}

// LDS.L @Rm+, PR
// 0100mmmm00100110
bool sh4_jit_ldsl_armp_pr(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                          struct il_code_block *block, unsigned pc,
                          struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned addr_reg = ((inst & 0x0f00) >> 8) + SH4_REG_R0;

    unsigned addr_slot = reg_slot(sh4, block, addr_reg);
    unsigned pr_slot = reg_slot(sh4, block, SH4_REG_PR);

    jit_read_32_slot(block, sh4->mem.map, addr_slot, pr_slot);
    jit_add_const32(block, addr_slot, 4);

    reg_map[SH4_REG_PR].stat = REG_STATUS_SLOT;
    reg_map[addr_reg].stat = REG_STATUS_SLOT;

    return true;
}

// MOV #imm, Rn
// 1110nnnniiiiiiii
bool sh4_jit_mov_imm8_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                         struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, cpu_inst_param inst) {
    int32_t imm32 = (int32_t)((int8_t)(inst & 0xff));
    unsigned dst_reg = ((inst & 0x0f00) >> 8) + SH4_REG_R0;

    unsigned dst_slot = reg_slot_noload(sh4, block, dst_reg);
    jit_set_slot(block, dst_slot, imm32);

    reg_map[dst_reg].stat = REG_STATUS_SLOT;

    return true;
}

// SHLL16 Rn
// 0100nnnn00101000
bool sh4_jit_shll16_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                       struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_no = ((inst & 0x0f00) >> 8) + SH4_REG_R0;
    unsigned slot_no = reg_slot(sh4, block, reg_no);
    jit_shll(block, slot_no, 16);

    reg_map[reg_no].stat = REG_STATUS_SLOT;

    return true;
}

// SHLL2 Rn
// 0100nnnn00001000
bool sh4_jit_shll2_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                      struct il_code_block *block, unsigned pc,
                      struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_no = ((inst & 0x0f00) >> 8) + SH4_REG_R0;
    unsigned slot_no = reg_slot(sh4, block, reg_no);
    jit_shll(block, slot_no, 2);

    reg_map[reg_no].stat = REG_STATUS_SLOT;

    return true;
}

// SHLL8 Rn
// 0100nnnn00011000
bool sh4_jit_shll8_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                      struct il_code_block *block, unsigned pc,
                      struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_no = ((inst & 0x0f00) >> 8) + SH4_REG_R0;
    unsigned slot_no = reg_slot(sh4, block, reg_no);
    jit_shll(block, slot_no, 8);

    reg_map[reg_no].stat = REG_STATUS_SLOT;

    return true;
}

// SHAR Rn
// 0100nnnn00100001
bool sh4_jit_shar_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                     struct il_code_block *block, unsigned pc,
                     struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_no = ((inst & 0x0f00) >> 8) + SH4_REG_R0;
    unsigned slot_no = reg_slot(sh4, block, reg_no);
    unsigned tmp_cpy = alloc_slot(block);
    unsigned sr_slot = reg_slot(sh4, block, SH4_REG_SR);

    // set the T-bit in SR from the shift-out.a
    jit_mov(block, slot_no, tmp_cpy);
    jit_and_const32(block, tmp_cpy, 1);
    jit_and_const32(block, sr_slot, ~1);
    jit_or(block, tmp_cpy, sr_slot);
    reg_map[SH4_REG_SR].stat = REG_STATUS_SLOT;

    free_slot(block, tmp_cpy);

    jit_shar(block, slot_no, 1);

    reg_map[reg_no].stat = REG_STATUS_SLOT;

    return true;
}

// SHLR Rn
// 0100nnnn00000001
bool sh4_jit_shlr_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                     struct il_code_block *block, unsigned pc,
                     struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_no = ((inst & 0x0f00) >> 8) + SH4_REG_R0;
    unsigned slot_no = reg_slot(sh4, block, reg_no);
    unsigned tmp_cpy = alloc_slot(block);
    unsigned sr_slot = reg_slot(sh4, block, SH4_REG_SR);

    // set the T-bit in SR from the shift-out.a
    jit_mov(block, slot_no, tmp_cpy);
    jit_and_const32(block, tmp_cpy, 1);
    jit_and_const32(block, sr_slot, ~1);
    jit_or(block, tmp_cpy, sr_slot);
    reg_map[SH4_REG_SR].stat = REG_STATUS_SLOT;

    free_slot(block, tmp_cpy);

    jit_shlr(block, slot_no, 1);

    reg_map[reg_no].stat = REG_STATUS_SLOT;

    return true;
}

// SHLL Rn
// 0100nnnn00000000
bool sh4_jit_shll_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                     struct il_code_block *block, unsigned pc,
                     struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_no = ((inst & 0x0f00) >> 8) + SH4_REG_R0;
    unsigned slot_no = reg_slot(sh4, block, reg_no);
    unsigned tmp_cpy = alloc_slot(block);
    unsigned sr_slot = reg_slot(sh4, block, SH4_REG_SR);

    // set the T-bit in SR from the shift-out.
    jit_mov(block, slot_no, tmp_cpy);
    jit_and_const32(block, tmp_cpy, 1<<31);
    jit_shlr(block, tmp_cpy, 31);
    jit_and_const32(block, sr_slot, ~1);
    jit_or(block, tmp_cpy, sr_slot);
    reg_map[SH4_REG_SR].stat = REG_STATUS_SLOT;

    free_slot(block, tmp_cpy);

    jit_shll(block, slot_no, 1);

    reg_map[reg_no].stat = REG_STATUS_SLOT;

    return true;
}

// SHAL Rn
// 0100nnnn00100000
bool sh4_jit_shal_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                     struct il_code_block *block, unsigned pc,
                     struct InstOpcode const *op, cpu_inst_param inst) {
    // As far as I know, SHLL and SHAL do the exact same thing.
    return sh4_jit_shll_rn(sh4, ctx, block, pc, op, inst);
}


// SHAD Rm, Rn
// 0100nnnnmmmm1100
bool sh4_jit_shad_rm_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                        struct il_code_block *block, unsigned pc,
                        struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_src = (inst & 0x00f0) >> 4;
    unsigned reg_dst = (inst & 0x0f00) >> 8;

    unsigned slot_src = reg_slot(sh4, block, reg_src);
    unsigned slot_dst = reg_slot(sh4, block, reg_dst);

    jit_shad(block, slot_dst, slot_src);

    reg_map[reg_dst].stat = REG_STATUS_SLOT;

    return true;
}

// SHLR2 Rn
// 0100nnnn00001001
bool sh4_jit_shlr2_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                      struct il_code_block *block, unsigned pc,
                      struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_no = ((inst & 0x0f00) >> 8) + SH4_REG_R0;
    unsigned slot_no = reg_slot(sh4, block, reg_no);
    jit_shlr(block, slot_no, 2);

    reg_map[reg_no].stat = REG_STATUS_SLOT;

    return true;
}

// SHLR8 Rn
// 0100nnnn00011001
bool sh4_jit_shlr8_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                      struct il_code_block *block, unsigned pc,
                      struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_no = ((inst & 0x0f00) >> 8) + SH4_REG_R0;
    unsigned slot_no = reg_slot(sh4, block, reg_no);
    jit_shlr(block, slot_no, 8);

    reg_map[reg_no].stat = REG_STATUS_SLOT;

    return true;
}

// SHLR16 Rn
// 0100nnnn00101001
bool sh4_jit_shlr16_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                       struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_no = ((inst & 0x0f00) >> 8) + SH4_REG_R0;
    unsigned slot_no = reg_slot(sh4, block, reg_no);
    jit_shlr(block, slot_no, 16);

    reg_map[reg_no].stat = REG_STATUS_SLOT;

    return true;
}

// SWAP.W Rm, Rn
// 0110nnnnmmmm1001
bool sh4_jit_swapw_rm_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                         struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_src = ((inst & 0x00f0) >> 4) + SH4_REG_R0;
    unsigned reg_dst = ((inst & 0x0f00) >> 8) + SH4_REG_R0;

    unsigned slot_src = reg_slot(sh4, block, reg_src);
    unsigned slot_dst = reg_slot_noload(sh4, block, reg_dst);

    unsigned slot_tmp = alloc_slot(block);

    jit_mov(block, slot_src, slot_tmp);
    jit_shlr(block, slot_tmp, 16);

    jit_mov(block, slot_src, slot_dst);
    jit_and_const32(block, slot_dst, 0xffff);
    jit_shll(block, slot_dst, 16);

    jit_or(block, slot_tmp, slot_dst);

    reg_map[reg_dst].stat = REG_STATUS_SLOT;

    free_slot(block, slot_tmp);

    return true;
}

// CMP/HI Rm, Rn
// 0011nnnnmmmm0110
bool sh4_jit_cmphi_rm_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                         struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_src = ((inst & 0x00f0) >> 4) + SH4_REG_R0;
    unsigned reg_dst = ((inst & 0x0f00) >> 8) + SH4_REG_R0;

    unsigned slot_src = reg_slot(sh4, block, reg_src);
    unsigned slot_dst = reg_slot(sh4, block, reg_dst);
    unsigned slot_sr = reg_slot(sh4, block, SH4_REG_SR);

    jit_and_const32(block, slot_sr, ~1);
    jit_set_gt_unsigned(block, slot_dst, slot_src, slot_sr);

    reg_map[SH4_REG_SR].stat = REG_STATUS_SLOT;

    return true;
}

// CMP/GT Rm, Rn
// 0011nnnnmmmm0111
bool sh4_jit_cmpgt_rm_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                         struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_src = ((inst & 0x00f0) >> 4) + SH4_REG_R0;
    unsigned reg_dst = ((inst & 0x0f00) >> 8) + SH4_REG_R0;

    unsigned slot_src = reg_slot(sh4, block, reg_src);
    unsigned slot_dst = reg_slot(sh4, block, reg_dst);
    unsigned slot_sr = reg_slot(sh4, block, SH4_REG_SR);

    jit_and_const32(block, slot_sr, ~1);
    jit_set_gt_signed(block, slot_dst, slot_src, slot_sr);

    reg_map[SH4_REG_SR].stat = REG_STATUS_SLOT;

    return true;
}

// CMP/EQ Rm, Rn
// 0011nnnnmmmm0000
bool sh4_jit_cmpeq_rm_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                         struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_src = ((inst & 0x00f0) >> 4) + SH4_REG_R0;
    unsigned reg_dst = ((inst & 0x0f00) >> 8) + SH4_REG_R0;

    unsigned slot_src = reg_slot(sh4, block, reg_src);
    unsigned slot_dst = reg_slot(sh4, block, reg_dst);
    unsigned slot_sr = reg_slot(sh4, block, SH4_REG_SR);

    jit_and_const32(block, slot_sr, ~1);
    jit_set_eq(block, slot_dst, slot_src, slot_sr);

    reg_map[SH4_REG_SR].stat = REG_STATUS_SLOT;

    return true;
}

// CMP/HS Rm, Rn
// 0011nnnnmmmm0010
bool sh4_jit_cmphs_rm_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                         struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_src = ((inst & 0x00f0) >> 4) + SH4_REG_R0;
    unsigned reg_dst = ((inst & 0x0f00) >> 8) + SH4_REG_R0;

    unsigned slot_src = reg_slot(sh4, block, reg_src);
    unsigned slot_dst = reg_slot(sh4, block, reg_dst);
    unsigned slot_sr = reg_slot(sh4, block, SH4_REG_SR);

    jit_and_const32(block, slot_sr, ~1);
    jit_set_ge_unsigned(block, slot_dst, slot_src, slot_sr);

    reg_map[SH4_REG_SR].stat = REG_STATUS_SLOT;

    return true;
}

// MULU.W Rm, Rn
// 0010nnnnmmmm1110
bool sh4_jit_muluw_rm_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                         struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_lhs = ((inst & 0x00f0) >> 4) + SH4_REG_R0;
    unsigned reg_rhs = ((inst & 0x0f00) >> 8) + SH4_REG_R0;

    unsigned slot_lhs = reg_slot(sh4, block, reg_lhs);
    unsigned slot_rhs = reg_slot(sh4, block, reg_rhs);
    unsigned slot_macl = reg_slot(sh4, block, SH4_REG_MACL);

    unsigned slot_lhs_16 = alloc_slot(block);
    unsigned slot_rhs_16 = alloc_slot(block);

    /*
     * TODO: x86 has instructions that can move and zero-extend at the same
     * time, and that would probably be faster than moving plus AND'ing.  I'd
     * have to add new IL op for that, which is why I'm doing it the naive way
     * for now.
     */
    jit_mov(block, slot_lhs, slot_lhs_16);
    jit_mov(block, slot_rhs, slot_rhs_16);
    jit_and_const32(block, slot_lhs_16, 0xffff);
    jit_and_const32(block, slot_rhs_16, 0xffff);

    jit_mul_u32(block, slot_lhs_16, slot_rhs_16, slot_macl);

    reg_map[SH4_REG_MACL].stat = REG_STATUS_SLOT;

    free_slot(block, slot_rhs_16);
    free_slot(block, slot_lhs_16);

    return true;
}

// STS MACL, Rn
// 0000nnnn00011010
bool sh4_jit_sts_macl_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                         struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_dst = ((inst & 0x0f00) >> 8) + SH4_REG_R0;

    unsigned slot_dst = reg_slot(sh4, block, reg_dst);
    unsigned slot_macl = reg_slot(sh4, block, SH4_REG_MACL);

    jit_mov(block, slot_macl, slot_dst);

    reg_map[reg_dst].stat = REG_STATUS_SLOT;

    return true;
}

// CMP/GE Rm, Rn
// 0011nnnnmmmm0011
bool sh4_jit_cmpge_rm_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                         struct il_code_block *block, unsigned pc,
                         struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_src = ((inst & 0x00f0) >> 4) + SH4_REG_R0;
    unsigned reg_dst = ((inst & 0x0f00) >> 8) + SH4_REG_R0;

    unsigned slot_src = reg_slot(sh4, block, reg_src);
    unsigned slot_dst = reg_slot(sh4, block, reg_dst);
    unsigned slot_sr = reg_slot(sh4, block, SH4_REG_SR);

    jit_and_const32(block, slot_sr, ~1);
    jit_set_ge_signed(block, slot_dst, slot_src, slot_sr);

    reg_map[SH4_REG_SR].stat = REG_STATUS_SLOT;

    return true;
}

// CMP/PL Rn
// 0100nnnn00010101
bool sh4_jit_cmppl_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                      struct il_code_block *block, unsigned pc,
                      struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_lhs = ((inst & 0x0f00) >> 8) + SH4_REG_R0;

    unsigned slot_lhs = reg_slot(sh4, block, reg_lhs);
    unsigned slot_sr = reg_slot(sh4, block, SH4_REG_SR);

    jit_and_const32(block, slot_sr, ~1);
    jit_set_gt_signed_const(block, slot_lhs, 0, slot_sr);

    reg_map[SH4_REG_SR].stat = REG_STATUS_SLOT;

    return true;
}

// CMP/PZ Rn
// 0100nnnn00010001
bool sh4_jit_cmppz_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                      struct il_code_block *block, unsigned pc,
                      struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_lhs = ((inst & 0x0f00) >> 8) + SH4_REG_R0;

    unsigned slot_lhs = reg_slot(sh4, block, reg_lhs);
    unsigned slot_sr = reg_slot(sh4, block, SH4_REG_SR);

    jit_and_const32(block, slot_sr, ~1);
    jit_set_ge_signed_const(block, slot_lhs, 0, slot_sr);

    reg_map[SH4_REG_SR].stat = REG_STATUS_SLOT;

    return true;
}

// NOT Rm, Rn
// 0110nnnnmmmm0111
bool sh4_jit_not_rm_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                       struct il_code_block *block, unsigned pc,
                       struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_src = ((inst & 0x00f0) >> 4) + SH4_REG_R0;
    unsigned reg_dst = ((inst & 0x0f00) >> 8) + SH4_REG_R0;

    unsigned slot_src = reg_slot(sh4, block, reg_src);
    unsigned slot_dst = reg_slot(sh4, block, reg_dst);

    jit_mov(block, slot_src, slot_dst);
    jit_not(block, slot_dst);

    reg_map[reg_dst].stat = REG_STATUS_SLOT;

    return true;
}

// DT Rn
// 0100nnnn00010000
bool sh4_jit_dt_rn(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                   struct il_code_block *block, unsigned pc,
                   struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_no = ((inst & 0x0f00) >> 8) + SH4_REG_R0;
    unsigned slot_no = reg_slot(sh4, block, reg_no);
    unsigned sr_slot = reg_slot(sh4, block, SH4_REG_SR);
    unsigned tmp_slot = alloc_slot(block);

    jit_and_const32(block, sr_slot, ~1);
    jit_add_const32(block, slot_no, ~(uint32_t)0);
    jit_mov(block, slot_no, tmp_slot);
    jit_slot_to_bool(block, tmp_slot);
    jit_not(block, tmp_slot);
    jit_and_const32(block, tmp_slot, 1);
    jit_or(block, tmp_slot, sr_slot);

    reg_map[reg_no].stat = REG_STATUS_SLOT;
    reg_map[SH4_REG_SR].stat = REG_STATUS_SLOT;

    return true;
}

// CLRT
// 0000000000001000
bool sh4_jit_clrt(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                  struct il_code_block *block, unsigned pc,
                  struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned sr_slot = reg_slot(sh4, block, SH4_REG_SR);

    jit_and_const32(block, sr_slot, ~1);

    reg_map[SH4_REG_SR].stat = REG_STATUS_SLOT;

    return true;
}

// SETT
// 0000000000011000
bool sh4_jit_sett(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                  struct il_code_block *block, unsigned pc,
                  struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned sr_slot = reg_slot(sh4, block, SH4_REG_SR);

    jit_or_const32(block, sr_slot, 1);

    reg_map[SH4_REG_SR].stat = REG_STATUS_SLOT;

    return true;
}

// MOVT Rn
// 0000nnnn00101001
bool sh4_jit_movt(Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                  struct il_code_block *block, unsigned pc,
                  struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned reg_no = ((inst & 0x0f00) >> 8) + SH4_REG_R0;

    unsigned slot_no = reg_slot(sh4, block, reg_no);
    unsigned sr_slot = reg_slot(sh4, block, SH4_REG_SR);

    jit_mov(block, sr_slot, slot_no);
    jit_and_const32(block, slot_no, 1);

    reg_map[reg_no].stat = REG_STATUS_SLOT;

    return true;
}

// STS.L PR, @-Rn
// 0100nnnn00100010
bool
sh4_jit_stsl_pr_amrn(struct Sh4 *sh4, struct sh4_jit_compile_ctx* ctx,
                     struct il_code_block *block, unsigned pc,
                     struct InstOpcode const *op, cpu_inst_param inst) {
    unsigned addr_reg = ((inst >> 8) & 0xf) + SH4_REG_R0;
    unsigned addr_slot = reg_slot(sh4, block, addr_reg);
    unsigned pr_slot = reg_slot(sh4, block, SH4_REG_PR);

    jit_add_const32(block, addr_slot, -4);
    jit_write_32_slot(block, sh4->mem.map, pr_slot, addr_slot);

    reg_map[addr_reg].stat = REG_STATUS_SLOT;
    reg_map[SH4_REG_PR].stat = REG_STATUS_SLOT;

    return true;
}

static unsigned reg_slot(Sh4 *sh4, struct il_code_block *block, unsigned reg_no) {
    struct residency *res = reg_map + reg_no;

    if (res->stat == REG_STATUS_SH4) {
        // need to load it into an unused slot
        unsigned slot_no = alloc_slot(block);
        res_associate_reg(reg_no, slot_no);
        res->stat = REG_STATUS_SLOT_AND_SH4;
        res->slot_no = slot_no;
        // TODO: set res->last_read here
        jit_load_slot(block, slot_no, sh4->reg + reg_no);
    }

    return res->slot_no;
}

static unsigned reg_slot_noload(Sh4 *sh4,
                                struct il_code_block *block, unsigned reg_no) {
    struct residency *res = reg_map + reg_no;
    if (res->stat == REG_STATUS_SH4) {
        unsigned slot_no = alloc_slot(block);
        res_associate_reg(reg_no, slot_no);
        res->stat = REG_STATUS_SLOT;
        res->slot_no = slot_no;
        // TODO: set res->last_read here
    } else if (res->stat == REG_STATUS_SLOT_AND_SH4) {
        res->stat = REG_STATUS_SLOT;
    }
    return res->slot_no;
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
static void res_disassociate_reg(Sh4 *sh4, struct il_code_block *block,
                                 unsigned reg_no) {
    res_drain_reg(sh4, block, reg_no);
    struct residency *res = reg_map + reg_no;
    res->stat = REG_STATUS_SH4;
}

static void sh4_jit_set_sr(void *ctx, uint32_t new_sr_val) {
    struct Sh4 *sh4 = (struct Sh4*)ctx;
    uint32_t old_sr = sh4->reg[SH4_REG_SR];
    sh4->reg[SH4_REG_SR] = new_sr_val;
    sh4_on_sr_change(sh4, old_sr);
}
