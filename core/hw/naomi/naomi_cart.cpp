#include "naomi_cart.h"
#include "naomi_regs.h"
#include "cfg/cfg.h"
#include "naomi.h"
#include "deps/libzip/zip.h"
#include "decrypt.h"
#include "naomi_roms.h"
#include "hw/flashrom/flashrom.h"
#include "hw/holly/holly_intc.h"
#include "m1cartridge.h"
#include "m4cartridge.h"

Cartridge *CurrentCartridge;

#if HOST_OS == OS_WINDOWS
	typedef HANDLE fd_t;
	#define INVALID_FD INVALID_HANDLE_VALUE
#else
	typedef int fd_t;
	#define INVALID_FD -1

	#include <unistd.h>
	#include <fcntl.h>
	#include <sys/mman.h>
	#include <errno.h>
#endif

fd_t*	RomCacheMap;
u32		RomCacheMapCount;

char naomi_game_id[33];

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
	{ 0,    0,    0,    0,    0,    0,    0,    0,    0, 1,    1,    0, 0 },
	{ 0x40, 0x01, 0x02, 0x80, 0x20, 0x10, 0x08, 0x04, 0, 0x80, 0x40, 0, 0 },
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

extern RomChip sys_rom;

static bool naomi_LoadBios(const char *filename)
{
	int biosid = 0;
	for (; BIOS[biosid].name != NULL; biosid++)
		if (!stricmp(BIOS[biosid].name, filename))
			break;
	if (BIOS[biosid].name == NULL)
	{
		printf("Unknown BIOS %s\n", filename);
		return false;
	}

	struct BIOS_t *bios = &BIOS[biosid];

#if HOST_OS != OS_DARWIN
	std::string basepath = get_readonly_data_path("/data/");
#else
	std::string basepath = get_readonly_data_path("/");
#endif
	zip *zip_archive = zip_open((basepath + filename).c_str(), 0, NULL);
	if (zip_archive == NULL)
	{
		printf("Cannot find BIOS %s\n", filename);
		return false;
	}

	int romid = 0;
	while (bios->blobs[romid].filename != NULL)
	{
		if (bios->blobs[romid].blob_type == Copy)
		{
			verify(bios->blobs[romid].offset + bios->blobs[romid].length <= BIOS_SIZE);
			verify(bios->blobs[romid].src_offset + bios->blobs[romid].length <= BIOS_SIZE);
			memcpy(sys_rom.data + bios->blobs[romid].offset, sys_rom.data + bios->blobs[romid].src_offset, bios->blobs[romid].length);
			printf("Copied: %x bytes from %07x to %07x\n", bios->blobs[romid].length, bios->blobs[romid].src_offset, bios->blobs[romid].offset);
		}
		else
		{
			zip_file* file = zip_fopen(zip_archive, bios->blobs[romid].filename, 0);
			if (!file) {
				printf("%s: Cannot open %s\n", filename, bios->blobs[romid].filename);
				goto error;
			}
			if (bios->blobs[romid].blob_type == Normal)
			{
				verify(bios->blobs[romid].offset + bios->blobs[romid].length <= BIOS_SIZE);
				size_t read = zip_fread(file, sys_rom.data + bios->blobs[romid].offset, bios->blobs[romid].length);
				printf("Mapped %s: %lx bytes at %07x\n", bios->blobs[romid].filename, read, bios->blobs[romid].offset);
			}
			else if (bios->blobs[romid].blob_type == InterleavedWord)
			{
				u8 *buf = (u8 *)malloc(bios->blobs[romid].length);
				if (buf == NULL)
				{
					printf("malloc failed\n");
					zip_fclose(file);
					goto error;
				}
				verify(bios->blobs[romid].offset + bios->blobs[romid].length <= BIOS_SIZE);
				size_t read = zip_fread(file, buf, bios->blobs[romid].length);
				u16 *to = (u16 *)(sys_rom.data + bios->blobs[romid].offset);
				u16 *from = (u16 *)buf;
				for (int i = bios->blobs[romid].length / 2; --i >= 0; to++)
					*to++ = *from++;
				free(buf);
				printf("Mapped %s: %lx bytes (interleaved word) at %07x\n", bios->blobs[romid].filename, read, bios->blobs[romid].offset);
			}
			else
				die("Unknown blob type\n");
			zip_fclose(file);
		}
		romid++;
	}

	zip_close(zip_archive);

	return true;

error:
	zip_close(zip_archive);
	return false;
}

static bool naomi_cart_LoadZip(char *filename)
{
	char *p = strrchr(filename, '/');
#ifdef _WIN32
	p = strrchr(p == NULL ? filename : p, '\\');
#endif
	if (p == NULL)
		p = filename;
	else
		p++;

	int gameid = 0;
	for (; Games[gameid].name != NULL; gameid++)
		if (!stricmp(Games[gameid].name, p))
			break;
	if (Games[gameid].name == NULL)
	{
		printf("Unknown game %s\n", filename);
		return false;
	}

	struct Game *game = &Games[gameid];

	if (game->bios != NULL)
	{
		if (!naomi_LoadBios(game->bios))
			return false;
	}

	zip *zip_archive = zip_open(filename, 0, NULL);
	if (zip_archive == NULL)
	{
		printf("Cannot open %s\n", filename);
		return false;
	}

	switch (game->cart_type)
	{
	case M1:
		{
			M1Cartridge *cart = new M1Cartridge(game->size);
			cart->SetKey(game->key);
			CurrentCartridge = cart;
		}
		break;
	case M2:
		{
			M2Cartridge *cart = new M2Cartridge(game->size);
			cart->SetKey(game->key);
			CurrentCartridge = cart;
		}
		break;
	case M4:
		{
			M4Cartridge *cart = new M4Cartridge(game->size);
			cart->SetM4Id(game->key);
			CurrentCartridge = cart;
		}
		break;
	default:
		die("Unsupported cartridge type\n");
		break;
	}

	int romid = 0;
	while (game->blobs[romid].filename != NULL)
	{
		u32 len = game->blobs[romid].length;

		if (game->blobs[romid].blob_type == Copy)
		{
			u8 *dst = (u8 *)CurrentCartridge->GetPtr(game->blobs[romid].offset, len);
			u8 *src = (u8 *)CurrentCartridge->GetPtr(game->blobs[romid].src_offset, len);
			memcpy(dst, src, game->blobs[romid].length);
			printf("Copied: %x bytes from %07x to %07x\n", game->blobs[romid].length, game->blobs[romid].src_offset, game->blobs[romid].offset);
		}
		else
		{
			zip_file* file = zip_fopen(zip_archive, game->blobs[romid].filename, 0);
			if (!file) {
				printf("%s: Cannot open %s\n", filename, game->blobs[romid].filename);
				goto error;
			}
			if (game->blobs[romid].blob_type == Normal)
			{
				u8 *dst = (u8 *)CurrentCartridge->GetPtr(game->blobs[romid].offset, len);
				size_t read = zip_fread(file, dst, game->blobs[romid].length);
				printf("Mapped %s: %lx bytes at %07x\n", game->blobs[romid].filename, read, game->blobs[romid].offset);
			}
			else if (game->blobs[romid].blob_type == InterleavedWord)
			{
				u8 *buf = (u8 *)malloc(game->blobs[romid].length);
				if (buf == NULL)
				{
					printf("malloc failed\n");
					zip_fclose(file);
					goto error;
				}
				size_t read = zip_fread(file, buf, game->blobs[romid].length);
				u16 *to = (u16 *)CurrentCartridge->GetPtr(game->blobs[romid].offset, len);
				u16 *from = (u16 *)buf;
				for (int i = game->blobs[romid].length / 2; --i >= 0; to++)
					*to++ = *from++;
				free(buf);
				printf("Mapped %s: %lx bytes (interleaved word) at %07x\n", game->blobs[romid].filename, read, game->blobs[romid].offset);
			}
			else if (game->blobs[romid].blob_type == M4Key)
			{
				verify(game->cart_type == M4);
				u8 *buf = (u8 *)malloc(game->blobs[romid].length);
				if (buf == NULL)
				{
					printf("malloc failed\n");
					zip_fclose(file);
					goto error;
				}
				size_t read = zip_fread(file, buf, game->blobs[romid].length);
				((M4Cartridge *)CurrentCartridge)->SetKeyData(buf);
				printf("Loaded %s: %lx bytes M4 Key\n", game->blobs[romid].filename, read);
			}
			else
				die("Unknown blob type\n");
			zip_fclose(file);
		}
		romid++;
	}
	zip_close(zip_archive);

	strcpy(naomi_game_id, CurrentCartridge->GetGameId().c_str());
	printf("NAOMI GAME ID [%s]\n", naomi_game_id);

	return true;

error:
	zip_close(zip_archive);
	delete CurrentCartridge;
	CurrentCartridge = NULL;
	return false;
}

bool naomi_cart_LoadRom(char* file)
{
	printf("\nnullDC-Naomi rom loader v1.2\n");

	if (CurrentCartridge != NULL)
	{
		delete CurrentCartridge;
		CurrentCartridge = NULL;
	}

	size_t folder_pos = strlen(file) - 1;
	while (folder_pos>1 && (file[folder_pos] != '\\' && file[folder_pos] != '/'))
		folder_pos--;

	folder_pos++;

	// FIXME: Data loss if buffer is too small
	char t[512];
	strncpy(t, file, sizeof(t));
	t[sizeof(t) - 1] = '\0';

	Naomi_Mapping = naomi_default_mapping;

	vector<string> files;
	vector<u32> fstart;
	vector<u32> fsize;
	u32 setsize = 0;
	bool raw_bin_file = false;

	char *pdot = strrchr(file, '.');

	if (pdot != NULL && (!strcmp(pdot, ".zip") || !strcmp(pdot, ".ZIP")))
		return naomi_cart_LoadZip(file);

	u8* RomPtr;
	u32 RomSize;

	if (pdot != NULL && (!strcmp(pdot, ".lst") || !strcmp(pdot, ".LST")))
	{
		// LST file

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
			printf("+Loading naomi rom that has no name\n");
		else
			*eon = 0;

		printf("+Loading naomi rom : %s\n", line);

		line = fgets(t, 512, fl);
		if (!line)
		{
			fclose(fl);
			return false;
		}

		RomSize = 0;

		while (line)
		{
			if (line[0] == '#')
				parse_comment(line);
			else
			{
				char filename[512];
				u32 addr, sz;
				if (sscanf(line, "\"%[^\"]\",%x,%x", filename, &addr, &sz) == 3)
				{
					files.push_back(filename);
					fstart.push_back(addr);
					fsize.push_back(sz);
					setsize += sz;
					RomSize = max(RomSize, (addr + sz));
				}
				else if (line[0] != 0 && line[0] != '\n' && line[0] != '\r')
					printf("Warning: invalid line in .lst file: %s\n", line);
			}
			line = fgets(t, 512, fl);
		}
		fclose(fl);
	}
	else
	{
		// BIN loading
		FILE* fp = fopen(t, "rb");
		if (fp == NULL)
			return false;

		fseek(fp, 0, SEEK_END);
		u32 file_size = ftell(fp);
		fclose(fp);
		files.push_back(t);
		fstart.push_back(0);
		fsize.push_back(file_size);
		setsize = file_size;
		RomSize = file_size;
		raw_bin_file = true;
	}

	printf("+%ld romfiles, %.2f MB set size, %.2f MB set address space\n", files.size(), setsize / 1024.f / 1024.f, RomSize / 1024.f / 1024.f);

	if (RomCacheMap)
	{
		RomCacheMapCount = 0;
		delete[] RomCacheMap;
	}

	RomCacheMapCount = (u32)files.size();
	RomCacheMap = new fd_t[files.size()]();

	//Allocate space for the ram, so we are sure we have a segment of continius ram
#if HOST_OS == OS_WINDOWS
	RomPtr = (u8*)VirtualAlloc(0, RomSize, MEM_RESERVE, PAGE_NOACCESS);
#else
	RomPtr = (u8*)mmap(0, RomSize, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
#endif

	verify(RomPtr != 0);
	verify(RomPtr != (void*)-1);

	bool load_error = false;

	//Create File Mapping Objects
	for (size_t i = 0; i<files.size(); i++)
	{
		if (!raw_bin_file)
		{
			strncpy(t, file, sizeof(t));
			t[sizeof(t) - 1] = '\0';
			t[folder_pos] = 0;
			strcat(t, files[i].c_str());
		}
		else
		{
			strncpy(t, files[i].c_str(), sizeof(t));
			t[sizeof(t) - 1] = '\0';
		}

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
			printf("-Unable to read file %s: error %d\n", t, errno);
			RomCacheMap[i] = INVALID_FD;
			load_error = true;
			break;
		}

#if HOST_OS == OS_WINDOWS
		// Windows doesn't allow mapping a read-only file to a memory area larger than the file size
		BY_HANDLE_FILE_INFORMATION file_info;
		GetFileInformationByHandle(RomCache, &file_info);
		fsize[i] = file_info.nFileSizeLow;
		RomCacheMap[i] = CreateFileMapping(RomCache, 0, PAGE_READONLY, 0, fsize[i], 0);
		verify(RomCacheMap[i] != NULL);
		verify(CloseHandle(RomCache));
#else
		RomCacheMap[i] = RomCache;
#endif

		verify(RomCacheMap[i] != INVALID_FD);
		//printf("-Preparing \"%s\" at 0x%08X, size 0x%08X\n", files[i].c_str(), fstart[i], fsize[i]);
	}

	//Release the segment we reserved so we can map the files there
#if HOST_OS == OS_WINDOWS
	verify(VirtualFree(RomPtr, 0, MEM_RELEASE));
#else
	munmap(RomPtr, RomSize);
#endif

	if (load_error)
	{
		for (size_t i = 0; i < files.size(); i++)
			if (RomCacheMap[i] != INVALID_FD)
#if HOST_OS == OS_WINDOWS
				CloseHandle(RomCacheMap[i]);
#else
				close(RomCacheMap[i]);
#endif
		return false;
	}

	//We have all file mapping objects, we start to map the ram

	//Map the files into the segment of the ram that was reserved
	for (size_t i = 0; i<RomCacheMapCount; i++)
	{
		u8* RomDest = RomPtr + fstart[i];

		if (RomCacheMap[i] == INVALID_FD)
		{
			//printf("-Reserving ram at 0x%08X, size 0x%08X\n", fstart[i], fsize[i]);
			
#if HOST_OS == OS_WINDOWS
			bool mapped = RomDest == VirtualAlloc(RomDest, fsize[i], MEM_RESERVE, PAGE_NOACCESS);
#else
			bool mapped = RomDest == (u8*)mmap(RomDest, RomSize, PROT_NONE, MAP_PRIVATE, 0, 0);
#endif

			verify(mapped);
		}
		else
		{
			//printf("-Mapping \"%s\" at 0x%08X, size 0x%08X\n", files[i].c_str(), fstart[i], fsize[i]);
#if HOST_OS == OS_WINDOWS
			bool mapped = RomDest == MapViewOfFileEx(RomCacheMap[i], FILE_MAP_READ, 0, 0, fsize[i], RomDest);
#else
			bool mapped = RomDest == mmap(RomDest, fsize[i], PROT_READ, MAP_PRIVATE, RomCacheMap[i], 0 );
#endif
			if (!mapped)
			{
				printf("-Mapping ROM FAILED: %s @ %08x size %x\n", files[i].c_str(), fstart[i], fsize[i]);
				return false;
			}
		}
	}

	//done :)
	printf("\nMapped ROM Successfully !\n\n");

	CurrentCartridge = new DecryptedCartridge(RomPtr, RomSize);
	strcpy(naomi_game_id, CurrentCartridge->GetGameId().c_str());
	printf("NAOMI GAME ID [%s]\n", naomi_game_id);

	return true;
}

bool naomi_cart_SelectFile(void* handle)
{
	char SelectedFile[512];

	cfgLoadStr("config", "image", SelectedFile, "null");
	
#if HOST_OS == OS_WINDOWS
	if (strcmp(SelectedFile, "null") == 0) {
		OPENFILENAME ofn = { 0 };
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hInstance = (HINSTANCE)GetModuleHandle(0);
		ofn.lpstrFile = SelectedFile;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrFilter = "*.lst\0*.lst\0*.bin\0*.bin\0*.dat\0*.dat\0\0";
		ofn.nFilterIndex = 0;
		ofn.hwndOwner = (HWND)handle;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

		if (GetOpenFileName(&ofn) <= 0)
			return true;
	}
#endif
	if (!naomi_cart_LoadRom(SelectedFile))
	{
		printf("Cannot load %s: error %d\n", SelectedFile, errno);
		return false;
	}

	cfgSaveStr("emu", "gamefile", SelectedFile);

	return true;
}

Cartridge::Cartridge(u32 size)
{
	RomPtr = (u8 *)malloc(size);
	RomSize = size;
	memset(RomPtr, 0xFF, RomSize);
}

Cartridge::~Cartridge()
{
	free(RomPtr);
}

bool Cartridge::Read(u32 offset, u32 size, void* dst)
{
	offset &= 0x1FFFFFFF;
	if (offset >= RomSize || (offset + size) > RomSize)
	{
		static u32 ones = 0xffffffff;

		// Makes Outtrigger boot
		EMUERROR("offset %d > %d\n", offset, RomSize);
		memcpy(dst, &ones, size);
	}
	else
	{
		memcpy(dst, &RomPtr[offset], size);
	}

	return true;
}

bool Cartridge::Write(u32 offset, u32 size, u32 data)
{
	EMUERROR("Invalid write @ %08x data %x\n", offset, data);
	return false;
}

void* Cartridge::GetPtr(u32 offset, u32& size)
{
	offset &= 0x1FFFffff;

	verify(offset < RomSize);
	verify((offset + size) <= RomSize);

	return &RomPtr[offset];
}

std::string Cartridge::GetGameId() {
	if (RomSize < 0x30 + 0x20)
		return "(ROM too small)";

	std::string game_id((char *)RomPtr + 0x30, 0x20);
	if (game_id == "AWNAOMI                         " && RomSize >= 0xFF50)
	{
		game_id = std::string((char *)RomPtr + 0xFF30, 0x20);
	}
	while (!game_id.empty() && game_id.back() == ' ')
		game_id.pop_back();
	return game_id;
}

void* NaomiCartridge::GetDmaPtr(u32& size)
{
	if ((DmaOffset & 0x1fffffff) >= RomSize)
	{
		EMUERROR("Error: DmaOffset >= RomSize\n");
		size = 0;
		return NULL;
	}
	size = min(size, RomSize - (DmaOffset & 0x1fffffff));
	return GetPtr(DmaOffset, size);
}

void NaomiCartridge::AdvancePtr(u32 size) {
}

u32 NaomiCartridge::ReadMem(u32 address, u32 size)
{
	verify(size!=1);
	//printf("+naomi?WTF? ReadMem: %X, %d\n", address, size);
	switch(address & 255)
	{
	case 0x3c:
		EMUERROR("naomi GD? READ: %X, %d", address, size);
		return reg_dimm_3c | (NaomiDataRead ? 0 : -1); //pretend the board isn't there for the bios
	case 0x40:
		EMUERROR("naomi GD? READ: %X, %d", address, size);
		return reg_dimm_40;
	case 0x44:
		EMUERROR("naomi GD? READ: %X, %d", address, size);
		return reg_dimm_44;
	case 0x48:
		EMUERROR("naomi GD? READ: %X, %d", address, size);
		return reg_dimm_48;

		//These are known to be valid on normal ROMs and DIMM board
	case NAOMI_ROM_OFFSETH_addr&255:
		return RomPioOffset>>16 | (RomPioAutoIncrement << 15);

	case NAOMI_ROM_OFFSETL_addr&255:
		return RomPioOffset&0xFFFF;

	case NAOMI_ROM_DATA_addr & 255:
		{
			u32 rv = 0;
			Read(RomPioOffset, 2, &rv);
			if (RomPioAutoIncrement)
				RomPioOffset += 2;

			return rv;
		}

	case NAOMI_DMA_COUNT_addr&255:
		return (u16) DmaCount;

	case NAOMI_BOARDID_READ_addr&255:
		return NaomiGameIDRead()?0x8000:0x0000;

		//What should i do to emulate 'nothing' ?
	case NAOMI_COMM_OFFSET_addr&255:
		#ifdef NAOMI_COMM
		printf("naomi COMM offs READ: %X, %d\n", address, size);
		return CommOffset;
		#endif
	case NAOMI_COMM_DATA_addr&255:
		#ifdef NAOMI_COMM
		printf("naomi COMM data read: %X, %d\n", CommOffset, size);
		if (CommSharedMem)
		{
			return CommSharedMem[CommOffset&0xF];
		}
		#endif
		return 1;


		//This should be valid
	case NAOMI_DMA_OFFSETH_addr&255:
		return DmaOffset>>16;
	case NAOMI_DMA_OFFSETL_addr&255:
		return DmaOffset&0xFFFF;

	case NAOMI_BOARDID_WRITE_addr&255:
		EMUERROR("naomi ReadBoardId: %X, %d", address, size);
		return 1;

	case 0x04C:
		EMUERROR("naomi GD? READ: %X, %d", address, size);
		return reg_dimm_4c;

	case 0x18:
		printf("naomi reg 0x18 : returning random data\n");
		return 0x4000^rand();
		break;

	default: break;
	}
	//EMUERROR("naomi?WTF? ReadMem: %X, %d", address, size);

	return 0xFFFF;
}

void NaomiCartridge::WriteMem(u32 address, u32 data, u32 size)
{
	//	printf("+naomi WriteMem: %X <= %X, %d\n", address, data, size);
	switch(address & 255)
	{
	case 0x3c:
		 if (0x1E03==data)
		 {
			 /*
			 if (!(reg_dimm_4c&0x100))
				asic_RaiseInterrupt(holly_EXP_PCI);
			 reg_dimm_4c|=1;*/
		 }
		 reg_dimm_3c=data;
		 EMUERROR("naomi GD? Write: %X <= %X, %d", address, data, size);
		 return;

	case 0x40:
		reg_dimm_40=data;
		EMUERROR("naomi GD? Write: %X <= %X, %d", address, data, size);
		return;
	case 0x44:
		reg_dimm_44=data;
		EMUERROR("naomi GD? Write: %X <= %X, %d", address, data, size);
		return;
	case 0x48:
		reg_dimm_48=data;
		EMUERROR("naomi GD? Write: %X <= %X, %d", address, data, size);
		return;

	case 0x4C:
		if (data&0x100)
		{
			asic_CancelInterrupt(holly_EXP_PCI);
			naomi_updates=100;
		}
		else if ((data&1)==0)
		{
			/*FILE* ramd=fopen("c:\\ndc.ram.bin","wb");
			fwrite(mem_b.data,1,RAM_SIZE,ramd);
			fclose(ramd);*/
			naomi_process(reg_dimm_3c,reg_dimm_40,reg_dimm_44,reg_dimm_48);
		}
		reg_dimm_4c=data&~0x100;
		EMUERROR("naomi GD? Write: %X <= %X, %d", address, data, size);
		return;

		//These are known to be valid on normal ROMs and DIMM board
	case NAOMI_ROM_OFFSETH_addr&255:
		RomPioAutoIncrement = (data & 0x8000) != 0;
		RomPioOffset&=0x0000ffff;
		RomPioOffset|=(data<<16)&0x7fff0000;
		PioOffsetChanged(RomPioOffset);
		return;

	case NAOMI_ROM_OFFSETL_addr&255:
		RomPioOffset&=0xffff0000;
		RomPioOffset|=data;
		PioOffsetChanged(RomPioOffset);
		return;

	case NAOMI_ROM_DATA_addr&255:
		Write(RomPioOffset, size, data);
		if (RomPioAutoIncrement)
			RomPioOffset += 2;

		return;

	case NAOMI_DMA_OFFSETH_addr&255:
		DmaOffset&=0x0000ffff;
		DmaOffset|=(data&0x7fff)<<16;
		DmaOffsetChanged(DmaOffset);
		return;

	case NAOMI_DMA_OFFSETL_addr&255:
		DmaOffset&=0xffff0000;
		DmaOffset|=data;
		DmaOffsetChanged(DmaOffset);
		return;

	case NAOMI_DMA_COUNT_addr&255:
		{
			DmaCount=data;
		}
		return;
	case NAOMI_BOARDID_WRITE_addr&255:
		NaomiGameIDWrite((u16)data);
		return;

		//What should i do to emulate 'nothing' ?
	case NAOMI_COMM_OFFSET_addr&255:
#ifdef NAOMI_COMM
		printf("naomi COMM ofset Write: %X <= %X, %d\n", address, data, size);
		CommOffset=data&0xFFFF;
#endif
		return;

	case NAOMI_COMM_DATA_addr&255:
		#ifdef NAOMI_COMM
		printf("naomi COMM data Write: %X <= %X, %d\n", CommOffset, data, size);
		if (CommSharedMem)
		{
			CommSharedMem[CommOffset&0xF]=data;
		}
		#endif
		return;

		//This should be valid
	case NAOMI_BOARDID_READ_addr&255:
		EMUERROR("naomi WriteMem: %X <= %X, %d", address, data, size);
		return;

	default: break;
	}
	EMUERROR("naomi?WTF? WriteMem: %X <= %X, %d", address, data, size);
}

void NaomiCartridge::Serialize(void** data, unsigned int* total_size)
{
	REICAST_S(RomPioOffset);
	REICAST_S(RomPioAutoIncrement);
	REICAST_S(DmaOffset);
	REICAST_S(DmaCount);
	Cartridge::Serialize(data, total_size);
}

void NaomiCartridge::Unserialize(void** data, unsigned int* total_size)
{
	REICAST_US(RomPioOffset);
	REICAST_US(RomPioAutoIncrement);
	REICAST_US(DmaOffset);
	REICAST_US(DmaCount);
	Cartridge::Unserialize(data, total_size);
}

bool M2Cartridge::Read(u32 offset, u32 size, void* dst)
{
	if (offset & 0x40000000)
	{
		if (offset == 0x4001fffe)
		{
			//printf("NAOMI CART DECRYPT read: %08x sz %d\n", offset, size);
			cyptoSetKey(key);
			u16 data = cryptoDecrypt();
			*(u16 *)dst = data;
			return true;
		}
		EMUERROR("Invalid read @ %08x\n", offset);
		return false;
	}
	return NaomiCartridge::Read(offset, size, dst);
}

bool M2Cartridge::Write(u32 offset, u32 size, u32 data)
{
	if (offset & 0x40000000)
	{
		//printf("NAOMI CART CRYPT write: %08x data %x sz %d\n", offset, data, size);
		if (offset & 0x00020000)
		{
			offset &= sizeof(naomi_cart_ram) - 1;
			naomi_cart_ram[offset] = data;
			naomi_cart_ram[offset + 1] = data >> 8;
			return true;
		}
		switch (offset & 0x1ffff)
		{
			case 0x1fff8:
				cyptoSetLowAddr(data);
				return true;
			case 0x1fffa:
				cyptoSetHighAddr(data);
				return true;
			case 0x1fffc:
				cyptoSetSubkey(data);
				return true;
		}
	}
	return NaomiCartridge::Write(offset, size, data);
}

u16 M2Cartridge::ReadCipheredData(u32 offset)
{
	if ((offset & 0xffff0000) == 0x01000000)
	{
		int base = 2 * (offset & 0x7fff);
		return naomi_cart_ram[base + 1] | (naomi_cart_ram[base] << 8);
	}
	verify(2 * offset + 1 < RomSize);
	return RomPtr[2 * offset + 1] | (RomPtr[2 * offset] << 8);

}

void M2Cartridge::Serialize(void** data, unsigned int* total_size) {
	REICAST_S(naomi_cart_ram);
	NaomiCartridge::Serialize(data, total_size);
}

void M2Cartridge::Unserialize(void** data, unsigned int* total_size) {
	REICAST_US(naomi_cart_ram);
	NaomiCartridge::Unserialize(data, total_size);
}
