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
#include "build.h"
#if FEAT_SHREC != DYNAREC_NONE
#include "blockmanager.h"
#include "ssa.h"
#include "decoder.h"
#include "hw/sh4/modules/mmu.h"
#include "hw/sh4/sh4_mem.h"

#define SHIL_MODE 2
#include "shil_canonical.h"

void SSAOptimizer::Optimize()
{
	AddVersionPass();
#if 0
	INFO_LOG(DYNAREC, "BEFORE");
	PrintBlock();
#endif

	ConstPropPass();
	// This should only be done for ram/vram/aram access
	// Disabled for now and probably not worth the trouble
	//WriteAfterWritePass();
	DeadCodeRemovalPass();
	SimplifyExpressionPass();
	CombineShiftsPass();
	DeadRegisterPass();
	IdentityMovePass();
	SingleBranchTargetPass();

#if 0
	if (stats.prop_constants > 0 || stats.dead_code_ops > 0 || stats.constant_ops_replaced > 0
			|| stats.dead_registers > 0 || stats.dyn_to_stat_blocks > 0 || stats.waw_blocks > 0 || stats.combined_shifts > 0)
	{
		//INFO_LOG(DYNAREC, "AFTER %08x", block->vaddr);
		//PrintBlock();
		INFO_LOG(DYNAREC, "STATS: %08x ops %zd constants %d constops replaced %d dead code %d dead regs %d dyn2stat blks %d waw %d shifts %d", block->vaddr, block->oplist.size(),
				stats.prop_constants, stats.constant_ops_replaced,
				stats.dead_code_ops, stats.dead_registers, stats.dyn_to_stat_blocks, stats.waw_blocks, stats.combined_shifts);
	}
#endif
}

void SSAOptimizer::AddVersionPass()
{
	memset(reg_versions, 0, sizeof(reg_versions));

	for (shil_opcode& op : block->oplist)
	{
		// FIXME shop_ifb should be assumed to increase versions too? (increment all reg_versions[])
		AddVersionToOperand(op.rs1, false);
		AddVersionToOperand(op.rs2, false);
		AddVersionToOperand(op.rs3, false);
		AddVersionToOperand(op.rd, true);
		AddVersionToOperand(op.rd2, true);
	}
}

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
bool SSAOptimizer::ExecuteConstOp(shil_opcode* op)
{
	if (!op->rs1.is_reg() && !op->rs2.is_reg() && !op->rs3.is_reg()
			&& (op->rs1.is_imm() || op->rs2.is_imm() || op->rs3.is_imm())
			&& op->rd.is_reg())
	{
		// Only immediate operands -> execute the op at compile time

		u32 rs1 = op->rs1.is_imm() ? op->rs1.imm_value() : 0;
		u32 rs2 = op->rs2.is_imm() ? op->rs2.imm_value() : 0;
		u32 rs3 = op->rs3.is_imm() ? op->rs3.imm_value() : 0;
		u32 rd;
		u32 rd2 = 0;

		switch (op->op)
		{
		case shop_mov32:
			rd = rs1;
			break;
		case shop_add:
			rd = shil_opcl_add::f1::impl(rs1, rs2);
			break;
		case shop_sub:
			rd = shil_opcl_sub::f1::impl(rs1, rs2);
			break;
		case shop_adc:
		case shop_sbc:
		case shop_negc:
		case shop_rocl:
		case shop_rocr:
			{
				u64 v;
				if (op->op == shop_adc)
					v = shil_opcl_adc::f1::impl(rs1, rs2, rs3);
				else if (op->op == shop_sbc)
					v = shil_opcl_sbc::f1::impl(rs1, rs2, rs3);
				else if (op->op == shop_rocl)
					v = shil_opcl_rocl::f1::impl(rs1, rs2);
				else if (op->op == shop_rocr)
					v = shil_opcl_rocr::f1::impl(rs1, rs2);
				else
					v = shil_opcl_negc::f1::impl(rs1, rs2);
				rd = (u32)v;
				rd2 = (u32)(v >> 32);

				shil_param op2_rd = shil_param(op->rd2._reg);
				op2_rd.version[0] = op->rd2.version[0];
				InsertMov32Op(op2_rd, shil_param(rd2));

				// the previous insert might have invalidated our reference
				op = &block->oplist[opnum - 1];
				op->rd2 = shil_param();
			}
			break;
		case shop_shl:
			rd = shil_opcl_shl::f1::impl(rs1, rs2);
			break;
		case shop_shr:
			rd = shil_opcl_shr::f1::impl(rs1, rs2);
			break;
		case shop_sar:
			rd = shil_opcl_sar::f1::impl(rs1, rs2);
			break;
		case shop_ror:
			rd = shil_opcl_ror::f1::impl(rs1, rs2);
			break;
		case shop_shld:
			rd = shil_opcl_shld::f1::impl(rs1, rs2);
			break;
		case shop_shad:
			rd = shil_opcl_shad::f1::impl(rs1, rs2);
			break;
		case shop_or:
			rd = shil_opcl_or::f1::impl(rs1, rs2);
			break;
		case shop_and:
			rd = shil_opcl_and::f1::impl(rs1, rs2);
			break;
		case shop_xor:
			rd = shil_opcl_xor::f1::impl(rs1, rs2);
			break;
		case shop_not:
			rd = shil_opcl_not::f1::impl(rs1);
			break;
		case shop_ext_s16:
			rd = shil_opcl_ext_s16::f1::impl(rs1);
			break;
		case shop_ext_s8:
			rd = shil_opcl_ext_s8::f1::impl(rs1);
			break;
		case shop_mul_i32:
			rd = shil_opcl_mul_i32::f1::impl(rs1, rs2);
			break;
		case shop_mul_u16:
			rd = shil_opcl_mul_u16::f1::impl(rs1, rs2);
			break;
		case shop_mul_s16:
			rd = shil_opcl_mul_s16::f1::impl(rs1, rs2);
			break;
		case shop_mul_u64:
		case shop_mul_s64:
			{
				u64 v;
				if (op->op == shop_mul_u64)
					v = shil_opcl_mul_u64::f1::impl(rs1, rs2);
				else
					v = shil_opcl_mul_s64::f1::impl(rs1, rs2);
				rd = (u32)v;
				rd2 = (u32)(v >> 32);

				shil_param op2_rd =  shil_param(op->rd2._reg);
				op2_rd.version[0] = op->rd2.version[0];
				InsertMov32Op(op2_rd, shil_param(rd2));

				// the previous insert might have invalidated our reference
				op = &block->oplist[opnum - 1];
				op->rd2 = shil_param();
			}
			break;
		case shop_test:
			rd = shil_opcl_test::f1::impl(rs1, rs2);
			break;
		case shop_neg:
			rd = shil_opcl_neg::f1::impl(rs1);
			break;
		case shop_swaplb:
			rd = shil_opcl_swaplb::f1::impl(rs1);
			break;
		case shop_seteq:
			rd = shil_opcl_seteq::f1::impl(rs1, rs2);
			break;
		case shop_setgt:
			rd = shil_opcl_setgt::f1::impl(rs1, rs2);
			break;
		case shop_setge:
			rd = shil_opcl_setge::f1::impl(rs1, rs2);
			break;
		case shop_setab:
			rd = shil_opcl_setab::f1::impl(rs1, rs2);
			break;
		case shop_setae:
			rd = shil_opcl_setae::f1::impl(rs1, rs2);
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
				u64 res =  op->op == shop_div32u ? shil_opcl_div32u::f1::impl(rs1, rs2, rs3) : shil_opcl_div32s::f1::impl(rs1, rs2, rs3);
				rd = (u32)res;
				constprop_values[RegValue(op->rd, 1)] = res >> 32;

				shil_param op2_rd =  shil_param((Sh4RegType)(op->rd._reg + 1));
				op2_rd.version[0] = op->rd.version[1];
				InsertMov32Op(op2_rd, shil_param(res >> 32));

				// the previous insert might have invalidated our reference
				op = &block->oplist[opnum - 1];
			}
			break;
		case shop_div32p2:
			rd = shil_opcl_div32p2::f1::impl(rs1, rs2, rs3);
			break;

		case shop_jdyn:
			{
				verify(BET_GET_CLS(block->BlockType) == BET_CLS_Dynamic);
				rs1 += rs2;
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
				block->NextBlock = NullAddress;
				block->has_jcond = false;
				// same remark regarding jdyn as in the previous case
				block->oplist.erase(block->oplist.begin() + opnum);
				opnum--;
				stats.dyn_to_stat_blocks++;

				return true;
			}
		case shop_fneg:
			{
				f32 frd = shil_opcl_fneg::f1::impl(reinterpret_cast<f32&>(rs1));
				rd = reinterpret_cast<u32&>(frd);
			}
			break;
		case shop_fadd:
			{
				f32 frd = shil_opcl_fadd::f1::impl(reinterpret_cast<f32&>(rs1), reinterpret_cast<f32&>(rs2));
				rd = reinterpret_cast<u32&>(frd);
			}
			break;
		case shop_fsub:
			{
				f32 frd = shil_opcl_fsub::f1::impl(reinterpret_cast<f32&>(rs1), reinterpret_cast<f32&>(rs2));
				rd = reinterpret_cast<u32&>(frd);
			}
			break;
		case shop_fmul:
			{
				f32 frd = shil_opcl_fmul::f1::impl(reinterpret_cast<f32&>(rs1), reinterpret_cast<f32&>(rs2));
				rd = reinterpret_cast<u32&>(frd);
			}
			break;
		case shop_fdiv:
			{
				f32 frd = shil_opcl_fdiv::f1::impl(reinterpret_cast<f32&>(rs1), reinterpret_cast<f32&>(rs2));
				rd = reinterpret_cast<u32&>(frd);
			}
			break;
		case shop_cvt_i2f_n:
			{
				f32 frd = shil_opcl_cvt_i2f_n::f1::impl(rs1);
				rd = reinterpret_cast<u32&>(frd);
			}
			break;
		case shop_cvt_i2f_z:
			{
				f32 frd = shil_opcl_cvt_i2f_z::f1::impl(rs1);
				rd = reinterpret_cast<u32&>(frd);
			}
			break;
		case shop_fsqrt:
			{
				f32 frd = shil_opcl_fsqrt::f1::impl(reinterpret_cast<f32&>(rs1));
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
				constprop_values[RegValue(op->rd, 1)] = rd_1;

				shil_param op2_rd =  shil_param((Sh4RegType)(op->rd._reg + 1));
				op2_rd.version[0] = op->rd.version[1];
				InsertMov32Op(op2_rd, shil_param(rd_1));

				// the previous insert might have invalidated our reference
				op = &block->oplist[opnum - 1];
				op->rd.type = FMT_F32;
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
			ERROR_LOG(DYNAREC, "unhandled constant op %d", op->op);
			die("unhandled constant op");
			break;
		}

		constprop_values[RegValue(op->rd)] = rd;
		if (op->rd2.is_r32())
			constprop_values[RegValue(op->rd2)] = rd2;
		ReplaceByMov32(*op, rd);

		return true;
	}
	else
	{
		return false;
	}
}

void SSAOptimizer::ConstPropPass()
{
	for (opnum = 0; opnum < (int)block->oplist.size(); opnum++)
	{
		shil_opcode& op = block->oplist[opnum];

		// TODO do shop_sub and others
		if (op.op != shop_setab && op.op != shop_setae && op.op != shop_setgt && op.op != shop_setge && op.op != shop_sub && op.op != shop_fsetgt
				 && op.op != shop_fseteq && op.op != shop_fdiv && op.op != shop_fsub && op.op != shop_fmac)
			ConstPropOperand(op.rs1);
		if (op.op != shop_rocr && op.op != shop_rocl && op.op != shop_fsetgt && op.op != shop_fseteq && op.op != shop_fmac)
			ConstPropOperand(op.rs2);
		if (op.op != shop_fmac && op.op != shop_adc)
			ConstPropOperand(op.rs3);

		if (op.op == shop_ifb)
		{
			constprop_values.clear();
		}
		else if (op.op == shop_sync_sr)
		{
			for (auto it = constprop_values.begin(); it != constprop_values.end(); )
			{
				Sh4RegType reg = it->first.get_reg();
				if (reg == reg_sr_status || (reg >= reg_r0 && reg <= reg_r7)
						|| (reg >= reg_r0_Bank && reg <= reg_r7_Bank))
					it = constprop_values.erase(it);
				else
					it++;
			}
		}
		else if (op.op == shop_div1)
		{
			for (auto it = constprop_values.begin(); it != constprop_values.end(); )
			{
				Sh4RegType reg = it->first.get_reg();
				if (reg == reg_sr_status)
					it = constprop_values.erase(it);
				else
					it++;
			}
		}
		else if (op.op == shop_sync_fpscr)
		{
			for (auto it = constprop_values.begin(); it != constprop_values.end(); )
			{
				Sh4RegType reg = it->first.get_reg();
				if (reg == reg_fpscr || reg == reg_old_fpscr || (reg >= reg_fr_0 && reg <= reg_xf_15))
					it = constprop_values.erase(it);
				else
					it++;
			}
		}
		else if (op.op == shop_readm || op.op == shop_writem)
		{
			if (op.rs1.is_imm() && !op.rs3.is_reg())
			{
				// Merge base addr and offset
				if (op.rs3.is_imm()) {
					op.rs1._imm += op.rs3.imm_value();
					op.rs3.type = FMT_NULL;
				}

				// If we know the address to read and it's in the same memory page(s) as the block
				// and if those pages are read-only, then we can directly read the memory at compile time
				// and propagate the read value as a constant.
				if (op.op == shop_readm  && block->read_only
						&& (op.rs1._imm >> 12) >= (block->vaddr >> 12)
						&& (op.rs1._imm >> 12) <= ((block->vaddr + block->sh4_code_size - 1) >> 12)
						&& op.size <= 4)
				{
					void *ptr;
					bool isRam;
					u32 paddr;
					if (rdv_readMemImmediate(op.rs1._imm, op.size, ptr, isRam, paddr, block) && isRam)
					{
						u32 v;
						switch (op.size)
						{
						case 1:
							v = (s32)*(::s8 *)ptr;
							break;
						case 2:
							v = (s32)*(::s16 *)ptr;
							break;
						case 4:
							v = *(u32 *)ptr;
							break;
						default:
							die("invalid size");
							v = 0;
							break;
						}
						ReplaceByMov32(op, v);
						constprop_values[RegValue(op.rd)] = v;
					}
				}
			}
			else
			{
				if (op.rs1.is_imm() && op.rs3.is_reg())
					// Swap rs1 and rs3 so that rs1 is never an immediate operand
					std::swap(op.rs1, op.rs3);
				if (op.rs3.is_imm() && op.rs3.imm_value() == 0)
					// 0 displacement has no effect
					op.rs3.type = FMT_NULL;
			}
		}
		else if (ExecuteConstOp(&op))
		{
		}
		else if (op.op == shop_and || op.op == shop_or || op.op == shop_xor || op.op == shop_add || op.op == shop_mul_s16 || op.op == shop_mul_u16
				  || op.op == shop_mul_i32 || op.op == shop_test || op.op == shop_seteq || op.op == shop_fseteq || op.op == shop_fadd || op.op == shop_fmul
				  || op.op == shop_mul_u64 || op.op == shop_mul_s64 || op.op == shop_adc || op.op == shop_setpeq)
		{
			if (op.rs1.is_imm() && op.rs2.is_reg())
			{
				// Swap rs1 and rs2 so that rs1 is never an immediate operand
				shil_param t = op.rs1;
				op.rs1 = op.rs2;
				op.rs2 = t;
			}
		}
		else if ((op.op == shop_shld || op.op == shop_shad) && op.rs2.is_imm())
		{
			// Replace shld/shad with shl/shr/sar
			u32 r2 = op.rs2.imm_value();
			if (r2 != 0)	// r2 == 0 is nop, handled in SimplifyExpressionPass()
			{
				if ((r2 & 0x1F) == 0)
				{
					if (op.op == shop_shld) {
						// rd = 0
						ReplaceByMov32(op, 0);
					}
					else
					{
						// rd = r1 >> 31
						op.op = shop_sar;
						op.rs2._imm = 31;
						stats.constant_ops_replaced++;
					}
				}
				else if ((r2 & 0x80000000) == 0)
				{
					// rd = r1 << (r2 & 0x1F)
					op.op = shop_shl;
					op.rs2._imm = r2 & 0x1F;
					stats.constant_ops_replaced++;
				}
				else
				{
					// rd = r1 >> ((~r2 & 0x1F) + 1)
					op.op = op.op == shop_shad ? shop_sar : shop_shr;
					op.rs2._imm = (~r2 & 0x1F) + 1;
					stats.constant_ops_replaced++;
				}
			}
		}
	}
}

void SSAOptimizer::DeadCodeRemovalPass()
{
	u32 last_versions[sh4_reg_count];
	std::set<RegValue> uses;

	memset(last_versions, -1, sizeof(last_versions));
	for (int opnum = (int)block->oplist.size() - 1; opnum >= 0; opnum--)
	{
		shil_opcode& op = block->oplist[opnum];
		bool dead_code = false;

		if (op.op == shop_ifb || (mmu_enabled() && (op.op == shop_readm || op.op == shop_writem)))
		{
			// if mmu enabled, mem accesses can throw an exception
			// so last_versions must be reset so the regs are correctly saved beforehand
			memset(last_versions, -1, sizeof(last_versions));
			continue;
		}
		if (op.op == shop_pref)
		{
			if (op.rs1.is_imm() && (op.rs1.imm_value() & 0xFC000000) != 0xE0000000)
				dead_code = true;
			else if (mmu_enabled())
			{
				memset(last_versions, -1, sizeof(last_versions));
				continue;
			}
		}
		if (op.op == shop_sync_sr)
		{
			last_versions[reg_sr_T] = -1;
			last_versions[reg_sr_status] = -1;
			for (int i = reg_r0; i <= reg_r7; i++)
				last_versions[i] = -1;
			for (int i = reg_r0_Bank; i <= reg_r7_Bank; i++)
				last_versions[i] = -1;
			continue;
		}
		if (op.op == shop_div1)
			last_versions[reg_sr_status] = -1;

		if (op.op == shop_sync_fpscr)
		{
			last_versions[reg_fpscr] = -1;
			last_versions[reg_old_fpscr] = -1;
			for (int i = reg_fr_0; i <= reg_xf_15; i++)
				last_versions[i] = -1;
			continue;
		}

		if (op.rd.is_reg())
		{
			bool unused_rd = true;
			for (u32 i = 0; i < op.rd.count(); i++)
			{
				if (last_versions[op.rd._reg + i] == (u32)-1)
				{
					last_versions[op.rd._reg + i] = op.rd.version[i];
					unused_rd = false;
					writeback_values.insert(RegValue(op.rd, i));
				}
				else
				{
					verify(op.rd.version[i] < last_versions[op.rd._reg + i]);
					if (uses.find(RegValue(op.rd, i)) != uses.end())
					{
						unused_rd = false;
					}
				}
			}
			dead_code = dead_code || unused_rd;
		}
		if (op.rd2.is_reg())
		{
			bool unused_rd = true;
			for (u32 i = 0; i < op.rd2.count(); i++)
			{
				if (last_versions[op.rd2._reg + i] == (u32)-1)
				{
					last_versions[op.rd2._reg + i] = op.rd2.version[i];
					unused_rd = false;
					writeback_values.insert(RegValue(op.rd2, i));
				}
				else
				{
					verify(op.rd2.version[i] < last_versions[op.rd2._reg + i]);
					if (uses.find(RegValue(op.rd2, i)) != uses.end())
					{
						unused_rd = false;
					}
				}
			}
			dead_code = dead_code && unused_rd;
		}
		if (dead_code && op.op != shop_readm)	// memory read on registers can have side effects
		{
			//printf("%08x DEAD %s\n", block->vaddr + op.guest_offs, op.dissasm().c_str());
			block->oplist.erase(block->oplist.begin() + opnum);
			stats.dead_code_ops++;
		}
		else
		{
			if (op.rs1.is_reg())
			{
				for (u32 i = 0; i < op.rs1.count(); i++)
					uses.insert(RegValue(op.rs1, i));
			}
			if (op.rs2.is_reg())
			{
				for (u32 i = 0; i < op.rs2.count(); i++)
					uses.insert(RegValue(op.rs2, i));
			}
			if (op.rs3.is_reg())
			{
				for (u32 i = 0; i < op.rs3.count(); i++)
					uses.insert(RegValue(op.rs3, i));
			}
		}
	}
}

void SSAOptimizer::SimplifyExpressionPass()
{
	for (size_t opnum = 0; opnum < block->oplist.size(); opnum++)
	{
		shil_opcode& op = block->oplist[opnum];
		if (op.rs2.is_imm() || op.rs2.is_null())
		{
			if (op.rs2.is_null() || op.rs2.imm_value() == 0)
			{
				// a & 0 == 0
				// a * 0 == 0
				// Not true for FPU ops because of Inf and NaN
				if (op.op == shop_and || op.op == shop_mul_i32 || op.op == shop_mul_s16 || op.op == shop_mul_u16)
				{
					//printf("%08x ZERO %s\n", block->vaddr + op.guest_offs, op.dissasm().c_str());
					ReplaceByMov32(op, 0);
				}
				// a * 0 == 0
				/* TODO 64-bit result
				else if (op.op == shop_mul_u64 || op.op == shop_mul_s64)
				{
					printf("%08x ZERO %s\n", block->vaddr + op.guest_offs, op.dissasm().c_str());
					ReplaceByMov32(op, 0);
				}
				*/
				// a + 0 == a
				// a - 0 == a
				// a | 0 == a
				// a ^ 0 == a
				// a >> 0 == a
				// a << 0 == a
				// Not true for FPU ops because of Inf and NaN
				else if (op.op == shop_add || op.op == shop_sub || op.op == shop_or || op.op == shop_xor
						|| op.op == shop_shl || op.op == shop_shr || op.op == shop_sar || op.op == shop_shad || op.op == shop_shld)
				{
					//printf("%08x IDEN %s\n", block->vaddr + op.guest_offs, op.dissasm().c_str());
					ReplaceByMov32(op);
				}
			}
			// a * 1 == a
			else if (op.rs2.imm_value() == 1
					&& (op.op == shop_mul_i32 || op.op == shop_mul_s16 || op.op == shop_mul_u16))
			{
				//printf("%08x IDEN %s\n", block->vaddr + op.guest_offs, op.dissasm().c_str());
				ReplaceByMov32(op);

				continue;
			}
		}
		// Not sure it's worth the trouble, except for the 'and' and 'xor'
		else if (op.rs1.is_r32i() && op.rs1._reg == op.rs2._reg)
		{
			// a + a == a * 2 == a << 1
			if (op.op == shop_add)
			{
				// There's quite a few of these
				//printf("%08x +t<< %s\n", block->vaddr + op.guest_offs, op.dissasm().c_str());
				op.op = shop_shl;
				op.rs2 = shil_param(1);
			}
			// a ^ a == 0
			// a - a == 0
			else if (op.op == shop_xor || op.op == shop_sub)
			{
				//printf("%08x ZERO %s\n", block->vaddr + op.guest_offs, op.dissasm().c_str());
				ReplaceByMov32(op, 0);
			}
			// SBC a, a == SBC 0,0
			else if (op.op == shop_sbc)
			{
				//printf("%08x ZERO %s\n", block->vaddr + op.guest_offs, op.dissasm().c_str());
				op.rs1 = shil_param(0);
				op.rs2 = shil_param(0);
				stats.prop_constants += 2;
			}
			// a & a == a
			// a | a == a
			else if (op.op == shop_and || op.op == shop_or)
			{
				//printf("%08x IDEN %s\n", block->vaddr + op.guest_offs, op.dissasm().c_str());
				ReplaceByMov32(op);
			}
		}
	}
}

void SSAOptimizer::DeadRegisterPass()
{
	std::map<RegValue, RegValue> aliases;		// (dest reg, version) -> (source reg, version)

	// Find aliases
	for (shil_opcode& op : block->oplist)
	{
		// ignore moves from/to int regs to/from fpu regs
		if (op.op == shop_mov32 && op.rs1.is_reg() && op.rd.is_r32i() == op.rs1.is_r32i())
		{
			RegValue dest_reg(op.rd);
			RegValue src_reg(op.rs1);
			auto it = aliases.find(src_reg);
			if (it != aliases.end())
				// use the final value if the src is itself aliased
				aliases[dest_reg] = it->second;
			else
				aliases[dest_reg] = src_reg;
		}
	}

	// Attempt to eliminate them
	for (auto& alias : aliases)
	{
		if (writeback_values.count(alias.first) > 0)
			continue;

		// Do a first pass to check that we can replace the value
		size_t defnum = -1;
		size_t usenum = -1;
		size_t aliasdef = -1;
		for (size_t opnum = 0; opnum < block->oplist.size(); opnum++)
		{
			shil_opcode* op = &block->oplist[opnum];
			// find def
			if (op->rd.is_r32() && RegValue(op->rd) == alias.first)
				defnum = opnum;
			else if (op->rd2.is_r32() && RegValue(op->rd2) == alias.first)
				defnum = opnum;

			// find alias redef
			if (aliasdef == (size_t)-1)
			{
				if (DefinesHigherVersion(op->rd, alias.second))
					aliasdef = opnum;
				else if (DefinesHigherVersion(op->rd2, alias.second))
					aliasdef = opnum;
				else if (op->op == shop_ifb)
					aliasdef = opnum;
			}

			// find last use
			if (UsesRegValue(op->rs1, alias.first))
			{
				if (op->rs1.count() == 1)
					usenum = opnum;
				else
				{
					usenum = 0xFFFF;	// Can't alias values used by vectors cuz they need adjacent regs
					aliasdef = 0;
					break;
				}
			}
			else if (UsesRegValue(op->rs2, alias.first))
			{
				if (op->rs2.count() == 1)
					usenum = opnum;
				else
				{
					usenum = 0xFFFF;
					aliasdef = 0;
					break;
				}
			}
			else if (UsesRegValue(op->rs3, alias.first))
			{
				if (op->rs3.count() == 1)
					usenum = opnum;
				else
				{
					usenum = 0xFFFF;
					aliasdef = 0;
					break;
				}
			}
		}
		verify(defnum != (size_t)-1);
		// If the alias is redefined before any use we can't use it
		if (aliasdef != (size_t)-1 && usenum != (size_t)-1 && aliasdef < usenum)
			continue;

		for (size_t opnum = defnum + 1; opnum <= usenum && usenum != (size_t)-1; opnum++)
		{
			shil_opcode* op = &block->oplist[opnum];
			ReplaceByAlias(op->rs1, alias.first, alias.second);
			ReplaceByAlias(op->rs2, alias.first, alias.second);
			ReplaceByAlias(op->rs3, alias.first, alias.second);
		}
		stats.dead_registers++;
		//printf("%08x DREG %s\n", block->vaddr + block->oplist[defnum].guest_offs, block->oplist[defnum].dissasm().c_str());
		block->oplist.erase(block->oplist.begin() + defnum);
	}
}

void SSAOptimizer::IdentityMovePass()
{
	// This pass creates holes in reg versions and should be run last
	// The versioning pass must be re-run if needed
	for (size_t opnum = 0; opnum < block->oplist.size(); opnum++)
	{
		shil_opcode& op = block->oplist[opnum];
		if (op.op == shop_mov32 && op.rs1.is_reg() && op.rd._reg == op.rs1._reg)
		{
			//printf("%08x DIDN %s\n", block->vaddr + op.guest_offs, op.dissasm().c_str());
			block->oplist.erase(block->oplist.begin() + opnum);
			opnum--;
			stats.dead_code_ops++;
		}
	}
}

void SSAOptimizer::CombineShiftsPass()
{
	for (int opnum = 0; opnum < (int)block->oplist.size() - 1; opnum++)
	{
		shil_opcode& op = block->oplist[opnum];
		shil_opcode& next_op = block->oplist[opnum + 1];
		if (op.op == next_op.op && (op.op == shop_shl || op.op == shop_shr || op.op == shop_sar) && next_op.rs1.is_r32i() && op.rd._reg == next_op.rs1._reg)
		{
			if (next_op.rs2._imm + op.rs2._imm <= 31)
			{
				next_op.rs2._imm += op.rs2._imm;
				//printf("%08x SHFT %s -> %d\n", block->vaddr + op.guest_offs, op.dissasm().c_str(), next_op.rs2._imm);
				ReplaceByMov32(op);
				stats.combined_shifts++;
			}
		}
	}
}

void SSAOptimizer::WriteAfterWritePass()
{
	for (int opnum = 0; opnum < (int)block->oplist.size() - 1; opnum++)
	{
		shil_opcode& op = block->oplist[opnum];
		shil_opcode& next_op = block->oplist[opnum + 1];
		if (op.op == next_op.op && op.op == shop_writem
				&& op.rs1.type == next_op.rs1.type
				&& op.rs1._imm == next_op.rs1._imm
				&& op.rs1.version[0] == next_op.rs1.version[0]
				&& op.rs3.type == next_op.rs3.type
				&& op.rs3._imm == next_op.rs3._imm
				&& op.rs3.version[0] == next_op.rs3.version[0]
				&& op.rs2.count() == next_op.rs2.count())
		{
			//printf("%08x DEAD %s\n", block->vaddr + op.guest_offs, op.dissasm().c_str());
			block->oplist.erase(block->oplist.begin() + opnum);
			opnum--;
			stats.dead_code_ops++;
			stats.waw_blocks++;
		}
	}
}

bool SSAOptimizer::skipSingleBranchTarget(u32& addr, bool updateCycles)
{
	if (addr == NullAddress)
		return false;
	bool success = false;
	const u32 start_page = block->vaddr >> 12;
	const u32 end_page = (block->vaddr + (block->guest_opcodes - 1) * 2) >> 12;
	for (int i = 0; i < 5; i++)
	{
		if ((addr >> 12) < start_page || ((addr + 2) >> 12) > end_page)
			break;

		u32 op = IReadMem16(addr);
		// Axxx: bra <bdisp12>
		if ((op & 0xF000) != 0xA000)
			break;

		u16 delayOp = IReadMem16(addr + 2);
		if (delayOp != 0x0000 && delayOp != 0x0009)	// nop
			break;

		int disp = GetSImm12(op) * 2 + 4;
		if (disp == 0)
			// infiniloop
			break;
		addr += disp;
		if (updateCycles)
		{
			dec_updateBlockCycles(block, op);
			dec_updateBlockCycles(block, delayOp);
		}
		success = true;
	}
	return success;
}

#endif	// FEAT_SHREC != DYNAREC_NONE
