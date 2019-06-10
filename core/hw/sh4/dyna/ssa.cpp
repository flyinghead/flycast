/*
    Created on: Jun 5, 2019

	Copyright 2019 flyinghead

	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "blockmanager.h"
#include "ssa.h"

#define SHIL_MODE 2
#include "shil_canonical.h"

void SSAOptimizer::InsertMov32Op(const shil_param& rd, const shil_param& rs)
{
	shil_opcode op2(block->oplist[opnum]);
	op2.op = shop_mov32;
	op2.rd = rd;
	op2.rd2 = shil_param();
	op2.rs1 = rs;
	op2.rs2 = shil_param();
	op2.rs3 = shil_param();

	block->oplist.insert(block->oplist.begin() + opnum + 1, op2);
	opnum++;


}
bool SSAOptimizer::ExecuteConstOp(shil_opcode& op)
{
	if (!op.rs1.is_reg() && !op.rs2.is_reg() && !op.rs3.is_reg()
			&& (op.rs1.is_imm() || op.rs2.is_imm() || op.rs3.is_imm())
			&& op.rd.is_reg())
	{
		// Only immediate operands -> execute the op at compile time

		u32 rs1 = op.rs1.is_imm() ? op.rs1.imm_value() : 0;
		u32 rs2 = op.rs2.is_imm() ? op.rs2.imm_value() : 0;
		u32 rs3 = op.rs3.is_imm() ? op.rs3.imm_value() : 0;
		u32 rd;
		u32 rd2;

		switch (op.op)
		{
		case shop_mov32:
			rd = rs1;
			break;
		case shop_add:
			rd = rs1 + rs2;
			break;
		case shop_sub:
			rd = rs1 - rs2;
			break;
		case shop_adc:
		case shop_sbc:
		case shop_negc:
		case shop_rocl:
		case shop_rocr:
			{
				u64 v;
				if (op.op == shop_adc)
					v = shil_opcl_adc::f1::impl(rs1, rs2, rs3);
				else if (op.op == shop_sbc)
					v = shil_opcl_sbc::f1::impl(rs1, rs2, rs3);
				else if (op.op == shop_rocl)
					v = shil_opcl_rocl::f1::impl(rs1, rs2);
				else if (op.op == shop_rocr)
					v = shil_opcl_rocr::f1::impl(rs1, rs2);
				else
					v = shil_opcl_negc::f1::impl(rs1, rs2);
				rd = (u32)v;
				rd2 = (u32)(v >> 32);

				shil_param op2_rd = shil_param(op.rd2._reg);
				op2_rd.version[0] = op.rd2.version[0];
				InsertMov32Op(op2_rd, shil_param(FMT_IMM, (u32)(v >> 32)));

				op.rd2 = shil_param();
			}
			break;
		case shop_shl:
			rd = rs1 << rs2;
			break;
		case shop_shr:
			rd = rs1 >> rs2;
			break;
		case shop_sar:
			rd = (s32) rs1 >> rs2;
			break;
		case shop_ror:
			rd = (rs1 >> rs2)
					| (rs1 << (32 - rs2));
			break;
		case shop_shld:
			rd = shil_opcl_shld::f1::impl(rs1, rs2);
			break;
		case shop_shad:
			rd = shil_opcl_shad::f1::impl(rs1, rs2);
			break;
		case shop_or:
			rd = rs1 | rs2;
			break;
		case shop_and:
			rd = rs1 & rs2;
			break;
		case shop_xor:
			rd = rs1 ^ rs2;
			break;
		case shop_not:
			rd = ~rs1;
			break;
		case shop_ext_s16:
			rd = (s32)(s16)rs1;
			break;
		case shop_ext_s8:
			rd = (s32)(s8)rs1;
			break;
		case shop_mul_i32:
			rd = rs1 * rs2;
			break;
		case shop_mul_u16:
			rd = (u16)(rs1 * rs2);
			break;
		case shop_mul_s16:
			rd = (s16)(rs1 * rs2);
			break;
		case shop_mul_u64:
		case shop_mul_s64:
			{
				u64 v;
				if (op.op == shop_mul_u64)
					v = shil_opcl_mul_u64::f1::impl(rs1, rs2);
				else
					v = shil_opcl_mul_s64::f1::impl(rs1, rs2);
				rd = (u32)v;
				rd2 = (u32)(v >> 32);

				shil_param op2_rd =  shil_param(op.rd2._reg);
				op2_rd.version[0] = op.rd2.version[0];
				InsertMov32Op(op2_rd, shil_param(FMT_IMM, rd2));

				op.rd2 = shil_param();
			}
			break;
		case shop_test:
			rd = (rs1 & rs2) == 0;
			break;
		case shop_neg:
			rd = -rs1;
			break;
		case shop_swaplb:
			rd = shil_opcl_swaplb::f1::impl(rs1);
			break;
		case shop_swap:
			rd = shil_opcl_swap::f1::impl(rs1);
			break;
		case shop_seteq:
		case shop_setgt:
		case shop_setge:
		case shop_setab:
		case shop_setae:
			{
				switch (op.op)
				{
				case shop_seteq:
					rd = rs1 == rs2;
					break;
				case shop_setge:
					rd = (s32)rs1 >= (s32)rs2;
					break;
				case shop_setgt:
					rd = (s32)rs1 > (s32)rs2;
					break;
				case shop_setab:
					rd = rs1 > rs2;
					break;
				case shop_setae:
					rd = rs1 >= rs2;
					break;
				default:
					break;
				}
			}
			break;
		case shop_setpeq:
			rd = shil_opcl_setpeq::f1::impl(rs1, rs2);
			break;
		case shop_xtrct:
			rd = shil_opcl_xtrct::f1::impl(rs1, rs2);
			break;

		case shop_div32u:
		case shop_div32s:
			{
				u64 res =  op.op == shop_div32u ? shil_opcl_div32u::f1::impl(rs1, rs2) : shil_opcl_div32s::f1::impl(rs1, rs2);
				rd = (u32)res;
				constprop_values[RegValue(op.rd, 1)] = res >> 32;

				shil_param op2_rd =  shil_param((Sh4RegType)(op.rd._reg + 1));
				op2_rd.version[0] = op.rd.version[1];
				InsertMov32Op(op2_rd, shil_param(FMT_IMM, res >> 32));
			}
			break;
		case shop_div32p2:
			rd = shil_opcl_div32p2::f1::impl(rs1, rs2, rs3);
			break;

		case shop_jdyn:
			{
				verify(BET_GET_CLS(block->BlockType) == BET_CLS_Dynamic);
				switch ((block->BlockType >> 1) & 3)
				{
				case BET_SCL_Jump:
				case BET_SCL_Ret:
					block->BlockType = BET_StaticJump;
					block->BranchBlock = rs1;
					break;
				case BET_SCL_Call:
					block->BlockType = BET_StaticCall;
					block->BranchBlock = rs1;
					break;
				case BET_SCL_Intr:
					block->BlockType = BET_StaticIntr;
					block->NextBlock = rs1;
					break;
				default:
					die("Unexpected block end type\n")
					;
				}
				// rd (that is jdyn) won't be updated but since it's not a real register
				// it shouldn't be a problem
				block->oplist.erase(block->oplist.begin() + opnum);
				opnum--;
				stats.dyn_to_stat_blocks++;

				return true;
			}
		case shop_jcond:
			{
				if (rs1 != (block->BlockType & 1))
				{
					block->BranchBlock = block->NextBlock;
				}
				block->BlockType = BET_StaticJump;
				block->NextBlock = 0xFFFFFFFF;
				block->has_jcond = false;
				// same remark regarding jdyn as in the previous case
				block->oplist.erase(block->oplist.begin() + opnum);
				opnum--;
				stats.dyn_to_stat_blocks++;

				return true;
			}
		case shop_fneg:
			rd = rs1 ^ 0x80000000;
			break;
		case shop_fadd:
			{
				f32 frd = reinterpret_cast<f32&>(rs1) + reinterpret_cast<f32&>(rs2);
				rd = reinterpret_cast<u32&>(frd);
			}
			break;
		case shop_fsub:
			{
				f32 frd = reinterpret_cast<f32&>(rs1) - reinterpret_cast<f32&>(rs2);
				rd = reinterpret_cast<u32&>(frd);
			}
			break;
		case shop_fmul:
			{
				f32 frd = reinterpret_cast<f32&>(rs1) * reinterpret_cast<f32&>(rs2);
				rd = reinterpret_cast<u32&>(frd);
			}
			break;
		case shop_fdiv:
			{
				f32 frd = reinterpret_cast<f32&>(rs1) / reinterpret_cast<f32&>(rs2);
				rd = reinterpret_cast<u32&>(frd);
			}
			break;
		case shop_cvt_i2f_n:
		case shop_cvt_i2f_z:
			{
				f32 frd = (float)(s32) rs1;
				rd = reinterpret_cast<u32&>(frd);
			}
			break;
		case shop_fsqrt:
			{
				f32 frd = sqrtf(reinterpret_cast<f32&>(rs1));
				rd = reinterpret_cast<u32&>(frd);
			}
			break;
		case shop_cvt_f2i_t:
			{
				f32 r1 = reinterpret_cast<f32&>(rs1);
				rd = shil_opcl_cvt_f2i_t::f1::impl(r1);
			}
			break;
		case shop_fsrra:
			{
				f32 frd = shil_opcl_fsrra::f1::impl(reinterpret_cast<f32&>(rs1));
				rd = reinterpret_cast<u32&>(frd);
			}
			break;
		case shop_fsca:
			{
				f32 tmp[2];
				shil_opcl_fsca::fsca_table::impl(tmp, rs1);
				rd = reinterpret_cast<u32&>(tmp[0]);
				u32 rd_1 = reinterpret_cast<u32&>(tmp[1]);
				constprop_values[RegValue(op.rd, 1)] = rd_1;

				shil_param op2_rd =  shil_param((Sh4RegType)(op.rd._reg + 1));
				op2_rd.version[0] = op.rd.version[1];
				InsertMov32Op(op2_rd, shil_param(FMT_IMM, rd_1));

				op.rd.type = FMT_F32;
			}
			break;
		case shop_fabs:
			{
				f32 frd = shil_opcl_fabs::f1::impl(reinterpret_cast<f32&>(rs1));
				rd = reinterpret_cast<u32&>(frd);
			}
			break;
		case shop_fmac:
			{
				f32 frd = shil_opcl_fmac::f1::impl(reinterpret_cast<f32&>(rs1), reinterpret_cast<f32&>(rs2), reinterpret_cast<f32&>(rs3));
				rd = reinterpret_cast<u32&>(frd);
			}
			break;
		case shop_fseteq:
			rd = shil_opcl_fseteq::f1::impl(reinterpret_cast<f32&>(rs1), reinterpret_cast<f32&>(rs2));
			break;
		case shop_fsetgt:
			rd = shil_opcl_fsetgt::f1::impl(reinterpret_cast<f32&>(rs1), reinterpret_cast<f32&>(rs2));
			break;

		default:
			printf("unhandled constant op %d\n", op.op);
			die("unhandled constant op");
			break;
		}

		constprop_values[RegValue(op.rd)] = rd;
		if (op.rd2.is_r32())
			constprop_values[RegValue(op.rd2)] = rd2;
		ReplaceByMov32(op, rd);

		return true;
	}
	else
	{
		return false;
	}
}
