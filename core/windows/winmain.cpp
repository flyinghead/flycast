/*
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
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS 1
#endif
#include "build.h"
#ifdef TARGET_UWP
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Globalization.DateTimeFormatting.h>
#include <winrt/Windows.Storage.h>
#include <io.h>
#include <fcntl.h>
#include <nowide/config.hpp>
#include <nowide/convert.hpp>
#include "cfg/option.h"
#include "ui/gui.h"
#endif
#include "oslib/oslib.h"
#include "stdclass.h"
#include "cfg/cfg.h"
#include "log/LogManager.h"
#include "sdl/sdl.h"
#include "emulator.h"
#include "ui/mainui.h"
#include "oslib/directory.h"
#ifdef USE_BREAKPAD
#include "breakpad/client/windows/handler/exception_handler.h"
#include "version.h"
#endif
#include "profiler/fc_profiler.h"

#include <nowide/args.hpp>
#include <nowide/stackstring.hpp>

#include <windows.h>
#include <windowsx.h>

static void setupPath()
{
#ifndef TARGET_UWP
	wchar_t fname[512];
	GetModuleFileNameW(0, fname, std::size(fname));

	std::string fn;
	nowide::stackstring path;
	if (!path.convert(fname))
		fn = ".\\";
	else
		fn = path.get();
	size_t pos = get_last_slash_pos(fn);
	if (pos != std::string::npos)
		fn = fn.substr(0, pos) + "\\";
	else
		fn = ".\\";
	set_user_config_dir(fn);
	add_system_data_dir(fn);

	std::string data_path = fn + "data\\";
	set_user_data_dir(data_path);
	flycast::mkdir(data_path.c_str(), 0755);
#else
	using namespace Windows::Storage;
	StorageFolder^ localFolder = Windows::Storage::ApplicationData::Current->LocalFolder;
	nowide::stackstring path;
	path.convert(localFolder->Path->Data());
	std::string homePath(path.get());
	homePath += '\\';
	set_user_config_dir(homePath);
	homePath += "data\\";
	set_user_data_dir(homePath);
	flycast::mkdir(homePath.c_str(), 0755);
	SetEnvironmentVariable(L"HOMEPATH", localFolder->Path->Data());
	SetEnvironmentVariable(L"HOMEDRIVE", nullptr);
#endif
}

static void reserveBottomMemory()
{
#if defined(_WIN64) && defined(_DEBUG)
    static bool s_initialized = false;
    if ( s_initialized )
        return;
    s_initialized = true;

    // Start by reserving large blocks of address space, and then
    // gradually reduce the size in order to capture all of the
    // fragments. Technically we should continue down to 64 KB but
    // stopping at 1 MB is sufficient to keep most allocators out.

    const size_t LOW_MEM_LINE = 0x100000000LL;
    size_t totalReservation = 0;
    size_t numVAllocs = 0;
    size_t numHeapAllocs = 0;
    for (size_t size = 256_MB; size >= 1_MB; size /= 2)
    {
        for (;;)
        {
            void* p = VirtualAlloc(0, size, MEM_RESERVE, PAGE_NOACCESS);
            if (!p)
                break;

            if ((size_t)p >= LOW_MEM_LINE)
            {
                // We don't need this memory, so release it completely.
                VirtualFree(p, 0, MEM_RELEASE);
                break;
            }

            totalReservation += size;
            ++numVAllocs;
        }
    }

    // Now repeat the same process but making heap allocations, to use up
    // the already reserved heap blocks that are below the 4 GB line.
    HANDLE heap = GetProcessHeap();
    for (size_t blockSize = 64_KB; blockSize >= 16; blockSize /= 2)
    {
        for (;;)
        {
            void* p = HeapAlloc(heap, 0, blockSize);
            if (!p)
                break;

            if ((size_t)p >= LOW_MEM_LINE)
            {
                // We don't need this memory, so release it completely.
                HeapFree(heap, 0, p);
                break;
            }

            totalReservation += blockSize;
            ++numHeapAllocs;
        }
    }

    // Perversely enough the CRT doesn't use the process heap. Suck up
    // the memory the CRT heap has already reserved.
    for (size_t blockSize = 64_KB; blockSize >= 16; blockSize /= 2)
    {
        for (;;)
        {
            void* p = malloc(blockSize);
            if (!p)
                break;

            if ((size_t)p >= LOW_MEM_LINE)
            {
                // We don't need this memory, so release it completely.
                free(p);
                break;
            }

            totalReservation += blockSize;
            ++numHeapAllocs;
        }
    }

    // Print diagnostics showing how many allocations we had to make in
    // order to reserve all of low memory, typically less than 200.
    char buffer[1000];
    sprintf_s(buffer, "Reserved %1.3f MB (%d vallocs,"
                      "%d heap allocs) of low-memory.\n",
            totalReservation / (1024 * 1024.0),
            (int)numVAllocs, (int)numHeapAllocs);
    OutputDebugStringA(buffer);
#endif
}
static void findKeyboardLayout()
{
#ifndef TARGET_UWP
	HKL keyboardLayout = GetKeyboardLayout(0);
	WORD lcid = HIWORD(keyboardLayout);
	switch (PRIMARYLANGID(lcid)) {
	case 0x09:	// English
		if (lcid == 0x0809)
			settings.input.keyboardLangId = KeyboardLayout::UK;
		else
			settings.input.keyboardLangId = KeyboardLayout::US;
		break;
	case 0x11:
		settings.input.keyboardLangId = KeyboardLayout::JP;
		break;
	case 0x07:
		settings.input.keyboardLangId = KeyboardLayout::GE;
		break;
	case 0x0c:
		settings.input.keyboardLangId = KeyboardLayout::FR;
		break;
	case 0x10:
		settings.input.keyboardLangId = KeyboardLayout::IT;
		break;
	case 0x0A:
		settings.input.keyboardLangId = KeyboardLayout::SP;
		break;
	default:
		break;
	}
#endif
}

#if defined(USE_BREAKPAD)
static bool dumpCallback(const wchar_t* dump_path,
		const wchar_t* minidump_id,
		void* context,
		EXCEPTION_POINTERS* exinfo,
		MDRawAssertionInfo* assertion,
		bool succeeded)
{
	if (succeeded)
	{
		wchar_t s[MAX_PATH + 32];
		_snwprintf(s, std::size(s), L"Minidump saved to '%s\\%s.dmp'", dump_path, minidump_id);
		::OutputDebugStringW(s);

		nowide::stackstring path;
		if (path.convert(dump_path))
		{
			std::string directory = path.get();
			if (path.convert(minidump_id))
			{
				std::string fullPath = directory + '\\' + std::string(path.get()) + ".dmp";
				registerCrash(directory.c_str(), fullPath.c_str());
			}
		}
	}
	return succeeded;
}
#endif

#ifdef TARGET_UWP
namespace nowide {

FILE *fopen(char const *file_name, char const *mode)
{
	wstackstring wname;
	if (!wname.convert(file_name))
	{
		errno = EINVAL;
		return nullptr;
	}
	DWORD dwDesiredAccess;
	DWORD dwCreationDisposition;
	int openFlags = 0;
	if (strchr(mode, '+') != nullptr)
		dwDesiredAccess = GENERIC_READ | GENERIC_WRITE;
	else if (strchr(mode, 'r') != nullptr)
	{
		openFlags |= _O_RDONLY;
		dwDesiredAccess = GENERIC_READ;
	}
	else
		dwDesiredAccess = GENERIC_WRITE;
	if (strchr(mode, 'w') != nullptr)
		dwCreationDisposition = CREATE_ALWAYS;
	else if (strchr(mode, 'a') != nullptr)
	{
		dwCreationDisposition = OPEN_ALWAYS;
		openFlags |= _O_APPEND;
	}
	else
		dwCreationDisposition = OPEN_EXISTING;
	if (strchr(mode, 'b') == nullptr)
		openFlags |= _O_TEXT;

	HANDLE fileh = CreateFile2FromAppW(wname.get(), dwDesiredAccess, FILE_SHARE_READ, dwCreationDisposition, nullptr);
	if (fileh == INVALID_HANDLE_VALUE)
		return nullptr;

	int fd = _open_osfhandle((intptr_t)fileh, openFlags);
	if (fd == -1)
	{
		WARN_LOG(COMMON, "_open_osfhandle failed");
		CloseHandle(fileh);
		return nullptr;
	}

	return _fdopen(fd, mode);
}

int remove(char const *name)
{
    wstackstring wname;
    if(!wname.convert(name)) {
        errno = EINVAL;
        return -1;
    }
    return _wremove(wname.get());
}

}
#endif

int main(int argc, char* argv[])
{
	nowide::args _(argc, argv);

#ifdef USE_BREAKPAD
	wchar_t tempDir[MAX_PATH + 1];
	GetTempPathW(MAX_PATH + 1, tempDir);

	static google_breakpad::CustomInfoEntry custom_entries[] = {
			google_breakpad::CustomInfoEntry(L"prod", L"Flycast"),
			google_breakpad::CustomInfoEntry(L"ver", L"" GIT_VERSION),
	};
	google_breakpad::CustomClientInfo custom_info = { custom_entries, std::size(custom_entries) };

	google_breakpad::ExceptionHandler handler(tempDir,
		nullptr,
		dumpCallback,
		nullptr,
		google_breakpad::ExceptionHandler::HANDLER_ALL,
		MiniDumpNormal,
		INVALID_HANDLE_VALUE,
		&custom_info);
	// crash on die() and failing verify()
	handler.set_handle_debug_exceptions(true);
#endif

#if defined(_WIN32) && defined(LOG_TO_PTY)
	setbuf(stderr, NULL);
#endif
	LogManager::Init();

	reserveBottomMemory();
	setupPath();
	findKeyboardLayout();

	if (flycast_init(argc, argv) != 0)
		die("Flycast initialization failed");

#ifdef USE_BREAKPAD
	nowide::stackstring nws;
	static std::string tempDir8;
	if (nws.convert(tempDir))
		tempDir8 = nws.get();
	auto async = std::async(std::launch::async, uploadCrashes, tempDir8);
#endif

#ifdef TARGET_UWP
	if (config::ContentPath.get().empty())
		config::ContentPath.get().push_back(get_writable_config_path(""));
#endif
	os_InstallFaultHandler();

	mainui_loop();

	flycast_term();
	os_UninstallFaultHandler();

	return 0;
}

[[noreturn]] void os_DebugBreak()
{
	__debugbreak();
	std::abort();
}

void os_DoEvents()
{
	FC_PROFILE_SCOPE;

#ifndef TARGET_UWP
	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		// If the message is WM_QUIT, exit the while loop
		if (msg.message == WM_QUIT)
		{
			dc_exit();
		}

		// Translate the message and dispatch it to WindowProc()
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
#endif
}

void os_RunInstance(int argc, const char *argv[])
{
	wchar_t exePath[MAX_PATH];
	GetModuleFileNameW(NULL, exePath, std::size(exePath));

	std::wstring cmdLine = L'"' + std::wstring(exePath) + L'"';
	for (int i = 0; i < argc; i++)
	{
		nowide::wstackstring wname;
		if (!wname.convert(argv[i])) {
			WARN_LOG(BOOT, "Invalid argument: %s", argv[i]);
			continue;
		}
		cmdLine += L" \"";
		for (wchar_t *p = wname.get(); *p != L'\0'; p++)
		{
			cmdLine += *p;
			if (*p == L'"')
				// escape double quote
				cmdLine += L'"';
		}
		cmdLine += L'"';
	}

	STARTUPINFOW startupInfo{};
	startupInfo.cb = sizeof(startupInfo);

	PROCESS_INFORMATION processInfo{};
	BOOL rc = CreateProcessW(exePath, (wchar_t *)cmdLine.c_str(), nullptr, nullptr, true, 0, nullptr, nullptr, &startupInfo, &processInfo);
	if (rc)
	{
		CloseHandle(processInfo.hProcess);
		CloseHandle(processInfo.hThread);
	}
	else
	{
		WARN_LOG(BOOT, "Cannot launch Flycast instance: error %d", GetLastError());
	}
}

void os_SetThreadName(const char *name)
{
#ifndef TARGET_UWP
	nowide::wstackstring wname;
	if (wname.convert(name))
	{
		static HRESULT (*SetThreadDescription)(HANDLE, PCWSTR);
		if (SetThreadDescription == nullptr)
		{
			// supported in Windows 10, version 1607 or Windows Server 2016
			HINSTANCE libh = LoadLibraryW(L"KernelBase.dll");
			if (libh != NULL)
				SetThreadDescription = (HRESULT (*)(HANDLE, PCWSTR))GetProcAddress(libh, "SetThreadDescription");
		}
		if (SetThreadDescription != nullptr)
			SetThreadDescription(GetCurrentThread(), wname.get());
	}
#endif
}

#ifdef VIDEO_ROUTING
#include "SpoutSender.h"
#include "SpoutDX.h"

static SpoutSender* spoutSender;
static spoutDX* spoutDXSender;

void os_VideoRoutingPublishFrameTexture(GLuint texID, GLuint texTarget, float w, float h)
{
	if (spoutSender == nullptr)
	{
		spoutSender = new SpoutSender();
		int boardID = cfgLoadInt("naomi", "BoardId", 0);
		char buf[32] = { 0 };
		vsnprintf(buf, sizeof(buf), (boardID == 0 ? "Flycast - Video Content" : "Flycast - Video Content - %d"), std::va_list(&boardID));
		spoutSender->SetSenderName(buf);
	}	
	spoutSender->SendTexture(texID, texTarget, w, h, true);
}

void os_VideoRoutingTermGL()
{
	if (spoutSender) 
	{
		spoutSender->ReleaseSender();
		spoutSender = nullptr;
	}
}

void os_VideoRoutingPublishFrameTexture(ID3D11Texture2D* pTexture)
{
	if (spoutDXSender == nullptr)
	{
		spoutDXSender = new spoutDX();
		ID3D11Resource* resource = nullptr;
		HRESULT hr = pTexture->QueryInterface(__uuidof(ID3D11Resource), reinterpret_cast<void**>(&resource));
		if (SUCCEEDED(hr))
		{
			ID3D11Device* pDevice = nullptr;
			resource->GetDevice(&pDevice);
			resource->Release();
			spoutDXSender->OpenDirectX11(pDevice);
			pDevice->Release();
			int boardID = cfgLoadInt("naomi", "BoardId", 0);
			char buf[32] = { 0 };
			vsnprintf(buf, sizeof(buf), (boardID == 0 ? "Flycast - Video Content" : "Flycast - Video Content - %d"), std::va_list(&boardID));
			spoutDXSender->SetSenderName(buf);
		}
		else
		{
			return;
		}
	}
	spoutDXSender->SendTexture(pTexture);
}

void os_VideoRoutingTermDX()
{
	if (spoutDXSender)
	{
		spoutDXSender->ReleaseSender();
		spoutDXSender = nullptr;
	}
}
#endif
