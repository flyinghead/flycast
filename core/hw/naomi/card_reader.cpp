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
#include "card_reader.h"
#include "oslib/oslib.h"

namespace card_reader {

InitialDCardReader initialDCardReader;

bool CardReader::loadCard(u8 *cardData, u32 len)
{
	std::string path = hostfs::getArcadeFlashPath() + ".card";
	FILE *fp = nowide::fopen(path.c_str(), "rb");
	if (fp == nullptr)
		return false;

	DEBUG_LOG(NAOMI, "Loading card file from %s", path.c_str());
	if (fread(cardData, 1, len, fp) != len)
		WARN_LOG(NAOMI, "Truncated or empty card file: %s" ,path.c_str());
	fclose(fp);

	return true;
}

void CardReader::saveCard(const u8 *cardData, u32 len)
{
	std::string path = hostfs::getArcadeFlashPath() + ".card";
	FILE *fp = nowide::fopen(path.c_str(), "wb");
	if (fp == nullptr)
	{
		WARN_LOG(NAOMI, "Can't create card file %s: errno %d", path.c_str(), errno);
		return;
	}
	DEBUG_LOG(NAOMI, "Saving card file to %s", path.c_str());
	if (fwrite(cardData, 1, len, fp) != len)
		WARN_LOG(NAOMI, "Truncated write to file: %s", path.c_str());
	fclose(fp);
}

void InitialDCardReader::write(u8 b)
{
	if (inBufferIdx == 0 && b == 5)
	{
		DEBUG_LOG(NAOMI, "Received RQ(5)");
		handleCommand();
		return;
	}
	inBuffer[inBufferIdx++] = b;
	if (inBufferIdx >= 3)
	{
		if (inBuffer[0] != 2)
		{
			INFO_LOG(NAOMI, "Unexpected cmd start byte %x", inBuffer[0]);
			inBufferIdx = 0;
			return;
		}
		u32 len = inBuffer[1];
		if (inBufferIdx < len + 2)
		{
			if (inBufferIdx == sizeof(inBuffer))
			{
				WARN_LOG(NAOMI, "Card reader buffer overflow");
				inBufferIdx = 0;
			}
			return;
		}
		u32 crc = calcCrc(&inBuffer[1], inBufferIdx - 2);
		if (crc != inBuffer[inBufferIdx - 1])
		{
			INFO_LOG(NAOMI, "Wrong crc: expected %x got %x", crc, inBuffer[inBufferIdx - 1]);
			inBufferIdx = 0;
			return;
		}
		DEBUG_LOG(NAOMI, "Received cmd %x len %d", inBuffer[2], inBuffer[1]);
		outBuffer[outBufferLen++] = 6;	// ACK
		rxCommandLen = inBufferIdx - 3;
		memcpy(rxCommand, inBuffer + 2, rxCommandLen);
		inBufferIdx = 0;
	}
}

u8 InitialDCardReader::read()
{
	verify(outBufferIdx < outBufferLen);
	u8 b = outBuffer[outBufferIdx++];
	DEBUG_LOG(NAOMI, "Sending %x", b);
	if (outBufferIdx == outBufferLen)
		outBufferIdx = outBufferLen = 0;
	return b;
}

void InitialDCardReader::insertCard()
{
	cardInserted = loadCard(cardData, sizeof(cardData));
	if (cardInserted)
		INFO_LOG(NAOMI, "Card inserted");
}

}
