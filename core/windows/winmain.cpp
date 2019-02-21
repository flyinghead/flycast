#include "oslib\oslib.h"
#include "oslib\audiostream.h"
#include "imgread\common.h"
#include "rend\gui.h"
#include "xinput_gamepad.h"

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

void dc_stop(void);
void dc_savestate();
void dc_loadstate();

bool VramLockedWrite(u8* address);
bool ngen_Rewrite(unat& addr,unat retadr,unat acc);
bool BM_LockedWrite(u8* address);
void UpdateController(u32 port);

static void init_kb_map();

void os_SetupInput()
{
	init_kb_map();
#if DC_PLATFORM == DC_PLATFORM_DREAMCAST
	mcfg_CreateDevices();
#endif
	XInputGamepadDevice::CreateDevices();
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
		printf("[GPF]Unhandled access to : 0x%X\n",address);
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

int msgboxf(const wchar* text,unsigned int type,...)
{
	va_list args;

	wchar temp[2048];
	va_start(args, type);
	vsprintf(temp, text, args);
	va_end(args);


	return MessageBox(NULL,temp,VER_SHORTNAME,type | MB_TASKMODAL);
}

// Gamepads
u16 kcode[4];
u32 vks[4];
s8 joyx[4],joyy[4];
u8 rt[4],lt[4];
extern bool coin_chute;
extern bool naomi_test_button;
// Mouse
extern s32 mo_x_abs;
extern s32 mo_y_abs;
extern u32 mo_buttons;
// Keyboard
static u32 kb_used = 0;
extern u8 kb_key[6];		// normal keys pressed
extern u8 kb_shift; 		// shift keys pressed (bitmask)
static u8 kb_map[256];
// Used to differentiate between main enter key and num keypad one
#define VK_NUMPAD_RETURN 0x0E

#define key_CONT_C            (1 << 0)
#define key_CONT_B            (1 << 1)
#define key_CONT_A            (1 << 2)
#define key_CONT_START        (1 << 3)
#define key_CONT_DPAD_UP      (1 << 4)
#define key_CONT_DPAD_DOWN    (1 << 5)
#define key_CONT_DPAD_LEFT    (1 << 6)
#define key_CONT_DPAD_RIGHT   (1 << 7)
#define key_CONT_Z            (1 << 8)
#define key_CONT_Y            (1 << 9)
#define key_CONT_X            (1 << 10)
#define key_CONT_D            (1 << 11)
#define key_CONT_DPAD2_UP     (1 << 12)
#define key_CONT_DPAD2_DOWN   (1 << 13)
#define key_CONT_DPAD2_LEFT   (1 << 14)
#define key_CONT_DPAD2_RIGHT  (1 << 15)
void UpdateInputState(u32 port)
	{
		//joyx[port]=pad.Lx;
		//joyy[port]=pad.Ly;
		lt[port]=GetAsyncKeyState('A')?255:0;
		rt[port]=GetAsyncKeyState('S')?255:0;

		joyx[port]=joyy[port]=0;

		if (GetAsyncKeyState('J'))
			joyx[port]-=126;
		if (GetAsyncKeyState('L'))
			joyx[port]+=126;

		if (GetAsyncKeyState('I'))
			joyy[port]-=126;
		if (GetAsyncKeyState('K'))
			joyy[port]+=126;

		kcode[port]=0xFFFF;
		if (GetAsyncKeyState('V'))
			kcode[port]&=~key_CONT_A;
		if (GetAsyncKeyState('C'))
			kcode[port]&=~key_CONT_B;
		if (GetAsyncKeyState('X'))
			kcode[port]&=~key_CONT_Y;
		if (GetAsyncKeyState('Z'))
			kcode[port]&=~key_CONT_X;

		if (GetAsyncKeyState(VK_SHIFT))
			kcode[port]&=~key_CONT_START;

		if (GetAsyncKeyState(VK_UP))
			kcode[port]&=~key_CONT_DPAD_UP;
		if (GetAsyncKeyState(VK_DOWN))
			kcode[port]&=~key_CONT_DPAD_DOWN;
		if (GetAsyncKeyState(VK_LEFT))
			kcode[port]&=~key_CONT_DPAD_LEFT;
		if (GetAsyncKeyState(VK_RIGHT))
			kcode[port]&=~key_CONT_DPAD_RIGHT;

		UpdateController(port);

		if (GetAsyncKeyState(VK_F1))
			settings.pvr.ta_skip = 100;

//		if (GetAsyncKeyState(VK_F2))
//			settings.pvr.ta_skip = 0;
		if (GetAsyncKeyState(VK_F2))
			dc_savestate();
		if (GetAsyncKeyState(VK_F4))
			dc_loadstate();


		if (GetAsyncKeyState(VK_F10))
			DiscSwap();
		if (GetAsyncKeyState(VK_ESCAPE))
			dc_stop();
#if DC_PLATFORM == DC_PLATFORM_NAOMI || DC_PLATFORM == DC_PLATFORM_ATOMISWAVE
		coin_chute = GetAsyncKeyState(VK_F8);
		naomi_test_button = GetAsyncKeyState(VK_F7);
#endif
		if (GetAsyncKeyState(VK_TAB))
			gui_open_settings();
}

void UpdateController(u32 port)
{
	std::shared_ptr<XInputGamepadDevice> gamepad = XInputGamepadDevice::GetXInputDevice(port);
	if (gamepad != NULL)
		gamepad->ReadInput();
	}

void UpdateVibration(u32 port, u32 value)
{
		u8 POW_POS = (value >> 8) & 0x3;
		u8 POW_NEG = (value >> 12) & 0x3;
		u8 FREQ = (value >> 16) & 0xFF;

		XINPUT_VIBRATION vib;

		double pow = (POW_POS + POW_NEG) / 7.0;
		double pow_l = pow * (0x3B - FREQ) / 17.0;
		double pow_r = pow * (FREQ - 0x07) / 15.0;

		if (pow_l > 1.0) pow_l = 1.0;
		if (pow_r > 1.0) pow_r = 1.0;

		vib.wLeftMotorSpeed = (u16)(65535 * pow_l);
		vib.wRightMotorSpeed = (u16)(65535 * pow_r);

		XInputSetState(port, &vib);
}

// Windows class name to register
#define WINDOW_CLASS "nilDC"

// Width and height of the window
#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720


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

	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MOUSEMOVE:
		{
			int xPos = GET_X_LPARAM(lParam);
			int yPos = GET_Y_LPARAM(lParam);
			mo_x_abs = (xPos - (WINDOW_WIDTH - 640 * WINDOW_HEIGHT / 480) / 2) * 480 / WINDOW_HEIGHT;
			mo_y_abs = yPos * 480 / WINDOW_HEIGHT;
			mo_buttons = 0xffffffff;
			if (wParam & MK_LBUTTON)
				mo_buttons &= ~(1 << 2);
			if (wParam & MK_MBUTTON)
				mo_buttons &= ~(1 << 3);
			if (wParam & MK_RBUTTON)
				mo_buttons &= ~(1 << 1);
		}
		break;

	case WM_KEYDOWN:
	case WM_KEYUP:
		if (wParam == VK_SHIFT)
		{
			if (message == WM_KEYDOWN)
				kb_shift |= 0x02 | 0x20;
			else
				kb_shift &= ~(0x02 | 0x20);
		}
		else if (wParam == VK_CONTROL)
		{
			if (message == WM_KEYDOWN)
				kb_shift |= 0x01 | 0x10;
			else
				kb_shift &= ~(0x01 | 0x10);
		}
		else
		{
			u8 keycode;
			if (wParam == VK_RETURN && (lParam & (1 << 24) != 0))
				keycode = VK_NUMPAD_RETURN;
			else
				keycode = wParam & 0xff;
			u8 dc_keycode = kb_map[keycode];
			if (dc_keycode != 0)
			{
				if (message == WM_KEYDOWN)
				{
					if (kb_used < ARRAY_SIZE(kb_key))
					{
						bool found = false;
						for (int i = 0; !found && i < kb_used; i++)
						{
							if (kb_key[i] == dc_keycode)
								found = true;
						}
						if (!found)
							kb_key[kb_used++] = dc_keycode;
					}
				}
				else
				{
					for (int i = 0; i < kb_used; i++)
					{
						if (kb_key[i] == dc_keycode)
						{
							kb_used--;
							for (int j = i; j < ARRAY_SIZE(kb_key) - 1; j++)
								kb_key[j] = kb_key[j + 1];
							kb_key[ARRAY_SIZE(kb_key) - 1] = 0;
							break;
						}
					}
				}
			}
		}
		break;

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
	sWC.hCursor = 0;
	sWC.lpszMenuName = 0;
	sWC.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
	sWC.lpszClassName = WINDOW_CLASS;
	unsigned int nWidth = WINDOW_WIDTH;
	unsigned int nHeight = WINDOW_HEIGHT;

	ATOM registerClass = RegisterClass(&sWC);
	if (!registerClass)
	{
		MessageBox(0, ("Failed to register the window class"), ("Error"), MB_OK | MB_ICONEXCLAMATION);
	}

	// Create the eglWindow
	RECT sRect;
	SetRect(&sRect, 0, 0, nWidth, nHeight);
	AdjustWindowRectEx(&sRect, WS_CAPTION | WS_SYSMENU, false, 0);
	HWND hWnd = CreateWindow( WINDOW_CLASS, VER_FULLNAME, WS_VISIBLE | WS_SYSMENU,
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
	VirtualProtect(ptr,sizeof(sz),PAGE_EXECUTE_READWRITE,&old);
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

int CALLBACK WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShowCmd)
{
	ReserveBottomMemory();

	int argc=0;
	wchar* cmd_line=GetCommandLineA();
	wchar** argv=CommandLineToArgvA(cmd_line,&argc);
	if(strstr(cmd_line,"NoConsole")==0)
	{
		if (AllocConsole())
		{
			freopen("CON","w",stdout);
			freopen("CON","w",stderr);
			freopen("CON","r",stdin);
		}
		SetConsoleCtrlHandler( (PHANDLER_ROUTINE) CtrlHandler, TRUE );
	}

	SetupPath();

#ifndef __GNUC__
	__try
#else
#ifdef _WIN64
	AddVectoredExceptionHandler(1, ExeptionHandler);
#else
	SetUnhandledExceptionFilter(&ExeptionHandler);
#endif
#endif
	{
		int dc_init(int argc,wchar* argv[]);
		void dc_run();
		void dc_term();
		dc_init(argc,argv);

		#ifdef _WIN64
			setup_seh();
		#endif

		dc_run();
		dc_term();
	}
#ifndef __GNUC__
	__except( ExeptionHandler(GetExceptionInformation()) )
	{
		printf("Unhandled exception - Emulation thread halted...\n");
	}
#endif
	SetUnhandledExceptionFilter(0);

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
			dc_stop();
		}

		// Translate the message and dispatch it to WindowProc()
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}




//Windoze Code implementation of commong classes from here and after ..

//Thread class
cThread::cThread(ThreadEntryFP* function,void* prm)
{
	Entry=function;
	param=prm;
}


void cThread::Start()
{
	hThread=CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)Entry,param,0,NULL);
	ResumeThread(hThread);
}

void cThread::WaitToEnd()
{
	WaitForSingleObject(hThread,INFINITE);
}
//End thread class

//cResetEvent Calss
cResetEvent::cResetEvent(bool State,bool Auto)
{
		hEvent = CreateEvent(
		NULL,             // default security attributes
		Auto?FALSE:TRUE,  // auto-reset event?
		State?TRUE:FALSE, // initial state is State
		NULL			  // unnamed object
		);
}
cResetEvent::~cResetEvent()
{
	//Destroy the event object ?
	 CloseHandle(hEvent);
}
void cResetEvent::Set()//Signal
{
	#if defined(DEBUG_THREADS)
		Sleep(rand() % 10);
	#endif
	SetEvent(hEvent);
}
void cResetEvent::Reset()//reset
{
	#if defined(DEBUG_THREADS)
		Sleep(rand() % 10);
	#endif
	ResetEvent(hEvent);
}
bool cResetEvent::Wait(u32 msec)//Wait for signal , then reset
{
	#if defined(DEBUG_THREADS)
		Sleep(rand() % 10);
	#endif
	return WaitForSingleObject(hEvent,msec) == WAIT_OBJECT_0;
}
void cResetEvent::Wait()//Wait for signal , then reset
{
	#if defined(DEBUG_THREADS)
		Sleep(rand() % 10);
	#endif
	WaitForSingleObject(hEvent,(u32)-1);
}
//End AutoResetEvent

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

static void init_kb_map()
{
	//04-1D Letter keys A-Z (in alphabetic order)
	kb_map['A'] = 0x04;
	kb_map['B'] = 0x05;
	kb_map['C'] = 0x06;
	kb_map['D'] = 0x07;
	kb_map['E'] = 0x08;
	kb_map['F'] = 0x09;
	kb_map['G'] = 0x0A;
	kb_map['H'] = 0x0B;
	kb_map['I'] = 0x0C;
	kb_map['J'] = 0x0D;
	kb_map['K'] = 0x0E;
	kb_map['L'] = 0x0F;
	kb_map['M'] = 0x10;
	kb_map['N'] = 0x11;
	kb_map['O'] = 0x12;
	kb_map['P'] = 0x13;
	kb_map['Q'] = 0x14;
	kb_map['R'] = 0x15;
	kb_map['S'] = 0x16;
	kb_map['T'] = 0x17;
	kb_map['U'] = 0x18;
	kb_map['V'] = 0x19;
	kb_map['W'] = 0x1A;
	kb_map['X'] = 0x1B;
	kb_map['Y'] = 0x1C;
	kb_map['Z'] = 0x1D;

	//1E-27 Number keys 1-0
	kb_map['1'] = 0x1E;
	kb_map['2'] = 0x1F;
	kb_map['3'] = 0x20;
	kb_map['4'] = 0x21;
	kb_map['5'] = 0x22;
	kb_map['6'] = 0x23;
	kb_map['7'] = 0x24;
	kb_map['8'] = 0x25;
	kb_map['9'] = 0x26;
	kb_map['0'] = 0x27;

	kb_map[VK_RETURN] = 0x28;
	kb_map[VK_ESCAPE] = 0x29;
	kb_map[VK_BACK] = 0x2A;
	kb_map[VK_TAB] = 0x2B;
	kb_map[VK_SPACE] = 0x2C;

	kb_map[VK_OEM_MINUS] = 0x2D;	// -
	kb_map[VK_OEM_PLUS] = 0x2E;	// =
	kb_map[VK_OEM_4] = 0x2F;	// [
	kb_map[VK_OEM_6] = 0x30;	// ]

	kb_map[VK_OEM_5] = 0x31;	// \ (US) unsure of keycode

	//32-34 "]", ";" and ":" (the 3 keys right of L)
	kb_map[VK_OEM_8] = 0x32;	// ~ (non-US) *,µ in FR layout
	kb_map[VK_OEM_1] = 0x33;	// ;
	kb_map[VK_OEM_7] = 0x34;	// '

	//35 hankaku/zenkaku / kanji (top left)
	kb_map[VK_OEM_3] = 0x35;	// `~ (US)

	//36-38 ",", "." and "/" (the 3 keys right of M)
	kb_map[VK_OEM_COMMA] = 0x36;
	kb_map[VK_OEM_PERIOD] = 0x37;
	kb_map[VK_OEM_2] = 0x38;

	// CAPSLOCK
	kb_map[VK_CAPITAL] = 0x39;

	//3A-45 Function keys F1-F12
	for (int i = 0;i < 12; i++)
		kb_map[VK_F1 + i] = 0x3A + i;

	//46-4E Control keys above cursor keys
	kb_map[VK_SNAPSHOT] = 0x46;		// Print Screen
	kb_map[VK_SCROLL] = 0x47;		// Scroll Lock
	kb_map[VK_PAUSE] = 0x48;		// Pause
	kb_map[VK_INSERT] = 0x49;
	kb_map[VK_HOME] = 0x4A;
	kb_map[VK_PRIOR] = 0x4B;
	kb_map[VK_DELETE] = 0x4C;
	kb_map[VK_END] = 0x4D;
	kb_map[VK_NEXT] = 0x4E;

	//4F-52 Cursor keys
	kb_map[VK_RIGHT] = 0x4F;
	kb_map[VK_LEFT] = 0x50;
	kb_map[VK_DOWN] = 0x51;
	kb_map[VK_UP] = 0x52;

	//53 Num Lock (Numeric keypad)
	kb_map[VK_NUMLOCK] = 0x53;
	//54 "/" (Numeric keypad)
	kb_map[VK_DIVIDE] = 0x54;
	//55 "*" (Numeric keypad)
	kb_map[VK_MULTIPLY] = 0x55;
	//56 "-" (Numeric keypad)
	kb_map[VK_SUBTRACT] = 0x56;
	//57 "+" (Numeric keypad)
	kb_map[VK_ADD] = 0x57;
	//58 Enter (Numeric keypad)
	kb_map[VK_NUMPAD_RETURN] = 0x58;
	//59-62 Number keys 1-0 (Numeric keypad)
	kb_map[VK_NUMPAD1] = 0x59;
	kb_map[VK_NUMPAD2] = 0x5A;
	kb_map[VK_NUMPAD3] = 0x5B;
	kb_map[VK_NUMPAD4] = 0x5C;
	kb_map[VK_NUMPAD5] = 0x5D;
	kb_map[VK_NUMPAD6] = 0x5E;
	kb_map[VK_NUMPAD7] = 0x5F;
	kb_map[VK_NUMPAD8] = 0x60;
	kb_map[VK_NUMPAD9] = 0x61;
	kb_map[VK_NUMPAD0] = 0x62;
	//63 "." (Numeric keypad)
	kb_map[VK_DECIMAL] = 0x63;
	//64 #| (non-US)
	//kb_map[94] = 0x64;
	//65 S3 key
	//66-A4 Not used
	//A5-DF Reserved
	//E0 Left Control
	//E1 Left Shift
	//E2 Left Alt
	//E3 Left S1
	//E4 Right Control
	//E5 Right Shift
	//E6 Right Alt
	//E7 Right S3
	//E8-FF Reserved
}
