#pragma once

#include "hw/sh4/sh4_if.h"
#include "ir_emitter.h"
#include "ir_executor.h"

namespace sh4 {
namespace ir {

// Obtain sh4_if interface wrappers that drive the IR interpreter.
// This mirrors the legacy Get_Sh4Interpreter function used by the core,
// but routes all calls to an internal Sh4IrInterpreter instance.
void Get_Sh4Interpreter(sh4_if* cpu);

class Sh4IrInterpreter : public Executor {
public:
    Sh4IrInterpreter();
    ~Sh4IrInterpreter() = default;

    void Run();
    void Start() { running_ = true; }
    void Stop() { running_ = false; }
    void Step();
    void Reset(bool hard);
    void Init();
    void Term() {}
    // Completely clear IR emitter caches, including global dedup map, to avoid
    // dangling Block pointers after the interpreter resets between unit-test
    // sub-cases.
    void ResetCache() { emitter_.ClearCaches(); }
    bool IsCpuRunning() { return running_; }

// Invalidate any cached block touching `addr` globally (helper for memory writes)
void GlobalInvalidate(u32 addr);
    Sh4Context* GetContext() { return ctx_; }

    // Invalidate a block at the specified address
    void InvalidateBlock(u32 addr);

private:
    bool running_ = false;
    Sh4Context* ctx_ = nullptr; // provided by core elsewhere
    Emitter emitter_;
    Executor executor_;
};

// Global singleton instance (defined in sh4_ir_interpreter.cpp)
extern Sh4IrInterpreter g_ir;

} // namespace ir
} // namespace sh4
