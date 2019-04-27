#include "oslib\oslib.h"
#include "oslib\audiostream.h"
#include "imgread\common.h"
#include "stdclass.h"
#include "cfg/cfg.h"
#include "xinput_gamepad.h"
#include "win_keyboard.h"

#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <windowsx.h>

#include <Xinput.h>
#include "hw\maple\maple_cfg.h"
#pragma comment(lib, "XInput9_1_0.lib")

PCHAR*
	CommandLineToArgvA(
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

	while( a = CmdLine[i] )
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

void dc_exit(void);

bool VramLockedWrite(u8* address);
bool ngen_Rewrite(unat& addr,unat retadr,unat acc);
bool BM_LockedWrite(u8* address);

static std::shared_ptr<WinKbGamepadDevice> kb_gamepad;
static std::shared_ptr<WinMouseGamepadDevice> mouse_gamepad;

void os_SetupInput()
{
#if DC_PLATFORM == DC_PLATFORM_DREAMCAST
	mcfg_CreateDevices();
#endif
	XInputGamepadDevice::CreateDevices();
	kb_gamepad = std::make_shared<WinKbGamepadDevice>(0);
	GamepadDevice::Register(kb_gamepad);
	mouse_gamepad = std::make_shared<WinMouseGamepadDevice>(0);
	GamepadDevice::Register(mouse_gamepad);
}

LONG ExeptionHandler(EXCEPTION_POINTERS *ExceptionInfo)
{
	EXCEPTION_POINTERS* ep = ExceptionInfo;

	u32 dwCode = ep->ExceptionRecord->ExceptionCode;

	EXCEPTION_RECORD* pExceptionRecord=ep->ExceptionRecord;

	if (dwCode != EXCEPTION_ACCESS_VIOLATION)
		return EXCEPTION_CONTINUE_SEARCH;

	u8* address=(u8*)pExceptionRecord->ExceptionInformation[1];

	//printf("[EXC] During access to : 0x%X\n", address);

	if (VramLockedWrite(address))
	{
		return EXCEPTION_CONTINUE_EXECUTION;
	}
#ifndef TARGET_NO_NVMEM
	else if (BM_LockedWrite(address))
	{
		return EXCEPTION_CONTINUE_EXECUTION;
	}
#endif
#if FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86
		else if ( ngen_Rewrite((unat&)ep->ContextRecord->Eip,*(unat*)ep->ContextRecord->Esp,ep->ContextRecord->Eax) )
		{
			//remove the call from call stack
			ep->ContextRecord->Esp+=4;
			//restore the addr from eax to ecx so its valid again
			ep->ContextRecord->Ecx=ep->ContextRecord->Eax;
			return EXCEPTION_CONTINUE_EXECUTION;
		}
#endif
	else
	{
		printf("[GPF]Unhandled access to : 0x%X\n",(unat)address);
	}

	return EXCEPTION_CONTINUE_SEARCH;
}


void SetupPath()
{
	char fname[512];
	GetModuleFileName(0,fname,512);
	string fn=string(fname);
	fn=fn.substr(0,fn.find_last_of('\\'));
	set_user_config_dir(fn);
	set_user_data_dir(fn);
}

// Gamepads
u16 kcode[4] = { 0xffff, 0xffff, 0xffff, 0xffff };
u32 vks[4];
s8 joyx[4],joyy[4];
u8 rt[4],lt[4];
// Mouse
extern s32 mo_x_abs;
extern s32 mo_y_abs;
extern u32 mo_buttons;
extern f32 mo_x_delta;
extern f32 mo_y_delta;
extern f32 mo_wheel_delta;
// Keyboard
static Win32KeyboardDevice keyboard(0);

void UpdateInputState(u32 port)
{
	/*
		 Disabled for now. Need new EMU_BTN_ANA_LEFT/RIGHT/.. virtual controller keys

	joyx[port]=joyy[port]=0;

	if (GetAsyncKeyState('J'))
		joyx[port]-=126;
	if (GetAsyncKeyState('L'))
		joyx[port]+=126;

	if (GetAsyncKeyState('I'))
		joyy[port]-=126;
	if (GetAsyncKeyState('K'))
		joyy[port]+=126;
	*/
	std::shared_ptr<XInputGamepadDevice> gamepad = XInputGamepadDevice::GetXInputDevice(port);
	if (gamepad != NULL)
		gamepad->ReadInput();
}

// Windows class name to register
#define WINDOW_CLASS "nilDC"

// Width and height of the window
#define DEFAULT_WINDOW_WIDTH  1280
#define DEFAULT_WINDOW_HEIGHT 720
extern int screen_width, screen_height;
static bool window_maximized = false;

LRESULT CALLBACK WndProc2(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
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
		screen_width = LOWORD(lParam);
		screen_height = HIWORD(lParam);
		window_maximized = (wParam & SIZE_MAXIMIZED) != 0;
		return 0;

	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
		switch (message)
		{
		case WM_LBUTTONDOWN:
			mouse_gamepad->gamepad_btn_input(0, true);
			break;
		case WM_LBUTTONUP:
			mouse_gamepad->gamepad_btn_input(0, false);
			break;
		case WM_MBUTTONDOWN:
			mouse_gamepad->gamepad_btn_input(1, true);
			break;
		case WM_MBUTTONUP:
			mouse_gamepad->gamepad_btn_input(1, false);
			break;
		case WM_RBUTTONDOWN:
			mouse_gamepad->gamepad_btn_input(2, true);
			break;
		case WM_RBUTTONUP:
			mouse_gamepad->gamepad_btn_input(2, false);
			break;
		}
		/* no break */
	case WM_MOUSEMOVE:
		{
			static int prev_x = -1;
			static int prev_y = -1;
			int xPos = GET_X_LPARAM(lParam);
			int yPos = GET_Y_LPARAM(lParam);
			mo_x_abs = (xPos - (screen_width - 640 * screen_height / 480) / 2) * 480 / screen_height;
			mo_y_abs = yPos * 480 / screen_height;
			mo_buttons = 0xffffffff;
			if (wParam & MK_LBUTTON)
				mo_buttons &= ~(1 << 2);
			if (wParam & MK_MBUTTON)
				mo_buttons &= ~(1 << 3);
			if (wParam & MK_RBUTTON)
				mo_buttons &= ~(1 << 1);
			if (prev_x != -1)
			{
				mo_x_delta += (f32)(xPos - prev_x) * settings.input.MouseSensitivity / 100.f;
				mo_y_delta += (f32)(yPos - prev_y) * settings.input.MouseSensitivity / 100.f;
			}
			prev_x = xPos;
			prev_y = yPos;
		}
		if (message != WM_MOUSEMOVE)
			return 0;
		break;
	case WM_MOUSEWHEEL:
		mo_wheel_delta -= (float)GET_WHEEL_DELTA_WPARAM(wParam)/(float)WHEEL_DELTA * 16;
		break;

	case WM_KEYDOWN:
	case WM_KEYUP:
		{
			u8 keycode;
			// bit 24 indicates whether the key is an extended key, such as the right-hand ALT and CTRL keys that appear on an enhanced 101- or 102-key keyboard.
			// (It also distinguishes between the main Return key and the numeric keypad Enter key)
			// The value is 1 if it is an extended key; otherwise, it is 0.
			if (wParam == VK_RETURN && ((lParam & (1 << 24)) != 0))
				keycode = VK_NUMPAD_RETURN;
			else
				keycode = wParam & 0xff;
			kb_gamepad->gamepad_btn_input(keycode, message == WM_KEYDOWN);
			keyboard.keyboard_input(keycode, message == WM_KEYDOWN);
		}
		break;
	case WM_CHAR:
		keyboard.keyboard_character((char)wParam);
		return 0;

	default:
		break;
	}

	// Calls the default window procedure for messages we did not handle
	return DefWindowProc(hWnd, message, wParam, lParam);
}

void* window_win;
void os_CreateWindow()
{
	WNDCLASS sWC;
	sWC.style = CS_HREDRAW | CS_VREDRAW;
	sWC.lpfnWndProc = WndProc2;
	sWC.cbClsExtra = 0;
	sWC.cbWndExtra = 0;
	sWC.hInstance = (HINSTANCE)GetModuleHandle(0);
	sWC.hIcon = 0;
	sWC.hCursor = LoadCursor(NULL, IDC_ARROW);
	sWC.lpszMenuName = 0;
	sWC.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
	sWC.lpszClassName = WINDOW_CLASS;
	screen_width = cfgLoadInt("windows", "width", DEFAULT_WINDOW_WIDTH);
	screen_height = cfgLoadInt("windows", "height", DEFAULT_WINDOW_HEIGHT);
	window_maximized = cfgLoadBool("windows", "maximized", false);

	ATOM registerClass = RegisterClass(&sWC);
	if (!registerClass)
	{
		MessageBox(0, ("Failed to register the window class"), ("Error"), MB_OK | MB_ICONEXCLAMATION);
	}

	// Create the eglWindow
	RECT sRect;
	SetRect(&sRect, 0, 0, screen_width, screen_height);
	AdjustWindowRectEx(&sRect, WS_OVERLAPPEDWINDOW, false, 0);

	HWND hWnd = CreateWindow( WINDOW_CLASS, VER_FULLNAME, WS_VISIBLE | WS_OVERLAPPEDWINDOW | (window_maximized ? WS_MAXIMIZE : 0),
		0, 0, sRect.right-sRect.left, sRect.bottom-sRect.top, NULL, NULL, sWC.hInstance, NULL);

	window_win=hWnd;
}

void* libPvr_GetRenderTarget()
{
	return window_win;
}

void* libPvr_GetRenderSurface()
{
	return GetDC((HWND)window_win);
}

BOOL CtrlHandler( DWORD fdwCtrlType )
{
	switch( fdwCtrlType )
	{
		case CTRL_SHUTDOWN_EVENT:
		case CTRL_LOGOFF_EVENT:
		// Pass other signals to the next handler.
		case CTRL_BREAK_EVENT:
		// CTRL-CLOSE: confirm that the user wants to exit.
		case CTRL_CLOSE_EVENT:
		// Handle the CTRL-C signal.
		case CTRL_C_EVENT:
			SendMessageA((HWND)libPvr_GetRenderTarget(),WM_CLOSE,0,0); //FIXEM
			return( TRUE );
		default:
			return FALSE;
	}
}


void os_SetWindowText(const char* text)
{
	if (GetWindowLong((HWND)libPvr_GetRenderTarget(),GWL_STYLE)&WS_BORDER)
	{
		SetWindowText((HWND)libPvr_GetRenderTarget(), text);
	}
}

void os_MakeExecutable(void* ptr, u32 sz)
{
	DWORD old;
	VirtualProtect(ptr, sz, PAGE_EXECUTE_READWRITE, &old);  // sizeof(sz) really?
}

void ReserveBottomMemory()
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

#ifdef _WIN64
#include "hw/sh4/dyna/ngen.h"

typedef union _UNWIND_CODE {
	struct {
		u8 CodeOffset;
		u8 UnwindOp : 4;
		u8 OpInfo : 4;
	};
	USHORT FrameOffset;
} UNWIND_CODE, *PUNWIND_CODE;

typedef struct _UNWIND_INFO {
	u8 Version : 3;
	u8 Flags : 5;
	u8 SizeOfProlog;
	u8 CountOfCodes;
	u8 FrameRegister : 4;
	u8 FrameOffset : 4;
	//ULONG ExceptionHandler;
	UNWIND_CODE UnwindCode[1];
	/*  UNWIND_CODE MoreUnwindCode[((CountOfCodes + 1) & ~1) - 1];
	*   union {
	*       OPTIONAL ULONG ExceptionHandler;
	*       OPTIONAL ULONG FunctionEntry;
	*   };
	*   OPTIONAL ULONG ExceptionData[]; */
} UNWIND_INFO, *PUNWIND_INFO;

static RUNTIME_FUNCTION Table[1];
static _UNWIND_INFO unwind_info[1];

EXCEPTION_DISPOSITION
__gnat_SEH_error_handler(struct _EXCEPTION_RECORD* ExceptionRecord,
void *EstablisherFrame,
struct _CONTEXT* ContextRecord,
	void *DispatcherContext)
{
	EXCEPTION_POINTERS ep;
	ep.ContextRecord = ContextRecord;
	ep.ExceptionRecord = ExceptionRecord;

	return (EXCEPTION_DISPOSITION)ExeptionHandler(&ep);
}

PRUNTIME_FUNCTION
seh_callback(
_In_ DWORD64 ControlPc,
_In_opt_ PVOID Context
) {
	unwind_info[0].Version = 1;
	unwind_info[0].Flags = UNW_FLAG_UHANDLER;
	/* We don't use the unwinding info so fill the structure with 0 values.  */
	unwind_info[0].SizeOfProlog = 0;
	unwind_info[0].CountOfCodes = 0;
	unwind_info[0].FrameOffset = 0;
	unwind_info[0].FrameRegister = 0;
	/* Add the exception handler.  */

//		unwind_info[0].ExceptionHandler =
	//	(DWORD)((u8 *)__gnat_SEH_error_handler - CodeCache);
	/* Set its scope to the entire program.  */
	Table[0].BeginAddress = 0;// (CodeCache - (u8*)__ImageBase);
	Table[0].EndAddress = /*(CodeCache - (u8*)__ImageBase) +*/ CODE_SIZE;
	Table[0].UnwindData = (DWORD)((u8 *)unwind_info - CodeCache);
	printf("TABLE CALLBACK\n");
	//for (;;);
	return Table;
}
void setup_seh() {
#if 1
	/* Get the base of the module.  */
	//u8* __ImageBase = (u8*)GetModuleHandle(NULL);
	/* Current version is always 1 and we are registering an
	exception handler.  */
	unwind_info[0].Version = 1;
	unwind_info[0].Flags = UNW_FLAG_NHANDLER;
	/* We don't use the unwinding info so fill the structure with 0 values.  */
	unwind_info[0].SizeOfProlog = 0;
	unwind_info[0].CountOfCodes = 1;
	unwind_info[0].FrameOffset = 0;
	unwind_info[0].FrameRegister = 0;
	/* Add the exception handler.  */

	unwind_info[0].UnwindCode[0].CodeOffset = 0;
	unwind_info[0].UnwindCode[0].UnwindOp = 2;// UWOP_ALLOC_SMALL;
	unwind_info[0].UnwindCode[0].OpInfo = 0x20 / 8;

	//unwind_info[0].ExceptionHandler =
		//(DWORD)((u8 *)__gnat_SEH_error_handler - CodeCache);
	/* Set its scope to the entire program.  */
	Table[0].BeginAddress = 0;// (CodeCache - (u8*)__ImageBase);
	Table[0].EndAddress = /*(CodeCache - (u8*)__ImageBase) +*/ CODE_SIZE;
	Table[0].UnwindData = (DWORD)((u8 *)unwind_info - CodeCache);
	/* Register the unwind information.  */
	RtlAddFunctionTable(Table, 1, (DWORD64)CodeCache);
#endif

	//verify(RtlInstallFunctionTableCallback((unat)CodeCache | 0x3, (DWORD64)CodeCache, CODE_SIZE, seh_callback, 0, 0));
}
#endif




// DEF_CONSOLE allows you to override linker subsystem and therefore default console //
//	: pragma isn't pretty but def's are configurable 
#ifdef DEF_CONSOLE
#pragma comment(linker, "/subsystem:console")

int main(int argc, char **argv)
{

#else
#pragma comment(linker, "/subsystem:windows")

int CALLBACK WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShowCmd)

{
	int argc=0;
	wchar* cmd_line=GetCommandLineA();
	wchar** argv=CommandLineToArgvA(cmd_line,&argc);
	for (int i = 0; i < argc; i++)
	{
		if (!stricmp(argv[i], "-console"))
		{
			if (AllocConsole())
			{
				freopen("CON", "w", stdout);
				freopen("CON", "w", stderr);
				freopen("CON", "r", stdin);
			}
			SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
		}
		else if (!stricmp(argv[i], "-log"))
		{
			const char *logfile;
			if (i < argc - 1)
			{
				logfile = argv[i + 1];
				i++;
			}
			else
				logfile = "reicast-log.txt";
			freopen(logfile, "w", stdout);
			freopen(logfile, "w", stderr);
		}
	}

#endif

	ReserveBottomMemory();
	SetupPath();

#ifdef _WIN64
	AddVectoredExceptionHandler(1, ExeptionHandler);
#else
	SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)&ExeptionHandler);
#endif
#ifndef __GNUC__
	__try
#endif
	{
		int reicast_init(int argc, char* argv[]);
		void *rend_thread(void *);
		void dc_term();

		if (reicast_init(argc, argv) != 0)
			die("Reicast initialization failed");

		#ifdef _WIN64
			setup_seh();
		#endif

		rend_thread(NULL);

		dc_term();
	}
#ifndef __GNUC__
	__except( ExeptionHandler(GetExceptionInformation()) )
	{
		printf("Unhandled exception - Emulation thread halted...\n");
	}
#endif
	SetUnhandledExceptionFilter(0);
	cfgSaveBool("windows", "maximized", window_maximized);
	if (!window_maximized && screen_width != 0 && screen_width != 0)
	{
		cfgSaveInt("windows", "width", screen_width);
		cfgSaveInt("windows", "height", screen_height);
	}

	return 0;
}



LARGE_INTEGER qpf;
double  qpfd;
//Helper functions
double os_GetSeconds()
{
	static bool initme = (QueryPerformanceFrequency(&qpf), qpfd=1/(double)qpf.QuadPart);
	LARGE_INTEGER time_now;

	QueryPerformanceCounter(&time_now);
	return time_now.QuadPart*qpfd;
}

void os_DebugBreak()
{
	__debugbreak();
}

//#include "plugins/plugin_manager.h"

void os_DoEvents()
{
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
}


void VArray2::LockRegion(u32 offset,u32 size)
{
	//verify(offset+size<this->size);
	verify(size!=0);
	DWORD old;
	VirtualProtect(((u8*)data)+offset , size, PAGE_READONLY,&old);
}
void VArray2::UnLockRegion(u32 offset,u32 size)
{
	//verify(offset+size<=this->size);
	verify(size!=0);
	DWORD old;
	VirtualProtect(((u8*)data)+offset , size, PAGE_READWRITE,&old);
}

int get_mic_data(u8* buffer) { return 0; }
int push_vmu_screen(u8* buffer) { return 0; }
