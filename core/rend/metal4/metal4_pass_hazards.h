// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "metal4_transfer_hazards.h"

namespace metal_sync {
inline Access passAccess(Access access) noexcept {
    if(access.passAllocation!=std::numeric_limits<uintptr_t>::max()) {
        access.allocation=access.passAllocation;access.alias=access.passAlias;
    }
    return access;
}
// Direct shader bindings cover whole allocation roots. Cache only the actual
// sparse bindings, not 190 strong resource references or full range records.
// Resources themselves remain in the submission's completion-owned residency.
class PassBindings {
    struct Bound {uintptr_t allocation;uint16_t slot;bool write,alias;};
    Bound entries[64];
    size_t count=0;
    bool overflow=false;
public:
    void set(unsigned slot,Access access,bool bound=true) noexcept {
        if(overflow)return;
        size_t index=0;while(index<count && entries[index].slot!=slot)++index;
        if(!bound){if(index<count)entries[index]=entries[--count];return;}
        if(index==count){if(count==64){overflow=true;return;}++count;}
        access=passAccess(access);entries[index]={access.allocation,uint16_t(slot),access.write,access.alias};
    }
    // Vertex slots 0..30; fragment buffers 32..62 and textures 64..191.
    // Mode 0 collects vertex, 1 fragment, 2 every compute binding.
    size_t collect(Access *out,unsigned mode) const noexcept {
        size_t size=0;
        for(size_t i=0;i<count;++i)if(mode==2 || (entries[i].slot<32)==(mode==0)) {
            const auto& b=entries[i];out[size++]={b.allocation,0,std::numeric_limits<uint64_t>::max(),b.write,b.alias};
        }
        return size;
    }
    bool opaque() const noexcept {return overflow;}
};
// Queue-consumer barriers apply only to their current encoder. Keep earlier
// accesses until a global producer release, rather than forgetting an epoch
// after a consumer barrier that cannot order an unrelated later encoder.
class PassHazards {
    static constexpr size_t Capacity=64;
    struct Record {Access access;uint64_t reads=0,writes=0;};
    struct Dependency {uint64_t reads,writes;bool alias,shared;};
    std::array<Record,Capacity> prior{},current{};
    // Prior history is immutable until end(). Repeated bindings in this
    // encoder may reuse both read/write dependency proofs, even if the access
    // mode or consumer stage changes between draws.
    std::array<Dependency,Capacity> currentDependencies;
    std::array<uint64_t,64> acquired{},acquiredAlias{};
    size_t priorCount=0,currentCount=0,currentNewCount=0;
    bool prefix=true,release=false;
    uint64_t acquiredAll=0,acquiredAliasAll=0;
    static bool same(const Access& a,const Access& b) noexcept {
        return a.allocation==b.allocation && a.begin==b.begin && a.end==b.end;
    }
    static size_t find(const std::array<Record,Capacity>& records,size_t count,const Access& access) noexcept {
        for(size_t i=0;i<count;++i)if(same(records[i].access,access))return i;
        return count;
    }
public:
    void reset() noexcept {priorCount=currentCount=currentNewCount=0;prefix=true;release=false;acquired.fill(0);acquiredAlias.fill(0);acquiredAll=acquiredAliasAll=0;}
    void begin() noexcept {currentCount=currentNewCount=0;release=false;acquired.fill(0);acquiredAlias.fill(0);acquiredAll=acquiredAliasAll=0;}
    Barrier before(uint64_t stage,const Access *accesses,size_t size,bool opaque=false) noexcept {
        if(!stage)return {};
        bool unknown=opaque || size>Capacity || (size && !accesses),alias=false;
        uint64_t after=0;size_t additions=0;
        // Existing current-pass records never move during this call. Retain
        // their indices for the recording phase instead of scanning twice.
        // A miss still searches again there: an earlier input in this same
        // batch may have inserted a duplicate allocation/range in the meantime.
        std::array<uint8_t,Capacity> currentIndices;
        std::array<Dependency,Capacity> newDependencies;
        if(accesses && size<=Capacity)for(size_t i=0;i<size;++i) {
            const auto access=passAccess(accesses[i]);
            if(access.begin==access.end)continue;
            unknown|=!access.allocation || access.begin>access.end;alias|=access.alias;
            const size_t currentIndex=find(current,currentCount,access);
            currentIndices[i]=currentIndex==currentCount?Capacity:currentIndex;
            Dependency dependency;
            if(currentIndex!=currentCount)dependency=currentDependencies[currentIndex];
            else {
                dependency={0,0,false,false};
                for(size_t j=0;j<priorCount;++j) {
                    const auto& old=prior[j];
                    dependency.shared|=same(old.access,access);
                    if(old.access.allocation==access.allocation && old.access.begin<access.end && access.begin<old.access.end) {
                        dependency.reads|=old.reads;dependency.writes|=old.writes;dependency.alias|=old.access.alias;
                    }
                }
                newDependencies[i]=dependency;
            }
            after|=dependency.writes | (access.write?dependency.reads:0);alias|=dependency.alias;
            if(currentIndex==currentCount && !dependency.shared)++additions;
        }
        // Count the union: an existing allocation/range in both pass histories
        // still occupies only one record when end() merges them.
        if(!unknown && priorCount+currentNewCount+additions>Capacity) {
            // Several shader slots can share one new allocation/range. Only
            // resolve those duplicates when the cheap upper bound would force
            // a global release; normal draws need no extra batch search.
            const size_t available=Capacity-priorCount-currentNewCount;
            size_t unique=0;
            for(size_t i=0;i<size;++i) {
                const auto access=passAccess(accesses[i]);
                if(access.begin==access.end || currentIndices[i]!=Capacity || newDependencies[i].shared)continue;
                bool repeated=false;
                for(size_t j=0;j<i;++j)if(same(access,passAccess(accesses[j]))){repeated=true;break;}
                if(!repeated && ++unique>available){unknown=true;break;}
            }
        }
        if(unknown){release=true;alias=true;}
        if(prefix || unknown){after=~uint64_t(0);alias=true;}
        uint64_t needed=0;
        if(stage==~uint64_t(0)) {
            needed=after&~acquiredAll | (alias?after&~acquiredAliasAll:0);
            acquiredAll|=needed;if(alias)acquiredAliasAll|=needed;
        } else {
            uint64_t bits=stage;
            while(bits){const unsigned bit=__builtin_ctzll(bits);bits&=bits-1;needed|=after&~(acquired[bit]|acquiredAll) | (alias?after&~(acquiredAlias[bit]|acquiredAliasAll):0);}
            if(needed){bits=stage;while(bits){const unsigned bit=__builtin_ctzll(bits);bits&=bits-1;acquired[bit]|=needed;if(alias)acquiredAlias[bit]|=needed;}}
        }
        if(!unknown && accesses)for(size_t i=0;i<size;++i) {
            const auto access=passAccess(accesses[i]);if(access.begin==access.end)continue;
            size_t index=currentIndices[i];
            if(index==Capacity)index=find(current,currentCount,access);
            if(index==currentCount){
                currentDependencies[index]=newDependencies[i];
                current[currentCount++]={access,0,0};currentNewCount+=!newDependencies[i].shared;
            }
            auto& record=current[index];record.access.alias|=access.alias;
            if(access.write)record.writes|=stage;else record.reads|=stage;
        }
        return {needed!=0,alias,needed};
    }
    bool requiresRelease() const noexcept {return prefix || release;}
    void end(bool published) noexcept {
        if(published){priorCount=0;prefix=false;}
        else for(size_t i=0;i<currentCount;++i) {
            const auto& record=current[i];size_t index=find(prior,priorCount,record.access);
            if(index==priorCount)prior[priorCount++]=record;
            else {prior[index].reads|=record.reads;prior[index].writes|=record.writes;prior[index].access.alias|=record.access.alias;}
        }
        currentCount=0;
    }
    size_t pendingCount() const noexcept {return priorCount;}
};
}
