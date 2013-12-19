
#include <string.h>

#include "types.h"

#include "hw/mem/_vmem.h"
#include "types.h"

string home_dir;

void SetHomeDir(const string& home)
{
	home_dir=home;
}

//subpath format: /data/fsca-table.bit
string GetPath(const string& subpath)
{
	return (home_dir+subpath);
}


#if 0
//File Enumeration
void FindAllFiles(FileFoundCB* callback,wchar* dir,void* param)
{
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	wchar DirSpec[MAX_PATH + 1];  // directory specification
	DWORD dwError;

	strncpy (DirSpec, dir, strlen(dir)+1);
	//strncat (DirSpec, "\\*", 3);
	
	hFind = FindFirstFile( DirSpec, &FindFileData);

	if (hFind == INVALID_HANDLE_VALUE) 
	{
		return;
	} 
	else 
	{

		if ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)==0)
		{
			callback(FindFileData.cFileName,param);
		}
u32 rv;
		while ( (rv=FindNextFile(hFind, &FindFileData)) != 0) 
		{ 
			if ((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)==0)
			{
				callback(FindFileData.cFileName,param);
			}
		}
		dwError = GetLastError();
		FindClose(hFind);
		if (dwError != ERROR_NO_MORE_FILES) 
		{
			return ;
		}
	}
	return ;
}
#endif

/*
#include "dc\sh4\rec_v1\compiledblock.h"
#include "dc\sh4\rec_v1\blockmanager.h"

bool VramLockedWrite(u8* address);
bool RamLockedWrite(u8* address,u32* sp);

*/