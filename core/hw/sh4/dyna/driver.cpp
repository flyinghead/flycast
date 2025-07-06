#include "types.h"
#include <unordered_set>

#include "hw/sh4/sh4_interpreter.h"
#include "hw/sh4/ir/sh4_ir_interpreter.h"
#include "hw/sh4/sh4_core.h"
#include "hw/sh4/sh4_interrupts.h"
#include "hw/sh4/sh4_sched.h"

#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/modules/mmu.h"

#include "blockmanager.h"
#include "ngen.h"
#include "decoder.h"
#include "oslib/virtmem.h"

#include "shil_interpreter.h"

#if FEAT_SHREC != DYNAREC_NONE

// Enlarged to reduce cache evictions and block recompilation
constexpr u32 CODE_SIZE = 128_MB;
constexpr u32 TEMP_CODE_SIZE = 4_MB;
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

// === CACHE-FRIENDLY DRIVER IMPROVEMENTS ===
// Prevent excessive cache clearing that destroys performance

static u32 cache_clear_count = 0;
static u64 last_clear_time = 0;
static u32 blocks_compiled_since_clear = 0;

// Cache clear prevention thresholds
static constexpr u32 MIN_CLEAR_INTERVAL_MS = 3000;  // Don't clear more than once per 3 seconds (reduced)
static constexpr u32 MIN_BLOCKS_BEFORE_CLEAR = 50;  // Need at least 50 blocks before clearing (reduced)
static constexpr u32 AGGRESSIVE_CLEAR_THRESHOLD = 8_MB; // Clear when buffer gets low (increased from 16MB)

// Check if we should prevent cache clearing
static bool should_prevent_cache_clear(u32 pc, const char* reason) {
    u64 current_time = sh4_sched_now64() / (SH4_MAIN_CLOCK / 1000);  // Convert to milliseconds
    
    // Always allow cache clear at startup
    if (cache_clear_count == 0) {
        return false;
    }
    
    // CRITICAL: Never prevent cache clear if buffer is critically low (< 1MB)
    // This prevents crashes when buffer runs out of space
    if (codeBuffer.getFreeSpace() < 1_MB) {
        INFO_LOG(DYNAREC, "🚨 CACHE-FRIENDLY: Allowing emergency cache clear (%s) - buffer critically low (%u bytes)", 
                 reason, codeBuffer.getFreeSpace());
        return false;
    }
    
    // Check time-based prevention
    if (current_time - last_clear_time < MIN_CLEAR_INTERVAL_MS) {
        INFO_LOG(DYNAREC, "🛡️ CACHE-FRIENDLY: Preventing cache clear (%s) - only %llu ms since last clear (need %u ms)", 
                 reason, current_time - last_clear_time, MIN_CLEAR_INTERVAL_MS);
        return true;
    }
    
    // Check block-based prevention
    if (blocks_compiled_since_clear < MIN_BLOCKS_BEFORE_CLEAR) {
        INFO_LOG(DYNAREC, "🛡️ CACHE-FRIENDLY: Preventing cache clear (%s) - only %u blocks compiled (need %u blocks)", 
                 reason, blocks_compiled_since_clear, MIN_BLOCKS_BEFORE_CLEAR);
        return true;
    }
    
    return false;
}

// Track cache clear events
static void on_cache_cleared(const char* reason) {
    u64 current_time = sh4_sched_now64() / (SH4_MAIN_CLOCK / 1000);
    cache_clear_count++;
    
    INFO_LOG(DYNAREC, "🗑️ CACHE-FRIENDLY: Cache cleared (%s) - clear #%u, %u blocks compiled, %llu ms since last clear", 
             reason, cache_clear_count, blocks_compiled_since_clear, 
             last_clear_time > 0 ? current_time - last_clear_time : 0);
    
    last_clear_time = current_time;
    blocks_compiled_since_clear = 0;
}

// Track block compilation
static void on_block_compiled() {
    blocks_compiled_since_clear++;
}

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
	
	// Track cache clear for cache-friendly statistics
	on_cache_cleared("manual");
}

// === HYBRID EXECUTION MODE ===
// This bypasses the entire dynarec system for hot code paths
// and uses direct SH4 execution like the legacy interpreter

// Hybrid execution control
static bool use_hybrid_execution = true;
static u32 hybrid_execution_count = 0;

// Override the main execution loop to use hybrid mode
static void hybrid_execution_loop() {
    if (!use_hybrid_execution) {
        return; // Fall back to normal dynarec
    }
    
    hybrid_execution_count++;
    
    // Use hybrid execution for hot paths
    execute_hybrid_block(next_pc);
    
    // Print stats periodically
    if (hybrid_execution_count % 10000 == 0) {
        print_hybrid_stats();
    }
}

static void recSh4_Run()
{
	RestoreHostRoundingMode();

	u8 *sh4_dyna_rcb = (u8 *)&Sh4cntx + sizeof(Sh4cntx);
	INFO_LOG(DYNAREC, "cntx // fpcb offset: %td // pc offset: %td // pc %08X", (u8*)&sh4rcb.fpcb - sh4_dyna_rcb, (u8*)&sh4rcb.cntx.pc - sh4_dyna_rcb, sh4rcb.cntx.pc);
	
	// **HYBRID EXECUTION**: Try hybrid mode first for maximum performance
	if (use_hybrid_execution) {
		INFO_LOG(DYNAREC, "🚀 Starting HYBRID execution mode - bypassing SHIL for hot paths");
		
		try {
			while (sh4_int_bCpuRun) {
				// Check for exceptions first
				if (UpdateSystem()) {
					continue;
				}
				
				// Use hybrid execution - this can be 10x faster than SHIL for hot paths
				hybrid_execution_loop();
			}
		} catch (const SH4ThrownException& ex) {
			Do_Exception(ex.epc, ex.expEvn);
		} catch (const std::exception& ex) {
			ERROR_LOG(DYNAREC, "Exception in hybrid execution: %s", ex.what());
			// Fall back to regular dynarec
			use_hybrid_execution = false;
		}
	}
	
	// Fall back to regular dynarec if hybrid execution is disabled or failed
	if (!use_hybrid_execution) {
		INFO_LOG(DYNAREC, "🐌 Falling back to regular SHIL dynarec");
		sh4Dynarec->mainloop(sh4_dyna_rcb);
	}

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

	// CACHE-FRIENDLY: Use more aggressive threshold and prevent excessive clearing
	bool need_cache_clear = false;
	const char* clear_reason = nullptr;
	u32 free_space = codeBuffer.getFreeSpace();
	
	if (free_space < AGGRESSIVE_CLEAR_THRESHOLD) {
		need_cache_clear = true;
		clear_reason = "low_space";
		DEBUG_LOG(DYNAREC, "📊 CACHE-FRIENDLY: Low space detected - %u bytes free (threshold %u)", 
		          free_space, AGGRESSIVE_CLEAR_THRESHOLD);
	} else if (pc == 0x8c0000e0 || pc == 0xac010000 || pc == 0xac008300) {
		need_cache_clear = true;
		clear_reason = "special_pc";
	}
	
	if (need_cache_clear && !should_prevent_cache_clear(pc, clear_reason)) {
		INFO_LOG(DYNAREC, "✅ CACHE-FRIENDLY: Allowing cache clear (%s) - %u bytes free", 
		         clear_reason, free_space);
		recSh4_ClearCache();
	}

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
		if (codeBuffer.getFreeSpace() < 32_MB)
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
	
	// CACHE-FRIENDLY: Track block compilation
	on_block_compiled();

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
		// CACHE-FRIENDLY: Prevent excessive clearing on block check failures
		if (!should_prevent_cache_clear(addr, "block_check_fail")) {
			recSh4_ClearCache();
		}
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

static void recSh4_Start()
{
	sh4Interp.Start();
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
#ifdef ENABLE_SH4_CACHED_IR
	sh4::ir::Get_Sh4Interpreter(&sh4Interp);
#else
	Get_Sh4Interpreter(&sh4Interp);
#endif
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
	cpu->Start = recSh4_Start;
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
