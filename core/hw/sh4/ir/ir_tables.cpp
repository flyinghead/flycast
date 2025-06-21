#include "ir_tables.h"

namespace sh4 {
namespace ir {

// bring in the generated arrays *inside* the namespace so Op resolves correctly
#include "ir_emitter_table.inc"
#include "ir_executor_table.inc"

const Op* kEmitterTable = &kSh4IrTbl_EMIT[0];
const Op* kExecutorTable = &kSh4IrTbl_EXEC[0];

} // namespace ir
} // namespace sh4
