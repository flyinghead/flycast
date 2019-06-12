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
#pragma once
#include <map>
#include <deque>
#include "types.h"
#include "decoder.h"
#include "hw/sh4/modules/mmu.h"
#include "ssa.h"

#define ssa_printf(...)

template<typename nreg_t, typename nregf_t, bool explode_spans = true>
class RegAlloc
{
public:
	RegAlloc() {}
	virtual ~RegAlloc() {}

	void DoAlloc(RuntimeBlockInfo* block, const nreg_t* regs_avail, const nregf_t* regsf_avail)
	{
		this->block = block;
		SSAOptimizer optim(block);
		optim.AddVersionPass();

		verify(host_gregs.empty());
		while (*regs_avail != (nreg_t)-1)
			host_gregs.push_back(*regs_avail++);

		verify(host_fregs.empty());
		while (*regsf_avail != (nregf_t)-1)
			host_fregs.push_back(*regsf_avail++);
	}

	void OpBegin(shil_opcode* op, int opid)
	{
		opnum = opid;
		if (op->op == shop_ifb)
		{
			FlushAllRegs(true);
		}
		else if (mmu_enabled() && (op->op == shop_readm || op->op == shop_writem || op->op == shop_pref))
		{
			FlushAllRegs(false);
		}
		else if (op->op == shop_sync_sr)
		{
			//FlushReg(reg_sr_T, true);
			FlushReg(reg_sr_status, true);
			FlushReg(reg_old_sr_status, true);
			for (int i = reg_r0; i <= reg_r7; i++)
				FlushReg((Sh4RegType)i, true);
			for (int i = reg_r0_Bank; i <= reg_r7_Bank; i++)
				FlushReg((Sh4RegType)i, true);
		}
		else if (op->op == shop_sync_fpscr)
		{
			FlushReg(reg_fpscr, true);
			FlushReg(reg_old_fpscr, true);
			for (int i = reg_fr_0; i <= reg_xf_15; i++)
				FlushReg((Sh4RegType)i, true);
		}
		// Flush regs used by vector ops
		if (op->rs1.is_reg() && op->rs1.count() > 1)
		{
			for (int i = 0; i < op->rs1.count(); i++)
				FlushReg((Sh4RegType)(op->rs1._reg + i), false);
		}
		if (op->rs2.is_reg() && op->rs2.count() > 1)
		{
			for (int i = 0; i < op->rs2.count(); i++)
				FlushReg((Sh4RegType)(op->rs2._reg + i), false);
		}
		if (op->rs3.is_reg() && op->rs3.count() > 1)
		{
			for (int i = 0; i < op->rs3.count(); i++)
				FlushReg((Sh4RegType)(op->rs3._reg + i), false);
		}
		if (op->op != shop_ifb)
		{
			AllocSourceReg(op->rs1);
			AllocSourceReg(op->rs2);
			AllocSourceReg(op->rs3);
			// Hard flush vector ops destination regs
			// Note that this is incorrect if a reg is both src (scalar) and dest (vec). However such an op doesn't exist.
			if (op->rd.is_reg() && op->rd.count() > 1)
			{
				for (int i = 0; i < op->rd.count(); i++)
				{
					verify(reg_alloced.count((Sh4RegType)(op->rd._reg + i)) == 0 || !reg_alloced[(Sh4RegType)(op->rd._reg + i)].write_back);
					FlushReg((Sh4RegType)(op->rd._reg + i), true);
				}
			}
			if (op->rd2.is_reg() && op->rd2.count() > 1)
			{
				for (int i = 0; i < op->rd2.count(); i++)
				{
					verify(reg_alloced.count((Sh4RegType)(op->rd2._reg + i)) == 0 || !reg_alloced[(Sh4RegType)(op->rd2._reg + i)].write_back);
					FlushReg((Sh4RegType)(op->rd2._reg + i), true);
				}
			}
			AllocDestReg(op->rd);
			AllocDestReg(op->rd2);
		}
		ssa_printf("%08x  %s gregs %ld fregs %ld\n", block->vaddr + op->guest_offs, op->dissasm().c_str(), host_gregs.size(), host_fregs.size());
	}

	void OpEnd(shil_opcode* op)
	{
		for (Sh4RegType reg : pending_flushes)
		{
			verify(!reg_alloced[reg].write_back);
			reg_alloced.erase(reg);
		}
		pending_flushes.clear();

		// Flush normally
		for (auto const& reg : reg_alloced)
		{
			FlushReg(reg.first, false);
		}

		// Hard flush all dirty regs. Useful for troubleshooting
//		while (!reg_alloced.empty())
//		{
//			auto it = reg_alloced.begin();
//
//			if (it->second.dirty)
//				it->second.write_back = true;
//			FlushReg(it->first, true);
//		}

		// Final writebacks
		if (op >= &block->oplist.back())
		{
			FlushAllRegs(false);
			final_opend = true;
		}
	}

	void SetOpnum(int num)
	{
		fast_forwarding = true;
		for (int i = 0; i < num; i++)
		{
			shil_opcode* op = &block->oplist[i];
			OpBegin(op, i);
			OpEnd(op);
		}
		OpBegin(&block->oplist[num], num);
		fast_forwarding = false;
	}

	bool IsAllocAny(const shil_param& prm)
	{
		if (prm.is_reg())
		{
			bool rv = IsAllocAny(prm._reg);
			if (prm.count() != 1)
			{
				for (u32 i = 1;i < prm.count(); i++)
					verify(IsAllocAny((Sh4RegType)(prm._reg + i)) == rv);
			}
			return rv;
		}
		else
		{
			return false;
		}
	}

	bool IsAllocg(const shil_param& prm)
	{
		if (prm.is_reg())
		{
			verify(prm.count() == 1);
			return IsAllocg(prm._reg);
		}
		else
		{
			return false;
		}
	}

	bool IsAllocf(const shil_param& prm)
	{
		if (prm.is_reg())
		{
			verify(prm.count() == 1);
			return IsAllocf(prm._reg);
		}
		else
		{
			return false;
		}
	}

	nreg_t mapg(const shil_param& prm)
	{
		verify(IsAllocg(prm));
		verify(prm.count() == 1);
		return mapg(prm._reg);
	}

	nregf_t mapf(const shil_param& prm)
	{
		verify(IsAllocf(prm));
		verify(prm.count() == 1);
		return mapf(prm._reg);
	}

	bool reg_used(nreg_t host_reg)
	{
		for (auto const& reg : reg_alloced)
			if ((nreg_t)reg.second.host_reg == host_reg && !IsFloat(reg.first))
				return true;
		return false;
	}

	bool regf_used(nregf_t host_reg)
	{
		for (auto const& reg : reg_alloced)
			if ((nregf_t)reg.second.host_reg == host_reg && IsFloat(reg.first))
				return true;
		return false;
	}

	void Cleanup() {
		verify(final_opend || block->oplist.size() == 0);
		final_opend = false;
		FlushAllRegs(true);
		verify(reg_alloced.empty());
		verify(pending_flushes.empty());
		block = NULL;
		host_fregs.clear();
		host_gregs.clear();
	}

	virtual void Preload(u32 reg, nreg_t nreg) = 0;
	virtual void Writeback(u32 reg, nreg_t nreg) = 0;

	virtual void Preload_FPU(u32 reg, nregf_t nreg) = 0;
	virtual void Writeback_FPU(u32 reg, nregf_t nreg) = 0;

private:
	struct reg_alloc {
		u32 host_reg;
		u16 version;
		bool write_back;
		bool dirty;
	};

	bool IsFloat(Sh4RegType reg)
	{
		return reg >= reg_fr_0 && reg <= reg_xf_15;
	}

	nreg_t mapg(Sh4RegType reg)
	{
		verify(reg_alloced.count(reg));
		return (nreg_t)reg_alloced[reg].host_reg;
	}

	nregf_t mapf(Sh4RegType reg)
	{
		verify(reg_alloced.count(reg));
		return (nregf_t)reg_alloced[reg].host_reg;
	}

	bool IsAllocf(Sh4RegType reg)
	{
		if (!IsFloat(reg))
			return false;
		return reg_alloced.find(reg) != reg_alloced.end();
	}

	bool IsAllocg(Sh4RegType reg)
	{
		if (IsFloat(reg))
			return false;
		return reg_alloced.find(reg) != reg_alloced.end();
	}

	bool IsAllocAny(Sh4RegType reg)
	{
		return IsAllocg(reg) || IsAllocf(reg);
	}

	void WriteBackReg(Sh4RegType reg_num, struct reg_alloc& reg_alloc)
	{
		if (reg_alloc.write_back)
		{
			if (!fast_forwarding)
			{
				ssa_printf("WB %s.%d <- %cx\n", name_reg(reg_num).c_str(), reg_alloc.version, 'a' + reg_alloc.host_reg);
				if (IsFloat(reg_num))
					Writeback_FPU(reg_num, (nregf_t)reg_alloc.host_reg);
				else
					Writeback(reg_num, (nreg_t)reg_alloc.host_reg);
			}
			reg_alloc.write_back = false;
			reg_alloc.dirty = false;
		}
	}

	void FlushReg(Sh4RegType reg_num, bool hard)
	{
		auto reg = reg_alloced.find(reg_num);
		if (reg != reg_alloced.end())
		{
			WriteBackReg(reg->first, reg->second);
			if (hard)
			{
				u32 host_reg = reg->second.host_reg;
				reg_alloced.erase(reg);
				if (IsFloat(reg_num))
					host_fregs.push_front((nregf_t)host_reg);
				else
					host_gregs.push_front((nreg_t)host_reg);
			}
		}
	}

	void FlushAllRegs(bool hard)
	{
		if (hard)
		{
			while (!reg_alloced.empty())
				FlushReg(reg_alloced.begin()->first, true);
		}
		else
		{
			for (auto const& reg : reg_alloced)
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
				reg_alloced[param._reg] = { host_reg, param.version[0], false, false };
				if (!fast_forwarding)
				{
					ssa_printf("PL %s.%d -> %cx\n", name_reg(param._reg).c_str(), param.version[0], 'a' + host_reg);
					if (IsFloat(param._reg))
						Preload_FPU(param._reg, (nregf_t)host_reg);
					else
						Preload(param._reg, (nreg_t)host_reg);
				}
			}
		}
	}

	bool NeedsWriteBack(Sh4RegType reg, u32 version)
	{
		for (int i = opnum + 1; i < block->oplist.size(); i++)
		{
			shil_opcode* op = &block->oplist[i];
			// if a subsequent op needs all or some regs flushed to mem
			// TODO we could look at the ifb op to optimize what to flush
			if (op->op == shop_ifb || (mmu_enabled() && (op->op == shop_readm || op->op == shop_writem || op->op == shop_pref)))
				return true;
			if (op->op == shop_sync_sr && (/*reg == reg_sr_T ||*/ reg == reg_sr_status || reg == reg_old_sr_status || (reg >= reg_r0 && reg <= reg_r7)
					|| (reg >= reg_r0_Bank && reg <= reg_r7_Bank)))
				return true;
			if (op->op == shop_sync_fpscr && (reg == reg_fpscr || reg == reg_old_fpscr || (reg >= reg_fr_0 && reg <= reg_xf_15)))
				return true;
			// if reg is used by a subsequent vector op that doesn't use reg allocation
			if (UsesReg(op, reg, version, true))
				return true;
			// no writeback needed if redefined
			if (DefsReg(op, reg, true))
				return false;
			if (DefsReg(op, reg, false))
				return false;
		}

		return true;
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
				reg_alloced[param._reg] = { host_reg, param.version[0], NeedsWriteBack(param._reg, param.version[0]), true };
				ssa_printf("   %s.%d -> %cx %s\n", name_reg(param._reg).c_str(), param.version[0], 'a' + host_reg, reg_alloced[param._reg].write_back ? "(wb)" : "");
			}
			else
			{
				reg_alloc& reg = reg_alloced[param._reg];
				verify(!reg.write_back);
				reg.write_back = NeedsWriteBack(param._reg, param.version[0]);
				reg.dirty = true;
				reg.version = param.version[0];
			}
			verify(reg_alloced[param._reg].dirty);
		}
	}

	void SpillReg(bool freg, bool source)
	{
		Sh4RegType spilled_reg = Sh4RegType::NoReg;
		int latest_use = -1;

		for (auto const& reg : reg_alloced)
		{
			if (IsFloat(reg.first) != freg)
				continue;
			// Don't spill already spilled regs
			bool pending = false;
			for (auto& pending_reg : pending_flushes)
				if (pending_reg == reg.first)
				{
					pending = true;
					break;
				}
			if (pending)
				continue;

			// Don't spill current op scalar dest regs
			shil_opcode* op = &block->oplist[opnum];
			if (DefsReg(op, reg.first, false))
				continue;

			// Find the first use, but ignore vec ops
			int first_use = -1;
			for (int i = opnum + (source ? 0 : 1); i < block->oplist.size(); i++)
			{
				op = &block->oplist[i];
				// Vector ops don't use reg alloc
				if (UsesReg(op, reg.first, reg.second.version, false))
				{
					first_use = i;
					break;
				}
			}
			if (first_use == -1)
			{
				latest_use = -1;
				spilled_reg = reg.first;
				break;
			}
			if (first_use > latest_use && first_use > opnum)
			{
				latest_use = first_use;
				spilled_reg = reg.first;
			}
		}
		if (latest_use != -1)
		{
			ssa_printf("RegAlloc: non optimal alloc? reg %s used in op %d\n", name_reg(spilled_reg).c_str(), latest_use);
			spills++;
			// need to write-back if dirty so reload works
			if (reg_alloced[spilled_reg].dirty)
				reg_alloced[spilled_reg].write_back = true;
		}
		verify(spilled_reg != Sh4RegType::NoReg);

		if (source)
			FlushReg(spilled_reg, true);
		else
		{
			// Delay the eviction of spilled regs from the map due to dest register allocation.
			// It's possible that the same host reg is allocated to a source operand
			// and to the (future) dest operand. In this case we want to keep both mappings
			// until the current op is done.
			WriteBackReg(spilled_reg, reg_alloced[spilled_reg]);
			u32 host_reg = reg_alloced[spilled_reg].host_reg;
			if (IsFloat(spilled_reg))
				host_fregs.push_front((nregf_t)host_reg);
			else
				host_gregs.push_front((nreg_t)host_reg);
			pending_flushes.push_back(spilled_reg);
		}
	}

	bool IsVectorOp(shil_opcode* op)
	{
		return op->rs1.count() > 1 || op->rs2.count() > 1 || op->rs3.count() > 1 || op->rd.count() > 1 || op->rd2.count() > 1;
	}

	bool UsesReg(shil_opcode* op, Sh4RegType reg, u32 version, bool vector)
	{
		if (op->rs1.is_reg() && reg >= op->rs1._reg && reg < (Sh4RegType)(op->rs1._reg + op->rs1.count())
				&& version == op->rs1.version[reg - op->rs1._reg]
				&& vector == (op->rs1.count() > 1))
			return true;
		if (op->rs2.is_reg() && reg >= op->rs2._reg && reg < (Sh4RegType)(op->rs2._reg + op->rs2.count())
				&& version == op->rs2.version[reg - op->rs2._reg]
				&& vector == (op->rs2.count() > 1))
			return true;
		if (op->rs3.is_reg() && reg >= op->rs3._reg && reg < (Sh4RegType)(op->rs3._reg + op->rs3.count())
				&& version == op->rs3.version[reg - op->rs3._reg]
				&& vector == (op->rs3.count() > 1))
			return true;

		return false;
	}

	bool DefsReg(shil_opcode* op, Sh4RegType reg, bool vector)
	{
		if (op->rd.is_reg() && reg >= op->rd._reg && reg < (Sh4RegType)(op->rd._reg + op->rd.count())
				&& vector == (op->rd.count() > 1))
			return true;
		if (op->rd2.is_reg() && reg >= op->rd2._reg && reg < (Sh4RegType)(op->rd2._reg + op->rd2.count())
				&& vector == (op->rd2.count() > 1))
			return true;
		return false;
	}

	RuntimeBlockInfo* block = NULL;
	deque<nreg_t> host_gregs;
	deque<nregf_t> host_fregs;
	vector<Sh4RegType> pending_flushes;
	std::map<Sh4RegType, reg_alloc> reg_alloced;
	int opnum = 0;

	bool final_opend = false;
	bool fast_forwarding = false;
public:
	u32 spills = 0;
};

#undef printf
