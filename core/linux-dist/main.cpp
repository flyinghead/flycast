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

#if defined(TARGET_EMSCRIPTEN)
  #include <emscripten.h>
#endif

#if defined(SUPPORT_X11)
  #include <X11/Xlib.h>
  #include <X11/Xatom.h>
  #include <X11/Xutil.h>

  #if !defined(GLES)
    #include <GL/gl.h>
    #include <GL/glx.h>
  #endif

  #include <map>
  map<int, int> x11_keymap;
#endif

#if !defined(ANDROID) && HOST_OS != OS_DARWIN && !defined(TARGET_EMSCRIPTEN)
  #include <sys/stat.h>
  #include <sys/types.h>
#endif

#if defined(USE_EVDEV)
  #ifdef TARGET_PANDORA
    #define EVDEV_DEVICE "/dev/input/event4"
  #else
    #define EVDEV_DEVICE "/dev/event2"
  #endif
#endif

#if defined(USE_JOYSTICK)
  #include <linux/joystick.h>
#endif

#ifdef TARGET_PANDORA
  #include <signal.h>
  #include <execinfo.h>
  #include <sys/soundcard.h>
  #define WINDOW_WIDTH  800
#else
  #define WINDOW_WIDTH  640
#endif
#define WINDOW_HEIGHT 480

void* x11_win = 0;
void* x11_disp = 0;

void* libPvr_GetRenderTarget()
{
  return x11_win;
}

void* libPvr_GetRenderSurface()
{
  return x11_disp;
}

int msgboxf(const wchar* text, unsigned int type, ...)
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



u16 kcode[4] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
u8 rt[4] = {0, 0, 0, 0};
u8 lt[4] = {0, 0, 0, 0};
u32 vks[4];
s8 joyx[4], joyy[4];

enum DCPad
{
  Btn_C       = 1,
  Btn_B       = 1<<1,
  Btn_A       = 1<<2,
  Btn_Start   = 1<<3,
  DPad_Up     = 1<<4,
  DPad_Down   = 1<<5,
  DPad_Left   = 1<<6,
  DPad_Right  = 1<<7,
  Btn_Z       = 1<<8,
  Btn_Y       = 1<<9,
  Btn_X       = 1<<10,
  Btn_D       = 1<<11,
  DPad2_Up    = 1<<12,
  DPad2_Down  = 1<<13,
  DPad2_Left  = 1<<14,
  DPad2_Right = 1<<15,

  Axis_LT = 0x10000,
  Axis_RT = 0x10001,
  Axis_X  = 0x20000,
  Axis_Y  = 0x20001,
};


void emit_WriteCodeCache();

static int joystick_fd = -1; // Joystick file descriptor
static int evdev_fd = -1;


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

int setup_input_evdev(const char* device)
{
  char name[256] = "Unknown";

  int fd = open(device, O_RDONLY);

  if (fd >= 0)
  {
    fcntl(fd, F_SETFL, O_NONBLOCK);
    if(ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0)
    {
      perror("evdev: ioctl");
    }
    printf("evdev: Found '%s' at '%s'\n", name, device);
  }
  else
  {
    perror("evdev: open");
  }

  return fd;
}

int setup_input_joystick(const char* device)
{
  int axis_count = 0;
  int button_count = 0;
  char name[128] = "Unknown";

  int fd = open(device, O_RDONLY);

  if(fd >= 0)
  {
    fcntl(fd, F_SETFL, O_NONBLOCK);
    ioctl(fd, JSIOCGAXES, &axis_count);
    ioctl(fd, JSIOCGBUTTONS, &button_count);
    ioctl(fd, JSIOCGNAME(sizeof(name)), &name);

    printf("joystick: Found '%s' with %d axis and %d buttons at '%s'.\n", name, axis_count, button_count, device);

    if (strcmp(name, "Microsoft X-Box 360 pad") == 0)
    {
      JMapBtn = JMapBtn_360;
      JMapAxis = JMapAxis_360;
      printf("joystick: Using Xbox 360 map\n");
    }
  }
  else
  {
    perror("joystick: open");
  }

  return fd;
}

void SetupInput()
{
  #if defined(USE_EVDEV)
    evdev_fd = setup_input_evdev(EVDEV_DEVICE);
  #endif

  #if defined(USE_JOYSTICK)
    joystick_fd = setup_input_joystick("/dev/input/js0");
  #endif
}

bool HandleKb(u32 port)
{
  #if defined(USE_EVDEV)
    if (evdev_fd < 0)
    {
      return false;
    }

    input_event ie;

    #if defined(TARGET_GCW0)
      #define KEY_A      0x1D
      #define KEY_B      0x38
      #define KEY_X      0x2A
      #define KEY_Y      0x39
      #define KEY_L      0xF
      #define KEY_R      0xE
      #define KEY_SELECT 0x1
      #define KEY_START  0x1C
      #define KEY_LEFT   0x69
      #define KEY_RIGHT  0x6A
      #define KEY_UP     0x67
      #define KEY_DOWN   0x6C
      #define KEY_LOCK   0x77    // Note that KEY_LOCK is a switch and remains pressed until it's switched back

      static int keys[13];
      while(read(evdev_fd, &ie, sizeof(ie)) == sizeof(ie))
      {
        //printf("type %i key %i state %i\n", ie.type, ie.code, ie.value);
        if (ie.type = EV_KEY)
        {
          switch (ie.code)
          {
            case KEY_SELECT: keys[ 9] = ie.value; break;
            case KEY_UP:     keys[ 1] = ie.value; break;
            case KEY_DOWN:   keys[ 2] = ie.value; break;
            case KEY_LEFT:   keys[ 3] = ie.value; break;
            case KEY_RIGHT:  keys[ 4] = ie.value; break;
            case KEY_Y:      keys[ 5] = ie.value; break;
            case KEY_B:      keys[ 6] = ie.value; break;
            case KEY_A:      keys[ 7] = ie.value; break;
            case KEY_X:      keys[ 8] = ie.value; break;
            case KEY_START:  keys[12] = ie.value; break;
          }
        }
      }
      if (keys[ 6]) { kcode[port] &= ~Btn_A; }
      if (keys[ 7]) { kcode[port] &= ~Btn_B; }
      if (keys[ 5]) { kcode[port] &= ~Btn_Y; }
      if (keys[ 8]) { kcode[port] &= ~Btn_X; }
      if (keys[ 1]) { kcode[port] &= ~DPad_Up; }
      if (keys[ 2]) { kcode[port] &= ~DPad_Down; }
      if (keys[ 3]) { kcode[port] &= ~DPad_Left; }
      if (keys[ 4]) { kcode[port] &= ~DPad_Right; }
      if (keys[12]) { kcode[port] &= ~Btn_Start; }
      if (keys[ 9]) { die("death by escape key"); }
      if (keys[10]) { rt[port] = 255; }
      if (keys[11]) { lt[port] = 255; }

      return true;

    #elif defined(TARGET_PANDORA)
      static int keys[13];
      while(read(evdev_fd,&ie,sizeof(ie))==sizeof(ie))
      {
        if (ie.type=EV_KEY)
        //printf("type %i key %i state %i\n", ie.type, ie.code, ie.value);
        switch (ie.code) {
          case KEY_SPACE:      keys[ 0] = ie.value; break;
          case KEY_UP:         keys[ 1] = ie.value; break;
          case KEY_DOWN:       keys[ 2] = ie.value; break;
          case KEY_LEFT:       keys[ 3] = ie.value; break;
          case KEY_RIGHT:      keys[ 4] = ie.value; break;
          case KEY_PAGEUP:     keys[ 5] = ie.value; break;
          case KEY_PAGEDOWN:   keys[ 6] = ie.value; break;
          case KEY_END:        keys[ 7] = ie.value; break;
          case KEY_HOME:       keys[ 8] = ie.value; break;
          case KEY_MENU:       keys[ 9] = ie.value; break;
          case KEY_RIGHTSHIFT: keys[10] = ie.value; break;
          case KEY_RIGHTCTRL:  keys[11] = ie.value; break;
          case KEY_LEFTALT:    keys[12] = ie.value; break;
        }
      }

      if (keys[ 0]) { kcode[port] &= ~Btn_C; }
      if (keys[ 6]) { kcode[port] &= ~Btn_A; }
      if (keys[ 7]) { kcode[port] &= ~Btn_B; }
      if (keys[ 5]) { kcode[port] &= ~Btn_Y; }
      if (keys[ 8]) { kcode[port] &= ~Btn_X; }
      if (keys[ 1]) { kcode[port] &= ~DPad_Up; }
      if (keys[ 2]) { kcode[port] &= ~DPad_Down; }
      if (keys[ 3]) { kcode[port] &= ~DPad_Left; }
      if (keys[ 4]) { kcode[port] &= ~DPad_Right; }
      if (keys[12]) { kcode[port] &= ~Btn_Start; }
      if (keys[ 9]) { die("death by escape key"); }
      if (keys[10]) { rt[port] = 255; }
      if (keys[11]) { lt[port] = 255; }

      return true;
    #else
      while(read(evdev_fd, &ie, sizeof(ie)) == sizeof(ie))
      {
        printf("type %i key %i state %i\n", ie.type, ie.code, ie.value);
      }
    #endif
  #endif

  return true;
}

bool HandleJoystick(u32 port)
{
  // Joystick must be connected
  if(joystick_fd < 0) {
    return false;
  }

  #if defined(USE_JOYSTICK)
    struct js_event JE;
    while(read(joystick_fd, &JE, sizeof(JE)) == sizeof(JE))
    if (JE.number < MAP_SIZE)
    {
      switch(JE.type & ~JS_EVENT_INIT)
      {
        case JS_EVENT_AXIS:
        {
          u32 mt = JMapAxis[JE.number] >> 16;
          u32 mo = JMapAxis[JE.number] & 0xFFFF;

          //printf("AXIS %d,%d\n",JE.number,JE.value);
          s8 v=(s8)(JE.value/256); //-127 ... + 127 range

          if (mt == 0)
          {
            kcode[port] |= mo;
            kcode[port] |= mo*2;
            if (v<-64)
            {
              kcode[port] &= ~mo;
            }
            else if (v>64)
            {
              kcode[port] &= ~(mo*2);
            }

           //printf("Mapped to %d %d %d\n",mo,kcode[port]&mo,kcode[port]&(mo*2));
          }
          else if (mt == 1)
          {
            if (v >= 0)
            {
              v++;  //up to 255
            }
            //printf("AXIS %d,%d Mapped to %d %d %d\n",JE.number,JE.value,mo,v,v+127);
            if (mo == 0)
            {
              lt[port] = (v + 127);
            }
            else if (mo == 1)
            {
              rt[port] = (v + 127);
            }
          }
          else if (mt == 2)
          {
            //  printf("AXIS %d,%d Mapped to %d %d [%d]",JE.number,JE.value,mo,v);
            if (mo == 0)
            {
              joyx[port] = v;
            }
            else if (mo == 1)
            {
              joyy[port] = v;
            }
          }
        }
        break;

        case JS_EVENT_BUTTON:
        {
          u32 mt = JMapBtn[JE.number] >> 16;
          u32 mo = JMapBtn[JE.number] & 0xFFFF;

          // printf("BUTTON %d,%d\n",JE.number,JE.value);

          if (mt == 0)
          {
            // printf("Mapped to %d\n",mo);
            if (JE.value)
            {
              kcode[port] &= ~mo;
            }
            else
            {
              kcode[port] |= mo;
            }
          }
          else if (mt == 1)
          {
            // printf("Mapped to %d %d\n",mo,JE.value?255:0);
            if (mo==0)
            {
              lt[port] = JE.value ? 255 : 0;
            }
            else if (mo==1)
            {
              rt[port] = JE.value ? 255 : 0;
            }
          }
        }
        break;
      }
    }
  #endif

  return true;
}

extern bool KillTex;

#ifdef TARGET_PANDORA
  static Cursor CreateNullCursor(Display *display, Window root)
  {
    Pixmap cursormask;
    XGCValues xgc;
    GC gc;
    XColor dummycolour;
    Cursor cursor;

    cursormask = XCreatePixmap(display, root, 1, 1, 1/*depth*/);
    xgc.function = GXclear;
    gc = XCreateGC(display, cursormask, GCFunction, &xgc);
    XFillRectangle(display, cursormask, gc, 0, 0, 1, 1);
    dummycolour.pixel = 0;
    dummycolour.red = 0;
    dummycolour.flags = 04;
    cursor = XCreatePixmapCursor(display, cursormask, cursormask, &dummycolour,&dummycolour, 0,0);
    XFreePixmap(display,cursormask);
    XFreeGC(display,gc);
    return cursor;
  }
#endif

int x11_dc_buttons = 0xFFFF;

void UpdateInputState(u32 port)
{
  static char key = 0;

  kcode[port] = x11_dc_buttons;
  rt[port] = 0;
  lt[port] = 0;

  #if defined(TARGET_EMSCRIPTEN)
    return;
  #endif

  HandleJoystick(port);
  HandleKb(port);

  #if defined(TARGET_GCW0) || defined(TARGET_PANDORA)
    return;
  #endif

  for(;;)
  {
    key = 0;
    read(STDIN_FILENO, &key, 1);

    if (0  == key || EOF == key) { break; }
    if ('k' == key) { KillTex=true; }

    #ifdef TARGET_PANDORA
        if (' ' == key) { kcode[port] &= ~Btn_C; }
        if ('6' == key) { kcode[port] &= ~Btn_A; }
        if ('O' == key) { kcode[port] &= ~Btn_B; }
        if ('5' == key) { kcode[port] &= ~Btn_Y; }
        if ('H' == key) { kcode[port] &= ~Btn_X; }
        if ('A' == key) { kcode[port] &= ~DPad_Up; }
        if ('B' == key) { kcode[port] &= ~DPad_Down; }
        if ('D' == key) { kcode[port] &= ~DPad_Left; }
        if ('C' == key) { kcode[port] &= ~DPad_Right; }

        if ('q' == key) { die("death by escape key"); }
    #endif
    //if (0x1b == key){ die("death by escape key"); } //this actually quits when i press left for some reason

    if ('a' == key) { rt[port] = 255; }
    if ('s' == key) { lt[port] = 255; }

    #if FEAT_SHREC != DYNAREC_NONE
      if ('b' == key) { emit_WriteCodeCache(); }
      if ('n' == key) { bm_Reset(); }
      if ('m' == key) { bm_Sort(); }
      if (',' == key)
      {
        emit_WriteCodeCache();
        bm_Sort();
      }
    #endif
  }
}

void os_DoEvents()
{
  #if defined(SUPPORT_X11)
    if (x11_win)
    {
      //Handle X11
      XEvent e;

      if(XCheckWindowEvent((Display*)x11_disp, (Window)x11_win, KeyPressMask | KeyReleaseMask, &e))
      {
        switch(e.type)
        {
          case KeyPress:
          case KeyRelease:
          {
            int dc_key = x11_keymap[e.xkey.keycode];

            if (e.type == KeyPress)
            {
              x11_dc_buttons &= ~dc_key;
            }
            else
            {
              x11_dc_buttons |= dc_key;
            }

            //printf("KEY: %d -> %d: %d\n",e.xkey.keycode, dc_key, x11_dc_buttons );
          }
          break;


          {
            printf("KEYRELEASE\n");
          }
          break;

        }
      }
    }
  #endif
}

void os_SetWindowText(const char * text)
{
  if (0==x11_win || 0==x11_disp || 1)
  {
    printf("%s\n",text);
  }
  #if defined(SUPPORT_X11)
  else if (x11_win)
  {
    XChangeProperty((Display*)x11_disp, (Window)x11_win,
      XInternAtom((Display*)x11_disp, "WM_NAME", False),     //WM_NAME,
      XInternAtom((Display*)x11_disp, "UTF8_STRING", False), //UTF8_STRING,
      8, PropModeReplace, (const unsigned char *)text, strlen(text));
  }
  #endif
}


void* x11_glc;

int ndcid=0;
void os_CreateWindow()
{
  #if defined(SUPPORT_X11)
    if (cfgLoadInt("pvr", "nox11", 0) == 0)
    {
      XInitThreads();
      // X11 variables
      Window       x11Window = 0;
      Display*     x11Display = 0;
      long         x11Screen = 0;
      XVisualInfo* x11Visual = 0;
      Colormap     x11Colormap = 0;

      /*
      Step 0 - Create a NativeWindowType that we can use it for OpenGL ES output
      */
      Window sRootWindow;
      XSetWindowAttributes sWA;
      unsigned int ui32Mask;
      int i32Depth;

      // Initializes the display and screen
      x11Display = XOpenDisplay(0);
      if (!x11Display && !(x11Display = XOpenDisplay(":0")))
      {
        printf("Error: Unable to open X display\n");
        return;
      }
      x11Screen = XDefaultScreen(x11Display);

      // Gets the window parameters
      sRootWindow = RootWindow(x11Display, x11Screen);

      int depth = CopyFromParent;

      #if !defined(GLES)
        // Get a matching FB config
        static int visual_attribs[] =
        {
          GLX_X_RENDERABLE    , True,
          GLX_DRAWABLE_TYPE   , GLX_WINDOW_BIT,
          GLX_RENDER_TYPE     , GLX_RGBA_BIT,
          GLX_X_VISUAL_TYPE   , GLX_TRUE_COLOR,
          GLX_RED_SIZE        , 8,
          GLX_GREEN_SIZE      , 8,
          GLX_BLUE_SIZE       , 8,
          GLX_ALPHA_SIZE      , 8,
          GLX_DEPTH_SIZE      , 24,
          GLX_STENCIL_SIZE    , 8,
          GLX_DOUBLEBUFFER    , True,
          //GLX_SAMPLE_BUFFERS  , 1,
          //GLX_SAMPLES         , 4,
          None
        };

        int glx_major, glx_minor;

        // FBConfigs were added in GLX version 1.3.
        if (!glXQueryVersion(x11Display, &glx_major, &glx_minor) ||
            ((glx_major == 1) && (glx_minor < 3)) || (glx_major < 1))
        {
          printf("Invalid GLX version");
          exit(1);
        }

        int fbcount;
        GLXFBConfig* fbc = glXChooseFBConfig(x11Display, x11Screen, visual_attribs, &fbcount);
        if (!fbc)
        {
          printf("Failed to retrieve a framebuffer config\n");
          exit(1);
        }
        printf("Found %d matching FB configs.\n", fbcount);

        GLXFBConfig bestFbc = fbc[0];
        XFree(fbc);

        // Get a visual
        XVisualInfo *vi = glXGetVisualFromFBConfig(x11Display, bestFbc);
        printf("Chosen visual ID = 0x%x\n", vi->visualid);


        depth = vi->depth;
        x11Visual = vi;

        x11Colormap = XCreateColormap(x11Display, RootWindow(x11Display, x11Screen), vi->visual, AllocNone);
      #else
        i32Depth = DefaultDepth(x11Display, x11Screen);
        x11Visual = new XVisualInfo;
        XMatchVisualInfo(x11Display, x11Screen, i32Depth, TrueColor, x11Visual);
        if (!x11Visual)
        {
          printf("Error: Unable to acquire visual\n");
          return;
        }
        x11Colormap = XCreateColormap(x11Display, sRootWindow, x11Visual->visual, AllocNone);
      #endif

      sWA.colormap = x11Colormap;

      // Add to these for handling other events
      sWA.event_mask = StructureNotifyMask | ExposureMask | ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask;
      ui32Mask = CWBackPixel | CWBorderPixel | CWEventMask | CWColormap;

      #ifdef TARGET_PANDORA
        int width = 800;
        int height = 480;
      #else
        int width = cfgLoadInt("x11", "width", WINDOW_WIDTH);
        int height = cfgLoadInt("x11", "height", WINDOW_HEIGHT);
      #endif

      if (width == -1)
      {
        width = XDisplayWidth(x11Display, x11Screen);
        height = XDisplayHeight(x11Display, x11Screen);
      }

      // Creates the X11 window
      x11Window = XCreateWindow(x11Display, RootWindow(x11Display, x11Screen), (ndcid%3)*640, (ndcid/3)*480, width, height,
        0, depth, InputOutput, x11Visual->visual, ui32Mask, &sWA);

      #ifdef TARGET_PANDORA
        // fullscreen
        Atom wmState = XInternAtom(x11Display, "_NET_WM_STATE", False);
        Atom wmFullscreen = XInternAtom(x11Display, "_NET_WM_STATE_FULLSCREEN", False);
        XChangeProperty(x11Display, x11Window, wmState, XA_ATOM, 32, PropModeReplace, (unsigned char *)&wmFullscreen, 1);

        XMapRaised(x11Display, x11Window);
      #else
        XMapWindow(x11Display, x11Window);

        #if !defined(GLES)
          #define GLX_CONTEXT_MAJOR_VERSION_ARB       0x2091
          #define GLX_CONTEXT_MINOR_VERSION_ARB       0x2092
          typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

          glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
          glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)glXGetProcAddressARB((const GLubyte*)"glXCreateContextAttribsARB");
          verify(glXCreateContextAttribsARB != 0);
          int context_attribs[] =
          {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
            GLX_CONTEXT_MINOR_VERSION_ARB, 1,
            GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_DEBUG_BIT_ARB,
            GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
            None
          };

          x11_glc = glXCreateContextAttribsARB(x11Display, bestFbc, 0, True, context_attribs);
          XSync(x11Display, False);

          if (!x11_glc)
          {
            die("Failed to create GL3.1 context\n");
          }
        #endif
      #endif

      XFlush(x11Display);

      //(EGLNativeDisplayType)x11Display;
      x11_disp = (void*)x11Display;
      x11_win = (void*)x11Window;
    }
    else
    {
      printf("Not creating X11 window ..\n");
    }
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

#ifdef TARGET_PANDORA
  void gl_term();

  void clean_exit(int sig_num)
  {
    void* array[10];
    size_t size;

    // close files
    if (joystick_fd >= 0) { close(joystick_fd); }
    if (evdev_fd >= 0) { close(evdev_fd); }

    // Close EGL context ???
    if (sig_num!=0)
    {
      gl_term();
    }

    // close XWindow
    if (x11_win)
    {
      XDestroyWindow(x11_disp, x11_win);
      x11_win = 0;
    }
    if (x11_disp)
    {
      XCloseDisplay(x11_disp);
      x11_disp = 0;
    }

    // finish cleaning
    if (sig_num!=0)
    {
      write(2, "\nSignal received\n", sizeof("\nSignal received\n"));

      size = backtrace(array, 10);
      backtrace_symbols_fd(array, size, STDERR_FILENO);
      exit(1);
    }
  }
#endif

int main(int argc, wchar* argv[])
{
  if (setup_curses() < 0)
  {
    printf("failed to setup curses!\n");
  }
  #ifdef TARGET_PANDORA
    signal(SIGSEGV, clean_exit);
    signal(SIGKILL, clean_exit);
  #endif

  /* Set home dir */
  string home = ".";
  #if defined(USES_HOMEDIR)
    if(getenv("HOME") != NULL)
    {
      home = (string)getenv("HOME") + "/.reicast";
      mkdir(home.c_str(), 0755); // create the directory if missing
    }
  #endif
  SetHomeDir(home);
  printf("Home dir is: %s\n", GetPath("/").c_str());

  #if defined(SUPPORT_X11)
    x11_keymap[113] = DPad_Left;
    x11_keymap[114] = DPad_Right;

    x11_keymap[111] = DPad_Up;
    x11_keymap[116] = DPad_Down;

    x11_keymap[52] = Btn_Y;
    x11_keymap[53] = Btn_X;
    x11_keymap[54] = Btn_B;
    x11_keymap[55] = Btn_A;

    /*
      //TODO: Fix sliders
      x11_keymap[38] = DPad_Down;
      x11_keymap[39] = DPad_Down;
    */

    x11_keymap[36] = Btn_Start;
  #endif

  common_linux_setup();

  SetupInput();

  settings.profile.run_counts=0;

  dc_init(argc,argv);

  #if !defined(TARGET_EMSCRIPTEN)
    dc_run();
  #else
    emscripten_set_main_loop(&dc_run, 100, false);
  #endif


  #ifdef TARGET_PANDORA
    clean_exit(0);
  #endif

  return 0;
}
#endif

int get_mic_data(u8* buffer) { return 0; }
int push_vmu_screen(u8* buffer) { return 0; }

void os_DebugBreak()
{
  #if !defined(TARGET_EMSCRIPTEN)
    raise(SIGTRAP);
  #else
    printf("DEBUGBREAK!\n");
    exit(-1);
  #endif
}
