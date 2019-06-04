/*
	Created on: Jun 2, 2019

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
#include <stdio.h>
#include <set>
#include <map>
#include <deque>
#include <math.h>
#include "types.h"
#include "decoder.h"
#include "hw/sh4/modules/mmu.h"

class SSAOptimizer
{
public:
	SSAOptimizer(RuntimeBlockInfo* blk) : block(blk) {}

	void Optimize()
	{
		AddVersionPass();
#if DEBUG
		printf("BEFORE\n");
		PrintBlock();
#endif

		ConstPropPass();
		DeadCodeRemovalPass();
		ConstantExpressionsPass();
		//DeadRegisterPass();
		//DoRegAlloc();

#if DEBUG
		if (stats.prop_constants > 0 || stats.dead_code_ops > 0 || stats.constant_ops_replaced > 0
				|| stats.constant_ops_removed || stats.dead_registers > 0)
		{
			printf("AFTER\n");
			PrintBlock();
			printf("STATS: constants %d constant ops replaced %d removed %d dead code %d dead regs %d\n\n", stats.prop_constants, stats.constant_ops_replaced,
					stats.constant_ops_removed, stats.dead_code_ops, stats.dead_registers);
		}
#endif
	}

private:
	// References a specific version of a register value
	class RegValue : public std::pair<Sh4RegType, u32>
	{
	public:
		RegValue(const shil_param& param, int index = 0)
			: std::pair<Sh4RegType, u32>((Sh4RegType)(param._reg + index), param.version[index])
		{
			verify(param.is_reg());
			verify(index >= 0 && index < param.count());
		}
		RegValue() : std::pair<Sh4RegType, u32>() { }

		Sh4RegType get_reg() const { return first; }
		u32 get_version() const { return second; }
	};

	struct reg_alloc {
		u32 host_reg;
		u16 version;
		bool write_back;
	};

	void PrintBlock()
	{
		for (const shil_opcode& op : block->oplist)
		{
			printf("%08x  %s\n", block->vaddr + op.guest_offs, op.dissasm().c_str());
		}
	}
	void AddVersionToOperand(shil_param& param, bool define)
	{
		if (param.is_reg())
		{
			if (define)
			{
				for (int i = 0; i < param.count(); i++)
					reg_versions[param._reg + i]++;
			}
			for (int i = 0; i < param.count(); i++)
				param.version[i] = reg_versions[param._reg + i];
		}
	}

	void AddVersionPass()
	{
		memset(reg_versions, 0, sizeof(reg_versions));

		for (shil_opcode& op : block->oplist)
		{
			AddVersionToOperand(op.rs1, false);
			AddVersionToOperand(op.rs2, false);
			AddVersionToOperand(op.rs3, false);
			AddVersionToOperand(op.rd, true);
			AddVersionToOperand(op.rd2, true);
		}
	}

	// mov rd, #v
	void ReplaceByMov32(shil_opcode& op, u32 v)
	{
		op.op = shop_mov32;
		op.rs1 = shil_param(FMT_IMM, v);
		op.rs2.type = FMT_NULL;
		op.rs3.type = FMT_NULL;
		stats.constant_ops_replaced++;
	}

	// mov rd, rs1
	void ReplaceByMov32(shil_opcode& op)
	{
		op.op = shop_mov32;
		op.rs2.type = FMT_NULL;
		op.rs3.type = FMT_NULL;
	}

	void ConstPropOperand(shil_param& param)
	{
		if (param.is_r32())
		{
			auto it = constprop_values.find(RegValue(param));
			if (it != constprop_values.end())
			{
				param.type = FMT_IMM;
				param._imm = it->second;
				stats.prop_constants++;
			}
		}
	}

	void ConstPropPass()
	{
		for (int opnum = 0; opnum < block->oplist.size(); opnum++)
		{
			shil_opcode& op = block->oplist[opnum];

			if (op.op != shop_setab && op.op != shop_setae && op.op != shop_setgt && op.op != shop_setge && op.op != shop_sub && op.op != shop_fsetgt
					 && op.op != shop_fseteq && op.op != shop_fdiv && op.op != shop_fsub && op.op != shop_fmac)
				ConstPropOperand(op.rs1);
			if (op.op != shop_rocr && op.op != shop_rocl && op.op != shop_fsetgt && op.op != shop_fseteq && op.op != shop_fmac)
				ConstPropOperand(op.rs2);
			if (op.op != shop_fmac && op.op != shop_adc)
				ConstPropOperand(op.rs3);

			if (op.op == shop_readm || op.op == shop_writem)
			{
				if (op.rs1.is_imm())
				{
					if (op.rs3.is_imm())
					{
						// Merge base addr and offset
						op.rs1._imm += op.rs3.imm_value();
						op.rs3.type = FMT_NULL;
					}
					else if (op.rs3.is_reg())
					{
						// Swap rs1 and rs3 so that rs1 is never an immediate operand
						shil_param t = op.rs1;
						op.rs1 = op.rs3;
						op.rs3 = t;
					}
				}
			}
			else if (!op.rs1.is_reg() && !op.rs2.is_reg() && !op.rs3.is_reg()
					&& (op.rs1.is_imm() || op.rs2.is_imm() || op.rs3.is_imm())
					&& op.rd.is_r32())
			{
				// Only immediate operands -> execute the op at compile time
				//printf("%08x IMM %s --> \n", block->vaddr + op.guest_offs, op.dissasm().c_str());
				const RegValue dest_reg(op.rd);

				if (op.op == shop_mov32)
				{
					constprop_values[dest_reg] = op.rs1.imm_value();
				}
				else if (op.op == shop_add)
				{
					u32 v = constprop_values[dest_reg]
								   = op.rs1.imm_value() + op.rs2.imm_value();
					ReplaceByMov32(op, v);
				}
				else if (op.op == shop_sub)
				{
					u32 v = constprop_values[dest_reg]
								   = op.rs1.imm_value() - op.rs2.imm_value();
					ReplaceByMov32(op, v);
				}
				else if (op.op == shop_shl)
				{
					u32 v = constprop_values[dest_reg]
								   = op.rs1.imm_value() << op.rs2.imm_value();
					ReplaceByMov32(op, v);
				}
				else if (op.op == shop_shr)
				{
					u32 v = constprop_values[dest_reg]
								   = op.rs1.imm_value() >> op.rs2.imm_value();
					ReplaceByMov32(op, v);
				}
				else if (op.op == shop_sar)
				{
					u32 v = constprop_values[dest_reg]
								   = (s32)op.rs1.imm_value() >> op.rs2.imm_value();
					ReplaceByMov32(op, v);
				}
				else if (op.op == shop_ror)
				{
					u32 v = constprop_values[dest_reg]
								   = (op.rs1.imm_value() >> op.rs2.imm_value())
								   | (op.rs1.imm_value() << (32 - op.rs2.imm_value()));
					ReplaceByMov32(op, v);
				}
				else if (op.op == shop_shld)
				{
					u32 r1 = op.rs1.imm_value();
					u32 r2 = op.rs2.imm_value();
					u32 rd = shil_opcl_shld::f1::impl(r1, r2);

					constprop_values[dest_reg] = rd;
					ReplaceByMov32(op, rd);
				}
				else if (op.op == shop_shad)
				{
					u32 r1 = op.rs1.imm_value();
					u32 r2 = op.rs2.imm_value();
					u32 rd = shil_opcl_shad::f1::impl(r1, r2);

					constprop_values[dest_reg] = rd;
					ReplaceByMov32(op, rd);
				}
				else if (op.op == shop_or)
				{
					u32 v = constprop_values[dest_reg]
								   = op.rs1.imm_value() | op.rs2.imm_value();
					ReplaceByMov32(op, v);
				}
				else if (op.op == shop_and)
				{
					u32 v = constprop_values[dest_reg]
								   = op.rs1.imm_value() & op.rs2.imm_value();
					ReplaceByMov32(op, v);
				}
				else if (op.op == shop_xor)
				{
					u32 v = constprop_values[dest_reg]
								   = op.rs1.imm_value() ^ op.rs2.imm_value();
					ReplaceByMov32(op, v);
				}
				else if (op.op == shop_not)
				{
					u32 v = constprop_values[dest_reg]
								   = ~op.rs1.imm_value();
					ReplaceByMov32(op, v);
				}
				else if (op.op == shop_ext_s16)
				{
					u32 v = constprop_values[dest_reg]
								   = (s32)(s16)op.rs1.imm_value();
					ReplaceByMov32(op, v);
				}
				else if (op.op == shop_ext_s8)
				{
					u32 v = constprop_values[dest_reg]
								   = (s32)(s8)op.rs1.imm_value();
					ReplaceByMov32(op, v);
				}
				else if (op.op == shop_mul_i32)
				{
					u32 v = constprop_values[dest_reg]
								   = op.rs1.imm_value() * op.rs2.imm_value();
					ReplaceByMov32(op, v);
				}
				else if (op.op == shop_mul_u16)
				{
					u32 v = constprop_values[dest_reg]
								   = (u16)(op.rs1.imm_value() * op.rs2.imm_value());
					ReplaceByMov32(op, v);
				}
				else if (op.op == shop_mul_s16)
				{
					u32 v = constprop_values[dest_reg]
								   = (s16)(op.rs1.imm_value() * op.rs2.imm_value());
					ReplaceByMov32(op, v);
				}
				else if (op.op == shop_test)
				{
					u32 v = constprop_values[dest_reg]
								   = (op.rs1.imm_value() & op.rs2.imm_value()) == 0;
					ReplaceByMov32(op, v);
				}
				else if (op.op == shop_neg)
				{
					u32 v = constprop_values[dest_reg]
								   = -op.rs1.imm_value();
					ReplaceByMov32(op, v);
				}
				else if (op.op == shop_swaplb)
				{
					u32 v = shil_opcl_swaplb::f1::impl(op.rs1.imm_value());
					constprop_values[dest_reg] = v;
					ReplaceByMov32(op, v);
				}
				else if (op.op == shop_seteq || op.op == shop_setgt || op.op == shop_setge || op.op == shop_setab || op.op == shop_setae)
				{
					u32 r1 = op.rs1.imm_value();
					u32 r2 = op.rs2.imm_value();
					u32 rd;
					switch (op.op)
					{
					case shop_seteq:
						rd = r1 == r2;
						break;
					case shop_setge:
						rd = (s32)r1 >= (s32)r2;
						break;
					case shop_setgt:
						rd = (s32)r1 > (s32)r2;
						break;
					case shop_setab:
						rd = r1 > r2;
						break;
					case shop_setae:
						rd = r1 >= r2;
						break;
					}
					constprop_values[dest_reg] = rd;
					ReplaceByMov32(op, rd);
				}
				else if (op.op == shop_jdyn)
				{
					// TODO check this
					verify(BET_GET_CLS(block->BlockType) == BET_CLS_Dynamic);
					switch ((block->BlockType >> 1) & 3)
					{
					case BET_SCL_Jump:
					case BET_SCL_Ret:
						block->BlockType = BET_StaticJump;
						block->BranchBlock = op.rs1.imm_value();
						break;
					case BET_SCL_Call:
						block->BlockType = BET_StaticCall;
						block->BranchBlock = op.rs1.imm_value();
						break;
					case BET_SCL_Intr:
						block->BlockType = BET_StaticIntr;
						block->NextBlock = op.rs1.imm_value();
						break;
					default:
						die("Unexpected block end type\n");
					}
					block->oplist.erase(block->oplist.begin() + opnum);
					opnum--;
					stats.constant_ops_removed++;
					//printf("DEAD\n");
					continue;
				}
				else if (op.op == shop_jcond)
				{
					if (op.rs1.imm_value() != (block->BlockType & 1))
					{
						block->BranchBlock = block->NextBlock;
					}
					block->BlockType = BET_StaticJump;
					block->NextBlock = 0xFFFFFFFF;
					block->has_jcond = false;
					block->oplist.erase(block->oplist.begin() + opnum);
					opnum--;
					stats.constant_ops_removed++;
					//printf("DEAD\n");
					continue;
				}
				else if (op.op == shop_fneg)
				{
					u32 rd = constprop_values[dest_reg] = op.rs1.imm_value() ^ 0x80000000;
					ReplaceByMov32(op, rd);
				}
				else if (op.op == shop_fadd)
				{
					f32 rd = reinterpret_cast<f32&>(op.rs1._imm) + reinterpret_cast<f32&>(op.rs2._imm);
					constprop_values[dest_reg] = reinterpret_cast<u32&>(rd);
					ReplaceByMov32(op, reinterpret_cast<u32&>(rd));
				}
				else if (op.op == shop_fmul)
				{
					f32 rd = reinterpret_cast<f32&>(op.rs1._imm) * reinterpret_cast<f32&>(op.rs2._imm);
					constprop_values[dest_reg] = reinterpret_cast<u32&>(rd);
					ReplaceByMov32(op, reinterpret_cast<u32&>(rd));
				}
				else if (op.op == shop_cvt_i2f_n || op.op == shop_cvt_i2f_z)
				{
					f32 rd = (float)(s32)op.rs1._imm;
					constprop_values[dest_reg] = reinterpret_cast<u32&>(rd);
					ReplaceByMov32(op, reinterpret_cast<u32&>(rd));
				}
				else if (op.op == shop_fsqrt)
				{
					f32 rd = sqrtf(reinterpret_cast<f32&>(op.rs1._imm));
					constprop_values[dest_reg] = reinterpret_cast<u32&>(rd);
					ReplaceByMov32(op, reinterpret_cast<u32&>(rd));
				}
				else
				{
					printf("unhandled constant op %d\n", op.op);
					die("RHHAAA");
				}

				//printf("%s\n", op.dissasm().c_str());
			}
			else if ((op.op == shop_and || op.op == shop_or || op.op == shop_xor || op.op == shop_add || op.op == shop_mul_s16 || op.op == shop_mul_u16
					  || op.op == shop_mul_i32 || op.op == shop_test || op.op == shop_seteq || op.op == shop_fseteq || op.op == shop_fadd || op.op == shop_fmul)
					&& op.rs1.is_imm() && op.rs2.is_reg())
			{
				// swap rs1 and rs2 so that rs1 is never an immediate operand
				shil_param t = op.rs1;
				op.rs1 = op.rs2;
				op.rs2 = t;
			}
		}
	}

	void DeadCodeRemovalPass()
	{
		u32 last_versions[sh4_reg_count];
		std::set<RegValue> uses;

		memset(last_versions, -1, sizeof(last_versions));
		for (int opnum = block->oplist.size() - 1; opnum >= 0; opnum--)
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
				last_versions[reg_old_sr_status] = -1;
				for (int i = reg_r0; i <= reg_r7; i++)
					last_versions[i] = -1;
				for (int i = reg_r0_Bank; i <= reg_r7_Bank; i++)
					last_versions[i] = -1;
				continue;
			}
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
				for (int i = 0; i < op.rd.count(); i++)
				{
					if (last_versions[op.rd._reg + i] == -1)
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
				for (int i = 0; i < op.rd2.count(); i++)
				{
					if (last_versions[op.rd2._reg + i] == -1)
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
			if (dead_code && op.op != shop_readm)	// TODO Can we also remove dead readm?
			{
				//printf("%08x DEAD %s\n", blk->vaddr + op.guest_offs, op.dissasm().c_str());
				block->oplist.erase(block->oplist.begin() + opnum);
				stats.dead_code_ops++;
			}
			else
			{
				//printf("%08x      %s\n", blk->vaddr + op.guest_offs, op.dissasm().c_str());
				if (op.rs1.is_reg())
				{
					for (int i = 0; i < op.rs1.count(); i++)
						uses.insert(RegValue(op.rs1, i));
				}
				if (op.rs2.is_reg())
				{
					for (int i = 0; i < op.rs2.count(); i++)
						uses.insert(RegValue(op.rs2, i));
				}
				if (op.rs3.is_reg())
				{
					for (int i = 0; i < op.rs3.count(); i++)
						uses.insert(RegValue(op.rs3, i));
				}
				if (op.op == shop_mov32 && op.rs1.is_reg())
				{
					RegValue dest_reg(op.rd);
					RegValue src_reg(op.rs1);
					auto it = aliases.find(src_reg);
					if (it != aliases.end())
						// use the final value if the dest is itself aliased
						aliases[dest_reg] = it->second;
					else
						aliases[dest_reg] = src_reg;
				}
			}
		}
	}

	void ConstantExpressionsPass()
	{
		for (int opnum = 0; opnum < block->oplist.size(); opnum++)
		{
			shil_opcode& op = block->oplist[opnum];
			if (op.rs2.is_imm())
			{
				if (op.rs2.imm_value() == 0)
				{
					// a & 0 == 0
					// a * 0 == 0
					if (op.op == shop_and || op.op == shop_mul_i32 || op.op == shop_mul_s16 || op.op == shop_mul_u16
							 || op.op == shop_fmul)
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
					else if (op.op == shop_add || op.op == shop_sub || op.op == shop_or || op.op == shop_xor
							|| op.op == shop_shl || op.op == shop_shr || op.op == shop_sar || op.op == shop_shad || op.op == shop_shld
							|| op.op == shop_fadd || op.op == shop_fsub)
					{
						//printf("%08x IDEN %s\n", block->vaddr + op.guest_offs, op.dissasm().c_str());
						if (op.rd._reg == op.rs1._reg)
						{
							block->oplist.erase(block->oplist.begin() + opnum);
							opnum--;
							stats.constant_ops_removed++;
						}
						else
						{
							ReplaceByMov32(op);
						}
						continue;
					}
				}
				// a * 1 == a
				else if (op.rs2.imm_value() == 1
						&& (op.op == shop_mul_i32 || op.op == shop_mul_s16 || op.op == shop_mul_u16))
				{
					//printf("%08x IDEN %s\n", block->vaddr + op.guest_offs, op.dissasm().c_str());
					if (op.rd._reg == op.rs1._reg)
					{
						block->oplist.erase(block->oplist.begin() + opnum);
						opnum--;
						stats.constant_ops_removed++;
					}
					else
					{
						ReplaceByMov32(op);
					}
					continue;
				}
				// TODO very rare
				// a * 1.0 == a
				// a / 1.0 == a
				else if (op.rs2.imm_value() == 0x3f800000	// 1.0
						&& (op.op == shop_fmul || op.op == shop_fdiv))
				{
					//printf("%08x IDEN %s\n", block->vaddr + op.guest_offs, op.dissasm().c_str());
					if (op.rd._reg == op.rs1._reg)
					{
						block->oplist.erase(block->oplist.begin() + opnum);
						opnum--;
						stats.constant_ops_removed++;
					}
					else
					{
						ReplaceByMov32(op);
					}
					continue;
				}
			}
			// Not sure it's worth the trouble, except for the xor perhaps
			else if (op.rs1.is_r32i() && op.rs1._reg == op.rs2._reg)
			{
				// a ^ a == 0
				if (op.op == shop_xor)
				{
					//printf("%08x ZERO %s\n", block->vaddr + op.guest_offs, op.dissasm().c_str());
					ReplaceByMov32(op, 0);
				}
				// a & a == a
				// a | a == a
				else if (op.op == shop_and || op.op == shop_or)
				{
					//printf("%08x IDEN %s\n", block->vaddr + op.guest_offs, op.dissasm().c_str());
					if (op.rd._reg == op.rs1._reg)
					{
						block->oplist.erase(block->oplist.begin() + opnum);
						opnum--;
						stats.constant_ops_removed++;
					}
					else
					{
						ReplaceByMov32(op);
					}
				}
			}
		}
	}

	void DeadRegisterPass()
	{
		for (auto alias : aliases)
		{
			if (writeback_values.count(alias.first) == 0)
			{
				// Do a first pass to check that we can replace the reg
				size_t defnum = -1;
				size_t usenum = -1;
				size_t aliasdef = -1;
				for (int opnum = 0; opnum < block->oplist.size(); opnum++)
				{
					shil_opcode* op = &block->oplist[opnum];
					if (op->rd.is_r32() && RegValue(op->rd) == alias.first)
						defnum = opnum;
					else if (op->rd2.is_r32() && RegValue(op->rd2) == alias.first)
						defnum = opnum;
					if (defnum == -1)
						continue;
					if (op->rd.is_reg() && alias.second.get_reg() >= op->rd._reg && alias.second.get_reg() < (Sh4RegType)(op->rd._reg + op->rd.count())
							&& op->rd.version[alias.second.get_reg() - op->rd._reg] > alias.second.get_version() && aliasdef == -1)
						aliasdef = opnum;
					else if (op->rd2.is_reg() && alias.second.get_reg() >= op->rd2._reg && alias.second.get_reg() < (Sh4RegType)(op->rd2._reg + op->rd2.count())
							&& op->rd2.version[alias.second.get_reg() - op->rd2._reg] > alias.second.get_version() && aliasdef == -1)
						aliasdef = opnum;
					if (op->rs1.is_r32() && op->rs1._reg == alias.first.get_reg() && op->rs1.version[0] == alias.first.get_version())
						usenum = opnum;
					if (op->rs2.is_r32() && op->rs2._reg == alias.first.get_reg() && op->rs2.version[0] == alias.first.get_version())
						usenum = opnum;
					if (op->rs3.is_r32() && op->rs3._reg == alias.first.get_reg() && op->rs3.version[0] == alias.first.get_version())
						usenum = opnum;
				}
				verify(defnum != -1);
				// If the alias is redefined before any use we can't do it
				if (aliasdef != -1 && usenum != -1 && aliasdef < usenum)
					continue;
				if (alias.first.get_reg() <= reg_r15)
					continue;
//				if (alias.first.get_reg() >= reg_fr_0 && alias.first.get_reg() <= reg_xf_15)
//					continue;
				for (opnum = defnum + 1; opnum <= usenum && usenum != -1; opnum++)
				{
					shil_opcode* op = &block->oplist[opnum];
					if (op->rs1.is_reg() && op->rs1.count() == 1 && op->rs1._reg == alias.first.first)
					{
						op->rs1._reg = alias.second.first;
						op->rs1.version[0] = alias.second.second;
						printf("DeadRegisterAlias rs1 replacing %s.%d by %s.%d\n", name_reg(alias.first.first).c_str(), alias.first.second,
								name_reg(alias.second.first).c_str(), alias.second.second);
					}
					if (op->rs2.is_reg() && op->rs2.count() == 1 && op->rs2._reg == alias.first.first)
					{
						op->rs2._reg = alias.second.first;
						op->rs2.version[0] = alias.second.second;
						printf("DeadRegisterAlias rs2 replacing %s.%d by %s.%d\n", name_reg(alias.first.first).c_str(), alias.first.second,
								name_reg(alias.second.first).c_str(), alias.second.second);
					}
					if (op->rs3.is_reg() && op->rs3.count() == 1 && op->rs3._reg == alias.first.first)
					{
						op->rs3._reg = alias.second.first;
						op->rs3.version[0] = alias.second.second;
						printf("DeadRegisterAlias rs3 replacing %s.%d by %s.%d\n", name_reg(alias.first.first).c_str(), alias.first.second,
								name_reg(alias.second.first).c_str(), alias.second.second);
					}
				}
				stats.dead_registers++;
				block->oplist.erase(block->oplist.begin() + defnum);
			}
		}
	}

	void DoRegAlloc()
	{
		host_gregs.clear();
		for (int i = 0; i < 6; i++)		// FIXME reg count
			host_gregs.push_front(i);
		host_fregs.clear();
		for (int i = 0; i < 4; i++)		// FIXME reg count
			host_fregs.push_front(i);

		printf("BLOCK\n");
		for (opnum = 0; opnum < block->oplist.size(); opnum++)
		{
			shil_opcode* op = &block->oplist[opnum];
			if (op->op == shop_ifb || (mmu_enabled() && (op->op == shop_readm || op->op == shop_writem || op->op == shop_pref)))
				FlushAllWritebacks();
			// Flush regs used by vector ops
			if (op->rs1.count() > 1 || op->rs2.count() > 1 || op->rs3.count() > 1)
			{
				for (int i = 0; i < op->rs1.count(); i++)
				{
					auto reg = reg_alloced.find((Sh4RegType)(op->rs1._reg + i));
					if (reg != reg_alloced.end() && reg->second.version == op->rs1.version[i])
						FlushReg((Sh4RegType)(op->rs1._reg + i), true);
				}
				for (int i = 0; i < op->rs2.count(); i++)
				{
					auto reg = reg_alloced.find((Sh4RegType)(op->rs2._reg + i));
					if (reg != reg_alloced.end() && reg->second.version == op->rs2.version[i])
						FlushReg((Sh4RegType)(op->rs2._reg + i), true);
				}
				for (int i = 0; i < op->rs3.count(); i++)
				{
					auto reg = reg_alloced.find((Sh4RegType)(op->rs3._reg + i));
					if (reg != reg_alloced.end() && reg->second.version == op->rs3.version[i])
						FlushReg((Sh4RegType)(op->rs3._reg + i), true);
				}
			}
			AllocSourceReg(op->rs1);
			AllocSourceReg(op->rs2);
			AllocSourceReg(op->rs3);
			AllocDestReg(op->rd);
			AllocDestReg(op->rd2);
			printf("%08x  %s\n", block->vaddr + op->guest_offs, op->dissasm().c_str());
		}
		FlushAllWritebacks();
	}

	void FlushReg(Sh4RegType reg_num, bool full)
	{
		auto reg = reg_alloced.find(reg_num);
		verify(reg != reg_alloced.end());
		if (reg->second.write_back)
		{
			printf("WB %s.%d\n", name_reg(reg_num).c_str(), reg->second.version);
			reg->second.write_back = false;
		}
		if (full)
		{
			u32 host_reg = reg->second.host_reg;
			if (reg_num >= reg_fr_0 && reg_num <= reg_xf_15)
				host_fregs.push_front(host_reg);
			else
				host_gregs.push_front(host_reg);
			reg_alloced.erase(reg);
		}
	}

	void FlushAllWritebacks()
	{
		for (auto reg : reg_alloced)
		{
			FlushReg(reg.first, false);
		}
	}

	void AllocSourceReg(const shil_param& param)
	{
		if (param.is_reg() && param.count() == 1)	// TODO EXPLODE_SPANS?
		{
			auto it = reg_alloced.find(param._reg);
			if (it == reg_alloced.end())
			{
				u32 host_reg;
				if (param.is_r32i())
				{
					if (host_gregs.empty())
					{
						SpillReg(false, true);
						verify(!host_gregs.empty());
					}
					host_reg = host_gregs.back();
					host_gregs.pop_back();
				}
				else
				{
					if (host_fregs.empty())
					{
						SpillReg(true, true);
						verify(!host_fregs.empty());
					}
					host_reg = host_fregs.back();
					host_fregs.pop_back();
				}
				reg_alloced[param._reg] = { host_reg, param.version[0], false };
				printf("PL %s -> %cx\n", name_reg(param._reg).c_str(), 'a' + host_reg);
			}
		}
	}

	bool NeedsWriteBack(Sh4RegType reg, u32 version)
	{
		u32 last_version = -1;
		for (int i = opnum + 1; i < block->oplist.size(); i++)
		{
			shil_opcode* op = &block->oplist[i];
			// if a subsequent op needs all regs flushed to mem
			if (op->op == shop_ifb || (mmu_enabled() && (op->op == shop_readm || op->op == shop_writem || op->op == shop_pref)))
				return true;
			// reg is used by a subsequent vector op that doesn't use reg allocation
			if (UsesReg(op, reg, version, true))
				return true;
			if (op->rd.is_reg() && reg >= op->rd._reg && reg < (Sh4RegType)(op->rd._reg + op->rd.count()))
				last_version = op->rd.version[reg - op->rd._reg];
			else if (op->rd2.is_reg() && reg >= op->rd2._reg && reg < (Sh4RegType)(op->rd2._reg + op->rd2.count()))
				last_version = op->rd2.version[reg - op->rd2._reg];
		}
		return last_version == -1 || version == last_version;
	}

	void AllocDestReg(const shil_param& param)
	{
		if (param.is_reg() && param.count() == 1)	// TODO EXPLODE_SPANS?
		{
			auto it = reg_alloced.find(param._reg);
			if (it == reg_alloced.end())
			{
				u32 host_reg;
				if (param.is_r32i())
				{
					if (host_gregs.empty())
					{
						SpillReg(false, false);
						verify(!host_gregs.empty());
					}
					host_reg = host_gregs.back();
					host_gregs.pop_back();
				}
				else
				{
					if (host_fregs.empty())
					{
						SpillReg(true, false);
						verify(!host_fregs.empty());
					}
					host_reg = host_fregs.back();
					host_fregs.pop_back();
				}
				reg_alloced[param._reg] = { host_reg, param.version[0], NeedsWriteBack(param._reg, param.version[0]) };
				printf("   %s -> %cx\n", name_reg(param._reg).c_str(), 'a' + host_reg);
			}
			else
			{
				reg_alloc& reg = reg_alloced[param._reg];
				reg.write_back = NeedsWriteBack(param._reg, param.version[0]);
				reg.version = param.version[0];
			}
		}
	}

	void SpillReg(bool freg, bool source)
	{
		Sh4RegType not_used_reg = NoReg;
		Sh4RegType best_reg = NoReg;
		int best_first_use = -1;

		for (auto reg : reg_alloced)
		{
			if ((reg.first >= reg_fr_0 && reg.first <= reg_xf_15) != freg)
				continue;

			// Find the first use, but prefer no write back
			int first_use = -1;
			for (int i = opnum + (source ? 0 : 1); i < block->oplist.size() /* && (first_use == -1 || write_back) */; i++)
			{
				shil_opcode* op = &block->oplist[i];
				if (UsesReg(op, reg.first, reg.second.version))
				{
					first_use = i;
					break;
				}
			}
			if (first_use == -1)
			{
				not_used_reg = reg.first;
				break;
			}
			if (first_use > best_first_use && first_use > opnum)
			{
				best_first_use = first_use;
				best_reg = reg.first;
			}
		}
		Sh4RegType spilled_reg;
		if (not_used_reg != NoReg)
			spilled_reg = not_used_reg;
		else
		{
			printf("RegAlloc: non optimal alloc? reg %s used in op %d\n", name_reg(best_reg).c_str(), best_first_use);
			spilled_reg = best_reg;
		}
		verify(spilled_reg != NoReg);

		if (reg_alloced[spilled_reg].write_back)
		{
			printf("WB %s.%d\n", name_reg(spilled_reg).c_str(), reg_alloced[spilled_reg].version);
		}
		u32 host_reg = reg_alloced[spilled_reg].host_reg;
		reg_alloced.erase(spilled_reg);
		if (freg)
			host_fregs.push_front(host_reg);
		else
			host_gregs.push_front(host_reg);
	}

	bool UsesReg(shil_opcode* op, Sh4RegType reg, u32 version, bool vector = false)
	{
		if (op->rs1.is_reg() && reg >= op->rs1._reg && reg < (Sh4RegType)(op->rs1._reg + op->rs1.count())
				&& version == op->rs1.version[reg - op->rs1._reg]
				&& (!vector || op->rs1.count() > 1))
			return true;
		if (op->rs2.is_reg() && reg >= op->rs2._reg && reg < (Sh4RegType)(op->rs2._reg + op->rs2.count())
				&& version == op->rs2.version[reg - op->rs2._reg]
				&& (!vector || op->rs2.count() > 1))
			return true;
		if (op->rs3.is_reg() && reg >= op->rs3._reg && reg < (Sh4RegType)(op->rs3._reg + op->rs3.count())
				&& version == op->rs3.version[reg - op->rs3._reg]
				&& (!vector || op->rs3.count() > 1))
			return true;
		return false;
	}

	RuntimeBlockInfo* block;
	std::set<RegValue> writeback_values;
	std::map<RegValue, RegValue> aliases;		// (dest reg, version) -> (source reg, version)

	struct {
		u32 prop_constants = 0;
		u32 constant_ops_replaced = 0;
		u32 constant_ops_removed = 0;
		u32 dead_code_ops = 0;
		u32 dead_registers = 0;
	} stats;
	// transient vars
	int opnum = 0;
	// add version pass
	u32 reg_versions[sh4_reg_count];
	// const prop pass
	std::map<RegValue, u32> constprop_values;	// (reg num, version) -> value
	// reg alloc
	deque<u32> host_gregs;
	deque<u32> host_fregs;
	std::map<Sh4RegType, reg_alloc> reg_alloced;
};
