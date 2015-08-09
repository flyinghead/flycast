#include "naomi_cart.h"
#include "cfg/cfg.h"

u8* RomPtr;
u32 RomSize;

HANDLE*	RomCacheMap;
u32		RomCacheMapCount;

char SelectedFile[512];

OPENFILENAME ofn;

bool naomi_cart_LoadRom(char* file)
{

	printf("\nnullDC-Naomi rom loader v1.2\n");

	size_t folder_pos = strlen(file) - 1;
	while (folder_pos>1 && file[folder_pos] != '\\')
		folder_pos--;

	folder_pos++;

	char t[512];
	strcpy(t, file);
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

	vector<string> files;
	vector<u32> fstart;
	vector<u32> fsize;

	u32 setsize = 0;
	RomSize = 0;

	while (line)
	{
		char filename[512];
		u32 addr, sz;
		sscanf(line, "\"%[^\"]\",%x,%x", filename, &addr, &sz);
		files.push_back(filename);
		fstart.push_back(addr);
		fsize.push_back(sz);
		setsize += sz;
		RomSize = max(RomSize, (addr + sz));
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
	RomCacheMap = new HANDLE[files.size()];

	strcpy(t, file);
	t[folder_pos] = 0;
	strcat(t, "ndcn-composed.cache");

	//Allocate space for the ram, so we are sure we have a segment of continius ram
	RomPtr = (u8*)VirtualAlloc(0, RomSize, MEM_RESERVE, PAGE_NOACCESS);
	verify(RomPtr != 0);

	strcpy(t, file);

	//Create File Mapping Objects
	for (size_t i = 0; i<files.size(); i++)
	{
		t[folder_pos] = 0;
		strcat(t, files[i].c_str());
		HANDLE RomCache;

		if (strcmp(files[i].c_str(), "null") == 0)
		{
			RomCacheMap[i] = INVALID_HANDLE_VALUE;
			continue;
		}

		RomCache = CreateFile(t, FILE_READ_ACCESS, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

		if (RomCache == INVALID_HANDLE_VALUE)
		{
			wprintf(L"-Unable to read file %s\n", files[i].c_str());
			RomCacheMap[i] = INVALID_HANDLE_VALUE;
			continue;
		}


		RomCacheMap[i] = CreateFileMapping(RomCache, 0, PAGE_READONLY, 0, fsize[i], 0);
		verify(RomCacheMap[i] != INVALID_HANDLE_VALUE);
		wprintf(L"-Preparing \"%s\" at 0x%08X, size 0x%08X\n", files[i].c_str(), fstart[i], fsize[i]);

		verify(CloseHandle(RomCache));
	}

	//We have all file mapping objects, we start to map the ram
	printf("+Mapping ROM\n");
	//Release the segment we reserved so we can map the files there
	verify(VirtualFree(RomPtr, 0, MEM_RELEASE));

	//Map the files into the segment of the ram that was reserved
	for (size_t i = 0; i<RomCacheMapCount; i++)
	{
		u8* RomDest = RomPtr + fstart[i];

		if (RomCacheMap[i] == INVALID_HANDLE_VALUE)
		{
			wprintf(L"-Reserving ram at 0x%08X, size 0x%08X\n", fstart[i], fsize[i]);
			verify(VirtualAlloc(RomDest, fsize[i], MEM_RESERVE, PAGE_NOACCESS));
		}
		else
		{
			wprintf(L"-Mapping \"%s\" at 0x%08X, size 0x%08X\n", files[i].c_str(), fstart[i], fsize[i]);
			if (RomDest != MapViewOfFileEx(RomCacheMap[i], FILE_MAP_READ, 0, 0, fsize[i], RomDest))
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

	ZeroMemory(&ofn, sizeof(OPENFILENAME));
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