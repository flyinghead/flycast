#include "naomi_cart.h"
#include "cfg/cfg.h"
#include "naomi.h"

u8* RomPtr;
u32 RomSize;

#if HOST_OS == OS_WINDOWS
	typedef HANDLE fd_t;
	#define INVALID_FD INVALID_HANDLE_VALUE
#else
	typedef int fd_t;
	#define INVALID_FD -1

	#include <unistd.h>
	#include <fcntl.h>
	#include <sys/mman.h>
#endif

fd_t*	RomCacheMap;
u32		RomCacheMapCount;

char SelectedFile[512];

extern s8 joyx[4],joyy[4];
extern u8 rt[4], lt[4];

static std::string trim(const std::string s)
{
	std::string r(s);
	while (!r.empty() && r[0] == ' ')
		r.erase(0, 1);
	while (!r.empty() && r[r.size() - 1] == ' ')
		r.erase(r.size() - 1);

	return r;
}

static u16 getJoystickXAxis()
{
	return (joyx[0] + 128) << 8;
}

static u16 getJoystickYAxis()
{
	return (joyy[0] + 128) << 8;
}

static u16 getLeftTriggerAxis()
{
	return lt[0] << 8;
}

static u16 getRightTriggerAxis()
{
	return rt[0] << 8;
}

static NaomiInputMapping naomi_default_mapping = {
	{ getJoystickXAxis, getJoystickYAxis, getRightTriggerAxis, getLeftTriggerAxis },
	{ 0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1 },
	{ 1,  2,  4,  8, 16, 32, 64,128,  1,  2,  4,  8 },
};

static void parse_comment(const char *line)
{
	std::string s(line + 1);
	s = trim(s);
	if (strncmp(s.c_str(), "input-mapping:", 14))
		return;

	s.erase(0, 14);

	s = trim(s);
	while (!s.empty())
	{
		size_t p = s.find_first_of(",");
		if (p == -1)
			p = s.size();
		std::string mapping = s.substr(0, p);
		size_t eq = mapping.find_first_of("=");
		if (eq == -1 || eq == mapping.size() - 1)
			printf("Warning: unparseable mapping %s\n", mapping.c_str());
		else
		{
			std::string dc_key = trim(mapping.substr(0, eq));
			std::string naomi_key = trim(mapping.substr(eq + 1));
			if (!strncmp(naomi_key.c_str(), "axis_", 5))
			{
				int axis = naomi_key[5] - '0';
				if (axis >= 4)
					printf("Warning: invalid axis number %d\n", axis);
				else
				{
					getNaomiAxisFP fp = NULL;
					if (dc_key == "axis_x")
						fp = &getJoystickXAxis;
					else if (dc_key == "axis_y")
						fp = &getJoystickYAxis;
					else if (dc_key == "axis_trigger_left")
						fp = &getLeftTriggerAxis;
					else if (dc_key == "axis_trigger_right")
						fp = &getRightTriggerAxis;
					else
						printf("Warning: invalid controller axis %s\n", dc_key.c_str());
					if (fp != NULL)
						Naomi_Mapping.axis[axis] = fp;
				}
			}
			else
			{
				int byte = naomi_key[0] - '0';
				size_t colon = naomi_key.find_first_of(":");
				if (colon == -1 || colon == naomi_key.size() - 1)
					printf("Warning: unparseable naomi key %s\n", naomi_key.c_str());
				else
				{
					int value = atoi(naomi_key.substr(colon + 1).c_str());
					int dc_btnnum = -1;
					if (dc_key == "x")
						dc_btnnum = 10;
					else if (dc_key == "y")
						dc_btnnum = 9;
					else if (dc_key == "a")
						dc_btnnum = 2;
					else if (dc_key == "b")
						dc_btnnum = 1;
					else if (dc_key == "c")
						dc_btnnum = 0;
					else if (dc_key == "d")
						dc_btnnum = 11;
					else if (dc_key == "z")
						dc_btnnum = 8;
					else if (dc_key == "up")
						dc_btnnum = 4;
					else if (dc_key == "down")
						dc_btnnum = 5;
					else if (dc_key == "left")
						dc_btnnum = 6;
					else if (dc_key == "right")
						dc_btnnum = 7;
					else if (dc_key == "start")
						dc_btnnum = 3;
					else
						printf("Warning: unparseable dc key %s\n", dc_key.c_str());
					if (dc_btnnum != -1)
					{
						Naomi_Mapping.button_mapping_byte[dc_btnnum] = byte;
						Naomi_Mapping.button_mapping_mask[dc_btnnum] = value;
						printf("Button %d: mapped to %d:%d\n", dc_btnnum, Naomi_Mapping.button_mapping_byte[dc_btnnum], Naomi_Mapping.button_mapping_mask[dc_btnnum]);
					}
				}
			}
		}
		if (p == s.size())
			break;
		s = trim(s.substr(p + 1));
	}

}

bool naomi_cart_LoadRom(char* file)
{

	printf("\nnullDC-Naomi rom loader v1.2\n");

	size_t folder_pos = strlen(file) - 1;
	while (folder_pos>1 && (file[folder_pos] != '\\' && file[folder_pos] != '/'))
		folder_pos--;

	folder_pos++;

	// FIXME: Data loss if buffer is too small
	char t[512];
	strncpy(t, file, sizeof(t));
	t[sizeof(t) - 1] = '\0';

	FILE* fl = fopen(t, "r");
	if (!fl)
		return false;

	char* line = fgets(t, 512, fl);
	if (!line)
	{
		fclose(fl);
		return false;
	}

	char* eon = strstr(line, "\n");
	if (!eon)
		printf("+Loading naomi rom that has no name\n", line);
	else
		*eon = 0;

	printf("+Loading naomi rom : %s\n", line);

	line = fgets(t, 512, fl);
	if (!line)
	{
		fclose(fl);
		return false;
	}

	Naomi_Mapping = naomi_default_mapping;

	vector<string> files;
	vector<u32> fstart;
	vector<u32> fsize;

	u32 setsize = 0;
	RomSize = 0;

	while (line)
	{
		if (line[0] == '#')
			parse_comment(line);
		else
		{
			char filename[512];
			u32 addr, sz;
			sscanf(line, "\"%[^\"]\",%x,%x", filename, &addr, &sz);
			files.push_back(filename);
			fstart.push_back(addr);
			fsize.push_back(sz);
			setsize += sz;
			RomSize = max(RomSize, (addr + sz));
		}
		line = fgets(t, 512, fl);
	}
	fclose(fl);

	printf("+%d romfiles, %.2f MB set size, %.2f MB set address space\n", files.size(), setsize / 1024.f / 1024.f, RomSize / 1024.f / 1024.f);

	if (RomCacheMap)
	{
		RomCacheMapCount = 0;
		delete RomCacheMap;
	}

	RomCacheMapCount = (u32)files.size();
	RomCacheMap = new fd_t[files.size()];

	// FIXME: Data loss if buffer is too small
	strncpy(t, file, sizeof(t));
	t[sizeof(t) - 1] = '\0';

	t[folder_pos] = 0;
	strcat(t, "ndcn-composed.cache");

	//Allocate space for the ram, so we are sure we have a segment of continius ram
#if HOST_OS == OS_WINDOWS
	RomPtr = (u8*)VirtualAlloc(0, RomSize, MEM_RESERVE, PAGE_NOACCESS);
#else
	RomPtr = (u8*)mmap(0, RomSize, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
#endif

	verify(RomPtr != 0);
	verify(RomPtr != (void*)-1);

	// FIXME: Data loss if buffer is too small
	strncpy(t, file, sizeof(t));
	t[sizeof(t) - 1] = '\0';

	//Create File Mapping Objects
	for (size_t i = 0; i<files.size(); i++)
	{
		t[folder_pos] = 0;
		strcat(t, files[i].c_str());
		fd_t RomCache;

		if (strcmp(files[i].c_str(), "null") == 0)
		{
			RomCacheMap[i] = INVALID_FD;
			continue;
		}
#if HOST_OS == OS_WINDOWS
		RomCache = CreateFile(t, FILE_READ_ACCESS, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
#else
		RomCache = open(t, O_RDONLY);
#endif
		if (RomCache == INVALID_FD)
		{
			wprintf(L"-Unable to read file %s\n", files[i].c_str());
			RomCacheMap[i] = INVALID_FD;
			continue;
		}

#if HOST_OS == OS_WINDOWS
		RomCacheMap[i] = CreateFileMapping(RomCache, 0, PAGE_READONLY, 0, fsize[i], 0);
		verify(CloseHandle(RomCache));
#else
		RomCacheMap[i] = RomCache;
#endif

		verify(RomCacheMap[i] != INVALID_FD);
		wprintf(L"-Preparing \"%s\" at 0x%08X, size 0x%08X\n", files[i].c_str(), fstart[i], fsize[i]);
	}

	//We have all file mapping objects, we start to map the ram
	printf("+Mapping ROM\n");
	//Release the segment we reserved so we can map the files there
#if HOST_OS == OS_WINDOWS
	verify(VirtualFree(RomPtr, 0, MEM_RELEASE));
#else
	munmap(RomPtr, RomSize);
#endif

	//Map the files into the segment of the ram that was reserved
	for (size_t i = 0; i<RomCacheMapCount; i++)
	{
		u8* RomDest = RomPtr + fstart[i];

		if (RomCacheMap[i] == INVALID_FD)
		{
			wprintf(L"-Reserving ram at 0x%08X, size 0x%08X\n", fstart[i], fsize[i]);
			
#if HOST_OS == OS_WINDOWS
			bool mapped = RomDest == VirtualAlloc(RomDest, fsize[i], MEM_RESERVE, PAGE_NOACCESS);
#else
			bool mapped = RomDest == (u8*)mmap(RomDest, RomSize, PROT_NONE, MAP_PRIVATE, 0, 0);
#endif

			verify(mapped);
		}
		else
		{
			wprintf(L"-Mapping \"%s\" at 0x%08X, size 0x%08X\n", files[i].c_str(), fstart[i], fsize[i]);
#if HOST_OS == OS_WINDOWS
			bool mapped = RomDest == MapViewOfFileEx(RomCacheMap[i], FILE_MAP_READ, 0, 0, fsize[i], RomDest);
#else
			bool mapped = RomDest == mmap(RomDest, fsize[i], PROT_READ, MAP_PRIVATE, RomCacheMap[i], 0 );
#endif
			if (!mapped)
			{
				printf("-Mapping ROM FAILED\n");
				//unmap file
				return false;
			}
		}
	}

	//done :)
	printf("\nMapped ROM Successfully !\n\n");


	return true;
}

bool naomi_cart_SelectFile(void* handle)
{
	cfgLoadStr("config", "image", SelectedFile, "null");
	
#if HOST_OS == OS_WINDOWS
	if (strcmp(SelectedFile, "null") == 0) {
		OPENFILENAME ofn = { 0 };
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hInstance = (HINSTANCE)GetModuleHandle(0);
		ofn.lpstrFile = SelectedFile;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrFilter = "*.lst\0*.lst\0\0";
		ofn.nFilterIndex = 0;
		ofn.hwndOwner = (HWND)handle;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

		if (GetOpenFileName(&ofn) <= 0)
			return true;
	}
#endif
	if (!naomi_cart_LoadRom(SelectedFile))
	{
		cfgSaveStr("emu", "gamefile", "naomi_bios");
	}
	else
	{
		cfgSaveStr("emu", "gamefile", SelectedFile);
	}


	printf("EEPROM file : %s.eeprom\n", SelectedFile);

	return true;
}

bool naomi_cart_Read(u32 offset, u32 size, void* dst) {
	if (!RomPtr)
		return false;

	memcpy(dst, naomi_cart_GetPtr(offset, size), size);
	return true;
}

void* naomi_cart_GetPtr(u32 offset, u32 size) {

	offset &= 0x0FFFffff;

	verify(offset < RomSize);
	verify((offset + size) < RomSize);

	return &RomPtr[offset];
}
