/*
    Created on: Oct 2, 2019

	Copyright 2019 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
#pragma once
#include "types.h"

#include <vulkan/vulkan.hpp>

//#define VK_DEBUG 1

#if VK_USE_64_BIT_PTR_DEFINES == 1
using VkHandle = void *;
#else
using VkHandle = uint64_t;
#endif

// RAII-like interface for objects that need to be deleted/released in the future
class Deletable
{
public:
	virtual ~Deletable() = default;
};

// Implementations of this interface control when object are in use and delete them when possible
class FlightManager
{
public:
	virtual void addToFlight(Deletable *object) = 0;

	virtual ~FlightManager() = default;
};

template<typename T>
class Deleter : public Deletable
{
public:
	Deleter() = delete;
	explicit Deleter(T& o) : o(o) {}
	Deleter(T&& o) : o(std::move(o)) {}
	~Deleter() override {
		if constexpr (std::is_pointer_v<T>)
			delete o;
	}

private:
	T o;
};
