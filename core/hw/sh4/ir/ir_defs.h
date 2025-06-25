#pragma once

#include <cstdint>
#include <vector>

namespace sh4 {
namespace ir {

// Simple IR opcode enumeration — extend as needed
enum class Op : uint8_t {
  NOP,
  ADD,
  SUB,
  MOV,
  LOAD8,
  LOAD16,
  LOAD32,
  STORE8,
  STORE16,
  STORE32,
  // Post-increment addressing forms
  LOAD8_POST,
  LOAD16_POST,
  LOAD32_POST,
  STORE8_POST,
  STORE16_POST,
  STORE32_POST,
  STORE8_PREDEC,
  STORE16_PREDEC,
  STORE32_PREDEC,
  MOV_B_REG_PREDEC,
  GET_MACH,
  GET_MACL,
  GET_PR,
  AND,
  OR,
  XOR,
  SHL,
  SHR,
  SAR,
  SWAP_B,
  SWAP_W,
  SHR_OP,
  SAR_OP,
  DT,
  AND_IMM,
  OR_IMM,
  XOR_IMM,
  NOT_OP,
  SHL1,
  SHR1,
  SAR1,
  AND_REG,
  OR_REG,
  XOR_REG,
  LOAD8_GBR,
  LOAD16_GBR,
  LOAD32_GBR,
  STORE8_GBR,
  STORE16_GBR,
  STORE32_GBR,
  MOVA,
  MULU_W,
  MULS_W,
  CMP_STR,
  BRANCH,
  END,
  CMP_EQ,
  CMP_EQ_IMM,  // Compare R0 with immediate value
  CMP_PL,
  CMP_HI,
  CMP_HS,
  TST_IMM,
  TST_REG,
  MOVT,
  BF,
  BT,
  // --- Auto-generated opcodes, extended for full SH4 coverage ---
  ADDC,
  ADDV,
  AND_B,
  BF_S,
  BRAF,
  BSR,
  BSRF,
  BT_S,
  CLRMAC,
  CLRS,
  CLRT,
  CMP_GE,
  CMP_GT,
  CMP_PZ,
  DIV0S,
  DIV0U,
  DIV1,
  DMULS_L,
  DMULU_L,
  EXTS_B,
  EXTS_W,
  EXTU_B,
  EXTU_W,
  FABS,
  FADD,
  FCMP_EQ,
  FCMP_GT,
  FCNVDS,
  FCNVSD,
  FDIV,
  FIPR,
  FLDI0,
  FLDI1,
  FLDS,
  FLOAT,
  FMAC,
  FMOV,
  FMOV_S,
  FMUL,
  FNEG,
  FRCHG,
  FSCA,
  FSQRT,
  FSCHG,
  FSQRT_D,
  FSRRA,
  FSTS,
  FSUB,
  FTRC,
  FTRV,
  FNEG_FPUL,
  FABS_FPUL,
  JMP,
  LDC,
  LDC_L,
  LDS,
  LDS_L,
  LDTLB,
  MAC_L,
  MAC_W,
  MOVCA_L,
  MOV_B,
  MOV_L,
  MOV_W,
  MUL_L,
  NEG,
  NEGC,
  NOP0,
  NOT,
  OCBI,
  OCBP,
  OCBWB,
  OR_B,
  PREF,
  ROTCL,
  ROTCR,
  ROTL,
  ROTR,
  RTE,
  SETS,
  SETT,
  SHAD,
  SHAL,
  SHAR,
  SHLD,
  SHLL,
  SHLL16,
  SHLL2,
  SHLL8,
  SHLR,
  SHLR16,
  SHLR2,
  SHLR8,
  SLEEP,
  LDC_SR, // Load GPR to System Register (e.g., LDC Rm, SR)
  STC_SR, // Store System Register to GPR (e.g., STC SR, Rn)
  STC,
  STC_L,
  STS,
  STS_L,
  SUBC,
  SUBV,
  SUBX,
  TAS_B,
  TRAPA,
  TST,
  TST_B,
  XOR_B,
  XTRCT,
  // --- new for stack save/restore of PR ---
  STS_PR_L, // store PR to @-Rn (pre-decrement long)
  LDS_PR_L, // load PR from @Rn+ (post-increment long)
  // --- load control regs from stack ---
  LDC_SSR_L, // load SSR from @Rn+ (post-increment long)
  LDC_SPC_L, // load SPC from @Rn+ (post-increment long)
  LDC_SGR_L, // load SGR from @Rn+ (post-increment long)
  LDC_SR_L,  // load SR from @Rn+ (post-increment long)
  MOVA_PC,   // MOVA @(disp,PC),R0 - loads effective address into R0
#include "ir_defs_auto.inc"
           // --- R0 offset addressing variants ---
  LOAD8_R0,
  STORE8_R0,
  STORE16_R0,
  STORE32_R0,
  STORE8_R0_REG,
  STORE16_R0_REG,
  STORE32_R0_REG,
  // R0-indexed variants that store value from Rm (not R0)
  STORE8_Rm_R0RN,
  STORE16_Rm_R0RN,
  STORE32_Rm_R0RN,
  LOAD16_R0,
  LOAD32_R0,
  FMOV_STORE_PREDEC, // FMOV.S FRm,@-Rn
  NUM_OPS
};

enum class RegType : uint8_t {
  NONE,   // Default or not applicable
  GPR,    // General Purpose Register (R0-R15)
  FGR,    // Floating Point Register (FR0-FR15, DR0-DR14, XF0-XF14)
  SPR,    // System/Special Purpose Register (SR, GBR, VBR, SSR, SPC, SGR, DBR,
          // MACH, MACL, PR, FPUL, FPSCR)
  PC_REG, // Program Counter (used as a conceptual register in some ops)
};

// Operand kinds
struct Operand {
  bool isImm = false;
  uint8_t reg = 0;              // if !isImm, register index
  RegType type = RegType::NONE; // Type of register if 'reg' is used
  int32_t imm = 0;              // valid when isImm
};

struct Instr {
  Op op{Op::NOP};
  Operand dst{};
  Operand src1{};
  Operand src2{};
  uint32_t pc{0};
  uint16_t raw{0};
  int32_t extra = 0; // displacement / branch target etc.
};

struct Block {
  uint32_t pcStart = 0;
  std::vector<Instr> code;
  uint32_t pcNext = 0; // fall-through
};

} // namespace ir
} // namespace sh4
