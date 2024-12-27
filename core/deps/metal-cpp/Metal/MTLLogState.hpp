//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Metal/MTLLogState.hpp
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

#include "MTLLogState.hpp"

namespace MTL
{
_MTL_ENUM(NS::Integer, LogLevel) {
    LogLevelUndefined = 0,
    LogLevelDebug = 1,
    LogLevelInfo = 2,
    LogLevelNotice = 3,
    LogLevelError = 4,
    LogLevelFault = 5,
};

using LogHandlerFunction = std::function<void(NS::String* subsystem, NS::String* category, MTL::LogLevel logLevel, NS::String* message)>;

class LogState : public NS::Referencing<LogState>
{
public:
    void addLogHandler(void (^block)(NS::String*, NS::String*, MTL::LogLevel, NS::String*));
    void addLogHandler(const LogHandlerFunction& handler);
};

class LogStateDescriptor : public NS::Copying<LogStateDescriptor>
{
public:
    static class LogStateDescriptor* alloc();

    class LogStateDescriptor*        init();

    MTL::LogLevel                    level() const;
    void                             setLevel(MTL::LogLevel level);

    NS::Integer                      bufferSize() const;
    void                             setBufferSize(NS::Integer bufferSize);
};

_MTL_CONST(NS::ErrorDomain, LogStateErrorDomain);

_MTL_ENUM(NS::UInteger, LogStateError) {
    LogStateErrorInvalidSize = 1,
    LogStateErrorInvalid = 2,
};

}

_MTL_PRIVATE_DEF_WEAK_CONST(NS::ErrorDomain, LogStateErrorDomain);

// method: addLogHandler:
_MTL_INLINE void MTL::LogState::addLogHandler(void (^block)(NS::String*, NS::String*, MTL::LogLevel, NS::String*))
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(addLogHandler_), block);
}

_MTL_INLINE void MTL::LogState::addLogHandler(const MTL::LogHandlerFunction& handler)
{
    __block LogHandlerFunction function = handler;

    addLogHandler(^void(NS::String* subsystem, NS::String* category, MTL::LogLevel logLevel, NS::String* message){
        function(subsystem, category, logLevel, message);
	});
}

// static method: alloc
_MTL_INLINE MTL::LogStateDescriptor* MTL::LogStateDescriptor::alloc()
{
    return NS::Object::alloc<MTL::LogStateDescriptor>(_MTL_PRIVATE_CLS(MTLLogStateDescriptor));
}

// method: init
_MTL_INLINE MTL::LogStateDescriptor* MTL::LogStateDescriptor::init()
{
    return NS::Object::init<MTL::LogStateDescriptor>();
}

// property: level
_MTL_INLINE MTL::LogLevel MTL::LogStateDescriptor::level() const
{
    return Object::sendMessage<MTL::LogLevel>(this, _MTL_PRIVATE_SEL(level));
}

_MTL_INLINE void MTL::LogStateDescriptor::setLevel(MTL::LogLevel level)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setLevel_), level);
}

// property: bufferSize
_MTL_INLINE NS::Integer MTL::LogStateDescriptor::bufferSize() const
{
    return Object::sendMessage<NS::Integer>(this, _MTL_PRIVATE_SEL(bufferSize));
}

_MTL_INLINE void MTL::LogStateDescriptor::setBufferSize(NS::Integer bufferSize)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setBufferSize_), bufferSize);
}
