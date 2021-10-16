/*
	Copyright 2021 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "oslib/oslib.h"
#include "stdclass.h"
#include "file/file_path.h"

const char *retro_get_system_directory();

extern char game_dir_no_slash[1024];
extern char vmu_dir_no_slash[PATH_MAX];
extern char content_name[PATH_MAX];
extern unsigned per_content_vmus;
extern std::string arcadeFlashPath;

namespace hostfs
{

std::string getVmuPath(const std::string& port)
{
   char filename[PATH_MAX + 8];

   if ((per_content_vmus == 1 && port == "A1")
		   || per_content_vmus == 2)
   {
      sprintf(filename, "%s.%s.bin", content_name, port.c_str());
      return std::string(vmu_dir_no_slash) + std::string(path_default_slash()) + filename;
   }
   else
   {
      sprintf(filename, "vmu_save_%s.bin", port.c_str());
      return std::string(game_dir_no_slash) + std::string(path_default_slash()) + filename;
   }
}

std::string getArcadeFlashPath()
{
	return arcadeFlashPath;
}

std::string findFlash(const std::string& prefix, const std::string& names_ro)
{
   std::string root(game_dir_no_slash);
   root += "/";

	char base[512];
	char temp[512];
	char names[512];
	strcpy(names,names_ro.c_str());
	sprintf(base,"%s",root.c_str());

	char* curr=names;
	char* next;
	do
	{
		next=strstr(curr,";");
		if(next) *next=0;
		if (curr[0]=='%')
		{
			sprintf(temp,"%s%s%s",base,prefix.c_str(),curr+1);
		}
		else
		{
			sprintf(temp,"%s%s",base,curr);
		}

		curr=next+1;

		if (path_is_valid(temp))
			return temp;
	} while(next);

	return "";
}

std::string getFlashSavePath(const std::string& prefix, const std::string& name)
{
   std::string root(game_dir_no_slash);

	return root + path_default_slash() + prefix + name;
}

std::string findNaomiBios(const std::string& name)
{
	std::string basepath(game_dir_no_slash);
	return basepath + path_default_slash() + name;
}

std::string getSavestatePath(int index, bool writable)
{
	// Not used
	return "";
}

std::string getVulkanCachePath()
{
	return std::string(game_dir_no_slash) + std::string(path_default_slash()) + "vulkan_pipeline.cache";
}

std::string getTextureLoadPath(const std::string& gameId)
{
	return std::string(retro_get_system_directory()) + "/dc/textures/"
						+ gameId + path_default_slash();
}

std::string getTextureDumpPath()
{
	return std::string(game_dir_no_slash) + std::string(path_default_slash())
			+ "texdump" + std::string(path_default_slash());
}

std::string getBiosFontPath()
{
	return std::string(game_dir_no_slash) + std::string(path_default_slash()) + "font.bin";
}

}

void dc_savestate(int index = 0)
{
	die("unsupported");
}

void dc_loadstate(int index = 0)
{
	die("unsupported");
}
