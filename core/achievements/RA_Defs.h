#pragma once

#ifdef _WIN32
#include <windows.h>
// RA_Integration expects __stdcall conventions for callbacks on Windows
#define CCONV __stdcall
#else
#define CCONV
#endif

// Console ID for Dreamcast
#define RA_DREAMCAST_ID 40

// Dialog IDs for invoking RA Tools
#define RA_IDM_LOGIN                    1
#define RA_IDM_LOGOUT                   2
#define RA_IDM_TOGGLE_HARDCORE          3
#define RA_IDM_ASSET_LIST               10
#define RA_IDM_ACHIEVEMENT_EDITOR       11
#define RA_IDM_MEMORY_INSPECTOR         12

// RA_Integration API typedefs (Windows Only)
#ifdef _WIN32
typedef int (*_RA_InitI_Fn)(void *hMainWnd, int nConsoleID, const char *sClientVersion);
typedef int (*_RA_Shutdown_Fn)();
// Renamed from _RA_Update_Fn to reflect modern DLL export preference, though we fallback if needed
typedef void (*_RA_DoAchievementsFrame_Fn)();
typedef int (*_RA_OnLoadNewRom_Fn)(const unsigned char *pData, unsigned int nDataSize);
typedef void (*_RA_SetConsoleID_Fn)(unsigned int nConsoleID);
typedef int (*_RA_OnReset_Fn)();
typedef void (*_RA_SetPaused_Fn)(int bIsPaused);

// Memory Bank functions
// Note: We use CCONV here to match the DLL, though standard void* casts in LoadLibrary usually hide this.
typedef void (*_RA_InstallMemoryBank_Fn)(int nBankID, void *pReader, void *pWriter, int nBankSize);
typedef void (*_RA_InstallMemoryBankBlockReader_Fn)(int nBankID, void *pReader);

typedef void (*_RA_InvokeDialog_Fn)(LPARAM nID);
typedef void (*_RA_UpdateHWnd_Fn)(HWND hMainHWND);

// Helper to check login status and trigger login
typedef const char *(*_RA_UserName_Fn)();
typedef void (*_RA_AttemptLogin_Fn)(int bBlocking);
#endif

// Callback signatures - MUST use CCONV (Defined as empty on non-Windows)
typedef unsigned char (CCONV *RA_ReadByte_Fn)(unsigned int nAddress);
typedef void (CCONV *RA_WriteByte_Fn)(unsigned int nAddress, unsigned char nValue);
typedef unsigned int (CCONV *RA_ReadMemoryBlock_Fn)(unsigned int nAddress, unsigned char *pBuffer, unsigned int nBytes);