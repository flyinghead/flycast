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
static int duplicateValidation()
{
    for(unsigned distinct : {1u,2u,4u,5u}) {
        metal_sync::PassHazards h;h.begin();h.end(true);
        std::array<Access,60> old{};for(size_t i=0;i<old.size();++i)old[i]={1+i,0,16,true};
        h.begin();h.before(1,old.data(),old.size());h.end(false);
        std::array<Access,64> duplicate{};
        for(size_t i=0;i<duplicate.size();++i)duplicate[i]={61+i%distinct,0,16,bool(i&1),false};
        h.begin();const auto barrier=h.before(2,duplicate.data(),duplicate.size());
        require(h.requiresRelease()==(distinct>4),"Duplicate batch inputs incorrectly consume independent history capacity");
        require(barrier.needed==(distinct>4),"Duplicate batch inserted an unnecessary global barrier or lost real overflow");
        h.end(h.requiresRelease());
        require(h.pendingCount()==(distinct>4?0u:60u+distinct),"Duplicate batch merge has an incorrect distinct range count");
    }
    std::puts("PASS duplicate batch identities count once; actual 65-record union still releases globally");
    return 0;
}
static int overlapValidation()
{
    for(size_t width : {4u,8u,16u,32u,40u,48u,64u}) {
        metal_sync::PassHazards h;
        std::array<Access,64> accesses{};
        for(size_t i=0;i<width;++i)accesses[i]={1+i,0,256,true};
        h.begin();h.end(true);
        h.begin();h.before(1,accesses.data(),width);h.end(false);
        h.begin();
        for(auto& access:accesses)access.write=false;
        for(unsigned draw=0;draw<32;++draw) {
            const auto acquired=h.before(2,accesses.data(),width);
            require(acquired.after==(draw==0?1u:0u),"Overlapping histories inserted an unnecessary global consumer barrier");
            require(!h.requiresRelease(),"Overlapping histories count one allocation twice toward the capacity limit");
        }
        h.end(false);
        require(h.pendingCount()==width,"Overlapping history changed its distinct allocation count");
    }
    // The cached proof must contain both prior reader and writer stages; a
    // binding may change access mode, stage or alias visibility within a pass.
    metal_sync::PassHazards h;h.begin();h.end(true);
    Access write{1,0,32,true},read{1,8,24,false},alias=read;alias.alias=true;
    h.begin();h.before(1,&write,1);h.end(false);
    h.begin();h.before(2,&read,1);h.end(false);
    h.begin();
    require(h.before(4,&read,1).after==1,"Cached read lost its prior writer");
    read.write=true;
    require(h.before(4,&read,1).after==2,"Read-to-write rebind lost its cached prior readers");
    require(h.before(4,&alias,1).after==1,"Alias rebind reused Device-only visibility");
    require(h.before(8,&read,1).after==3,"Consumer-stage change reused another stage's acquisition");
    h.end(false);
    h.begin();read.write=false;
    require(h.before(16,&read,1).after==13,"New encoder reused a stale dependency proof");
    h.end(false);
    h.reset();h.begin();h.end(true);
    std::array<Access,40> old{},current{};
    for(size_t i=0;i<40;++i){old[i]={1+i,0,16,true};current[i]={21+i,0,16,false};}
    h.begin();h.before(1,old.data(),old.size());h.end(false);
    h.begin();h.before(2,current.data(),current.size());
    require(!h.requiresRelease(),"Partially shared histories counted 80 records instead of their 60-record union");
    std::array<Access,5> extra{};for(size_t i=0;i<5;++i)extra[i]={61+i,0,16,true};
    require(h.before(2,extra.data(),extra.size()).needed && h.requiresRelease(),"Real union overflow lost its conservative boundary");
    h.end(true);require(h.pendingCount()==0,"Overflow release retained incomplete history");
    std::puts("PASS repeated overlapping histories through 64 allocations retain scoped barriers without false capacity releases");
    return 0;
}
// Independent per-byte stage-clock oracle for the batch corpus. It does not
// model the planner's records, capacity accounting or dependency-mask cache.
class BatchClockOracle {
    using Clock=std::array<uint32_t,3>;
    struct Cell {Clock reads{},writes{};};
    std::array<Cell,73*32> prior{},current{};
    Clock priorClock{},currentClock{},global{},priorUnknown{},currentUnknown{};
    std::array<Clock,3> acquired{};
public:
    void begin(){current=prior;currentClock=priorClock;currentUnknown=priorUnknown;acquired={};}
    void operation(uint64_t stages,const Access *inputs,size_t count,bool opaque,metal_sync::Barrier barrier) {
        if(!stages)return;
        bool unknown=opaque || count>64 || (count && !inputs),active=unknown;
        if(inputs && count<=64)for(size_t i=0;i<count;++i) {
            const auto a=metal_sync::passAccess(inputs[i]);if(a.begin==a.end)continue;
            active=true;unknown|=!a.allocation || a.begin>a.end;
        }
        for(unsigned c=0;c<3;++c)if(stages&(uint64_t(1)<<c)) {
            if(barrier.needed)for(unsigned p=0;p<3;++p)if(barrier.after&(uint64_t(1)<<p))acquired[c][p]=priorClock[p];
            if(!active)continue;
            for(unsigned p=0;p<3;++p) {
                const auto covered=std::max(global[p],acquired[c][p]);
                require((unknown?priorClock[p]:priorUnknown[p])<=covered,"Batch lacks a happens-before path for opaque memory");
            }
            ++currentClock[c];
            if(unknown){currentUnknown[c]=currentClock[c];continue;}
            for(size_t i=0;i<count;++i) {
                const auto a=metal_sync::passAccess(inputs[i]);if(a.begin==a.end)continue;
                require(a.allocation<73 && a.end<=32,"Batch oracle input left its byte-address universe");
                for(size_t byte=a.begin;byte<a.end;++byte) {
                    const auto index=a.allocation*32+byte;const auto& old=prior[index];
                    for(unsigned p=0;p<3;++p) {
                        const auto needed=std::max(old.writes[p],a.write?old.reads[p]:0u);
                        require(needed<=std::max(global[p],acquired[c][p]),"Batch RAW/WAR/WAW lacks a per-byte stage-clock path");
                    }
                    if(a.write)current[index].writes[c]=currentClock[c];else current[index].reads[c]=currentClock[c];
                }
            }
        }
    }
    void end(bool published){prior=current;priorClock=currentClock;priorUnknown=currentUnknown;if(published)global=currentClock;}
};
static int batchValidation()
{
    uint32_t random=0x48415a44;
    auto next=[&]{random^=random<<13;random^=random>>17;random^=random<<5;return random;};
    uint64_t total=1469598103934665603ULL;
    for(unsigned trial=0;trial<2000;++trial) {
        metal_sync::PassHazards h;BatchClockOracle oracle;uint64_t digest=1469598103934665603ULL;
        auto mix=[&](uint64_t value){digest=(digest^value)*1099511628211ULL;};
        for(unsigned pass=0;pass<32;++pass) {
            h.begin();oracle.begin();
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
                oracle.operation(stage,supplied,count,opaque,barrier);
                mix(barrier.after);mix(barrier.needed);mix(barrier.alias);mix(h.requiresRelease());
            }
            const bool release=h.requiresRelease();h.end(release);oracle.end(release);mix(h.pendingCount());
            require(h.pendingCount()<=64,"Batch exceeded pass-history capacity");
        }
        std::printf("BATCH %u %016llx\n",trial,(unsigned long long)digest);
        total=(total^digest)*1099511628211ULL;
    }
    std::printf("PASS 192000 batch operations satisfy independent per-byte stage clocks; digest=%016llx (not an equivalence claim to historical conservative decisions)\n",(unsigned long long)total);
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
    if(argc==2 && std::strcmp(argv[1],"--duplicate-validation")==0)return duplicateValidation();
    if(argc==2 && std::strcmp(argv[1],"--overlap-validation")==0)return overlapValidation();
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
