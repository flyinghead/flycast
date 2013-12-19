#include "types.h"
#include "cfg/cfg.h"

#if HOST_OS==OS_LINUX
#include <poll.h>
#include <termios.h>
//#include <curses.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/time.h>
#include "hw/sh4/dyna/blockmanager.h"
#include <unistd.h>




#if defined(SUPPORT_X11)
	#include <X11/Xlib.h>
	#include <X11/Xatom.h>
	#include <X11/Xutil.h>
#endif

#if !defined(ANDROID)
	#include <linux/joystick.h>
#endif

#define WINDOW_WIDTH	640
#define WINDOW_HEIGHT	480

void* x11_win,* x11_disp;
void* libPvr_GetRenderTarget() 
{ 
	return x11_win; 
}

void* libPvr_GetRenderSurface() 
{ 
	return x11_disp;
}

int msgboxf(const wchar* text,unsigned int type,...)
{
	va_list args;

	wchar temp[2048];
	va_start(args, type);
	vsprintf(temp, text, args);
	va_end(args);

	//printf(NULL,temp,VER_SHORTNAME,type | MB_TASKMODAL);
	puts(temp);
	return MBX_OK;
}



u16 kcode[4];
u32 vks[4];
s8 joyx[4],joyy[4];
u8 rt[4],lt[4];

enum DCPad {
	Btn_C		= 1,
	Btn_B		= 1<<1,
	Btn_A		= 1<<2,
	Btn_Start	= 1<<3,
	DPad_Up		= 1<<4,
	DPad_Down	= 1<<5,
	DPad_Left	= 1<<6,
	DPad_Right	= 1<<7,
	Btn_Z		= 1<<8,
	Btn_Y		= 1<<9,
	Btn_X		= 1<<10,
	Btn_D		= 1<<11,
	DPad2_Up	= 1<<12,
	DPad2_Down	= 1<<13,
	DPad2_Left	= 1<<14,
	DPad2_Right	= 1<<15,

	Axis_LT= 0x10000,
	Axis_RT= 0x10001,
	Axis_X= 0x20000,
	Axis_Y= 0x20001,
};


void emit_WriteCodeCache();

static int JoyFD    = -1;     // Joystick file descriptor
static int kbfd = -1; 

#define MAP_SIZE 32

const u32 JMapBtn_USB[MAP_SIZE] =
  { Btn_Y,Btn_B,Btn_A,Btn_X,0,0,0,0,0,Btn_Start };

const u32 JMapAxis_USB[MAP_SIZE] =
  { Axis_X,Axis_Y,0,0,0,0,0,0,0,0 };

const u32 JMapBtn_360[MAP_SIZE] =
  { Btn_A,Btn_B,Btn_X,Btn_Y,0,0,0,Btn_Start,0,0 };

const u32 JMapAxis_360[MAP_SIZE] =
  { Axis_X,Axis_Y,Axis_LT,0,0,Axis_RT,DPad_Left,DPad_Up,0,0 };

const u32* JMapBtn=JMapBtn_USB;
const u32* JMapAxis=JMapAxis_USB;


void SetupInput()
{
	for (int port=0;port<4;port++)
	{
		kcode[port]=0xFFFF;
		rt[port]=0;
		lt[port]=0;
	}

	if (true) {
		const char* device = "/dev/event2";
		char name[256]= "Unknown";

		if ((kbfd = open(device, O_RDONLY)) > 0) {
			fcntl(kbfd,F_SETFL,O_NONBLOCK);
			if(ioctl(kbfd, EVIOCGNAME(sizeof(name)), name) < 0) {
				perror("evdev ioctl");
			}

			printf("The device on %s says its name is %s\n",device, name);

		}
		else
			perror("evdev open");
	}

	// Open joystick device
	JoyFD = open("/dev/input/js0",O_RDONLY);
		
	if(JoyFD>=0)
	{
		int AxisCount,ButtonCount;
		char Name[128];

		AxisCount   = 0;
		ButtonCount = 0;
		Name[0]     = '\0';

		fcntl(JoyFD,F_SETFL,O_NONBLOCK);
		ioctl(JoyFD,JSIOCGAXES,&AxisCount);
		ioctl(JoyFD,JSIOCGBUTTONS,&ButtonCount);
		ioctl(JoyFD,JSIOCGNAME(sizeof(Name)),&Name);
		
		printf("SDK: Found '%s' joystick with %d axis and %d buttons\n",Name,AxisCount,ButtonCount);

		if (strcmp(Name,"Microsoft X-Box 360 pad")==0)
		{
			JMapBtn=JMapBtn_360;
			JMapAxis=JMapAxis_360;
			printf("Using Xbox 360 map\n");
		}
	}
}

bool HandleKb(u32 port) {
	struct input_event ie;
	if (kbfd < 0)
		return false;

  	while(read(kbfd,&ie,sizeof(ie))==sizeof(ie)) {
		printf("type %i key %i state %i\n", ie.type, ie.code, ie.value);
	}

}

bool HandleJoystick(u32 port)
{
  
  struct js_event JE;

  // Joystick must be connected
  if(JoyFD<0) return false;

  while(read(JoyFD,&JE,sizeof(JE))==sizeof(JE))
	  if (JE.number<MAP_SIZE)
	  {
		  switch(JE.type & ~JS_EVENT_INIT)
		  {
		  case JS_EVENT_AXIS:
			  {
				  u32 mt=JMapAxis[JE.number]>>16;
				  u32 mo=JMapAxis[JE.number]&0xFFFF;
				  
				 //printf("AXIS %d,%d\n",JE.number,JE.value);
				  s8 v=(s8)(JE.value/256); //-127 ... + 127 range
				  
				  if (mt==0)
				  {
					  kcode[port]|=mo;
					  kcode[port]|=mo*2;
					  if (v<-64)
					  {
						  kcode[port]&=~mo;
					  }
					  else if (v>64)
					  {
						  kcode[port]&=~(mo*2);
					  }

					 // printf("Mapped to %d %d %d\n",mo,kcode[port]&mo,kcode[port]&(mo*2));
				  }
				  else if (mt==1)
				  {
					  if (v>=0) v++;	//up to 255

					//   printf("AXIS %d,%d Mapped to %d %d %d\n",JE.number,JE.value,mo,v,v+127);

					  if (mo==0)
						  lt[port]=v+127;
					  else if (mo==1)
						  rt[port]=v+127;
				  }
				  else if (mt==2)
				  {
					//  printf("AXIS %d,%d Mapped to %d %d [%d]",JE.number,JE.value,mo,v);
					  if (mo==0)
						  joyx[port]=v;
					  else if (mo==1)
						  joyy[port]=v;
				  }
			  }
			  break;

		  case JS_EVENT_BUTTON:
			  {
				  u32 mt=JMapBtn[JE.number]>>16;
				  u32 mo=JMapBtn[JE.number]&0xFFFF;

				// printf("BUTTON %d,%d\n",JE.number,JE.value);

				  if (mt==0)
				  {
					 // printf("Mapped to %d\n",mo);
					  if (JE.value)
						  kcode[port]&=~mo;
					  else
						  kcode[port]|=mo;
				  }
				  else if (mt==1)
				  {
					 // printf("Mapped to %d %d\n",mo,JE.value?255:0);
					  if (mo==0)
						  lt[port]=JE.value?255:0;
					  else if (mo==1)
						  rt[port]=JE.value?255:0;
				  }

			  }
			  break;
		  }
	  }

	  return true;
}

extern bool KillTex;

void UpdateInputState(u32 port)
{
	static char key = 0;

	if (HandleJoystick(port)) return;
	if (HandleKb(port)) return;

	kcode[port]=0xFFFF;
	rt[port]=0;
	lt[port]=0;

	for(;;)
	{
		key = 0;
		read(STDIN_FILENO, &key, 1);

		if (0  == key || EOF == key) break;
		if ('k' == key) KillTex=true;

		if ('b' == key) { kcode[port] &= ~Btn_C; }
		if ('v' == key) { kcode[port] &= ~Btn_A; }
		if ('c' == key) { kcode[port] &= ~Btn_B; }
		if ('x' == key) { kcode[port] &= ~Btn_Y; }
		if ('z' == key) { kcode[port] &= ~Btn_X; }
		if ('i' == key) { kcode[port] &= ~DPad_Up;    }
		if ('k' == key) { kcode[port] &= ~DPad_Down;  }
		if ('j' == key) { kcode[port] &= ~DPad_Left;  }
		if ('l' == key) { kcode[port] &= ~DPad_Right; }

		if (0x0A== key) { kcode[port] &= ~Btn_Start;  }
		//if (0x1b == key){ die("death by escape key"); } //this actually quits when i press left for some reason

		if ('a' == key) rt[port]=255;
		if ('s' == key) lt[port]=255;
#if !defined(HOST_NO_REC)
		if ('b' == key)	emit_WriteCodeCache();
		if ('n' == key)	bm_Reset();
		if ('m' == key)	bm_Sort();
		if (',' == key)	{ emit_WriteCodeCache(); bm_Sort(); }
#endif
	}
}


void os_DoEvents()
{

}

void os_SetWindowText(const char * text)
{
	if (0==x11_win || 0==x11_disp || 1)
		printf("%s\n",text);
#if defined(SUPPORT_X11)
	else {
		XChangeProperty((Display*)x11_disp, (Window)x11_win,
			XInternAtom((Display*)x11_disp, "WM_NAME",		False),		//WM_NAME,
			XInternAtom((Display*)x11_disp, "UTF8_STRING",	False),		//UTF8_STRING,
			8, PropModeReplace, (const unsigned char *)text, strlen(text));
	}
#endif
}


int ndcid=0;
void os_CreateWindow()
{
#if defined(SUPPORT_X11)
	if (cfgLoadInt("pvr","nox11",0)==0)
		{
			// X11 variables
			Window				x11Window	= 0;
			Display*			x11Display	= 0;
			long				x11Screen	= 0;
			XVisualInfo*		x11Visual	= 0;
			Colormap			x11Colormap	= 0;

			/*
			Step 0 - Create a NativeWindowType that we can use it for OpenGL ES output
			*/
			Window					sRootWindow;
			XSetWindowAttributes	sWA;
			unsigned int			ui32Mask;
			int						i32Depth;

			// Initializes the display and screen
			x11Display = XOpenDisplay( 0 );
			if (!x11Display && !(x11Display = XOpenDisplay( ":0" )))
			{
				printf("Error: Unable to open X display\n");
				return;
			}
			x11Screen = XDefaultScreen( x11Display );

			// Gets the window parameters
			sRootWindow = RootWindow(x11Display, x11Screen);
			i32Depth = DefaultDepth(x11Display, x11Screen);
			x11Visual = new XVisualInfo;
			XMatchVisualInfo( x11Display, x11Screen, i32Depth, TrueColor, x11Visual);
			if (!x11Visual)
			{
				printf("Error: Unable to acquire visual\n");
				return;
			}
			x11Colormap = XCreateColormap( x11Display, sRootWindow, x11Visual->visual, AllocNone );
			sWA.colormap = x11Colormap;

			// Add to these for handling other events
			sWA.event_mask = StructureNotifyMask | ExposureMask | ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask;
			ui32Mask = CWBackPixel | CWBorderPixel | CWEventMask | CWColormap;

			int width=cfgLoadInt("x11","width", WINDOW_WIDTH);
			int height=cfgLoadInt("x11","height", WINDOW_HEIGHT);

			if (width==-1)
			{
				width=XDisplayWidth(x11Display,x11Screen);
				height=XDisplayHeight(x11Display,x11Screen);
			}
			// Creates the X11 window
			x11Window = XCreateWindow( x11Display, RootWindow(x11Display, x11Screen), (ndcid%3)*640, (ndcid/3)*480, width, height,
				0, CopyFromParent, InputOutput, CopyFromParent, ui32Mask, &sWA);
			XMapWindow(x11Display, x11Window);
			XFlush(x11Display);

			//(EGLNativeDisplayType)x11Display;
			x11_disp=(void*)x11Display;
			x11_win=(void*)x11Window;
		}
		else
			printf("Not creating X11 window ..\n");
#endif
}

termios tios, orig_tios;

int setup_curses()
{
    //initscr();
    //cbreak();
    //noecho();


    /* Get current terminal settings */
    if (tcgetattr(STDIN_FILENO, &orig_tios)) {
        printf("Error getting current terminal settings\n");
        return -1;
    }

    memcpy(&tios, &orig_tios, sizeof(struct termios));
    tios.c_lflag &= ~ICANON;    //(ECHO|ICANON);&= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ECHOPRT | ECHOKE | ICRNL);

    tios.c_cc[VTIME] = 0;
    tios.c_cc[VMIN]  = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &tios)) {
        printf("Error applying terminal settings\n");
        return -2;
    }

    if (tcgetattr(STDIN_FILENO, &tios)) {
        tcsetattr(0, TCSANOW, &orig_tios);
        printf("Error while asserting terminal settings\n");
        return -3;
    }

    if ((tios.c_lflag & ICANON) || !(tios.c_lflag & ECHO)) {
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_tios);
        printf("Could not apply all terminal settings\n");
        return -4;
    }

    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    return 1;
}

void common_linux_setup();
int dc_init(int argc,wchar* argv[]);
void dc_run();

int main(int argc, wchar* argv[])
{
	//if (argc==2) 
		//ndcid=atoi(argv[1]);

	if (setup_curses() < 0) die("failed to setup curses!\n");
	SetHomeDir(".");

	printf("Home dir is: %s\n",GetPath("/").c_str());

	common_linux_setup();

	SetupInput();
	
	settings.profile.run_counts=0;
		
	dc_init(argc,argv);

	dc_run();

	return 0;
}

u32 os_Push(void* frame, u32 samples, bool wait)
{
return 1;
}
#endif
