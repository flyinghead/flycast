#include "types.h"
#include <unordered_set>

#include "hw/sh4/sh4_interpreter.h"
#include "hw/sh4/sh4_core.h"
#include "hw/sh4/sh4_interrupts.h"

#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/modules/mmu.h"

#include <ctime>

#include "blockmanager.h"
#include "ngen.h"
#include "decoder.h"
#include "oslib/virtmem.h"

#if FEAT_SHREC != DYNAREC_NONE

constexpr u32 CODE_SIZE = 10_MB;
constexpr u32 TEMP_CODE_SIZE = 1_MB;
constexpr u32 FULL_SIZE = CODE_SIZE + TEMP_CODE_SIZE;

#if defined(_WIN32) || FEAT_SHREC != DYNAREC_JIT || defined(TARGET_IPHONE) || defined(TARGET_ARM_MAC)
static u8 *SH4_TCB;
#else
alignas(4096) static u8 SH4_TCB[FULL_SIZE]
#if defined(__OpenBSD__)
	__attribute__((section(".openbsd.mutable")));
#elif defined(__unix__) || defined(__SWITCH__)
	__attribute__((section(".text")));
#elif defined(__APPLE__)
	__attribute__((section("__TEXT,.text")));
#else
	#error SH4_TCB ALLOC
#endif
#endif

static u8* CodeCache;
static u8* TempCodeCache;
ptrdiff_t cc_rx_offset;

static std::unordered_set<u32> smc_hotspots;

static sh4_if sh4Interp;
static Sh4CodeBuffer codeBuffer;
Sh4Dynarec *sh4Dynarec;

void *Sh4CodeBuffer::get()
{
	return tempBuffer ? &TempCodeCache[tempLastAddr] : &CodeCache[lastAddr];
}

void Sh4CodeBuffer::advance(u32 size)
{
	if (tempBuffer)
		tempLastAddr += size;
	else
		lastAddr += size;
}

u32 Sh4CodeBuffer::getFreeSpace()
{
	if (tempBuffer)
		return TEMP_CODE_SIZE - tempLastAddr;
	else
		return CODE_SIZE - lastAddr;
}

void *Sh4CodeBuffer::getBase()
{
	return CodeCache;
}

u32 Sh4CodeBuffer::getSize()
{
	return FULL_SIZE;
}

void Sh4CodeBuffer::reset(bool temporary)
{
	if (temporary)
		tempLastAddr = 0;
	else
		lastAddr = 0;
}

static void clear_temp_cache(bool full)
{
	//printf("recSh4:Temp Code Cache clear at %08X\n", curr_pc);
	codeBuffer.reset(true);
	bm_ResetTempCache(full);
}

static void recSh4_ClearCache()
{
	INFO_LOG(DYNAREC, "recSh4:Dynarec Cache clear at %08X free space %d", next_pc, codeBuffer.getFreeSpace());
	codeBuffer.reset(false);
	bm_ResetCache();
	smc_hotspots.clear();
	clear_temp_cache(true);
}

static void recSh4_Run()
{
	sh4_int_bCpuRun = true;
	RestoreHostRoundingMode();

	u8 *sh4_dyna_rcb = (u8 *)&Sh4cntx + sizeof(Sh4cntx);
	INFO_LOG(DYNAREC, "cntx // fpcb offset: %td // pc offset: %td // pc %08X", (u8*)&sh4rcb.fpcb - sh4_dyna_rcb, (u8*)&sh4rcb.cntx.pc - sh4_dyna_rcb, sh4rcb.cntx.pc);
	
	sh4Dynarec->mainloop(sh4_dyna_rcb);

	sh4_int_bCpuRun = false;
}

void AnalyseBlock(RuntimeBlockInfo* blk);

bool RuntimeBlockInfo::Setup(u32 rpc,fpscr_t rfpu_cfg)
{
	addr = host_code_size = 0;
	guest_cycles = guest_opcodes = host_opcodes = 0;
	sh4_code_size = 0;
	pBranchBlock=pNextBlock=0;
	code=0;
	has_jcond=false;
	BranchBlock = NullAddress;
	NextBlock = NullAddress;
	BlockType = BET_SCL_Intr;
	has_fpu_op = false;
	temp_block = false;
	
	vaddr = rpc;
	if (vaddr & 1)
	{
		// read address error
		Do_Exception(vaddr, Sh4Ex_AddressErrorRead);
		return false;
	}
	else if (mmu_enabled())
	{
		MmuError rv = mmu_instruction_translation(vaddr, addr);
		if (rv != MmuError::NONE)
		{
			DoMMUException(vaddr, rv, MMU_TT_IREAD);
			return false;
		}
	}
	else
	{
		addr = vaddr;
	}
	fpu_cfg=rfpu_cfg;
	
	oplist.clear();

	try {
		if (!dec_DecodeBlock(this, SH4_TIMESLICE / 2))
			return false;
	}
	catch (const SH4ThrownException& ex) {
		Do_Exception(rpc, ex.expEvn);
		return false;
	}
	SetProtectedFlags();

	AnalyseBlock(this);

	return true;
}

DynarecCodeEntryPtr rdv_CompilePC(u32 blockcheck_failures)
{
	const u32 pc = next_pc;

	if (codeBuffer.getFreeSpace() < 16_KB || pc == 0x8c0000e0 || pc == 0xac010000 || pc == 0xac008300)
		recSh4_ClearCache();

	RuntimeBlockInfo* rbi = sh4Dynarec->allocateBlock();

	if (!rbi->Setup(pc, fpscr))
	{
		delete rbi;
		return nullptr;
	}
	rbi->blockcheck_failures = blockcheck_failures;
	if (smc_hotspots.find(rbi->addr) != smc_hotspots.end())
	{
		codeBuffer.useTempBuffer(true);
		if (codeBuffer.getFreeSpace() < 16_KB)
			clear_temp_cache(false);
		rbi->temp_block = true;
		if (rbi->read_only)
			INFO_LOG(DYNAREC, "WARNING: temp block %x (%x) is protected!", rbi->vaddr, rbi->addr);
	}
	bool do_opts = !rbi->temp_block;
	bool block_check = !rbi->read_only;
	sh4Dynarec->compile(rbi, block_check, do_opts);
	verify(rbi->code != nullptr);

	bm_AddBlock(rbi);

	codeBuffer.useTempBuffer(false);

	return rbi->code;
}

DynarecCodeEntryPtr DYNACALL rdv_FailedToFindBlock_pc()
{
	return rdv_FailedToFindBlock(next_pc);
}

DynarecCodeEntryPtr DYNACALL rdv_FailedToFindBlock(u32 pc)
{
	//DEBUG_LOG(DYNAREC, "rdv_FailedToFindBlock %08x", pc);
	next_pc=pc;
	DynarecCodeEntryPtr code = rdv_CompilePC(0);
	if (code == NULL)
		code = bm_GetCodeByVAddr(next_pc);
	else
		code = (DynarecCodeEntryPtr)CC_RW2RX(code);
	return code;
}

static void ngen_FailedToFindBlock_internal() {
	rdv_FailedToFindBlock(Sh4cntx.pc);
}

void (*ngen_FailedToFindBlock)() = &ngen_FailedToFindBlock_internal;

// addr must be the physical address of the start of the block
DynarecCodeEntryPtr DYNACALL rdv_BlockCheckFail(u32 addr)
{
	DEBUG_LOG(DYNAREC, "rdv_BlockCheckFail @ %08x", addr);
	u32 blockcheck_failures = 0;
	if (mmu_enabled())
	{
		RuntimeBlockInfoPtr block = bm_GetBlock(addr);
		if (block)
		{
			blockcheck_failures = block->blockcheck_failures + 1;
			if (blockcheck_failures > 5)
			{
				bool inserted = smc_hotspots.insert(addr).second;
				if (inserted)
					DEBUG_LOG(DYNAREC, "rdv_BlockCheckFail SMC hotspot @ %08x fails %d", addr, blockcheck_failures);
			}
			bm_DiscardBlock(block.get());
		}
	}
	else
	{
		next_pc = addr;
		recSh4_ClearCache();
	}
	return (DynarecCodeEntryPtr)CC_RW2RX(rdv_CompilePC(blockcheck_failures));
}

DynarecCodeEntryPtr rdv_FindOrCompile()
{
	DynarecCodeEntryPtr rv = bm_GetCodeByVAddr(next_pc);  // Returns exec addr
	if (rv == ngen_FailedToFindBlock)
		rv = (DynarecCodeEntryPtr)CC_RW2RX(rdv_CompilePC(0));  // Returns rw addr
	
	return rv;
}

void* DYNACALL rdv_LinkBlock(u8* code,u32 dpc)
{
	// code is the RX addr to return after, however bm_GetBlock returns RW
	//DEBUG_LOG(DYNAREC, "rdv_LinkBlock %p pc %08x", code, dpc);
	RuntimeBlockInfoPtr rbi = bm_GetBlock(code);
	bool stale_block = false;
	if (!rbi)
	{
		stale_block = true;
		rbi = bm_GetStaleBlock(code);
	}
	
	verify(rbi != NULL);

	u32 bcls = BET_GET_CLS(rbi->BlockType);

	if (bcls == BET_CLS_Static)
	{
		if (rbi->BlockType == BET_StaticIntr)
			next_pc = rbi->NextBlock;
		else
			next_pc = rbi->BranchBlock;
	}
	else if (bcls == BET_CLS_Dynamic)
	{
		next_pc = dpc;
	}
	else if (bcls == BET_CLS_COND)
	{
		if (dpc)
			next_pc = rbi->BranchBlock;
		else
			next_pc = rbi->NextBlock;
	}

	DynarecCodeEntryPtr rv = rdv_FindOrCompile();  // Returns rx ptr

	if (!mmu_enabled() && !stale_block)
	{
		if (bcls == BET_CLS_Dynamic)
		{
			verify(rbi->relink_data == 0 || rbi->pBranchBlock == NULL);

			if (rbi->pBranchBlock != NULL)
			{
				rbi->pBranchBlock->RemRef(rbi);
				rbi->pBranchBlock = NULL;
				rbi->relink_data = 1;
			}
			else if (rbi->relink_data == 0)
			{
				rbi->pBranchBlock = bm_GetBlock(next_pc).get();
				rbi->pBranchBlock->AddRef(rbi);
			}
		}
		else
		{
			RuntimeBlockInfo* nxt = bm_GetBlock(next_pc).get();

			if (rbi->BranchBlock == next_pc)
				rbi->pBranchBlock = nxt;
			if (rbi->NextBlock == next_pc)
				rbi->pNextBlock = nxt;

			nxt->AddRef(rbi);
		}
		u32 ncs = rbi->relink_offset + rbi->Relink();
		verify(rbi->host_code_size >= ncs);
		rbi->host_code_size = ncs;
	}
	else
	{
		INFO_LOG(DYNAREC, "null RBI: from %08X to %08X -- unlinked stale block -- code %p next %p", rbi->vaddr, next_pc, code, rv);
	}
	
	return (void*)rv;
}

static void recSh4_Stop()
{
	sh4Interp.Stop();
}

static void recSh4_Step()
{
	sh4Interp.Step();
}

static void recSh4_Reset(bool hard)
{
	sh4Interp.Reset(hard);
	recSh4_ClearCache();
	if (hard)
		bm_Reset();
}

static void recSh4_Init()
{
	INFO_LOG(DYNAREC, "recSh4 Init");
	Get_Sh4Interpreter(&sh4Interp);
	sh4Interp.Init();
	bm_Init();
	
	if (addrspace::virtmemEnabled())
		verify(&mem_b[0] == ((u8*)p_sh4rcb->sq_buffer + 512 + 0x0C000000));

	// Call the platform-specific magic to make the pages RWX
	CodeCache = nullptr;
#ifdef FEAT_NO_RWX_PAGES
	bool rc = virtmem::prepare_jit_block(SH4_TCB, FULL_SIZE, (void**)&CodeCache, &cc_rx_offset);
#else
	bool rc = virtmem::prepare_jit_block(SH4_TCB, FULL_SIZE, (void**)&CodeCache);
#endif
	verify(rc);
	// Ensure the pointer returned is non-null
	verify(CodeCache != nullptr);

	TempCodeCache = CodeCache + CODE_SIZE;
	sh4Dynarec->init(codeBuffer);
	bm_ResetCache();
}

static void recSh4_Term()
{
	INFO_LOG(DYNAREC, "recSh4 Term");
#ifdef FEAT_NO_RWX_PAGES
	if (CodeCache != nullptr)
		virtmem::release_jit_block(CodeCache, (u8 *)CodeCache + cc_rx_offset, FULL_SIZE);
#else
	if (CodeCache != nullptr && CodeCache != SH4_TCB)
		virtmem::release_jit_block(CodeCache, FULL_SIZE);
#endif
	CodeCache = nullptr;
	TempCodeCache = nullptr;
	bm_Term();
	sh4Interp.Term();
}

static bool recSh4_IsCpuRunning()
{
	return sh4Interp.IsCpuRunning();
}

void Get_Sh4Recompiler(sh4_if* cpu)
{
	cpu->Run = recSh4_Run;
	cpu->Stop = recSh4_Stop;
	cpu->Step = recSh4_Step;
	cpu->Reset = recSh4_Reset;
	cpu->Init = recSh4_Init;
	cpu->Term = recSh4_Term;
	cpu->IsCpuRunning = recSh4_IsCpuRunning;
	cpu->ResetCache = recSh4_ClearCache;
}

static bool translateAddress(u32 addr, int size, u32 access, u32& outAddr, RuntimeBlockInfo* block)
{
	if (mmu_enabled() && mmu_is_translated(addr, size))
	{
		if (addr & (size - 1))
			// Unaligned
			return false;
		if (block != nullptr
				&& (addr >> 12) != (block->vaddr >> 12)
				&& (addr >> 12) != ((block->vaddr + block->sh4_code_size - 1) >> 12))
			// When full mmu is on, only consider addresses in the same 4k page
			return false;

		u32 paddr;
		MmuError rv = access == MMU_TT_DREAD ?
				mmu_data_translation<MMU_TT_DREAD>(addr, paddr)
				: mmu_data_translation<MMU_TT_DWRITE>(addr, paddr);
		if (rv != MmuError::NONE)
			return false;

		addr = paddr;
	}
	outAddr = addr;

	return true;
}

bool rdv_readMemImmediate(u32 addr, int size, void*& ptr, bool& isRam, u32& physAddr, RuntimeBlockInfo* block)
{
	size = std::min(size, 4);
	if (!translateAddress(addr, size, MMU_TT_DREAD, physAddr, block))
		return false;
	ptr = addrspace::readConst(physAddr, isRam, size);

	return true;
}

bool rdv_writeMemImmediate(u32 addr, int size, void*& ptr, bool& isRam, u32& physAddr, RuntimeBlockInfo* block)
{
	size = std::min(size, 4);
	if (!translateAddress(addr, size, MMU_TT_DWRITE, physAddr, block))
		return false;
	ptr = addrspace::writeConst(physAddr, isRam, size);

	return true;
}

void rdv_SetFailedToFindBlockHandler(void (*handler)())
{
	ngen_FailedToFindBlock = handler;
}
#endif  // FEAT_SHREC != DYNAREC_NONE
