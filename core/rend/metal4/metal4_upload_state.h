// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <atomic>
#include <cstdint>
#include <memory>

namespace metal_upload {
enum class Phase : uint8_t { Pending, Ready, Failed, Canceled };
class State {
	std::atomic<Phase> value{Phase::Pending};
	const uintptr_t producer;
public:
	explicit State(uintptr_t producer=0):producer(producer) {}
	uintptr_t producerQueue() const noexcept { return producer; }
	Phase phase() const noexcept { return value.load(std::memory_order_acquire); }
	// Pending is usable only on the owning producer queue, after its uploads
	// have been appended/submitted in order. It never implies GPU completion.
	bool usableOn(uintptr_t consumer) const noexcept {
		auto p=phase();return p==Phase::Ready || (p==Phase::Pending && producer && producer==consumer);
	}
	bool complete(bool success) noexcept {
		auto expected=Phase::Pending;
		return value.compare_exchange_strong(expected,success?Phase::Ready:Phase::Failed,std::memory_order_acq_rel);
	}
	bool cancel() noexcept {
		auto expected=Phase::Pending;
		return value.compare_exchange_strong(expected,Phase::Canceled,std::memory_order_acq_rel);
	}
};
using FailureFlag=std::shared_ptr<std::atomic<bool>>;
// The encoding owner invalidates abandoned images immediately. Retaining an
// unsubmitted command/encoder must not keep those images adoptable. Its slot
// lease still belongs to the command observer until actual retirement.
inline void cancelPending(const std::shared_ptr<State>& state,const FailureFlag& failed) noexcept {
	if(!state)return;
	if(state->cancel() && failed)failed->store(true,std::memory_order_release);
}
}

#ifdef __OBJC__
#include "metal4_commands.h"
namespace metal_upload {
// Reserves available after allocating its observer. Exactly one of command
// feedback or unsubmitted-command destruction returns that staging-slot lease.
// No context/renderer pointer is retained by the completion callback.
API_AVAILABLE(macos(26.0))
std::shared_ptr<State> watch(FCMetal4Commands *command,dispatch_semaphore_t available,
	const FailureFlag& failed,double *waitMs=nullptr);
// Same cancellation-safe lease for presentation slots, without upload state
// or texture-cache failure flags. The callback never owns the context.
API_AVAILABLE(macos(26.0))
void reserveSlot(FCMetal4Commands *command,dispatch_semaphore_t available);
// Transfer an already reserved nonblocking display-slot lease. If setup
// throws, the caller still owns it; after return only the command retires it.
API_AVAILABLE(macos(26.0))
void adoptSlot(FCMetal4Commands *command,dispatch_semaphore_t available);
}
#endif
