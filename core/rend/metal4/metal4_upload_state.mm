// SPDX-License-Identifier: GPL-2.0-or-later
#if defined(USE_METAL) || defined(FLYCAST_METAL_STANDALONE)
#include "metal4_upload_state.h"
#include <chrono>
#include <stdexcept>

namespace metal_upload {
namespace {
struct Retirement {
	std::shared_ptr<State> state;
	FailureFlag failed;
	dispatch_semaphore_t available;
	std::atomic<bool> retired{false};
	bool acquired=false;
	Retirement(std::shared_ptr<State> state,FailureFlag failed,dispatch_semaphore_t available)
		:state(std::move(state)),failed(std::move(failed)),available(available) {}
	void complete(bool success) noexcept {
		if(retired.exchange(true))return;
		if(state && state->complete(success) && !success && failed)
			failed->store(true,std::memory_order_release);
		if(acquired)dispatch_semaphore_signal(available);
	}
	~Retirement() {
		if(retired.exchange(true))return;
		const bool canceled=state && state->cancel();
		if(acquired){if(canceled && failed)failed->store(true,std::memory_order_release);dispatch_semaphore_signal(available);}
	}
};
API_AVAILABLE(macos(26.0))
void reserve(FCMetal4Commands *command,dispatch_semaphore_t available,const std::shared_ptr<State>& state,
	const FailureFlag& failed,double *waitMs,bool alreadyReserved=false)
{
	if(!command || command.status!=FCMetal4StatusEncoding || !command.commandQueue || !command.native || !available)
		throw std::invalid_argument("Invalid Metal upload lifetime observer");
	auto retirement=std::make_shared<Retirement>(state,failed,available);
	// Install all allocating callback state before taking the staging lease.
	// Encoding/commit belongs to this producer; feedback cannot run here yet.
	[command addCompletedHandler:^(FCMetal4Commands *done){retirement->complete(done.status==FCMetal4StatusCompleted);}];
	if(alreadyReserved){retirement->acquired=true;return;}
	const auto start=waitMs?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
	dispatch_semaphore_wait(available,DISPATCH_TIME_FOREVER);retirement->acquired=true;
	if(waitMs)*waitMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
}
}
std::shared_ptr<State> watch(FCMetal4Commands *command,dispatch_semaphore_t available,
	const FailureFlag& failed,double *waitMs)
{
	if(!failed)throw std::invalid_argument("Missing Metal upload failure flag");
	auto state=std::make_shared<State>(reinterpret_cast<uintptr_t>((__bridge void *)command.commandQueue));
	reserve(command,available,state,failed,waitMs);return state;
}
void reserveSlot(FCMetal4Commands *command,dispatch_semaphore_t available)
{
	reserve(command,available,{},{},nullptr);
}
void adoptSlot(FCMetal4Commands *command,dispatch_semaphore_t available)
{
	reserve(command,available,{},{},nullptr,true);
}
}
#endif
