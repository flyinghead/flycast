// SPDX-License-Identifier: GPL-2.0-or-later
#include "rend/metal4/metal4_transfer_hazards.h"
#include <array>
#include <bitset>
#include <cstdio>
#include <stdexcept>
#include <vector>
#include <algorithm>
using metal_sync::Access;
using metal_sync::TransferHazards;
static void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
struct Operation {uint64_t stage;std::array<Access,2> accesses;size_t size;bool opaque;};
static bool uncertain(const Operation& op) {
    if(op.opaque)return true;
    for(size_t i=0;i<op.size;++i)if(!op.accesses[i].allocation || op.accesses[i].begin>op.accesses[i].end)return true;
    return false;
}
static bool active(const Operation& op) {
    if(uncertain(op))return true;
    for(size_t i=0;i<op.size;++i)if(op.accesses[i].begin!=op.accesses[i].end)return true;
    return false;
}
static bool dependent(const Operation& a,const Operation& b) {
    if(!active(a) || !active(b))return false;
    if(uncertain(a) || uncertain(b))return true;
    for(size_t i=0;i<a.size;++i)for(size_t j=0;j<b.size;++j) {
        const auto& x=a.accesses[i];const auto& y=b.accesses[j];
        if(x.begin==x.end || y.begin==y.end)continue;
        if(x.allocation==y.allocation && (x.write || y.write)
            && std::max(x.begin,y.begin)<std::min(x.end,y.end))return true;
    }
    return false;
}
int main() {
    try {
        TransferHazards h;const Access a{10,0,16,true},b{11,0,16,true};
        require(!h.before(1,&a,1).needed && !h.before(1,&b,1).needed,"Independent writes were serialized");
        require(h.before(1,&a,1).needed,"Write-after-write hazard missed");
        h.reset();Access x{10,0,16,false},y{10,16,32,true};
        require(!h.before(1,&x,1).needed && !h.before(1,&x,1).needed && !h.before(1,&y,1).needed,"Read/read or adjacent ranges serialized");
        y.begin=15;require(h.before(1,&y,1).needed,"Partially overlapping write-after-read missed");
        h.reset();require(!h.before(2,nullptr,0,true).needed,"Initial dispatch requires no intra barrier");
        Access empty{10,0,0,true};require(!h.before(1,&empty,1).needed,"Empty fill consumed a dependency");
        const auto afterEmpty=h.before(2,nullptr,0,true);require(afterEmpty.needed && afterEmpty.after==2,"Empty blit erased dispatch/dispatch dependency");
        h.reset();Access alias{0,0,16,true,true};h.before(1,&a,1);auto barrier=h.before(1,&alias,1);
        require(barrier.needed && barrier.alias,"Unknown alias did not require cache-alias visibility");
        barrier=h.before(2,nullptr,0,true);require(barrier.needed && barrier.alias && barrier.after==1,"Alias producer lost its visibility");
        h.reset();h.before(1,nullptr,65,false);require(h.before(2,nullptr,0,true).needed,"Oversized/invalid access description was trusted");
        // Independent stage-clock oracle: barriers add edges from every earlier
        // operation in the producer stages to all later consumer-stage work.
        // Every conflicting operation pair must have a transitive happens-before
        // edge. This oracle never shares the planner's bounded pending epochs.
        uint32_t random=0xa341316c;auto next=[&]{random^=random<<13;random^=random>>17;random^=random<<5;return random;};
        size_t operations=0,hazards=0,barriers=0;
        for(unsigned trial=0;trial<4000;++trial) {
            h.reset();std::vector<Operation> history;
            std::array<std::bitset<128>,2> priorByStage{},fences{};
            for(unsigned step=0;step<128;++step) {
                Operation op{};op.stage=(next()&3)==0?2:1;op.opaque=op.stage==2;op.size=op.opaque?0:1+(next()&1);
                for(size_t i=0;i<op.size;++i) {
                    auto& access=op.accesses[i];access.allocation=1+next()%24;access.begin=next()%64;access.end=access.begin+next()%24;access.write=(next()&1)!=0;
                    if(next()%31==0){access.allocation=0;access.alias=true;}
                }
                const auto barrier=h.before(op.stage,op.accesses.data(),op.size,op.opaque);
                const unsigned stage=op.stage==1?0:1;
                if(barrier.needed) {
                    ++barriers;require(barrier.after && !(barrier.after&~uint64_t(3)),"Invalid producer stage mask");
                    for(unsigned producer=0;producer<2;++producer)if(barrier.after&(1u<<producer))fences[stage]|=priorByStage[producer];
                }
                const auto predecessors=fences[stage];
                for(size_t old=0;old<history.size();++old)if(dependent(history[old],op)) {
                    ++hazards;require(predecessors.test(old),"A conflicting pair has no stage-barrier happens-before path");
                }
                if(active(op)){priorByStage[stage]|=predecessors;priorByStage[stage].set(step);}
                history.push_back(op);++operations;require(h.pendingCount()<=64,"Hazard history exceeded fixed capacity");
            }
        }
        h.reset();unsigned capacityBarriers=0;
        for(uintptr_t i=1;i<=1024;++i) {
            Access independent{i,0,8,true};capacityBarriers+=h.before(1,&independent,1).needed;
        }
        require(capacityBarriers==15,"Bounded history fallback differs");
        std::printf("PASS transfer hazards: %zu randomized operations, %zu dependency edges ordered by %zu stage barriers; RAW/WAR/WAW, read/read, empty operations, alias/opaque fallback, reset, 1024 independent writes/15 capacity barriers\n",operations,hazards,barriers);
        return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
