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

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

#include "sh4asm.h"
#include "parser.h"

#define MAX_TOKENS 32
static struct sh4asm_tok tokens[MAX_TOKENS];
static unsigned n_tokens;

#define CHECK_(cond, func, line, file) do_check((cond), #cond, func, line, file)
#define CHECK(cond) CHECK_((cond), __func__, __LINE__, __FILE__)

static void do_check(bool success, char const *cond, char const *func,
                     unsigned line, char const *file) {
    if (!success)
        sh4asm_error("%s - sanity check failed: \"%s\" (line %d of %s)\n",
             func, cond, line, file);
}

#define CHECK_RN_(tok_idx, func, line, file)                    \
    CHECK_(tokens[(tok_idx)].tp == SH4ASM_TOK_RN, func, line, file)
#define CHECK_RN(tok_idx) CHECK_RN_(tok_idx, __func__, __LINE__, __FILE__)
#define CHECK_RN_BANK(tok_idx) CHECK_(tokens[(tok_idx)].tp == SH4ASM_TOK_RN_BANK, \
                                      __func__, __LINE__, __FILE__)
#define CHECK_FRN(tok_idx) CHECK_FRN_(tok_idx, __func__, __LINE__, __FILE__)
#define CHECK_FRN_(tok_idx, func, line, file)                           \
    CHECK_(tokens[(tok_idx)].tp == SH4ASM_TOK_FRN, func, line, file)
#define CHECK_DRN(tok_idx) CHECK_DRN_((tok_idx), __func__, __LINE__, __FILE__)
#define CHECK_DRN_(tok_idx, func, line, file) do_check_drn((tok_idx), func, line, file)
#define CHECK_XDN(tok_idx) do_check_xdn((tok_idx), __LINE__, __FILE__)
#define CHECK_FVN(tok_idx) do_check_fvn((tok_idx), __LINE__, __FILE__)
#define CHECK_IMM(tok_idx) CHECK_(tokens[(tok_idx)].tp == SH4ASM_TOK_IMM,  \
                                  __func__, __LINE__, __FILE__)
#define CHECK_DISP(tok_idx) CHECK_(tokens[(tok_idx)].tp == SH4ASM_TOK_DISP,    \
                                   __func__, __LINE__, __FILE__)

#define CHECK_R0(tok_idx) do_check_r0((tok_idx), __func__, __LINE__, __FILE__)
#define CHECK_FR0(tok_idx) do_check_fr0((tok_idx), __func__, __LINE__, __FILE__)

static void do_check_r0(unsigned tok_idx, char const *func,
                        unsigned line, char const *file) {
    CHECK_RN_(tok_idx, func, line, file);
    CHECK_(tokens[tok_idx].val.reg_idx == 0, func, line, file);
}

static void do_check_fr0(unsigned tok_idx, char const *func,
                         unsigned line, char const *file) {
    CHECK_FRN_(tok_idx, func, line, file);
    CHECK_(tokens[tok_idx].val.reg_idx == 0, func, line, file);
}

static void do_check_drn(unsigned tok_idx, char const *func,
                         unsigned line, char const *file) {
    CHECK_(tokens[tok_idx].tp == SH4ASM_TOK_DRN, func, line, file);
    unsigned reg_idx = tokens[tok_idx].val.reg_idx;
    CHECK_(reg_idx < 16, func, line, file);
    CHECK_(!(reg_idx & 1), func, line, file);
}

static void do_check_xdn(unsigned tok_idx, unsigned line, char const *file) {
    CHECK(tokens[tok_idx].tp == SH4ASM_TOK_XDN);
    unsigned reg_idx = tokens[tok_idx].val.reg_idx;
    if (reg_idx >= 16)
        sh4asm_error("invalid out-of-range banked double-precision floating-point "
             "register index %u (see line %d of %s)", reg_idx, line, file);
    if (reg_idx & 1)
        sh4asm_error("invalid non-even banked double-precision floating-point "
             "register index %u (see line %d of %s)", reg_idx, line, file);
}

static void do_check_fvn(unsigned tok_idx, unsigned line, char const *file) {
    CHECK(tokens[tok_idx].tp == SH4ASM_TOK_FVN);
    unsigned reg_idx = tokens[tok_idx].val.reg_idx;
    if (reg_idx >= 16)
        sh4asm_error("invalid out-of-range double-precision floating-point register "
             "index %u (see line %d of %s)", reg_idx, line, file);
    if (reg_idx & 3)
        sh4asm_error("invalid non-even double-precision floating-point register "
             "index %u (see line %d of %s)", reg_idx, line, file);
}

static sh4asm_bin_emit_handler_func emit;

typedef void(*parser_emit_func)(void);

#define OP_NOARG(op)                            \
    static void emit_##op(void) {               \
        sh4asm_bin_##op(emit);                     \
    }

OP_NOARG(div0u)
OP_NOARG(rts)
OP_NOARG(clrmac)
OP_NOARG(clrs)
OP_NOARG(clrt)
OP_NOARG(ldtlb)
OP_NOARG(nop)
OP_NOARG(rte)
OP_NOARG(sets)
OP_NOARG(sett)
OP_NOARG(sleep)
OP_NOARG(frchg)
OP_NOARG(fschg)

#define OP_RN(op)                               \
    static void emit_##op##_rn(void) {          \
        CHECK_RN(1);                            \
        int reg_no = tokens[1].val.reg_idx;     \
        sh4asm_bin_##op##_rn(emit, reg_no);        \
    }

OP_RN(movt)
OP_RN(cmppz)
OP_RN(cmppl)
OP_RN(dt)
OP_RN(rotl)
OP_RN(rotr)
OP_RN(rotcl)
OP_RN(rotcr)
OP_RN(shal)
OP_RN(shar)
OP_RN(shll)
OP_RN(shlr)
OP_RN(shll2)
OP_RN(shlr2)
OP_RN(shll8)
OP_RN(shlr8)
OP_RN(shll16)
OP_RN(shlr16)
OP_RN(braf)
OP_RN(bsrf)

#define OP_ARN(op)                              \
    static void emit_##op##_arn(void) {         \
        CHECK_RN(2);                            \
        int reg_no = tokens[2].val.reg_idx;     \
        sh4asm_bin_##op##_arn(emit, reg_no);       \
    }

OP_ARN(tasb)
OP_ARN(ocbi)
OP_ARN(ocbp)
OP_ARN(ocbwb)
OP_ARN(pref)
OP_ARN(jmp)
OP_ARN(jsr)

#define OP_RM_REG(op, reg)                      \
    static void emit_##op##_rm_##reg(void) {    \
        CHECK_RN(1);                            \
        int reg_no = tokens[1].val.reg_idx;     \
        sh4asm_bin_##op##_rm_##reg(emit, reg_no);  \
    }

OP_RM_REG(ldc, sr)
OP_RM_REG(ldc, gbr)
OP_RM_REG(ldc, vbr)
OP_RM_REG(ldc, ssr)
OP_RM_REG(ldc, spc)
OP_RM_REG(ldc, dbr)
OP_RM_REG(lds, mach)
OP_RM_REG(lds, macl)
OP_RM_REG(lds, pr)
OP_RM_REG(lds, fpscr)
OP_RM_REG(lds, fpul)

#define OP_REG_RN(op, reg)                          \
    static void emit_##op##_##reg##_rn(void) {      \
        CHECK_RN(3);                                \
        int reg_no = tokens[3].val.reg_idx;         \
        sh4asm_bin_##op##_##reg##_rn(emit, reg_no);    \
    }

OP_REG_RN(stc, sr)
OP_REG_RN(stc, gbr)
OP_REG_RN(stc, vbr)
OP_REG_RN(stc, ssr)
OP_REG_RN(stc, spc)
OP_REG_RN(stc, sgr)
OP_REG_RN(stc, dbr)
OP_REG_RN(sts, mach)
OP_REG_RN(sts, macl)
OP_REG_RN(sts, pr)
OP_REG_RN(sts, fpscr)
OP_REG_RN(sts, fpul)

#define OP_ARMP_REG(op, reg)                        \
    static void emit_##op##_armp_##reg(void) {      \
        CHECK_RN(2);                                \
        int reg_no = tokens[2].val.reg_idx;         \
        sh4asm_bin_##op##_armp_##reg(emit, reg_no);    \
    }

OP_ARMP_REG(ldcl, sr)
OP_ARMP_REG(ldcl, gbr)
OP_ARMP_REG(ldcl, vbr)
OP_ARMP_REG(ldcl, ssr)
OP_ARMP_REG(ldcl, spc)
OP_ARMP_REG(ldcl, dbr)
OP_ARMP_REG(ldsl, mach)
OP_ARMP_REG(ldsl, macl)
OP_ARMP_REG(ldsl, pr)
OP_ARMP_REG(ldsl, fpscr)
OP_ARMP_REG(ldsl, fpul)

#define OP_REG_AMRN(op, reg)                        \
    static void emit_##op##_##reg##_amrn(void)  {   \
        CHECK_RN(5);                                \
        int reg_no = tokens[5].val.reg_idx;         \
        sh4asm_bin_##op##_##reg##_amrn(emit, reg_no);  \
    }

OP_REG_AMRN(stcl, sr)
OP_REG_AMRN(stcl, gbr)
OP_REG_AMRN(stcl, vbr)
OP_REG_AMRN(stcl, ssr)
OP_REG_AMRN(stcl, spc)
OP_REG_AMRN(stcl, sgr)
OP_REG_AMRN(stcl, dbr)
OP_REG_AMRN(stsl, mach)
OP_REG_AMRN(stsl, macl)
OP_REG_AMRN(stsl, pr)
OP_REG_AMRN(stsl, fpscr)
OP_REG_AMRN(stsl, fpul)

#define OP_R0_ARN(op)                                                   \
    static void emit_##op##_r0_arn(void) {                              \
        CHECK_RN(4);                                                    \
        CHECK_R0(1);                                                    \
        int reg_no = tokens[4].val.reg_idx;                             \
        sh4asm_bin_##op##_r0_arn(emit, reg_no);                            \
    }

OP_R0_ARN(movcal)

#define OP_FRN(op)                                                      \
    static void emit_##op##_frn(void) {                                 \
        CHECK_FRN(1);                                                   \
        int reg_no = tokens[1].val.reg_idx;                             \
        sh4asm_bin_##op##_frn(emit, reg_no);                               \
    }

OP_FRN(fldi0)
OP_FRN(fldi1)
OP_FRN(fabs)
OP_FRN(fneg)
OP_FRN(fsqrt)
OP_FRN(fsrra)

#define OP_FRM_REG(op, reg)                     \
    static void emit_##op##_frm_##reg(void) {   \
        CHECK_FRN(1);                           \
        int reg_no = tokens[1].val.reg_idx;     \
        sh4asm_bin_##op##_frm_##reg(emit, reg_no); \
    }

OP_FRM_REG(flds, fpul)
OP_FRM_REG(ftrc, fpul)

#define OP_REG_FRN(op, reg)                         \
    static void emit_##op##_##reg##_frn(void) {     \
        CHECK_FRN(3);                               \
        int reg_no = tokens[3].val.reg_idx;         \
        sh4asm_bin_##op##_##reg##_frn(emit, reg_no);   \
    }

OP_REG_FRN(fsts, fpul)
OP_REG_FRN(float, fpul)

#define OP_IMM8_R0(op)                          \
    static void emit_##op##_imm8_r0(void) {     \
        CHECK_R0(3);                            \
        CHECK_IMM(1);                           \
        int imm_val = tokens[1].val.as_int;     \
        sh4asm_bin_##op##_imm8_r0(emit, imm_val);  \
    }

OP_IMM8_R0(cmpeq)
OP_IMM8_R0(and)
OP_IMM8_R0(or)
OP_IMM8_R0(tst)
OP_IMM8_R0(xor)

#define OP_IMM8_A_R0_REG(op, reg)                       \
    static void emit_##op##_imm8_a_r0_##reg(void) {     \
        CHECK_IMM(1);                                   \
        CHECK_R0(5);                                    \
        int imm_val = tokens[1].val.as_int;             \
        sh4asm_bin_##op##_imm8_a_r0_##reg(emit, imm_val);  \
    }

OP_IMM8_A_R0_REG(andb, gbr)
OP_IMM8_A_R0_REG(orb, gbr)
OP_IMM8_A_R0_REG(tstb, gbr)
OP_IMM8_A_R0_REG(xorb, gbr)

#define OP_OFFS8(op)                            \
    static void emit_##op##_offs8(void) {       \
        CHECK_DISP(1);                          \
        int disp_val = tokens[1].val.as_int;    \
        sh4asm_bin_##op##_offs8(emit, disp_val);   \
    }

OP_OFFS8(bf)
OP_OFFS8(bfs)
OP_OFFS8(bt)
OP_OFFS8(bts)

#define OP_IMM8(op)                             \
    static void emit_##op##_imm8(void) {        \
        CHECK_IMM(1);                           \
        int imm_val = tokens[1].val.as_int;     \
        sh4asm_bin_##op##_imm8(emit, imm_val);     \
    }

OP_IMM8(trapa);

#define OP_R0_A_DISP8_REG(op, reg)                          \
    static void emit_##op##_r0_a_disp8_##reg(void) {        \
        CHECK_R0(1);                                        \
        CHECK_DISP(5);                                      \
        int disp_val = tokens[5].val.as_int;                \
        sh4asm_bin_##op##_r0_a_disp8_##reg(emit, disp_val);    \
    }

OP_R0_A_DISP8_REG(movb, gbr)
OP_R0_A_DISP8_REG(movw, gbr)
OP_R0_A_DISP8_REG(movl, gbr)

#define OP_A_DISP8_REG_R0(op, reg)                          \
    static void emit_##op##_a_disp8_##reg##_r0(void) {      \
        CHECK_R0(8);                                        \
        CHECK_DISP(3);                                      \
        int disp_val = tokens[3].val.as_int;                \
        sh4asm_bin_##op##_a_disp8_##reg##_r0(emit, disp_val);  \
    }

OP_A_DISP8_REG_R0(movb, gbr)
OP_A_DISP8_REG_R0(movw, gbr)
OP_A_DISP8_REG_R0(movl, gbr)

#define OP_A_OFFS8_REG_R0(op, reg)              \
    static void emit_##op##_a_offs8_##reg##_r0(void) {      \
        CHECK_R0(8);                                        \
        CHECK_DISP(3);                                      \
        int disp_val = tokens[3].val.as_int;                \
        sh4asm_bin_##op##_a_offs8_##reg##_r0(emit, disp_val);  \
    }

OP_A_OFFS8_REG_R0(mova, pc)

#define OP_OFFS12(op)                           \
    static void emit_##op##_offs12(void) {      \
        CHECK_DISP(1);                          \
        int disp_val = tokens[1].val.as_int;    \
        sh4asm_bin_##op##_offs12(emit, disp_val);  \
    }

OP_OFFS12(bra)
OP_OFFS12(bsr)

#define OP_IMM8_RN(op)                                  \
    static void emit_##op##_imm8_rn(void) {             \
        CHECK_IMM(1);                                   \
        CHECK_RN(3);                                    \
        int imm_val = tokens[1].val.as_int;             \
        int reg_no = tokens[3].val.reg_idx;             \
        sh4asm_bin_##op##_imm8_rn(emit, imm_val, reg_no);  \
    }

OP_IMM8_RN(mov)
OP_IMM8_RN(add)

#define OP_A_OFFS8_REG_RN(op, reg)                                  \
    static void emit_##op##_a_offs8_##reg##_rn(void) {              \
        CHECK_DISP(3);                                              \
        CHECK_RN(8);                                                \
        int disp_val = tokens[3].val.as_int;                        \
        int reg_no = tokens[8].val.reg_idx;                         \
        sh4asm_bin_##op##_a_offs8_##reg##_rn(emit, disp_val, reg_no);  \
    }

OP_A_OFFS8_REG_RN(movw, pc)
OP_A_OFFS8_REG_RN(movl, pc)

#define OP_RM_RN(op)                                \
    static void emit_##op##_rm_rn(void) {           \
        CHECK_RN(1);                                \
        CHECK_RN(3);                                \
        int rm_no = tokens[1].val.as_int;           \
        int rn_no = tokens[3].val.as_int;           \
        sh4asm_bin_##op##_rm_rn(emit, rm_no, rn_no);   \
    }

OP_RM_RN(mov)
OP_RM_RN(swapb)
OP_RM_RN(swapw)
OP_RM_RN(xtrct)
OP_RM_RN(add)
OP_RM_RN(addc)
OP_RM_RN(addv)
OP_RM_RN(cmpeq)
OP_RM_RN(cmphs)
OP_RM_RN(cmpge)
OP_RM_RN(cmphi)
OP_RM_RN(cmpgt)
OP_RM_RN(cmpstr)
OP_RM_RN(div1)
OP_RM_RN(div0s)
OP_RM_RN(dmulsl)
OP_RM_RN(dmulul)
OP_RM_RN(extsb)
OP_RM_RN(extsw)
OP_RM_RN(extub)
OP_RM_RN(extuw)
OP_RM_RN(mull)
OP_RM_RN(mulsw)
OP_RM_RN(muluw)
OP_RM_RN(neg)
OP_RM_RN(negc)
OP_RM_RN(sub)
OP_RM_RN(subc)
OP_RM_RN(subv)
OP_RM_RN(and)
OP_RM_RN(not)
OP_RM_RN(or)
OP_RM_RN(tst)
OP_RM_RN(xor)
OP_RM_RN(shad)
OP_RM_RN(shld)

#define OP_RM_A_R0_RN(op)                                \
    static void emit_##op##_rm_a_r0_rn(void) {           \
        CHECK_RN(1);                                     \
        CHECK_R0(5);                                     \
        CHECK_RN(7);                                     \
        int rm_no = tokens[1].val.reg_idx;               \
        int rn_no = tokens[7].val.reg_idx;               \
        sh4asm_bin_##op##_rm_a_r0_rn(emit, rm_no, rn_no);   \
    }

OP_RM_A_R0_RN(movb)
OP_RM_A_R0_RN(movw)
OP_RM_A_R0_RN(movl)

#define OP_A_R0_RM_RN(op)                                \
    static void emit_##op##_a_r0_rm_rn(void) {           \
        CHECK_R0(3);                                     \
        CHECK_RN(5);                                     \
        CHECK_RN(8);                                     \
        int rm_no = tokens[5].val.reg_idx;               \
        int rn_no = tokens[8].val.reg_idx;               \
        sh4asm_bin_##op##_a_r0_rm_rn(emit, rm_no, rn_no);   \
    }

OP_A_R0_RM_RN(movb)
OP_A_R0_RM_RN(movw)
OP_A_R0_RM_RN(movl)

#define OP_RM_ARN(op)                                    \
    static void emit_##op##_rm_arn(void) {               \
        CHECK_RN(1);                                     \
        CHECK_RN(4);                                     \
        int rm_no = tokens[1].val.reg_idx;               \
        int rn_no = tokens[4].val.reg_idx;               \
        sh4asm_bin_##op##_rm_arn(emit, rm_no, rn_no);       \
    }

OP_RM_ARN(movb)
OP_RM_ARN(movw)
OP_RM_ARN(movl)

#define OP_ARM_RN(op)                                    \
    static void emit_##op##_arm_rn(void) {               \
        CHECK_RN(2);                                     \
        CHECK_RN(4);                                     \
        int rm_no = tokens[2].val.reg_idx;               \
        int rn_no = tokens[4].val.reg_idx;               \
        sh4asm_bin_##op##_arm_rn(emit, rm_no, rn_no);       \
    }

OP_ARM_RN(movb)
OP_ARM_RN(movw)
OP_ARM_RN(movl)

#define OP_RM_AMRN(op)                                    \
    static void emit_##op##_rm_amrn(void) {               \
        CHECK_RN(1);                                      \
        CHECK_RN(5);                                      \
        int rm_no = tokens[1].val.reg_idx;                \
        int rn_no = tokens[5].val.reg_idx;                \
        sh4asm_bin_##op##_rm_amrn(emit, rm_no, rn_no);       \
    }

OP_RM_AMRN(movb)
OP_RM_AMRN(movw)
OP_RM_AMRN(movl)

#define OP_ARMP_RN(op)                                    \
    static void emit_##op##_armp_rn(void) {               \
        CHECK_RN(2);                                      \
        CHECK_RN(5);                                      \
        int rm_no = tokens[2].val.reg_idx;                \
        int rn_no = tokens[5].val.reg_idx;                \
        sh4asm_bin_##op##_armp_rn(emit, rm_no, rn_no);       \
    }

OP_ARMP_RN(movb)
OP_ARMP_RN(movw)
OP_ARMP_RN(movl)

#define OP_ARMP_ARNP(op)                                    \
    static void emit_##op##_armp_arnp(void) {               \
        CHECK_RN(2);                                        \
        CHECK_RN(6);                                        \
        int rm_no = tokens[2].val.reg_idx;                  \
        int rn_no = tokens[6].val.reg_idx;                  \
        sh4asm_bin_##op##_armp_arnp(emit, rm_no, rn_no);       \
    }

OP_ARMP_ARNP(macw)
OP_ARMP_ARNP(macl)

#define OP_FRM_FRN(op)                                       \
    static void emit_##op##_frm_frn(void) {                  \
        CHECK_FRN(1);                                        \
        CHECK_FRN(3);                                        \
        int frm_no = tokens[1].val.reg_idx;                  \
        int frn_no = tokens[3].val.reg_idx;                  \
        sh4asm_bin_##op##_frm_frn(emit, frm_no, frn_no);        \
    }

OP_FRM_FRN(fmov)
OP_FRM_FRN(fadd)
OP_FRM_FRN(fcmpeq)
OP_FRM_FRN(fcmpgt)
OP_FRM_FRN(fdiv)
OP_FRM_FRN(fmul)
OP_FRM_FRN(fsub)

#define OP_ARM_FRN(op)                              \
    static void emit_##op##_arm_frn(void) {             \
        CHECK_RN(2);                                    \
        CHECK_FRN(4);                                   \
        int rm_no = tokens[2].val.reg_idx;              \
        int frn_no = tokens[4].val.reg_idx;             \
        sh4asm_bin_##op##_arm_frn(emit, rm_no, frn_no);    \
    }

OP_ARM_FRN(fmovs)

#define OP_A_R0_RM_FRN(op)                              \
    static void emit_##op##_a_r0_rm_frn(void) {             \
        CHECK_R0(3);                                        \
        CHECK_RN(5);                                        \
        CHECK_FRN(8);                                       \
        int rm_no = tokens[5].val.reg_idx;                  \
        int frn_no = tokens[8].val.reg_idx;                 \
        sh4asm_bin_##op##_a_r0_rm_frn(emit, rm_no, frn_no);    \
    }

OP_A_R0_RM_FRN(fmovs)

#define OP_ARMP_FRN(op)                             \
    static void emit_##op##_armp_frn(void) {            \
        CHECK_RN(2);                                    \
        CHECK_FRN(5);                                   \
        int rm_no = tokens[2].val.reg_idx;              \
        int frn_no = tokens[5].val.reg_idx;             \
        sh4asm_bin_##op##_armp_frn(emit, rm_no, frn_no);   \
    }

OP_ARMP_FRN(fmovs)

#define OP_FRM_ARN(op)                              \
    static void emit_##op##_frm_arn(void) {             \
        CHECK_FRN(1);                                   \
        CHECK_RN(4);                                    \
        int rn_no = tokens[4].val.reg_idx;              \
        int frm_no = tokens[1].val.reg_idx;             \
        sh4asm_bin_##op##_frm_arn(emit, frm_no, rn_no);    \
    }

OP_FRM_ARN(fmovs)

#define OP_FRM_AMRN(op)                             \
    static void emit_##op##_frm_amrn(void) {            \
        CHECK_FRN(1);                                   \
        CHECK_RN(5);                                    \
        int rn_no = tokens[5].val.reg_idx;              \
        int frm_no = tokens[1].val.reg_idx;             \
        sh4asm_bin_##op##_frm_amrn(emit, frm_no, rn_no);   \
    }

OP_FRM_AMRN(fmovs)

#define OP_FRM_A_R0_RN(op)                                  \
    static void emit_##op##_frm_a_r0_rn(void) {             \
        CHECK_FRN(1);                                       \
        CHECK_R0(5);                                        \
        CHECK_RN(7);                                        \
        int rn_no = tokens[7].val.reg_idx;                  \
        int frm_no = tokens[1].val.reg_idx;                 \
        sh4asm_bin_##op##_frm_a_r0_rn(emit, frm_no, rn_no);    \
    }

OP_FRM_A_R0_RN(fmovs)

#define OP_FR0_FRM_FRN(op)                                  \
    static void emit_##op##_fr0_frm_frn(void) {             \
        CHECK_FR0(1);                                       \
        CHECK_FRN(3);                                       \
        CHECK_FRN(5);                                       \
        int frm_no = tokens[3].val.reg_idx;                 \
        int frn_no = tokens[5].val.reg_idx;                 \
        sh4asm_bin_##op##_fr0_frm_frn(emit, frm_no, frn_no);   \
    }

OP_FR0_FRM_FRN(fmac)

#define OP_RM_RN_BANK(op)                                   \
    static void emit_##op##_rm_rn_bank(void) {              \
        CHECK_RN(1);                                        \
        CHECK_RN_BANK(3);                                   \
        int rm_no = tokens[1].val.reg_idx;                  \
        int rn_bank_no = tokens[3].val.reg_idx;             \
        sh4asm_bin_##op##_rm_rn_bank(emit, rm_no, rn_bank_no); \
    }

OP_RM_RN_BANK(ldc)

#define OP_RM_BANK_RN(op)                                   \
    static void emit_##op##_rm_bank_rn(void) {              \
        CHECK_RN_BANK(1);                                   \
        CHECK_RN(3);                                        \
        int rm_bank_no = tokens[1].val.reg_idx;             \
        int rn_no = tokens[3].val.reg_idx;                  \
        sh4asm_bin_##op##_rm_bank_rn(emit, rm_bank_no, rn_no); \
    }

OP_RM_BANK_RN(stc)

#define OP_ARMP_RN_BANK(op)                                     \
    static void emit_##op##_armp_rn_bank(void) {                \
        CHECK_RN(2);                                            \
        CHECK_RN_BANK(5);                                       \
        int rm_no = tokens[2].val.reg_idx;                      \
        int rn_bank_no = tokens[5].val.reg_idx;                 \
        sh4asm_bin_##op##_armp_rn_bank(emit, rm_no, rn_bank_no);   \
    }

OP_ARMP_RN_BANK(ldcl)

#define OP_RM_BANK_AMRN(op)                                     \
    static void emit_##op##_rm_bank_amrn(void) {                \
        CHECK_RN_BANK(1);                                       \
        CHECK_RN(5);                                            \
        int rm_bank_no = tokens[1].val.reg_idx;                 \
        int rn_no = tokens[5].val.reg_idx;                      \
        sh4asm_bin_##op##_rm_bank_amrn(emit, rm_bank_no, rn_no);   \
    }

OP_RM_BANK_AMRN(stcl)

#define OP_R0_A_DISP4_RN(op)                                        \
    static void emit_##op##_r0_a_disp4_rn(void) {                   \
        CHECK_R0(1);                                                \
        CHECK_DISP(5);                                              \
        CHECK_RN(7);                                                \
        int disp_val = tokens[5].val.as_int;                        \
        int reg_no = tokens[7].val.reg_idx;                         \
        sh4asm_bin_##op##_r0_a_disp4_rn(emit, disp_val, reg_no);       \
    }

OP_R0_A_DISP4_RN(movb)
OP_R0_A_DISP4_RN(movw)

#define OP_A_DISP4_RM_R0(op)                                    \
    static void emit_##op##_a_disp4_rm_r0(void) {               \
        CHECK_DISP(3);                                          \
        CHECK_RN(5);                                            \
        CHECK_R0(8);                                            \
        int disp_val = tokens[3].val.as_int;                    \
        int reg_no = tokens[5].val.reg_idx;                     \
        sh4asm_bin_##op##_a_disp4_rm_r0(emit, disp_val, reg_no);   \
    }

OP_A_DISP4_RM_R0(movb)
OP_A_DISP4_RM_R0(movw)

#define OP_RM_A_DISP4_RN(op)                                            \
    static void emit_##op##_rm_a_disp4_rn(void) {                       \
        CHECK_RN(1);                                                    \
        CHECK_DISP(5);                                                  \
        CHECK_RN(7);                                                    \
        int src_reg = tokens[1].val.reg_idx;                            \
        int disp_val = tokens[5].val.as_int;                            \
        int dst_reg = tokens[7].val.reg_idx;                            \
        sh4asm_bin_##op##_rm_a_disp4_rn(emit, src_reg, disp_val, dst_reg); \
    }

OP_RM_A_DISP4_RN(movl)

#define OP_A_DISP4_RM_RN(op)                                            \
    static void emit_##op##_a_disp4_rm_rn(void) {                       \
        CHECK_DISP(3);                                                  \
        CHECK_RN(5);                                                    \
        CHECK_RN(8);                                                    \
        int disp_val = tokens[3].val.as_int;                            \
        int src_reg = tokens[5].val.reg_idx;                            \
        int dst_reg = tokens[8].val.reg_idx;                            \
        sh4asm_bin_##op##_a_disp4_rm_rn(emit, disp_val, src_reg, dst_reg); \
    }

OP_A_DISP4_RM_RN(movl)

#define OP_DRM_DRN(op)                                  \
    static void emit_##op##_drm_drn(void) {             \
        CHECK_DRN(1);                                   \
        CHECK_DRN(3);                                   \
        int src_reg = tokens[1].val.reg_idx;            \
        int dst_reg = tokens[3].val.reg_idx;            \
        sh4asm_bin_##op##_drm_drn(emit, src_reg, dst_reg); \
    }

OP_DRM_DRN(fmov)
OP_DRM_DRN(fadd)
OP_DRM_DRN(fcmpeq)
OP_DRM_DRN(fcmpgt)
OP_DRM_DRN(fdiv)
OP_DRM_DRN(fmul)
OP_DRM_DRN(fsub)

#define OP_DRM_XDN(op)                                      \
    static void emit_##op##_drm_xdn(void) {                 \
        CHECK_DRN(1);                                       \
        CHECK_XDN(3);                                       \
        int src_reg = tokens[1].val.reg_idx;                \
        int dst_reg = tokens[3].val.reg_idx;                \
        sh4asm_bin_##op##_drm_xdn(emit, src_reg, dst_reg);     \
    }

OP_DRM_XDN(fmov)

#define OP_XDM_DRN(op)                                      \
    static void emit_##op##_xdm_drn(void) {                 \
        CHECK_XDN(1);                                       \
        CHECK_DRN(3);                                       \
        int src_reg = tokens[1].val.reg_idx;                \
        int dst_reg = tokens[3].val.reg_idx;                \
        sh4asm_bin_##op##_xdm_drn(emit, src_reg, dst_reg);     \
    }

OP_XDM_DRN(fmov)

#define OP_XDM_XDN(op)                                      \
    static void emit_##op##_xdm_xdn(void) {                 \
        CHECK_XDN(1);                                       \
        CHECK_XDN(3);                                       \
        int src_reg = tokens[1].val.reg_idx;                \
        int dst_reg = tokens[3].val.reg_idx;                \
        sh4asm_bin_##op##_xdm_xdn(emit, src_reg, dst_reg);     \
    }

OP_XDM_XDN(fmov)

#define OP_ARM_DRN(op)                                  \
    static void emit_##op##_arm_drn(void) {             \
        CHECK_RN(2);                                    \
        CHECK_DRN(4);                                   \
        int src_reg = tokens[2].val.reg_idx;            \
        int dst_reg = tokens[4].val.reg_idx;            \
        sh4asm_bin_##op##_arm_drn(emit, src_reg, dst_reg); \
    }

OP_ARM_DRN(fmov)

#define OP_A_R0_RM_DRN(op)                                  \
    static void emit_##op##_a_r0_rm_drn(void) {             \
        CHECK_R0(3);                                        \
        CHECK_RN(5);                                        \
        CHECK_DRN(8);                                       \
        int src_reg = tokens[5].val.reg_idx;                \
        int dst_reg = tokens[8].val.reg_idx;                \
        sh4asm_bin_##op##_a_r0_rm_drn(emit, src_reg, dst_reg); \
    }

OP_A_R0_RM_DRN(fmov)

#define OP_ARMP_DRN(op)                                     \
    static void emit_##op##_armp_drn(void) {                \
        CHECK_RN(2);                                        \
        CHECK_DRN(5);                                       \
        int src_reg = tokens[2].val.reg_idx;                \
        int dst_reg = tokens[5].val.reg_idx;                \
        sh4asm_bin_##op##_armp_drn(emit, src_reg, dst_reg);    \
    }

OP_ARMP_DRN(fmov)

#define OP_ARM_XDN(op)                                  \
    static void emit_##op##_arm_xdn(void) {             \
        CHECK_RN(2);                                    \
        CHECK_XDN(4);                                   \
        int src_reg = tokens[2].val.reg_idx;            \
        int dst_reg = tokens[4].val.reg_idx;            \
        sh4asm_bin_##op##_arm_xdn(emit, src_reg, dst_reg); \
    }

OP_ARM_XDN(fmov)

#define OP_ARMP_XDN(op)                                     \
    static void emit_##op##_armp_xdn(void) {                \
        CHECK_RN(2);                                        \
        CHECK_XDN(5);                                       \
        int src_reg = tokens[2].val.reg_idx;                \
        int dst_reg = tokens[5].val.reg_idx;                \
        sh4asm_bin_##op##_armp_xdn(emit, src_reg, dst_reg);    \
    }

OP_ARMP_XDN(fmov)

#define OP_A_R0_RM_XDN(op)                                  \
    static void emit_##op##_a_r0_rm_xdn(void) {             \
        CHECK_R0(3);                                        \
        CHECK_RN(5);                                        \
        CHECK_XDN(8);                                       \
        int src_reg = tokens[5].val.reg_idx;                \
        int dst_reg = tokens[8].val.reg_idx;                \
        sh4asm_bin_##op##_a_r0_rm_xdn(emit, src_reg, dst_reg); \
    }

OP_A_R0_RM_XDN(fmov)

#define OP_DRM_ARN(op)                                      \
    static void emit_##op##_drm_arn(void) {                 \
        CHECK_DRN(1);                                       \
        CHECK_RN(4);                                        \
        int src_reg = tokens[1].val.reg_idx;                \
        int dst_reg = tokens[4].val.reg_idx;                \
        sh4asm_bin_##op##_drm_arn(emit, src_reg, dst_reg);     \
    }

OP_DRM_ARN(fmov)

#define OP_DRM_AMRN(op)                                      \
    static void emit_##op##_drm_amrn(void) {                 \
        CHECK_DRN(1);                                        \
        CHECK_RN(5);                                         \
        int src_reg = tokens[1].val.reg_idx;                 \
        int dst_reg = tokens[5].val.reg_idx;                 \
        sh4asm_bin_##op##_drm_amrn(emit, src_reg, dst_reg);     \
    }

OP_DRM_AMRN(fmov)

#define OP_DRM_A_R0_RN(op)                                  \
    static void emit_##op##_drm_a_r0_rn(void) {             \
        CHECK_DRN(1);                                       \
        CHECK_R0(5);                                        \
        CHECK_RN(7);                                        \
        int src_reg = tokens[1].val.reg_idx;                \
        int dst_reg = tokens[7].val.reg_idx;                \
        sh4asm_bin_##op##_drm_a_r0_rn(emit, src_reg, dst_reg); \
    }

OP_DRM_A_R0_RN(fmov)

#define OP_XDM_ARN(op)                                      \
    static void emit_##op##_xdm_arn(void) {                 \
        CHECK_XDN(1);                                       \
        CHECK_RN(4);                                        \
        int src_reg = tokens[1].val.reg_idx;                \
        int dst_reg = tokens[4].val.reg_idx;                \
        sh4asm_bin_##op##_xdm_arn(emit, src_reg, dst_reg);     \
    }

OP_XDM_ARN(fmov)

#define OP_XDM_AMRN(op)                                      \
    static void emit_##op##_xdm_amrn(void) {                 \
        CHECK_XDN(1);                                        \
        CHECK_RN(5);                                         \
        int src_reg = tokens[1].val.reg_idx;                 \
        int dst_reg = tokens[5].val.reg_idx;                 \
        sh4asm_bin_##op##_xdm_amrn(emit, src_reg, dst_reg);     \
    }

OP_XDM_AMRN(fmov)

#define OP_XDM_A_R0_RN(op)                                  \
    static void emit_##op##_xdm_a_r0_rn(void) {             \
        CHECK_XDN(1);                                       \
        CHECK_R0(5);                                        \
        CHECK_RN(7);                                        \
        int src_reg = tokens[1].val.reg_idx;                \
        int dst_reg = tokens[7].val.reg_idx;                \
        sh4asm_bin_##op##_xdm_a_r0_rn(emit, src_reg, dst_reg); \
    }

OP_XDM_A_R0_RN(fmov)

#define OP_DRN(op)                              \
    static void emit_##op##_drn(void) {         \
        CHECK_DRN(1);                           \
        int src_reg = tokens[1].val.reg_idx;    \
        sh4asm_bin_##op##_drn(emit, src_reg);      \
    }

OP_DRN(fabs)
OP_DRN(fneg)
OP_DRN(fsqrt)

#define OP_DRM_FPUL(op)                         \
    static void emit_##op##_drm_fpul(void) {    \
        CHECK_DRN(1);                           \
        int src_reg = tokens[1].val.reg_idx;    \
        sh4asm_bin_##op##_drm_fpul(emit, src_reg); \
    }

OP_DRM_FPUL(fcnvds)
OP_DRM_FPUL(ftrc)

#define OP_FPUL_DRN(op)                         \
    static void emit_##op##_fpul_drn(void) {    \
        CHECK_DRN(3);                           \
        int src_reg = tokens[3].val.reg_idx;    \
        sh4asm_bin_##op##_fpul_drn(emit, src_reg); \
    }

OP_FPUL_DRN(fcnvsd)
OP_FPUL_DRN(float)
OP_FPUL_DRN(fsca)

#define OP_FVM_FVN(op)                                  \
    static void emit_##op##_fvm_fvn(void) {             \
        CHECK_FVN(1);                                   \
        CHECK_FVN(3);                                   \
        int src_reg = tokens[1].val.reg_idx;            \
        int dst_reg = tokens[3].val.reg_idx;            \
        sh4asm_bin_##op##_fvm_fvn(emit, src_reg, dst_reg); \
    }

OP_FVM_FVN(fipr)

#define OP_XMTRX_FVN(op)                        \
    static void emit_##op##_xmtrx_fvn(void) {   \
        CHECK_FVN(3);                           \
        int reg_no = tokens[3].val.reg_idx;     \
        sh4asm_bin_##op##_xmtrx_fvn(emit, reg_no); \
    }

OP_XMTRX_FVN(ftrv)

struct pattern {
    parser_emit_func emit;
    enum tok_tp toks[MAX_TOKENS];
} const tok_ptrns[] = {
    // opcodes which take no arguments (noarg)
    { emit_div0u, { SH4ASM_TOK_DIV0U, SH4ASM_TOK_NEWLINE } },
    { emit_rts, { SH4ASM_TOK_RTS, SH4ASM_TOK_NEWLINE } },
    { emit_clrmac, { SH4ASM_TOK_CLRMAC, SH4ASM_TOK_NEWLINE } },
    { emit_clrs, { SH4ASM_TOK_CLRS, SH4ASM_TOK_NEWLINE } },
    { emit_clrt, { SH4ASM_TOK_CLRT, SH4ASM_TOK_NEWLINE } },
    { emit_ldtlb, { SH4ASM_TOK_LDTLB, SH4ASM_TOK_NEWLINE } },
    { emit_nop, { SH4ASM_TOK_NOP, SH4ASM_TOK_NEWLINE } },
    { emit_rte, { SH4ASM_TOK_RTE, SH4ASM_TOK_NEWLINE } },
    { emit_sets, { SH4ASM_TOK_SETS, SH4ASM_TOK_NEWLINE } },
    { emit_sett, { SH4ASM_TOK_SETT, SH4ASM_TOK_NEWLINE } },
    { emit_sleep, { SH4ASM_TOK_SLEEP, SH4ASM_TOK_NEWLINE } },
    { emit_frchg, { SH4ASM_TOK_FRCHG, SH4ASM_TOK_NEWLINE } },
    { emit_fschg, { SH4ASM_TOK_FSCHG, SH4ASM_TOK_NEWLINE } },

    { emit_movt_rn, { SH4ASM_TOK_MOVT, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_cmppz_rn, { SH4ASM_TOK_CMPPZ, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_cmppl_rn, { SH4ASM_TOK_CMPPL, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_dt_rn, { SH4ASM_TOK_DT, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_rotl_rn, { SH4ASM_TOK_ROTL, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_rotr_rn, { SH4ASM_TOK_ROTR, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_rotcl_rn, { SH4ASM_TOK_ROTCL, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_rotcr_rn, { SH4ASM_TOK_ROTCR, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_shal_rn, { SH4ASM_TOK_SHAL, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_shar_rn, { SH4ASM_TOK_SHAR, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_shll_rn, { SH4ASM_TOK_SHLL, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_shlr_rn, { SH4ASM_TOK_SHLR, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_shll2_rn, { SH4ASM_TOK_SHLL2, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_shlr2_rn, { SH4ASM_TOK_SHLR2, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_shll8_rn, { SH4ASM_TOK_SHLL8, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_shlr8_rn, { SH4ASM_TOK_SHLR8, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_shll16_rn, { SH4ASM_TOK_SHLL16, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_shlr16_rn, { SH4ASM_TOK_SHLR16, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_braf_rn, { SH4ASM_TOK_BRAF, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_bsrf_rn, { SH4ASM_TOK_BSRF, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },

    { emit_tasb_arn, { SH4ASM_TOK_TASB, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_ocbi_arn, { SH4ASM_TOK_OCBI, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_ocbp_arn, { SH4ASM_TOK_OCBP, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_ocbwb_arn, { SH4ASM_TOK_OCBWB, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_pref_arn, { SH4ASM_TOK_PREF, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_jmp_arn, { SH4ASM_TOK_JMP, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_jsr_arn, { SH4ASM_TOK_JSR, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },

    { emit_ldc_rm_sr, { SH4ASM_TOK_LDC, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_SR, SH4ASM_TOK_NEWLINE } },
    { emit_ldc_rm_gbr, { SH4ASM_TOK_LDC, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_GBR, SH4ASM_TOK_NEWLINE } },
    { emit_ldc_rm_vbr, { SH4ASM_TOK_LDC, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_VBR, SH4ASM_TOK_NEWLINE } },
    { emit_ldc_rm_ssr, { SH4ASM_TOK_LDC, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_SSR, SH4ASM_TOK_NEWLINE } },
    { emit_ldc_rm_spc, { SH4ASM_TOK_LDC, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_SPC, SH4ASM_TOK_NEWLINE } },
    { emit_ldc_rm_dbr, { SH4ASM_TOK_LDC, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_DBR, SH4ASM_TOK_NEWLINE } },
    { emit_lds_rm_mach, { SH4ASM_TOK_LDS, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_MACH, SH4ASM_TOK_NEWLINE } },
    { emit_lds_rm_macl, { SH4ASM_TOK_LDS, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_MACL, SH4ASM_TOK_NEWLINE } },
    { emit_lds_rm_pr, { SH4ASM_TOK_LDS, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_PR, SH4ASM_TOK_NEWLINE } },
    { emit_lds_rm_fpscr, { SH4ASM_TOK_LDS, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_FPSCR, SH4ASM_TOK_NEWLINE } },
    { emit_lds_rm_fpul, { SH4ASM_TOK_LDS, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_FPUL, SH4ASM_TOK_NEWLINE } },

    { emit_stc_sr_rn, { SH4ASM_TOK_STC, SH4ASM_TOK_SR, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_stc_gbr_rn, { SH4ASM_TOK_STC, SH4ASM_TOK_GBR, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_stc_vbr_rn, { SH4ASM_TOK_STC, SH4ASM_TOK_VBR, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_stc_ssr_rn, { SH4ASM_TOK_STC, SH4ASM_TOK_SSR, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_stc_spc_rn, { SH4ASM_TOK_STC, SH4ASM_TOK_SPC, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_stc_sgr_rn, { SH4ASM_TOK_STC, SH4ASM_TOK_SGR, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_stc_dbr_rn, { SH4ASM_TOK_STC, SH4ASM_TOK_DBR, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_sts_mach_rn, { SH4ASM_TOK_STS, SH4ASM_TOK_MACH, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_sts_macl_rn, { SH4ASM_TOK_STS, SH4ASM_TOK_MACL, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_sts_pr_rn, { SH4ASM_TOK_STS, SH4ASM_TOK_PR, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_sts_fpscr_rn, { SH4ASM_TOK_STS, SH4ASM_TOK_FPSCR, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_sts_fpul_rn, { SH4ASM_TOK_STS, SH4ASM_TOK_FPUL, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },

    { emit_ldcl_armp_sr, { SH4ASM_TOK_LDCL, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_PLUS, SH4ASM_TOK_COMMA, SH4ASM_TOK_SR, SH4ASM_TOK_NEWLINE } },
    { emit_ldcl_armp_gbr, { SH4ASM_TOK_LDCL, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_PLUS, SH4ASM_TOK_COMMA, SH4ASM_TOK_GBR, SH4ASM_TOK_NEWLINE } },
    { emit_ldcl_armp_vbr, { SH4ASM_TOK_LDCL, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_PLUS, SH4ASM_TOK_COMMA, SH4ASM_TOK_VBR, SH4ASM_TOK_NEWLINE } },
    { emit_ldcl_armp_ssr, { SH4ASM_TOK_LDCL, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_PLUS, SH4ASM_TOK_COMMA, SH4ASM_TOK_SSR, SH4ASM_TOK_NEWLINE } },
    { emit_ldcl_armp_spc, { SH4ASM_TOK_LDCL, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_PLUS, SH4ASM_TOK_COMMA, SH4ASM_TOK_SPC, SH4ASM_TOK_NEWLINE } },
    { emit_ldcl_armp_dbr, { SH4ASM_TOK_LDCL, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_PLUS, SH4ASM_TOK_COMMA, SH4ASM_TOK_DBR, SH4ASM_TOK_NEWLINE } },
    { emit_ldsl_armp_mach, { SH4ASM_TOK_LDSL, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_PLUS, SH4ASM_TOK_COMMA, SH4ASM_TOK_MACH, SH4ASM_TOK_NEWLINE } },
    { emit_ldsl_armp_macl, { SH4ASM_TOK_LDSL, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_PLUS, SH4ASM_TOK_COMMA, SH4ASM_TOK_MACL, SH4ASM_TOK_NEWLINE } },
    { emit_ldsl_armp_pr, { SH4ASM_TOK_LDSL, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_PLUS, SH4ASM_TOK_COMMA, SH4ASM_TOK_PR, SH4ASM_TOK_NEWLINE } },
    { emit_ldsl_armp_fpscr, { SH4ASM_TOK_LDSL, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_PLUS, SH4ASM_TOK_COMMA, SH4ASM_TOK_FPSCR, SH4ASM_TOK_NEWLINE } },
    { emit_ldsl_armp_fpul, { SH4ASM_TOK_LDSL, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_PLUS, SH4ASM_TOK_COMMA, SH4ASM_TOK_FPUL, SH4ASM_TOK_NEWLINE } },

    { emit_stcl_sr_amrn, { SH4ASM_TOK_STCL, SH4ASM_TOK_SR, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_MINUS, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_stcl_gbr_amrn, { SH4ASM_TOK_STCL, SH4ASM_TOK_GBR, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_MINUS, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_stcl_vbr_amrn, { SH4ASM_TOK_STCL, SH4ASM_TOK_VBR, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_MINUS, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_stcl_ssr_amrn, { SH4ASM_TOK_STCL, SH4ASM_TOK_SSR, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_MINUS, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_stcl_spc_amrn, { SH4ASM_TOK_STCL, SH4ASM_TOK_SPC, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_MINUS, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_stcl_sgr_amrn, { SH4ASM_TOK_STCL, SH4ASM_TOK_SGR, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_MINUS, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_stcl_dbr_amrn, { SH4ASM_TOK_STCL, SH4ASM_TOK_DBR, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_MINUS, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_stsl_mach_amrn, { SH4ASM_TOK_STSL, SH4ASM_TOK_MACH, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_MINUS, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_stsl_macl_amrn, { SH4ASM_TOK_STSL, SH4ASM_TOK_MACL, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_MINUS, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_stsl_pr_amrn, { SH4ASM_TOK_STSL, SH4ASM_TOK_PR, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_MINUS, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_stsl_fpscr_amrn, { SH4ASM_TOK_STSL, SH4ASM_TOK_FPSCR, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_MINUS, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_stsl_fpul_amrn, { SH4ASM_TOK_STSL, SH4ASM_TOK_FPUL, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_MINUS, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },

    { emit_movcal_r0_arn, { SH4ASM_TOK_MOVCAL, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },

    { emit_fldi0_frn, { SH4ASM_TOK_FLDI0, SH4ASM_TOK_FRN, SH4ASM_TOK_NEWLINE } },
    { emit_fldi1_frn, { SH4ASM_TOK_FLDI1, SH4ASM_TOK_FRN, SH4ASM_TOK_NEWLINE } },
    { emit_fabs_frn, { SH4ASM_TOK_FABS, SH4ASM_TOK_FRN, SH4ASM_TOK_NEWLINE } },
    { emit_fneg_frn, { SH4ASM_TOK_FNEG, SH4ASM_TOK_FRN, SH4ASM_TOK_NEWLINE } },
    { emit_fsqrt_frn, { SH4ASM_TOK_FSQRT, SH4ASM_TOK_FRN, SH4ASM_TOK_NEWLINE } },
    { emit_fsrra_frn, { SH4ASM_TOK_FSRRA, SH4ASM_TOK_FRN, SH4ASM_TOK_NEWLINE } },

    { emit_flds_frm_fpul, { SH4ASM_TOK_FLDS, SH4ASM_TOK_FRN, SH4ASM_TOK_COMMA, SH4ASM_TOK_FPUL, SH4ASM_TOK_NEWLINE } },
    { emit_ftrc_frm_fpul, { SH4ASM_TOK_FTRC, SH4ASM_TOK_FRN, SH4ASM_TOK_COMMA, SH4ASM_TOK_FPUL, SH4ASM_TOK_NEWLINE } },

    { emit_fsts_fpul_frn, { SH4ASM_TOK_FSTS, SH4ASM_TOK_FPUL, SH4ASM_TOK_COMMA, SH4ASM_TOK_FRN, SH4ASM_TOK_NEWLINE } },
    { emit_float_fpul_frn, { SH4ASM_TOK_FLOAT, SH4ASM_TOK_FPUL, SH4ASM_TOK_COMMA, SH4ASM_TOK_FRN, SH4ASM_TOK_NEWLINE } },

    { emit_cmpeq_imm8_r0, { SH4ASM_TOK_CMPEQ, SH4ASM_TOK_IMM, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_and_imm8_r0, { SH4ASM_TOK_AND, SH4ASM_TOK_IMM, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_or_imm8_r0, { SH4ASM_TOK_OR, SH4ASM_TOK_IMM, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_tst_imm8_r0, { SH4ASM_TOK_TST, SH4ASM_TOK_IMM, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_xor_imm8_r0, { SH4ASM_TOK_XOR, SH4ASM_TOK_IMM, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },

    { emit_andb_imm8_a_r0_gbr,
      { SH4ASM_TOK_ANDB, SH4ASM_TOK_IMM, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_OPENPAREN, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA,
        SH4ASM_TOK_GBR, SH4ASM_TOK_CLOSEPAREN, SH4ASM_TOK_NEWLINE } },
    { emit_orb_imm8_a_r0_gbr,
      { SH4ASM_TOK_ORB, SH4ASM_TOK_IMM, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_OPENPAREN, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA,
        SH4ASM_TOK_GBR, SH4ASM_TOK_CLOSEPAREN, SH4ASM_TOK_NEWLINE } },
    { emit_tstb_imm8_a_r0_gbr,
      { SH4ASM_TOK_TSTB, SH4ASM_TOK_IMM, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_OPENPAREN, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA,
        SH4ASM_TOK_GBR, SH4ASM_TOK_CLOSEPAREN, SH4ASM_TOK_NEWLINE } },
    { emit_xorb_imm8_a_r0_gbr,
      { SH4ASM_TOK_XORB, SH4ASM_TOK_IMM, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_OPENPAREN, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA,
        SH4ASM_TOK_GBR, SH4ASM_TOK_CLOSEPAREN, SH4ASM_TOK_NEWLINE } },

    { emit_bf_offs8, { SH4ASM_TOK_BF, SH4ASM_TOK_DISP, SH4ASM_TOK_NEWLINE } },
    { emit_bfs_offs8, { SH4ASM_TOK_BFS, SH4ASM_TOK_DISP, SH4ASM_TOK_NEWLINE } },
    { emit_bt_offs8, { SH4ASM_TOK_BT, SH4ASM_TOK_DISP, SH4ASM_TOK_NEWLINE } },
    { emit_bts_offs8, { SH4ASM_TOK_BTS, SH4ASM_TOK_DISP, SH4ASM_TOK_NEWLINE } },

    { emit_trapa_imm8, { SH4ASM_TOK_TRAPA, SH4ASM_TOK_IMM, SH4ASM_TOK_NEWLINE } },

    { emit_movb_r0_a_disp8_gbr,
      { SH4ASM_TOK_MOVB, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_OPENPAREN, SH4ASM_TOK_DISP,
        SH4ASM_TOK_COMMA, SH4ASM_TOK_GBR, SH4ASM_TOK_CLOSEPAREN, SH4ASM_TOK_NEWLINE } },
    { emit_movw_r0_a_disp8_gbr,
      { SH4ASM_TOK_MOVW, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_OPENPAREN, SH4ASM_TOK_DISP,
        SH4ASM_TOK_COMMA, SH4ASM_TOK_GBR, SH4ASM_TOK_CLOSEPAREN, SH4ASM_TOK_NEWLINE } },
    { emit_movl_r0_a_disp8_gbr,
      { SH4ASM_TOK_MOVL, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_OPENPAREN, SH4ASM_TOK_DISP,
        SH4ASM_TOK_COMMA, SH4ASM_TOK_GBR, SH4ASM_TOK_CLOSEPAREN, SH4ASM_TOK_NEWLINE } },

    { emit_movb_a_disp8_gbr_r0,
      { SH4ASM_TOK_MOVB, SH4ASM_TOK_AT, SH4ASM_TOK_OPENPAREN, SH4ASM_TOK_DISP,
        SH4ASM_TOK_COMMA, SH4ASM_TOK_GBR, SH4ASM_TOK_CLOSEPAREN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_movw_a_disp8_gbr_r0,
      { SH4ASM_TOK_MOVW, SH4ASM_TOK_AT, SH4ASM_TOK_OPENPAREN, SH4ASM_TOK_DISP,
        SH4ASM_TOK_COMMA, SH4ASM_TOK_GBR, SH4ASM_TOK_CLOSEPAREN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_movl_a_disp8_gbr_r0,
      { SH4ASM_TOK_MOVL, SH4ASM_TOK_AT, SH4ASM_TOK_OPENPAREN, SH4ASM_TOK_DISP,
        SH4ASM_TOK_COMMA, SH4ASM_TOK_GBR, SH4ASM_TOK_CLOSEPAREN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },

    { emit_mova_a_offs8_pc_r0,
      { SH4ASM_TOK_MOVA, SH4ASM_TOK_AT, SH4ASM_TOK_OPENPAREN, SH4ASM_TOK_DISP,
        SH4ASM_TOK_COMMA, SH4ASM_TOK_PC, SH4ASM_TOK_CLOSEPAREN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },

    { emit_bra_offs12, { SH4ASM_TOK_BRA, SH4ASM_TOK_DISP, SH4ASM_TOK_NEWLINE } },
    { emit_bsr_offs12, { SH4ASM_TOK_BSR, SH4ASM_TOK_DISP, SH4ASM_TOK_NEWLINE } },

    { emit_mov_imm8_rn, { SH4ASM_TOK_MOV, SH4ASM_TOK_IMM, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_add_imm8_rn, { SH4ASM_TOK_ADD, SH4ASM_TOK_IMM, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },

    { emit_movw_a_offs8_pc_rn,
      { SH4ASM_TOK_MOVW, SH4ASM_TOK_AT, SH4ASM_TOK_OPENPAREN, SH4ASM_TOK_DISP, SH4ASM_TOK_COMMA, SH4ASM_TOK_PC,
        SH4ASM_TOK_CLOSEPAREN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_movl_a_offs8_pc_rn,
      { SH4ASM_TOK_MOVL, SH4ASM_TOK_AT, SH4ASM_TOK_OPENPAREN, SH4ASM_TOK_DISP, SH4ASM_TOK_COMMA, SH4ASM_TOK_PC,
        SH4ASM_TOK_CLOSEPAREN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },

    { emit_mov_rm_rn, { SH4ASM_TOK_MOV, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_swapb_rm_rn, { SH4ASM_TOK_SWAPB, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_swapw_rm_rn, { SH4ASM_TOK_SWAPW, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_xtrct_rm_rn, { SH4ASM_TOK_XTRCT, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_add_rm_rn, { SH4ASM_TOK_ADD, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_addc_rm_rn, { SH4ASM_TOK_ADDC, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_addv_rm_rn, { SH4ASM_TOK_ADDV, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_cmpeq_rm_rn, { SH4ASM_TOK_CMPEQ, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_cmphs_rm_rn, { SH4ASM_TOK_CMPHS, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_cmpge_rm_rn, { SH4ASM_TOK_CMPGE, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_cmphi_rm_rn, { SH4ASM_TOK_CMPHI, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_cmpgt_rm_rn, { SH4ASM_TOK_CMPGT, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_cmpstr_rm_rn, { SH4ASM_TOK_CMPSTR, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_div1_rm_rn, { SH4ASM_TOK_DIV1, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_div0s_rm_rn, { SH4ASM_TOK_DIV0S, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_dmulsl_rm_rn, { SH4ASM_TOK_DMULSL, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_dmulul_rm_rn, { SH4ASM_TOK_DMULUL, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_extsb_rm_rn, { SH4ASM_TOK_EXTSB, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_extsw_rm_rn, { SH4ASM_TOK_EXTSW, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_extub_rm_rn, { SH4ASM_TOK_EXTUB, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_extuw_rm_rn, { SH4ASM_TOK_EXTUW, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_mull_rm_rn, { SH4ASM_TOK_MULL, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_mulsw_rm_rn, { SH4ASM_TOK_MULSW, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_muluw_rm_rn, { SH4ASM_TOK_MULUW, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_neg_rm_rn, { SH4ASM_TOK_NEG, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_negc_rm_rn, { SH4ASM_TOK_NEGC, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_sub_rm_rn, { SH4ASM_TOK_SUB, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_subc_rm_rn, { SH4ASM_TOK_SUBC, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_subv_rm_rn, { SH4ASM_TOK_SUBV, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_and_rm_rn, { SH4ASM_TOK_AND, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_not_rm_rn, { SH4ASM_TOK_NOT, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_or_rm_rn, { SH4ASM_TOK_OR, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_tst_rm_rn, { SH4ASM_TOK_TST, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_xor_rm_rn, { SH4ASM_TOK_XOR, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_shad_rm_rn, { SH4ASM_TOK_SHAD, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_shld_rm_rn, { SH4ASM_TOK_SHLD, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },

    { emit_movb_rm_a_r0_rn,
      { SH4ASM_TOK_MOVB, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_OPENPAREN, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA,
        SH4ASM_TOK_RN, SH4ASM_TOK_CLOSEPAREN, SH4ASM_TOK_NEWLINE } },
    { emit_movw_rm_a_r0_rn,
      { SH4ASM_TOK_MOVW, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_OPENPAREN, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA,
        SH4ASM_TOK_RN, SH4ASM_TOK_CLOSEPAREN, SH4ASM_TOK_NEWLINE } },
    { emit_movl_rm_a_r0_rn,
      { SH4ASM_TOK_MOVL, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_OPENPAREN, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA,
        SH4ASM_TOK_RN, SH4ASM_TOK_CLOSEPAREN, SH4ASM_TOK_NEWLINE } },

    { emit_movb_a_r0_rm_rn,
      { SH4ASM_TOK_MOVB, SH4ASM_TOK_AT, SH4ASM_TOK_OPENPAREN, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN,
        SH4ASM_TOK_CLOSEPAREN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_movw_a_r0_rm_rn,
      { SH4ASM_TOK_MOVW, SH4ASM_TOK_AT, SH4ASM_TOK_OPENPAREN, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN,
        SH4ASM_TOK_CLOSEPAREN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_movl_a_r0_rm_rn,
      { SH4ASM_TOK_MOVL, SH4ASM_TOK_AT, SH4ASM_TOK_OPENPAREN, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN,
        SH4ASM_TOK_CLOSEPAREN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },

    { emit_movb_rm_arn,
      { SH4ASM_TOK_MOVB, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_movw_rm_arn,
      { SH4ASM_TOK_MOVW, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_movl_rm_arn,
      { SH4ASM_TOK_MOVL, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },

    { emit_movb_arm_rn,
      { SH4ASM_TOK_MOVB, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_movw_arm_rn,
      { SH4ASM_TOK_MOVW, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_movl_arm_rn,
      { SH4ASM_TOK_MOVL, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },

    { emit_movb_rm_amrn,
      { SH4ASM_TOK_MOVB, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_MINUS, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_movw_rm_amrn,
      { SH4ASM_TOK_MOVW, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_MINUS, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_movl_rm_amrn,
      { SH4ASM_TOK_MOVL, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_MINUS, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },

    { emit_movb_armp_rn,
      { SH4ASM_TOK_MOVB, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_PLUS, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_movw_armp_rn,
      { SH4ASM_TOK_MOVW, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_PLUS, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_movl_armp_rn,
      { SH4ASM_TOK_MOVL, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_PLUS, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },

    { emit_macl_armp_arnp,
      { SH4ASM_TOK_MAC_DOT_L, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_PLUS, SH4ASM_TOK_COMMA,
        SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_PLUS, SH4ASM_TOK_NEWLINE } },
    { emit_macw_armp_arnp,
      { SH4ASM_TOK_MAC_DOT_W, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_PLUS, SH4ASM_TOK_COMMA,
        SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_PLUS, SH4ASM_TOK_NEWLINE } },

    { emit_fmov_frm_frn,
      { SH4ASM_TOK_FMOV, SH4ASM_TOK_FRN, SH4ASM_TOK_COMMA, SH4ASM_TOK_FRN, SH4ASM_TOK_NEWLINE } },
    { emit_fadd_frm_frn,
      { SH4ASM_TOK_FADD, SH4ASM_TOK_FRN, SH4ASM_TOK_COMMA, SH4ASM_TOK_FRN, SH4ASM_TOK_NEWLINE } },
    { emit_fcmpeq_frm_frn,
      { SH4ASM_TOK_FCMPEQ, SH4ASM_TOK_FRN, SH4ASM_TOK_COMMA, SH4ASM_TOK_FRN, SH4ASM_TOK_NEWLINE } },
    { emit_fcmpgt_frm_frn,
      { SH4ASM_TOK_FCMPGT, SH4ASM_TOK_FRN, SH4ASM_TOK_COMMA, SH4ASM_TOK_FRN, SH4ASM_TOK_NEWLINE } },
    { emit_fdiv_frm_frn,
      { SH4ASM_TOK_FDIV, SH4ASM_TOK_FRN, SH4ASM_TOK_COMMA, SH4ASM_TOK_FRN, SH4ASM_TOK_NEWLINE } },
    { emit_fmul_frm_frn,
      { SH4ASM_TOK_FMUL, SH4ASM_TOK_FRN, SH4ASM_TOK_COMMA, SH4ASM_TOK_FRN, SH4ASM_TOK_NEWLINE } },
    { emit_fsub_frm_frn,
      { SH4ASM_TOK_FSUB, SH4ASM_TOK_FRN, SH4ASM_TOK_COMMA, SH4ASM_TOK_FRN, SH4ASM_TOK_NEWLINE } },

    { emit_fmovs_arm_frn,
      { SH4ASM_TOK_FMOVS, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_FRN,
        SH4ASM_TOK_NEWLINE } },

    { emit_fmovs_a_r0_rm_frn,
      { SH4ASM_TOK_FMOVS, SH4ASM_TOK_AT, SH4ASM_TOK_OPENPAREN, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN,
        SH4ASM_TOK_CLOSEPAREN, SH4ASM_TOK_COMMA, SH4ASM_TOK_FRN, SH4ASM_TOK_NEWLINE } },

    { emit_fmovs_armp_frn,
      { SH4ASM_TOK_FMOVS, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_PLUS, SH4ASM_TOK_COMMA, SH4ASM_TOK_FRN, SH4ASM_TOK_NEWLINE } },

    { emit_fmovs_frm_arn,
      { SH4ASM_TOK_FMOVS, SH4ASM_TOK_FRN, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },

    { emit_fmovs_frm_amrn,
      { SH4ASM_TOK_FMOVS, SH4ASM_TOK_FRN, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT,
        SH4ASM_TOK_MINUS, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },

    { emit_fmovs_frm_a_r0_rn,
      { SH4ASM_TOK_FMOVS, SH4ASM_TOK_FRN, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_OPENPAREN, SH4ASM_TOK_RN,
        SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_CLOSEPAREN, SH4ASM_TOK_NEWLINE } },

    { emit_fmac_fr0_frm_frn,
      { SH4ASM_TOK_FMAC, SH4ASM_TOK_FRN, SH4ASM_TOK_COMMA, SH4ASM_TOK_FRN,
        SH4ASM_TOK_COMMA, SH4ASM_TOK_FRN, SH4ASM_TOK_NEWLINE } },

    { emit_ldc_rm_rn_bank,
      { SH4ASM_TOK_LDC, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN_BANK, SH4ASM_TOK_NEWLINE } },

    { emit_stc_rm_bank_rn,
      { SH4ASM_TOK_STC, SH4ASM_TOK_RN_BANK, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },

    { emit_ldcl_armp_rn_bank,
      { SH4ASM_TOK_LDCL, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_PLUS, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN_BANK,
        SH4ASM_TOK_NEWLINE } },

    { emit_stcl_rm_bank_amrn,
      { SH4ASM_TOK_STCL, SH4ASM_TOK_RN_BANK, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_MINUS, SH4ASM_TOK_RN,
        SH4ASM_TOK_NEWLINE } },

    { emit_movb_r0_a_disp4_rn,
      { SH4ASM_TOK_MOVB, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_OPENPAREN, SH4ASM_TOK_DISP,
        SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_CLOSEPAREN, SH4ASM_TOK_NEWLINE } },
    { emit_movw_r0_a_disp4_rn,
      { SH4ASM_TOK_MOVW, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_OPENPAREN, SH4ASM_TOK_DISP,
        SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_CLOSEPAREN, SH4ASM_TOK_NEWLINE } },

    { emit_movb_a_disp4_rm_r0,
      { SH4ASM_TOK_MOVB, SH4ASM_TOK_AT, SH4ASM_TOK_OPENPAREN, SH4ASM_TOK_DISP, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN,
        SH4ASM_TOK_CLOSEPAREN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },
    { emit_movw_a_disp4_rm_r0,
      { SH4ASM_TOK_MOVW, SH4ASM_TOK_AT, SH4ASM_TOK_OPENPAREN, SH4ASM_TOK_DISP, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN,
        SH4ASM_TOK_CLOSEPAREN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },

    { emit_movl_rm_a_disp4_rn,
      { SH4ASM_TOK_MOVL, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_OPENPAREN, SH4ASM_TOK_DISP,
        SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_CLOSEPAREN, SH4ASM_TOK_NEWLINE } },

    { emit_movl_a_disp4_rm_rn,
      { SH4ASM_TOK_MOVL, SH4ASM_TOK_AT, SH4ASM_TOK_OPENPAREN, SH4ASM_TOK_DISP, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN,
        SH4ASM_TOK_CLOSEPAREN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },

    { emit_fmov_drm_drn,
      { SH4ASM_TOK_FMOV, SH4ASM_TOK_DRN, SH4ASM_TOK_COMMA, SH4ASM_TOK_DRN, SH4ASM_TOK_NEWLINE } },
    { emit_fadd_drm_drn,
      { SH4ASM_TOK_FADD, SH4ASM_TOK_DRN, SH4ASM_TOK_COMMA, SH4ASM_TOK_DRN, SH4ASM_TOK_NEWLINE } },
    { emit_fcmpeq_drm_drn,
      { SH4ASM_TOK_FCMPEQ, SH4ASM_TOK_DRN, SH4ASM_TOK_COMMA, SH4ASM_TOK_DRN, SH4ASM_TOK_NEWLINE } },
    { emit_fcmpgt_drm_drn,
      { SH4ASM_TOK_FCMPGT, SH4ASM_TOK_DRN, SH4ASM_TOK_COMMA, SH4ASM_TOK_DRN, SH4ASM_TOK_NEWLINE } },
    { emit_fdiv_drm_drn,
      { SH4ASM_TOK_FDIV, SH4ASM_TOK_DRN, SH4ASM_TOK_COMMA, SH4ASM_TOK_DRN, SH4ASM_TOK_NEWLINE } },
    { emit_fmul_drm_drn,
      { SH4ASM_TOK_FMUL, SH4ASM_TOK_DRN, SH4ASM_TOK_COMMA, SH4ASM_TOK_DRN, SH4ASM_TOK_NEWLINE } },
    { emit_fsub_drm_drn,
      { SH4ASM_TOK_FSUB, SH4ASM_TOK_DRN, SH4ASM_TOK_COMMA, SH4ASM_TOK_DRN, SH4ASM_TOK_NEWLINE } },

    { emit_fmov_drm_xdn,
      { SH4ASM_TOK_FMOV, SH4ASM_TOK_DRN, SH4ASM_TOK_COMMA, SH4ASM_TOK_XDN, SH4ASM_TOK_NEWLINE } },

    { emit_fmov_xdm_drn,
      { SH4ASM_TOK_FMOV, SH4ASM_TOK_XDN, SH4ASM_TOK_COMMA, SH4ASM_TOK_DRN, SH4ASM_TOK_NEWLINE } },

    { emit_fmov_xdm_xdn,
      { SH4ASM_TOK_FMOV, SH4ASM_TOK_XDN, SH4ASM_TOK_COMMA, SH4ASM_TOK_XDN, SH4ASM_TOK_NEWLINE } },

    { emit_fmov_arm_drn,
      { SH4ASM_TOK_FMOV, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_DRN, SH4ASM_TOK_NEWLINE } },

    { emit_fmov_a_r0_rm_drn,
      { SH4ASM_TOK_FMOV, SH4ASM_TOK_AT, SH4ASM_TOK_OPENPAREN, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN,
        SH4ASM_TOK_CLOSEPAREN, SH4ASM_TOK_COMMA, SH4ASM_TOK_DRN, SH4ASM_TOK_NEWLINE } },

    { emit_fmov_armp_drn,
      { SH4ASM_TOK_FMOV, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_PLUS, SH4ASM_TOK_COMMA, SH4ASM_TOK_DRN, SH4ASM_TOK_NEWLINE } },

    { emit_fmov_arm_xdn,
      { SH4ASM_TOK_FMOV, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_XDN, SH4ASM_TOK_NEWLINE } },

    { emit_fmov_armp_xdn,
      { SH4ASM_TOK_FMOV, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_PLUS, SH4ASM_TOK_COMMA, SH4ASM_TOK_XDN, SH4ASM_TOK_NEWLINE } },

    { emit_fmov_a_r0_rm_xdn,
      { SH4ASM_TOK_FMOV, SH4ASM_TOK_AT, SH4ASM_TOK_OPENPAREN, SH4ASM_TOK_RN, SH4ASM_TOK_COMMA, SH4ASM_TOK_RN,
        SH4ASM_TOK_CLOSEPAREN, SH4ASM_TOK_COMMA, SH4ASM_TOK_XDN, SH4ASM_TOK_NEWLINE } },

    { emit_fmov_drm_amrn,
      { SH4ASM_TOK_FMOV, SH4ASM_TOK_DRN, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT,
        SH4ASM_TOK_MINUS, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },

    { emit_fmov_drm_arn,
      { SH4ASM_TOK_FMOV, SH4ASM_TOK_DRN, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },

    { emit_fmov_drm_a_r0_rn,
      { SH4ASM_TOK_FMOV, SH4ASM_TOK_DRN, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_OPENPAREN, SH4ASM_TOK_RN,
        SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_CLOSEPAREN, SH4ASM_TOK_NEWLINE } },

    { emit_fmov_xdm_arn,
      { SH4ASM_TOK_FMOV, SH4ASM_TOK_XDN, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },

    { emit_fmov_xdm_amrn,
      { SH4ASM_TOK_FMOV, SH4ASM_TOK_XDN, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT,
        SH4ASM_TOK_MINUS, SH4ASM_TOK_RN, SH4ASM_TOK_NEWLINE } },

    { emit_fmov_xdm_a_r0_rn,
      { SH4ASM_TOK_FMOV, SH4ASM_TOK_XDN, SH4ASM_TOK_COMMA, SH4ASM_TOK_AT, SH4ASM_TOK_OPENPAREN, SH4ASM_TOK_RN,
        SH4ASM_TOK_COMMA, SH4ASM_TOK_RN, SH4ASM_TOK_CLOSEPAREN, SH4ASM_TOK_NEWLINE } },

    { emit_fabs_drn, { SH4ASM_TOK_FABS, SH4ASM_TOK_DRN, SH4ASM_TOK_NEWLINE } },
    { emit_fneg_drn, { SH4ASM_TOK_FNEG, SH4ASM_TOK_DRN, SH4ASM_TOK_NEWLINE } },
    { emit_fsqrt_drn, { SH4ASM_TOK_FSQRT, SH4ASM_TOK_DRN, SH4ASM_TOK_NEWLINE } },

    { emit_fcnvds_drm_fpul,
      { SH4ASM_TOK_FCNVDS, SH4ASM_TOK_DRN, SH4ASM_TOK_COMMA, SH4ASM_TOK_FPUL, SH4ASM_TOK_NEWLINE } },
    { emit_ftrc_drm_fpul,
      { SH4ASM_TOK_FTRC, SH4ASM_TOK_DRN, SH4ASM_TOK_COMMA, SH4ASM_TOK_FPUL, SH4ASM_TOK_NEWLINE } },

    { emit_fcnvsd_fpul_drn,
      { SH4ASM_TOK_FCNVSD, SH4ASM_TOK_FPUL, SH4ASM_TOK_COMMA, SH4ASM_TOK_DRN, SH4ASM_TOK_NEWLINE } },
    { emit_float_fpul_drn,
      { SH4ASM_TOK_FLOAT, SH4ASM_TOK_FPUL, SH4ASM_TOK_COMMA, SH4ASM_TOK_DRN, SH4ASM_TOK_NEWLINE } },
    { emit_fsca_fpul_drn,
      { SH4ASM_TOK_FSCA, SH4ASM_TOK_FPUL, SH4ASM_TOK_COMMA, SH4ASM_TOK_DRN, SH4ASM_TOK_NEWLINE } },

    { emit_fipr_fvm_fvn,
      { SH4ASM_TOK_FIPR, SH4ASM_TOK_FVN, SH4ASM_TOK_COMMA, SH4ASM_TOK_FVN, SH4ASM_TOK_NEWLINE } },

    { emit_ftrv_xmtrx_fvn,
      { SH4ASM_TOK_FTRV, SH4ASM_TOK_XMTRX, SH4ASM_TOK_COMMA, SH4ASM_TOK_FVN, SH4ASM_TOK_NEWLINE } },

    { NULL }
};

static void push_token(struct sh4asm_tok const *tk);
static void process_line(void);
static bool check_pattern(struct pattern const *ptrn);

void sh4asm_parser_input_token(struct sh4asm_tok const *tk) {
    if (tk->tp == SH4ASM_TOK_NEWLINE) {
        process_line();
        n_tokens = 0;
    } else {
        push_token(tk);
    }
}

void sh4asm_parser_set_emitter(sh4asm_bin_emit_handler_func em) {
    emit = em;
}

static void push_token(struct sh4asm_tok const *tk) {
    if (n_tokens >= MAX_TOKENS)
        sh4asm_error("too many tokens");
    tokens[n_tokens++] = *tk;
}

static void process_line(void) {
    struct pattern const *curs = tok_ptrns;

    while (curs->emit) {
        if (check_pattern(curs)) {
            curs->emit();
            return;
        }
        curs++;
    }

    printf("%u tokens\n\t", n_tokens);
    unsigned tok_idx;
    for (tok_idx = 0; tok_idx < n_tokens; tok_idx++)
        printf("%s ", sh4asm_tok_as_str(tokens + tok_idx));
    puts("\n");

    sh4asm_error("unrecognized pattern");
}

static bool check_pattern(struct pattern const *ptrn) {
    enum tok_tp const *cur_tok = ptrn->toks;
    unsigned tok_idx = 0;

    while (*cur_tok != SH4ASM_TOK_NEWLINE && tok_idx < n_tokens) {
        if (*cur_tok != tokens[tok_idx].tp)
            return false;

        cur_tok++;
        tok_idx++;
    }

    return (*cur_tok == SH4ASM_TOK_NEWLINE) && (tok_idx == n_tokens);
}

void sh4asm_parser_reset(void) {
    n_tokens = 0;
}
