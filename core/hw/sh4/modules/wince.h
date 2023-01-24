/*
	Copyright 2019 flyinghead

	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "hw/sh4/sh4_sched.h"

#ifdef TRACE_WINCE_SYSCALLS
#define	PUserKData 			0x00005800
#define	SYSHANDLE_OFFSET	0x004
#define	SYS_HANDLE_BASE		64
#define SH_WIN32                0
#define SH_CURTHREAD            1
#define SH_CURPROC              2

static bool read_mem32(u32 addr, u32& data)
{
	u32 pa;
	const TLB_Entry *entry;
	if (mmu_full_lookup<false>(addr, &entry, pa) != MMU_ERROR_NONE)
		return false;
	data = ReadMem32_nommu(pa);
	return true;
}

static bool read_mem16(u32 addr, u16& data)
{
	u32 pa;
	const TLB_Entry *entry;
	if (mmu_full_lookup<false>(addr, &entry, pa) != MMU_ERROR_NONE)
		return false;
	data = ReadMem16_nommu(pa);
	return true;
}

static bool read_mem8(u32 addr, u8& data)
{
	u32 pa;
	const TLB_Entry *entry;
	if (mmu_full_lookup<false>(addr, &entry, pa) != MMU_ERROR_NONE)
		return false;
	data = ReadMem8_nommu(pa);
	return true;
}

static inline u32 getCurrentThreadId()
{
	u32 addr = PUserKData + SYSHANDLE_OFFSET + SH_CURTHREAD * 4;
	u32 tid;
	if (read_mem32(addr, tid))
		return tid;
	else
		return 0;
}

static inline u32 getCurrentProcessId()
{
	u32 addr = PUserKData + SYSHANDLE_OFFSET + SH_CURPROC * 4;
	u32 pid;
	if (read_mem32(addr, pid))
		return pid;
	else
		return 0;
}

#define FIRST_METHOD					0xFFFFFE01
#define APICALL_SCALE					2

static const char *wince_apis[] = {
		// SH_*
		"WIN32", "CURTHREAD", "CURPROC", "WDMAPI",
		// HT_*
		"EVENT", "MUTEX", "APISET", "FILE", "FIND", "DBFILE", "DBFIND", "SOCKET", "INTERFACE", "SEMAPHORE", "FSMAP", "WNETENUM",
		// SH_*
		"GDI", "WMGR", "INIT", "COMM", "FILESYS", "SHELL", "DEVMGR", "TAPI", "PATCHER", "IMM", "WNET", "?", "?", "?", "?", "USER"
};

static const char *wince_methods[][256] = {
		// WIN32
		{
			"Nop", "N/S", "CreateAPISet", "VirtualAlloc", "VirtualFree", "VirtualProtect", "VirtualQuery", "VirtualCopy",
			"LoadLibraryW", "FreeLibrary", "GetProcAddressW", "ThreadAttachAllDLLs", "ThreadDetachAllDLLs", "GetTickCount", "OutputDebugStringW", "TlsCall",
			"GetSystemInfo", "(ropen)", "(rread)", "(rwrite)", "(rlseek)", "(rclose)", "RegisterDbgZones", "NKvDbgPrintfW",
			"ProfileSyscall", "FindResource", "LoadResource", "SizeofResource", "GetRealTime", "SetRealTime", "ProcessDetachAllDLLs", "ExtractResource",
			// 32
			"GetRomFileInfo", "GetRomFileBytes", "CacheRangeFlush", "AddTrackedItem", "DeleteTrackedItem", "PrintTrackedItem", "GetKPhys", "GiveKPhys",
			"SetExceptionHandler", "RegisterTrackedItem", "FilterTrackedItem", "SetKernelAlarm", "RefreshKernelAlarm", "CeGetRandomSeed", "CloseProcOE", "SetGwesOOMEvent",
			"StringCompress", "StringDecompress", "BinaryCompress", "BinaryDecompress", "CreateEvent", "CreateProc", "CreateThread", "InputDebugCharW",
			"TakeCritSec", "LeaveCritSec", "WaitForMultiple", "MapPtrToProcess", "MapPtrUnsecure", "GetProcFromPtr", "IsBadPtr", "GetProcAddrBits",
			// 64
			"GetFSHeapInfo", "OtherThreadsRunning", "KillAllOtherThreads", "GetOwnerProcess", "GetCallerProcess", "GetIdleTime", "SetLowestScheduledPriority", "IsPrimaryThread",
			"SetProcPermissions", "GetCurrentPermissions", "(74?)", "SetDaylightTime", "SetTimeZoneBias", "SetCleanRebootFlag", "CreateCrit", "PowerOffSystem",
			"CreateMutex", "SetDbgZone", "Sleep", "TurnOnProfiling", "TurnOffProfiling", "CeGetCurrentTrust", "CeGetCallerTrust", "NKTerminateThread",
			"SetLastError", "GetLastError", "GetProcName", "TerminateSelf", "CloseAllHandles", "SetHandleOwner", "LoadDriver", "CreateFileMapping",
			// 96
			"UnmapViewOfFile", "FlushViewOfFile", "CreateFileForMappingW", "KernelIoControl", "GetThreadCallStack", "PPSHRestart", "(102?)", "UpdateNLSInfo",
			"ConnectDebugger", "InterruptInitialize", "InterruptDone", "InterruptDisable", "SetKMode", "SetPowerOffHandler", "SetGwesPowerHandler", "SetHardwareWatch",
			"QueryAPISetID", "PerformCallBack", "CaptureContext", "GetCallerProcessIndex", "WaitForDebugEvent", "ContinueDebugEvent", "DebugNotify", "OpenProcess",
			"THCreateSnapshot", "THGrow", "NotifyForceCleanboot", "DumpKCallProfile", "GetProcessVersion", "GetModuleFileNameW", "QueryPerformanceCounter", "QueryPerformanceFrequency",
			// 128
			"KernExtractIcons", "ForcePageout", "GetThreadTimes", "GetModuleHandleW", "SetWDevicePowerHandler", "SetStdioPathW", "GetStdioPathW", "ReadRegistryFromOEM",
			"WriteRegistryToOEM", "WriteDebugLED", "LockPages", "UnlockPages", "VirtualSetAttributes", "SetRAMMode", "SetStoreQueueBase", "FlushViewOfFileMaybe",
		},
		// CURTHREAD
		{
			"CloseHandle", "(AllHandle_Wait)", "Suspend", "Resume", "SetThreadPrio", "GetThreadPrio", "GetRetCode", "GetContext",
			"SetContext", "Terminate", "CeGetPrio", "CeSetPrio", "CeGetQuant", "CeSetQuant"
		},
		// CURPROC
		{
			"CloseHandle", "(AllHandle_Wait)", "Terminate", "GetRetCode", "FlushICache", "ReadMemory", "WriteMemory", "DebugActive", "GetModInfo", "SetVer"
		},
		// unused
		{
		},
		// EVENT
		{
			"CloseHandle", "(AllHandle_Wait)", "Modify", "AddAccess", "GetData", "SetData"
		},
		// MUTEX
		{
			"CloseHandle", "(AllHandle_Wait)", "ReleaseMutex"
		},
		// APISET
		{
			"CloseHandle", "(AllHandle_Wait)",  "Register", "CreateHandle", "Verify"
		},
		// FILE
		{
			"CloseFileHandle", "(NULL)", "ReadFile", "WriteFile", "GetFileSize", "SetFilePointer", "GetFileInformationByHandle", "FlushFileBuffers",
			"GetFileTime", "SetFileTime", "SetEndOfFile", "DeviceIoControl", "ReadFileWithSeek", "WriteFileWithSeek"
		},
		// FIND
		{
			"FindClose", "(NULL)", "FindNextFile"
		},
		// DBFILE
		{
		},
		// DBFIND
		{
		},
		// SOCKET
		{
		},
		// INTERFACE
		{
		},
		// SEMAPHORE
		{
		},
		// FSMAP
		{
		},
		// WNETENUM
		{
		},
		// GDI
		{
		},
		// WMGR
		{
		},
		// INIT
		{
		},
		// COMM
		{
		},
		// FILESYS
		{
			"ProcNotify", "(AllHandle_Wait)", "CreateDirectory", "RemoveDirectory", "MoveFile", "CopyFile", "DeleteFile", "GetFileAttibutes",
			"FindFirstFile", "CreateFile", "CeRegisterFileSystemNotification", "CeRegisterReplNotification", "CeOidGetInfoEx", "CeFindFirstDatabaseEx", "CeCreateDatabaseEx", "CeSetDatabaseInfoEx",
			"CeOpenDatabaseEx", "RegCloseKey", "RegCreateKeyExW", "RegDeleteKeyW", "RegDeleteValueW", "RegEnumValueW", "RegEnumKeyExW", "RegOpenKeyExW",
			"RegQueryInfoKeyW", "RegQueryValueExW", "RegSetValueExW", "GetTempPathW", "CeDeleteDatabaseEx", "CheckPassword", "SetPassword", "SetFileAttributesW",
			"GetStoreInformation", "CeGetReplChangeMask", "CeSetReplChangeMask", "CeGetReplChangeBitsEx", "CeClearReplChangeBitsEx", "CeGetReplOtherBitsEx", "CeSetReplOtherBitsEx", "GetSystemMemoryDivision",
			"SetSystemMemoryDivision", "RegCopyFile", "CloseAllFileHandles", "PrestoChangoFileName", "RegRestoreFile", "RegisterAFS", "DeregisterAFS", "GetPasswordActive",
			"SetPasswordActive", "RegFlushKey", "NotifyMountedFS", "CeSetReplChangeBitsEx", "RegisterAFSName", "DeregisterAFSName", "GetDiskFreeSpaceExW", "RegisterHiddenAFS",
			"CeChangeDatabaseLCID", "DumpHeap", "CeMountDBVol", "CeEnumDBVolumes", "CeUnmountDBVol", "CeFlushDBVol", "CeFreeNotification"
		},
		// SHELL
		{
		},
		// DEVMGR
		{
		},
		// TAPI
		{
		},
		// PATCHER
		{
		},
		// IMM
		{
		},
		// WNET
		{
		},
		{
		},
		{
		},
		{
		},
		{
		},
		// USER
		{
			"NotifyCallback", "(unused)", "RegisterClass", "UnregisterClass", "CreateWindowEx", "PostMessage", "PostQuitMessage", "SendMessage",
			"GetMessage", "TranslateMessage", "DispatchMessage", "GetCapture", "SetCapture", "ReleaseCapture", "(ununsed)", "(ununsed)",
			"(ununsed)", "(ununsed)", "(ununsed)", "GetSystemMetrics", "ImageList_GetDragImage", "ImageList_GetIconSize", "ImageList_SetIconSize", "ImageList_GetImageInfo",
			"ImageList_Merge", "ShowCursor", "SetCursorPos", "ImageList_CopyDitherImage", "ImageList_DrawIndirect", "ImageList_DragShowNolock", "CWindowManager::WindowFromPoint(tagPOINT)", "(unused)",
		},
};

extern u32 unresolved_ascii_string;
extern u32 unresolved_unicode_string;

static inline std::string get_unicode_string(u32 addr)
{
	std::string str;
	while (true)
	{
		u16 c;
		if (!read_mem16(addr, c))
		{
			unresolved_unicode_string = addr;
			return "(page fault)";
		}
		if (c == 0)
			break;
		str += (char)c;
		addr += 2;
	}
	return str;
}

static inline std::string get_ascii_string(u32 addr)
{
	std::string str;
	while (true)
	{
		u8 c;
		if (!read_mem8(addr++, c))
		{
			unresolved_ascii_string = addr;
			return "(page fault)";
		}
		if (c == 0)
			break;
		str += (char)c;
	}
	return str;
}

static bool print_wince_syscall(u32 address)
{
	if (address & 1)
	{
		if (address == 0xfffffd5d || address == 0xfffffd05)	// Sleep, QueryPerformanceCounter
			return true;
		if (address == 0xfffffde7) // GetTickCount
		{
			// This should make this syscall faster
			//r[0] = sh4_sched_now64() * 1000 / SH4_MAIN_CLOCK;
			//next_pc = pr;
			//skip_exception = true;
			return true;
		}

		u32 api_id;
		if (address <= FIRST_METHOD)
			api_id = (((FIRST_METHOD - address) / APICALL_SCALE) >> 8) & 0x3F;
		else
			api_id = 0;
		const char *api;
		char api_buf[128];

		if (api_id < ARRAY_SIZE(wince_apis))
			api = wince_apis[api_id];
		else
		{
			sprintf(api_buf, "[%d]", api_id);
			api = api_buf;
		}

		u32 meth_id;
		if (address <= FIRST_METHOD)
			meth_id = ((FIRST_METHOD - address) / APICALL_SCALE) & 0xFF;
		else
			meth_id = ((address - FIRST_METHOD) / APICALL_SCALE) & 0xFF;
		const char *method = NULL;
		char method_buf[128];

		if (api_id < ARRAY_SIZE(wince_methods) && meth_id < ARRAY_SIZE(wince_methods[api_id]))
			method = wince_methods[api_id][meth_id];
		if (method == NULL)
		{
			sprintf(method_buf, "[%d]", meth_id);
			method = method_buf;
		}
		printf("WinCE %08x %04x.%04x %s: %s", address, getCurrentProcessId() & 0xffff, getCurrentThreadId() & 0xffff, api, method);
		if (address == 0xfffffd51)		// SetLastError
			printf(" dwErrCode = %x\n", r[4]);
		else if (address == 0xffffd5ef)	// CreateFile
			printf(" lpFileName = %s\n", get_unicode_string(r[4]).c_str());
		else if (address == 0xfffffd97) // CreateProc
			printf(" imageName = %s, commandLine = %s\n", get_unicode_string(r[4]).c_str(), get_unicode_string(r[5]).c_str());
		else if (!strcmp("DebugNotify", method))
			printf(" %x, %x\n", r[4], r[5]);
		else if (address == 0xffffd5d3) // RegOpenKeyExW
			printf(" hKey = %x, lpSubKey = %s\n", r[4], get_unicode_string(r[5]).c_str());
		else if (!strcmp("LoadLibraryW", method))
			printf(" fileName = %s\n", get_unicode_string(r[4]).c_str());
		else if (!strcmp("GetProcAddressW", method))
			printf(" hModule = %x, procName = %s\n", r[4], get_unicode_string(r[5]).c_str());
		else if (!strcmp("NKvDbgPrintfW", method))
			printf(" fmt = %s\n", get_unicode_string(r[4]).c_str());
		else if (!strcmp("OutputDebugStringW", method))
			printf(" str = %s\n", get_unicode_string(r[4]).c_str());
		else if (!strcmp("RegisterAFSName", method))
			printf(" name = %s\n", get_unicode_string(r[4]).c_str());
		else if (!strcmp("CreateAPISet", method))
			printf(" name = %s\n", get_ascii_string(r[4]).c_str());
		else if (!strcmp("Register", method) && !strcmp("APISET", api))
			printf(" p = %x, id = %x\n", r[4], r[5]);
		else
			printf("\n");
		// might be useful to detect errors? (hidden & dangerous)
		//if (!strcmp("GetProcName", method))
		//	os_DebugBreak();
		return true;
	}
	else
		return false;

}
#endif

#if defined(FAST_MMU) && defined(USE_WINCE_HACK)
static bool wince_resolve_address(u32 va, TLB_Entry &entry)
{
	// WinCE hack
	if ((va & 0x80000000) == 0)
	{
		const u32 ram_mask = RAM_MASK;
		u32 page_group = *(u32 *)&mem_b[(CCN_TTB + ((va >> 25) << 2)) & ram_mask];
		u32 page = ((va >> 16) & 0x1ff) << 2;
		u32 paddr = *(u32 *)&mem_b[(page_group + page) & ram_mask];
		if (paddr & 0x80000000)
		{
			u32 whatever = *(u32 *)&mem_b[(r_bank[4] + 0x14) & ram_mask];
			if (whatever != *(u32 *)&mem_b[paddr & ram_mask])
			{
				paddr += 12;
				u32 ptel = *(u32 *)&mem_b[(paddr + ((va >> 10) & 0x3c)) & ram_mask];
				//FIXME CCN_PTEA = paddr >> 29;
				if (ptel != 0)
				{
					entry.Data.reg_data = ptel - 1;
					entry.Address.ASID = CCN_PTEH.ASID;
					entry.Assistance.reg_data = 0;
					u32 sz = entry.Data.SZ1 * 2 + entry.Data.SZ0;
					entry.Address.VPN = (va & mmu_mask[sz]) >> 10;

					return true;
				}
			}
		}
	}
	else
	{
		// SQ
		if (((va >> 26) & 0x3F) == 0x38)
		{
			u32 r1 = (va - 0xe0000000) & 0xfff00000;
			//r1 &= 0xfff00000;
			//u32 r0 = ReadMem32_nommu(0x8C01258C);	// FIXME
			//u32 r0 = 0x8c138b14;
			//r0 = ReadMem32_nommu(r0);	// 0xE0001F5
			u32 r0 = 0xe0001f5;
			r0 += r1;
			entry.Data.reg_data = r0 - 1;
			entry.Assistance.reg_data = r0 >> 29;
			entry.Address.ASID = CCN_PTEH.ASID;
			u32 sz = entry.Data.SZ1 * 2 + entry.Data.SZ0;
			entry.Address.VPN = (va & mmu_mask[sz]) >> 10;

			return true;
		}
	}

	return false;
}
#endif
