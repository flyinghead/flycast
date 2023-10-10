/*
	Copyright 2022 flyinghead

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
#include "scraper.h"
#include "http_client.h"
#include "stdclass.h"
#include "emulator.h"
#include "imgread/common.h"
#include "imgread/isofs.h"
#include "reios/reios.h"
#include "pvrparser.h"
#include <stb_image_write.h>
#include <random>

bool Scraper::downloadImage(const std::string& url, const std::string& localName)
{
	DEBUG_LOG(COMMON, "downloading %s", url.c_str());
	std::vector<u8> content;
	std::string contentType;
	if (!http::success(http::get(url, content, contentType)))
	{
		WARN_LOG(COMMON, "downloadImage http error: %s", url.c_str());
		return false;
	}
	if (contentType.substr(0, 6) != "image/")
	{
		WARN_LOG(COMMON, "downloadImage bad content type %s", contentType.c_str());
		return false;
	}
	if (content.empty())
	{
		WARN_LOG(COMMON, "downloadImage: empty content");
		return false;
	}
	FILE *f = nowide::fopen(localName.c_str(), "wb");
	if (f == nullptr)
	{
		WARN_LOG(COMMON, "can't create local file %s: error %d", localName.c_str(), errno);
		return false;
	}
	fwrite(&content[0], 1, content.size(), f);
	fclose(f);

	return true;
}

std::string Scraper::makeUniqueFilename(const std::string& url)
{
	static std::random_device randomDev;
	static std::mt19937 mt(randomDev());
	static std::uniform_int_distribution<int> dist(1, 1000000000);

	std::string extension = get_file_extension(url);
	std::string path;
	do {
		path = saveDirectory + std::to_string(dist(mt)) + "." + extension;
	} while (file_exists(path));
	return path;
}

void OfflineScraper::scrape(GameBoxart& item)
{
	if (item.parsed)
		return;
	item.parsed = true;
	int platform = getGamePlatform(item.fileName);
	if (platform == DC_PLATFORM_DREAMCAST)
	{
		if (item.gamePath.empty())
		{
			// Dreamcast BIOS
			item.uniqueId.clear();
			item.searchName.clear();
			return;
		}
		Disc *disc = nullptr;
		try {
			disc = OpenDisc(item.gamePath);
			if (disc == nullptr)
				WARN_LOG(COMMON, "Can't open disk %s", item.gamePath.c_str());
		} catch (const std::runtime_error& e) {
			WARN_LOG(COMMON, "Can't open disk %s: %s", item.gamePath.c_str(), e.what());
		} catch (const std::exception& e) {
			// For some reason, this doesn't catch FlycastException on macOS/clang, so we need the
			// previous catch block
			WARN_LOG(COMMON, "Can't open disk %s: %s", item.gamePath.c_str(), e.what());
		}
		if (disc == nullptr)
		{
			// No need to retry if the disk is invalid/corrupted
			item.scraped = true;
			item.uniqueId.clear();
			item.searchName.clear();
			return;
		}

		u8 sector[2048];
		disc->ReadSectors(disc->GetBaseFAD(), 1, sector, sizeof(sector));
		ip_meta_t diskId;
		memcpy(&diskId, sector, sizeof(diskId));
		// Sanity check
		if (memcmp(diskId.hardware_id, "SEGA SEGAKATANA ", sizeof(diskId.hardware_id))
				|| memcmp(diskId.maker_id, "SEGA ENTERPRISES", sizeof(diskId.maker_id)))
		{
			WARN_LOG(COMMON, "Invalid IP META for disk %s", item.gamePath.c_str());
			item.scraped = true;
			item.uniqueId.clear();
			item.searchName.clear();
			delete disc;
			return;
		}

		if (item.boxartPath.empty())
		{
			IsoFs isofs(disc);
			std::unique_ptr<IsoFs::Directory> root(isofs.getRoot());
			if (root != nullptr)
			{
				std::unique_ptr<IsoFs::Entry> entry(root->getEntry("0GDTEX.PVR"));
				if (entry != nullptr && !entry->isDirectory())
				{
					IsoFs::File *gdtexFile = (IsoFs::File *)entry.get();
					std::vector<u8> data(gdtexFile->getSize());
					gdtexFile->read(data.data(), data.size());

					std::vector<u8> out;
					u32 w, h;
					if (pvrParse(data.data(), data.size(), w, h, out))
					{
						stbi_flip_vertically_on_write(0);
						item.setBoxartPath(makeUniqueFilename("gdtex.png"));
						stbi_write_png(item.boxartPath.c_str(), w, h, 4, out.data(), 0);
					}
				}
			}
		}
		delete disc;

		item.uniqueId = trim_trailing_ws(std::string(diskId.product_number, sizeof(diskId.product_number)));
		std::replace_if(item.uniqueId.begin(), item.uniqueId.end(), [](u8 c) {
			return !std::isprint(c);
		}, ' ');

		item.searchName = trim_trailing_ws(std::string(diskId.software_name, sizeof(diskId.software_name)));
		if (item.searchName.empty())
			item.searchName = item.name;
		else
		{
			std::replace_if(item.searchName.begin(), item.searchName.end(), [](u8 c) {
				return !std::isprint(c);
			}, ' ');
		}

		if (diskId.area_symbols[0] != '\0')
		{
			item.region = 0;
			if (diskId.area_symbols[0] == 'J')
				item.region |= GameBoxart::JAPAN;
			if (diskId.area_symbols[1] == 'U')
				item.region |= GameBoxart::USA;
			if (diskId.area_symbols[2] == 'E')
				item.region |= GameBoxart::EUROPE;
		}
		else
			item.region = GameBoxart::JAPAN | GameBoxart::USA | GameBoxart::EUROPE;
	}
	else
	{
		item.uniqueId.clear();
		// Use first one in case of alternate names (Virtua Tennis / Power Smash)
		size_t spos = item.searchName.find('/');
		if (spos != std::string::npos)
			item.searchName = trim_trailing_ws(item.searchName.substr(0, spos));
		// Delete trailing (...) and [...]
		while (!item.searchName.empty())
		{
			size_t pos{ std::string::npos };
			if (item.searchName.back() == ')')
				pos = item.searchName.find_last_of('(');
			else if (item.searchName.back() == ']')
				pos = item.searchName.find_last_of('[');
			if (pos == std::string::npos)
				break;
			item.searchName = trim_trailing_ws(item.searchName.substr(0, pos));
		}
	}
}
