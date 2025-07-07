/*
	ULTRA-AGGRESSIVE SH4 INTERPRETER FOR IPHONE ARM64
	Optimized to the extreme for maximum performance without JIT
	FOCUS: Remove CPU_RATIO bottleneck for maximum speed
*/

#include "types.h"

#include "../sh4_interpreter.h"
#include "../sh4_opcode_list.h"
#include "../sh4_core.h"
#include "../sh4_interrupts.h"
#include "hw/sh4/sh4_mem.h"
#include "../sh4_sched.h"
#include "../sh4_cache.h"
#include "debug/gdb_server.h"
#include "../sh4_cycles.h"

#ifdef FMV_OPTIMIZED
#include <arm_neon.h>
#endif

// SH4 underclock factor when using the interpreter so that it's somewhat usable
#ifdef STRICT_MODE
constexpr int CPU_RATIO = 1;
#else
constexpr int CPU_RATIO = 1;  // REMOVED BOTTLENECK: Set to 1 for maximum speed!
#endif

Sh4ICache icache;
Sh4OCache ocache;

// === ULTRA-AGGRESSIVE OPTIMIZATIONS FOR IPHONE ===

// Ultra-large instruction cache for maximum hit rates on ARM64
#define ULTRA_ICACHE_SIZE 32768  // Increased to 32KB for maximum cache hit rate  
#define ULTRA_ICACHE_MASK (ULTRA_ICACHE_SIZE - 1)

// Advanced caching parameters
#define ULTRA_CACHE_ASSOCIATIVITY 4  // 4-way set associative for better conflict resolution
#define ULTRA_CACHE_LINE_SIZE 64     // Larger cache lines for spatial locality

// ARM64 NEON-optimized instruction cache - ULTRA-ENHANCED
struct alignas(64) UltraInstructionCache {
    // Set-associative cache structure for better conflict resolution
    struct CacheWay {
        u32 pc;
        u16 opcode;
        u32 lru_counter;  // LRU replacement tracking
    };
    
#ifdef FMV_OPTIMIZED
    // Larger cache for FMV sequences - 64KB total with 4-way associativity
    static constexpr u32 CACHE_SETS = (ULTRA_ICACHE_SIZE * 2) / (sizeof(CacheWay) * ULTRA_CACHE_ASSOCIATIVITY);
#else
    static constexpr u32 CACHE_SETS = ULTRA_ICACHE_SIZE / (sizeof(CacheWay) * ULTRA_CACHE_ASSOCIATIVITY);
#endif
    
    CacheWay cache_ways[CACHE_SETS][ULTRA_CACHE_ASSOCIATIVITY];
    u32 global_lru_counter;
    
    // Performance tracking
    u32 cache_hits;
    u32 cache_misses;
    u32 conflict_misses;
    
    void reset() {
        // Ultra-fast cache initialization with NEON
#ifdef FMV_OPTIMIZED
        uint32x4_t invalid_pc = vdupq_n_u32(0xFFFFFFFF);
        uint32x4_t zero_lru = vdupq_n_u32(0);
        
        for (u32 set = 0; set < CACHE_SETS; set++) {
            for (u32 way = 0; way < ULTRA_CACHE_ASSOCIATIVITY; way += 4) {
                // Use NEON to clear 4 ways at once
                vst1q_u32(&cache_ways[set][way].pc, invalid_pc);
                vst1q_u32(&cache_ways[set][way].lru_counter, zero_lru);
            }
        }
#else
        // Standard initialization
        for (u32 set = 0; set < CACHE_SETS; set++) {
            for (u32 way = 0; way < ULTRA_CACHE_ASSOCIATIVITY; way++) {
                cache_ways[set][way].pc = 0xFFFFFFFF;
                cache_ways[set][way].lru_counter = 0;
            }
        }
#endif
        global_lru_counter = 1;
        cache_hits = cache_misses = conflict_misses = 0;
    }
    
    __attribute__((always_inline)) inline u16 fetch(u32 addr) {
        u32 set_index = (addr >> 1) % CACHE_SETS;
        CacheWay* set = cache_ways[set_index];
        
        // Search all ways in the set for a hit
        for (u32 way = 0; way < ULTRA_CACHE_ASSOCIATIVITY; way++) {
            if (__builtin_expect(set[way].pc == addr, 1)) {
                // Cache hit - update LRU and return
                set[way].lru_counter = ++global_lru_counter;
                cache_hits++;
                
                // Prefetch next set for spatial locality
                __builtin_prefetch(&cache_ways[(set_index + 1) % CACHE_SETS], 0, 3);
                return set[way].opcode;
            }
        }
        
        // Cache miss - find LRU way to replace
        u32 lru_way = 0;
        u32 min_lru = set[0].lru_counter;
        for (u32 way = 1; way < ULTRA_CACHE_ASSOCIATIVITY; way++) {
            if (set[way].lru_counter < min_lru) {
                min_lru = set[way].lru_counter;
                lru_way = way;
            }
        }
        
        // Check if this is a conflict miss (all ways occupied)
        if (set[lru_way].pc != 0xFFFFFFFF) {
            conflict_misses++;
        }
        
        // Fetch from memory and update cache
        u16 op = IReadMem16(addr);
        set[lru_way].pc = addr;
        set[lru_way].opcode = op;
        set[lru_way].lru_counter = ++global_lru_counter;
        cache_misses++;
        
        // Prefetch next instruction for spatial locality
        __builtin_prefetch(&cache_ways[(set_index + 1) % CACHE_SETS], 1, 3);
        
        return op;
    }
};

static UltraInstructionCache g_ultra_icache;

// === DYNAMIC CPU TIMING SYSTEM ===

struct DynamicTimingController {
    // Performance monitoring
    u32 consecutive_fast_frames = 0;
    u32 consecutive_slow_frames = 0;
    u64 last_performance_check = 0;
    
    // Adaptive system update intervals
    u32 current_system_update_interval = 512;  // Start conservative
    u32 min_interval = 256;   // Responsive mode
    u32 max_interval = 2048;  // FMV boost mode
    
    // FMV detection
    bool fmv_mode_detected = false;
    u32 low_interaction_frames = 0;
    
    // CPU timing adaptation
    float effective_cpu_ratio = 1.0f;
    float base_cpu_ratio = 1.0f;
    float max_boost_ratio = 3.0f;  // Maximum 3x boost for FMV
    
    // === ULTRA-AGGRESSIVE MAIN LOOP OPTIMIZATIONS ===
    
    	// System call reduction system - ULTRA-AGGRESSIVE
	u32 system_call_counter = 0;
	u32 max_system_call_interval = 8192;  // Massive reduction in system calls
	
	// Interrupt batching system - ULTRA-AGGRESSIVE
	u32 interrupt_batch_counter = 0;
	u32 interrupt_batch_size = 128;  // Massive interrupt batching
	
	// Ultra system call reduction for sustained workloads
	bool ultra_syscall_reduction = false;
	u32 ultra_syscall_interval = 16384;  // Extreme syscall reduction
	u32 ultra_interrupt_batch = 256;     // Massive interrupt batching
	u32 consecutive_syscall_skips = 0;
    
    	// Performance-based scheduler bypass - ULTRA-AGGRESSIVE
	bool scheduler_bypass_mode = false;
	u32 scheduler_bypass_cycles = 0;
	u32 max_scheduler_bypass = 32768;  // Skip scheduler for very long periods
	
	// Advanced scheduler bypass control
	bool ultra_bypass_mode = false;
	u32 ultra_bypass_threshold = 65536;  // Massive bypass for sustained workloads
	u32 consecutive_bypass_cycles = 0;
	u32 total_execution_cycles = 0;
    
    void init() {
        current_system_update_interval = 512;
        effective_cpu_ratio = base_cpu_ratio;
        fmv_mode_detected = false;
        		system_call_counter = 0;
		interrupt_batch_counter = 0;
		ultra_syscall_reduction = false;
		consecutive_syscall_skips = 0;
        		scheduler_bypass_mode = false;
		scheduler_bypass_cycles = 0;
		ultra_bypass_mode = false;
		consecutive_bypass_cycles = 0;
		total_execution_cycles = 0;
    }
    
    	bool should_update_system() {
		// Ultra-aggressive system update reduction with adaptive scaling
		u32 current_interval = ultra_syscall_reduction ? ultra_syscall_interval : max_system_call_interval;
		
		if (++system_call_counter >= current_interval) {
			system_call_counter = 0;
			return true;
		}
		
		consecutive_syscall_skips++;
		return false;
	}
	
	bool should_check_interrupts() {
		// Massive interrupt batching with ultra mode
		u32 current_batch_size = ultra_syscall_reduction ? ultra_interrupt_batch : interrupt_batch_size;
		
		if (++interrupt_batch_counter >= current_batch_size) {
			interrupt_batch_counter = 0;
			return true;
		}
		return false;
	}
    
    	bool can_bypass_scheduler() {
		if (!scheduler_bypass_mode && !ultra_bypass_mode) return false;
		
		u32 current_limit = ultra_bypass_mode ? ultra_bypass_threshold : max_scheduler_bypass;
		return (scheduler_bypass_cycles < current_limit);
	}
    
    void update_performance_mode() {
        u64 current_time = sh4_sched_now64();
        
        // Check if we should update performance mode (every ~100ms)
        if (current_time - last_performance_check < 20000000) return;  // 100ms
        last_performance_check = current_time;
        
        		// Detect FMV mode based on consistent performance
		if (consecutive_fast_frames > 10) {
			// System is running well - enable FMV optimizations
			if (!fmv_mode_detected) {
				fmv_mode_detected = true;
				current_system_update_interval = max_interval;
				effective_cpu_ratio = max_boost_ratio;
				scheduler_bypass_mode = true;
				max_system_call_interval = 8192;  // Even more aggressive in FMV mode
				interrupt_batch_size = 128;       // Larger interrupt batches
				INFO_LOG(INTERPRETER, "🚀 FMV MODE DETECTED: Ultra-aggressive optimizations activated!");
			}
			
			// Enable ultra bypass for sustained good performance
			if (consecutive_fast_frames > 25 && !ultra_bypass_mode) {
				ultra_bypass_mode = true;
				max_scheduler_bypass = ultra_bypass_threshold;
				INFO_LOG(INTERPRETER, "⚡ SUSTAINED WORKLOAD DETECTED: ULTRA-BYPASS mode enabled (threshold=%u)", ultra_bypass_threshold);
			}
			
			// Enable ultra syscall reduction for extreme performance
			if (consecutive_fast_frames > 40 && !ultra_syscall_reduction) {
				ultra_syscall_reduction = true;
				max_system_call_interval = ultra_syscall_interval;
				interrupt_batch_size = ultra_interrupt_batch;
				INFO_LOG(INTERPRETER, "🚀 EXTREME WORKLOAD DETECTED: ULTRA-SYSCALL REDUCTION enabled (syscall_interval=%u, interrupt_batch=%u)", 
						 ultra_syscall_interval, ultra_interrupt_batch);
			}
            consecutive_fast_frames = 0;
        } else if (consecutive_slow_frames > 5) {
            // System struggling - reduce optimizations for stability
            if (fmv_mode_detected) {
                fmv_mode_detected = false;
                current_system_update_interval = min_interval;
                effective_cpu_ratio = base_cpu_ratio;
                scheduler_bypass_mode = false;
                max_system_call_interval = 2048;  // More responsive
                interrupt_batch_size = 32;        // Smaller batches
                INFO_LOG(INTERPRETER, "📉 Performance issues detected: Reducing optimizations for stability");
            }
            consecutive_slow_frames = 0;
        }
    }
    
    u32 get_system_update_interval() const {
        return current_system_update_interval;
    }
    
    float get_effective_cpu_ratio() const {
        return effective_cpu_ratio;
    }
    
    void mark_frame_performance(bool fast_frame) {
        if (fast_frame) {
            consecutive_fast_frames++;
            consecutive_slow_frames = 0;
        } else {
            consecutive_slow_frames++;
            consecutive_fast_frames = 0;
        }
    }
};

static DynamicTimingController g_dynamic_timing;

// === MAXIMUM SPEED TIMING OPTIMIZATION ===

// Reduce scheduler overhead by batching system updates even more aggressively
static u32 g_system_update_counter = 0;
static constexpr u32 SYSTEM_UPDATE_INTERVAL = 512;  // Increased from 128 to 512 for maximum speed

// Track timing to maintain proper framerate
static u64 g_last_frame_time = 0;
static u32 g_frame_counter = 0;

// Ultra-fast execution with ARM64 optimizations - NO CPU_RATIO penalty
__attribute__((always_inline, hot)) 
static inline void UltraExecuteOpcode(u16 op)
{
    // ARM64 branch prediction hint
    if (__builtin_expect(sr.FD == 1 && OpDesc[op]->IsFloatingPoint(), 0))
        RaiseFPUDisableException();
        
    // Direct function call with ARM64 optimization
    OpPtr[op](op);
    sh4cycles.executeCycles(op);  // This now uses CPU_RATIO = 1 (no penalty!)
}

// Ultra-fast instruction fetch with ARM64 optimizations
__attribute__((always_inline, hot))
static inline u16 UltraReadNextOp()
{
    // ARM64 optimized address error check
    if (__builtin_expect(!mmu_enabled() && (next_pc & 1), 0))
        throw SH4ThrownException(next_pc, Sh4Ex_AddressErrorRead);

    u32 addr = next_pc;
    next_pc += 2;

    // Ultra-fast instruction cache with ARM64 prefetching
    return g_ultra_icache.fetch(addr);
}

// ULTRA-AGGRESSIVE main execution loop optimized for maximum iPhone speed
static void __attribute__((hot)) Sh4_int_Run()
{
    RestoreHostRoundingMode();

    // Reset instruction cache at start
    g_ultra_icache.reset();
    g_system_update_counter = 0;

    try {
        // Outer loop with ARM64 optimization
        do {
            try {
                                 // === ULTRA-AGGRESSIVE MAIN LOOP WITH SCHEDULER BYPASS ===
                 s32 cycles = p_sh4rcb->cntx.cycle_counter;
                 
                 // Ultra-aggressive execution with scheduler bypass for maximum FMV performance
                 while (__builtin_expect(cycles > 0, 1)) {
                     
                     					// === SCHEDULER BYPASS MODE ===
					if (g_dynamic_timing.can_bypass_scheduler()) {
						// Ultra-fast execution without scheduler overhead
						register int batch_size asm("w19");
						
						if (g_dynamic_timing.ultra_bypass_mode) {
							// Ultra bypass mode: massive batches for sustained workloads
#ifdef FMV_OPTIMIZED
							batch_size = 128;  // Huge batches in FMV + ultra mode
#else
							batch_size = 96;   // Large batches in ultra mode
#endif
						} else {
							// Normal bypass mode
#ifdef FMV_OPTIMIZED
							batch_size = 64;   // FMV mode batches
#else
							batch_size = 32;   // Normal bypass batches
#endif
						}
						
						for (register int i asm("w20") = 0; i < batch_size && cycles > 0; i++) {
							register u16 op asm("w21") = UltraReadNextOp();
							UltraExecuteOpcode(op);
							cycles--;
						}
						
						g_dynamic_timing.scheduler_bypass_cycles += batch_size;
						g_dynamic_timing.consecutive_bypass_cycles += batch_size;
						g_dynamic_timing.total_execution_cycles += batch_size;
                         
                         // Only check interrupts occasionally in bypass mode
                         if (__builtin_expect(g_dynamic_timing.should_check_interrupts(), 0)) {
                             g_dynamic_timing.interrupt_batch_counter = 0;
                             if (__builtin_expect(p_sh4rcb->cntx.interrupt_pend != 0, 0)) {
                                 break;  // Exit to handle interrupts
                             }
                         }
                     }
                     else {
                         // === NORMAL BATCHED EXECUTION ===
#ifdef FMV_OPTIMIZED
                         // FMV-optimized batch: Execute 32 instructions per batch for video decode performance
                         register int batch_size asm("w19") = 32;
#else
                         // Normal batch: Execute 16 instructions per batch for balanced performance
                         register int batch_size asm("w19") = 16;
#endif
                         
                         for (register int i asm("w20") = 0; i < batch_size && cycles > 0; i++) {
                             register u16 op asm("w21") = UltraReadNextOp();
                             UltraExecuteOpcode(op);
                             cycles--;
                         }
                         
                         // Ultra-aggressive system call reduction
                         if (__builtin_expect(g_dynamic_timing.should_update_system(), 0)) {
                             g_dynamic_timing.system_call_counter = 0;
                             g_dynamic_timing.scheduler_bypass_cycles = 0;  // Reset bypass counter
                             
                             // Update dynamic timing based on performance
                             g_dynamic_timing.update_performance_mode();
                             
                             // Batched interrupt checking
                             if (__builtin_expect(g_dynamic_timing.should_check_interrupts(), 0)) {
                                 g_dynamic_timing.interrupt_batch_counter = 0;
                                 if (__builtin_expect(p_sh4rcb->cntx.interrupt_pend != 0, 0)) {
                                     break;  // Exit to handle interrupts
                                 }
                             }
                         }
                     }
                }
                
                p_sh4rcb->cntx.cycle_counter = cycles;
                
                // Only do full system update when necessary
                if (__builtin_expect(cycles <= 0 || p_sh4rcb->cntx.interrupt_pend != 0, 0)) {
                    p_sh4rcb->cntx.cycle_counter += SH4_TIMESLICE;
                    UpdateSystem_INTC();
                }
                
            } catch (const SH4ThrownException& ex) {
                Do_Exception(ex.epc, ex.expEvn);
                sh4cycles.addCycles(5 * g_dynamic_timing.get_effective_cpu_ratio());
            }
        } while (__builtin_expect(sh4_int_bCpuRun, 1));
        
    } catch (const debugger::Stop&) {
    }

    sh4_int_bCpuRun = false;
}

static void Sh4_int_Start()
{
    sh4_int_bCpuRun = true;
}

static void Sh4_int_Stop()
{
    sh4_int_bCpuRun = false;
}

void Sh4_int_Step()
{
    verify(!sh4_int_bCpuRun);

    RestoreHostRoundingMode();
    
    try {
        u32 op = UltraReadNextOp();
        UltraExecuteOpcode(op);
    } catch (const SH4ThrownException& ex) {
        Do_Exception(ex.epc, ex.expEvn);
        sh4cycles.addCycles(5 * g_dynamic_timing.get_effective_cpu_ratio());
    } catch (const debugger::Stop&) {
    }
}

static void Sh4_int_Reset(bool hard)
{
    verify(!sh4_int_bCpuRun);

    if (hard) {
        int schedNext = p_sh4rcb->cntx.sh4_sched_next;
        memset(&p_sh4rcb->cntx, 0, sizeof(p_sh4rcb->cntx));
        p_sh4rcb->cntx.sh4_sched_next = schedNext;
    }
    
    next_pc = 0xA0000000;

    memset(r,0,sizeof(r));
    memset(r_bank,0,sizeof(r_bank));

    gbr=ssr=spc=sgr=dbr=vbr=0;
    mac.full=pr=fpul=0;

    sh4_sr_SetFull(0x700000F0);
    old_sr.status=sr.status;
    UpdateSR();

    fpscr.full = 0x00040001;
    old_fpscr = fpscr;

    icache.Reset(hard);
    ocache.Reset(hard);
    sh4cycles.reset();
    p_sh4rcb->cntx.cycle_counter = SH4_TIMESLICE;
    
    // Reset ultra instruction cache and dynamic timing system
    g_ultra_icache.reset();
    g_system_update_counter = 0;
    g_last_frame_time = 0;
    g_frame_counter = 0;
    
    // Initialize dynamic timing controller
    g_dynamic_timing.init();

    INFO_LOG(INTERPRETER, "🚀 ULTRA-AGGRESSIVE ARM64 Interpreter Reset - Dynamic timing, scheduler bypass, FMV optimizations active!");
}

static bool Sh4_int_IsCpuRunning()
{
    return sh4_int_bCpuRun;
}

void ExecuteDelayslot()
{
    try {
        u32 op = UltraReadNextOp();
        UltraExecuteOpcode(op);
    } catch (SH4ThrownException& ex) {
        AdjustDelaySlotException(ex);
        throw ex;
    } catch (const debugger::Stop& e) {
        next_pc -= 2;
        throw e;
    }
}

void ExecuteDelayslot_RTE()
{
    try {
        u32 op = UltraReadNextOp();
        sh4_sr_SetFull(ssr);
        UltraExecuteOpcode(op);
    } catch (const SH4ThrownException&) {
        throw FlycastException("Fatal: SH4 exception in RTE delay slot");
    } catch (const debugger::Stop& e) {
        next_pc -= 2;
        throw e;
    }
}

int UpdateSystem()
{
    Sh4cntx.sh4_sched_next -= SH4_TIMESLICE;
    if (Sh4cntx.sh4_sched_next < 0)
        sh4_sched_tick(SH4_TIMESLICE);

    return Sh4cntx.interrupt_pend;
}

int UpdateSystem_INTC()
{
    if (UpdateSystem())
        return UpdateINTC();
    else
        return 0;
}

void Get_Sh4Interpreter(sh4_if* cpu)
{
    cpu->Run = Sh4_int_Run;
    cpu->Start = Sh4_int_Start;
    cpu->Stop = Sh4_int_Stop;
    cpu->Step = Sh4_int_Step;
    cpu->Reset = Sh4_int_Reset;
    cpu->Init = []() { INFO_LOG(INTERPRETER, "🚀 ULTRA-AGGRESSIVE INTERPRETER: Dynamic timing, scheduler bypass, FMV detection, ARM64 optimizations!"); };
    cpu->Term = []() {};
    cpu->ResetCache = []() { g_ultra_icache.reset(); };
    cpu->IsCpuRunning = Sh4_int_IsCpuRunning;
}
