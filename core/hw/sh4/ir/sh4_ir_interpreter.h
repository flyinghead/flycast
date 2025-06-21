#pragma once

#include "hw/sh4/sh4_if.h"
#include "ir_emitter.h"
#include "ir_executor.h"

namespace sh4 {
namespace ir {

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
    void ResetCache() { emitter_ = Emitter{}; }
    bool IsCpuRunning() { return running_; }
    Sh4Context* GetContext() { return ctx_; }

    // Invalidate a block at the specified address
    void InvalidateBlock(u32 addr);

private:
    bool running_ = false;
    Sh4Context* ctx_ = nullptr; // provided by core elsewhere
    Emitter emitter_;
    Executor executor_;
};

} // namespace ir
} // namespace sh4
