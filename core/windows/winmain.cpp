#include "oslib\oslib.h"
#include "oslib\audiostream_rif.h"

#define _WIN32_WINNT 0x0500 
#include <windows.h>


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



bool VramLockedWrite(u8* address);
bool ngen_Rewrite(unat& addr,unat retadr,unat acc);
bool BM_LockedWrite(u8* address);

int ExeptionHandler(u32 dwCode, void* pExceptionPointers)
{
	EXCEPTION_POINTERS* ep=(EXCEPTION_POINTERS*)pExceptionPointers;

	EXCEPTION_RECORD* pExceptionRecord=ep->ExceptionRecord;

	if (dwCode != EXCEPTION_ACCESS_VIOLATION)
		return EXCEPTION_CONTINUE_SEARCH;

	u8* address=(u8*)pExceptionRecord->ExceptionInformation[1];

	if (VramLockedWrite(address))
	{
		return EXCEPTION_CONTINUE_EXECUTION;
	}
	else if (BM_LockedWrite(address))
	{
		return EXCEPTION_CONTINUE_EXECUTION;
	}
	else if ( ngen_Rewrite((unat&)ep->ContextRecord->Eip,*(unat*)ep->ContextRecord->Esp,ep->ContextRecord->Eax) )
	{
		//remove the call from call stack
		ep->ContextRecord->Esp+=4;
		//restore the addr from eax to ecx so its valid again
		ep->ContextRecord->Ecx=ep->ContextRecord->Eax;
		return EXCEPTION_CONTINUE_EXECUTION;
	}
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
	SetHomeDir(fn);
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

u16 kcode[4];
u32 vks[4];
s8 joyx[4],joyy[4];
u8 rt[4],lt[4];
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

		if (GetAsyncKeyState('1'))
			settings.pvr.ta_skip = 1;

		if (GetAsyncKeyState('2'))
			settings.pvr.ta_skip = 0;
	}



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

	default:
		break;
	}

	// Calls the default window procedure for messages we did not handle
	return DefWindowProc(hWnd, message, wParam, lParam);
}

// Windows class name to register
#define WINDOW_CLASS "nilDC"

// Width and height of the window
#define WINDOW_WIDTH  1024
#define WINDOW_HEIGHT 512


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

	void os_InitAudio();
	os_InitAudio();
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


u64 cycl_glob;
cResetEvent evt_hld(false,true);


double speed_load_mspdf;
extern double full_rps;

void os_wait_cycl(u32 cycl)
{
	if (cycl>8*1000*1000)
		cycl=8*1000*1000;

	
	static double trolol=os_GetSeconds();

	double newt=os_GetSeconds();
	double ets=(newt-trolol)*200*1000*1000;

	bool fast_enough=ets < cycl;
	
	bool wait = full_rps >5 && (fast_enough || os_IsAudioBufferedLots());

	speed_load_mspdf=(speed_load_mspdf*0.96235 + ets/cycl*10)/1.96235;

	if (wait &&  os_IsAudioBuffered())
	{
		while (cycl_glob<cycl && os_IsAudioBuffered())
			evt_hld.Wait(8);

		if (cycl_glob>cycl)
			InterlockedExchangeSubtract(&cycl_glob,cycl);
	}
	else //if (os_IsAudioBufferedLots())
	{
		//cycl_glob=0;
	}


	static int last_fe=fast_enough;
	if (!fast_enough || !last_fe)
		printf("Speed %.2f (%.2f%%) (%d)\n",ets/cycl*10,cycl/ets*100,os_getusedSamples());
	
	last_fe=fast_enough;



	trolol=os_GetSeconds();
}


void os_consume(double t)
{
	double cyc=t*190*1000*1000;

	if ((cycl_glob+cyc)<10*1000*1000)
	{
		InterlockedExchangeAdd(&cycl_glob,cyc);
	}
	else
	{
		cycl_glob=10*1000*1000;
	}

	evt_hld.Set();
}

void* tick_th(void* p)
{
		SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_TIME_CRITICAL);
		double old=os_GetSeconds();
		for(;;)
		{
			Sleep(4);
			double newt=os_GetSeconds();
			os_consume(newt-old);
			old=newt;
		}
}

cThread tick_thd(&tick_th,0);

int CALLBACK WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShowCmd)
{
	tick_thd.Start();

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

	__try
	{	
		int dc_init(int argc,wchar* argv[]);
		void dc_run();
		void dc_term();
		dc_init(argc,argv);
		dc_run();
		dc_term();
	}
	__except( (EXCEPTION_CONTINUE_EXECUTION==ExeptionHandler( GetExceptionCode(), (GetExceptionInformation()))) ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH )
	{
		printf("Unhandled exception - Emulation thread halted...\n");
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

//#include "plugins/plugin_manager.h"

void os_DoEvents()
{
	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		// If the message is WM_QUIT, exit the while loop
		if (msg.message == WM_QUIT)
		{
			sh4_cpu.Stop();
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
	hThread=CreateThread(NULL,NULL,(LPTHREAD_START_ROUTINE)Entry,param,0,NULL);
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
	SetEvent(hEvent);
}
void cResetEvent::Reset()//reset
{
	ResetEvent(hEvent);
}
void cResetEvent::Wait(u32 msec)//Wait for signal , then reset
{
	WaitForSingleObject(hEvent,msec);
}
void cResetEvent::Wait()//Wait for signal , then reset
{
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