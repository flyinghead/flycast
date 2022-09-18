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

#ifndef SH4ASM_LEXER_H_
#define SH4ASM_LEXER_H_

enum tok_tp {
    SH4ASM_TOK_COMMA,
    SH4ASM_TOK_OPENPAREN,
    SH4ASM_TOK_CLOSEPAREN,
    SH4ASM_TOK_AT,
    SH4ASM_TOK_NEWLINE,
    SH4ASM_TOK_PLUS,
    SH4ASM_TOK_MINUS,

    // opcodes
    SH4ASM_TOK_DIV0U,
    SH4ASM_TOK_RTS,
    SH4ASM_TOK_CLRMAC,
    SH4ASM_TOK_CLRS,
    SH4ASM_TOK_CLRT,
    SH4ASM_TOK_LDTLB,
    SH4ASM_TOK_NOP,
    SH4ASM_TOK_RTE,
    SH4ASM_TOK_SETS,
    SH4ASM_TOK_SETT,
    SH4ASM_TOK_SLEEP,
    SH4ASM_TOK_FRCHG,
    SH4ASM_TOK_FSCHG,
    SH4ASM_TOK_MOVT,
    SH4ASM_TOK_CMPPZ,
    SH4ASM_TOK_CMPPL,
    SH4ASM_TOK_DT,
    SH4ASM_TOK_ROTL,
    SH4ASM_TOK_ROTR,
    SH4ASM_TOK_ROTCL,
    SH4ASM_TOK_ROTCR,
    SH4ASM_TOK_SHAL,
    SH4ASM_TOK_SHAR,
    SH4ASM_TOK_SHLL,
    SH4ASM_TOK_SHLR,
    SH4ASM_TOK_SHLL2,
    SH4ASM_TOK_SHLR2,
    SH4ASM_TOK_SHLL8,
    SH4ASM_TOK_SHLR8,
    SH4ASM_TOK_SHLL16,
    SH4ASM_TOK_SHLR16,
    SH4ASM_TOK_BRAF,
    SH4ASM_TOK_BSRF,
    SH4ASM_TOK_CMPEQ,
    SH4ASM_TOK_ANDB,
    SH4ASM_TOK_AND,
    SH4ASM_TOK_ORB,
    SH4ASM_TOK_OR,
    SH4ASM_TOK_TST,
    SH4ASM_TOK_TSTB,
    SH4ASM_TOK_XOR,
    SH4ASM_TOK_XORB,
    SH4ASM_TOK_BF,
    SH4ASM_TOK_BFS,
    SH4ASM_TOK_BT,
    SH4ASM_TOK_BTS,
    SH4ASM_TOK_BRA,
    SH4ASM_TOK_BSR,
    SH4ASM_TOK_TRAPA,
    SH4ASM_TOK_TASB,
    SH4ASM_TOK_OCBI,
    SH4ASM_TOK_OCBP,
    SH4ASM_TOK_OCBWB,
    SH4ASM_TOK_PREF,
    SH4ASM_TOK_JMP,
    SH4ASM_TOK_JSR,
    SH4ASM_TOK_LDC,
    SH4ASM_TOK_STC,
    SH4ASM_TOK_LDCL,
    SH4ASM_TOK_STCL,
    SH4ASM_TOK_MOV,
    SH4ASM_TOK_ADD,
    SH4ASM_TOK_MOVW,
    SH4ASM_TOK_MOVL,
    SH4ASM_TOK_SWAPB,
    SH4ASM_TOK_SWAPW,
    SH4ASM_TOK_XTRCT,
    SH4ASM_TOK_ADDC,
    SH4ASM_TOK_ADDV,
    SH4ASM_TOK_CMPHS,
    SH4ASM_TOK_CMPGE,
    SH4ASM_TOK_CMPHI,
    SH4ASM_TOK_CMPGT,
    SH4ASM_TOK_CMPSTR,
    SH4ASM_TOK_DIV1,
    SH4ASM_TOK_DIV0S,
    SH4ASM_TOK_DMULSL,
    SH4ASM_TOK_DMULUL,
    SH4ASM_TOK_EXTSB,
    SH4ASM_TOK_EXTSW,
    SH4ASM_TOK_EXTUB,
    SH4ASM_TOK_EXTUW,
    SH4ASM_TOK_MULL,
    SH4ASM_TOK_MULSW,
    SH4ASM_TOK_MULUW,
    SH4ASM_TOK_NEG,
    SH4ASM_TOK_NEGC,
    SH4ASM_TOK_SUB,
    SH4ASM_TOK_SUBC,
    SH4ASM_TOK_SUBV,
    SH4ASM_TOK_NOT,
    SH4ASM_TOK_SHAD,
    SH4ASM_TOK_SHLD,
    SH4ASM_TOK_LDS,
    SH4ASM_TOK_STS,
    SH4ASM_TOK_LDSL,
    SH4ASM_TOK_STSL,
    SH4ASM_TOK_MOVB,
    SH4ASM_TOK_MOVA,
    SH4ASM_TOK_MOVCAL,
    SH4ASM_TOK_MAC_DOT_L,
    SH4ASM_TOK_MAC_DOT_W,
    SH4ASM_TOK_FLDI0,
    SH4ASM_TOK_FLDI1,
    SH4ASM_TOK_FMOV,
    SH4ASM_TOK_FMOVS,
    SH4ASM_TOK_FLDS,
    SH4ASM_TOK_FSTS,
    SH4ASM_TOK_FABS,
    SH4ASM_TOK_FADD,
    SH4ASM_TOK_FCMPEQ,
    SH4ASM_TOK_FCMPGT,
    SH4ASM_TOK_FDIV,
    SH4ASM_TOK_FLOAT,
    SH4ASM_TOK_FMAC,
    SH4ASM_TOK_FMUL,
    SH4ASM_TOK_FNEG,
    SH4ASM_TOK_FSQRT,
    SH4ASM_TOK_FSUB,
    SH4ASM_TOK_FTRC,
    SH4ASM_TOK_FCNVDS,
    SH4ASM_TOK_FCNVSD,
    SH4ASM_TOK_FIPR,
    SH4ASM_TOK_FTRV,
    SH4ASM_TOK_FSCA,
    SH4ASM_TOK_FSRRA,

    // registers
    SH4ASM_TOK_SR,
    SH4ASM_TOK_GBR,
    SH4ASM_TOK_VBR,
    SH4ASM_TOK_SSR,
    SH4ASM_TOK_SPC,
    SH4ASM_TOK_SGR,
    SH4ASM_TOK_DBR,
    SH4ASM_TOK_PC,
    SH4ASM_TOK_MACH,
    SH4ASM_TOK_MACL,
    SH4ASM_TOK_PR,
    SH4ASM_TOK_FPUL,
    SH4ASM_TOK_FPSCR,

    SH4ASM_TOK_RN,
    SH4ASM_TOK_RN_BANK,
    SH4ASM_TOK_FRN,
    SH4ASM_TOK_DRN,
    SH4ASM_TOK_XDN,
    SH4ASM_TOK_FVN,

    SH4ASM_TOK_XMTRX,

    SH4ASM_TOK_IMM,
    SH4ASM_TOK_DISP
};

union sh4asm_tok_val {
    int as_int;
    int reg_idx;
};

struct sh4asm_tok {
    enum tok_tp tp;

    // this only has meaning for certain types of tokens
    union sh4asm_tok_val val;
};

typedef void(*sh4asm_tok_emit_func)(struct sh4asm_tok const*);

/*
 * input the given character to the lexer.  If the character is successfully
 * tokenized, then it is input to the emitter function.
 *
 * the struct tok pointer input to the emitter is not persistent, and should not
 * be accessed after the emitter function returns.
 */
void sh4asm_lexer_input_char(char ch, sh4asm_tok_emit_func emit);

/*
 * return a text-based representation of the given token.
 * the string returned by this function is not persistent,
 * and therefore should not be referenced again aftetr the next
 * time this function is called.
 */
char const *sh4asm_tok_as_str(struct sh4asm_tok const *tk);

// reset the lexer to its default state
void sh4asm_lexer_reset(void);

#endif
