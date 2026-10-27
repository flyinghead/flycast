// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <functional>
#include <cstddef>
#include <string>
struct TA_context;
extern std::function<void(TA_context *)> metalOffscreenParsedTa;
extern bool metalOffscreenRecoveryOnly;
extern bool metalOffscreenMaterialAdmissionOnly;
extern bool metalOffscreenFixedPipelinesOnly;
extern double metalOffscreenVideoFieldDuration;
extern bool metalOffscreenResolveAdmissionOnly;
extern bool metalOffscreenStorageAdmissionOnly;
extern bool metalOffscreenResolveBenchmark;
extern bool metalOffscreenStorageOnly, metalOffscreenStorageBenchmark;
extern bool metalOffscreenPackedModifiers;
extern bool metalOffscreenStorageLifetimeOnly;
extern bool metalOffscreenIntermediates,metalOffscreenIntermediatesBenchmark;
extern bool metalOffscreenBoundaryProfile;
extern bool metalOffscreenPassProfile;
extern bool metalOffscreenProofProfile,metalOffscreenProofBenchmark;
extern bool metalOffscreenPlanProfile,metalOffscreenPlanBenchmark;
extern bool metalOffscreenPoolProfile,metalOffscreenPoolBenchmark;
extern bool metalOffscreenReuseResidency;
extern bool metalOffscreenCacheProfile,metalOffscreenCacheOptimized;
extern bool metalOffscreenContextLifecycle;
extern std::string metalOffscreenCachePhase;
struct MetalOffscreenAllocationStats { size_t calls=0,bytes=0; };
void metalOffscreenBeginAllocations();
MetalOffscreenAllocationStats metalOffscreenEndAllocations();
extern double metalOffscreenVideoTime;
void metalOffscreenValidatePvrLifecycle();
bool metalOffscreenQueueRender(TA_context *context);
bool metalOffscreenPumpPvr();
enum class MetalOffscreenFault { None, Cancel, BeforeCommit, BeforeEnqueue, AfterEnqueue, Feedback };
#ifdef __OBJC__
#include "rend/metal4/metal4_commands.h"
FCMetal4Queue *metalOffscreenMakeQueue(id<MTLDevice> device) API_AVAILABLE(macos(26.0));
void metalOffscreenRecreateContext() API_AVAILABLE(macos(26.0));
id<MTLTexture> metalOffscreenTakeDrawableTexture() API_AVAILABLE(macos(26.0));
// Only the next newly created wrapper is instrumented. Native work remains
// well formed; synthetic failures do not fault or reset the physical GPU.
void metalOffscreenArmCommand(MetalOffscreenFault fault,id<MTLSharedEvent> gate,bool legacyBoundaries=false,bool passDependencies=true);
FCMetal4Commands *metalOffscreenLastCommand() API_AVAILABLE(macos(26.0));
void metalOffscreenClearCommandHook();
#endif
