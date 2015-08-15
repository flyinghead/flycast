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
  #include "linux-dist/x11.h"
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
#endif


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

  #if defined(SUPPORT_X11)
    input_x11_init();
  #endif
}

extern bool KillTex;

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
    input_x11_handle();
  #endif
}

void os_SetWindowText(const char * text)
{
  printf("%s\n",text);
  #if defined(SUPPORT_X11)
    x11_window_set_text(text);
  #endif
}

void os_CreateWindow()
{
  #if defined(SUPPORT_X11)
    x11_window_create();
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

    x11_window_destroy():

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
