// SPDX-License-Identifier: GPL-2.0-or-later
#include "rend/metal4/metal4_preload_budget.h"
#include <atomic>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <stdexcept>
#include <thread>
#include <vector>
static thread_local int failAllocation=-1;
void *operator new(std::size_t bytes) {
	if(failAllocation==0)throw std::bad_alloc();
	if(failAllocation>0)--failAllocation;
	if(void *p=std::malloc(bytes?bytes:1))return p;
	throw std::bad_alloc();
}
void operator delete(void *p) noexcept {std::free(p);}
void operator delete(void *p,std::size_t) noexcept {std::free(p);}
static void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
int main(){try{
	using metal_preload::Budget;
	auto budget=std::make_shared<Budget>(100,2);
	require(!budget->reserve(0) && !budget->reserve(101),"Invalid admission accepted");
	auto a=budget->reserve(60),b=budget->reserve(40);require(a && b && !budget->reserve(1),"Byte budget not enforced");
	require(!a->resize(61) && !a->resize(0) && a->resize(30),"Resize rollback/shrink failed");
	require(b->resize(70) && !budget->reserve(1),"Resize charge or image count limit failed");
	auto pinned=b;b.reset();a.reset();require(budget->usage().bytes==70 && budget->usage().images==1,"Shared ticket released early");
	pinned.reset();require(!budget->usage().bytes && !budget->usage().images,"Final owner did not release");
	for(int fail:{0,1}) {
		failAllocation=fail;bool caught=false;try{auto ignored=budget->reserve(1);}catch(const std::bad_alloc&){caught=true;}
		failAllocation=-1;require(caught && !budget->usage().bytes && !budget->usage().images,"Allocation failure leaked charge/deadlocked");
	}
	auto noImages=std::make_shared<Budget>(100,0);require(!noImages->reserve(1),"Zero image capacity accepted allocation");
	auto empty=std::make_shared<Budget>(0,10);require(!empty->reserve(1),"Zero byte capacity accepted allocation");
	auto wide=std::make_shared<Budget>(UINT64_MAX,UINT64_MAX);auto full=wide->reserve(UINT64_MAX);
	require(full && !wide->reserve(1) && full->resize(UINT64_MAX-1),"Overflow boundary failed");
	auto one=wide->reserve(1);require(one && !wide->reserve(1),"Overflow boundary used wrapped arithmetic");full.reset();one.reset();
	// Independent vector sum oracle, including failed growth and count limits.
	uint64_t rng=1234567,checks=0;std::array<std::shared_ptr<Budget::Ticket>,16> tickets;std::array<uint64_t,16> sizes{};
	budget=std::make_shared<Budget>(256,8);
	for(unsigned step=0;step<100000;++step) {
		rng=rng*6364136223846793005ull+1;const unsigned slot=(rng>>32)%tickets.size();const uint64_t bytes=1+((rng>>40)%100);
		uint64_t sum=0,count=0;for(auto size:sizes){sum+=size;count+=size!=0;}
		switch((rng>>12)%3) {
		case 0:if(!tickets[slot]){const bool expected=count<8 && bytes<=256-sum;tickets[slot]=budget->reserve(bytes);require(bool(tickets[slot])==expected,"Admission differs from independent sum oracle");if(expected)sizes[slot]=bytes;}break;
		case 1:if(tickets[slot]){const bool expected=bytes<=256-(sum-sizes[slot]);require(tickets[slot]->resize(bytes)==expected,"Resize differs from independent sum oracle");if(expected)sizes[slot]=bytes;}break;
		case 2:tickets[slot].reset();sizes[slot]=0;break;
		}
		sum=count=0;for(auto size:sizes){sum+=size;count+=size!=0;}auto actual=budget->usage();require(actual.bytes==sum && actual.images==count,"Charge differs from independent sum oracle");++checks;
	}
	for(auto& ticket:tickets)ticket.reset();
	auto concurrent=std::make_shared<Budget>(1024,16);std::atomic<bool> invalid{false};std::vector<std::thread> threads;
	for(unsigned thread=0;thread<8;++thread)threads.emplace_back([&,thread]{
		std::array<std::shared_ptr<Budget::Ticket>,8> held;uint64_t seed=thread+1;
		for(unsigned i=0;i<10000;++i){seed=seed*6364136223846793005ull+1;auto& ticket=held[(seed>>32)%held.size()];ticket.reset();ticket=concurrent->reserve(1+(seed%128));if(ticket && (i&1))ticket->resize(1+(seed%256));auto state=concurrent->usage();if(state.bytes>1024 || state.images>16)invalid=true;}
	});
	for(auto& thread:threads)thread.join();require(!invalid && !concurrent->usage().bytes && !concurrent->usage().images,"Concurrent reservation/release exceeded quota or leaked");
	std::printf("PASS preload budget: %llu independent sum-oracle steps, 80000 concurrent operations, byte/image caps, overflow, resize, shared ownership and both allocation failures\n",(unsigned long long)checks);return 0;
}catch(const std::exception& e){failAllocation=-1;std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
