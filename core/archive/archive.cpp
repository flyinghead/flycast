/*
    Created on: Nov 23, 2018

	Copyright 2018 flyinghead

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

#include "archive.h"
#include "7zArchive.h"
#ifndef _MSC_VER
#include "ZipArchive.h"
#endif

Archive *OpenArchive(const char *path)
{
	std::string base_path(path);

#ifndef _MSC_VER
	Archive *sz_archive = new SzArchive();
	if (sz_archive->Open(base_path.c_str()) || sz_archive->Open((base_path + ".7z").c_str()) || sz_archive->Open((base_path + ".7Z").c_str()))
		return sz_archive;
	delete sz_archive;

	Archive *zip_archive = new ZipArchive();
	if (zip_archive->Open(base_path.c_str()) || zip_archive->Open((base_path + ".zip").c_str()) || zip_archive->Open((base_path + ".ZIP").c_str()))
		return zip_archive;
	delete zip_archive;
#endif

	return NULL;
}




