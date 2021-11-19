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
#include "build.h"
#if defined(_WIN32) && !defined(TARGET_UWP)
#include "types.h"
#include <windows.h>
#include <d3d9.h>
#include "imgui_impl_dx9.h"
#include "comptr.h"
#include "d3d_overlay.h"
#include "wsi/context.h"

class DXContext : public GraphicsContext
{
public:
	bool init(bool keepCurrentWindow = false);
	void term() override;
	void EndImGuiFrame();
	void Present();
	const ComPtr<IDirect3D9>& getD3D() const { return pD3D; }
	const ComPtr<IDirect3DDevice9>& getDevice() const { return pDevice; }
	void resize() override;
	void setOverlay(bool overlayOnly) { this->overlayOnly = overlayOnly; }
	std::string getDriverName() override {
		D3DADAPTER_IDENTIFIER9 id;
		pD3D->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &id);
		return std::string(id.Description);
	}
	std::string getDriverVersion() override {
		D3DADAPTER_IDENTIFIER9 id;
		pD3D->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &id);
		return std::to_string(id.DriverVersion.HighPart >> 16) + "." + std::to_string((u16)id.DriverVersion.HighPart)
			+ "." + std::to_string(id.DriverVersion.LowPart >> 16) + "." + std::to_string((u16)id.DriverVersion.LowPart);
	}
	void setFrameRendered() {
		frameRendered = true;
	}

private:
	void resetDevice();

	ComPtr<IDirect3D9> pD3D;
	ComPtr<IDirect3DDevice9> pDevice;
	D3DPRESENT_PARAMETERS d3dpp{};
	bool overlayOnly = false;
	D3DOverlay overlay;
	bool swapOnVSync = false;
	bool frameRendered = false;
};
extern DXContext theDXContext;
#endif
