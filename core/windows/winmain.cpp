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
#include <nowide/stackstring.hpp>
#endif
#include "oslib/oslib.h"
#include "oslib/audiostream.h"
#include "imgread/common.h"
#include "stdclass.h"
#include "cfg/cfg.h"
#include "win_keyboard.h"
#include "log/LogManager.h"
#include "wsi/context.h"
#if defined(USE_SDL)
#include "sdl/sdl.h"
#else
#include "xinput_gamepad.h"
#endif
#include "hw/maple/maple_devs.h"
#include "emulator.h"
#include "rend/mainui.h"
#include "../shell/windows/resource.h"
#include "rawinput.h"
#include "oslib/directory.h"
#ifdef USE_BREAKPAD
#include "breakpad/client/windows/handler/exception_handler.h"
#include "version.h"
#endif

#include <windows.h>
#include <windowsx.h>

static PCHAR*
	commandLineToArgvA(
	PCHAR CmdLine,
	int* _argc
	)
{
	PCHAR* argv;
	PCHAR  _argv;
	ULONG   len;
	ULONG   argc;
	CHAR    a;
	ULONG   i, j;

	BOOLEAN  in_QM;
	BOOLEAN  in_TEXT;
	BOOLEAN  in_SPACE;

	len = strlen(CmdLine);
	i = ((len+2)/2)*sizeof(PVOID) + sizeof(PVOID);

	argv = (PCHAR*)GlobalAlloc(GMEM_FIXED,
		i + (len+2)*sizeof(CHAR));

	_argv = (PCHAR)(((PUCHAR)argv)+i);

	argc = 0;
	argv[argc] = _argv;
	in_QM = FALSE;
	in_TEXT = FALSE;
	in_SPACE = TRUE;
	i = 0;
	j = 0;

	while ((a = CmdLine[i]) != 0)
	{
		if(in_QM)
		{
			if(a == '\"')
			{
				in_QM = FALSE;
			}
			else
			{
				_argv[j] = a;
				j++;
			}
		}
		else
		{
			switch(a)
			{
			case '\"':
				in_QM = TRUE;
				in_TEXT = TRUE;
				if(in_SPACE) {
					argv[argc] = _argv+j;
					argc++;
				}
				in_SPACE = FALSE;
				break;
			case ' ':
			case '\t':
			case '\n':
			case '\r':
				if(in_TEXT)
				{
					_argv[j] = '\0';
					j++;
				}
				in_TEXT = FALSE;
				in_SPACE = TRUE;
				break;
			default:
				in_TEXT = TRUE;
				if(in_SPACE)
				{
					argv[argc] = _argv+j;
					argc++;
				}
				_argv[j] = a;
				j++;
				in_SPACE = FALSE;
				break;
			}
		}
		i++;
	}
	_argv[j] = '\0';
	argv[argc] = NULL;

	(*_argc) = argc;
	return argv;
}

#ifndef USE_SDL

static std::shared_ptr<WinMouse> mouse;
static std::shared_ptr<Win32KeyboardDevice> keyboard;
static bool mouseCaptured;
static POINT savedMousePos;
static bool gameRunning;

static void captureMouse(bool);

static void emuEventCallback(Event event, void *)
{
	static bool captureOn;
	switch (event)
	{
	case Event::Pause:
		captureOn = mouseCaptured;
		captureMouse(false);
		gameRunning = false;
		break;
	case Event::Resume:
		gameRunning = true;
		captureMouse(captureOn);
		break;
	default:
		break;
	}
}

static void checkRawInput()
{
	if ((bool)config::UseRawInput != (bool)keyboard)
		return;
	if (config::UseRawInput)
	{
		GamepadDevice::Unregister(keyboard);
		keyboard = nullptr;;
		GamepadDevice::Unregister(mouse);
		mouse = nullptr;
		rawinput::init();
	}
	else
	{
		rawinput::term();
		keyboard = std::make_shared<Win32KeyboardDevice>(0);
		GamepadDevice::Register(keyboard);
		mouse = std::make_shared<WinMouse>();
		GamepadDevice::Register(mouse);
	}
}
#endif

void os_SetupInput()
{
#if defined(USE_SDL)
	input_sdl_init();
#else
	XInputGamepadDevice::CreateDevices();
	EventManager::listen(Event::Pause, emuEventCallback);
	EventManager::listen(Event::Resume, emuEventCallback);
	checkRawInput();
#endif
#ifndef TARGET_UWP
	if (config::UseRawInput)
		rawinput::init();
#endif
}

void os_TermInput()
{
#if defined(USE_SDL)
	input_sdl_quit();
#endif
#ifndef TARGET_UWP
	if (config::UseRawInput)
		rawinput::term();
#endif
}

static void setupPath()
{
#ifndef TARGET_UWP
	wchar_t fname[512];
	GetModuleFileNameW(0, fname, ARRAY_SIZE(fname));

	std::string fn;
	nowide::stackstring path;
	if (!path.convert(fname))
		fn = ".\\";
	else
		fn = path.c_str();
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
	std::string homePath(path.c_str());
	homePath += '\\';
	set_user_config_dir(homePath);
	homePath += "data\\";
	set_user_data_dir(homePath);
	flycast::mkdir(homePath.c_str(), 0755);
	SetEnvironmentVariable(L"HOMEPATH", localFolder->Path->Data());
	SetEnvironmentVariable(L"HOMEDRIVE", nullptr);
#endif
}

void UpdateInputState()
{
#if defined(USE_SDL)
	input_sdl_handle();
#else
	for (int port = 0; port < 4; port++)
	{
		std::shared_ptr<XInputGamepadDevice> gamepad = XInputGamepadDevice::GetXInputDevice(port);
		if (gamepad != nullptr)
			gamepad->ReadInput();
	}
#endif
}

#ifndef USE_SDL
static HWND hWnd;

// Windows class name to register
#define WINDOW_CLASS "nilDC"
static int window_x, window_y;

// Width and height of the window
#define DEFAULT_WINDOW_WIDTH  1280
#define DEFAULT_WINDOW_HEIGHT 720
static bool window_maximized = false;

static void centerMouse()
{
	RECT rect;
	GetWindowRect(hWnd, &rect);
	SetCursorPos((rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2);
}

static void captureMouse(bool capture)
{
	if (hWnd == nullptr || !gameRunning)
		return;
	if (capture == mouseCaptured)
		return;

	if (!capture)
	{
		os_SetWindowText(VER_EMUNAME);
		mouseCaptured = false;
		SetCursorPos(savedMousePos.x, savedMousePos.y);
		while (ShowCursor(true) < 0)
			;
	}
	else
	{
		os_SetWindowText("Flycast - mouse capture");
		mouseCaptured = true;
		GetCursorPos(&savedMousePos);
		while (ShowCursor(false) >= 0)
			;
		centerMouse();
	}
}

static void toggleFullscreen();

static LRESULT CALLBACK WndProc2(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		/*
		Here we are handling 2 system messages: screen saving and monitor power.
		They are especially relevant on mobile devices.
		*/
	case WM_SYSCOMMAND:
		{
			switch (wParam)
			{
			case SC_SCREENSAVE:   // Screensaver trying to start ?
			case SC_MONITORPOWER: // Monitor trying to enter powersave ?
				return 0;         // Prevent this from happening
			}
			break;
		}
		// Handles the close message when a user clicks the quit icon of the window
	case WM_CLOSE:
		PostQuitMessage(0);
		return 1;

	case WM_SIZE:
		settings.display.width = LOWORD(lParam);
		settings.display.height = HIWORD(lParam);
		window_maximized = (wParam & SIZE_MAXIMIZED) != 0;
		GraphicsContext::Instance()->resize();
		return 0;

	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
		gui_set_mouse_position(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		checkRawInput();
		switch (message)
		{
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
			if (!mouseCaptured && !config::UseRawInput)
				mouse->setButton(Mouse::LEFT_BUTTON, message == WM_LBUTTONDOWN);
			gui_set_mouse_button(0, message == WM_LBUTTONDOWN);
			break;

		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
			if (!mouseCaptured && !config::UseRawInput)
				mouse->setButton(Mouse::MIDDLE_BUTTON, message == WM_MBUTTONDOWN);
			gui_set_mouse_button(2, message == WM_MBUTTONDOWN);
			break;
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
			if (!mouseCaptured && !config::UseRawInput)
				mouse->setButton(Mouse::RIGHT_BUTTON, message == WM_RBUTTONDOWN);
			gui_set_mouse_button(1, message == WM_RBUTTONDOWN);
			break;
		}
		if (mouseCaptured)
			break;
		/* no break */
	case WM_MOUSEMOVE:
		gui_set_mouse_position(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		checkRawInput();
		if (mouseCaptured)
			// TODO relative mouse move if !rawinput
			centerMouse();
		else if (!config::UseRawInput)
		{
			int xPos = GET_X_LPARAM(lParam);
			int yPos = GET_Y_LPARAM(lParam);
			mouse->setAbsPos(xPos, yPos, settings.display.width, settings.display.height);

			if (wParam & MK_LBUTTON)
				mouse->setButton(Mouse::LEFT_BUTTON, true);
			if (wParam & MK_MBUTTON)
				mouse->setButton(Mouse::MIDDLE_BUTTON, true);
			if (wParam & MK_RBUTTON)
				mouse->setButton(Mouse::RIGHT_BUTTON, true);
		}
		if (message != WM_MOUSEMOVE)
			return 0;
		break;
	case WM_MOUSEWHEEL:
		gui_set_mouse_wheel(-(float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA * 16);
		checkRawInput();
		if (!config::UseRawInput)
			mouse->setWheel(-GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA);
		break;

	case WM_KEYDOWN:
	case WM_KEYUP:
		{
			if (message == WM_KEYDOWN
					&& ((wParam == VK_CONTROL && GetAsyncKeyState(VK_LMENU) < 0)
							|| (wParam == VK_MENU && GetAsyncKeyState(VK_LCONTROL) < 0)))
			{
				captureMouse(!mouseCaptured);
				break;
			}
			checkRawInput();
			if (!config::UseRawInput)
			{
				u8 keycode;
				// bit 24 indicates whether the key is an extended key, such as the right-hand ALT and CTRL keys that appear on an enhanced 101- or 102-key keyboard.
				// (It also distinguishes between the main Return key and the numeric keypad Enter key)
				// The value is 1 if it is an extended key; otherwise, it is 0.
				if (wParam == VK_RETURN && ((lParam & (1 << 24)) != 0))
					keycode = VK_NUMPAD_RETURN;
				else
					keycode = wParam & 0xff;
				keyboard->keyboard_input(keycode, message == WM_KEYDOWN);
			}
		}
		break;
	
	case WM_SYSKEYDOWN:
		if (wParam == VK_RETURN)
		{
			if ((HIWORD(lParam) & KF_ALTDOWN))
				toggleFullscreen();
		}
		else if (wParam == VK_CONTROL && (lParam & (1 << 24)) == 0 && GetAsyncKeyState(VK_LMENU) < 0)
		{
			captureMouse(!mouseCaptured);
		}
		break;

	case WM_CHAR:
		gui_keyboard_input((u16)wParam);
		return 0;

	default:
		break;
	}

	// Calls the default window procedure for messages we did not handle
	return DefWindowProc(hWnd, message, wParam, lParam);
}

static bool windowClassRegistered;

void CreateMainWindow()
{
	if (hWnd != NULL)
		return;
	HINSTANCE hInstance = (HINSTANCE)GetModuleHandle(0);
	if (!windowClassRegistered)
	{
		WNDCLASS sWC;
		sWC.style = CS_HREDRAW | CS_VREDRAW;
		sWC.lpfnWndProc = WndProc2;
		sWC.cbClsExtra = 0;
		sWC.cbWndExtra = 0;
		sWC.hInstance = hInstance;
		sWC.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
		sWC.hCursor = LoadCursor(NULL, IDC_ARROW);
		sWC.lpszMenuName = 0;
		sWC.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
		sWC.lpszClassName = WINDOW_CLASS;
		ATOM registerClass = RegisterClass(&sWC);
		if (!registerClass)
			MessageBox(0, "Failed to register the window class", "Error", MB_OK | MB_ICONEXCLAMATION);
		else
			windowClassRegistered = true;
		settings.display.width = cfgLoadInt("window", "width", DEFAULT_WINDOW_WIDTH);
		settings.display.height = cfgLoadInt("window", "height", DEFAULT_WINDOW_HEIGHT);
		window_maximized = cfgLoadBool("window", "maximized", false);
	}

	// Create the eglWindow
	RECT sRect;
	SetRect(&sRect, 0, 0, settings.display.width, settings.display.height);
	AdjustWindowRectEx(&sRect, WS_OVERLAPPEDWINDOW, false, 0);

	hWnd = CreateWindow(WINDOW_CLASS, VER_EMUNAME, WS_VISIBLE | WS_OVERLAPPEDWINDOW | (window_maximized ? WS_MAXIMIZE : 0),
			window_x, window_y, sRect.right - sRect.left, sRect.bottom - sRect.top, NULL, NULL, hInstance, NULL);
	if (GraphicsContext::Instance() != nullptr)
		GraphicsContext::Instance()->setWindow((void *)hWnd, (void *)GetDC((HWND)hWnd));
}
#endif

void os_CreateWindow()
{
#if defined(USE_SDL)
	sdl_window_create();
#else
	CreateMainWindow();
	initRenderApi((void *)hWnd, (void *)GetDC((HWND)hWnd));
#endif	// !USE_SDL
}

#ifndef USE_SDL
static void destroyMainWindow()
{
	if (hWnd)
	{
		WINDOWPLACEMENT placement;
		placement.length = sizeof(WINDOWPLACEMENT);
		GetWindowPlacement(hWnd, &placement);
		window_maximized = placement.showCmd == SW_SHOWMAXIMIZED;
		window_x = placement.rcNormalPosition.left;
		window_y = placement.rcNormalPosition.top;
		DestroyWindow(hWnd);
		hWnd = NULL;
	}
}

static void toggleFullscreen()
{
	static RECT rSaved;
	static bool fullscreen=false;

	fullscreen = !fullscreen;


	if (fullscreen)
	{
		GetWindowRect(hWnd, &rSaved);

		MONITORINFO mi = { sizeof(mi) };
		HMONITOR hmon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
		if (GetMonitorInfo(hmon, &mi)) {

			SetWindowLongPtr(hWnd, GWL_EXSTYLE, WS_EX_APPWINDOW | WS_EX_TOPMOST);
			SetWindowLongPtr(hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);

			SetWindowPos(hWnd, HWND_TOPMOST, mi.rcMonitor.left, mi.rcMonitor.top,
				mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, 
				SWP_SHOWWINDOW|SWP_FRAMECHANGED|SWP_ASYNCWINDOWPOS);
		}
	}
	else {
		
		SetWindowLongPtr(hWnd, GWL_EXSTYLE, WS_EX_APPWINDOW | WS_EX_TOPMOST);
		SetWindowLongPtr(hWnd, GWL_STYLE, WS_VISIBLE | WS_OVERLAPPEDWINDOW | (window_maximized ? WS_MAXIMIZE : 0));

		SetWindowPos(hWnd, NULL, rSaved.left, rSaved.top,
			rSaved.right - rSaved.left, rSaved.bottom - rSaved.top, 
			SWP_SHOWWINDOW|SWP_FRAMECHANGED|SWP_ASYNCWINDOWPOS|SWP_NOZORDER);
	}

}

HWND getNativeHwnd()
{
	return hWnd;
}
#endif

void os_SetWindowText(const char* text)
{
#if defined(USE_SDL)
	sdl_window_set_text(text);
#else
	if (GetWindowLongPtr(hWnd, GWL_STYLE) & WS_BORDER)
	{
		SetWindowText(hWnd, text);
	}
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
    size_t oneMB = 1024 * 1024;
    for (size_t size = 256 * oneMB; size >= oneMB; size /= 2)
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
    for (size_t blockSize = 64 * 1024; blockSize >= 16; blockSize /= 2)
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
    for (size_t blockSize = 64 * 1024; blockSize >= 16; blockSize /= 2)
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
		_snwprintf(s, ARRAY_SIZE(s), L"Minidump saved to '%s\\%s.dmp'", dump_path, minidump_id);
		::OutputDebugStringW(s);
	}
	return succeeded;
}
#endif

#ifdef TARGET_UWP

void gui_load_game()
{
	using namespace Windows::Storage;
	using namespace Concurrency;

	auto picker = ref new Pickers::FileOpenPicker();
	picker->ViewMode = Pickers::PickerViewMode::List;

	picker->FileTypeFilter->Append(".chd");
	picker->FileTypeFilter->Append(".gdi");
	picker->FileTypeFilter->Append(".cue");
	picker->FileTypeFilter->Append(".cdi");
	picker->FileTypeFilter->Append(".zip");
	picker->FileTypeFilter->Append(".7z");
	picker->FileTypeFilter->Append(".elf");
	if (!config::HideLegacyNaomiRoms)
	{
		picker->FileTypeFilter->Append(".bin");
		picker->FileTypeFilter->Append(".lst");
		picker->FileTypeFilter->Append(".dat");
	}
	picker->SuggestedStartLocation = Pickers::PickerLocationId::DocumentsLibrary;

	create_task(picker->PickSingleFileAsync()).then([](StorageFile ^file) {
		if (file)
		{
			NOTICE_LOG(COMMON, "Picked file: %S", file->Path->Data());
			nowide::stackstring path;
			if (path.convert(file->Path->Data()))
				gui_start_game(path.c_str());
		}
	});
}

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

	HANDLE fileh = CreateFile2FromAppW(wname.c_str(), dwDesiredAccess, FILE_SHARE_READ, dwCreationDisposition, nullptr);
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
    return _wremove(wname.c_str());
}

}

extern "C" int SDL_main(int argc, char* argv[])
{


#elif defined(DEF_CONSOLE)
// DEF_CONSOLE allows you to override linker subsystem and therefore default console
//	: pragma isn't pretty but def's are configurable
#pragma comment(linker, "/subsystem:console")

int main(int argc, char** argv)
{

#else
#pragma comment(linker, "/subsystem:windows")

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShowCmd)
{
	int argc = 0;
	char* cmd_line = GetCommandLineA();
	char** argv = commandLineToArgvA(cmd_line, &argc);
#endif

#ifdef USE_BREAKPAD
	wchar_t tempDir[MAX_PATH + 1];
	GetTempPathW(MAX_PATH + 1, tempDir);

	static google_breakpad::CustomInfoEntry custom_entries[] = {
			google_breakpad::CustomInfoEntry(L"prod", L"Flycast"),
			google_breakpad::CustomInfoEntry(L"ver", L"" GIT_VERSION),
	};
	google_breakpad::CustomClientInfo custom_info = { custom_entries, ARRAY_SIZE(custom_entries) };

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

#ifdef TARGET_UWP
	if (config::ContentPath.get().empty())
		config::ContentPath.get().push_back(get_writable_config_path(""));
#endif
	os_InstallFaultHandler();

	mainui_loop();

#ifdef USE_SDL
	sdl_window_destroy();
#else
	termRenderApi();
	destroyMainWindow();
	cfgSaveBool("window", "maximized", window_maximized);
	if (!window_maximized && settings.display.width != 0 && settings.display.height != 0)
	{
		cfgSaveInt("window", "width", settings.display.width);
		cfgSaveInt("window", "height", settings.display.height);
	}
#endif

	flycast_term();
	os_UninstallFaultHandler();

	return 0;
}

void os_DebugBreak()
{
	__debugbreak();
}

void os_DoEvents()
{
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
