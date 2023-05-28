/*******************************************************************************
 *
 * Copyright (c) 2017, 2019 snickerbockers <chimerasaurusrex@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
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

#ifndef SH4ASM_BIN_EMIT_H_
#define SH4ASM_BIN_EMIT_H_

#include <stdint.h>

#include "sh4_opcodes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void(*sh4asm_bin_emit_handler_func)(uint16_t);

static void
sh4asm_bin_emit_inst(sh4asm_bin_emit_handler_func emit, uint16_t inst) {
    emit(inst);
}

static uint16_t sh4asm_bin_assemblenoarg(uint16_t opcode) {
    return opcode;
}

static uint16_t sh4asm_bin_assemblern(uint16_t opcode, unsigned rn) {
    return opcode | (uint16_t)((rn & 15) << 8);
}

static uint16_t sh4asm_bin_assembleimm8(uint16_t opcode, unsigned imm8) {
    return opcode | (uint16_t)(imm8 & 0xff);
}

static uint16_t sh4asm_bin_assembleimm12(uint16_t opcode, unsigned imm12) {
    return opcode | (uint16_t)(imm12 & 0xfff);
}

static uint16_t
sh4asm_bin_assemblern_imm8(uint16_t opcode, unsigned rn, unsigned imm8) {
    return opcode | (uint16_t)((rn & 15) << 8) | (uint16_t)(imm8 & 0xff);
}

static uint16_t
sh4asm_bin_assemblerm_rn(uint16_t opcode, unsigned rm, unsigned rn) {
    return opcode | (uint16_t)((rm & 15) << 4) | (uint16_t)((rn & 15) << 8);
}

static uint16_t sh4asm_bin_assemblerm_rn_bank(uint16_t opcode,
                                              unsigned rm, unsigned rn_bank) {
    return opcode | (uint16_t)((rm & 15) << 8) | (uint16_t)((rn_bank & 7) << 4);
}

static uint16_t
sh4asm_bin_assemblern_imm4(uint16_t opcode, unsigned rn, unsigned imm4) {
    return opcode | (uint16_t)((rn & 15) << 4) | (uint16_t)(imm4 & 15);
}

static uint16_t sh4asm_bin_assemblerm_rn_imm4(uint16_t opcode, unsigned rm,
                                              unsigned rn, unsigned imm4) {
    return opcode | (uint16_t)(imm4 & 15) | (uint16_t)((rm & 15) << 4) |
        (uint16_t)((rn & 15) << 8);
}

static uint16_t
sh4asm_bin_assembledrm_drn(uint16_t opcode, unsigned drm, unsigned drn) {
    drm = (drm >> 1) & 7;
    drn = (drn >> 1) & 7;
    return opcode | (uint16_t)(drm << 5) | (uint16_t)(drn << 9);
}

static uint16_t
sh4asm_bin_assemblerm_drn(uint16_t opcode, unsigned rm, unsigned drn) {
    drn = (drn >> 1) & 7;
    return opcode | (uint16_t)((rm & 15) << 4) | (unsigned)(drn << 9);
}

static uint16_t
sh4asm_bin_assembledrm_rn(uint16_t opcode, unsigned drm, unsigned rn) {
    drm = (drm >> 1) & 7;
    return opcode | (uint16_t)((rn & 15) << 8) | (uint16_t)(drm << 5);
}

static uint16_t sh4asm_bin_assembledrn(uint16_t opcode, unsigned drn) {
    drn = (drn >> 1) & 7;
    return opcode | (uint16_t)(drn << 9);
}

static uint16_t
sh4asm_bin_assemblefvm_fvn(uint16_t opcode, unsigned fvm, unsigned fvn) {
    fvm = (fvm >> 2) & 3;
    fvn = (fvn >> 2) & 3;
    return opcode | (uint16_t)(fvm << 8) | (uint16_t)(fvn << 10);
}

static uint16_t sh4asm_bin_assemblefvn(uint16_t opcode, unsigned fvn) {
    fvn = (fvn >> 2) & 3;
    return opcode | (uint16_t)(fvn << 10);
}

// OP
// xxxxxxxxxxxxxxxx
#define SH4ASM_BIN_DEF_NOARG(op, val)                                   \
    static inline void sh4asm_bin_##op(sh4asm_bin_emit_handler_func em) { \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblenoarg(val));      \
    }

// OP Rn
// xxxxnnnnxxxxxxxx
#define SH4ASM_BIN_DEF_RN(op, val)                                      \
    static inline void sh4asm_bin_##op##_rn(sh4asm_bin_emit_handler_func em, \
                                            unsigned rn) {              \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblern(val, rn));     \
    }

// OP @Rn
// xxxxnnnnxxxxxxxx
#define SH4ASM_BIN_DEF_ARN(op, val)                                     \
    static inline void sh4asm_bin_##op##_arn(sh4asm_bin_emit_handler_func em, \
                                             unsigned rn) {             \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblern(val, rn));     \
    }

// OP Rm, REG
// xxxxmmmmxxxxxxxx
#define SH4ASM_BIN_DEF_RM_REG(op, val, reg)                             \
    static inline void sh4asm_bin_##op##_rm_##reg(sh4asm_bin_emit_handler_func em, \
                                                  unsigned rm) {        \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblern(val, rm));     \
    }

// OP REG, Rn
// xxxxnnnnxxxxxxxx
#define SH4ASM_BIN_DEF_REG_RN(op, val, reg)                             \
    static inline void sh4asm_bin_##op##_##reg##_rn(sh4asm_bin_emit_handler_func em, \
                                                    unsigned rn) {      \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblern(val, rn));     \
    }

// OP @Rm+, REG
// xxxxnnnnxxxxxxxx
#define SH4ASM_BIN_DEF_ARMP_REG(op, val, reg)                           \
    static inline void sh4asm_bin_##op##_armp_##reg(sh4asm_bin_emit_handler_func em, \
                                                    unsigned rm) {      \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblern(val, rm));     \
    }

// OP REG, @-Rn
// xxxxnnnnxxxxxxxx
#define SH4ASM_BIN_DEF_REG_AMRN(op, val, reg)                           \
    static inline void sh4asm_bin_##op##_##reg##_amrn(sh4asm_bin_emit_handler_func em, \
                                                      unsigned rn) {    \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblern(val, rn));     \
    }

// OP REG, @Rn
// xxxxnnnnxxxxxxxx
#define SH4ASM_BIN_DEF_REG_ARN(op, val, reg)                            \
    static inline void sh4asm_bin_##op##_##reg##_arn(sh4asm_bin_emit_handler_func em, \
                                                     unsigned rn) {     \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblern(val, rn));     \
    }

// OP FRn
#define SH4ASM_BIN_DEF_FRN(op, val)                                     \
    static inline void sh4asm_bin_##op##_frn(sh4asm_bin_emit_handler_func em, \
                                             unsigned frn) {            \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblern(val, frn));    \
    }

// OP FRm, REG
// xxxxmmmmxxxxxxxx
#define SH4ASM_BIN_DEF_FRM_REG(op, val, reg)                            \
    static inline void sh4asm_bin_##op##_frm_##reg(sh4asm_bin_emit_handler_func em, \
                                                   unsigned frm) {      \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblern(val, frm));    \
    }

// OP REG, FRn
// xxxxnnnnxxxxxxxx
#define SH4ASM_BIN_DEF_REG_FRN(op, val, reg)                            \
    static inline void sh4asm_bin_##op##_##reg##_frn(sh4asm_bin_emit_handler_func em, \
                                                     unsigned frn) {    \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblern(val, frn));    \
    }

// OP #imm8, REG
// xxxxxxxxiiiiiiii
#define SH4ASM_BIN_DEF_IMM8_REG(op, val, reg)                           \
    static inline void sh4asm_bin_##op##_imm8_##reg(sh4asm_bin_emit_handler_func em, \
                                                    unsigned imm8) {    \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assembleimm8(val, imm8)); \
    }

// OP #imm8, @(REG1, REG2)
// xxxxxxxxiiiiiiii
#define SH4ASM_BIN_DEF_IMM8_A_REG_REG(op, val, reg1, reg2)              \
    static inline void                                                  \
    sh4asm_bin_##op##_imm8_a_##reg1##_##reg2(sh4asm_bin_emit_handler_func em, \
                                             unsigned imm8) {           \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assembleimm8(val, imm8)); \
    }

// OP #imm8
// xxxxxxxxiiiiiiii
#define SH4ASM_BIN_DEF_IMM8(op, val)                                    \
    static inline void sh4asm_bin_##op##_imm8(sh4asm_bin_emit_handler_func em, \
                                              unsigned imm8) {          \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assembleimm8(val, imm8)); \
    }

/*
 * OP offset (relative to PC)
 * xxxxxxxxiiiiiiii
 *
 * the input to the generated function is the offset from the program-counter
 * register.  The target-address is this offset plus the PC.
 */
#define SH4ASM_BIN_DEF_OFFS8(op, val, disp_shift, trans)                \
    static inline void sh4asm_bin_##op##_offs8(sh4asm_bin_emit_handler_func em, \
                                               unsigned offset) {       \
        unsigned disp8 = ((offset - (trans)) >> (disp_shift)) & 0xff;   \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assembleimm8(val, disp8)); \
    }

// OP REG, @(disp8, REG)
// xxxxxxxxiiiiiiii
#define SH4ASM_BIN_DEF_REG_A_DISP8_REG(op, val, reg1, reg2, disp_shift) \
    static inline void                                                  \
    sh4asm_bin_##op##_##reg1##_a_disp8_##reg2(sh4asm_bin_emit_handler_func em, \
                                              unsigned disp8) {         \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assembleimm8(val, (disp8) >> disp_shift)); \
    }

// OP @(disp8, REG), REG
// xxxxxxxxiiiiiiii
#define SH4ASM_BIN_DEF_A_DISP8_REG_REG(op, val, reg1, reg2, disp_shift) \
    static inline void                                                  \
    sh4asm_bin_##op##_a_disp8_##reg1##_##reg2(sh4asm_bin_emit_handler_func em, \
                                              unsigned disp8) {         \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assembleimm8(val, (disp8) >> disp_shift)); \
    }

/*
 * OP @(offset, REG), REG
 * xxxxxxxxiiiiiiii
 *
 * the input to the generated function is the offset from the program-counter
 * register.  The target-address is this offset plus the PC.
 */
#define SH4ASM_BIN_DEF_A_OFFS8_REG_REG(op, val, reg1, reg2, disp_shift, trans) \
    static inline void                                                  \
    sh4asm_bin_##op##_a_offs8_##reg1##_##reg2(sh4asm_bin_emit_handler_func em, \
                                              unsigned offset) {        \
        unsigned disp8 = ((offset - (trans)) >> (disp_shift)) & 0xff;   \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assembleimm8(val, disp8)); \
    }

// OP offs12
// xxxxiiiiiiiiiiii
#define SH4ASM_BIN_DEF_OFFS12(op, val, disp_shift, trans)               \
    static inline void                                                  \
    sh4asm_bin_##op##_offs12(sh4asm_bin_emit_handler_func em, unsigned offset) { \
        unsigned disp12 = ((offset - (trans)) >> (disp_shift)) & 0xfff; \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assembleimm12(val, disp12)); \
    }

// OP #imm8, Rn
// xxxxnnnniiiiiiii
#define SH4ASM_BIN_DEF_IMM8_RN(op, val)                                 \
    static inline void                                                  \
    sh4asm_bin_##op##_imm8_rn(sh4asm_bin_emit_handler_func em,          \
                              unsigned imm8, unsigned rn) {             \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblern_imm8((val), (rn), (imm8))); \
    }

// OP @(disp8, REG), Rn
// xxxxnnnniiiiiiii
#define SH4ASM_BIN_DEF_A_OFFS8_REG_RN(op, val, reg, disp_shift, trans)  \
    static inline void                                                  \
    sh4asm_bin_##op##_a_offs8_##reg##_rn(sh4asm_bin_emit_handler_func em, \
                                         unsigned offset, unsigned rn) { \
        unsigned disp8 = ((offset - (trans)) >> (disp_shift)) & 0xff;   \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblern_imm8((val), rn, disp8)); \
    }

// OP Rm, Rn
// xxxxnnnnmmmmxxxx
#define SH4ASM_BIN_DEF_RM_RN(op, val)                                   \
    static inline void sh4asm_bin_##op##_rm_rn(sh4asm_bin_emit_handler_func em, \
                                               unsigned rm, unsigned rn) { \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblerm_rn((val), rm, rn)); \
    }

// OP Rm, @(REG, Rn)
// xxxxnnnnmmmmxxxx
#define SH4ASM_BIN_DEF_RM_A_REG_RN(op, val, reg)                        \
    static inline void                                                  \
    sh4asm_bin_##op##_rm_a_##reg##_rn(sh4asm_bin_emit_handler_func em,  \
                                      unsigned rm, unsigned rn) {       \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblerm_rn((val), rm, rn)); \
    }

// OP @(REG, Rm), Rn
// xxxxnnnnmmmmxxxx
#define SH4ASM_BIN_DEF_A_REG_RM_RN(op, val, reg)                        \
    static inline void                                                  \
    sh4asm_bin_##op##_a_##reg##_rm_rn(sh4asm_bin_emit_handler_func em,  \
                                      unsigned rm, unsigned rn) {       \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblerm_rn((val), rm, rn)); \
    }

// OP Rm, @Rn
// xxxxnnnnmmmmxxxx
#define SH4ASM_BIN_DEF_RM_ARN(op, val)                                  \
    static inline void sh4asm_bin_##op##_rm_arn(sh4asm_bin_emit_handler_func em, \
                                                unsigned rm, unsigned rn) { \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblerm_rn((val), rm, rn)); \
    }

// OP @Rm, Rn
// xxxxnnnnmmmmxxxx
#define SH4ASM_BIN_DEF_ARM_RN(op, val)                                  \
    static inline void sh4asm_bin_##op##_arm_rn(sh4asm_bin_emit_handler_func em, \
                                                unsigned rm, unsigned rn) { \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblerm_rn((val), rm, rn)); \
    }

// OP Rm, @-Rn
// xxxxnnnnmmmmxxxx
#define SH4ASM_BIN_DEF_RM_AMRN(op, val)                                 \
    static inline void sh4asm_bin_##op##_rm_amrn(sh4asm_bin_emit_handler_func em, \
                                                 unsigned rm, unsigned rn) { \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblerm_rn((val), rm, rn)); \
    }

// OP @Rm+, Rn
// xxxxnnnnmmmmxxxx
#define SH4ASM_BIN_DEF_ARMP_RN(op, val)                                 \
    static inline void sh4asm_bin_##op##_armp_rn(sh4asm_bin_emit_handler_func em, \
                                                 unsigned rm, unsigned rn) { \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblerm_rn((val), rm, rn)); \
    }

// OP @Rm+, @Rn+
// xxxxnnnnmmmmxxxx
#define SH4ASM_BIN_DEF_ARMP_ARNP(op, val)                               \
    static inline void sh4asm_bin_##op##_armp_arnp(sh4asm_bin_emit_handler_func em, \
                                                   unsigned rm, unsigned rn) { \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblerm_rn((val), rm, rn)); \
    }

// OP FRm, FRn
// xxxxnnnnmmmmxxxx
#define SH4ASM_BIN_DEF_FRM_FRN(op, val)                                 \
    static inline void sh4asm_bin_##op##_frm_frn(sh4asm_bin_emit_handler_func em, \
                                                 unsigned frm, unsigned frn) { \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblerm_rn((val), frm, frn)); \
    }

// OP @Rm, FRn
// xxxxnnnnmmmmxxxx
#define SH4ASM_BIN_DEF_ARM_FRN(op, val)                                 \
    static inline void sh4asm_bin_##op##_arm_frn(sh4asm_bin_emit_handler_func em, \
                                                 unsigned rm, unsigned frn) { \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblerm_rn((val), rm, frn)); \
    }

// OP @(REG, Rm), FRn
// xxxxnnnnmmmmxxxx
#define SH4ASM_BIN_DEF_A_REG_RM_FRN(op, val, reg)                       \
    static inline void                                                  \
    sh4asm_bin_##op##_a_##reg##_rm_frn(sh4asm_bin_emit_handler_func em, \
                                       unsigned rm, unsigned frn) {     \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblerm_rn((val), rm, frn)); \
    }

// OP @Rm+, Frn
// xxxxnnnnmmmmxxxx
#define SH4ASM_BIN_DEF_ARMP_FRN(op, val)                                \
    static inline void sh4asm_bin_##op##_armp_frn(sh4asm_bin_emit_handler_func em, \
                                                  unsigned rm, unsigned frn) { \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblerm_rn((val), rm, frn)); \
    }

// OP FRm, @Rn
// xxxxnnnnmmmmxxxx
#define SH4ASM_BIN_DEF_FRM_ARN(op, val)                                 \
    static inline void sh4asm_bin_##op##_frm_arn(sh4asm_bin_emit_handler_func em, \
                                                 unsigned frm, unsigned rn) { \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblerm_rn((val), frm, rn)); \
    }

// OP FRm, @-Rn
// xxxxnnnnmmmmxxxx
#define SH4ASM_BIN_DEF_FRM_AMRN(op, val)                                \
    static inline void sh4asm_bin_##op##_frm_amrn(sh4asm_bin_emit_handler_func em, \
                                                  unsigned frm, unsigned rn) { \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblerm_rn((val), frm, rn)); \
    }

// OP FRm, @(REG, Rn)
// xxxxnnnnmmmmxxxx
#define SH4ASM_BIN_DEF_FRM_A_REG_RN(op, val, reg)                       \
    static inline void                                                  \
    sh4asm_bin_##op##_frm_a_##reg##_rn(sh4asm_bin_emit_handler_func em, \
                                       unsigned frm, unsigned rn) {     \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblerm_rn((val), frm, rn)); \
    }

// OP REG, FRM, FRN
// xxxxnnnnmmmmxxxx
#define SH4ASM_BIN_DEF_REG_FRM_FRN(op, val, reg)                        \
    static inline void                                                  \
    sh4asm_bin_##op##_##reg##_frm_frn(sh4asm_bin_emit_handler_func em,  \
                                      unsigned frm, unsigned frn) {     \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblerm_rn((val), frm, frn)); \
    }

// OP Rm, Rn_BANK
#define SH4ASM_BIN_DEF_RM_RN_BANK(op, val)                              \
    static inline void                                                  \
    sh4asm_bin_##op##_rm_rn_bank(sh4asm_bin_emit_handler_func em,       \
                                 unsigned rm, unsigned rn_bank) {       \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblerm_rn_bank((val), rm, rn_bank)); \
    }

// OP Rm, Rn_BANK
#define SH4ASM_BIN_DEF_RM_BANK_RN(op, val)                              \
    static inline void                                                  \
    sh4asm_bin_##op##_rm_bank_rn(sh4asm_bin_emit_handler_func em,       \
                                 unsigned rm_bank, unsigned rn) {       \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblerm_rn_bank((val), rn, rm_bank)); \
    }

// OP @Rm+, Rn_BANK
#define SH4ASM_BIN_DEF_ARMP_RN_BANK(op, val)                            \
    static inline void                                                  \
    sh4asm_bin_##op##_armp_rn_bank(sh4asm_bin_emit_handler_func em,     \
                                   unsigned rm, unsigned rn_bank) {     \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblerm_rn_bank((val), rm, rn_bank)); \
    }

// OP @Rm_BANK, @-Rn
#define SH4ASM_BIN_DEF_RM_BANK_AMRN(op, val)                            \
    static inline void                                                  \
    sh4asm_bin_##op##_rm_bank_amrn(sh4asm_bin_emit_handler_func em,     \
                                   unsigned rm_bank, unsigned rn) {     \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblerm_rn_bank((val), rn, rm_bank)); \
    }

// OP REG, @(disp4, Rn)
// xxxxxxxxnnnndddd
#define SH4ASM_BIN_DEF_REG_A_DISP4_RN(op, val, reg, disp_shift)         \
    static inline void                                                  \
    sh4asm_bin_##op##_##reg##_a_disp4_rn(sh4asm_bin_emit_handler_func em, \
                                         unsigned disp4, unsigned rn) { \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblern_imm4((val), rn, \
                                                              disp4 >> (disp_shift))); \
    }

// OP @(disp4, Rm), REG
#define SH4ASM_BIN_DEF_A_DISP4_RM_REG(op, val, reg, disp_shift)         \
    static inline void                                                  \
    sh4asm_bin_##op##_a_disp4_rm_##reg(sh4asm_bin_emit_handler_func em, \
                                       unsigned disp4, unsigned rm) {   \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblern_imm4((val), rm, \
                                                              disp4 >> (disp_shift))); \
    }

// OP Rm, @(disp4, Rn)
// xxxxnnnnmmmmdddd
#define SH4ASM_BIN_DEF_RM_A_DISP4_RN(op, val, disp_shift)               \
    static inline void                                                  \
    sh4asm_bin_##op##_rm_a_disp4_rn(sh4asm_bin_emit_handler_func em,    \
                                    unsigned rm, unsigned disp4, unsigned rn) { \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblerm_rn_imm4((val), rm, rn, \
                                                                 disp4 >> (disp_shift))); \
    }

// OP @(disp4, Rm), Rn
// xxxxnnnnmmmmdddd
#define SH4ASM_BIN_DEF_A_DISP4_RM_RN(op, val, disp_shift)               \
    static inline void                                                  \
    sh4asm_bin_##op##_a_disp4_rm_rn(sh4asm_bin_emit_handler_func em,    \
                                    unsigned disp4, unsigned rm, unsigned rn) { \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblerm_rn_imm4((val), rm, rn, \
                                                                 disp4 >> (disp_shift))); \
    }

// OP DRm, DRn
// xxxxnnnxmmmxxxxx
#define SH4ASM_BIN_DEF_DRM_DRN(op, val)                                 \
    static inline void                                                  \
    sh4asm_bin_##op##_drm_drn(sh4asm_bin_emit_handler_func em,          \
                              unsigned drm, unsigned drn) {             \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assembledrm_drn((val), drm, drn)); \
    }

// OP DRm, XDn
// xxxxnnnxmmmxxxxx
#define SH4ASM_BIN_DEF_DRM_XDN(op, val)                                 \
    static inline void                                                  \
    sh4asm_bin_##op##_drm_xdn(sh4asm_bin_emit_handler_func em,          \
                              unsigned drm, unsigned xdn) {             \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assembledrm_drn((val), drm, xdn)); \
    }

// OP XDm, DRn
// xxxxnnnxmmmxxxxx
#define SH4ASM_BIN_DEF_XDM_DRN(op, val)                                 \
    static inline void                                                  \
    sh4asm_bin_##op##_xdm_drn(sh4asm_bin_emit_handler_func em,          \
                              unsigned xdm, unsigned drn) {             \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assembledrm_drn((val), xdm, drn)); \
    }

// OP XDm, XDn
// xxxxnnnxmmmxxxxx
#define SH4ASM_BIN_DEF_XDM_XDN(op, val)                                 \
    static inline void                                                  \
    sh4asm_bin_##op##_xdm_xdn(sh4asm_bin_emit_handler_func em,          \
                              unsigned xdm, unsigned xdn) {             \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assembledrm_drn((val), xdm, xdn)); \
    }

// OP @Rm, DRn
#define SH4ASM_BIN_DEF_ARM_DRN(op, val)                                 \
    static inline void                                                  \
    sh4asm_bin_##op##_arm_drn(sh4asm_bin_emit_handler_func em,          \
                              unsigned rm, unsigned drn) {              \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblerm_drn((val), rm, drn)); \
    }

// OP @(REG, Rm), DRn
#define SH4ASM_BIN_DEF_A_REG_RM_DRN(op, val, reg)                       \
    static inline void                                                  \
    sh4asm_bin_##op##_a_##reg##_rm_drn(sh4asm_bin_emit_handler_func em, \
                                       unsigned rm, unsigned drn) {     \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblerm_drn((val), rm, drn)); \
    }

// OP @Rm+, DRn
#define SH4ASM_BIN_DEF_ARMP_DRN(op, val)                                \
    static inline void                                                  \
    sh4asm_bin_##op##_armp_drn(sh4asm_bin_emit_handler_func em,         \
                               unsigned rm, unsigned drn) {             \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblerm_drn((val), rm, drn)); \
    }

// OP @Rm, XDn
#define SH4ASM_BIN_DEF_ARM_XDN(op, val)                                 \
    static inline void                                                  \
    sh4asm_bin_##op##_arm_xdn(sh4asm_bin_emit_handler_func em,          \
                              unsigned rm, unsigned xdn) {              \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblerm_drn((val), rm, xdn)); \
    }

// OP @Rm+, XDn
#define SH4ASM_BIN_DEF_ARMP_XDN(op, val)                                \
    static inline void                                                  \
    sh4asm_bin_##op##_armp_xdn(sh4asm_bin_emit_handler_func em,         \
                               unsigned rm, unsigned xdn) {             \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblerm_drn((val), rm, xdn)); \
    }

// OP @(REG, Rm), XDn
#define SH4ASM_BIN_DEF_A_REG_RM_XDN(op, val, reg)                       \
    static inline void                                                  \
    sh4asm_bin_##op##_a_##reg##_rm_xdn(sh4asm_bin_emit_handler_func em, \
                                       unsigned rm, unsigned xdn) {     \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblerm_drn((val), rm, xdn)); \
    }

// OP DRm, @Rn
#define SH4ASM_BIN_DEF_DRM_ARN(op, val)                                 \
    static inline void                                                  \
    sh4asm_bin_##op##_drm_arn(sh4asm_bin_emit_handler_func em,          \
                              unsigned drm, unsigned rn) {              \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assembledrm_rn((val), drm, rn)); \
    }

// OP DRm, @-Rn
#define SH4ASM_BIN_DEF_DRM_AMRN(op, val)                                \
    static inline void                                                  \
    sh4asm_bin_##op##_drm_amrn(sh4asm_bin_emit_handler_func em,         \
                               unsigned drm, unsigned rn) {             \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assembledrm_rn((val), drm, rn)); \
    }

// OP DRm, @(REG, Rn)
#define SH4ASM_BIN_DEF_DRM_A_REG_RN(op, val, reg)                       \
    static inline void                                                  \
    sh4asm_bin_##op##_drm_a_##reg##_rn(sh4asm_bin_emit_handler_func em, \
                                       unsigned drm, unsigned rn) {     \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assembledrm_rn((val), drm, rn)); \
    }

// OP XDm, @Rn
#define SH4ASM_BIN_DEF_XDM_ARN(op, val)                                 \
    static inline void                                                  \
    sh4asm_bin_##op##_xdm_arn(sh4asm_bin_emit_handler_func em,          \
                              unsigned xdm, unsigned rn) {              \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assembledrm_rn((val), xdm, rn)); \
    }

// OP XDm, @-Rn
#define SH4ASM_BIN_DEF_XDM_AMRN(op, val)                                \
    static inline void                                                  \
    sh4asm_bin_##op##_xdm_amrn(sh4asm_bin_emit_handler_func em,         \
                               unsigned xdm, unsigned rn) {             \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assembledrm_rn((val), xdm, rn)); \
    }

// OP XDm, @(REG, Rn)
#define SH4ASM_BIN_DEF_XDM_A_REG_RN(op, val, reg)                       \
    static inline void                                                  \
    sh4asm_bin_##op##_xdm_a_##reg##_rn(sh4asm_bin_emit_handler_func em, \
                                       unsigned xdm, unsigned rn) {     \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assembledrm_rn((val), xdm, rn)); \
    }

// OP DRn
#define SH4ASM_BIN_DEF_DRN(op, val)                                     \
    static inline void                                                  \
    sh4asm_bin_##op##_drn(sh4asm_bin_emit_handler_func em, unsigned drn) { \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assembledrn((val), drn)); \
    }

// OP DRm, REG
#define SH4ASM_BIN_DEF_DRM_REG(op, val, reg)                            \
    static inline void                                                  \
    sh4asm_bin_##op##_drm_##reg(sh4asm_bin_emit_handler_func em, unsigned drm) { \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assembledrn((val), drm)); \
    }

// OP REG, DRn
#define SH4ASM_BIN_DEF_REG_DRN(op, val, reg)                            \
    static inline void                                                  \
    sh4asm_bin_##op##_##reg##_drn(sh4asm_bin_emit_handler_func em, unsigned drn) { \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assembledrn((val), drn)); \
    }

// OP FVm, FVn
#define SH4ASM_BIN_DEF_FVM_FVN(op, val)                                 \
    static inline void                                                  \
    sh4asm_bin_##op##_fvm_fvn(sh4asm_bin_emit_handler_func em,          \
                              unsigned fvm, unsigned fvn) {             \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblefvm_fvn((val), fvm, fvn)); \
    }

// OP REG, FVn
// xxxxnnxxxxxxxxxx
#define SH4ASM_BIN_DEF_REG_FVN(op, val, reg)                            \
    static inline void                                                  \
    sh4asm_bin_##op##_##reg##_fvn(sh4asm_bin_emit_handler_func em, unsigned fvn) { \
        sh4asm_bin_emit_inst((em), sh4asm_bin_assemblefvn((val), fvn)); \
    }

// opcodes which take no arguments (noarg)
SH4ASM_BIN_DEF_NOARG(div0u, SH4ASM_OPCODE_DIV0U)
SH4ASM_BIN_DEF_NOARG(rts, SH4ASM_OPCODE_RTS)
SH4ASM_BIN_DEF_NOARG(clrmac, SH4ASM_OPCODE_CLRMAC)
SH4ASM_BIN_DEF_NOARG(clrs, SH4ASM_OPCODE_CLRS)
SH4ASM_BIN_DEF_NOARG(clrt, SH4ASM_OPCODE_CLRT)
SH4ASM_BIN_DEF_NOARG(ldtlb, SH4ASM_OPCODE_LDTLB)
SH4ASM_BIN_DEF_NOARG(nop, SH4ASM_OPCODE_NOP)
SH4ASM_BIN_DEF_NOARG(rte, SH4ASM_OPCODE_RTE)
SH4ASM_BIN_DEF_NOARG(sets, SH4ASM_OPCODE_SETS)
SH4ASM_BIN_DEF_NOARG(sett, SH4ASM_OPCODE_SETT)
SH4ASM_BIN_DEF_NOARG(sleep, SH4ASM_OPCODE_SLEEP)
SH4ASM_BIN_DEF_NOARG(frchg, SH4ASM_OPCODE_FRCHG)
SH4ASM_BIN_DEF_NOARG(fschg, SH4ASM_OPCODE_FSCHG)

SH4ASM_BIN_DEF_RN(movt, SH4ASM_OPCODE_MOVT_RN)
SH4ASM_BIN_DEF_RN(cmppz, SH4ASM_OPCODE_CMPPZ_RN)
SH4ASM_BIN_DEF_RN(cmppl, SH4ASM_OPCODE_CMPPL_RN)
SH4ASM_BIN_DEF_RN(dt, SH4ASM_OPCODE_DT_RN)
SH4ASM_BIN_DEF_RN(rotl, SH4ASM_OPCODE_ROTL_RN)
SH4ASM_BIN_DEF_RN(rotr, SH4ASM_OPCODE_ROTR_RN)
SH4ASM_BIN_DEF_RN(rotcl, SH4ASM_OPCODE_ROTCL_RN)
SH4ASM_BIN_DEF_RN(rotcr, SH4ASM_OPCODE_ROTCR_RN)
SH4ASM_BIN_DEF_RN(shal, SH4ASM_OPCODE_SHAL_RN)
SH4ASM_BIN_DEF_RN(shar, SH4ASM_OPCODE_SHAR_RN)
SH4ASM_BIN_DEF_RN(shll, SH4ASM_OPCODE_SHLL_RN)
SH4ASM_BIN_DEF_RN(shlr, SH4ASM_OPCODE_SHLR_RN)
SH4ASM_BIN_DEF_RN(shll2, SH4ASM_OPCODE_SHLL2_RN)
SH4ASM_BIN_DEF_RN(shlr2, SH4ASM_OPCODE_SHLR2_RN)
SH4ASM_BIN_DEF_RN(shll8, SH4ASM_OPCODE_SHLL8_RN)
SH4ASM_BIN_DEF_RN(shlr8, SH4ASM_OPCODE_SHLR8_RN)
SH4ASM_BIN_DEF_RN(shll16, SH4ASM_OPCODE_SHLL16_RN)
SH4ASM_BIN_DEF_RN(shlr16, SH4ASM_OPCODE_SHLR16_RN)
SH4ASM_BIN_DEF_RN(braf, SH4ASM_OPCODE_BRAF_RN)
SH4ASM_BIN_DEF_RN(bsrf, SH4ASM_OPCODE_BSRF_RN)

SH4ASM_BIN_DEF_ARN(tasb, SH4ASM_OPCODE_TASB_ARN)
SH4ASM_BIN_DEF_ARN(ocbi, SH4ASM_OPCODE_OCBI_ARN)
SH4ASM_BIN_DEF_ARN(ocbp, SH4ASM_OPCODE_OCBP_ARN)
SH4ASM_BIN_DEF_ARN(ocbwb, SH4ASM_OPCODE_OCBWB_ARN)
SH4ASM_BIN_DEF_ARN(pref, SH4ASM_OPCODE_PREF_ARN)
SH4ASM_BIN_DEF_ARN(jmp, SH4ASM_OPCODE_JMP_ARN)
SH4ASM_BIN_DEF_ARN(jsr, SH4ASM_OPCODE_JSR_ARN)

SH4ASM_BIN_DEF_RM_REG(ldc, SH4ASM_OPCODE_LDC_RM_SR, sr)
SH4ASM_BIN_DEF_RM_REG(ldc, SH4ASM_OPCODE_LDC_RM_GBR, gbr)
SH4ASM_BIN_DEF_RM_REG(ldc, SH4ASM_OPCODE_LDC_RM_VBR, vbr)
SH4ASM_BIN_DEF_RM_REG(ldc, SH4ASM_OPCODE_LDC_RM_SSR, ssr)
SH4ASM_BIN_DEF_RM_REG(ldc, SH4ASM_OPCODE_LDC_RM_SPC, spc)
SH4ASM_BIN_DEF_RM_REG(ldc, SH4ASM_OPCODE_LDC_RM_DBR, dbr)
SH4ASM_BIN_DEF_RM_REG(lds, SH4ASM_OPCODE_LDS_RM_MACH, mach)
SH4ASM_BIN_DEF_RM_REG(lds, SH4ASM_OPCODE_LDS_RM_MACL, macl)
SH4ASM_BIN_DEF_RM_REG(lds, SH4ASM_OPCODE_LDS_RM_PR, pr)
SH4ASM_BIN_DEF_RM_REG(lds, SH4ASM_OPCODE_LDS_RM_FPSCR, fpscr)
SH4ASM_BIN_DEF_RM_REG(lds, SH4ASM_OPCODE_LDS_RM_FPUL, fpul)

SH4ASM_BIN_DEF_REG_RN(stc, SH4ASM_OPCODE_STC_SR_RN, sr)
SH4ASM_BIN_DEF_REG_RN(stc, SH4ASM_OPCODE_STC_GBR_RN, gbr)
SH4ASM_BIN_DEF_REG_RN(stc, SH4ASM_OPCODE_STC_VBR_RN, vbr)
SH4ASM_BIN_DEF_REG_RN(stc, SH4ASM_OPCODE_STC_SSR_RN, ssr)
SH4ASM_BIN_DEF_REG_RN(stc, SH4ASM_OPCODE_STC_SPC_RN, spc)
SH4ASM_BIN_DEF_REG_RN(stc, SH4ASM_OPCODE_STC_SGR_RN, sgr)
SH4ASM_BIN_DEF_REG_RN(stc, SH4ASM_OPCODE_STC_DBR_RN, dbr)
SH4ASM_BIN_DEF_REG_RN(sts, SH4ASM_OPCODE_STS_MACH_RN, mach)
SH4ASM_BIN_DEF_REG_RN(sts, SH4ASM_OPCODE_STS_MACL_RN, macl)
SH4ASM_BIN_DEF_REG_RN(sts, SH4ASM_OPCODE_STS_PR_RN, pr)
SH4ASM_BIN_DEF_REG_RN(sts, SH4ASM_OPCODE_STS_FPSCR_RN, fpscr)
SH4ASM_BIN_DEF_REG_RN(sts, SH4ASM_OPCODE_STS_FPUL_RN, fpul)

SH4ASM_BIN_DEF_ARMP_REG(ldcl, SH4ASM_OPCODE_LDCL_ARMP_SR, sr)
SH4ASM_BIN_DEF_ARMP_REG(ldcl, SH4ASM_OPCODE_LDCL_ARMP_GBR, gbr)
SH4ASM_BIN_DEF_ARMP_REG(ldcl, SH4ASM_OPCODE_LDCL_ARMP_VBR, vbr)
SH4ASM_BIN_DEF_ARMP_REG(ldcl, SH4ASM_OPCODE_LDCL_ARMP_SSR, ssr)
SH4ASM_BIN_DEF_ARMP_REG(ldcl, SH4ASM_OPCODE_LDCL_ARMP_SPC, spc)
SH4ASM_BIN_DEF_ARMP_REG(ldcl, SH4ASM_OPCODE_LDCL_ARMP_DBR, dbr)
SH4ASM_BIN_DEF_ARMP_REG(ldsl, SH4ASM_OPCODE_LDSL_ARMP_MACH, mach)
SH4ASM_BIN_DEF_ARMP_REG(ldsl, SH4ASM_OPCODE_LDSL_ARMP_MACL, macl)
SH4ASM_BIN_DEF_ARMP_REG(ldsl, SH4ASM_OPCODE_LDSL_ARMP_PR, pr)
SH4ASM_BIN_DEF_ARMP_REG(ldsl, SH4ASM_OPCODE_LDSL_ARMP_FPSCR, fpscr)
SH4ASM_BIN_DEF_ARMP_REG(ldsl, SH4ASM_OPCODE_LDSL_ARMP_FPUL, fpul)

SH4ASM_BIN_DEF_REG_AMRN(stcl, SH4ASM_OPCODE_STCL_SR_AMRN, sr)
SH4ASM_BIN_DEF_REG_AMRN(stcl, SH4ASM_OPCODE_STCL_GBR_AMRN, gbr)
SH4ASM_BIN_DEF_REG_AMRN(stcl, SH4ASM_OPCODE_STCL_VBR_AMRN, vbr)
SH4ASM_BIN_DEF_REG_AMRN(stcl, SH4ASM_OPCODE_STCL_SSR_AMRN, ssr)
SH4ASM_BIN_DEF_REG_AMRN(stcl, SH4ASM_OPCODE_STCL_SPC_AMRN, spc)
SH4ASM_BIN_DEF_REG_AMRN(stcl, SH4ASM_OPCODE_STCL_SGR_AMRN, sgr)
SH4ASM_BIN_DEF_REG_AMRN(stcl, SH4ASM_OPCODE_STCL_DBR_AMRN, dbr)
SH4ASM_BIN_DEF_REG_AMRN(stsl, SH4ASM_OPCODE_STSL_MACH_AMRN, mach)
SH4ASM_BIN_DEF_REG_AMRN(stsl, SH4ASM_OPCODE_STSL_MACL_AMRN, macl)
SH4ASM_BIN_DEF_REG_AMRN(stsl, SH4ASM_OPCODE_STSL_PR_AMRN, pr)
SH4ASM_BIN_DEF_REG_AMRN(stsl, SH4ASM_OPCODE_STSL_FPSCR_AMRN, fpscr)
SH4ASM_BIN_DEF_REG_AMRN(stsl, SH4ASM_OPCODE_STSL_FPUL_AMRN, fpul)

SH4ASM_BIN_DEF_REG_ARN(movcal, SH4ASM_OPCODE_MOVCAL_R0_ARN, r0)

SH4ASM_BIN_DEF_FRN(fldi0, SH4ASM_OPCODE_FLDI0_FRN)
SH4ASM_BIN_DEF_FRN(fldi1, SH4ASM_OPCODE_FLDI1_FRN)
SH4ASM_BIN_DEF_FRN(fabs, SH4ASM_OPCODE_FABS_FRN)
SH4ASM_BIN_DEF_FRN(fneg, SH4ASM_OPCODE_FNEG_FRN)
SH4ASM_BIN_DEF_FRN(fsqrt, SH4ASM_OPCODE_FSQRT_FRN)
SH4ASM_BIN_DEF_FRN(fsrra, SH4ASM_OPCODE_FSRRA_FRN)

SH4ASM_BIN_DEF_FRM_REG(flds, SH4ASM_OPCODE_FLDS_FRM_FPUL, fpul)
SH4ASM_BIN_DEF_FRM_REG(ftrc, SH4ASM_OPCODE_FTRC_FRM_FPUL, fpul)

SH4ASM_BIN_DEF_REG_FRN(fsts, SH4ASM_OPCODE_FSTS_FPUL_FRN, fpul)
SH4ASM_BIN_DEF_REG_FRN(float, SH4ASM_OPCODE_FLOAT_FPUL_FRN, fpul)

SH4ASM_BIN_DEF_IMM8_REG(cmpeq, SH4ASM_OPCODE_CMPEQ_IMM8_R0, r0)
SH4ASM_BIN_DEF_IMM8_REG(and, SH4ASM_OPCODE_AND_IMM8_R0, r0)
SH4ASM_BIN_DEF_IMM8_REG(or, SH4ASM_OPCODE_OR_IMM8_R0, r0)
SH4ASM_BIN_DEF_IMM8_REG(tst, SH4ASM_OPCODE_TST_IMM8_R0, r0)
SH4ASM_BIN_DEF_IMM8_REG(xor, SH4ASM_OPCODE_XOR_IMM8_R0, r0)

SH4ASM_BIN_DEF_IMM8_A_REG_REG(andb, SH4ASM_OPCODE_ANDB_IMM8_A_R0_GBR, r0, gbr)
SH4ASM_BIN_DEF_IMM8_A_REG_REG(orb, SH4ASM_OPCODE_ORB_IMM8_A_R0_GBR, r0, gbr)
SH4ASM_BIN_DEF_IMM8_A_REG_REG(tstb, SH4ASM_OPCODE_TSTB_IMM8_A_R0_GBR, r0, gbr)
SH4ASM_BIN_DEF_IMM8_A_REG_REG(xorb, SH4ASM_OPCODE_XORB_IMM8_A_R0_GBR, r0, gbr)

SH4ASM_BIN_DEF_OFFS8(bf, SH4ASM_OPCODE_BF_DISP8, 1, 4);
SH4ASM_BIN_DEF_OFFS8(bfs, SH4ASM_OPCODE_BFS_DISP8, 1, 4)
SH4ASM_BIN_DEF_OFFS8(bt, SH4ASM_OPCODE_BT_DISP8, 1, 4)
SH4ASM_BIN_DEF_OFFS8(bts, SH4ASM_OPCODE_BTS_DISP8, 1, 4)

SH4ASM_BIN_DEF_IMM8(trapa, SH4ASM_OPCODE_TRAPA_IMM8)

SH4ASM_BIN_DEF_REG_A_DISP8_REG(movb, SH4ASM_OPCODE_MOVB_R0_A_DISP8_GBR, r0, gbr, 0)
SH4ASM_BIN_DEF_REG_A_DISP8_REG(movw, SH4ASM_OPCODE_MOVW_R0_A_DISP8_GBR, r0, gbr, 1)
SH4ASM_BIN_DEF_REG_A_DISP8_REG(movl, SH4ASM_OPCODE_MOVL_R0_A_DISP8_GBR, r0, gbr, 2)

SH4ASM_BIN_DEF_A_DISP8_REG_REG(movb, SH4ASM_OPCODE_MOVB_A_DISP8_GBR_R0, gbr, r0, 0)
SH4ASM_BIN_DEF_A_DISP8_REG_REG(movw, SH4ASM_OPCODE_MOVW_A_DISP8_GBR_R0, gbr, r0, 1)
SH4ASM_BIN_DEF_A_DISP8_REG_REG(movl, SH4ASM_OPCODE_MOVL_A_DISP8_GBR_R0, gbr, r0, 2)

SH4ASM_BIN_DEF_A_OFFS8_REG_REG(mova, SH4ASM_OPCODE_MOVA_A_DISP8_PC_R0, pc, r0, 2, 4)

SH4ASM_BIN_DEF_OFFS12(bra, SH4ASM_OPCODE_BRA_DISP12, 1, 4)
SH4ASM_BIN_DEF_OFFS12(bsr, SH4ASM_OPCODE_BSR_DISP12, 1, 4)

SH4ASM_BIN_DEF_IMM8_RN(mov, SH4ASM_OPCODE_MOV_IMM8_RN)
SH4ASM_BIN_DEF_IMM8_RN(add, SH4ASM_OPCODE_ADD_IMM8_RN)

SH4ASM_BIN_DEF_A_OFFS8_REG_RN(movw, SH4ASM_OPCODE_MOVW_A_DISP8_PC_RN, pc, 1, 4)
SH4ASM_BIN_DEF_A_OFFS8_REG_RN(movl, SH4ASM_OPCODE_MOVL_A_DISP8_PC_RN, pc, 2, 4)

SH4ASM_BIN_DEF_RM_RN(mov, SH4ASM_OPCODE_MOV_RM_RN)
SH4ASM_BIN_DEF_RM_RN(swapb, SH4ASM_OPCODE_SWAPB_RM_RN)
SH4ASM_BIN_DEF_RM_RN(swapw, SH4ASM_OPCODE_SWAPW_RM_RN)
SH4ASM_BIN_DEF_RM_RN(xtrct, SH4ASM_OPCODE_XTRCT_RM_RN)
SH4ASM_BIN_DEF_RM_RN(add, SH4ASM_OPCODE_ADD_RM_RN)
SH4ASM_BIN_DEF_RM_RN(addc, SH4ASM_OPCODE_ADDC_RM_RN)
SH4ASM_BIN_DEF_RM_RN(addv, SH4ASM_OPCODE_ADDV_RM_RN)
SH4ASM_BIN_DEF_RM_RN(cmpeq, SH4ASM_OPCODE_CMPEQ_RM_RN)
SH4ASM_BIN_DEF_RM_RN(cmphs, SH4ASM_OPCODE_CMPHS_RM_RN)
SH4ASM_BIN_DEF_RM_RN(cmpge, SH4ASM_OPCODE_CMPGE_RM_RN)
SH4ASM_BIN_DEF_RM_RN(cmphi, SH4ASM_OPCODE_CMPHI_RM_RN)
SH4ASM_BIN_DEF_RM_RN(cmpgt, SH4ASM_OPCODE_CMPGT_RM_RN)
SH4ASM_BIN_DEF_RM_RN(cmpstr, SH4ASM_OPCODE_CMPSTR_RM_RN)
SH4ASM_BIN_DEF_RM_RN(div1, SH4ASM_OPCODE_DIV1_RM_RN)
SH4ASM_BIN_DEF_RM_RN(div0s, SH4ASM_OPCODE_DIV0S_RM_RN)
SH4ASM_BIN_DEF_RM_RN(dmulsl, SH4ASM_OPCODE_DMULSL_RM_RN)
SH4ASM_BIN_DEF_RM_RN(dmulul, SH4ASM_OPCODE_DMULUL_RM_RN)
SH4ASM_BIN_DEF_RM_RN(extsb, SH4ASM_OPCODE_EXTSB_RM_RN)
SH4ASM_BIN_DEF_RM_RN(extsw, SH4ASM_OPCODE_EXTSW_RM_RN)
SH4ASM_BIN_DEF_RM_RN(extub, SH4ASM_OPCODE_EXTUB_RM_RN)
SH4ASM_BIN_DEF_RM_RN(extuw, SH4ASM_OPCODE_EXTUW_RM_RN)
SH4ASM_BIN_DEF_RM_RN(mull, SH4ASM_OPCODE_MULL_RM_RN)
SH4ASM_BIN_DEF_RM_RN(mulsw, SH4ASM_OPCODE_MULSW_RM_RN)
SH4ASM_BIN_DEF_RM_RN(muluw, SH4ASM_OPCODE_MULUW_RM_RN)
SH4ASM_BIN_DEF_RM_RN(neg, SH4ASM_OPCODE_NEG_RM_RN)
SH4ASM_BIN_DEF_RM_RN(negc, SH4ASM_OPCODE_NEGC_RM_RN)
SH4ASM_BIN_DEF_RM_RN(sub, SH4ASM_OPCODE_SUB_RM_RN)
SH4ASM_BIN_DEF_RM_RN(subc, SH4ASM_OPCODE_SUBC_RM_RN)
SH4ASM_BIN_DEF_RM_RN(subv, SH4ASM_OPCODE_SUBV_RM_RN)
SH4ASM_BIN_DEF_RM_RN(and, SH4ASM_OPCODE_AND_RM_RN)
SH4ASM_BIN_DEF_RM_RN(not, SH4ASM_OPCODE_NOT_RM_RN)
SH4ASM_BIN_DEF_RM_RN(or, SH4ASM_OPCODE_OR_RM_RN)
SH4ASM_BIN_DEF_RM_RN(tst, SH4ASM_OPCODE_TST_RM_RN)
SH4ASM_BIN_DEF_RM_RN(xor, SH4ASM_OPCODE_XOR_RM_RN)
SH4ASM_BIN_DEF_RM_RN(shad, SH4ASM_OPCODE_SHAD_RM_RN)
SH4ASM_BIN_DEF_RM_RN(shld, SH4ASM_OPCODE_SHLD_RM_RN)

SH4ASM_BIN_DEF_RM_A_REG_RN(movb, SH4ASM_OPCODE_MOVB_RM_A_R0_RN, r0)
SH4ASM_BIN_DEF_RM_A_REG_RN(movw, SH4ASM_OPCODE_MOVW_RM_A_R0_RN, r0)
SH4ASM_BIN_DEF_RM_A_REG_RN(movl, SH4ASM_OPCODE_MOVL_RM_A_R0_RN, r0)

SH4ASM_BIN_DEF_A_REG_RM_RN(movb, SH4ASM_OPCODE_MOVB_A_R0_RM_RN, r0)
SH4ASM_BIN_DEF_A_REG_RM_RN(movw, SH4ASM_OPCODE_MOVW_A_R0_RM_RN, r0)
SH4ASM_BIN_DEF_A_REG_RM_RN(movl, SH4ASM_OPCODE_MOVL_A_R0_RM_RN, r0)

SH4ASM_BIN_DEF_RM_ARN(movb, SH4ASM_OPCODE_MOVB_RM_ARN)
SH4ASM_BIN_DEF_RM_ARN(movw, SH4ASM_OPCODE_MOVW_RM_ARN)
SH4ASM_BIN_DEF_RM_ARN(movl, SH4ASM_OPCODE_MOVL_RM_ARN)

SH4ASM_BIN_DEF_ARM_RN(movb, SH4ASM_OPCODE_MOVB_ARM_RN)
SH4ASM_BIN_DEF_ARM_RN(movw, SH4ASM_OPCODE_MOVW_ARM_RN)
SH4ASM_BIN_DEF_ARM_RN(movl, SH4ASM_OPCODE_MOVL_ARM_RN)

SH4ASM_BIN_DEF_RM_AMRN(movb, SH4ASM_OPCODE_MOVB_RM_AMRN)
SH4ASM_BIN_DEF_RM_AMRN(movw, SH4ASM_OPCODE_MOVW_RM_AMRN)
SH4ASM_BIN_DEF_RM_AMRN(movl, SH4ASM_OPCODE_MOVL_RM_AMRN)

SH4ASM_BIN_DEF_ARMP_RN(movb, SH4ASM_OPCODE_MOVB_ARMP_RN)
SH4ASM_BIN_DEF_ARMP_RN(movw, SH4ASM_OPCODE_MOVW_ARMP_RN)
SH4ASM_BIN_DEF_ARMP_RN(movl, SH4ASM_OPCODE_MOVL_ARMP_RN)

SH4ASM_BIN_DEF_ARMP_ARNP(macl, SH4ASM_OPCODE_MACL_ARMP_ARNP)
SH4ASM_BIN_DEF_ARMP_ARNP(macw, SH4ASM_OPCODE_MACW_ARMP_ARNP)

SH4ASM_BIN_DEF_FRM_FRN(fmov, SH4ASM_OPCODE_FMOV_FRM_FRN)
SH4ASM_BIN_DEF_FRM_FRN(fadd, SH4ASM_OPCODE_FADD_FRM_FRN)
SH4ASM_BIN_DEF_FRM_FRN(fcmpeq, SH4ASM_OPCODE_FCMPEQ_FRM_FRN)
SH4ASM_BIN_DEF_FRM_FRN(fcmpgt, SH4ASM_OPCODE_FCMPGT_FRM_FRN)
SH4ASM_BIN_DEF_FRM_FRN(fdiv, SH4ASM_OPCODE_FDIV_FRM_FRN)
SH4ASM_BIN_DEF_FRM_FRN(fmul, SH4ASM_OPCODE_FMUL_FRM_FRN)
SH4ASM_BIN_DEF_FRM_FRN(fsub, SH4ASM_OPCODE_FSUB_FRM_FRN)

SH4ASM_BIN_DEF_ARM_FRN(fmovs, SH4ASM_OPCODE_FMOVS_ARM_FRN)

SH4ASM_BIN_DEF_A_REG_RM_FRN(fmovs, SH4ASM_OPCODE_FMOVS_A_R0_RM_FRN, r0)

SH4ASM_BIN_DEF_ARMP_FRN(fmovs, SH4ASM_OPCODE_FMOVS_ARMP_FRN)

SH4ASM_BIN_DEF_FRM_ARN(fmovs, SH4ASM_OPCODE_FMOVS_FRM_ARN)

SH4ASM_BIN_DEF_FRM_AMRN(fmovs, SH4ASM_OPCODE_FMOVS_FRM_AMRN)

SH4ASM_BIN_DEF_FRM_A_REG_RN(fmovs, SH4ASM_OPCODE_FMOVS_FRM_A_R0_RN, r0)

SH4ASM_BIN_DEF_REG_FRM_FRN(fmac, SH4ASM_OPCODE_FMAC_FR0_FRM_FRN, fr0)

SH4ASM_BIN_DEF_RM_RN_BANK(ldc, SH4ASM_OPCODE_LDC_RM_RN_BANK)

SH4ASM_BIN_DEF_RM_BANK_RN(stc, SH4ASM_OPCODE_STC_RM_BANK_RN)

SH4ASM_BIN_DEF_ARMP_RN_BANK(ldcl, SH4ASM_OPCODE_LDCL_ARMP_RN_BANK)

SH4ASM_BIN_DEF_RM_BANK_AMRN(stcl, SH4ASM_OPCODE_STCL_RM_BANK_AMRN)

SH4ASM_BIN_DEF_REG_A_DISP4_RN(movb, SH4ASM_OPCODE_MOVB_R0_A_DISP4_RN, r0, 0)
SH4ASM_BIN_DEF_REG_A_DISP4_RN(movw, SH4ASM_OPCODE_MOVW_R0_A_DISP4_RN, r0, 1)

SH4ASM_BIN_DEF_A_DISP4_RM_REG(movb, SH4ASM_OPCODE_MOVB_A_DISP4_RM_R0, r0, 0)
SH4ASM_BIN_DEF_A_DISP4_RM_REG(movw, SH4ASM_OPCODE_MOVW_A_DISP4_RM_R0, r0, 1)

SH4ASM_BIN_DEF_RM_A_DISP4_RN(movl, SH4ASM_OPCODE_MOVL_RM_A_DISP4_RN, 2)

SH4ASM_BIN_DEF_A_DISP4_RM_RN(movl, SH4ASM_OPCODE_MOVL_A_DISP4_RM_RN, 2)

SH4ASM_BIN_DEF_DRM_DRN(fmov, SH4ASM_OPCODE_FMOV_DRM_DRN)
SH4ASM_BIN_DEF_DRM_DRN(fadd, SH4ASM_OPCODE_FADD_DRM_DRN)
SH4ASM_BIN_DEF_DRM_DRN(fcmpeq, SH4ASM_OPCODE_FCMPEQ_DRM_DRN)
SH4ASM_BIN_DEF_DRM_DRN(fcmpgt, SH4ASM_OPCODE_FCMPGT_DRM_DRN)
SH4ASM_BIN_DEF_DRM_DRN(fdiv, SH4ASM_OPCODE_FDIV_DRM_DRN)
SH4ASM_BIN_DEF_DRM_DRN(fmul, SH4ASM_OPCODE_FMUL_DRM_DRN)
SH4ASM_BIN_DEF_DRM_DRN(fsub, SH4ASM_OPCODE_FSUB_DRM_DRN)

SH4ASM_BIN_DEF_DRM_XDN(fmov, SH4ASM_OPCODE_FMOV_DRM_XDN)

SH4ASM_BIN_DEF_XDM_DRN(fmov, SH4ASM_OPCODE_FMOV_XDM_DRN)

SH4ASM_BIN_DEF_XDM_XDN(fmov, SH4ASM_OPCODE_FMOV_XDM_XDN)

SH4ASM_BIN_DEF_ARM_DRN(fmov, SH4ASM_OPCODE_FMOV_ARM_DRN)

SH4ASM_BIN_DEF_A_REG_RM_DRN(fmov, SH4ASM_OPCODE_FMOV_A_R0_RM_DRN, r0)

SH4ASM_BIN_DEF_ARMP_DRN(fmov, SH4ASM_OPCODE_FMOV_ARMP_DRN)

SH4ASM_BIN_DEF_ARM_XDN(fmov, SH4ASM_OPCODE_FMOV_ARM_XDN)

SH4ASM_BIN_DEF_ARMP_XDN(fmov, SH4ASM_OPCODE_FMOV_ARMP_XDN)

SH4ASM_BIN_DEF_A_REG_RM_XDN(fmov, SH4ASM_OPCODE_FMOV_A_R0_RM_XDN, r0)

SH4ASM_BIN_DEF_DRM_ARN(fmov, SH4ASM_OPCODE_FMOV_DRM_ARN)

SH4ASM_BIN_DEF_DRM_AMRN(fmov, SH4ASM_OPCODE_FMOV_DRM_AMRN)

SH4ASM_BIN_DEF_DRM_A_REG_RN(fmov, SH4ASM_OPCODE_FMOV_DRM_A_R0_RN, r0)

SH4ASM_BIN_DEF_XDM_ARN(fmov, SH4ASM_OPCODE_FMOV_XDM_ARN)

SH4ASM_BIN_DEF_XDM_AMRN(fmov, SH4ASM_OPCODE_FMOV_XDM_AMRN)

SH4ASM_BIN_DEF_XDM_A_REG_RN(fmov, SH4ASM_OPCODE_FMOV_XDM_A_R0_RN, r0)

SH4ASM_BIN_DEF_DRN(fabs, SH4ASM_OPCODE_FABS_DRN)
SH4ASM_BIN_DEF_DRN(fneg, SH4ASM_OPCODE_FNEG_DRN)
SH4ASM_BIN_DEF_DRN(fsqrt, SH4ASM_OPCODE_FSQRT_DRN)

SH4ASM_BIN_DEF_DRM_REG(fcnvds, SH4ASM_OPCODE_FCNVDS_DRM_FPUL, fpul)
SH4ASM_BIN_DEF_DRM_REG(ftrc, SH4ASM_OPCODE_FTRC_DRM_FPUL, fpul)

SH4ASM_BIN_DEF_REG_DRN(fcnvsd, SH4ASM_OPCODE_FCNVSD_FPUL_DRN, fpul)
SH4ASM_BIN_DEF_REG_DRN(float, SH4ASM_OPCODE_FLOAT_FPUL_DRN, fpul)
SH4ASM_BIN_DEF_REG_DRN(fsca, SH4ASM_OPCODE_FSCA_FPUL_DRN, fpul)

SH4ASM_BIN_DEF_FVM_FVN(fipr, SH4ASM_OPCODE_FIPR_FVM_FVN)

SH4ASM_BIN_DEF_REG_FVN(ftrv, SH4ASM_OPCODE_FTRV_XMTRX_FVN, xmtrx)

#ifdef __cplusplus
}
#endif

#endif
