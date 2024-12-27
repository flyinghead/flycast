//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLResidencySet.hpp
//
// Copyright 2020-2024 Apple Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//-------------------------------------------------------------------------------------------------------------------------------------------------------------

#pragma once

#include "MTLDefines.hpp"
#include "MTLHeaderBridge.hpp"
#include "MTLPrivate.hpp"

#include <Foundation/Foundation.hpp>

namespace MTL
{
class ResidencySetDescriptor : public NS::Copying<ResidencySetDescriptor>
{
public:
    static class ResidencySetDescriptor* alloc();

    class ResidencySetDescriptor*        init();

    NS::String*                          label() const;
    void                                 setLabel(const NS::String* label);

    NS::UInteger                         initialCapacity() const;
    void                                 setInitialCapacity(NS::UInteger initialCapacity);
};

class ResidencySet : public NS::Referencing<ResidencySet>
{
public:
    class Device* device() const;

    NS::String*   label() const;

    uint64_t      allocatedSize() const;

    void          requestResidency();

    void          endResidency();

    void          addAllocation(const class Allocation* allocation);

    void          addAllocations(const class Allocation* const allocations[], NS::UInteger count);

    void          removeAllocation(const class Allocation* allocation);

    void          removeAllocations(const class Allocation* const allocations[], NS::UInteger count);

    void          removeAllAllocations();

    bool          containsAllocation(const class Allocation* anAllocation);

    NS::Array*    allAllocations() const;

    NS::UInteger  allocationCount() const;

    void          commit();
};

}

// static method: alloc
_MTL_INLINE MTL::ResidencySetDescriptor* MTL::ResidencySetDescriptor::alloc()
{
    return NS::Object::alloc<MTL::ResidencySetDescriptor>(_MTL_PRIVATE_CLS(MTLResidencySetDescriptor));
}

// method: init
_MTL_INLINE MTL::ResidencySetDescriptor* MTL::ResidencySetDescriptor::init()
{
    return NS::Object::init<MTL::ResidencySetDescriptor>();
}

// property: label
_MTL_INLINE NS::String* MTL::ResidencySetDescriptor::label() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(label));
}

_MTL_INLINE void MTL::ResidencySetDescriptor::setLabel(const NS::String* label)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setLabel_), label);
}

// property: initialCapacity
_MTL_INLINE NS::UInteger MTL::ResidencySetDescriptor::initialCapacity() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(initialCapacity));
}

_MTL_INLINE void MTL::ResidencySetDescriptor::setInitialCapacity(NS::UInteger initialCapacity)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setInitialCapacity_), initialCapacity);
}

// property: device
_MTL_INLINE MTL::Device* MTL::ResidencySet::device() const
{
    return Object::sendMessage<MTL::Device*>(this, _MTL_PRIVATE_SEL(device));
}

// property: label
_MTL_INLINE NS::String* MTL::ResidencySet::label() const
{
    return Object::sendMessage<NS::String*>(this, _MTL_PRIVATE_SEL(label));
}

// property: allocatedSize
_MTL_INLINE uint64_t MTL::ResidencySet::allocatedSize() const
{
    return Object::sendMessage<uint64_t>(this, _MTL_PRIVATE_SEL(allocatedSize));
}

// method: requestResidency
_MTL_INLINE void MTL::ResidencySet::requestResidency()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(requestResidency));
}

// method: endResidency
_MTL_INLINE void MTL::ResidencySet::endResidency()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(endResidency));
}

// method: addAllocation:
_MTL_INLINE void MTL::ResidencySet::addAllocation(const MTL::Allocation* allocation)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(addAllocation_), allocation);
}

// method: addAllocations:count:
_MTL_INLINE void MTL::ResidencySet::addAllocations(const MTL::Allocation* const allocations[], NS::UInteger count)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(addAllocations_count_), allocations, count);
}

// method: removeAllocation:
_MTL_INLINE void MTL::ResidencySet::removeAllocation(const MTL::Allocation* allocation)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(removeAllocation_), allocation);
}

// method: removeAllocations:count:
_MTL_INLINE void MTL::ResidencySet::removeAllocations(const MTL::Allocation* const allocations[], NS::UInteger count)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(removeAllocations_count_), allocations, count);
}

// method: removeAllAllocations
_MTL_INLINE void MTL::ResidencySet::removeAllAllocations()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(removeAllAllocations));
}

// method: containsAllocation:
_MTL_INLINE bool MTL::ResidencySet::containsAllocation(const MTL::Allocation* anAllocation)
{
    return Object::sendMessage<bool>(this, _MTL_PRIVATE_SEL(containsAllocation_), anAllocation);
}

// property: allAllocations
_MTL_INLINE NS::Array* MTL::ResidencySet::allAllocations() const
{
    return Object::sendMessage<NS::Array*>(this, _MTL_PRIVATE_SEL(allAllocations));
}

// property: allocationCount
_MTL_INLINE NS::UInteger MTL::ResidencySet::allocationCount() const
{
    return Object::sendMessage<NS::UInteger>(this, _MTL_PRIVATE_SEL(allocationCount));
}

// method: commit
_MTL_INLINE void MTL::ResidencySet::commit()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(commit));
}
