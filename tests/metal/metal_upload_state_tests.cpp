// SPDX-License-Identifier: GPL-2.0-or-later
#include "rend/metal4/metal4_upload_state.h"
#include "rend/metal4/metal4_upload_scope.h"
#include <array>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
static void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
int main(){try{
	using namespace metal_upload;
	for(auto outcome:{Phase::Ready,Phase::Failed,Phase::Canceled}) {
		State state(17);require(state.phase()==Phase::Pending && state.usableOn(17) && !state.usableOn(18),"Pending upload escaped its producer queue");
		require(outcome==Phase::Canceled?state.cancel():state.complete(outcome==Phase::Ready),"First retirement transition failed");
		require(state.phase()==outcome && !state.complete(true) && !state.complete(false) && !state.cancel(),"Terminal state changed on late callback/cancellation");
		require(state.usableOn(17)==(outcome==Phase::Ready) && state.usableOn(18)==(outcome==Phase::Ready),"Terminal upload visibility wrong");
	}
	State unbound;require(!unbound.usableOn(0) && !unbound.usableOn(1),"Unbound pending state admitted");
	{
		auto failed=std::make_shared<std::atomic<bool>>(false);
		cancelPending({},failed);require(!failed->load(),"Empty batch reported cancellation");
		auto state=std::make_shared<State>(17),alias=state;
		cancelPending(state,failed);
		require(alias->phase()==Phase::Canceled && failed->load() && !alias->usableOn(17),"Owner cancellation left a retained pending image usable");
		failed->store(false);cancelPending(state,failed);
		require(!failed->load(),"Repeated cancellation reissued an already consumed failure");
		require(!state->complete(true),"Late successful feedback revived canceled upload");
		failed->store(false);auto ready=std::make_shared<State>(17);ready->complete(true);cancelPending(ready,failed);
		require(ready->phase()==Phase::Ready && !failed->load(),"Cancellation changed a successfully completed upload");
	}
	struct ScopeProbe {
		unsigned depth=0,ended=0,canceled=0;bool fail=false,deferred=false;
		void beginTextureUpdates(){++depth;}
		void endTextureUpdates(bool defer){--depth;++ended;deferred=defer;if(fail)throw std::runtime_error("submit");}
		void cancelTextureUpdates()noexcept{--depth;++canceled;}
	} probe;
	{Scope scope(&probe,true);}require(!probe.depth && probe.ended==1 && probe.deferred,"Normal deferred scope did not finish");
	probe.fail=true;bool caught=false;try{Scope scope(&probe);}catch(const std::runtime_error& e){caught=std::string(e.what())=="submit";}
	require(caught && !probe.depth && probe.ended==2 && !probe.canceled,"Normal-exit submit exception was swallowed/terminated");
	caught=false;try{Scope scope(&probe);throw std::runtime_error("prepare");}catch(const std::runtime_error& e){caught=std::string(e.what())=="prepare";}
	require(caught && !probe.depth && probe.ended==2 && probe.canceled==1,"Unwinding scope attempted another submission");
	probe.fail=false;{Scope parent(&probe);try{Scope child(&probe);throw std::runtime_error("nested");}catch(const std::exception&){}require(probe.depth==1,"Nested cancellation unbalanced depth");}
	require(!probe.depth && probe.ended==3 && probe.canceled==2,"Parent scope did not recover after nested failure");
	constexpr unsigned Count=4096,Rounds=32;
	for(unsigned round=0;round<Rounds;++round) {
		std::vector<std::unique_ptr<State>> states;states.reserve(Count);
		std::vector<std::atomic<unsigned>> wins(Count);for(unsigned i=0;i<Count;++i){states.push_back(std::make_unique<State>(17));wins[i]=0;}
		std::atomic<bool> start{false};
		std::thread complete([&]{while(!start.load(std::memory_order_acquire))std::this_thread::yield();for(unsigned i=0;i<Count;++i)if(states[i]->complete((i+round)&1))++wins[i];});
		std::thread cancel([&]{while(!start.load(std::memory_order_acquire))std::this_thread::yield();for(unsigned i=0;i<Count;++i)if(states[i]->cancel())++wins[i];});
		start.store(true,std::memory_order_release);complete.join();cancel.join();
		for(unsigned i=0;i<Count;++i){auto p=states[i]->phase();require(wins[i]==1 && p!=Phase::Pending,"Retirement race did not select exactly one terminal transition");require(states[i]->usableOn(18)==(p==Phase::Ready),"Retirement race exposed invalid image to another queue");}
	}
	std::printf("PASS upload state: producer-only pending visibility, terminal stability, immediate owner cancellation with retained aliases, unbound rejection, %u completion/cancel races with exactly one winner; scope success/deferral, normal-exit submit exception, unwind-only cancellation and balanced nested depth\n",Count*Rounds);return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
