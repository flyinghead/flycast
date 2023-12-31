/*******************************************************************************
 *
 * Copyright (c) 2017, 2019, 2020 snickerbockers <snickerbockers@washemu.org>
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

#ifndef SH4ASM_TXT_EMIT_H_
#define SH4ASM_TXT_EMIT_H_

#include <stdio.h>
#include <stdint.h>

typedef void(*sh4asm_txt_emit_handler_func)(char);

static void
sh4asm_txt_emit_str(sh4asm_txt_emit_handler_func em, char const *txt) {
    while (*txt)
        em(*txt++);
}

static char const *sh4asm_gen_reg_str(unsigned idx) {
    static char const* names[16] = {
        "r0", "r1",   "r2",  "r3",
        "r4", "r5",   "r6",  "r7",
        "r8", "r9",   "r10", "r11",
        "r12", "r13", "r14", "r15"
    };
    return names[idx & 15];
}

static char const *sh4asm_bank_reg_str(unsigned idx) {
    static char const *names[16] = {
        "r0_bank",  "r1_bank",  "r2_bank",  "r3_bank",
        "r4_bank",  "r5_bank",  "r6_bank",  "r7_bank",
        "r8_bank",  "r9_bank",  "r10_bank", "r11_bank",
        "r12_bank", "r13_bank", "r14_bank", "r15_bank"
    };
    return names[idx & 15];
}

static char const *sh4asm_fr_reg_str(unsigned idx) {
    static char const *names[16] = {
        "fr0",  "fr1",  "fr2",  "fr3",
        "fr4",  "fr5",  "fr6",  "fr7",
        "fr8",  "fr9",  "fr10", "fr11",
        "fr12", "fr13", "fr14", "fr15"
    };
    return names[idx & 15];
}

static char const *sh4asm_dr_reg_str(unsigned idx) {
    static char const *names[8] = {
        "dr0", "dr2", "dr4", "dr6", "dr8", "dr10", "dr12", "dr14"
    };
    return names[(idx >> 1) & 7];
}

static char const *sh4asm_xd_reg_str(unsigned idx) {
    static char const *names[8] = {
        "xd0", "xd2", "xd4", "xd6", "xd8", "xd10", "xd12", "xd14"
    };
    return names[(idx >> 1) & 7];
}

static char const *sh4asm_fv_reg_str(unsigned idx) {
    static char const *names[4] = {
        "fv0", "fv4", "fv8", "fv12"
    };
    return names[(idx >> 2) & 3];
}

static char const *sh4asm_imm8_str(unsigned imm8, unsigned shift) {
    // TODO: pad output to two digits
    static char buf[8];
    snprintf(buf, sizeof(buf), "0x%x", //"0x%02x",
             imm8 & ((256 << shift) - 1) & ~((1 << shift) - 1));
    buf[7] = '\0';
    return buf;
}

static char const *sh4asm_imm12_str(int imm12, uint32_t pc) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "0x%08x", (int)(pc + imm12));
    buf[31] = '\0';
    return buf;
}

static char const *sh4asm_disp8_pc_str(int disp8, uint32_t pc) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "0x%08x", disp8);
    buf[31] = '\0';
    return buf;
}

static char const *sh4asm_disp8_pc_comment(int disp8, uint32_t pc) {
    static char buf[32];
    snprintf(buf, sizeof(buf), " ! 0x%08x", (int)(pc + disp8));
    buf[31] = '\0';
    return buf;
}

static char const *sh4asm_disp4_str(unsigned disp4, unsigned shift) {
    // convert to hex
    static char buf[8];
    snprintf(buf, sizeof(buf), "%d", //"0x%x",
             disp4 & ((16 << shift) - 1) & ~((1 << shift)-1));
    buf[7] = '\0';
    return buf;
}

static char const *sh4asm_disp8_str(unsigned disp8, unsigned shift) {
    // TODO: pad output to two hex digits
    static char buf[8];
    snprintf(buf, sizeof(buf), "%d", //"0x%02x",
             disp8 & ((256 << shift) - 1) & ~((1 << shift)-1));
    buf[7] = '\0';
    return buf;
}

// OP
#define SH4ASM_DEF_TXT_NOARG(op, lit)                                          \
    static inline void sh4asm_txt_##op(sh4asm_txt_emit_handler_func em) { \
        sh4asm_txt_emit_str(em, lit);                                              \
    }

// OP Rn
#define SH4ASM_DEF_TXT_RN(op, lit)                                             \
    static inline void sh4asm_txt_##op##_rn(sh4asm_txt_emit_handler_func em, \
                                            unsigned rn) {              \
        sh4asm_txt_emit_str(em, lit);                                              \
        sh4asm_txt_emit_str(em, " ");                                              \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rn));                           \
    }

// OP Rm, REG
#define SH4ASM_DEF_TXT_RM_REG(op, lit, reg)                                    \
    static inline void sh4asm_txt_##op##_rm_##reg(sh4asm_txt_emit_handler_func em, \
                                                  unsigned rm) {        \
        sh4asm_txt_emit_str(em, lit" ");                                           \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rm));                           \
        sh4asm_txt_emit_str(em, ", " #reg);                                        \
    }

// OP REG, Rn
#define SH4ASM_DEF_TXT_REG_RN(op, lit, reg)                                    \
    static inline void sh4asm_txt_##op##_##reg##_rn(sh4asm_txt_emit_handler_func em, \
                                                    unsigned rm) {      \
        sh4asm_txt_emit_str(em, lit" " #reg ", ");                                 \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rm));                           \
    }

// OP @Rn
#define SH4ASM_DEF_TXT_ARN(op, lit)                                            \
    static inline void sh4asm_txt_##op##_arn(sh4asm_txt_emit_handler_func em, \
                                             unsigned rn) {             \
        sh4asm_txt_emit_str(em, lit " @");                                         \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rn));                           \
    }

// OP @Rm+, REG
#define SH4ASM_DEF_TXT_ARMP_REG(op, lit, reg)                                  \
    static inline void sh4asm_txt_##op##_armp_##reg(sh4asm_txt_emit_handler_func em, \
                                                    unsigned rm) {      \
        sh4asm_txt_emit_str(em, lit " @");                                         \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rm));                           \
        sh4asm_txt_emit_str(em, "+, " #reg);                                       \
    }

// OP REG, @-Rn
#define SH4ASM_DEF_TXT_REG_AMRN(op, lit, reg)                                  \
    static inline void sh4asm_txt_##op##_##reg##_amrn(sh4asm_txt_emit_handler_func em, \
                                                      unsigned rn) {    \
        sh4asm_txt_emit_str(em, lit " " #reg ", @-");                              \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rn));                           \
    }

// OP REG, @Rn
#define SH4ASM_DEF_TXT_REG_ARN(op, lit, reg)                                   \
    static inline void sh4asm_txt_##op##_##reg##_arn(sh4asm_txt_emit_handler_func em, \
                                                     unsigned rn) {     \
        sh4asm_txt_emit_str(em, lit " " #reg ", @");                               \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rn));                           \
    }

// OP FRn
#define SH4ASM_DEF_TXT_FRN(op, lit)                                            \
    static inline void sh4asm_txt_##op##_frn(sh4asm_txt_emit_handler_func em, \
                                             unsigned frn) {            \
        sh4asm_txt_emit_str(em, lit " ");                                          \
        sh4asm_txt_emit_str(em, sh4asm_fr_reg_str(frn));                           \
    }

// OP FRm, REG
#define SH4ASM_DEF_TXT_FRM_REG(op, lit, reg)                                   \
    static inline void sh4asm_txt_##op##_frm_##reg(sh4asm_txt_emit_handler_func em, \
                                                   unsigned frm) {      \
        sh4asm_txt_emit_str(em, lit " ");                                          \
        sh4asm_txt_emit_str(em, sh4asm_fr_reg_str(frm));                           \
        sh4asm_txt_emit_str(em, ", " #reg);                                        \
    }

// OP REG, FRn
#define SH4ASM_DEF_TXT_REG_FRN(op, lit, reg)                                   \
    static inline void sh4asm_txt_##op##_##reg##_frn(sh4asm_txt_emit_handler_func em, \
                                                     unsigned frn) {    \
        sh4asm_txt_emit_str(em, lit " " #reg ", ");                                \
        sh4asm_txt_emit_str(em, sh4asm_fr_reg_str(frn));                           \
    }

// OP #imm8, REG
#define SH4ASM_DEF_TXT_IMM8_REG(op, lit, reg, imm_shift)                       \
    static inline void sh4asm_txt_##op##_imm8_##reg(sh4asm_txt_emit_handler_func em, \
                                                    unsigned imm8) {    \
        sh4asm_txt_emit_str(em, lit " #");                                         \
        sh4asm_txt_emit_str(em, sh4asm_imm8_str(imm8, imm_shift));                 \
        sh4asm_txt_emit_str(em, ", " #reg);                                        \
    }

// OP #imm8, @(REG1, REG2)
#define SH4ASM_DEF_TXT_IMM8_A_REG_REG(op, lit, reg1, reg2, imm_shift)          \
    static inline void                                                  \
    sh4asm_txt_##op##_imm8_a_##reg1##_##reg2(sh4asm_txt_emit_handler_func em, \
                                             unsigned imm8) {           \
        sh4asm_txt_emit_str(em, lit " #");                                         \
        sh4asm_txt_emit_str(em, sh4asm_imm8_str(imm8, imm_shift));                 \
        sh4asm_txt_emit_str(em, ", @(" #reg1 ", " #reg2 ")");                      \
    }

// OP REG1, @(disp8, REG2)
#define SH4ASM_DEF_TXT_REG_A_DISP8_REG(op, lit, reg1, reg2, disp_shift)        \
    static inline void                                                  \
    sh4asm_txt_##op##_##reg1##_a_disp8_##reg2(sh4asm_txt_emit_handler_func em, \
                                              unsigned disp8) {         \
        sh4asm_txt_emit_str(em, lit " " #reg1 ", @(");                             \
        sh4asm_txt_emit_str(em, sh4asm_disp8_str(disp8, disp_shift));              \
        sh4asm_txt_emit_str(em, ", " #reg2 ")");                                   \
    }

// OP @(disp8, REG1), REG2
#define SH4ASM_DEF_TXT_A_DISP8_REG1_REG2(op, lit, reg1, reg2, disp_shift)      \
    static inline void                                                  \
    sh4asm_txt_##op##_a_disp8_##reg1##_##reg2(sh4asm_txt_emit_handler_func em, \
                                              unsigned disp8) {         \
        sh4asm_txt_emit_str(em, lit " @(");                                        \
        sh4asm_txt_emit_str(em, sh4asm_disp8_str(disp8, disp_shift));              \
        sh4asm_txt_emit_str(em, ", " #reg1 "), " #reg2);                           \
    }

// OP @(disp8, PC), REG2
#define SH4ASM_DEF_TXT_A_DISP8_PC_REG2(op, lit, reg2)                   \
    static inline void                                                  \
    sh4asm_txt_##op##_a_disp8_pc_##reg2(sh4asm_txt_emit_handler_func em, \
                                        int disp8, uint32_t pc) {       \
        sh4asm_txt_emit_str(em, lit " @(");                             \
        sh4asm_txt_emit_str(em, sh4asm_disp8_pc_str(disp8, pc));        \
        sh4asm_txt_emit_str(em, ", pc), " #reg2);                       \
        sh4asm_txt_emit_str(em, sh4asm_disp8_pc_comment(disp8, pc));    \
    }

// OP disp8
#define SH4ASM_DEF_TXT_DISP8_PC(op, lit, disp_shift)                    \
    static inline void                                                  \
    sh4asm_txt_##op##_disp8_pc(sh4asm_txt_emit_handler_func em,         \
                               int disp8, uint32_t pc) {                \
        sh4asm_txt_emit_str(em, lit " ");                               \
        sh4asm_txt_emit_str(em, sh4asm_disp8_pc_str(disp8, pc));        \
        sh4asm_txt_emit_str(em, sh4asm_disp8_pc_comment(disp8, pc));    \
    }

// OP #imm8
#define SH4ASM_DEF_TXT_IMM8(op, lit, imm_shift)                                \
    static inline void sh4asm_txt_##op##_imm8(sh4asm_txt_emit_handler_func em, \
                                              unsigned imm8) {          \
        sh4asm_txt_emit_str(em, lit " #");                                         \
        sh4asm_txt_emit_str(em, sh4asm_imm8_str(imm8, imm_shift));                 \
    }

// OP offs12
#define SH4ASM_DEF_TXT_OFFS12(op, lit)                                  \
    static inline void                                                  \
    sh4asm_txt_##op##_offs12(sh4asm_txt_emit_handler_func em,           \
                             int imm12, uint32_t pc) {                  \
        sh4asm_txt_emit_str(em, lit " ");                               \
        sh4asm_txt_emit_str(em, sh4asm_imm12_str(imm12, pc));           \
    }

// OP #imm8, Rn
#define SH4ASM_DEF_TXT_IMM8_RN(op, lit, imm_shift)                             \
    static inline void sh4asm_txt_##op##_imm8_rn(sh4asm_txt_emit_handler_func em, \
                                                 unsigned imm8, unsigned rn) { \
        sh4asm_txt_emit_str(em, lit " #");                                         \
        sh4asm_txt_emit_str(em, sh4asm_imm8_str(imm8, imm_shift));                 \
        sh4asm_txt_emit_str(em, ", ");                                             \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rn));                           \
    }

// OP @(disp8, PC), Rn
#define SH4ASM_DEF_TXT_A_DISP8_PC_RN(op, lit)                           \
    static inline void                                                  \
    sh4asm_txt_##op##_a_disp8_pc_rn(sh4asm_txt_emit_handler_func em,    \
                                    int disp8, unsigned pc, unsigned rn) { \
        sh4asm_txt_emit_str(em, lit " @(");                             \
        sh4asm_txt_emit_str(em, sh4asm_disp8_pc_str(disp8, pc));        \
        sh4asm_txt_emit_str(em, ", pc), ");                             \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rn));                \
        sh4asm_txt_emit_str(em, sh4asm_disp8_pc_comment(disp8, pc));    \
    }

// OP Rm, Rn
#define SH4ASM_DEF_TXT_RM_RN(op, lit)                                          \
    static inline void sh4asm_txt_##op##_rm_rn(sh4asm_txt_emit_handler_func em, \
                                               unsigned rm, unsigned rn) { \
        sh4asm_txt_emit_str(em, lit " ");                                          \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rm));                           \
        sh4asm_txt_emit_str(em, ", ");                                             \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rn));                           \
    }

// OP Rm, Rn_BANK
#define SH4ASM_DEF_TXT_RM_RN_BANK(op, lit)                                 \
    static inline void                                              \
    sh4asm_txt_##op##_rm_rn_bank(sh4asm_txt_emit_handler_func em,   \
                                 unsigned rm, unsigned rn_bank) {   \
        sh4asm_txt_emit_str(em, lit " ");                                      \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rm));                       \
        sh4asm_txt_emit_str(em, ", ");                                         \
        sh4asm_txt_emit_str(em, sh4asm_bank_reg_str(rn_bank));                 \
    }

// OP @Rm+, Rn_BANK
#define SH4ASM_DEF_TXT_ARMP_RN_BANK(op, lit)                               \
    static inline void                                              \
    sh4asm_txt_##op##_armp_rn_bank(sh4asm_txt_emit_handler_func em, \
                                   unsigned rm, unsigned rn_bank) { \
        sh4asm_txt_emit_str(em, lit " @");                                     \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rm));                       \
        sh4asm_txt_emit_str(em, "+, ");                                        \
        sh4asm_txt_emit_str(em, sh4asm_bank_reg_str(rn_bank));                 \
    }

// OP Rm_BANK, Rn
#define SH4ASM_DEF_TXT_RM_BANK_RN(op, lit)                                 \
    static inline void                                              \
    sh4asm_txt_##op##_rm_bank_rn(sh4asm_txt_emit_handler_func em,   \
                                 unsigned rm_bank, unsigned rn) {   \
        sh4asm_txt_emit_str(em, lit " ");                                      \
        sh4asm_txt_emit_str(em, sh4asm_bank_reg_str(rm_bank));                 \
        sh4asm_txt_emit_str(em, ", ");                                         \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rn));                       \
    }

// OP Rm_BANK, @-Rn
#define SH4ASM_DEF_TXT_RM_BANK_AMRN(op, lit)                               \
    static inline void                                              \
    sh4asm_txt_##op##_rm_bank_amrn(sh4asm_txt_emit_handler_func em, \
                                   unsigned rm_bank, unsigned rn) { \
        sh4asm_txt_emit_str(em, lit " ");                                      \
        sh4asm_txt_emit_str(em, sh4asm_bank_reg_str(rm_bank));                 \
        sh4asm_txt_emit_str(em, ", @-");                                       \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rn));                       \
    }

// OP Rm, @(REG, Rn)
#define SH4ASM_DEF_TXT_RM_A_REG_RN(op, lit, reg)                               \
    static inline void                                                  \
    sh4asm_txt_##op##_rm_a_##reg##_rn(sh4asm_txt_emit_handler_func em,  \
                                      unsigned rm, unsigned rn) {       \
        sh4asm_txt_emit_str(em, lit " ");                                          \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rm));                           \
        sh4asm_txt_emit_str(em, ", @(" #reg", ");                                  \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rn));                           \
        sh4asm_txt_emit_str(em, ")");                                              \
    }

// OP @(REG, Rm), Rn
#define SH4ASM_DEF_TXT_A_REG_RM_RN(op, lit, reg)                               \
    static inline void                                                  \
    sh4asm_txt_##op##_a_##reg##_rm_rn(sh4asm_txt_emit_handler_func em,  \
                                      unsigned rm, unsigned rn) {       \
        sh4asm_txt_emit_str(em, lit " @(" #reg ", ");                              \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rm));                           \
        sh4asm_txt_emit_str(em, "), ");                                            \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rn));                           \
    }

// OP Rm, @Rn
#define SH4ASM_DEF_TXT_RM_ARN(op, lit)                                         \
    static inline void sh4asm_txt_##op##_rm_arn(sh4asm_txt_emit_handler_func em, \
                                                unsigned rm, unsigned rn) { \
        sh4asm_txt_emit_str(em, lit " ");                                          \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rm));                           \
        sh4asm_txt_emit_str(em, ", @");                                            \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rn));                           \
    }

// OP @Rm, Rn
#define SH4ASM_DEF_TXT_ARM_RN(op, lit)                                         \
    static inline void sh4asm_txt_##op##_arm_rn(sh4asm_txt_emit_handler_func em, \
                                                unsigned rm, unsigned rn) { \
        sh4asm_txt_emit_str(em, lit " @");                                         \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rm));                           \
        sh4asm_txt_emit_str(em, ", ");                                             \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rn));                           \
    }

// OP Rm, @-Rn
#define SH4ASM_DEF_TXT_RM_AMRN(op, lit)                                        \
    static inline void sh4asm_txt_##op##_rm_amrn(sh4asm_txt_emit_handler_func em, \
                                                 unsigned rm, unsigned rn) { \
        sh4asm_txt_emit_str(em, lit " ");                                          \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rm));                           \
        sh4asm_txt_emit_str(em, ", @-");                                           \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rn));                           \
    }

// OP @Rm+, Rn
#define SH4ASM_DEF_TXT_ARMP_RN(op, lit)                                        \
    static inline void sh4asm_txt_##op##_armp_rn(sh4asm_txt_emit_handler_func em, \
                                                 unsigned rm, unsigned rn) { \
        sh4asm_txt_emit_str(em, lit " @");                                         \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rm));                           \
        sh4asm_txt_emit_str(em, "+, ");                                            \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rn));                           \
    }

// OP @Rm+, @Rn+
#define SH4ASM_DEF_TXT_ARMP_ARNP(op, lit)                                      \
    static inline void sh4asm_txt_##op##_armp_arnp(sh4asm_txt_emit_handler_func em, \
                                                   unsigned rm, unsigned rn) { \
        sh4asm_txt_emit_str(em, lit " @");                                         \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rm));                           \
        sh4asm_txt_emit_str(em, "+, @");                                           \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rn));                           \
        sh4asm_txt_emit_str(em, "+");                                              \
    }

// OP FRm, FRn
#define SH4ASM_DEF_TXT_FRM_FRN(op, lit)                                        \
    static inline void sh4asm_txt_##op##_frm_frn(sh4asm_txt_emit_handler_func em, \
                                                 unsigned frm, unsigned frn) { \
        sh4asm_txt_emit_str(em, lit " ");                                          \
        sh4asm_txt_emit_str(em, sh4asm_fr_reg_str(frm));                           \
        sh4asm_txt_emit_str(em, ", ");                                             \
        sh4asm_txt_emit_str(em, sh4asm_fr_reg_str(frn));                           \
    }

// OP @Rm, FRn
#define SH4ASM_DEF_TXT_ARM_FRN(op, lit)                                        \
    static inline void sh4asm_txt_##op##_arm_frn(sh4asm_txt_emit_handler_func em, \
                                                 unsigned rm, unsigned frn) { \
        sh4asm_txt_emit_str(em, lit " @");                                         \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rm));                           \
        sh4asm_txt_emit_str(em, ", ");                                             \
        sh4asm_txt_emit_str(em, sh4asm_fr_reg_str(frn));                           \
    }

// OP @(REG, Rm), FRn
#define SH4ASM_DEF_TXT_A_REG_RM_FRN(op, lit, reg)                              \
    static inline void sh4asm_txt_##op##_a_##reg##_rm_frn(sh4asm_txt_emit_handler_func em, \
                                                          unsigned rm, unsigned frn) { \
        sh4asm_txt_emit_str(em, lit " @(" #reg ", ");                              \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rm));                           \
        sh4asm_txt_emit_str(em, "), ");                                            \
        sh4asm_txt_emit_str(em, sh4asm_fr_reg_str(frn));                           \
    }

// OP @Rm+, FRn
#define SH4ASM_DEF_TXT_ARMP_FRN(op, lit)                                       \
    static inline void sh4asm_txt_##op##_armp_frn(sh4asm_txt_emit_handler_func em, \
                                                  unsigned rm, unsigned frn) { \
        sh4asm_txt_emit_str(em, lit " @");                                         \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rm));                           \
        sh4asm_txt_emit_str(em, "+, ");                                            \
        sh4asm_txt_emit_str(em, sh4asm_fr_reg_str(frn));                           \
    }

// OP FRm, @Rn
#define SH4ASM_DEF_TXT_FRM_ARN(op, lit)                                        \
    static inline void sh4asm_txt_##op##_frm_arn(sh4asm_txt_emit_handler_func em, \
                                                 unsigned frm, unsigned rn) { \
        sh4asm_txt_emit_str(em, lit " ");                                          \
        sh4asm_txt_emit_str(em, sh4asm_fr_reg_str(frm));                           \
        sh4asm_txt_emit_str(em, ", @");                                            \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rn));                           \
    }

// OP FRm, @-Rn
#define SH4ASM_DEF_TXT_FRM_AMRN(op, lit)                                       \
    static inline void sh4asm_txt_##op##_frm_amrn(sh4asm_txt_emit_handler_func em, \
                                                  unsigned frm, unsigned rn) { \
        sh4asm_txt_emit_str(em, lit " ");                                          \
        sh4asm_txt_emit_str(em, sh4asm_fr_reg_str(frm));                           \
        sh4asm_txt_emit_str(em, ", @-");                                           \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rn));                           \
    }

// OP FRm, @(REG, Rn)
#define SH4ASM_DEF_TXT_FRM_A_REG_RN(op, lit, reg)                              \
    static inline void sh4asm_txt_##op##_frm_a_##reg##_rn(sh4asm_txt_emit_handler_func em, \
                                                          unsigned frm, unsigned rn) { \
        sh4asm_txt_emit_str(em, lit " ");                                          \
        sh4asm_txt_emit_str(em, sh4asm_fr_reg_str(frm));                           \
        sh4asm_txt_emit_str(em, ", @(" #reg ", ");                                 \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rn));                           \
        sh4asm_txt_emit_str(em, ")");                                              \
    }

// OP REG, FRm, Frn
#define SH4ASM_DEF_TXT_REG_FRM_FRN(op, lit, reg)                               \
    static inline void                                                  \
    sh4asm_txt_##op##_##reg##_frm_frn(sh4asm_txt_emit_handler_func em,  \
                                      unsigned frm, unsigned frn) {     \
        sh4asm_txt_emit_str(em, lit " " #reg ", ");                                \
        sh4asm_txt_emit_str(em, sh4asm_fr_reg_str(frm));                           \
        sh4asm_txt_emit_str(em, ", ");                                             \
        sh4asm_txt_emit_str(em, sh4asm_fr_reg_str(frn));                           \
    }

// OP REG, @(disp4, Rn)
#define SH4ASM_DEF_TXT_REG_A_DISP4_RN(op, lit, reg, disp_shift)                \
    static inline void                                                  \
    sh4asm_txt_##op##_##reg##_a_disp4_rn(sh4asm_txt_emit_handler_func em, \
                                         unsigned disp4, unsigned rn) { \
        sh4asm_txt_emit_str(em, lit " " #reg ", @(");                              \
        sh4asm_txt_emit_str(em, sh4asm_disp4_str(disp4, disp_shift));              \
        sh4asm_txt_emit_str(em, ", ");                                             \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rn));                           \
        sh4asm_txt_emit_str(em, ")");                                              \
    }

// OP @(disp4, Rm), REG
#define SH4ASM_DEF_TXT_A_DISP4_RM_REG(op, lit, reg, disp_shift)                \
    static inline void                                                  \
    sh4asm_txt_##op##_a_disp4_rm_##reg(sh4asm_txt_emit_handler_func em, \
                                       unsigned disp4, unsigned rm) {   \
        sh4asm_txt_emit_str(em, lit " @(");                                        \
        sh4asm_txt_emit_str(em, sh4asm_disp4_str(disp4, disp_shift));              \
        sh4asm_txt_emit_str(em, ", ");                                             \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rm));                           \
        sh4asm_txt_emit_str(em, "), " #reg);                                       \
    }

// OP rm, @(disp4, rn)
#define SH4ASM_DEF_TXT_RM_A_DISP4_RN(op, lit, disp_shift)                      \
    static inline void                                                  \
    sh4asm_txt_##op##_rm_a_disp4_rn(sh4asm_txt_emit_handler_func em,    \
                                    unsigned rm, unsigned disp4, unsigned rn) { \
        sh4asm_txt_emit_str(em, lit " ");                                          \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rm));                           \
        sh4asm_txt_emit_str(em, ", @(");                                           \
        sh4asm_txt_emit_str(em, sh4asm_disp4_str(disp4, disp_shift));              \
        sh4asm_txt_emit_str(em, ", ");                                             \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rn));                           \
        sh4asm_txt_emit_str(em, ")");                                              \
    }

// OP @(disp4, rm), rn
#define SH4ASM_DEF_TXT_A_DISP4_RM_RN(op, lit, disp_shift)                      \
    static inline void                                                  \
    sh4asm_txt_##op##_a_disp4_rm_rn(sh4asm_txt_emit_handler_func em,    \
                                    unsigned disp4, unsigned rm, unsigned rn) { \
        sh4asm_txt_emit_str(em, lit " @(");                                        \
        sh4asm_txt_emit_str(em, sh4asm_disp4_str(disp4, disp_shift));              \
        sh4asm_txt_emit_str(em, ", ");                                             \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rm));                           \
        sh4asm_txt_emit_str(em, "), ");                                            \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rn));                           \
    }

// OP DRm, DRn
#define SH4ASM_DEF_TXT_DRM_DRN(op, lit)                                        \
    static inline void sh4asm_txt_##op##_drm_drn(sh4asm_txt_emit_handler_func em, \
                                                 unsigned drm, unsigned drn) { \
        sh4asm_txt_emit_str(em, lit " ");                                          \
        sh4asm_txt_emit_str(em, sh4asm_dr_reg_str(drm));                           \
        sh4asm_txt_emit_str(em, ", ");                                             \
        sh4asm_txt_emit_str(em, sh4asm_dr_reg_str(drn));                           \
    }

// OP DRm, XDn
#define SH4ASM_DEF_TXT_DRM_XDN(op, lit)                                        \
    static inline void sh4asm_txt_##op##_drm_xdn(sh4asm_txt_emit_handler_func em, \
                                                 unsigned drm, unsigned xdn) { \
        sh4asm_txt_emit_str(em, lit " ");                                          \
        sh4asm_txt_emit_str(em, sh4asm_dr_reg_str(drm));                           \
        sh4asm_txt_emit_str(em, ", ");                                             \
        sh4asm_txt_emit_str(em, sh4asm_xd_reg_str(xdn));                           \
    }

// OP XDm, DRn
#define SH4ASM_DEF_TXT_XDM_DRN(op, lit)                                        \
    static inline void sh4asm_txt_##op##_xdm_drn(sh4asm_txt_emit_handler_func em, \
                                                 unsigned xdm, unsigned drn) { \
        sh4asm_txt_emit_str(em, lit " ");                                          \
        sh4asm_txt_emit_str(em, sh4asm_xd_reg_str(xdm));                           \
        sh4asm_txt_emit_str(em, ", ");                                             \
        sh4asm_txt_emit_str(em, sh4asm_dr_reg_str(drn));                           \
    }

// OP XDm, XDn
#define SH4ASM_DEF_TXT_XDM_XDN(op, lit)                                        \
    static inline void sh4asm_txt_##op##_xdm_xdn(sh4asm_txt_emit_handler_func em, \
                                                 unsigned xdm, unsigned xdn) { \
        sh4asm_txt_emit_str(em, lit " ");                                          \
        sh4asm_txt_emit_str(em, sh4asm_xd_reg_str(xdm));                           \
        sh4asm_txt_emit_str(em, ", ");                                             \
        sh4asm_txt_emit_str(em, sh4asm_xd_reg_str(xdn));                           \
    }

// OP DRm, @Rn
#define SH4ASM_DEF_TXT_DRM_ARN(op, lit)                                        \
    static inline void sh4asm_txt_##op##_drm_arn(sh4asm_txt_emit_handler_func em, \
                                                 unsigned drm, unsigned rn) { \
        sh4asm_txt_emit_str(em, lit " ");                                          \
        sh4asm_txt_emit_str(em, sh4asm_dr_reg_str(drm));                           \
        sh4asm_txt_emit_str(em, ", @");                                            \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rn));                           \
    }

// OP DRm, @-Rn
#define SH4ASM_DEF_TXT_DRM_AMRN(op, lit)                                       \
    static inline void sh4asm_txt_##op##_drm_amrn(sh4asm_txt_emit_handler_func em, \
                                                  unsigned drm, unsigned rn) { \
        sh4asm_txt_emit_str(em, lit " ");                                          \
        sh4asm_txt_emit_str(em, sh4asm_dr_reg_str(drm));                           \
        sh4asm_txt_emit_str(em, ", @-");                                           \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rn));                           \
    }

// OP DRm, @(REG, Rn)
#define SH4ASM_DEF_TXT_DRM_A_REG_RN(op, lit, reg)                              \
    static inline void                                                  \
    sh4asm_txt_##op##_drm_a_##reg##_rn(sh4asm_txt_emit_handler_func em, \
                                       unsigned drm, unsigned rn) {     \
        sh4asm_txt_emit_str(em, lit " ");                                          \
        sh4asm_txt_emit_str(em, sh4asm_dr_reg_str(drm));                           \
        sh4asm_txt_emit_str(em, ", @(" #reg ", ");                                 \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rn));                           \
        sh4asm_txt_emit_str(em, ")");                                              \
    }

// OP Xdm, @Rn
#define SH4ASM_DEF_TXT_XDM_ARN(op, lit)                                        \
    static inline void sh4asm_txt_##op##_xdm_arn(sh4asm_txt_emit_handler_func em, \
                                                 unsigned xdm, unsigned rn) { \
        sh4asm_txt_emit_str(em, lit " ");                                          \
        sh4asm_txt_emit_str(em, sh4asm_xd_reg_str(xdm));                           \
        sh4asm_txt_emit_str(em, ", @");                                            \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rn));                           \
    }

// OP Xdm, @-Rn
#define SH4ASM_DEF_TXT_XDM_AMRN(op, lit)                                       \
    static inline void sh4asm_txt_##op##_xdm_amrn(sh4asm_txt_emit_handler_func em, \
                                                  unsigned xdm, unsigned rn) { \
        sh4asm_txt_emit_str(em, lit " ");                                          \
        sh4asm_txt_emit_str(em, sh4asm_xd_reg_str(xdm));                           \
        sh4asm_txt_emit_str(em, ", @-");                                           \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rn));                           \
    }

// OP Xdm, @(REG, Rn)
#define SH4ASM_DEF_TXT_XDM_A_REG_RN(op, lit, reg)                              \
    static inline void                                                  \
    sh4asm_txt_##op##_xdm_a_##reg##_rn(sh4asm_txt_emit_handler_func em, \
                                       unsigned xdm, unsigned rn) {     \
        sh4asm_txt_emit_str(em, lit " ");                                          \
        sh4asm_txt_emit_str(em, sh4asm_xd_reg_str(xdm));                           \
        sh4asm_txt_emit_str(em, ", @(" #reg ", ");                                 \
        sh4asm_txt_emit_str(em, sh4asm_gen_reg_str(rn));                           \
        sh4asm_txt_emit_str(em, ")");                                              \
    }

// OP DRn
#define SH4ASM_DEF_TXT_DRN(op, lit)                                            \
    static inline void sh4asm_txt_##op##_drn(sh4asm_txt_emit_handler_func em, \
                                             unsigned drn) {            \
        sh4asm_txt_emit_str(em, lit " ");                                          \
        sh4asm_txt_emit_str(em, sh4asm_dr_reg_str(drn));                           \
    }

// OP DRm, REG
#define SH4ASM_DEF_TXT_DRM_REG(op, lit, reg)                                   \
    static inline void sh4asm_txt_##op##_drm_##reg(sh4asm_txt_emit_handler_func em, \
                                                   unsigned drm) {      \
        sh4asm_txt_emit_str(em, lit " ");                                          \
        sh4asm_txt_emit_str(em, sh4asm_dr_reg_str(drm));                           \
        sh4asm_txt_emit_str(em, ", " #reg);                                        \
    }

// OP REG, DRn
#define SH4ASM_DEF_TXT_REG_DRN(op, lit, reg)                                   \
    static inline void sh4asm_txt_##op##_##reg##_drn(sh4asm_txt_emit_handler_func em, \
                                                     unsigned drn) {    \
        sh4asm_txt_emit_str(em, lit " " #reg ", ");                                \
        sh4asm_txt_emit_str(em, sh4asm_dr_reg_str(drn));                           \
    }

// OP FVm, FVn
#define SH4ASM_DEF_TXT_FVM_FVN(op, lit)                                        \
    static inline void sh4asm_txt_##op##_fvm_fvn(sh4asm_txt_emit_handler_func em, \
                                                 unsigned fvm, unsigned fvn) { \
        sh4asm_txt_emit_str(em, lit " ");                                          \
        sh4asm_txt_emit_str(em, sh4asm_fv_reg_str(fvm));                           \
        sh4asm_txt_emit_str(em, ", ");                                             \
        sh4asm_txt_emit_str(em, sh4asm_fv_reg_str(fvn));                           \
    }

// OP REG, FVn
#define SH4ASM_DEF_TXT_REG_FVN(op, lit, reg)                               \
    static inline void                                              \
    sh4asm_txt_##op##_##reg##_fvn(sh4asm_txt_emit_handler_func em,  \
                                  unsigned fvn) {                   \
        sh4asm_txt_emit_str(em, lit " " #reg ", ");                            \
        sh4asm_txt_emit_str(em, sh4asm_fv_reg_str(fvn));                       \
    }

SH4ASM_DEF_TXT_NOARG(div0u, "div0u   ")
SH4ASM_DEF_TXT_NOARG(rts, "rts     ")
SH4ASM_DEF_TXT_NOARG(clrmac, "clrmac  ")
SH4ASM_DEF_TXT_NOARG(clrs, "clrs    ")
SH4ASM_DEF_TXT_NOARG(clrt, "clrt    ")
SH4ASM_DEF_TXT_NOARG(ldtlb, "ldtlb   ")
SH4ASM_DEF_TXT_NOARG(nop, "nop     ")
SH4ASM_DEF_TXT_NOARG(rte, "rte     ")
SH4ASM_DEF_TXT_NOARG(sets, "sets    ")
SH4ASM_DEF_TXT_NOARG(sett, "sett    ")
SH4ASM_DEF_TXT_NOARG(sleep, "sleep   ")
SH4ASM_DEF_TXT_NOARG(frchg, "frchg   ")
SH4ASM_DEF_TXT_NOARG(fschg, "fschg   ")

SH4ASM_DEF_TXT_RN(movt, "movt    ")
SH4ASM_DEF_TXT_RN(cmppz, "cmp/pz  ")
SH4ASM_DEF_TXT_RN(cmppl, "cmp/pl  ")
SH4ASM_DEF_TXT_RN(dt, "dt      ")
SH4ASM_DEF_TXT_RN(rotl, "rotl    ")
SH4ASM_DEF_TXT_RN(rotr, "rotr    ")
SH4ASM_DEF_TXT_RN(rotcl, "rotcl   ")
SH4ASM_DEF_TXT_RN(rotcr, "rotcr   ")
SH4ASM_DEF_TXT_RN(shal, "shal    ")
SH4ASM_DEF_TXT_RN(shar, "shar    ")
SH4ASM_DEF_TXT_RN(shll, "shll    ")
SH4ASM_DEF_TXT_RN(shlr, "shlr    ")
SH4ASM_DEF_TXT_RN(shll2, "shll2   ")
SH4ASM_DEF_TXT_RN(shlr2, "shlr2   ")
SH4ASM_DEF_TXT_RN(shll8, "shll8   ")
SH4ASM_DEF_TXT_RN(shlr8, "shlr8   ")
SH4ASM_DEF_TXT_RN(shll16, "shll16  ")
SH4ASM_DEF_TXT_RN(shlr16, "shlr16  ")
SH4ASM_DEF_TXT_RN(braf, "braf    ")
SH4ASM_DEF_TXT_RN(bsrf, "bsrf    ")

SH4ASM_DEF_TXT_ARN(tasb, "tas.b   ")
SH4ASM_DEF_TXT_ARN(ocbi, "ocbi    ")
SH4ASM_DEF_TXT_ARN(ocbp, "ocbp    ")
SH4ASM_DEF_TXT_ARN(ocbwb, "ocbwb   ")
SH4ASM_DEF_TXT_ARN(pref, "pref    ")
SH4ASM_DEF_TXT_ARN(jmp, "jmp     ")
SH4ASM_DEF_TXT_ARN(jsr, "jsr     ")

SH4ASM_DEF_TXT_RM_REG(ldc, "ldc     ", sr)
SH4ASM_DEF_TXT_RM_REG(ldc, "ldc     ", gbr)
SH4ASM_DEF_TXT_RM_REG(ldc, "ldc     ", vbr)
SH4ASM_DEF_TXT_RM_REG(ldc, "ldc     ", ssr)
SH4ASM_DEF_TXT_RM_REG(ldc, "ldc     ", spc)
SH4ASM_DEF_TXT_RM_REG(ldc, "ldc     ", dbr)
SH4ASM_DEF_TXT_RM_REG(lds, "lds     ", mach)
SH4ASM_DEF_TXT_RM_REG(lds, "lds     ", macl)
SH4ASM_DEF_TXT_RM_REG(lds, "lds     ", pr)
SH4ASM_DEF_TXT_RM_REG(lds, "lds     ", fpscr)
SH4ASM_DEF_TXT_RM_REG(lds, "lds     ", fpul)

SH4ASM_DEF_TXT_REG_RN(stc, "stc     ", sr)
SH4ASM_DEF_TXT_REG_RN(stc, "stc     ", gbr)
SH4ASM_DEF_TXT_REG_RN(stc, "stc     ", vbr)
SH4ASM_DEF_TXT_REG_RN(stc, "stc     ", ssr)
SH4ASM_DEF_TXT_REG_RN(stc, "stc     ", spc)
SH4ASM_DEF_TXT_REG_RN(stc, "stc     ", sgr)
SH4ASM_DEF_TXT_REG_RN(stc, "stc     ", dbr)
SH4ASM_DEF_TXT_REG_RN(sts, "sts     ", mach)
SH4ASM_DEF_TXT_REG_RN(sts, "sts     ", macl)
SH4ASM_DEF_TXT_REG_RN(sts, "sts     ", pr)
SH4ASM_DEF_TXT_REG_RN(sts, "sts     ", fpscr)
SH4ASM_DEF_TXT_REG_RN(sts, "sts     ", fpul)

SH4ASM_DEF_TXT_ARMP_REG(ldcl, "ldc.l   ", sr)
SH4ASM_DEF_TXT_ARMP_REG(ldcl, "ldc.l   ", gbr)
SH4ASM_DEF_TXT_ARMP_REG(ldcl, "ldc.l   ", vbr)
SH4ASM_DEF_TXT_ARMP_REG(ldcl, "ldc.l   ", ssr)
SH4ASM_DEF_TXT_ARMP_REG(ldcl, "ldc.l   ", spc)
SH4ASM_DEF_TXT_ARMP_REG(ldcl, "ldc.l   ", dbr)
SH4ASM_DEF_TXT_ARMP_REG(ldsl, "lds.l   ", mach)
SH4ASM_DEF_TXT_ARMP_REG(ldsl, "lds.l   ", macl)
SH4ASM_DEF_TXT_ARMP_REG(ldsl, "lds.l   ", pr)
SH4ASM_DEF_TXT_ARMP_REG(ldsl, "lds.l   ", fpscr)
SH4ASM_DEF_TXT_ARMP_REG(ldsl, "lds.l   ", fpul)

SH4ASM_DEF_TXT_REG_AMRN(stcl, "stc.l   ", sr)
SH4ASM_DEF_TXT_REG_AMRN(stcl, "stc.l   ", gbr)
SH4ASM_DEF_TXT_REG_AMRN(stcl, "stc.l   ", vbr)
SH4ASM_DEF_TXT_REG_AMRN(stcl, "stc.l   ", ssr)
SH4ASM_DEF_TXT_REG_AMRN(stcl, "stc.l   ", spc)
SH4ASM_DEF_TXT_REG_AMRN(stcl, "stc.l   ", sgr)
SH4ASM_DEF_TXT_REG_AMRN(stcl, "stc.l   ", dbr)
SH4ASM_DEF_TXT_REG_AMRN(stsl, "sts.l   ", mach)
SH4ASM_DEF_TXT_REG_AMRN(stsl, "sts.l   ", macl)
SH4ASM_DEF_TXT_REG_AMRN(stsl, "sts.l   ", pr)
SH4ASM_DEF_TXT_REG_AMRN(stsl, "sts.l   ", fpscr)
SH4ASM_DEF_TXT_REG_AMRN(stsl, "sts.l   ", fpul)

SH4ASM_DEF_TXT_REG_ARN(movcal, "movca.l ", r0)

SH4ASM_DEF_TXT_FRN(fldi0, "fldi0   ")
SH4ASM_DEF_TXT_FRN(fldi1, "fldi1   ")
SH4ASM_DEF_TXT_FRN(fabs, "fabs    ")
SH4ASM_DEF_TXT_FRN(fneg, "fneg    ")
SH4ASM_DEF_TXT_FRN(fsqrt, "fsqrt   ")
SH4ASM_DEF_TXT_FRN(fsrra, "fsrra   ")

SH4ASM_DEF_TXT_FRM_REG(flds, "flds    ", fpul)
SH4ASM_DEF_TXT_FRM_REG(ftrc, "ftrc    ", fpul)

SH4ASM_DEF_TXT_REG_FRN(fsts, "fsts    ", fpul)
SH4ASM_DEF_TXT_REG_FRN(float, "float   ", fpul)

SH4ASM_DEF_TXT_IMM8_REG(cmpeq, "cmp/eq  ", r0, 0)
SH4ASM_DEF_TXT_IMM8_REG(and, "and     ", r0, 0)
SH4ASM_DEF_TXT_IMM8_REG(or, "or      ", r0, 0)
SH4ASM_DEF_TXT_IMM8_REG(tst, "tst     ", r0, 0)
SH4ASM_DEF_TXT_IMM8_REG(xor, "xor     ", r0, 0)

SH4ASM_DEF_TXT_IMM8_A_REG_REG(andb, "and.b   ", r0, gbr, 0)
SH4ASM_DEF_TXT_IMM8_A_REG_REG(orb, "or.b    ", r0, gbr, 0)
SH4ASM_DEF_TXT_IMM8_A_REG_REG(tstb, "tst.b   ", r0, gbr, 0)
SH4ASM_DEF_TXT_IMM8_A_REG_REG(xorb, "xor.b   ", r0, gbr, 0)

SH4ASM_DEF_TXT_DISP8_PC(bf, "bf      ", 1)
SH4ASM_DEF_TXT_DISP8_PC(bfs, "bf/s    ", 1)
SH4ASM_DEF_TXT_DISP8_PC(bt, "bt      ", 1)
SH4ASM_DEF_TXT_DISP8_PC(bts, "bt/s    ", 1)

SH4ASM_DEF_TXT_IMM8(trapa, "trapa   ", 0)

SH4ASM_DEF_TXT_REG_A_DISP8_REG(movb, "mov.b   ", r0, gbr, 0)
SH4ASM_DEF_TXT_REG_A_DISP8_REG(movw, "mov.w   ", r0, gbr, 1)
SH4ASM_DEF_TXT_REG_A_DISP8_REG(movl, "mov.l   ", r0, gbr, 2)

SH4ASM_DEF_TXT_A_DISP8_REG1_REG2(movb, "mov.b   ", gbr, r0, 0)
SH4ASM_DEF_TXT_A_DISP8_REG1_REG2(movw, "mov.w   ", gbr, r0, 1)
SH4ASM_DEF_TXT_A_DISP8_REG1_REG2(movl, "mov.l   ", gbr, r0, 2)
SH4ASM_DEF_TXT_A_DISP8_PC_REG2(mova, "mova    ", r0)

SH4ASM_DEF_TXT_OFFS12(bra, "bra     ")
SH4ASM_DEF_TXT_OFFS12(bsr, "bsr     ")

SH4ASM_DEF_TXT_IMM8_RN(mov, "mov     ", 0)
SH4ASM_DEF_TXT_IMM8_RN(add, "add     ", 0)

SH4ASM_DEF_TXT_A_DISP8_PC_RN(movw, "mov.w   ")
SH4ASM_DEF_TXT_A_DISP8_PC_RN(movl, "mov.l   ")

SH4ASM_DEF_TXT_RM_RN(mov, "mov     ")
SH4ASM_DEF_TXT_RM_RN(swapb, "swap.b  ")
SH4ASM_DEF_TXT_RM_RN(swapw, "swap.w  ")
SH4ASM_DEF_TXT_RM_RN(xtrct, "xtrct   ")
SH4ASM_DEF_TXT_RM_RN(add, "add     ")
SH4ASM_DEF_TXT_RM_RN(addc, "addc    ")
SH4ASM_DEF_TXT_RM_RN(addv, "addv    ")
SH4ASM_DEF_TXT_RM_RN(cmpeq, "cmp/eq  ")
SH4ASM_DEF_TXT_RM_RN(cmphs, "cmp/hs  ")
SH4ASM_DEF_TXT_RM_RN(cmpge, "cmp/ge  ")
SH4ASM_DEF_TXT_RM_RN(cmphi, "cmp/hi  ")
SH4ASM_DEF_TXT_RM_RN(cmpgt, "cmp/gt  ")
SH4ASM_DEF_TXT_RM_RN(cmpstr, "cmp/str ")
SH4ASM_DEF_TXT_RM_RN(div1, "div1    ")
SH4ASM_DEF_TXT_RM_RN(div0s, "div0s   ")
SH4ASM_DEF_TXT_RM_RN(dmulsl, "dmuls.l ")
SH4ASM_DEF_TXT_RM_RN(dmulul, "dmulu.l ")
SH4ASM_DEF_TXT_RM_RN(extsb, "exts.b  ")
SH4ASM_DEF_TXT_RM_RN(extsw, "exts.w  ")
SH4ASM_DEF_TXT_RM_RN(extub, "extu.b  ")
SH4ASM_DEF_TXT_RM_RN(extuw, "extu.w  ")
SH4ASM_DEF_TXT_RM_RN(mull, "mul.l   ")
SH4ASM_DEF_TXT_RM_RN(mulsw, "muls.w  ")
SH4ASM_DEF_TXT_RM_RN(muluw, "mulu.w  ")
SH4ASM_DEF_TXT_RM_RN(neg, "neg     ")
SH4ASM_DEF_TXT_RM_RN(negc, "negc    ")
SH4ASM_DEF_TXT_RM_RN(sub, "sub     ")
SH4ASM_DEF_TXT_RM_RN(subc, "subc    ")
SH4ASM_DEF_TXT_RM_RN(subv, "subv    ")
SH4ASM_DEF_TXT_RM_RN(and, "and     ")
SH4ASM_DEF_TXT_RM_RN(not, "not     ")
SH4ASM_DEF_TXT_RM_RN(or, "or      ")
SH4ASM_DEF_TXT_RM_RN(tst, "tst     ")
SH4ASM_DEF_TXT_RM_RN(xor, "xor     ")
SH4ASM_DEF_TXT_RM_RN(shad, "shad    ")
SH4ASM_DEF_TXT_RM_RN(shld, "shld    ")

SH4ASM_DEF_TXT_RM_RN_BANK(ldc, "ldc     ")

SH4ASM_DEF_TXT_ARMP_RN_BANK(ldcl, "ldc.l   ")

SH4ASM_DEF_TXT_RM_BANK_RN(stc, "stc     ")

SH4ASM_DEF_TXT_RM_BANK_AMRN(stcl, "stc.l   ")

SH4ASM_DEF_TXT_RM_A_REG_RN(movb, "mov.b   ", r0)
SH4ASM_DEF_TXT_RM_A_REG_RN(movw, "mov.w   ", r0)
SH4ASM_DEF_TXT_RM_A_REG_RN(movl, "mov.l   ", r0)

SH4ASM_DEF_TXT_A_REG_RM_RN(movb, "mov.b   ", r0)
SH4ASM_DEF_TXT_A_REG_RM_RN(movw, "mov.w   ", r0)
SH4ASM_DEF_TXT_A_REG_RM_RN(movl, "mov.l   ", r0)

SH4ASM_DEF_TXT_RM_ARN(movb, "mov.b   ")
SH4ASM_DEF_TXT_RM_ARN(movw, "mov.w   ")
SH4ASM_DEF_TXT_RM_ARN(movl, "mov.l   ")

SH4ASM_DEF_TXT_ARM_RN(movb, "mov.b   ")
SH4ASM_DEF_TXT_ARM_RN(movw, "mov.w   ")
SH4ASM_DEF_TXT_ARM_RN(movl, "mov.l   ")

SH4ASM_DEF_TXT_RM_AMRN(movb, "mov.b   ")
SH4ASM_DEF_TXT_RM_AMRN(movw, "mov.w   ")
SH4ASM_DEF_TXT_RM_AMRN(movl, "mov.l   ")

SH4ASM_DEF_TXT_ARMP_RN(movb, "mov.b   ")
SH4ASM_DEF_TXT_ARMP_RN(movw, "mov.w   ")
SH4ASM_DEF_TXT_ARMP_RN(movl, "mov.l   ")

SH4ASM_DEF_TXT_ARMP_ARNP(macl, "mac.l   ")
SH4ASM_DEF_TXT_ARMP_ARNP(macw, "mac.w   ")

SH4ASM_DEF_TXT_FRM_FRN(fmov, "fmov    ")
SH4ASM_DEF_TXT_FRM_FRN(fadd, "fadd    ")
SH4ASM_DEF_TXT_FRM_FRN(fcmpeq, "fcmp/eq ")
SH4ASM_DEF_TXT_FRM_FRN(fcmpgt, "fcmp/gt ")
SH4ASM_DEF_TXT_FRM_FRN(fdiv, "fdiv    ")
SH4ASM_DEF_TXT_FRM_FRN(fmul, "fmul    ")
SH4ASM_DEF_TXT_FRM_FRN(fsub, "fsub    ")

SH4ASM_DEF_TXT_REG_FRM_FRN(fmac, "fmac    ", fr0)

SH4ASM_DEF_TXT_ARM_FRN(fmovs, "fmov.s  ")

SH4ASM_DEF_TXT_A_REG_RM_FRN(fmovs, "fmov.s  ", r0)

SH4ASM_DEF_TXT_ARMP_FRN(fmovs, "fmov.s  ")

SH4ASM_DEF_TXT_FRM_ARN(fmovs, "fmov.s  ")

SH4ASM_DEF_TXT_FRM_AMRN(fmovs, "fmov.s  ")

SH4ASM_DEF_TXT_FRM_A_REG_RN(fmovs, "fmov.s  ", r0)

SH4ASM_DEF_TXT_REG_A_DISP4_RN(movb, "mov.b   ", r0, 0)
SH4ASM_DEF_TXT_REG_A_DISP4_RN(movw, "mov.w   ", r0, 1)

SH4ASM_DEF_TXT_A_DISP4_RM_REG(movb, "mov.b   ", r0, 0)
SH4ASM_DEF_TXT_A_DISP4_RM_REG(movw, "mov.w   ", r0, 1)

SH4ASM_DEF_TXT_RM_A_DISP4_RN(movl, "mov.l   ", 2)

SH4ASM_DEF_TXT_A_DISP4_RM_RN(movl, "mov.l   ", 2)

SH4ASM_DEF_TXT_DRM_DRN(fmov, "fmov    ")
SH4ASM_DEF_TXT_DRM_DRN(fadd, "fadd    ")
SH4ASM_DEF_TXT_DRM_DRN(fcmpeq, "fcmp/eq ")
SH4ASM_DEF_TXT_DRM_DRN(fcmpgt, "fcmp/gt ")
SH4ASM_DEF_TXT_DRM_DRN(fdiv, "fdiv    ")
SH4ASM_DEF_TXT_DRM_DRN(fmul, "fmul    ")
SH4ASM_DEF_TXT_DRM_DRN(fsub, "fsub    ")

SH4ASM_DEF_TXT_DRM_XDN(fmov, "fmov    ")

SH4ASM_DEF_TXT_XDM_DRN(fmov, "fmov    ")

SH4ASM_DEF_TXT_XDM_XDN(fmov, "fmov    ")

SH4ASM_DEF_TXT_DRM_ARN(fmov, "fmov    ")

SH4ASM_DEF_TXT_DRM_AMRN(fmov, "fmov    ")

SH4ASM_DEF_TXT_DRM_A_REG_RN(fmov, "fmov    ", r0)

SH4ASM_DEF_TXT_XDM_ARN(fmov, "fmov    ")

SH4ASM_DEF_TXT_XDM_AMRN(fmov, "fmov    ")

SH4ASM_DEF_TXT_XDM_A_REG_RN(fmov, "fmov    ", r0)

SH4ASM_DEF_TXT_DRN(fabs, "fabs    ")
SH4ASM_DEF_TXT_DRN(fneg, "fneg    ")
SH4ASM_DEF_TXT_DRN(fsqrt, "fsqrt   ")

SH4ASM_DEF_TXT_DRM_REG(fcnvds, "fcnvds  ", fpul)
SH4ASM_DEF_TXT_DRM_REG(ftrc, "ftrc    ", fpul)

SH4ASM_DEF_TXT_REG_DRN(fcnvsd, "fcnvsd  ", fpul)
SH4ASM_DEF_TXT_REG_DRN(float, "float   ", fpul)
SH4ASM_DEF_TXT_REG_DRN(fsca, "fsca    ", fpul)

SH4ASM_DEF_TXT_FVM_FVN(fipr, "fipr    ")

SH4ASM_DEF_TXT_REG_FVN(ftrv, "ftrv    ", xmtrx)

#endif
