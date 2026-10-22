// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace metal_sync {
// Resource identities are allocation roots, never transient view addresses.
// Unknown aliases use allocation=0 and force an opaque dependency. Independent
// texture levels use disjoint intervals only after resolving parent offsets.
struct Access {
    uintptr_t allocation=0;
    uint64_t begin=0,end=std::numeric_limits<uint64_t>::max();
    bool write=false,alias=false;
    // Queue-only identity proof. The intra-compute planner intentionally keeps
    // its original conservative CPU-visible alias domain. Imported buffers use
    // zero here; only the wrapper's allocating factories certify ownership.
    uintptr_t passAllocation=std::numeric_limits<uintptr_t>::max();
    bool passAlias=false;
};
struct Barrier {
    bool needed=false,alias=false;
    uint64_t after=0;
};
class TransferHazards {
    static constexpr size_t Capacity=64;
    std::array<Access,Capacity> pending{};
    size_t count=0;
    uint64_t stages=0;
    bool unknown=false,aliases=false;
public:
    void reset() noexcept {count=0;stages=0;unknown=false;aliases=false;}
    // An unknown dispatch may access resources through opaque shader inputs.
    // It joins the same epoch but never enables an independence proof.
    Barrier before(uint64_t nextStage,const Access *accesses,size_t size,bool opaque=false) noexcept {
        bool uncertain=opaque || size>Capacity || (size && !accesses);
        bool nextAliases=false;size_t active=0;
        if(accesses && size<=Capacity)for(size_t i=0;i<size;++i) {
            const auto& next=accesses[i];
            uncertain|=next.allocation==0 || next.begin>next.end;
            nextAliases|=next.alias;
            active+=next.begin!=next.end;
        }
        // A no-op must not consume an unknown dispatch's dependency epoch:
        // a barrier before an empty blit cannot order a later dispatch.
        if(!uncertain && !active)return {};
        bool conflict=stages && (unknown || uncertain || count+active>Capacity);
        if(!conflict && !uncertain)for(size_t i=0;i<size && !conflict;++i) {
            const auto& next=accesses[i];if(next.begin==next.end)continue;
            for(size_t j=0;j<count;++j) {
                const auto& old=pending[j];
                if(old.allocation==next.allocation && (old.write || next.write)
                    && old.begin<next.end && next.begin<old.end) {conflict=true;break;}
            }
        }
        Barrier barrier{conflict,conflict && (aliases || nextAliases),conflict ? stages : 0};
        if(conflict) {count=0;stages=0;unknown=false;aliases=false;}
        if(uncertain) {unknown=true;stages|=nextStage;aliases|=nextAliases;}
        else if(active) {
            for(size_t i=0;i<size;++i)if(accesses[i].begin!=accesses[i].end) {
                const auto& next=accesses[i];bool repeatedRead=false;
                if(!next.write)for(size_t j=0;j<count;++j) {
                    const auto& old=pending[j];
                    if(!old.write && old.allocation==next.allocation && old.begin==next.begin && old.end==next.end) {
                        repeatedRead=true;break;
                    }
                }
                if(!repeatedRead)pending[count++]=next;
            }
            stages|=nextStage;aliases|=nextAliases;
        }
        return barrier;
    }
    size_t pendingCount() const noexcept {return count;}
};
}
