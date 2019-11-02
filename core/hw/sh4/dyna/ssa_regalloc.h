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

#define ssa_printf(...) DEBUG_LOG(DYNAREC, __VA_ARGS__)

template<typename nreg_t, typename nregf_t, bool _64bits = true>
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
		// TODO dup code with NeedsWriteBack
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
		if (IsVector(op->rs1))
		{
			for (int i = 0; i < op->rs1.count(); i++)
				FlushReg((Sh4RegType)(op->rs1._reg + i), false);
		}
		if (IsVector(op->rs2))
		{
			for (int i = 0; i < op->rs2.count(); i++)
				FlushReg((Sh4RegType)(op->rs2._reg + i), false);
		}
		if (IsVector(op->rs3))
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
			if (IsVector(op->rd))
			{
				for (int i = 0; i < op->rd.count(); i++)
				{
					verify(reg_alloced.count((Sh4RegType)(op->rd._reg + i)) == 0 || !reg_alloced[(Sh4RegType)(op->rd._reg + i)].write_back);
					FlushReg((Sh4RegType)(op->rd._reg + i), true);
				}
			}
			if (IsVector(op->rd2))
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
		ssa_printf("%08x  %s gregs %zd fregs %zd", block->vaddr + op->guest_offs, op->dissasm().c_str(), host_gregs.size(), host_fregs.size());
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
		return IsAllocg(prm) || IsAllocf(prm);
	}

	bool IsAllocg(const shil_param& prm)
	{
		if (prm.is_reg() && IsAllocg(prm._reg))
		{
			verify(prm.count() == 1);
			return true;
		}
		return false;
	}

	bool IsAllocf(const shil_param& prm)
	{
		if (prm.is_reg())
		{
			if (!_64bits && prm.is_r64f())
				return false;
			return IsAllocf(prm._reg, prm.count());
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
		if (_64bits)
			verify(prm.count() <= 2);
		else
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

	virtual void Preload_FPU(u32 reg, nregf_t nreg, bool _64bit) = 0;
	virtual void Writeback_FPU(u32 reg, nregf_t nreg, bool _64bit) = 0;
	// merge reg1 (least significant 32 bits) and reg2 (most significant 32 bits) into reg1 (64-bit result)
	virtual void Merge_FPU(nregf_t reg1, nregf_t reg2) { die("not implemented"); }

private:
	struct reg_alloc {
		u32 host_reg;
		u16 version[2];
		bool write_back;
		bool dirty;
		bool _64bit;
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

	bool IsAllocf(Sh4RegType reg, int size)
	{
		if (!IsFloat(reg))
			return false;
		auto it = reg_alloced.find(reg);
		if (it == reg_alloced.end())
			return false;
		verify(it->second._64bit == (size == 2));
		return true;
	}

	bool IsAllocg(Sh4RegType reg)
	{
		if (IsFloat(reg))
			return false;
		return reg_alloced.find(reg) != reg_alloced.end();
	}

	bool IsVector(const shil_param& param)
	{
		return param.is_reg() && param.count() > (_64bits ? 2 : 1);
	}

	bool ContainsReg(const shil_param& param, Sh4RegType reg)
	{
		return param.is_reg() && reg >= param._reg && reg < (Sh4RegType)(param._reg + param.count());
	}

	void WriteBackReg(Sh4RegType reg_num, struct reg_alloc& reg_alloc)
	{
		if (reg_alloc.write_back)
		{
			if (!fast_forwarding)
			{
				ssa_printf("WB %s.%d <- %cx", name_reg(reg_num).c_str(), reg_alloc.version[0], 'a' + reg_alloc.host_reg);
				if (IsFloat(reg_num))
					Writeback_FPU(reg_num, (nregf_t)reg_alloc.host_reg, reg_alloc._64bit);
				else
					Writeback(reg_num, (nreg_t)reg_alloc.host_reg);
			}
			reg_alloc.write_back = false;
			reg_alloc.dirty = false;
		}
	}
protected:
	void FlushReg(Sh4RegType reg_num, bool hard, bool write_if_dirty = false)
	{
		auto reg = reg_alloced.find(reg_num);
		if (reg != reg_alloced.end())
		{
			if (write_if_dirty && reg->second.dirty)
				reg->second.write_back = true;
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

private:
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
		if (param.is_reg()
				&& ((_64bits && param.count() <= 2)	|| (!_64bits && param.count() == 1)))
		{
			Handle64bitRegisters(param, true);

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
				if (param.is_r64f())
					reg_alloced[param._reg] = { host_reg, { param.version[0], param.version[1] }, false, false, true };
				else
					reg_alloced[param._reg] = { host_reg, { param.version[0] }, false, false, false };
				if (!fast_forwarding)
				{
					ssa_printf("PL %s.%d -> %cx", name_reg(param._reg).c_str(), param.version[0], 'a' + host_reg);
					if (IsFloat(param._reg))
						Preload_FPU(param._reg, (nregf_t)host_reg, param.count() == 2);
					else
						Preload(param._reg, (nreg_t)host_reg);
				}
			}
			else
			{
				verify(it->second._64bit == (param.count() == 2));
			}
			verify(param.count() == 1 || reg_alloced.find((Sh4RegType)(param._reg + 1)) == reg_alloced.end());
		}
	}

	bool NeedsWriteBack(Sh4RegType reg, u32 version)
	{
		for (int i = opnum + 1; i < block->oplist.size(); i++)
		{
			shil_opcode* op = &block->oplist[i];
			// if a subsequent op needs all or some regs flushed to mem
			switch (op->op)
			{
			// TODO we could look at the ifb op to optimize what to flush
			case shop_ifb:
				return true;
			case shop_readm:
			case shop_writem:
			case shop_pref:
				if (mmu_enabled())
					return true;
				break;
			case shop_sync_sr:
				if (/*reg == reg_sr_T ||*/ reg == reg_sr_status || reg == reg_old_sr_status || (reg >= reg_r0 && reg <= reg_r7)
						|| (reg >= reg_r0_Bank && reg <= reg_r7_Bank))
					return true;
				break;
			case shop_sync_fpscr:
				if (reg == reg_fpscr || reg == reg_old_fpscr || (reg >= reg_fr_0 && reg <= reg_xf_15))
					return true;
				break;
			default:
				break;
			}
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
		if (param.is_reg()
				&& ((_64bits && param.count() <= 2)	|| (!_64bits && param.count() == 1)))
		{
			Handle64bitRegisters(param, false);

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
				if (param.is_r64f())
					reg_alloced[param._reg] = {
							host_reg,
							{ param.version[0], param.version[1] },
							NeedsWriteBack(param._reg, param.version[0])
								|| NeedsWriteBack((Sh4RegType)(param._reg + 1), param.version[1]),
							true,
							true };
				else
					reg_alloced[param._reg] = {
							host_reg,
							{ param.version[0] },
							NeedsWriteBack(param._reg, param.version[0]),
							true,
							false };
				ssa_printf("   %s.%d -> %cx %s", name_reg(param._reg).c_str(), param.version[0], 'a' + host_reg, reg_alloced[param._reg].write_back ? "(wb)" : "");
			}
			else
			{
				reg_alloc& reg = reg_alloced[param._reg];
				verify(!reg.write_back);
				reg.write_back = NeedsWriteBack(param._reg, param.version[0]);
				reg.dirty = true;
				reg.version[0] = param.version[0];
				verify(reg._64bit == param.is_r64f());
				if (param.is_r64f())
				{
					reg.version[1] = param.version[1];
					// TODO this is handled by Handle64BitsRegisters()
					reg.write_back = reg.write_back || NeedsWriteBack((Sh4RegType)(param._reg + 1), param.version[1]);
				}
			}
			verify(reg_alloced[param._reg].dirty);
			verify(param.count() == 1 || reg_alloced.find((Sh4RegType)(param._reg + 1)) == reg_alloced.end());
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
				if (UsesReg(op, reg.first, reg.second.version[0], false)
						|| (reg.second._64bit && UsesReg(op, (Sh4RegType)(reg.first + 1), reg.second.version[1], false)))
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
			ssa_printf("RegAlloc: non optimal alloc? reg %s used in op %d", name_reg(spilled_reg).c_str(), latest_use);
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
			reg_alloc& alloc = reg_alloced[spilled_reg];
			WriteBackReg(spilled_reg, alloc);
			u32 host_reg = alloc.host_reg;
			if (IsFloat(spilled_reg))
				host_fregs.push_front((nregf_t)host_reg);
			else
				host_gregs.push_front((nreg_t)host_reg);
			pending_flushes.push_back(spilled_reg);
		}
	}

	bool UsesReg(shil_opcode* op, Sh4RegType reg, u32 version, bool vector)
	{
		if (ContainsReg(op->rs1, reg)
				&& version == op->rs1.version[reg - op->rs1._reg]
				&& vector == IsVector(op->rs1))
			return true;
		if (ContainsReg(op->rs2, reg)
				&& version == op->rs2.version[reg - op->rs2._reg]
				&& vector == IsVector(op->rs2))
			return true;
		if (ContainsReg(op->rs3, reg)
				&& version == op->rs3.version[reg - op->rs3._reg]
				&& vector == IsVector(op->rs3))
			return true;

		return false;
	}

	bool DefsReg(shil_opcode* op, Sh4RegType reg, bool vector)
	{
		if (ContainsReg(op->rd, reg) && vector == IsVector(op->rd))
			return true;
		if (ContainsReg(op->rd2, reg) && vector == IsVector(op->rd2))
			return true;
		return false;
	}

	void Handle64bitRegisters(const shil_param& param, bool source)
	{
		if (!(_64bits && (param.is_r32f() || param.is_r64f())))
			return;
		auto it = reg_alloced.find(param._reg);
		if (it != reg_alloced.end() && it->second._64bit != param.is_r64f())
		{
			if (param.is_r64f())
			{
				// Try to merge existing halves
				auto it2 = reg_alloced.find((Sh4RegType)(param._reg + 1));
				if (it2 != reg_alloced.end())
				{
					if (source)
						it->second.dirty = it->second.dirty || it2->second.dirty;
					else
						it->second.dirty = false;
					it->second._64bit = true;
					nregf_t host_reg2 = (nregf_t)it2->second.host_reg;
					reg_alloced.erase(it2);
					Merge_FPU((nregf_t)it->second.host_reg, host_reg2);
					return;
				}
			}
			// Write back the 64-bit register even if used as destination because the other half needs to be saved
			FlushReg(it->first, true, source || it->second._64bit);
		}
		if (param.is_r64f())
		{
			auto it2 = reg_alloced.find((Sh4RegType)(param._reg + 1));
			if (it2 != reg_alloced.end())
				FlushReg(it2->first, true, source);
		}
		else if (param._reg & 1)
		{
			auto it2 = reg_alloced.find((Sh4RegType)(param._reg - 1));
			if (it2 != reg_alloced.end() && it2->second._64bit)
				// Write back even when used as destination because the other half needs to be saved
				FlushReg(it2->first, true, true);
		}
	}

#if 0
	// Currently unused. Doesn't seem to help much
	bool DefsReg(int from, int to, Sh4RegType reg)
	{
		for (int i = from; i <= to; i++)
		{
			shil_opcode* op = &block->oplist[i];
			if (op->rd.is_reg() && reg >= op->rd._reg && reg < (Sh4RegType)(op->rd._reg + op->rd.count()))
				return true;
			if (op->rd2.is_reg() && reg >= op->rd2._reg && reg < (Sh4RegType)(op->rd2._reg + op->rd2.count()))
				return true;
		}
		return false;
	}

	bool EarlyLoad(int cur_op, int early_op)
	{
		shil_opcode* op = &block->oplist[cur_op];
		shil_opcode* next_op = &block->oplist[early_op];
		if (next_op->op == shop_ifb || next_op->op == shop_sync_sr || next_op->op == shop_sync_fpscr)
			return false;
		if (next_op->rs1.is_r32() && !DefsReg(cur_op, early_op - 1, next_op->rs1._reg))
		{
			if ((next_op->rs1.is_r32i() && !host_gregs.empty())
					|| (next_op->rs1.is_r32f() && !host_fregs.empty()))
			{
				ssa_printf("Early loading (+%d) rs1: %s", early_op - cur_op, name_reg(next_op->rs1._reg).c_str());
				AllocSourceReg(next_op->rs1);
			}
		}
		if (next_op->rs2.is_r32() && !DefsReg(cur_op, early_op - 1, next_op->rs2._reg))
		{
			if ((next_op->rs2.is_r32i() && !host_gregs.empty())
					|| (next_op->rs2.is_r32f() && !host_fregs.empty()))
			{
				ssa_printf("Early loading (+%d) rs2: %s", early_op - cur_op, name_reg(next_op->rs1._reg).c_str());
				AllocSourceReg(next_op->rs2);
			}
		}
		if (next_op->rs3.is_r32() && !DefsReg(cur_op, early_op - 1, next_op->rs3._reg))
		{
			if ((next_op->rs3.is_r32i() && !host_gregs.empty())
					|| (next_op->rs3.is_r32f() && !host_fregs.empty()))
			{
				ssa_printf("Early loading (+%d) rs3: %s", early_op - cur_op, name_reg(next_op->rs1._reg).c_str());
				AllocSourceReg(next_op->rs3);
			}
		}
		return true;
	}
#endif

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
