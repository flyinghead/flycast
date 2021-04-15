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
#ifdef _WIN32
#include <windows.h>
#include <d3d9.h>
#include "imgui_impl_dx9.h"
#include "types.h"

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

private:
	T *ptr = nullptr;
};

class DXContext
{
public:
	bool Init();
	void Term();
	void EndImGuiFrame();
	void Present();
	const ComPtr<IDirect3D9>& getD3D() const { return pD3D; }
	const ComPtr<IDirect3DDevice9>& getDevice() const { return pDevice; }
	void resize();
	void setOverlay(bool overlay) { this->overlay = overlay; }
	std::string getDriverName() const {
		D3DADAPTER_IDENTIFIER9 id;
		pD3D->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &id);
		return std::string(id.Description);
	}
	std::string getDriverVersion() const {
		D3DADAPTER_IDENTIFIER9 id;
		pD3D->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &id);
		return std::to_string(id.DriverVersion.HighPart >> 16) + "." + std::to_string((u16)id.DriverVersion.HighPart)
			+ "." + std::to_string(id.DriverVersion.LowPart >> 16) + "." + std::to_string((u16)id.DriverVersion.LowPart);
	}
	void setNativeWindow(HWND hWnd) {
		this->hWnd = hWnd;
	}

private:
	void resetDevice();

	ComPtr<IDirect3D9> pD3D;
	ComPtr<IDirect3DDevice9> pDevice;
	D3DPRESENT_PARAMETERS d3dpp{};
	bool overlay = false;
	HWND hWnd = nullptr;
};
extern DXContext theDXContext;
#endif
