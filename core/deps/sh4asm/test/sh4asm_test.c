/*******************************************************************************
 *
 * Copyright (c) 2016,2017,2019,2020 snickerbockers <chimerasaurusrex@gmail.com>
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

#include <err.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <getopt.h>

#include "sh4asm.h"
#include "disas.h"

#define SH4ASM_BUF_MAX 1
unsigned sh4asm_buf_len;
static uint16_t sh4asm_buf[SH4ASM_BUF_MAX];

#define SH4ASM_TXT_LEN 128
static char sh4asm_disas[SH4ASM_TXT_LEN];
unsigned sh4asm_disas_len;

static void neo_bin_emit(uint16_t dat) {
    if (sh4asm_buf_len >= SH4ASM_BUF_MAX)
        errx(1, "sh4asm buffer overflow");

    sh4asm_buf[sh4asm_buf_len++] = dat;
}

static void clear_bin(void) {
    sh4asm_buf_len = 0;
}

static void neo_asm_emit(char ch) {
    if (sh4asm_disas_len >= SH4ASM_TXT_LEN)
        errx(1, "sh4asm disassembler buffer overflow");
    sh4asm_disas[sh4asm_disas_len++] = ch;
}

static void clear_asm(void) {
    sh4asm_disas_len = 0;
}

/*
 * This function tests assembler and disassembler functionality on the given
 * string by first assembling it, then disassessembling it then reassembling it
 * and checking the results of the two assembly operations to see if they are
 * equal (they should be).  The instructions are only compared in binary form
 * because there is not a 1:1 mapping between text-based assembly and binary
 * instructions (whitespace, hex/decimal, double-precision floating point
 * instructions that share opcodes with single-precision floating point
 * instructions, etc.).
 *
 * Of course this doesn't technically test that the assembler is correct, but
 * if it's idempotent then it probably is correct.
 *
 * This function returns true on test-pass, and false on test-fail
 */
bool test_inst(char const *inst) {
    size_t len = strlen(inst);

    if (!len)
        errx(1, "empty instruction (this is a proble with the test)");
    if (inst[len - 1] != '\n') {
        errx(1, "instructions need to end with newlines (this is a problem "
             "with the test)");
    }

    printf("about to test %s", inst);

    // first assemble the instruction
    clear_bin();
    sh4asm_set_emitter(neo_bin_emit);
    while (*inst)
        sh4asm_input_char(tolower(*inst++));
    if (sh4asm_buf_len != 1) {
        printf("invalid sh4asm output length (expected 1, got %u)\n",
               sh4asm_buf_len);
        return false;
    }
    uint16_t inst_bin = sh4asm_buf[0];

    // now disassemble it
    clear_asm();
    sh4asm_disas_inst(inst_bin, neo_asm_emit, 0);

    // add in a newline
    neo_asm_emit('\n');
    neo_asm_emit('\0');

    char const *new_inst = sh4asm_disas;

    printf("The returned instruction was %s", new_inst);

    // now reassemble the instruction
    clear_bin();
    sh4asm_set_emitter(neo_bin_emit);
    while (*new_inst)
        sh4asm_input_char(*new_inst++);
    if (sh4asm_buf_len != 1) {
        printf("invalid sh4asm output length (expected 1, got %u)\n",
               sh4asm_buf_len);
        return false;
    }

    if (sh4asm_buf[0] != inst_bin) {
        printf("Error: first assembly returned 0x%04x, second assembly "
               "returned 0x%04x\n",
               (unsigned)inst_bin, (unsigned)sh4asm_buf[0]);
        return false;
    }

    printf("success!\n");

    return true;
}

/*
 * <N> means to generate a random N-bit integer.
 * Obviously N cannot be greater than 16
 */
char const *insts_to_test[] = {
    "DIV0U\n",
    "RTS\n",
    "CLRMAC\n",
    "CLRS\n",
    "CLRT\n",
    "LDTLB\n",
    "NOP\n",
    "RTE\n",
    "SETS\n",
    "SETT\n",
    "SLEEP\n",
    "FRCHG\n",
    "FSCHG\n",
    "MOVT R<4>\n",
    "CMP/PZ R<4>\n",
    "CMP/PL R<4>\n",
    "DT R<4>\n",
    "ROTL R<4>\n",
    "ROTR R<4>\n",
    "ROTCL R<4>\n",
    "ROTCR R<4>\n",
    "SHAL R<4>\n",
    "SHAR R<4>\n",
    "SHLL R<4>\n",
    "SHLR R<4>\n",
    "SHLL2 R<4>\n",
    "SHLR2 R<4>\n",
    "SHLL8 R<4>\n",
    "SHLR8 R<4>\n",
    "SHLL16 R<4>\n",
    "SHLR16 R<4>\n",
    "BRAF R<4>\n",
    "BSRF R<4>\n",
    "CMP/EQ #<8>, R0\n",
    "AND.B #<8>, @(R0, GBR)\n",
    "AND #<8>, R0\n",
    "OR.B #<8>, @(R0, GBR)\n",
    "OR #<8>, R0\n",
    "TST #<8>, R0\n",
    "TST.B #<8>, @(R0, GBR)\n",
    "XOR #<8>, R0\n",
    "XOR.B #<8>, @(R0, GBR)\n",
    "TRAPA #<8>\n",
    "TAS.B @R<4>\n",
    "OCBI @R<4>\n",
    "OCBP @R<4>\n",
    "OCBWB @R<4>\n",
    "PREF @R<4>\n",
    "JMP @R<4>\n",
    "JSR @R<4>\n",
    "LDC R<4>, SR\n",
    "LDC R<4>, GBR\n",
    "LDC R<4>, VBR\n",
    "LDC R<4>, SSR\n",
    "LDC R<4>, SPC\n",
    "LDC R<4>, DBR\n",
    "STC SR, R<4>\n",
    "STC GBR, R<4>\n",
    "STC VBR, R<4>\n",
    "STC SSR, R<4>\n",
    "STC SPC, R<4>\n",
    "STC SGR, R<4>\n",
    "STC DBR, R<4>\n",
    "LDC.L @R<4>+, SR\n",
    "LDC.L @R<4>+, GBR\n",
    "LDC.L @R<4>+, VBR\n",
    "LDC.L @R<4>+, SSR\n",
    "LDC.L @R<4>+, SPC\n",
    "LDC.L @R<4>+, DBR\n",
    "STC.L SR, @-R<4>\n",
    "STC.L GBR, @-R<4>\n",
    "STC.L VBR, @-R<4>\n",
    "STC.L SSR, @-R<4>\n",
    "STC.L SPC, @-R<4>\n",
    "STC.L SGR, @-R<4>\n",
    "STC.L DBR, @-R<4>\n",
    "MOV #<8>, R<4>\n",
    "ADD #<8>, R<4>\n",
    "MOV R<4>, R<4>\n",
    "SWAP.B R<4>, R<4>\n",
    "SWAP.W R<4>, R<4>\n",
    "XTRCT R<4>, R<4>\n",
    "ADD R<4>, R<4>\n",
    "ADDC R<4>, R<4>\n",
    "ADDV R<4>, R<4>\n",
    "CMP/EQ R<4>, R<4>\n",
    "CMP/HS R<4>, R<4>\n",
    "CMP/GE R<4>, R<4>\n",
    "CMP/HI R<4>, R<4>\n",
    "CMP/GT R<4>, R<4>\n",
    "CMP/STR R<4>, R<4>\n",
    "DIV1 R<4>, R<4>\n",
    "DIV0S R<4>, R<4>\n",
    "DMULS.L R<4>, R<4>\n",
    "DMULU.L R<4>, R<4>\n",
    "EXTS.B R<4>, R<4>\n",
    "EXTS.W R<4>, R<4>\n",
    "EXTU.B R<4>, R<4>\n",
    "EXTU.W R<4>, R<4>\n",
    "MUL.L R<4>, R<4>\n",
    "MULS.W R<4>, R<4>\n",
    "MULU.W R<4>, R<4>\n",
    "NEG R<4>, R<4>\n",
    "NEGC R<4>, R<4>\n",
    "SUB R<4>, R<4>\n",
    "SUBC R<4>, R<4>\n",
    "SUBV R<4>, R<4>\n",
    "AND R<4>, R<4>\n",
    "NOT R<4>, R<4>\n",
    "OR R<4>, R<4>\n",
    "TST R<4>, R<4>\n",
    "XOR R<4>, R<4>\n",
    "SHAD R<4>, R<4>\n",
    "SHLD R<4>, R<4>\n",
    "LDC R<4>, R<3>_BANK\n",
    "LDC.L @R<4>+, R<3>_BANK\n",
    "STC R<3>_BANK, R<4>\n",
    "STC.L R<3>_BANK, @-R<4>\n",
    "LDS R<4>, MACH\n",
    "LDS R<4>, MACL\n",
    "STS MACH, R<4>\n",
    "STS MACL, R<4>\n",
    "LDS R<4>, PR\n",
    "STS PR, R<4>\n",
    "LDS.L @R<4>+, MACH\n",
    "LDS.L @R<4>+, MACL\n",
    "STS.L MACH, @-R<4>\n",
    "STS.L MACL, @-R<4>\n",
    "LDS.L @R<4>+, PR\n",
    "STS.L PR, @-R<4>\n",
    "MOV.B R<4>, @R<4>\n",
    "MOV.W R<4>, @R<4>\n",
    "MOV.L R<4>, @R<4>\n",
    "MOV.B @R<4>, R<4>\n",
    "MOV.W @R<4>, R<4>\n",
    "MOV.L @R<4>, R<4>\n",
    "MOV.B R<4>, @-R<4>\n",
    "MOV.W R<4>, @-R<4>\n",
    "MOV.L R<4>, @-R<4>\n",
    "MOV.B @R<4>+, R<4>\n",
    "MOV.W @R<4>+, R<4>\n",
    "MOV.L @R<4>+, R<4>\n",
    "MAC.L @R<4>+, @R<4>+\n",
    "MAC.W @R<4>+, @R<4>+\n",
    "MOV.B R0, @(<4,1>, R<4>)\n",
    "MOV.W R0, @(<4,2>, R<4>)\n",
    "MOV.L R<4>, @(<4,4>, R<4>)\n",
    "MOV.B @(<4,1>, R<4>), R0\n",
    "MOV.W @(<4,2>, R<4>), R0\n",
    "MOV.L @(<4,4>, R<4>), R<4>\n",
    "MOV.B R<4>, @(R0, R<4>)\n",
    "MOV.W R<4>, @(R0, R<4>)\n",
    "MOV.L R<4>, @(R0, R<4>)\n",
    "MOV.B @(R0, R<4>), R<4>\n",
    "MOV.W @(R0, R<4>), R<4>\n",
    "MOV.L @(R0, R<4>), R<4>\n",
    "MOV.B R0, @(<8,1>, GBR)\n",
    "MOV.W R0, @(<8,2>, GBR)\n",
    "MOV.L R0, @(<8,4>, GBR)\n",
    "MOV.B @(<8,1>, GBR), R0\n",
    "MOV.W @(<8,2>, GBR), R0\n",
    "MOV.L @(<8,4>, GBR), R0\n",
    "MOVCA.L R0, @R<4>\n",
    "FLDI0 FR<4>\n",
    "FLDI1 FR<4>\n",
    "FMOV FR<4>, FR<4>\n",
    "FMOV.S @R<4>, FR<4>\n",
    "FMOV.S @(R0, R<4>), FR<4>\n",
    "FMOV.S @R<4>+, FR<4>\n",
    "FMOV.S FR<4>, @R<4>\n",
    "FMOV.S FR<4>, @-R<4>\n",
    "FMOV.S FR<4>, @(R0, R<4>)\n",
    "FMOV DR<3,2>, DR<3,2>\n",
    "FMOV @R<4>, DR<3,2>\n",
    "FMOV @(R0, R<4>), DR<3,2>\n",
    "FMOV @R<4>+, DR<3,2>\n",
    "FMOV DR<3,2>, @R<4>\n",
    "FMOV DR<3,2>, @-R<4>\n",
    "FMOV DR<3,2>, @(R0, R<4>)\n",
    "FLDS FR<4>, FPUL\n",
    "FSTS FPUL, FR<4>\n",
    "FABS FR<4>\n",
    "FADD FR<4>, FR<4>\n",
    "FCMP/EQ FR<4>, FR<4>\n",
    "FCMP/GT FR<4>, FR<4>\n",
    "FDIV FR<4>, FR<4>\n",
    "FLOAT FPUL, FR<4>\n",
    "FMAC FR0, FR<4>, FR<4>\n",
    "FMUL FR<4>, FR<4>\n",
    "FNEG FR<4>\n",
    "FSQRT FR<4>\n",
    "FSUB FR<4>, FR<4>\n",
    "FTRC FR<4>, FPUL\n",
    "FABS DR<3,2>\n",
    "FADD DR<3,2>, DR<3,2>\n",
    "FCMP/EQ DR<3,2>, DR<3,2>\n",
    "FCMP/GT DR<3,2>, DR<3,2>\n",
    "FDIV DR<3,2>, DR<3,2>\n",
    "FCNVDS DR<3,2>, FPUL\n",
    "FCNVSD FPUL, DR<3,2>\n",
    "FLOAT FPUL, DR<3,2>\n",
    "FMUL DR<3,2>, DR<3,2>\n",
    "FNEG DR<3,2>\n",
    "FSQRT DR<3,2>\n",
    "FSUB DR<3,2>, DR<3,2>\n",
    "FTRC DR<3,2>, FPUL\n",
    "LDS R<4>, FPSCR\n",
    "LDS R<4>, FPUL\n",
    "LDS.L @R<4>+, FPSCR\n",
    "LDS.L @R<4>+, FPUL\n",
    "STS FPSCR, R<4>\n",
    "STS FPUL, R<4>\n",
    "STS.L FPSCR, @-R<4>\n",
    "STS.L FPUL, @-R<4>\n",
    "FMOV DR<3,2>, XD<3,2>\n",
    "FMOV XD<3,2>, DR<3,2>\n",
    "FMOV XD<3,2>, XD<3,2>\n",
    "FMOV @R<4>, XD<3,2>\n",
    "FMOV @R<4>+, XD<3,2>\n",
    "FMOV @(R0, R<4>), XD<3,2>\n",
    "FMOV XD<3,2>, @R<4>\n",
    "FMOV XD<3,2>, @-R<4>\n",
    "FMOV XD<3,2>, @(R0, R<4>)\n",
    "FIPR FV<2,4>, FV<2,4>\n",
    "FTRV XMTRX, FV<2,4>\n",
    "FRCHG\n",
    "FSCHG\n",
    "FSCA FPUL, DR<3,2>\n",
    "FSRRA FR<4>\n",
    "MOV.W @(<8,4>, PC), R<4>\n",
    "MOV.L @(<8,4>, PC), R<4>\n",
    "MOVA @(<8,4>, PC), R0\n",
    "BF <8>\n",
    "BF/S <8>\n",
    "BT <8>\n",
    "BT/S <8>\n",
    "BRA <12>\n",
    "BSR <12>\n",

    NULL
};

// lookup table for n-bit integer masks
#define MASK_MAX 16
static unsigned masks[1 + MASK_MAX] = {
    0,
    0x1,
    0x3,
    0x7,
    0xf,
    0x1f,
    0x3f,
    0x7f,
    0xff,
    0x1ff,
    0x3ff,
    0x7ff,
    0xfff,
    0x1fff,
    0x3fff,
    0x7fff,
    0xffff
};

enum parse_state {
    PARSE_STATE_NORM,
    PARSE_STATE_N_BITS,
    PARSE_STATE_SCALE
};

static void process_inst_str(char *inst_out, unsigned max_len,
                             char const *inst_in) {
    char const *pos;
    enum parse_state state = PARSE_STATE_NORM;
    unsigned out_idx = 0;
    unsigned n_bits = 0, scale;
#define BUF_LEN 32
    char n_bits_str[BUF_LEN];
    char scale_str[BUF_LEN];
    unsigned n_bits_len;
    unsigned scale_len;

    /*
     * instruction template format:
     *     <N> - random N-bit integer
     *     <N,M> - random N-bit interger multiplied by M
     */
    for (pos = inst_in; *pos; pos++) {
        switch (state) {
        case PARSE_STATE_NORM:
            if (*pos == '<') {
                state = PARSE_STATE_N_BITS;
                memset(n_bits_str, 0, sizeof(n_bits_str));
                n_bits_len = 0;
            } else if (*pos == '>') {
                errx(1, "unexpected '>' character in instruction template "
                     "\"%s\"", inst_in);
            } else {
                if (out_idx >= max_len)
                    errx(1, "buffer overflow in %s", __func__);
                inst_out[out_idx++] = *pos;
            }
            break;
        case PARSE_STATE_N_BITS:
            if (*pos == ',' || *pos == '>') {
                n_bits_str[BUF_LEN-1] = '\0';
                n_bits = atoi(n_bits_str);
            }

            if (*pos == ',') {
                state = PARSE_STATE_SCALE;
                memset(scale_str, 0, sizeof(scale_str));
                scale_len = 0;
            } else if (*pos == '>') {
                state = PARSE_STATE_NORM;
                if (n_bits >= 16)
                    errx(1, "can't  generate a %u-bit mask", n_bits);
                unsigned val = rand() & masks[n_bits];
                int n_chars = snprintf(inst_out + out_idx,
                                       max_len - out_idx, "%u", val);
                if (n_chars >= (max_len - out_idx))
                    errx(1, "buffer overflow in %s", __func__);
                out_idx += n_chars;
            } else if (isdigit(*pos)) {
                if (n_bits_len >= BUF_LEN)
                    errx(1, "buffer overflow while parsing bit-length string");
                n_bits_str[n_bits_len++] = *pos;
            } else {
                errx(1, "unexpected character '%c'", *pos);
            }
            break;
        case PARSE_STATE_SCALE:
            if (*pos == '>') {
                state = PARSE_STATE_NORM;
                scale_str[BUF_LEN-1] = '\0';
                scale = atoi(scale_str);
                unsigned val = (rand() & masks[n_bits]) * scale;
                int n_chars = snprintf(inst_out + out_idx,
                                       max_len - out_idx, "%u", val);
                if (n_chars >= (max_len - out_idx))
                    errx(1, "buffer overflow in %s", __func__);
                out_idx += n_chars;
            } else if (isdigit(*pos)) {
                if (scale_len >= BUF_LEN)
                    errx(1, "buffer overflow while parsing scale string");
                scale_str[scale_len++] = *pos;
            } else {
                errx(1, "unexpected character '%c'", *pos);
            }
            break;
        default:
            errx(1, "invalid state %d", (int)state);
        }
    }

    if (state != PARSE_STATE_NORM)
        errx(1, "abrupt termination");
    if (out_idx >= max_len)
        errx(1, "buffer overflow");
    if (out_idx >= max_len)
        errx(1, "buffer overflow in %s", __func__);
    inst_out[out_idx] = '\0';
}

__attribute__((__noreturn__)) static void
error_handler(char const *fmt, va_list args) {
    vfprintf(stderr, fmt, args);
    abort();
}

// this returns 0 on success, nonzero on failure
int test_all_insts(unsigned seed) {
    unsigned n_tests = 0;
    unsigned n_success = 0;
    char const **inst = insts_to_test;
    char processed[64];

    sh4asm_set_error_handler(error_handler);

    srand(seed);
    while (*inst) {
        memset(processed, 0, sizeof(processed));
        process_inst_str(processed, sizeof(processed), *inst);
        if (test_inst(processed))
            n_success++;

        n_tests++;
        inst++;
    }

    double percent = 100.0 * (double)n_success / (double)n_tests;
    printf("%u tests run - %u successes (%f percent)\n",
           n_tests, n_success, percent);

    return n_success == n_tests ? 0 : 1;
}

int main(int argc, char **argv) {
    unsigned int seed = time(NULL);
    int opt;

    while ((opt = getopt(argc, argv, "s:")) > 0) {
        if (opt == 's')
            seed = atoi(optarg);
    }

    return test_all_insts(seed);
}
