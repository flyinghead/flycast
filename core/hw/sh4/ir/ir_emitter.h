#pragma once

#include "ir_defs.h"
#include <unordered_map>

namespace sh4 {
namespace ir {

class Emitter {
public:
    // Translate starting at pc, returns pointer to cached block.
    const Block* BuildBlock(uint32_t pc);
    void ClearCaches();
private:
    std::unordered_map<uint32_t, Block> cache_;
    Block& CreateNew(uint32_t pc);
    void EmitInstr(Block& blk, uint16_t opcode);
};

} // namespace ir
} // namespace sh4
