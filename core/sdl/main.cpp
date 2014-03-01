#include "types.h"
#include "cfg/cfg.h"

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

#include <SDL/SDL.h>
#include <EGL/egl.h>

#ifdef USE_OSS
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#endif

#include <signal.h>
#include <execinfo.h>

#include "hw/mem/_vmem.h"
	
#ifdef TARGET_PANDORA
#define WINDOW_WIDTH	800
#else
#define WINDOW_WIDTH	640
#endif
#define WINDOW_HEIGHT	480

void* x11_win=(NativeWindowType)NULL,* x11_disp=EGL_DEFAULT_DISPLAY;
SDL_Surface *screen=NULL;

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

extern bool KillTex;

extern void dc_term();

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

static SDL_Joystick *JoySDL    = 0;

#ifdef USE_OSS
static int audio_fd = -1;
#endif


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
#ifdef TARGET_PANDORA
u32  JSensitivity[256];  // To have less sensitive value on nubs
#endif

void SetupInput()
{
	for (int port=0;port<4;port++)
	{
		kcode[port]=0xFFFF;
		rt[port]=0;
		lt[port]=0;
	}

	// Open joystick device
	int numjoys = SDL_NumJoysticks();
	printf("Number of Joysticks found = %i\n", numjoys);
	if (numjoys > 0)
		JoySDL = SDL_JoystickOpen(0);
	printf("Joystick openned\n");	
	if(JoySDL)
	{
		int AxisCount,ButtonCount;
		const char* Name;

		AxisCount   = 0;
		ButtonCount = 0;
//		Name[0]     = '\0';

		AxisCount = SDL_JoystickNumAxes(JoySDL);
		ButtonCount = SDL_JoystickNumButtons(JoySDL);
		Name = SDL_JoystickName(0);
		
		printf("SDK: Found '%s' joystick with %d axis and %d buttons\n",Name,AxisCount,ButtonCount);

		if (strcmp(Name,"Microsoft X-Box 360 pad")==0)
		{
			JMapBtn=JMapBtn_360;
			JMapAxis=JMapAxis_360;
			printf("Using Xbox 360 map\n");
		}
	} else printf("SDK: No Joystick Found\n");
	#ifdef TARGET_PANDORA
	float v;
	int j;
	for (int i=0; i<128; i++) {
		v=((float)i)/127.0f;
		v=(v+v*v)/2.0f;
		j=(int)(v*127.0f);
		if (j>127) j=127;
		JSensitivity[128-i]=-j;
		JSensitivity[128+i]=j;
	}
	#endif
	SDL_ShowCursor( 0 );
	if (SDL_WM_GrabInput( SDL_GRAB_ON ) != SDL_GRAB_ON)
		printf("SDK: Error while grabbing mouse\n");
}

extern char OSD_Info[128];
extern int OSD_Delay;
extern char OSD_Counters[256];
extern int OSD_Counter;

bool HandleEvents(u32 port) {

	static int keys[13];
	static int mouse_use = 0;
	SDL_Event event;
	int k, value;
	int xx, yy;
	char *num_mode[] = {"Off", "Up/Down => RT/LT", "Left/Right => LT/RT", "U/D/L/R => A/B/X/Y"};
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT:
							 die("death by SDL request");
							 break;
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				k = event.key.keysym.sym;
				value = (event.type==SDL_KEYDOWN)?1:0;
				 //printf("type %i key %i \n", event.type, k);
				switch (k) {
				#if defined(TARGET_PANDORA)
					case SDLK_SPACE: 	keys[0]=value; break;
					case SDLK_UP:		keys[1]=value; break;
					case SDLK_DOWN:		keys[2]=value; break;
					case SDLK_LEFT:		keys[3]=value; break;
					case SDLK_RIGHT:	keys[4]=value; break;
					case SDLK_PAGEUP:	keys[5]=value; break;
					case SDLK_PAGEDOWN:	keys[6]=value; break;
					case SDLK_END:		keys[7]=value; break;
					case SDLK_HOME:		keys[8]=value; break;
					case SDLK_MENU:
					case SDLK_ESCAPE:	keys[9]=value; break;
					case SDLK_RSHIFT:	keys[11]=value; break;
					case SDLK_RCTRL:	keys[10]=value; break;
					case SDLK_LALT:		keys[12]=value; break;
					case SDLK_k:		KillTex=true; break;
					case SDLK_n:    if (value) {mouse_use=(mouse_use+1)%4; snprintf(OSD_Info, 128, "Right Nub mode: %s\n", num_mode[mouse_use]); OSD_Delay=300;}; break;  
					case SDLK_s:        if (value) {settings.aica.NoSound=!settings.aica.NoSound; snprintf(OSD_Info, 128, "Sound %s\n", (settings.aica.NoSound)?"Off":"On"); OSD_Delay=300;};break;
					case SDLK_c:    if (value) {OSD_Counter=1-OSD_Counter;};break;

				#else
				#error *TODO*
				#endif
				}
				break;
			case SDL_JOYBUTTONDOWN:
			case SDL_JOYBUTTONUP:
				value = (event.type==SDL_JOYBUTTONDOWN)?1:0;
				k = event.jbutton.button;
				{
					u32 mt=JMapBtn[k]>>16;
					u32 mo=JMapBtn[k]&0xFFFF;
					
					// printf("BUTTON %d,%d\n",JE.number,JE.value);
					
					if (mt==0)
					{
						// printf("Mapped to %d\n",mo);
						if (value)
						kcode[port]&=~mo;
						else
						kcode[port]|=mo;
					}
					else if (mt==1)
					{
						// printf("Mapped to %d %d\n",mo,JE.value?255:0);
						if (mo==0)
						lt[port]=value?255:0;
						else if (mo==1)
						rt[port]=value?255:0;
					}
					
				}
				break;
			case SDL_JOYAXISMOTION:
				k = event.jaxis.axis;
				value = event.jaxis.value;
				{
					u32 mt=JMapAxis[k]>>16;
					u32 mo=JMapAxis[k]&0xFFFF;
					
					//printf("AXIS %d,%d\n",JE.number,JE.value);
					s8 v=(s8)(value/256); //-127 ... + 127 range
					#ifdef TARGET_PANDORA
					v=JSensitivity[128+v];
					#endif
					
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
      case SDL_MOUSEMOTION:
          xx = event.motion.xrel;
          yy = event.motion.yrel;
          // some caping and dead zone...
          if (abs(xx)<4) xx = 0;
          if (abs(yy)<4) yy = 0;
          xx = xx*255/20; yy = yy*255/20;
          if (xx>255) xx = 255; if (xx<-255) xx = -255;
          if (yy>255) yy = 255; if (yy<-255) yy = -255;
          //if (abs(xx)>0 || abs(yy)>0) printf("mouse %i, %i\n", xx, yy);
          switch (mouse_use) {
            case 0:  // nothing
              break;
            case 1:  // Up=RT, Down=LT
              if (yy<0) rt[port]=-yy;
              if (yy>0) lt[port]=yy;
              break;
            case 2:  // Left=LT, Right=RT
              if (xx<0) lt[port]=-xx;
              if (xx>0) rt[port]=xx;
              break;
            case 3:  // Nub = ABXY
              if (xx<-127) kcode[port] &= ~Btn_X;
              if (xx>127) kcode[port] &= ~Btn_B;
              if (yy<-127) kcode[port] &= ~Btn_Y;
              if (yy>127) kcode[port] &= ~Btn_A;
              break;
          }
        break;
		}
			
	}
			
	if (keys[0]) { kcode[port] &= ~Btn_C; }
	if (keys[6]) { kcode[port] &= ~Btn_A; }
	if (keys[7]) { kcode[port] &= ~Btn_B; }
	if (keys[5]) { kcode[port] &= ~Btn_Y; }
	if (keys[8]) { kcode[port] &= ~Btn_X; }
	if (keys[1]) { kcode[port] &= ~DPad_Up;    }
	if (keys[2]) { kcode[port] &= ~DPad_Down;  }
	if (keys[3]) { kcode[port] &= ~DPad_Left;  }
	if (keys[4]) { kcode[port] &= ~DPad_Right; }
	if (keys[12]){ kcode[port] &= ~Btn_Start; }
	if (keys[9]){ 
			//die("death by escape key"); 
			//printf("death by escape key\n"); 
			// clean exit
			dc_term();
		
			// is there a proper way to exit? dc_term() doesn't end the dc_run() loop it seems
			die("death by escape key"); 
		} 
	if (keys[10]) rt[port]=255;
	if (keys[11]) lt[port]=255;
	
	return true;
}

void UpdateInputState(u32 port)
{
	static char key = 0;

	kcode[port]=0xFFFF;
	rt[port]=0;
	lt[port]=0;
	
	HandleEvents(port);
}

void os_DoEvents()
{

}

void os_SetWindowText(const char * text)
{
	#ifndef TARGET_PANDORA
	SDL_WM_SetCaption(text, NULL);		// *TODO*  Set Icon also...
	#endif
	#ifdef TARGET_PANDORA
	strncpy(OSD_Counters, text, 256);
	#endif
}


int ndcid=0;
void os_CreateWindow()
{
	#ifdef TARGET_PANDORA
	int width=800;
	int height=480;
	int flags=SDL_FULLSCREEN;
	#else
	int width=cfgLoadInt("x11","width", WINDOW_WIDTH);
	int height=cfgLoadInt("x11","height", WINDOW_HEIGHT);
	int flags=SDL_SWSURFACE;
	#endif
	screen = SDL_SetVideoMode(width, height, 0, flags);
	if (!screen)
		die("error creating SDL screen");
	printf("Created SDL Windows (%ix%i) successfully\n", width, height);
}


void common_linux_setup();
int dc_init(int argc,wchar* argv[]);
void dc_run();

void gl_term();

void clean_exit(int sig_num) {
	void *array[10];
	size_t size;
	
	// close files
	if (JoySDL) 		SDL_JoystickClose(JoySDL);
	#ifdef USE_OSS
	if (audio_fd>=0) 	close(audio_fd);
	#endif

	// Close EGL context ???
	if (sig_num!=0)
		gl_term();

	SDL_Quit();
}

#ifdef USE_OSS
void init_sound()
{
    if((audio_fd=open("/dev/dsp",O_WRONLY))<0)
		printf("Couldn't open /dev/dsp.\n");
    else
	{
	  printf("sound enabled, dsp openned for write\n");
	  int tmp=44100;
	  int err_ret;
	  err_ret=ioctl(audio_fd,SNDCTL_DSP_SPEED,&tmp);
	  printf("set Frequency to %i, return %i (rate=%i)\n", 44100, err_ret, tmp);
	  int channels=2;
	  err_ret=ioctl(audio_fd, SNDCTL_DSP_CHANNELS, &channels);	  
	  printf("set dsp to stereo (%i => %i)\n", channels, err_ret);
	  int format=AFMT_S16_LE;
	  err_ret=ioctl(audio_fd, SNDCTL_DSP_SETFMT, &format);
	  printf("set dsp to %s audio (%i/%i => %i)\n", "16bits signed" ,AFMT_S16_LE, format, err_ret);
	  int frag=(4<<16)|11;
	  int f=frag;
	  err_ret=ioctl(audio_fd, SNDCTL_DSP_SETFRAGMENT, &frag);
	  printf("set dsp fragment to %i of %i bytes (0x%x => %i)\n", "16bits signed" ,(f>>16), 1<<(f&0xff), frag, err_ret);
	  /*
	  // this doesn't help stutering, and the emu goes too fast after that
	  err_ret=ioctl(audio_fd, SNDCTL_DSP_NONBLOCK, NULL);
	  printf("set dsp to non-blocking ( => %i)\n", err_ret);
	  */
	}
}
#endif

int main(int argc, wchar* argv[])
{
	//if (argc==2) 
		//ndcid=atoi(argv[1]);

#if defined(USES_HOMEDIR)
	string home = (string)getenv("HOME");
	if(home.c_str())
	{
		home += "/.reicast";
		mkdir(home.c_str(), 0755); // create the directory if missing
		SetHomeDir(home);
	}
	else
		SetHomeDir(".");
#else
	SetHomeDir(".");
#endif

	printf("Home dir is: %s\n",GetPath("/").c_str());

	common_linux_setup();

	printf("common linux setup done\n");
	
	settings.profile.run_counts=0;
		
	dc_init(argc,argv);

	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK|SDL_INIT_NOPARACHUTE)==-1)
	die("error initializing SDL");
	
	SetupInput();
	
	#ifdef USE_OSS	
		init_sound();
	#endif
	
	dc_run();
	
	clean_exit(0);

	return 0;
}

u32 os_Push(void* frame, u32 samples, bool wait)
{
#ifdef USE_OSS
static bool blocking = true;
	if (wait!=blocking) {
		fcntl(audio_fd, F_SETFD, O_WRONLY | wait?0:O_NONBLOCK);
		blocking=wait;
	}
	write(audio_fd, frame, samples*4);
#endif
return 1;
}
