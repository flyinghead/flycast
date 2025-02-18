/*
	Copyright 2025 flyinghead

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
#include <windows.h>

namespace detail
{
	template <typename Ret, typename... Args>
	struct ProxyTraits {
		using funcType = Ret (WINAPI *)(Args...);
	};
}

class WinLibLoader
{
public:
	WinLibLoader(const char* name = nullptr) : name(name) {
	}
	~WinLibLoader() {
		if (hinst != NULL)
			FreeLibrary(hinst);
	}

	template <typename Ret, typename... Args>
	auto getFunc(const char* functionName, Ret(WINAPI * const funcPtr)(Args...))
	{
		using funcType = typename detail::ProxyTraits<Ret, Args...>::funcType;
		if (!loaded()) {
			if (!load(name))
				return static_cast<funcType>(nullptr);
		}
		return reinterpret_cast<funcType>(GetProcAddress(hinst, functionName));
	}

	bool load(const char* name)
	{
		if (hinst != NULL)
			FreeLibrary(hinst);
		hinst = LoadLibraryA(name);
		return hinst != NULL;
	}

	bool loaded() const { return hinst != NULL; }

private:
	const char* name;
	HINSTANCE hinst = NULL;
};
