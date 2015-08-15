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

#if defined(USES_HOMEDIR)
  #include <sys/stat.h>
#endif

#if defined(USE_EVDEV)
  #include "linux-dist/evdev.h"
#endif

#if defined(USE_JOYSTICK)
  #include "linux-dist/joystick.h"
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

void emit_WriteCodeCache();

#if defined(USE_EVDEV)
  /* evdev input */
  static Controller controllers[4] = {
    { -1, NULL },
    { -1, NULL },
    { -1, NULL },
    { -1, NULL }
  };
#endif

#if defined(USE_JOYSTICK)
  /* legacy joystick input */
  static int joystick_fd = -1; // Joystick file descriptor
#endif

void SetupInput()
{
  #if defined(USE_EVDEV)
    char evdev_config_key[17];
    int evdev_device_id[4] = { -1, -1, -1, -1 };
    
    int evdev_device_length, port, i;
    char* evdev_device;
    
    for (port = 0; port < 4; port++)
    {
      sprintf(evdev_config_key, "evdev_device_id_%d", port+1);
      evdev_device_id[port] = cfgLoadInt("input", evdev_config_key, EVDEV_DEFAULT_DEVICE_ID(port+1));
      
      // Check if the same device is already in use on another port
      if (evdev_device_id[port] < 0)
      {
        printf("evdev: Controller %d disabled by config.\n", port + 1);
      }
      else
      {
        for (i = 0; i < port; i++)
        {
            if (evdev_device_id[port] == evdev_device_id[i])
            {
                die("You can't assign the same device to multiple ports!\n");
            }
        }

        evdev_device_length = snprintf(NULL, 0, EVDEV_DEVICE_STRING, evdev_device_id[port]);
        evdev_device = (char*)malloc(evdev_device_length + 1);
        sprintf(evdev_device, EVDEV_DEVICE_STRING, evdev_device_id[port]);
        input_evdev_init(&controllers[port], evdev_device);
        free(evdev_device);
      }
    }
  #endif

  #if defined(USE_JOYSTICK)
    int joystick_device_id = cfgLoadInt("input", "joystick_device_id", JOYSTICK_DEFAULT_DEVICE_ID);
    if (joystick_device_id < 0) {
      puts("joystick input disabled by config.\n");
    }
    else
    {
      int joystick_device_length = snprintf(NULL, 0, JOYSTICK_DEVICE_STRING, joystick_device_id);
      char* joystick_device = (char*)malloc(joystick_device_length + 1);
      sprintf(joystick_device, JOYSTICK_DEVICE_STRING, joystick_device_id);
      joystick_fd = input_joystick_init(joystick_device);
      free(joystick_device);
    }
  #endif
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
  #if defined(TARGET_EMSCRIPTEN)
    return;
  #endif

  #if defined(USE_JOYSTICK)
    input_joystick_handle(joystick_fd, port);
  #endif

  #if defined(USE_EVDEV)
    input_evdev_handle(&controllers[port], port);
  #endif
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
              kcode[0] &= ~dc_key;
            }
            else
            {
              kcode[0] |= dc_key;
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

    if (joystick_fd >= 0) { close(joystick_fd); }
    for (int port = 0; port < 4 ; port++)
    {
      if (evdev_fd[port] >= 0)
      {
        close(evdev_fd[port]);
      }
    }

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
    x11_keymap[113] = DC_DPAD_LEFT;
    x11_keymap[114] = DC_DPAD_RIGHT;

    x11_keymap[111] = DC_DPAD_UP;
    x11_keymap[116] = DC_DPAD_DOWN;

    x11_keymap[53] = DC_BTN_X;
    x11_keymap[54] = DC_BTN_B;
    x11_keymap[55] = DC_BTN_A;

    /*
      //TODO: Fix sliders
      x11_keymap[38] = DPad_Down;
      x11_keymap[39] = DPad_Down;
    */

    x11_keymap[36] = DC_BTN_START;
  #endif

  common_linux_setup();

  settings.profile.run_counts=0;

  dc_init(argc,argv);

  SetupInput();

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
