// SPDX-License-Identifier: GPL-2.0-or-later
#include "rend/metal4/metal4_pass_hazards.h"
#include <bitset>
#include <cstdio>
#include <stdexcept>
#include <vector>
#include <chrono>
#include <cstring>
#include <algorithm>
using metal_sync::Access;
static void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
struct Op {uint64_t stage;Access access;bool opaque;unsigned pass;};
static bool conflict(const Op& a,const Op& b) {
    if(a.pass==b.pass)return false;
    if(a.opaque || b.opaque)return true;
    return a.access.allocation==b.access.allocation && (a.access.write || b.access.write)
        && a.access.begin<b.access.end && b.access.begin<a.access.end;
}
static volatile uint64_t profileSink;
static int batchValidation()
{
    uint32_t random=0x48415a44;
    auto next=[&]{random^=random<<13;random^=random>>17;random^=random<<5;return random;};
    uint64_t total=1469598103934665603ULL;
    for(unsigned trial=0;trial<2000;++trial) {
        metal_sync::PassHazards h;uint64_t digest=1469598103934665603ULL;
        auto mix=[&](uint64_t value){digest=(digest^value)*1099511628211ULL;};
        for(unsigned pass=0;pass<32;++pass) {
            h.begin();
            for(unsigned step=0;step<3;++step) {
                std::array<Access,66> accesses{};const size_t count=next()%67;
                for(size_t i=0;i<count;++i) {
                    accesses[i]={next()%73,next()%16,16+next()%16,bool(next()%2),bool(next()%11==0)};
                    if(i && next()%4==0)accesses[i]=accesses[next()%i];
                    if(next()%13==0)accesses[i].end=accesses[i].begin;
                    if(next()%29==0)accesses[i].end=0;
                    if(next()%7==0){accesses[i].passAllocation=next()%73;accesses[i].passAlias=bool(next()%2);}
                }
                uint64_t stage=uint64_t(1)<<(next()%3);
                if(next()%19==0)stage=~uint64_t(0);
                if(next()%23==0)stage=0;
                const Access *supplied=next()%43==0?nullptr:accesses.data();
                const bool opaque=next()%37==0;
                const auto barrier=h.before(stage,supplied,count,opaque);
                mix(barrier.after);mix(barrier.needed);mix(barrier.alias);mix(h.requiresRelease());
            }
            const bool release=h.requiresRelease();h.end(release);mix(h.pendingCount());
            require(h.pendingCount()<=64,"Batch exceeded pass-history capacity");
        }
        std::printf("BATCH %u %016llx\n",trial,(unsigned long long)digest);
        total=(total^digest)*1099511628211ULL;
    }
    // Captured from the original double-search implementation. Covers every
    // returned stage mask, alias flag, release decision and pending-count value.
    require(total==0xbe307884e2d3cd23ULL,"Batch barrier decisions differ from the original planner");
    std::puts("PASS 192000 batch operations match original barrier decisions");
    return 0;
}
static int profile()
{
    // Fixed shader-binding patterns, including duplicate and empty ranges.
    // All setup is outside the timed region. Report a digest of every barrier,
    // release and history size so two implementations can be compared exactly.
    for(unsigned pattern=0;pattern<6;++pattern) {
        std::array<std::array<Access,64>,8> inputs{};
        const size_t width=pattern==0?4:pattern==1?8:pattern==2?16:pattern==3?32:64;
        for(size_t bank=0;bank<inputs.size();++bank)for(size_t i=0;i<width;++i) {
            auto& a=inputs[bank][i];
            a={1+(pattern==4?i%4:i)+(pattern==3?bank*8:0),0,256,(i+bank)%5==0,(i+bank)%13==0};
            if(pattern==5 && i%3==0)a.end=0;
        }
        std::vector<double> times;uint64_t expected=0;
        for(unsigned run=0;run<21;++run) {
            metal_sync::PassHazards h;uint64_t digest=1469598103934665603ULL;
            auto mix=[&](uint64_t value){digest=(digest^value)*1099511628211ULL;};
            const auto start=std::chrono::steady_clock::now();
            for(unsigned pass=0;pass<128;++pass) {
                h.begin();
                for(unsigned draw=0;draw<32;++draw) {
                    const auto barrier=h.before(uint64_t(1)<<(draw%3),inputs[pass%8].data(),width);
                    mix(barrier.after);mix(unsigned(barrier.needed)|(unsigned(barrier.alias)<<1));
                }
                const bool release=h.requiresRelease();mix(release);h.end(release);mix(h.pendingCount());
            }
            const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
            profileSink=digest;
            if(run==0)expected=digest;else require(digest==expected,"Unstable barrier profile");
            if(run>=4)times.push_back(us);
        }
        std::sort(times.begin(),times.end());
        std::printf("PROFILE pass hazards pattern=%u accesses=%zu calls=4096 median_us=%.3f digest=%016llx samples=%zu\n",
            pattern,width,times[times.size()/2],(unsigned long long)expected,times.size());
    }
    return 0;
}
int main(int argc,char **argv){try {
    if(argc==2 && std::strcmp(argv[1],"--profile")==0)return profile();
    if(argc==2 && std::strcmp(argv[1],"--batch-validation")==0)return batchValidation();
    static_assert(sizeof(metal_sync::PassBindings)<=1100,"Binding cache is no longer compact");
    metal_sync::PassBindings bindings;std::array<Access,64> collected;
    bindings.set(0,{1,0,16,false});bindings.set(32,{2,0,16,true});bindings.set(64,{3,0,16,false});
    require(bindings.collect(collected.data(),0)==1 && collected[0].allocation==1,"Vertex binding classification differs");
    require(bindings.collect(collected.data(),1)==2,"Fragment texture/buffer classification differs");
    bindings.set(32,{},false);bindings.set(64,{4,0,16,true});
    require(bindings.collect(collected.data(),1)==1 && collected[0].allocation==4 && collected[0].write,"Unbind/rebind retained stale resource access");
    for(unsigned i=0;i<200;++i)bindings.set(i,{100+i,0,16,false});
    require(bindings.opaque(),"Excess shader bindings bypassed conservative fallback");
    metal_sync::PassHazards h;Access a{2,0,16,true},b{3,0,16,true},read=a;read.write=false;
    h.begin();require(h.before(1,&a,1).needed && h.requiresRelease(),"First pass did not acquire/release unknown queue prefix");h.end(true);
    h.begin();require(!h.before(1,&a,1).needed,"Published prefix was not retired");h.end(false);
    h.begin();require(!h.before(2,&b,1).needed,"Independent pass was serialized");h.end(false);
    h.begin();require(h.before(2,&read,1).after==1,"RAW producer stage was lost");h.end(false);
    h.begin();require(h.before(4,&read,1).after==1,"Earlier writer was forgotten after another pass's consumer barrier");h.end(false);
    h.begin();require(h.before(1,&a,1).after==7,"WAR/WAW reader stages were lost");h.end(false);
    h.begin();require(h.before(2,nullptr,0,true).needed && h.requiresRelease(),"Opaque pass lost global boundary");h.end(true);
    h.begin();require(!h.before(1,&read,1).needed,"Opaque producer release failed to retire preceding records");h.end(false);
    h.reset();h.begin();h.end(true);
    unsigned fallbacks=0;
    for(uintptr_t i=1;i<=1024;++i){Access next{i,0,16,true};h.begin();h.before(1,&next,1);const bool release=h.requiresRelease();fallbacks+=release;h.end(release);require(h.pendingCount()<=64,"Unbounded pass history");}
    require(fallbacks==15,"Capacity fallback differs");
    h.reset();h.begin();h.end(true);
    Access ownedA{1,0,16,true,true,100,false},ownedB{1,0,16,true,true,101,false};
    h.begin();h.before(1,&ownedA,1);h.end(false);
    h.begin();require(!h.before(2,&ownedB,1).needed,"Distinct proven allocations share a false queue alias domain");h.end(false);
    h.begin();Access imported=ownedA;imported.passAllocation=0;imported.passAlias=true;
    require(h.before(4,&imported,1).needed && h.requiresRelease(),"Unknown import bypassed owned allocation hazards");h.end(true);
    h.begin();h.before(1,&a,1);h.end(false);
    h.begin();auto acquire=h.before(2,&read,1);require(acquire.needed && !acquire.alias,"Device-only acquisition fixture differs");
    read.alias=true;acquire=h.before(2,&read,1);require(acquire.needed && acquire.alias,"Alias visibility was incorrectly deduplicated after a Device-only barrier");h.end(false);
    // Independent encoder-local consumer/global producer stage-clock oracle.
    // Only operations from earlier encoders participate in queue dependencies.
    uint32_t state=0x43f795a1;auto next=[&]{state^=state<<13;state^=state>>17;state^=state<<5;return state;};
    size_t operations=0,edges=0,consumers=0,releases=0;
    for(unsigned trial=0;trial<2000;++trial) {
        h.reset();std::vector<Op> history;
        std::array<std::bitset<1536>,3> priorByStage{};
        std::bitset<1536> global;
        for(unsigned pass=0;pass<128;++pass) {
            h.begin();std::array<std::bitset<1536>,3> acquired{};auto current=priorByStage;
            for(unsigned step=0;step<4;++step) {
                const unsigned stage=next()%3;Op op{uint64_t(1)<<stage,{1+next()%32,next()%48,0,bool(next()&1),false},next()%43==0,pass};
                if(next()%13==0)op.stage=~uint64_t(0);
                op.access.end=op.access.begin+1+next()%32;
                if(next()%37==0){op.access.allocation=0;op.access.alias=true;op.opaque=true;}
                const auto barrier=h.before(op.stage,&op.access,1,op.opaque);
                if(barrier.needed){++consumers;for(unsigned c=0;c<3;++c)if(op.stage&(uint64_t(1)<<c))for(unsigned p=0;p<3;++p)if(barrier.after&(uint64_t(1)<<p))acquired[c]|=priorByStage[p];}
                for(unsigned c=0;c<3;++c)if(op.stage&(uint64_t(1)<<c)) {
                    auto predecessors=global|acquired[c];
                    for(size_t old=0;old<history.size();++old)if(conflict(history[old],op))for(unsigned p=0;p<3;++p)if(history[old].stage&(uint64_t(1)<<p)) {++edges;require(predecessors.test(old*3+p),"Inter-pass conflict lacks a stage-specific happens-before path");}
                    current[c]|=predecessors;current[c].set(history.size()*3+c);
                }
                history.push_back(op);++operations;
            }
            const bool release=h.requiresRelease();h.end(release);priorByStage=current;
            if(release){++releases;for(const auto& stage:priorByStage)global|=stage;}
            require(h.pendingCount()<=64,"Pass history exceeds capacity");
        }
    }
    std::printf("PASS pass hazards: %zu operations, %zu inter-encoder edges, %zu consumers, %zu global releases; RAW/WAR/WAW, independent passes, scoped-consumer history retention, opaque/prefix/capacity fallback\n",operations,edges,consumers,releases);
    return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
