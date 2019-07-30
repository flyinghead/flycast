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
#include "hw/sh4/sh4_mem.h"

class SSAOptimizer
{
public:
	SSAOptimizer(RuntimeBlockInfo* blk) : block(blk) {}

	void Optimize()
	{
		AddVersionPass();
#if DEBUG
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

#if DEBUG
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
		RegValue(Sh4RegType reg, u32 version)
			: std::pair<Sh4RegType, u32>(reg, version) { }
		RegValue() : std::pair<Sh4RegType, u32>() { }

		Sh4RegType get_reg() const { return first; }
		u32 get_version() const { return second; }
	};

	void PrintBlock()
	{
		for (const shil_opcode& op : block->oplist)
		{
			INFO_LOG(DYNAREC, "%08x  %s", block->vaddr + op.guest_offs, op.dissasm().c_str());
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

	// mov rd, #v
	void ReplaceByMov32(shil_opcode& op, u32 v)
	{
		verify(op.rd2.is_null());
		op.op = shop_mov32;
		op.rs1 = shil_param(FMT_IMM, v);
		op.rs2.type = FMT_NULL;
		op.rs3.type = FMT_NULL;
		stats.constant_ops_replaced++;
	}

	// mov rd, rs1
	void ReplaceByMov32(shil_opcode& op)
	{
		verify(op.rd2.is_null());
		op.op = shop_mov32;
		op.rs2.type = FMT_NULL;
		op.rs3.type = FMT_NULL;
	}

	// mov rd, rs
	void InsertMov32Op(const shil_param& rd, const shil_param& rs);

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

	bool ExecuteConstOp(shil_opcode* op);

	void ConstPropPass()
	{
		for (opnum = 0; opnum < block->oplist.size(); opnum++)
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
					if (reg == reg_sr_status || reg == reg_old_sr_status || (reg >= reg_r0 && reg <= reg_r7)
							|| (reg >= reg_r0_Bank && reg <= reg_r7_Bank))
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

					// If we know the address to read and it's in the same memory page(s) as the block
					// and if those pages are read-only, then we can directly read the memory at compile time
					// and propagate the read value as a constant.
					if (op.rs1.is_imm() && op.op == shop_readm  && block->read_only
							&& (op.rs1._imm >> 12) >= (block->vaddr >> 12)
							&& (op.rs1._imm >> 12) <= ((block->vaddr + block->sh4_code_size - 1) >> 12)
							&& (op.flags & 0x7f) <= 4)
					{
						bool doit = false;
						if (mmu_enabled())
						{
							// Only for user space addresses
							if ((op.rs1._imm & 0x80000000) == 0)
								doit = true;
							else
								// And kernel RAM addresses
								doit = IsOnRam(op.rs1._imm);
						}
						else
							doit = IsOnRam(op.rs1._imm);
						if (doit)
						{
							u32 v;
							switch (op.flags & 0x7f)
							{
							case 1:
								v = (s32)(::s8)ReadMem8(op.rs1._imm);
								break;
							case 2:
								v = (s32)(::s16)ReadMem16(op.rs1._imm);
								break;
							case 4:
								v = ReadMem32(op.rs1._imm);
								break;
							default:
								die("invalid size");
								break;
							}
							ReplaceByMov32(op, v);
							constprop_values[RegValue(op.rd)] = v;
						}
					}
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
				if ((r2 & 0x80000000) == 0)
				{
					// rd = r1 << (r2 & 0x1F)
					op.op = shop_shl;
					op.rs2._imm = r2 & 0x1F;
					stats.constant_ops_replaced++;
				}
				else if ((r2 & 0x1F) == 0)
				{
					if (op.op == shop_shl)
						// rd = 0
						ReplaceByMov32(op, 0);
					else
					{
						// rd = r1 >> 31;
						op.op = shop_sar;
						op.rs2._imm = 31;
						stats.constant_ops_replaced++;
					}
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
			}
		}
	}

	void SimplifyExpressionPass()
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
					op.rs2 = shil_param(FMT_IMM, 1);
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
					op.rs1 = shil_param(FMT_IMM, 0);
					op.rs2 = shil_param(FMT_IMM, 0);
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

	bool DefinesHigherVersion(const shil_param& param, RegValue reg_ver)
	{
		return param.is_reg()
				&& reg_ver.get_reg() >= param._reg
				&& reg_ver.get_reg() < (Sh4RegType)(param._reg + param.count())
				&& param.version[reg_ver.get_reg() - param._reg] > reg_ver.get_version();
	}

	bool UsesRegValue(const shil_param& param, RegValue reg_ver)
	{
		return param.is_reg()
				&& reg_ver.get_reg() >= param._reg
				&& reg_ver.get_reg() < (Sh4RegType)(param._reg + param.count())
				&& param.version[reg_ver.get_reg() - param._reg] == reg_ver.get_version();
	}

	void ReplaceByAlias(shil_param& param, const RegValue& from, const RegValue& to)
	{
		if (param.is_r32() && param._reg == from.get_reg())
		{
			verify(param.version[0] == from.get_version());
			param._reg = to.get_reg();
			param.version[0] = to.get_version();
			//printf("DeadRegisterPass replacing %s.%d by %s.%d\n", name_reg(from.get_reg()).c_str(), from.get_version(),
			//		name_reg(to.get_reg()).c_str(), to.get_version());
		}
	}

	void DeadRegisterPass()
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
			for (int opnum = 0; opnum < block->oplist.size(); opnum++)
			{
				shil_opcode* op = &block->oplist[opnum];
				// find def
				if (op->rd.is_r32() && RegValue(op->rd) == alias.first)
					defnum = opnum;
				else if (op->rd2.is_r32() && RegValue(op->rd2) == alias.first)
					defnum = opnum;

				// find alias redef
				if (DefinesHigherVersion(op->rd, alias.second) && aliasdef == -1)
					aliasdef = opnum;
				else if (DefinesHigherVersion(op->rd2, alias.second) && aliasdef == -1)
					aliasdef = opnum;

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
			verify(defnum != -1);
			// If the alias is redefined before any use we can't use it
			if (aliasdef != -1 && usenum != -1 && aliasdef < usenum)
				continue;

			for (int opnum = defnum + 1; opnum <= usenum && usenum != -1; opnum++)
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

	void IdentityMovePass()
	{
		// This pass creates holes in reg versions and should be run last
		// The versioning pass must be re-run if needed
		for (int opnum = 0; opnum < block->oplist.size(); opnum++)
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

	void CombineShiftsPass()
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

	void WriteAfterWritePass()
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

	RuntimeBlockInfo* block;
	std::set<RegValue> writeback_values;

	struct {
		u32 prop_constants = 0;
		u32 constant_ops_replaced = 0;
		u32 dead_code_ops = 0;
		u32 dead_registers = 0;
		u32 dyn_to_stat_blocks = 0;
		u32 waw_blocks = 0;
		u32 combined_shifts = 0;
	} stats;

	// transient vars
	// add version pass
	u32 reg_versions[sh4_reg_count];
	// const prop pass
	std::map<RegValue, u32> constprop_values;	// (reg num, version) -> value
	int opnum = 0;
};
