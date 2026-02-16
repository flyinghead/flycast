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
#pragma once
#include <cstdio>
#include <set>
#include <map>
#include "types.h"
#include "shil.h"
#include "blockmanager.h"

class SSAOptimizer
{
public:
	SSAOptimizer(RuntimeBlockInfo* blk) : block(blk) {}

	void Optimize();
	void AddVersionPass();

private:
	// References a specific version of a register value
	class RegValue : public std::pair<Sh4RegType, u32>
	{
	public:
		RegValue(const shil_param& param, int index = 0)
			: std::pair<Sh4RegType, u32>((Sh4RegType)(param._reg + index), param.version[index])
		{
			verify(param.is_reg());
			verify(index >= 0 && index < (int)param.count());
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
				for (u32 i = 0; i < param.count(); i++)
					reg_versions[param._reg + i]++;
			}
			for (u32 i = 0; i < param.count(); i++)
				param.version[i] = reg_versions[param._reg + i];
		}
	}

	// mov rd, #v
	void ReplaceByMov32(shil_opcode& op, u32 v)
	{
		verify(op.rd2.is_null());
		op.op = shop_mov32;
		op.rs1 = shil_param(v);
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
	void ConstPropPass();
	void DeadCodeRemovalPass();
	void SimplifyExpressionPass();

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

	void DeadRegisterPass();
	void IdentityMovePass();
	void CombineShiftsPass();
	void WriteAfterWritePass();
	bool skipSingleBranchTarget(u32& addr, bool updateCycles);

	void SingleBranchTargetPass()
	{
		if (block->read_only)
		{
			bool updateCycles = !skipSingleBranchTarget(block->BranchBlock, true);
			skipSingleBranchTarget(block->NextBlock, updateCycles);
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
