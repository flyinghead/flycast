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

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdio.h>

#include "sh4asm.h"
#include "lexer.h"

static struct sh4asm_tok_mapping {
    char const *txt;
    enum tok_tp tok;
} const tok_map[] = {
    { ",", SH4ASM_TOK_COMMA },
    { "(", SH4ASM_TOK_OPENPAREN },
    { ")", SH4ASM_TOK_CLOSEPAREN },
    { "@", SH4ASM_TOK_AT },
    { "\\n", SH4ASM_TOK_NEWLINE },
    { "+", SH4ASM_TOK_PLUS },
    { "div0u", SH4ASM_TOK_DIV0U },
    { "rts", SH4ASM_TOK_RTS },
    { "clrmac", SH4ASM_TOK_CLRMAC },
    { "clrs", SH4ASM_TOK_CLRS },
    { "clrt", SH4ASM_TOK_CLRT },
    { "ldtlb", SH4ASM_TOK_LDTLB },
    { "nop", SH4ASM_TOK_NOP },
    { "rte", SH4ASM_TOK_RTE },
    { "sets", SH4ASM_TOK_SETS },
    { "sett", SH4ASM_TOK_SETT },
    { "sleep", SH4ASM_TOK_SLEEP },
    { "frchg", SH4ASM_TOK_FRCHG },
    { "fschg", SH4ASM_TOK_FSCHG },
    { "movt", SH4ASM_TOK_MOVT },
    { "cmp/pz", SH4ASM_TOK_CMPPZ },
    { "cmp/pl", SH4ASM_TOK_CMPPL },
    { "dt", SH4ASM_TOK_DT },
    { "rotl", SH4ASM_TOK_ROTL },
    { "rotr", SH4ASM_TOK_ROTR },
    { "rotcl", SH4ASM_TOK_ROTCL },
    { "rotcr", SH4ASM_TOK_ROTCR },
    { "shal", SH4ASM_TOK_SHAL },
    { "shar", SH4ASM_TOK_SHAR },
    { "shll", SH4ASM_TOK_SHLL },
    { "shlr", SH4ASM_TOK_SHLR },
    { "shll2", SH4ASM_TOK_SHLL2 },
    { "shlr2", SH4ASM_TOK_SHLR2 },
    { "shll8", SH4ASM_TOK_SHLL8 },
    { "shlr8", SH4ASM_TOK_SHLR8 },
    { "shll16", SH4ASM_TOK_SHLL16 },
    { "shlr16", SH4ASM_TOK_SHLR16 },
    { "braf", SH4ASM_TOK_BRAF },
    { "bsrf", SH4ASM_TOK_BSRF },
    { "cmp/eq", SH4ASM_TOK_CMPEQ },
    { "and.b", SH4ASM_TOK_ANDB },
    { "and", SH4ASM_TOK_AND },
    { "or.b", SH4ASM_TOK_ORB },
    { "or", SH4ASM_TOK_OR },
    { "tst", SH4ASM_TOK_TST },
    { "tst.b", SH4ASM_TOK_TSTB },
    { "xor", SH4ASM_TOK_XOR },
    { "xor.b", SH4ASM_TOK_XORB },
    { "bf", SH4ASM_TOK_BF },
    { "bf/s", SH4ASM_TOK_BFS },
    { "bt", SH4ASM_TOK_BT },
    { "bt/s", SH4ASM_TOK_BTS },
    { "bra", SH4ASM_TOK_BRA },
    { "bsr", SH4ASM_TOK_BSR },
    { "trapa", SH4ASM_TOK_TRAPA },
    { "tas.b", SH4ASM_TOK_TASB },
    { "ocbi", SH4ASM_TOK_OCBI },
    { "ocbp", SH4ASM_TOK_OCBP },
    { "ocbwb", SH4ASM_TOK_OCBWB },
    { "pref", SH4ASM_TOK_PREF },
    { "jmp", SH4ASM_TOK_JMP },
    { "jsr", SH4ASM_TOK_JSR },
    { "ldc", SH4ASM_TOK_LDC },
    { "stc", SH4ASM_TOK_STC },
    { "ldc.l", SH4ASM_TOK_LDCL },
    { "stc.l", SH4ASM_TOK_STCL },
    { "mov", SH4ASM_TOK_MOV },
    { "add", SH4ASM_TOK_ADD },
    { "mov.w", SH4ASM_TOK_MOVW },
    { "mov.l", SH4ASM_TOK_MOVL },
    { "swap.b", SH4ASM_TOK_SWAPB },
    { "swap.w", SH4ASM_TOK_SWAPW },
    { "xtrct", SH4ASM_TOK_XTRCT },
    { "addc", SH4ASM_TOK_ADDC },
    { "addv", SH4ASM_TOK_ADDV },
    { "cmp/hs", SH4ASM_TOK_CMPHS },
    { "cmp/ge", SH4ASM_TOK_CMPGE },
    { "cmp/hi", SH4ASM_TOK_CMPHI },
    { "cmp/gt", SH4ASM_TOK_CMPGT },
    { "cmp/str", SH4ASM_TOK_CMPSTR },
    { "div1", SH4ASM_TOK_DIV1 },
    { "div0s", SH4ASM_TOK_DIV0S },
    { "dmuls.l", SH4ASM_TOK_DMULSL },
    { "dmulu.l", SH4ASM_TOK_DMULUL },
    { "exts.b", SH4ASM_TOK_EXTSB },
    { "exts.w", SH4ASM_TOK_EXTSW },
    { "extu.b", SH4ASM_TOK_EXTUB },
    { "extu.w", SH4ASM_TOK_EXTUW },
    { "mul.l", SH4ASM_TOK_MULL },
    { "muls.w", SH4ASM_TOK_MULSW },
    { "mulu.w", SH4ASM_TOK_MULUW },
    { "neg", SH4ASM_TOK_NEG },
    { "negc", SH4ASM_TOK_NEGC },
    { "sub", SH4ASM_TOK_SUB },
    { "subc", SH4ASM_TOK_SUBC },
    { "subv", SH4ASM_TOK_SUBV },
    { "not", SH4ASM_TOK_NOT },
    { "shad", SH4ASM_TOK_SHAD },
    { "shld", SH4ASM_TOK_SHLD },
    { "lds", SH4ASM_TOK_LDS },
    { "sts", SH4ASM_TOK_STS },
    { "lds.l", SH4ASM_TOK_LDSL },
    { "sts.l", SH4ASM_TOK_STSL },
    { "mov.b", SH4ASM_TOK_MOVB },
    { "mova", SH4ASM_TOK_MOVA },
    { "movca.l", SH4ASM_TOK_MOVCAL },
    { "mac.w", SH4ASM_TOK_MAC_DOT_W },
    { "mac.l", SH4ASM_TOK_MAC_DOT_L },
    { "fldi0", SH4ASM_TOK_FLDI0 },
    { "fldi1", SH4ASM_TOK_FLDI1 },
    { "fmov", SH4ASM_TOK_FMOV },
    { "fmov.s", SH4ASM_TOK_FMOVS },
    { "flds", SH4ASM_TOK_FLDS },
    { "fsts", SH4ASM_TOK_FSTS },
    { "fabs", SH4ASM_TOK_FABS },
    { "fadd", SH4ASM_TOK_FADD },
    { "fcmp/eq", SH4ASM_TOK_FCMPEQ },
    { "fcmp/gt", SH4ASM_TOK_FCMPGT },
    { "fdiv", SH4ASM_TOK_FDIV },
    { "float", SH4ASM_TOK_FLOAT },
    { "fmac", SH4ASM_TOK_FMAC },
    { "fmul", SH4ASM_TOK_FMUL },
    { "fneg", SH4ASM_TOK_FNEG },
    { "fsqrt", SH4ASM_TOK_FSQRT },
    { "fsub", SH4ASM_TOK_FSUB },
    { "ftrc", SH4ASM_TOK_FTRC },
    { "fcnvds", SH4ASM_TOK_FCNVDS },
    { "fcnvsd", SH4ASM_TOK_FCNVSD },
    { "fipr", SH4ASM_TOK_FIPR },
    { "ftrv", SH4ASM_TOK_FTRV },
    { "fsca", SH4ASM_TOK_FSCA },
    { "fsrra", SH4ASM_TOK_FSRRA },


    { "sr", SH4ASM_TOK_SR },
    { "gbr", SH4ASM_TOK_GBR },
    { "vbr", SH4ASM_TOK_VBR },
    { "ssr", SH4ASM_TOK_SSR },
    { "spc", SH4ASM_TOK_SPC },
    { "sgr", SH4ASM_TOK_SGR },
    { "dbr", SH4ASM_TOK_DBR },
    { "pc", SH4ASM_TOK_PC },
    { "mach", SH4ASM_TOK_MACH },
    { "macl", SH4ASM_TOK_MACL },
    { "pr", SH4ASM_TOK_PR },
    { "fpul", SH4ASM_TOK_FPUL },
    { "fpscr", SH4ASM_TOK_FPSCR },

    { "xmtrx", SH4ASM_TOK_XMTRX },

    { NULL }
};
#define SH4ASM_TOK_LEN_MAX 32
static char cur_tok[SH4ASM_TOK_LEN_MAX];
static unsigned tok_len;
static bool in_comment;

static struct sh4asm_tok_mapping const* check_tok(void) {
    struct sh4asm_tok_mapping const *curs = tok_map;

    while (curs->txt) {
        if (strcmp(cur_tok, curs->txt) == 0) {
            return curs;
        }
        curs++;
    }

    return NULL; // no token found
}

void sh4asm_lexer_input_char(char ch, sh4asm_tok_emit_func emit) {
    if (tok_len >= (SH4ASM_TOK_LEN_MAX - 1))
        sh4asm_error("Token is too long");

    if (in_comment) {
        if (ch == '\n')
            in_comment = false;
        else
            ch = ' ';
    } else if (ch == '!') {
        in_comment = true;
        ch = ' ';
    }

    if (isspace(ch) || ch == ',' || ch == '@' || ch == '(' || ch == ')' ||
        ch == '\0' || ch == '\n' || ch == '+' || ch == '-') {
        if (tok_len) {
            cur_tok[tok_len] = '\0';

            struct sh4asm_tok_mapping const *mapping = check_tok();
            if (mapping) {
                // 'normal' single-word token
                struct sh4asm_tok tk = {
                    .tp = mapping->tok
                };
                emit(&tk);
            } else if (cur_tok[0] == '#' && tok_len > 1) {
                // string literal
                errno = 0;
                long val_as_long = strtol(cur_tok + 1, NULL, 0);
                if (errno)
                    sh4asm_error("failed to decode integer literal");
                struct sh4asm_tok tk = {
                    .tp = SH4ASM_TOK_IMM,
                    .val = { .as_int = val_as_long }
                };
                emit(&tk);
            } else if (cur_tok[0] == 'r' && (tok_len == 2 || tok_len == 3)) {
                // general-purpose register
                int reg_no = atoi(cur_tok + 1);
                if (reg_no < 0 || reg_no > 15)
                    sh4asm_error("invalid register index %d", reg_no);
                struct sh4asm_tok tk = {
                    .tp = SH4ASM_TOK_RN,
                    .val = { .reg_idx = reg_no }
                };
                emit(&tk);
            } else if (cur_tok[0] == 'r' && (tok_len == 7 || tok_len == 8) &&
                       cur_tok[tok_len - 1] == 'k' && cur_tok[tok_len - 2] == 'n' &&
                       cur_tok[tok_len - 3] == 'a' && cur_tok[tok_len - 4] == 'b' &&
                       cur_tok[tok_len - 5] == '_') {
                // banked general-purpose register
                int reg_no = atoi(cur_tok + 1);
                if (reg_no < 0 || reg_no > 15)
                    sh4asm_error("invalid banked register index %d", reg_no);
                struct sh4asm_tok tk = {
                    .tp = SH4ASM_TOK_RN_BANK,
                    .val = { .reg_idx = reg_no }
                };
                emit(&tk);
            } else if ((tok_len == 3 || tok_len == 4) &&
                       cur_tok[0] == 'f' && cur_tok[1] == 'r') {
                // floating-point register
                int reg_no = atoi(cur_tok + 2);
                if (reg_no < 0 || reg_no > 15)
                    sh4asm_error("invalid floating-point register index %d",
                         reg_no);
                struct sh4asm_tok tk = {
                    .tp = SH4ASM_TOK_FRN,
                    .val = { .reg_idx = reg_no }
                };
                emit(&tk);
            } else if ((tok_len == 3 || tok_len == 4) &&
                       cur_tok[0] == 'd' && cur_tok[1] == 'r') {
                // double-precision floating-point register
                int reg_no = atoi(cur_tok + 2);
                if (reg_no < 0 || reg_no > 15 || (reg_no & 1))
                    sh4asm_error("invalid double-precision floating-point "
                         "register index %d", reg_no);
                struct sh4asm_tok tk = {
                    .tp = SH4ASM_TOK_DRN,
                    .val = { .reg_idx = reg_no }
                };
                emit(&tk);
            } else if ((tok_len == 3 || tok_len == 4) &&
                       cur_tok[0] == 'x' && cur_tok[1] == 'd') {
                // double-precision floating-point register (banked-out)
                int reg_no = atoi(cur_tok + 2);
                if (reg_no < 0 || reg_no > 15 || (reg_no & 1))
                    sh4asm_error("invalid banked double-precision floating-point "
                         "register index %d", reg_no);
                struct sh4asm_tok tk = {
                    .tp = SH4ASM_TOK_XDN,
                    .val = { .reg_idx = reg_no }
                };
                emit(&tk);
            } else if ((tok_len == 3 || tok_len == 4) &&
                       cur_tok[0] == 'f' && cur_tok[1] == 'v') {
                // floating-point vector register
                int reg_no = atoi(cur_tok + 2);
                if (reg_no < 0 || reg_no > 15 || (reg_no & 3))
                    sh4asm_error("invalid floating-point vector register index "
                         "%d\n", reg_no);
                struct sh4asm_tok tk = {
                    .tp = SH4ASM_TOK_FVN,
                    .val = { .reg_idx = reg_no }
                };
                emit(&tk);
            } else {
                /*
                 * Maybe it's an offset (which is an integer literal without a
                 * preceding '#' character).  Try to decode it as one, and
                 * error out if that assumption does not fit.
                 */
                errno = 0;
                long val_as_long = strtol(cur_tok, NULL, 0);
                if (errno)
                    sh4asm_error("unrecognized token \"%s\"", cur_tok);
                struct sh4asm_tok tk = {
                    .tp = SH4ASM_TOK_DISP,
                    .val = { .as_int = val_as_long }
                };
                emit(&tk);
            }

            tok_len = 0;
        }

        // don't forget the comma if that was the delimter that brought us here
        if (ch == ',') {
            struct sh4asm_tok tk = {
                .tp = SH4ASM_TOK_COMMA
            };
            emit(&tk);
        } else if (ch == '(') {
            struct sh4asm_tok tk = {
                .tp = SH4ASM_TOK_OPENPAREN
            };
            emit(&tk);
        } else if (ch == ')') {
            struct sh4asm_tok tk = {
                .tp = SH4ASM_TOK_CLOSEPAREN
            };
            emit(&tk);
        } else if (ch == '@') {
            struct sh4asm_tok tk = {
                .tp = SH4ASM_TOK_AT
            };
            emit(&tk);
        } else if (ch == '\n') {
            struct sh4asm_tok tk = {
                .tp = SH4ASM_TOK_NEWLINE
            };
            emit(&tk);
        } else if (ch == '+') {
            struct sh4asm_tok tk = {
                .tp = SH4ASM_TOK_PLUS
            };
            emit(&tk);
        } else if (ch == '-') {
            struct sh4asm_tok tk = {
                .tp = SH4ASM_TOK_MINUS
            };
            emit(&tk);
        }
    } else {
        cur_tok[tok_len++] = ch;
    }
}

char const *sh4asm_tok_as_str(struct sh4asm_tok const *tk) {
    static char buf[SH4ASM_TOK_LEN_MAX];

    if (tk->tp == SH4ASM_TOK_IMM) {
        snprintf(buf, SH4ASM_TOK_LEN_MAX, "#0x%x", tk->val.as_int);
        buf[SH4ASM_TOK_LEN_MAX - 1] = '\0';
        return buf;
    } else if (tk->tp == SH4ASM_TOK_RN) {
        snprintf(buf, SH4ASM_TOK_LEN_MAX, "r%u", tk->val.reg_idx);
        buf[SH4ASM_TOK_LEN_MAX - 1] = '\0';
        return buf;
    } else if (tk->tp == SH4ASM_TOK_RN_BANK) {
        snprintf(buf, SH4ASM_TOK_LEN_MAX, "r%u_bank", tk->val.reg_idx);
        buf[SH4ASM_TOK_LEN_MAX - 1] = '\0';
        return buf;
    } else if (tk->tp == SH4ASM_TOK_FRN) {
        snprintf(buf, SH4ASM_TOK_LEN_MAX, "fr%u", tk->val.reg_idx);
        buf[SH4ASM_TOK_LEN_MAX - 1] = '\0';
        return buf;
    } else if (tk->tp == SH4ASM_TOK_DRN) {
        snprintf(buf, SH4ASM_TOK_LEN_MAX, "dr%u", tk->val.reg_idx);
        buf[SH4ASM_TOK_LEN_MAX - 1] = '\0';
        return buf;
    } else if (tk->tp == SH4ASM_TOK_XDN) {
        snprintf(buf, SH4ASM_TOK_LEN_MAX, "xd%u", tk->val.reg_idx);
        buf[SH4ASM_TOK_LEN_MAX - 1] = '\0';
        return buf;
    } else if (tk->tp == SH4ASM_TOK_FVN) {
        snprintf(buf, SH4ASM_TOK_LEN_MAX, "fv%u", tk->val.reg_idx);
        buf[SH4ASM_TOK_LEN_MAX - 1] = '\0';
        return buf;
    } else if (tk->tp == SH4ASM_TOK_DISP) {
        snprintf(buf, SH4ASM_TOK_LEN_MAX, "0x%x", tk->val.as_int);
        buf[SH4ASM_TOK_LEN_MAX - 1] = '\0';
        return buf;
    }

    struct sh4asm_tok_mapping const *curs = tok_map;
    while (curs->txt) {
        if (curs->tok == tk->tp)
            return curs->txt;
        curs++;
    }

    return NULL;
}

void sh4asm_lexer_reset(void) {
    tok_len = 0;
}
