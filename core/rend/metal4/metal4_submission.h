// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "metal4_commands.h"
#ifdef FLYCAST_METAL_STANDALONE
#include <stdexcept>
using MetalSubmissionException=std::runtime_error;
#else
#include "hw/pvr/Renderer_if.h"
using MetalSubmissionException=RendererException;
#endif
// Both the production display callback and isolated native tests submit through
// this same completion-group ownership boundary.
API_AVAILABLE(macos(26.0))
inline void commitCommands(FCMetal4Commands *command,dispatch_group_t submissions)
{
    if(!command || !submissions || command.status>=FCMetal4StatusCommitted)
        throw MetalSubmissionException("Invalid Metal command buffer submission");
    // A failed pre-submit attempt can leave this handler on an encoding
    // command that is retried later. Cancel only that attempt's group entry;
    // its stale handler must never retire a later submission or another owner.
    // Before enqueue only the producer accesses pending; afterwards only feedback does.
    __block bool pending=false;
    [command addCompletedHandler:^(FCMetal4Commands *){
        if(pending){pending=false;dispatch_group_leave(submissions);}
    }];
    dispatch_group_enter(submissions);
    pending=true;
    try {[command commit];}
    catch(...) {
        if(command.status==FCMetal4StatusEncoding){pending=false;dispatch_group_leave(submissions);}
        throw;
    }
}
