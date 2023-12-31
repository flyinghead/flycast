/*******************************************************************************
 *
 * Copyright (c) 2017, 2019, 2020 snickerbockers <chimerasaurusrex@gmail.com>
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

#ifndef SH4ASM_DISAS_H_
#define SH4ASM_DISAS_H_

#include <stdint.h>
#include <string.h>

#include "sh4asm_txt_emit.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SH4ASM_STATIC static inline

/*******************************************************************************
 *
 * BEGIN PUBLIC API
 *
 ******************************************************************************/

// this must match the asm_emit_handler_func typedef in sh4_asm_emit.h
typedef void(*sh4asm_disas_emit_func)(char);

SH4ASM_STATIC void
sh4asm_disas_inst(uint16_t inst, sh4asm_disas_emit_func em, uint32_t pc);

/*******************************************************************************
 *
 * END PUBLIC API
 *
 ******************************************************************************/

// Set all bits up to but not including bit_no:
#define SH4ASM_SET_TO_BIT(bit_no)                   \
    ((uint32_t)((((uint64_t)1) << (bit_no)) - 1))

// set all bits between first and last (inclusive)
#define SH4ASM_BIT_RANGE(first, last)                           \
    (SH4ASM_SET_TO_BIT(last + 1) & ~SH4ASM_SET_TO_BIT(first))

SH4ASM_STATIC void
sh4asm_opcode_non_inst_(unsigned const *quads, sh4asm_disas_emit_func em) {
#define NON_INST_BUF_LEN 64
    char buf[NON_INST_BUF_LEN];
    memset(buf, 0, sizeof(buf));
    // TODO: make sure the quads get printed in the right order
    snprintf(buf, NON_INST_BUF_LEN, ".byte %x%x %x%x\n",
             quads[0], quads[1], quads[2], quads[3]);
    buf[NON_INST_BUF_LEN - 1] = '\0';
    char const *outp = buf;
    while (*outp)
        em(*outp++);
}

SH4ASM_STATIC void
sh4asm_disas_0xx2_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    switch (quads[1]) {
    case 0:
        sh4asm_txt_stc_sr_rn(em, quads[2]);
        break;
    case 1:
        sh4asm_txt_stc_gbr_rn(em, quads[2]);
        break;
    case 2:
        sh4asm_txt_stc_vbr_rn(em, quads[2]);
        break;
    case 3:
        sh4asm_txt_stc_ssr_rn(em, quads[2]);
        break;
    case 4:
        sh4asm_txt_stc_spc_rn(em, quads[2]);
        break;
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
        // mask is 0xf08f
        sh4asm_txt_stc_rm_bank_rn(em, quads[1] & 7, quads[2]);
        break;
    default:
        sh4asm_opcode_non_inst_(quads, em);
    }
}

SH4ASM_STATIC void
sh4asm_disas_0xx3_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    // mask is 0xf0ff
    switch (quads[1]) {
    case 0:
        sh4asm_txt_bsrf_rn(em, quads[2]);
        break;
    case 2:
        sh4asm_txt_braf_rn(em, quads[2]);
        break;
    case 8:
        sh4asm_txt_pref_arn(em, quads[2]);
        break;
    case 9:
        sh4asm_txt_ocbi_arn(em, quads[2]);
        break;
    case 10:
        sh4asm_txt_ocbp_arn(em, quads[2]);
        break;
    case 11:
        sh4asm_txt_ocbwb_arn(em, quads[2]);
        break;
    case 12:
        sh4asm_txt_movcal_r0_arn(em, quads[2]);
        break;
    default:
        sh4asm_opcode_non_inst_(quads, em);
    }
}

SH4ASM_STATIC void
sh4asm_disas_0xx4_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    // mask is 0xf00f
    sh4asm_txt_movb_rm_a_r0_rn(em, quads[1], quads[2]);
}

SH4ASM_STATIC void
sh4asm_disas_0xx5_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    // mask is 0xf00f
    sh4asm_txt_movw_rm_a_r0_rn(em, quads[1], quads[2]);
}

SH4ASM_STATIC void
sh4asm_disas_0xx6_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    // mask is 0xf00f
    sh4asm_txt_movl_rm_a_r0_rn(em, quads[1], quads[2]);
}

SH4ASM_STATIC void
sh4asm_disas_0xx7_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    // mask is 0xf00f
    sh4asm_txt_mull_rm_rn(em, quads[1], quads[2]);
}

SH4ASM_STATIC void
sh4asm_disas_0xx8_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    // mask is 0xffff
    if (quads[2] != 0) {
        sh4asm_opcode_non_inst_(quads, em);
        return;
    }

    switch (quads[1]) {
    case 0:
        sh4asm_txt_clrt(em);
        break;
    case 1:
        sh4asm_txt_sett(em);
        break;
    case 2:
        sh4asm_txt_clrmac(em);
        break;
    case 3:
        sh4asm_txt_ldtlb(em);
        break;
    case 4:
        sh4asm_txt_clrs(em);
        break;
    case 5:
        sh4asm_txt_sets(em);
        break;
    default:
        sh4asm_opcode_non_inst_(quads, em);
    }
}

SH4ASM_STATIC void
sh4asm_disas_0xx9_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    switch (quads[1]) {
    case 0:
        // mask is 0xffff
        if (quads[2] == 0)
            sh4asm_txt_nop(em);
        else
            goto non_inst;
        break;
    case 1:
        // mask is 0xffff
        if (quads[2] == 0)
            sh4asm_txt_div0u(em);
        else
            goto non_inst;
        break;
    case 2:
        // mask is 0xf0ff
        sh4asm_txt_movt_rn(em, quads[2]);
        break;
    default:
        goto non_inst;
    }
    return;
 non_inst:
    sh4asm_opcode_non_inst_(quads, em);
}

SH4ASM_STATIC void
sh4asm_disas_0xxa_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    // mask is 0xf0ff
    switch (quads[1]) {
    case 0:
        sh4asm_txt_sts_mach_rn(em, quads[2]);
        break;
    case 1:
        sh4asm_txt_sts_macl_rn(em, quads[2]);
        break;
    case 2:
        sh4asm_txt_sts_pr_rn(em, quads[2]);
        break;
    case 3:
        sh4asm_txt_stc_sgr_rn(em, quads[2]);
        break;
    case 5:
        sh4asm_txt_sts_fpul_rn(em, quads[2]);
        break;
    case 6:
        sh4asm_txt_sts_fpscr_rn(em, quads[2]);
        break;
    case 15:
        sh4asm_txt_stc_dbr_rn(em, quads[2]);
        break;
    default:
        sh4asm_opcode_non_inst_(quads, em);
    }
}

SH4ASM_STATIC void
sh4asm_disas_0xxb_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    // mask is 0xffff
    if (quads[2])
        goto non_inst;
    switch (quads[1]) {
    case 0:
        sh4asm_txt_rts(em);
        break;
    case 1:
        sh4asm_txt_sleep(em);
        break;
    case 2:
        sh4asm_txt_rte(em);
        break;
    default:
        goto non_inst;
    }
    return;
 non_inst:
    sh4asm_opcode_non_inst_(quads, em);
}

SH4ASM_STATIC void
sh4asm_disas_0xxc_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    // mask is 0xf00f
    sh4asm_txt_movb_a_r0_rm_rn(em, quads[1], quads[2]);
}

SH4ASM_STATIC void
sh4asm_disas_0xxd_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    // mask is 0xf00f
    sh4asm_txt_movw_a_r0_rm_rn(em, quads[1], quads[2]);
}

static void sh4asm_disas_0xxe_(unsigned const *quads,
                               sh4asm_disas_emit_func em, uint32_t pc) {
    // mask is 0xf00f
    sh4asm_txt_movl_a_r0_rm_rn(em, quads[1], quads[2]);
}

static void sh4asm_disas_0xxf_(unsigned const *quads,
                               sh4asm_disas_emit_func em, uint32_t pc) {
    // mask is 0xf00f
    sh4asm_txt_macl_armp_arnp(em, quads[1], quads[2]);
}

static void sh4asm_disas_0xxx_(unsigned const *quads,
                               sh4asm_disas_emit_func em, uint32_t pc) {
    switch (quads[0]) {
    case 2:
        sh4asm_disas_0xx2_(quads, em, pc);
        break;
    case 3:
        sh4asm_disas_0xx3_(quads, em, pc);
        break;
    case 4:
        sh4asm_disas_0xx4_(quads, em, pc);
        break;
    case 5:
        sh4asm_disas_0xx5_(quads, em, pc);
        break;
    case 6:
        sh4asm_disas_0xx6_(quads, em, pc);
        break;
    case 7:
        sh4asm_disas_0xx7_(quads, em, pc);
        break;
    case 8:
        sh4asm_disas_0xx8_(quads, em, pc);
        break;
    case 9:
        sh4asm_disas_0xx9_(quads, em, pc);
        break;
    case 10:
        sh4asm_disas_0xxa_(quads, em, pc);
        break;
    case 11:
        sh4asm_disas_0xxb_(quads, em, pc);
        break;
    case 12:
        sh4asm_disas_0xxc_(quads, em, pc);
        break;
    case 13:
        sh4asm_disas_0xxd_(quads, em, pc);
        break;
    case 14:
        sh4asm_disas_0xxe_(quads, em, pc);
        break;
    case 15:
        sh4asm_disas_0xxf_(quads, em, pc);
        break;
    default:
        sh4asm_opcode_non_inst_(quads, em);
    }
}

SH4ASM_STATIC void
sh4asm_disas_1xxx_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    // mask is 0xf000
    unsigned rn = quads[2];
    unsigned rm = quads[1];
    unsigned disp = quads[0];

    sh4asm_txt_movl_rm_a_disp4_rn(em, rm, disp << 2, rn);
}

SH4ASM_STATIC void
sh4asm_disas_2xxx_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    // mask is 0xf00f
    unsigned rm = quads[1];
    unsigned rn = quads[2];
    switch (quads[0]) {
    case 0:
        sh4asm_txt_movb_rm_arn(em, rm, rn);
        break;
    case 1:
        sh4asm_txt_movw_rm_arn(em, rm, rn);
        break;
    case 2:
        sh4asm_txt_movl_rm_arn(em, rm, rn);
        break;
    case 4:
        sh4asm_txt_movb_rm_amrn(em, rm, rn);
        break;
    case 5:
        sh4asm_txt_movw_rm_amrn(em, rm, rn);
        break;
    case 6:
        sh4asm_txt_movl_rm_amrn(em, rm, rn);
        break;
    case 7:
        sh4asm_txt_div0s_rm_rn(em, rm, rn);
        break;
    case 8:
        sh4asm_txt_tst_rm_rn(em, rm, rn);
        break;
    case 9:
        sh4asm_txt_and_rm_rn(em, rm, rn);
        break;
    case 10:
        sh4asm_txt_xor_rm_rn(em, rm, rn);
        break;
    case 11:
        sh4asm_txt_or_rm_rn(em, rm, rn);
        break;
    case 12:
        sh4asm_txt_cmpstr_rm_rn(em, rm, rn);
        break;
    case 13:
        sh4asm_txt_xtrct_rm_rn(em, rm, rn);
        break;
    case 14:
        sh4asm_txt_muluw_rm_rn(em, rm, rn);
        break;
    case 15:
        sh4asm_txt_mulsw_rm_rn(em, rm, rn);
        break;
    default:
        sh4asm_opcode_non_inst_(quads, em);
    }
}

SH4ASM_STATIC void
sh4asm_disas_3xxx_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    // mask is 0xf00f
    unsigned rm = quads[1];
    unsigned rn = quads[2];
    switch (quads[0]) {
    case 0:
        sh4asm_txt_cmpeq_rm_rn(em, rm, rn);
        break;
    case 2:
        sh4asm_txt_cmphs_rm_rn(em, rm, rn);
        break;
    case 3:
        sh4asm_txt_cmpge_rm_rn(em, rm, rn);
        break;
    case 4:
        sh4asm_txt_div1_rm_rn(em, rm, rn);
        break;
    case 5:
        sh4asm_txt_dmulul_rm_rn(em, rm, rn);
        break;
    case 6:
        sh4asm_txt_cmphi_rm_rn(em, rm, rn);
        break;
    case 7:
        sh4asm_txt_cmpgt_rm_rn(em, rm, rn);
        break;
    case 8:
        sh4asm_txt_sub_rm_rn(em, rm, rn);
        break;
    case 10:
        sh4asm_txt_subc_rm_rn(em, rm, rn);
        break;
    case 11:
        sh4asm_txt_subv_rm_rn(em, rm, rn);
        break;
    case 12:
        sh4asm_txt_add_rm_rn(em, rm, rn);
        break;
    case 13:
        sh4asm_txt_dmulsl_rm_rn(em, rm, rn);
        break;
    case 14:
        sh4asm_txt_addc_rm_rn(em, rm, rn);
        break;
    case 15:
        sh4asm_txt_addv_rm_rn(em, rm, rn);
        break;
    default:
        sh4asm_opcode_non_inst_(quads, em);
    }
}

SH4ASM_STATIC void
sh4asm_disas_4xx3_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    switch (quads[1]) {
    case 0:
        sh4asm_txt_stcl_sr_amrn(em, quads[2]);
        break;
    case 1:
        sh4asm_txt_stcl_gbr_amrn(em, quads[2]);
        break;
    case 2:
        sh4asm_txt_stcl_vbr_amrn(em, quads[2]);
        break;
    case 3:
        sh4asm_txt_stcl_ssr_amrn(em, quads[2]);
        break;
    case 4:
        sh4asm_txt_stcl_spc_amrn(em, quads[2]);
        break;
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
        sh4asm_txt_stcl_rm_bank_amrn(em, quads[1] & 7, quads[2]);
        break;
    default:
        sh4asm_opcode_non_inst_(quads, em);
    }
}

SH4ASM_STATIC void
sh4asm_disas_4xxe_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    switch (quads[1]) {
    case 0:
        sh4asm_txt_ldc_rm_sr(em, quads[2]);
        break;
    case 1:
        sh4asm_txt_ldc_rm_gbr(em, quads[2]);
        break;
    case 2:
        sh4asm_txt_ldc_rm_vbr(em, quads[2]);
        break;
    case 3:
        sh4asm_txt_ldc_rm_ssr(em, quads[2]);
        break;
    case 4:
        sh4asm_txt_ldc_rm_spc(em, quads[2]);
        break;
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
        sh4asm_txt_ldc_rm_rn_bank(em, quads[2], quads[1] & 7);
        break;
    default:
        sh4asm_opcode_non_inst_(quads, em);
    }
}

SH4ASM_STATIC void
sh4asm_disas_4xx7_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    switch (quads[1]) {
    case 0:
        sh4asm_txt_ldcl_armp_sr(em, quads[2]);
        break;
    case 1:
        sh4asm_txt_ldcl_armp_gbr(em, quads[2]);
        break;
    case 2:
        sh4asm_txt_ldcl_armp_vbr(em, quads[2]);
        break;
    case 3:
        sh4asm_txt_ldcl_armp_ssr(em, quads[2]);
        break;
    case 4:
        sh4asm_txt_ldcl_armp_spc(em, quads[2]);
        break;
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
        sh4asm_txt_ldcl_armp_rn_bank(em, quads[2], quads[1] & 7);
        break;
    default:
        sh4asm_opcode_non_inst_(quads, em);
    }
}

SH4ASM_STATIC void
sh4asm_disas_4xx0_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    switch (quads[1]) {
    case 0:
        sh4asm_txt_shll_rn(em, quads[2]);
        break;
    case 1:
        sh4asm_txt_dt_rn(em, quads[2]);
        break;
    case 2:
        sh4asm_txt_shal_rn(em, quads[2]);
        break;
    default:
        sh4asm_opcode_non_inst_(quads, em);
    }
}

SH4ASM_STATIC void
sh4asm_disas_4xx1_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    switch (quads[1]) {
    case 0:
        sh4asm_txt_shlr_rn(em, quads[2]);
        break;
    case 1:
        sh4asm_txt_cmppz_rn(em, quads[2]);
        break;
    case 2:
        sh4asm_txt_shar_rn(em, quads[2]);
        break;
    default:
        sh4asm_opcode_non_inst_(quads, em);
    }
}

SH4ASM_STATIC void
sh4asm_disas_4xx2_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    switch (quads[1]) {
    case 0:
        sh4asm_txt_stsl_mach_amrn(em, quads[2]);
        break;
    case 1:
        sh4asm_txt_stsl_macl_amrn(em, quads[2]);
        break;
    case 2:
        sh4asm_txt_stsl_pr_amrn(em, quads[2]);
        break;
    case 3:
        sh4asm_txt_stcl_sgr_amrn(em, quads[2]);
        break;
    case 5:
        sh4asm_txt_stsl_fpul_amrn(em, quads[2]);
        break;
    case 6:
        sh4asm_txt_stsl_fpscr_amrn(em, quads[2]);
        break;
    case 15:
        sh4asm_txt_stcl_dbr_amrn(em, quads[2]);
        break;
    default:
        sh4asm_opcode_non_inst_(quads, em);
    }
}

SH4ASM_STATIC void
sh4asm_disas_4xx4_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    switch (quads[1]) {
    case 0:
        sh4asm_txt_rotl_rn(em, quads[2]);
        break;
    case 2:
        sh4asm_txt_rotcl_rn(em, quads[2]);
        break;
    default:
        sh4asm_opcode_non_inst_(quads, em);
    }
}

SH4ASM_STATIC void
sh4asm_disas_4xx5_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    switch (quads[1]) {
    case 0:
        sh4asm_txt_rotr_rn(em, quads[2]);
        break;
    case 1:
        sh4asm_txt_cmppl_rn(em, quads[2]);
        break;
    case 2:
        sh4asm_txt_rotcr_rn(em, quads[2]);
        break;
    default:
        sh4asm_opcode_non_inst_(quads, em);
    }
}

SH4ASM_STATIC void
sh4asm_disas_4xx6_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    switch (quads[1]) {
    case 0:
        sh4asm_txt_ldsl_armp_mach(em, quads[2]);
        break;
    case 1:
        sh4asm_txt_ldsl_armp_macl(em, quads[2]);
        break;
    case 2:
        sh4asm_txt_ldsl_armp_pr(em, quads[2]);
        break;
    case 5:
        sh4asm_txt_ldsl_armp_fpul(em, quads[2]);
        break;
    case 6:
        sh4asm_txt_ldsl_armp_fpscr(em, quads[2]);
        break;
    case 15:
        sh4asm_txt_ldcl_armp_dbr(em, quads[2]);
        break;
    default:
        sh4asm_opcode_non_inst_(quads, em);
    }
}

SH4ASM_STATIC void
sh4asm_disas_4xx8_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    switch (quads[1]) {
    case 0:
        sh4asm_txt_shll2_rn(em, quads[2]);
        break;
    case 1:
        sh4asm_txt_shll8_rn(em, quads[2]);
        break;
    case 2:
        sh4asm_txt_shll16_rn(em, quads[2]);
        break;
    default:
        sh4asm_opcode_non_inst_(quads, em);
    }
}

SH4ASM_STATIC void
sh4asm_disas_4xx9_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    switch (quads[1]) {
    case 0:
        sh4asm_txt_shlr2_rn(em, quads[2]);
        break;
    case 1:
        sh4asm_txt_shlr8_rn(em, quads[2]);
        break;
    case 2:
        sh4asm_txt_shlr16_rn(em, quads[2]);
        break;
    default:
        sh4asm_opcode_non_inst_(quads, em);
    }
}

SH4ASM_STATIC void
sh4asm_disas_4xxa_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    switch (quads[1]) {
    case 0:
        sh4asm_txt_lds_rm_mach(em, quads[2]);
        break;
    case 1:
        sh4asm_txt_lds_rm_macl(em, quads[2]);
        break;
    case 2:
        sh4asm_txt_lds_rm_pr(em, quads[2]);
        break;
    case 5:
        sh4asm_txt_lds_rm_fpul(em, quads[2]);
        break;
    case 6:
        sh4asm_txt_lds_rm_fpscr(em, quads[2]);
        break;
    case 15:
        sh4asm_txt_ldc_rm_dbr(em, quads[2]);
        break;
    default:
        sh4asm_opcode_non_inst_(quads, em);
    }
}

SH4ASM_STATIC void
sh4asm_disas_4xxb_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    switch (quads[1]) {
    case 0:
        sh4asm_txt_jsr_arn(em, quads[2]);
        break;
    case 1:
        sh4asm_txt_tasb_arn(em, quads[2]);
        break;
    case 2:
        sh4asm_txt_jmp_arn(em, quads[2]);
        break;
    default:
        sh4asm_opcode_non_inst_(quads, em);
    }
}

SH4ASM_STATIC void
sh4asm_disas_4xxx_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    switch (quads[0]) {
    case 0:
        sh4asm_disas_4xx0_(quads, em, pc);
        break;
    case 1:
        sh4asm_disas_4xx1_(quads, em, pc);
        break;
    case 2:
        sh4asm_disas_4xx2_(quads, em, pc);
        break;
    case 3:
        sh4asm_disas_4xx3_(quads, em, pc);
        break;
    case 4:
        sh4asm_disas_4xx4_(quads, em, pc);
        break;
    case 5:
        sh4asm_disas_4xx5_(quads, em, pc);
        break;
    case 6:
        sh4asm_disas_4xx6_(quads, em, pc);
        break;
    case 7:
        sh4asm_disas_4xx7_(quads, em, pc);
        break;
    case 8:
        sh4asm_disas_4xx8_(quads, em, pc);
        break;
    case 9:
        sh4asm_disas_4xx9_(quads, em, pc);
        break;
    case 10:
        sh4asm_disas_4xxa_(quads, em, pc);
        break;
    case 11:
        sh4asm_disas_4xxb_(quads, em, pc);
        break;
    case 12:
        sh4asm_txt_shad_rm_rn(em, quads[1], quads[2]);
        break;
    case 13:
        sh4asm_txt_shld_rm_rn(em, quads[1], quads[2]);
        break;
    case 14:
        sh4asm_disas_4xxe_(quads, em, pc);
        break;
    case 15:
        sh4asm_txt_macw_armp_arnp(em, quads[1], quads[2]);
        break;
    default:
        sh4asm_opcode_non_inst_(quads, em);
    }
}

SH4ASM_STATIC void
sh4asm_disas_5xxx_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    // mask is 0xf000
    sh4asm_txt_movl_a_disp4_rm_rn(em, quads[0] << 2, quads[1], quads[2]);
}

SH4ASM_STATIC void
sh4asm_disas_6xxx_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    unsigned rm = quads[1];
    unsigned rn = quads[2];
    switch (quads[0]) {
    case 0:
        sh4asm_txt_movb_arm_rn(em, rm, rn);
        break;
    case 1:
        sh4asm_txt_movw_arm_rn(em, rm, rn);
        break;
    case 2:
        sh4asm_txt_movl_arm_rn(em, rm, rn);
        break;
    case 3:
        sh4asm_txt_mov_rm_rn(em, rm, rn);
        break;
    case 4:
        sh4asm_txt_movb_armp_rn(em, rm, rn);
        break;
    case 5:
        sh4asm_txt_movw_armp_rn(em, rm, rn);
        break;
    case 6:
        sh4asm_txt_movl_armp_rn(em, rm, rn);
        break;
    case 7:
        sh4asm_txt_not_rm_rn(em, rm, rn);
        break;
    case 8:
        sh4asm_txt_swapb_rm_rn(em, rm, rn);
        break;
    case 9:
        sh4asm_txt_swapw_rm_rn(em, rm, rn);
        break;
    case 10:
        sh4asm_txt_negc_rm_rn(em, rm, rn);
        break;
    case 11:
        sh4asm_txt_neg_rm_rn(em, rm, rn);
        break;
    case 12:
        sh4asm_txt_extub_rm_rn(em, rm, rn);
        break;
    case 13:
        sh4asm_txt_extuw_rm_rn(em, rm, rn);
        break;
    case 14:
        sh4asm_txt_extsb_rm_rn(em, rm, rn);
        break;
    case 15:
        sh4asm_txt_extsw_rm_rn(em, rm, rn);
        break;
    default:
        sh4asm_opcode_non_inst_(quads, em);
    }
}

SH4ASM_STATIC void
sh4asm_disas_7xxx_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    // mask is 0xf000
    sh4asm_txt_add_imm8_rn(em, (quads[1] << 4) | (quads[0]), quads[2]);
}

SH4ASM_STATIC void
sh4asm_disas_8xxx_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    // mask is 0xff00
    int disp8;
    switch (quads[2]) {
    case 0:
        sh4asm_txt_movb_r0_a_disp4_rn(em, quads[0], quads[1]);
        break;
    case 1:
        sh4asm_txt_movw_r0_a_disp4_rn(em, quads[0] << 1, quads[1]);
        break;
    case 4:
        sh4asm_txt_movb_a_disp4_rm_r0(em, quads[0], quads[1]);
        break;
    case 5:
        sh4asm_txt_movw_a_disp4_rm_r0(em, quads[0] << 1, quads[1]);
        break;
    case 8:
        sh4asm_txt_cmpeq_imm8_r0(em, (quads[1] << 4) | quads[0]);
        break;
    case 9:
        disp8 = (quads[1] << 4) | quads[0];
        if (disp8 & (1 << 7))
            disp8 |= ~SH4ASM_BIT_RANGE(0, 7);
        sh4asm_txt_bt_disp8_pc(em, 2 * disp8 + 4, pc);
        break;
    case 11:
        disp8 = (quads[1] << 4) | quads[0];
        if (disp8 & (1 << 7))
            disp8 |= ~SH4ASM_BIT_RANGE(0, 7);
        sh4asm_txt_bf_disp8_pc(em, 2 * disp8 + 4, pc);
        break;
    case 13:
        disp8 = (quads[1] << 4) | quads[0];
        if (disp8 & (1 << 7))
            disp8 |= ~SH4ASM_BIT_RANGE(0, 7);
        sh4asm_txt_bts_disp8_pc(em, 2 * disp8 + 4, pc);
        break;
    case 15:
        disp8 = (quads[1] << 4) | quads[0];
        if (disp8 & (1 << 7))
            disp8 |= ~SH4ASM_BIT_RANGE(0, 7);
        sh4asm_txt_bfs_disp8_pc(em, 2 * disp8 + 4, pc);
        break;
    default:
        sh4asm_opcode_non_inst_(quads, em);
    }
}

SH4ASM_STATIC void
sh4asm_disas_9xxx_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    // mask is 0xf000
    int disp8 = (quads[1] << 4) | quads[0];
    if (disp8 & (1 << 7))
        disp8 |= ~SH4ASM_BIT_RANGE(0, 7);
    unsigned reg_no = quads[2];
    sh4asm_txt_movw_a_disp8_pc_rn(em, 2 * disp8 + 4, pc, reg_no);
}

SH4ASM_STATIC void
sh4asm_disas_axxx_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    // mask is 0xf000
    int imm_val = (quads[2] << 8) | (quads[1] << 4) | quads[0];
    if (imm_val & (1 << 11))
        imm_val |= ~SH4ASM_BIT_RANGE(0, 11);
    int offs = 2 * imm_val + 4;
    sh4asm_txt_bra_offs12(em, offs, pc);
}

SH4ASM_STATIC void
sh4asm_disas_bxxx_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    // mask is 0xf000
    int imm_val = (quads[2] << 8) | (quads[1] << 4) | quads[0];
    if (imm_val & (1 << 11))
        imm_val |= ~SH4ASM_BIT_RANGE(0, 11);
    int offs = 2 * imm_val + 4;
    sh4asm_txt_bsr_offs12(em, offs, pc);
}

SH4ASM_STATIC void
sh4asm_disas_cxxx_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    // mask is 0xff00
    unsigned imm_val = (quads[1] << 4) | quads[0];
    int disp8 = (quads[1] << 4) | quads[0];
    if (disp8 & (1 << 7))
        disp8 |= ~SH4ASM_BIT_RANGE(0, 7);
    switch (quads[2]) {
    case 0:
        sh4asm_txt_movb_r0_a_disp8_gbr(em, imm_val);
        break;
    case 1:
        sh4asm_txt_movw_r0_a_disp8_gbr(em, imm_val << 1);
        break;
    case 2:
        sh4asm_txt_movl_r0_a_disp8_gbr(em, imm_val << 2);
        break;
    case 3:
        sh4asm_txt_trapa_imm8(em, imm_val);
        break;
    case 4:
        sh4asm_txt_movb_a_disp8_gbr_r0(em, imm_val);
        break;
    case 5:
        sh4asm_txt_movw_a_disp8_gbr_r0(em, imm_val << 1);
        break;
    case 6:
        sh4asm_txt_movl_a_disp8_gbr_r0(em, imm_val << 2);
        break;
    case 7:
        sh4asm_txt_mova_a_disp8_pc_r0(em, 4 * disp8 + 4, pc);
        break;
    case 8:
        sh4asm_txt_tst_imm8_r0(em, imm_val);
        break;
    case 9:
        sh4asm_txt_and_imm8_r0(em, imm_val);
        break;
    case 10:
        sh4asm_txt_xor_imm8_r0(em, imm_val);
        break;
    case 11:
        sh4asm_txt_or_imm8_r0(em, imm_val);
        break;
    case 12:
        sh4asm_txt_tstb_imm8_a_r0_gbr(em, imm_val);
        break;
    case 13:
        sh4asm_txt_andb_imm8_a_r0_gbr(em, imm_val);
        break;
    case 14:
        sh4asm_txt_xorb_imm8_a_r0_gbr(em, imm_val);
        break;
    case 15:
        sh4asm_txt_orb_imm8_a_r0_gbr(em, imm_val);
        break;
    default:
        sh4asm_opcode_non_inst_(quads, em);
    }
}

SH4ASM_STATIC void
sh4asm_disas_dxxx_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    // mask is 0xf000
    int disp8 = (quads[1] << 4) | quads[0];
    if (disp8 & (1 << 7))
        disp8 |= ~SH4ASM_BIT_RANGE(0, 7);
    unsigned reg_no = quads[2];
    sh4asm_txt_movl_a_disp8_pc_rn(em, 4 * disp8 + 4, pc, reg_no);
}

SH4ASM_STATIC void
sh4asm_disas_exxx_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    // mask is 0xf000
    unsigned imm_val = (quads[1] << 4) | quads[0];
    unsigned reg_no = quads[2];
    sh4asm_txt_mov_imm8_rn(em, imm_val, reg_no);
}

SH4ASM_STATIC void
sh4asm_disas_fxfd_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    switch (quads[2]) {
    case 1:
    case 5:
    case 9:
    case 13:
        sh4asm_txt_ftrv_xmtrx_fvn(em, (quads[2] & 12));
        break;
    case 11:
        sh4asm_txt_frchg(em);
        break;
    case 3:
        sh4asm_txt_fschg(em);
        break;
    case 0:
    case 2:
    case 4:
    case 6:
    case 8:
    case 10:
    case 12:
    case 14:
        sh4asm_txt_fsca_fpul_drn(em, quads[2]);
        break;
    default:
        sh4asm_opcode_non_inst_(quads, em);
    }
}

SH4ASM_STATIC void
sh4asm_disas_fxxd_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    switch (quads[1]) {
    case 0:
        sh4asm_txt_fsts_fpul_frn(em, quads[2]);
        break;
    case 1:
        sh4asm_txt_flds_frm_fpul(em, quads[2]);
        break;
    case 2:
        sh4asm_txt_float_fpul_frn(em, quads[2]);
        break;
    case 3:
        sh4asm_txt_ftrc_frm_fpul(em, quads[2]);
        break;
    case 4:
        sh4asm_txt_fneg_frn(em, quads[2]);
        break;
    case 5:
        sh4asm_txt_fabs_frn(em, quads[2]);
        break;
    case 6:
        sh4asm_txt_fsqrt_frn(em, quads[2]);
        break;
    case 7:
        sh4asm_txt_fsrra_frn(em, quads[2]);
        break;
    case 8:
        sh4asm_txt_fldi0_frn(em, quads[2]);
        break;
    case 9:
        sh4asm_txt_fldi1_frn(em, quads[2]);
        break;
    case 10:
        // mask is 0xf1ff
        if (!(quads[2] & 1))
            sh4asm_txt_fcnvsd_fpul_drn(em, quads[2]);
        else
            sh4asm_opcode_non_inst_(quads, em);
        break;
    case 11:
        // mask is 0xf1ff
        if (!(quads[2] & 1))
            sh4asm_txt_fcnvds_drm_fpul(em, quads[2]);
        else
            sh4asm_opcode_non_inst_(quads, em);
        break;
    case 14:
        sh4asm_txt_fipr_fvm_fvn(em, (quads[2] & 3) << 2, quads[2] & 12);
        break;
    case 15:
        sh4asm_disas_fxfd_(quads, em, pc);
        break;
    default:
        sh4asm_opcode_non_inst_(quads, em);
    }
}

SH4ASM_STATIC void
sh4asm_disas_fxxx_(unsigned const *quads,
                   sh4asm_disas_emit_func em, uint32_t pc) {
    switch (quads[0]) {
    case 0:
        // mask is f00f
        // alternatively, the mask is f11f for the double-precision version
        sh4asm_txt_fadd_frm_frn(em, quads[1], quads[2]);
        break;
    case 1:
        // mask is f00f
        // alternatively, the mask is f11f for the double-precision version
        sh4asm_txt_fsub_frm_frn(em, quads[1], quads[2]);
        break;
    case 2:
        // mask is f00f
        // alternatively, the mask is f11f for the double-precision version
        sh4asm_txt_fmul_frm_frn(em, quads[1], quads[2]);
        break;
    case 3:
        // mask is f00f
        // alternatively, the mask is f11f for the double-precision version
        sh4asm_txt_fdiv_frm_frn(em, quads[1], quads[2]);
        break;
    case 4:
        // mask is f00f
        // alternatively, the mask is f11f for the double-precision version
        sh4asm_txt_fcmpeq_frm_frn(em, quads[1], quads[2]);
        break;
    case 5:
        // mask is f00f
        // alternatively, the mask is f11f for the double-precision version
        sh4asm_txt_fcmpgt_frm_frn(em, quads[1], quads[2]);
        break;
    case 6:
        // mask is f00f
        // alternatively, the mask is f10f for the double-precision version
        sh4asm_txt_fmovs_a_r0_rm_frn(em, quads[1], quads[2]);
        break;
    case 7:
        // mask is f00f
        // alternatively, the mask is f01f for the double-precision version
        sh4asm_txt_fmovs_frm_a_r0_rn(em, quads[1], quads[2]);
        break;
    case 8:
        // mask is f00f
        // alternatively, the mask is f10f for the double-precision versions
        sh4asm_txt_fmovs_arm_frn(em, quads[1], quads[2]);
        break;
    case 9:
        // mask is f00f
        // alternatively, the mask is f10f for the double-precision versions
        sh4asm_txt_fmovs_armp_frn(em, quads[1], quads[2]);
        break;
    case 10:
        // mask is f00f
        // alternatively, the mask is f01f for the double-precision version
        sh4asm_txt_fmovs_frm_arn(em, quads[1], quads[2]);
        break;
    case 11:
        // mask is f00f
        // alternatively, the mask is f01f for the double-precision version
        sh4asm_txt_fmovs_frm_amrn(em, quads[1], quads[2]);
        break;
    case 12:
        // mask is f00f
        // alternatively, the mask is f11f for the double-precision version
        sh4asm_txt_fmov_frm_frn(em, quads[1], quads[2]);
        break;
    case 13:
        sh4asm_disas_fxxd_(quads, em, pc);
        break;
    case 14:
        // mask is f00f
        sh4asm_txt_fmac_fr0_frm_frn(em, quads[1], quads[2]);
        break;
    default:
        sh4asm_opcode_non_inst_(quads, em);
    }
}

SH4ASM_STATIC void
sh4asm_disas_inst(uint16_t inst, sh4asm_disas_emit_func em, uint32_t pc) {
    unsigned const quads[4] = {
        inst & 0xfu,
        (inst & 0x00f0u) >> 4,
        (inst & 0x0f00u) >> 8,
        (inst & 0xf000u) >> 12
    };
    switch (quads[3]) {
    case 0:
        sh4asm_disas_0xxx_(quads, em, pc);
        break;
    case 1:
        // mask is 0xf000
        sh4asm_disas_1xxx_(quads, em, pc);
        break;
    case 2:
        // mask is 0xf00f
        sh4asm_disas_2xxx_(quads, em, pc);
        break;
    case 3:
        // mask is 0xf00f
        sh4asm_disas_3xxx_(quads, em, pc);
        break;
    case 4:
        sh4asm_disas_4xxx_(quads, em, pc);
        break;
    case 5:
        // mask is 0xf000
        sh4asm_disas_5xxx_(quads, em, pc);
        break;
    case 6:
        // mask is 0xf00f
        sh4asm_disas_6xxx_(quads, em, pc);
        break;
    case 7:
        // mask is 0xf000
        sh4asm_disas_7xxx_(quads, em, pc);
        break;
    case 8:
        // mask is 0xff00
        sh4asm_disas_8xxx_(quads, em, pc);
        break;
    case 9:
        // mask is 0xf000
        sh4asm_disas_9xxx_(quads, em, pc);
        break;
    case 10:
        // mask is 0xf000
        sh4asm_disas_axxx_(quads, em, pc);
        break;
    case 11:
        // mask is 0xf000
        sh4asm_disas_bxxx_(quads, em, pc);
        break;
    case 12:
        // mask is 0xff00
        sh4asm_disas_cxxx_(quads, em, pc);
        break;
    case 13:
        // mask is 0xf000
        sh4asm_disas_dxxx_(quads, em, pc);
        break;
    case 14:
        // mask is 0xf000
        sh4asm_disas_exxx_(quads, em, pc);
        break;
    case 15:
        // floating-point opcodes
        sh4asm_disas_fxxx_(quads, em, pc);
        break;
    default:
        goto non_inst;
    }
    return;
 non_inst:
    sh4asm_opcode_non_inst_(quads, em);
}

#ifdef __cplusplus
}
#endif

#endif
