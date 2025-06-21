#pragma once

#include "ir_defs.h"
#include "hw/sh4/sh4_if.h"

namespace sh4 {
namespace ir {

class Executor {
public:
    // Execute a single IR block using provided SH4 context.
    void ExecuteBlock(const Block* blk, Sh4Context* ctx);
    
    // Reset any cached block pointers to force re-fetching blocks
    // This ensures proper handling of self-modifying code
    void ResetCachedBlocks();
    
    // Track the last executed block for cache invalidation
    const Block* lastExecutedBlock = nullptr;
};

} // namespace ir
} // namespace sh4
