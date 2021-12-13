/*
	Copyright 2021 flyinghead

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
#include <utility>

template<typename T>
class ComPtr
{
public:
	ComPtr() = default;
	ComPtr(const ComPtr& other) : ptr(other.ptr) {
		if (ptr != nullptr)
			ptr->AddRef();
	}
	ComPtr(ComPtr&& other) noexcept {
		std::swap(ptr, other.ptr);
	}
	~ComPtr() {
		if (ptr != nullptr)
			ptr->Release();
	}

	ComPtr& operator=(const ComPtr& other) {
		if (this != &other)
			*this = ComPtr(other);
		return *this;
	}
	ComPtr& operator=(ComPtr&& other) noexcept {
		std::swap(ptr, other.ptr);
		return *this;
	}

	T* operator->() const noexcept {
		return ptr;
	}
	explicit operator bool() const noexcept {
		return ptr != nullptr;
	}
	operator T*() const noexcept {
		return ptr;
	}
	T*& get() noexcept {
		return ptr;
	}
	void reset(T *ptr = nullptr) {
		if (ptr == this->ptr)
			return;
		std::swap(this->ptr, ptr);
		if (ptr != nullptr)
			ptr->Release();
	}

	template<typename I>
	HRESULT as(ComPtr<I>& p) const {
		return ptr->QueryInterface(IID_PPV_ARGS(&p.get()));
	}

private:
	T *ptr = nullptr;
};
