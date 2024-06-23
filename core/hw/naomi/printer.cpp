/*
	Copyright 2023 flyinghead

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
#include "types.h"
#include "stdclass.h"
#include "printer.h"
#include "serialize.h"
#include "oslib/oslib.h"
#include <cassert>
#include <memory>
#include <vector>
#ifdef STANDALONE_TEST
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#undef INFO_LOG
#define INFO_LOG(t, s, ...) printf(s "\n",  __VA_ARGS__)
#undef NOTICE_LOG
#define NOTICE_LOG INFO_LOG
#undef ERROR_LOG
#define ERROR_LOG INFO_LOG
#else
#include "oslib/resources.h"
#endif
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

namespace printer
{

static u8 reverseBits(u8 b)
{
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}

class BitmapWriter
{
public:
	BitmapWriter(int printerWidth) : printerWidth(printerWidth)
	{
#ifndef STANDALONE_TEST
		size_t size;
		ascii8x16 = resource::load("fonts/printer_ascii8x16.bin", size);
		ascii12x24 = resource::load("fonts/printer_ascii12x24.bin", size);
		kanji16x16 = resource::load("fonts/printer_kanji16x16.bin", size);
		kanji24x24 = resource::load("fonts/printer_kanji24x24.bin", size);
#else
		ascii8x16 = loadFont("printer_ascii8x16.bin");
		ascii12x24 = loadFont("printer_ascii12x24.bin");
		kanji16x16 = loadFont("printer_kanji16x16.bin");
		kanji24x24 = loadFont("printer_kanji24x24.bin");
#endif
	}

	template<typename T>
	void print(T text)
	{
		if (text == '\n' || text == '\r')
		{
			linefeed();
			return;
		}
		const int hScale = 1 + doubleHeight;
		const int wScale = 1 + doubleWidth;
		const u8 *glyph;
		int width;
		int height;
		bool msb = true;
		if (sizeof(T) == 1 && customCharsEnabled && (u8)text < customChars.size() && customChars[(u8)text].width > 0)
		{
			glyph = &customChars[text].data[0];
			width = customChars[text].width; // / wScale; // FIXME tduno2 hack, breaks tduno
			height = customChars[text].height;
			msb = msbBitmap;
		}
		else
		{
			glyph = getGlyph(text);
			width = sizeof(T) == 1 ?
							bigFont ? 12 : 8
						  : bigFont ? 24 : 16;
			height = bigFont ? 24 : 16;
		}

		if (penx + width * wScale > printerWidth)
			linefeed();
		maxLineHeight = std::max(maxLineHeight, height * hScale + maxUnderline);
		u8 *pen = getPenPosition(height * hScale);
		for (int y = 0; y < height; y++)
		{
			for (int dbh = 0; dbh < hScale; dbh++)
			{
				for (int x = 0; x < width && pen <= &page.back(); x++)
				{
					const u8 *src = glyph + x / 8;
					bool b = *src & (msb ? 0x80 >> (x % 8) : 1 << (x % 8));
					b ^= reversed;
					if (b)
					{
						if (xorMode)
							*pen ^= 0xff;
						else
							*pen |= 0xff;
					}
					pen++;
					if (wScale > 1 && pen <= &page.back())
					{
						if (b)
						{
							if (xorMode)
								*pen ^= 0xff;
							else
								*pen |= 0xff;
						}
						pen++;
					}
				}
				pen += printerWidth - width * wScale;
			}
			glyph += (width + 7) / 8;
		}
		if (!reversed) // reversed has priority over underline
			for (int y = 0; y < maxUnderline; y++)
			{
				u8 *pen = getPenPosition(height * hScale + y) + printerWidth * (height * hScale + y);
				const int ulwidth = width * wScale + hspace * (sizeof(T) == 1 ? 1 : 2);
				for (int x = 0; x < ulwidth && pen <= &page.back(); x++)
				{
					if (xorMode)
						*pen ^= 0xff;
					else
						*pen |= 0xff;
					pen++;
				}
			}
		penx += width * wScale;
		if (reversed)
		{
			// Fill inter-char space
			// FIXME only if there's another char on the same line
			u8 *pen = getPenPosition(height * hScale);
			for (int y = 0; y < height * hScale; y++)
			{
				for (int x = 0; x < hspace && pen <= &page.back(); x++)
				{
					if (xorMode)
						*pen++ ^= 0xff;
					else
						*pen++ |= 0xff;
				}
				pen += printerWidth - hspace;
			}
		}
		penx += hspace * (sizeof(T) == 1 ? 1 : 2);
		printBufferEmpty = false;
	}

	void printImage(int hpos, int width, int height, const u8 *data, bool msb)
	{
		//printf("printImage: hpos %d w %d h %d msb %d\n", hpos, width, height, msb);
		const int savePenx = penx;
		penx = hpos;
		u8 *pen = getPenPosition(height);
		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
			{
				bool b = data[x / 8] & (msb ? 0x80 >> (x % 8) : 1 << (x % 8));
				if (b)
				{
					if (xorMode)
						*pen ^= 0xff;
					else
						*pen |= 0xff;
				}
				pen++;
			}
			data += (width + 7) / 8;
			pen += printerWidth - width;
		}
		penx = savePenx;
		if (_advancePenOnBitmap)
			// F355 and tduno[2] seem to handle bitmap differently. tduno[2] needs the pen to advance when printing bitmaps,
			// and will do reverse feed to print over it. F355 doesn't reverse feed so the pen must not advance.
			peny += height;
	}

	void linefeed(int dots)
	{
		if (!printBufferEmpty)
			linefeed();
		if (dots > 0)
			getPenPosition(dots);
		penx = 0;
		peny = std::max(0, peny + dots);
	}

	void selectFont(bool big) {
		bigFont = big;
	}
	void setReversed(bool enable) {
		reversed = enable;
	}
	void setHSpace(int dots) {
		hspace = dots;
	}
	void setVSpace(int dots) {
		vspace = dots;
	}
	void setDoubleWidth(bool enabled) {
		doubleWidth = enabled;
	}
	void setDoubleHeight(bool enabled) {
		doubleHeight = enabled;
	}
	bool getBitmapMSB() const {
		return msbBitmap;
	}
	void setBitmapMSB(bool enabled) {
		msbBitmap = enabled;
	}
	void setXorMode(bool enabled) {
		xorMode = enabled;
	}
	void setMaxCharWidth(int width) {
		maxCharWidth = width;
	}
	void setCustomChar(char code, int width, int height, const u8 *data)
	{
		if ((u8)code >= customChars.size())
			customChars.resize(code + 1);
		CustomChar& cchar = customChars[code];
		cchar.width = std::min(width, maxCharWidth);
		cchar.height = height;
		cchar.data.resize((cchar.width + 7) / 8 * height);
		if (msbBitmap)
		{
			int widthBytes = (cchar.width + 7) / 8;
			for (int y = 0; y < height; y++)
				for (int x = 0; x < widthBytes; x++)
					cchar.data[widthBytes * y + x] = reverseBits(data[(width + 7) / 8 * y + x]);
		}
		else if (cchar.width == width) {
			memcpy(cchar.data.data(), data, cchar.data.size());
		}
		else
		{
			for (int y = 0; y < height; y++)
				memcpy(cchar.data.data() + cchar.width / 8 * y,
						data + (width + 7) / 8 * y,
						cchar.width / 8);
		}
	}
	void enableCustomChars(bool enable) {
		customCharsEnabled = enable;
	}
	void drawRuledLine(int from, int to)
	{
		if (from > to)
			std::swap(from, to);
		if (ruledLine.empty())
			ruledLine.resize(printerWidth);
		for (int d = from; d <= to && d < (int)ruledLine.size(); d++)
			ruledLine[d] = 0xff;
	}
	void drawRuledPattern(u8 n1, u8 n2)
	{
		if (ruledLine.empty())
			ruledLine.resize(printerWidth);
		u8 pattern[16];
		for (int i = 0; i < 8; i++)
		{
			u8 mask = msbBitmap ? 0x80 >> i : 1 << i;
			pattern[i] = (n1 & mask) ? 0xff : 0;
			pattern[8 + i] = (n2 & mask) ? 0xff : 0;
		}
		for (int d = 0; d < (int)ruledLine.size(); d++)
			ruledLine[d] = pattern[d % std::size(pattern)];
	}
	void clearRuledLine() {
		ruledLine.clear();
	}
	void printRuledLine()
	{
		if (!ruledLineMode)
		{
			linefeed(1);
			return;
		}
		if (!printBufferEmpty)
			linefeed();
		penx = 0;
		u8 *pen = getPenPosition(1);
		for (int x = 0; x < printerWidth && x < (int)ruledLine.size(); x++)
		{
			if (ruledLine[x] != 0)
			{
				if (xorMode)
					*pen ^= 0xff;
				else
					*pen |= 0xff;
			}
			pen++;
		}
		peny++;
	}
	void setRuledLineMode(bool enabled) {
		ruledLineMode = enabled;
	}

	void advancePenOnBitmap() {
		_advancePenOnBitmap = true;
	}

	void setUnderline(int dots) {
		underline = dots;
		maxUnderline = std::max(maxUnderline, underline);
	}

	bool isDirty() const {
		return lines > 0;
	}
	bool save(const std::string& filename)
	{
		if (page.empty())
			return false;
		const auto& appendData = [](void *context, void *data, int size) {
			std::vector<u8>& v = *(std::vector<u8> *)context;
			v.insert(v.end(), (u8 *)data, (u8 *)data + size);
		};
		stbi_flip_vertically_on_write(0);
		if (settings.content.gameId.substr(0, 4) == "F355")
		{
			u8 *data = nullptr;
			int x, y, comp;
#ifndef STANDALONE_TEST
			size_t size;
			std::unique_ptr<u8[]> fileData = resource::load("picture/f355_print_template.png", size);
			data = stbi_load_from_memory(fileData.get(), size, &x, &y, &comp, STBI_rgb_alpha);
#else
			FILE *f = fopen("f355_print_template.png", "rb");
			if (f != nullptr)
			{
				data = stbi_load_from_file(f, &x, &y, &comp, STBI_rgb_alpha);
				fclose(f);
			}
			else
				fprintf(stderr, "Can't open template file %d\n", errno);
#endif
			if (data != nullptr && (x != printerWidth || comp != STBI_rgb_alpha))
			{
				ERROR_LOG(NAOMI, "Invalid printer template: width %d comp %d", x, comp);
				stbi_image_free(data);
				data = nullptr;
			}
			if (data != nullptr)
			{
				if (lines > y)
				{
					u8 *newData = (u8 *)malloc(printerWidth * 4 * lines);
					const u8 *end = newData + printerWidth * 4 * lines;
					for (u8 *p = newData; p < end; p += printerWidth * 4 * y)
						memcpy(p, data, std::min(printerWidth * 4 * y, (int)(end - p)));
					stbi_image_free(data);
					data = newData;
				}
				u32 *p = (u32 *)data;
				for (u8 b : page)
				{
					if (b == 0xff)
						*p = 0xff000000;
					p++;
				}
				std::vector<u8> pngData;
				stbi_write_png_to_func(appendData, &pngData, printerWidth, lines, 4, data, printerWidth * 4);
				stbi_image_free(data);
				try {
					hostfs::saveScreenshot(filename, pngData);
				} catch (const FlycastException& e) {
					ERROR_LOG(NAOMI, "Error saving print out: %s", e.what());
					return false;
				}

				return true;
			}
		}
		for (u8& b : page)
			b = 0xff - b;
		std::vector<u8> pngData;
		stbi_write_png_to_func(appendData, &pngData, printerWidth, lines, 1, &page[0], printerWidth);
		try {
			hostfs::saveScreenshot(filename, pngData);
		} catch (const FlycastException& e) {
			ERROR_LOG(NAOMI, "Error saving print out: %s", e.what());
			return false;
		}
		return true;
	}

	void serialize(Serializer& ser)
	{
		ser << printerWidth;
		ser << (u32)page.size();
		ser.serialize(page.data(), page.size());
		ser << lines;
		ser << penx;
		ser << peny;
		ser << vspace;
		ser << hspace;
		ser << bigFont;
		ser << reversed;
		ser << doubleWidth;
		ser << doubleHeight;
		ser << msbBitmap;
		ser << maxLineHeight;
		ser << xorMode;
		ser << _advancePenOnBitmap;
		ser << printBufferEmpty;

		ser << customCharsEnabled;

		ser << (u32)customChars.size();
		for (const CustomChar& cc : customChars)
		{
			ser << cc.width;
			ser << cc.height;
			ser << (u32)cc.data.size();
			ser.serialize(cc.data.data(), cc.data.size());
		}

		ser << (u32)ruledLine.size();
		ser.serialize(ruledLine.data(), ruledLine.size());
		ser << ruledLineMode;
		ser << underline;
		ser << maxUnderline;
	}
	void deserialize(Deserializer& deser)
	{
		deser >> printerWidth;
		u32 size;
		deser >> size;
		page.resize(size);
		deser.deserialize(page.data(), page.size());
		deser >> lines;
		deser >> penx;
		deser >> peny;
		deser >> vspace;
		deser >> hspace;
		deser >> bigFont;
		deser >> reversed;
		deser >> doubleWidth;
		deser >> doubleHeight;
		deser >> msbBitmap;
		deser >> maxLineHeight;
		deser >> xorMode;
		deser >> _advancePenOnBitmap;
		deser >> printBufferEmpty;

		deser >> customCharsEnabled;

		deser >> size;
		customChars.resize(size);
		for (CustomChar& cc : customChars)
		{
			deser >> cc.width;
			deser >> cc.height;
			deser >> size;
			cc.data.resize(size);
			deser.deserialize(cc.data.data(), cc.data.size());
		}

		deser >> size;
		ruledLine.resize(size);
		deser.deserialize(ruledLine.data(), ruledLine.size());
		deser >> ruledLineMode;
		deser >> underline;
		deser >> maxUnderline;
	}

private:
	void linefeed()
	{
		if (maxLineHeight == 0)
		{
			const int height = bigFont ? 24 : 16;
			const int hScale = 1 + doubleHeight;
			maxLineHeight = height * hScale;
		}
		penx = 0;
		const int fromY = peny;
		peny += maxLineHeight + maxUnderline + vspace;
		maxUnderline = 0;
		maxLineHeight = 0;
		printBufferEmpty = true;
		if (ruledLineMode)
		{
			getPenPosition(0);
			for (int y = fromY; y < peny; y++)
			{
				u8 *pen = &page[y * printerWidth];
				for (int x = 0; x < printerWidth && x < (int)ruledLine.size(); x++)
				{
					if (ruledLine[x] != 0)
					{
						if (xorMode)
							*pen ^= 0xff;
						else
							*pen |= 0xff;
					}
					pen++;
				}
			}
		}
	}

	const u8 *getGlyph(char c)
	{
		const u8 *glyph;
		int glyphSize;
		if (bigFont)
		{
			glyph = ascii12x24.get();
			glyphSize = 2 * 24;
		}
		else
		{
			glyph = ascii8x16.get();
			glyphSize = 16;
		}
		if ((uint8_t)c < ' ')
			return glyph;
		else
			return glyph + glyphSize * ((uint8_t)c - ' ');
	}

	const u8 *getGlyph(wchar_t c)
	{
		const u8 *glyph;
		int glyphSize;
		if (bigFont)
		{
			glyph = kanji24x24.get();
			glyphSize = 3 * 24;
		}
		else
		{
			glyph = kanji16x16.get();
			glyphSize = 2 * 16;
		}
		if (c == ' ')
			return glyph;
		uint8_t plane = c >> 8;
		c &= 0xff;
		if (plane < 0x21 || plane > 0x7e || c < 0x21 || c > 0x7e)
			return glyph;
		else
			return glyph + glyphSize * (1 + (plane - 0x21) * 94 + (c - 0x21));
	}

	u8 *getPenPosition(int height)
	{
		if (peny + height > lines)
			addLines(peny + height - lines);
		return &page[peny * printerWidth + penx];
	}

	void addLines(int count)
	{
		lines += count;
		page.insert(page.end(), printerWidth * count, 0);
	}

	std::unique_ptr<u8[]> loadFont(const char *fname)
	{
		FILE *f = fopen(fname, "rb");
		if (!f) {
			perror(fname);
			return nullptr;
		}
		fseek(f, 0, SEEK_END);
		size_t sz = ftell(f);
		fseek(f, 0, SEEK_SET);
		std::unique_ptr<u8[]> data = std::make_unique<u8[]>(sz);
		fread(data.get(), sz, 1, f);
		fclose(f);
		return data;
	}

	int printerWidth = 920;
	std::vector<u8> page;
	int lines = 0;
	int penx = 0;
	int peny = 0;
	int vspace = 28;
	int hspace = 2;
	bool bigFont = false;
	bool reversed = false;
	bool doubleWidth = false;
	bool doubleHeight = false;
	bool msbBitmap = false;
	int maxLineHeight = 0;
	bool xorMode = false;
	bool _advancePenOnBitmap = false;
	bool printBufferEmpty = false;

	bool customCharsEnabled = false;
	struct CustomChar
	{
		int width;
		int height;
		std::vector<u8> data;
	};
	std::vector<CustomChar> customChars;
	int maxCharWidth = 48;	// depends on the printer type? 48 for tduno, 64 for ntvmys

	std::vector<u8> ruledLine;
	bool ruledLineMode = false;
	int underline = 0;
	int maxUnderline = 0;

	std::unique_ptr<u8[]> ascii8x16;
	std::unique_ptr<u8[]> ascii12x24;
	std::unique_ptr<u8[]> kanji16x16;
	std::unique_ptr<u8[]> kanji24x24;
};

//
// Nichipri NP413-FA esc/pos printer emulation
// Nichipri Industrial is now Nippon Primex
//
class ThermalPrinter
{
public:
	void setMaxCharWidth(int width) {
		getWriter().setMaxCharWidth(width);
	}

	void print(char c)
	{
		if (expectedDataBytes > 0)
		{
			dataBytes.push_back(c);
			if (expectedDataBytes == dataBytes.size())
			{
				switch (state)
				{
				case ESC:
					executeEscCommand();
					break;
				case DC2:
					executeDc2Command();
					break;
				case DC3:
					executeDc3Command();
					break;
				default:
					assert(false);
					break;
				}
				if (expectedDataBytes == dataBytes.size())
				{
					expectedDataBytes = 0;
					if (!dc3Lock || state != DC3)
						state = Default;
					dataBytes.clear();
				}
			}
		}
		else
		{
			switch (state)
			{
			case ESC:
				escCommand(c);
				break;

			case DC2:
				dc2Command(c);
				break;

			case DC3:
				dc3Command(c);
				break;

			default:
				switch (c)
				{
				case 0x1b: // ESC
					state = ESC;
					break;
				case 0x12: // DC2
					state = DC2;
					break;
				case 0x13: // DC3
					state = DC3;
					break;
				case '\r':
				case '\n':
					//printf("\\r\n");
					getWriter().print('\r');
					break;
				case 0x18: // CAN Erase print buffer
				case '\0':
					// ignore
					break;
				default:
					if (kanji)
					{
						if (kanjiByte0 == 0)
						{
							if (c <= ' ')
								getWriter().print(c);
							else
								kanjiByte0 = c;
						}
						else
						{
							wchar_t code = ((u8)kanjiByte0 << 8) | (u8)c;
							//printf("[%x]", code); fflush(stdout);
							getWriter().print(code);
							kanjiByte0 = 0;
						}
					}
					else
					{
						//if (c >= ' ')
						//	printf("%c", c);
						//else
						//	printf("[%02x]", (u8)c);
						getWriter().print(c);
					}
					break;
				}
			}
		}
	}

	void serialize(Serializer& ser)
	{
		ser << state;
		ser << dc3Lock;
		ser << curCommand;
		ser << expectedDataBytes;

		ser << (u32)dataBytes.size();
		ser.serialize(dataBytes.data(), dataBytes.size());

		ser << kanji;
		ser << kanjiByte0;

		ser << (u32)bitmaps.size();
		for (const Bitmap& bm : bitmaps)
		{
			ser << bm.width;
			ser << bm.height;
			ser << bm.msb;
			ser << (u32)bm.data.size();
			ser.serialize(bm.data.data(), bm.data.size());
		}

		if (bitmapWriter == nullptr)
			ser << false;
		else
		{
			ser << true;
			bitmapWriter->serialize(ser);
		}
	}

	void deserialize(Deserializer& deser)
	{
		deser >> state;
		deser >> dc3Lock;
		deser >> curCommand;
		deser >> expectedDataBytes;

		u32 size;
		deser >> size;
		dataBytes.resize(size);
		deser.deserialize(dataBytes.data(), dataBytes.size());

		deser >> kanji;
		deser >> kanjiByte0;

		deser >> size;
		bitmaps.resize(size);
		for (Bitmap& bm : bitmaps)
		{
			deser >> bm.width;
			deser >> bm.height;
			if (deser.version() >= Deserializer::V48)
				deser >> bm.msb;
			else
				bm.msb = false;
			deser >> size;
			bm.data.resize(size);
			deser.deserialize(bm.data.data(), bm.data.size());
		}

		bool b;
		deser >> b;
		if (!b)
			bitmapWriter.reset();
		else
			getWriter().deserialize(deser);
	}

private:
	void escCommand(char c)
	{
		curCommand = c;
		switch (c)
		{
		case '3': // smallest LF pitch set (same as A)
		case 'A': // line space setting (n dots)
		case 'J': // smallest pitch line feed
		case 'j': // Reverse paper feed after printing (n dots)
		case ' ': // Character right space set (n dots)
		case 'I': // Reversed b/w character on/off
		case 'W': // set/reset double-width chars
		case 'w': // set/reset double-height chars
		case '=': // Image LSB/MSB selection
		case '#': // Overlay mode selection (0: OR, 1: XOR)
		case '-': // Set/reset underline
			expectedDataBytes = 1;
			break;
		case 'i': // Full cut
			//printf("<<<full cut>>>\n");
			state = Default;
			if (bitmapWriter && bitmapWriter->isDirty())
			{
				std::string date = timeToISO8601(time(nullptr));
				std::replace(date.begin(), date.end(), '/', '-');
				std::replace(date.begin(), date.end(), ':', '-');
				std::string s = settings.content.gameId + " - " + date + ".png";
				const bool success = bitmapWriter->save(s);
				bitmapWriter.reset();
				if (success) {
					os_notify("Print out saved", 5000, s.c_str());
					NOTICE_LOG(NAOMI, "%s", s.c_str());
				}
			}
			break;
		case 'K': // Set Kanji mode
			kanji = true;
			state = Default;
			break;
		case 'H': // Cancel Kanji mode
			kanji = false;
			state = Default;
			break;
		case '2':
			getWriter().setVSpace(16);
			state = Default;
			break;
		case 'E': // set/reset Emphasized Characters FIXME doesn't seem to take a param in tduno
		case 'F': // Undocumented??
			state = Default;
			break;
		default:
			// unhandled but need to ignore data...
			INFO_LOG(NAOMI, "Unhandled ESC [%c]\n", c);
			state = Default;
			break;
		}
	}

	void executeEscCommand()
	{
		switch (curCommand)
		{
		case '3':
		case 'A':
			getWriter().setVSpace((u8)dataBytes[0]);
			break;
		case 'I':
			getWriter().setReversed(dataBytes[0] & 1);
			break;
		case 'J':
			getWriter().linefeed((u8)dataBytes[0]);
			break;
		case ' ':
			getWriter().setHSpace((u8)dataBytes[0] & 0x7f);
			break;
		case 'W':
			getWriter().setDoubleWidth(dataBytes[0] & 1);
			break;
		case 'w':
			getWriter().setDoubleHeight(dataBytes[0] & 1);
			break;
		case '-':
			getWriter().setUnderline(dataBytes[0] & 7);
			break;
		case '=':
			getWriter().setBitmapMSB((dataBytes[0] & 1) != 0);
			break;
		case '#':
			getWriter().setXorMode(dataBytes[0] & 1);
			break;
		case 'j':
			getWriter().linefeed(-(u8)dataBytes[0]);
			break;
		default:
			//printf("ESC %c", curCommand);
			//for (auto c : dataBytes)
			//	printf(" %d", (u8)c);
			//printf("\n");
			break;
		}
	}

	void dc2Command(char c)
	{
		curCommand = c;
		switch (c)
		{
		case 'R':	// Read memory switch (should take a param but apparently doesn't ?)
		case 'V':	// Raster bit image printing (w h [bytes ...]) FIXME tduno: w=0, h=0 and no data (?) or no param at all, f355: 1 param only (0)
			state = Default;
			break;
		case 'D':	// Allocate/free download char area
		case 'F':	// Font size selection. 0: Font B (8×16, 16×16), 1: Font A (12×24, 24×24)
		case 'G':	// Allocate/free external char area
		case 'U':	// Deletes the bitmap specified by n and releases the memory area used
		case 'p':	// Select out-of-paper error ??? probably not
		case '~':	// Set print density setting (n% with n in [50, 200])
		case 'O':	// set/reset optional font printing
			expectedDataBytes = 1;
			break;
		case 'S':	// Select a bitmap (n1) and specify the print position in the horizontal direction (n2 * 8 dots)
			expectedDataBytes = 2;
			break;
		case 'T':	// Create bitmap: n w yl yh [data...] where n is the bitmap id [0, 255], w is the width in bytes [1, 127],
					// yl + yh * 256 is the number of lines [1, 1023]
			expectedDataBytes = 4;
			break;
		case 'm':	// Mark position detection: s nl nh where n is the max feed amount until detection [0, 65535]
					// s & 3 == 0: Feed the paper in the forward direction until it passes the marking position.
					// s & 3 == 1: Feed the paper in the forward direction to the marking position.
					// s & 3 == 2: Feed the paper in the opposite direction until it passes the marking position.
					// s & 3 == 3: Feed the paper in the opposite direction to the marking position.
			expectedDataBytes = 3;
			break;
		case 'P':	// Register Optional Font?
			expectedDataBytes = 4;
			break;
		default:
			// unhandled but need to ignore data...
			INFO_LOG(NAOMI, "Unhandled DC2 [%c]\n", c);
			state = Default;
			break;
		}
	}

	void executeDc2Command()
	{
		switch (curCommand)
		{
		case 'F':
			getWriter().selectFont(dataBytes[0] & 1);
			break;
		case 'O':
			getWriter().enableCustomChars(dataBytes[0] & 1);
			break;
		case 'P':
			// Register optional font: s e y x [data...] k
			// s: start char code [20, 7e]
			// e: end char code [20, 7e]
			// y: vertical dots [1, 7f]
			// x: horizontal dots [1, 7f]
			// d: data bytes
			// k: data bytes count = INT((y + 7) / 8) × x × (e - s + 1)
			if (expectedDataBytes == 4)
			{
				expectedDataBytes += ((dataBytes[2] & 0x7f) + 7) / 8 * (dataBytes[3] & 0x7f) * ((u8)dataBytes[1] - (u8)dataBytes[0] + 1);
			}
			else
			{
				const char s = dataBytes[0] & 0x7f;
				const char e = dataBytes[1] & 0x7f;
				const int y = dataBytes[2] & 0x7f;
				const int x = dataBytes[3] & 0x7f;
				const u8 *p = (u8 *)&dataBytes[4];
				const int charSize = (x + 7) / 8 * y;
				for (char c = s; c <= e; c++)
				{
					getWriter().setCustomChar(c, x, y, p);
					p += charSize;
				}
				//printf("Characters %c to %c: %d x %d\n", s, e, x, y);
			}
			break;
		case 'S':
			{
				const size_t idx = (u8)dataBytes[0];
				const int hpos = (u8)dataBytes[1] * 8;
				if (idx < bitmaps.size())
					getWriter().printImage(hpos, bitmaps[idx].width, bitmaps[idx].height, &bitmaps[idx].data[0], bitmaps[idx].msb);
			}
			break;
		case 'T':
			{
				// bitmap
				if (expectedDataBytes == 4)
				{
					expectedDataBytes += ((u8)dataBytes[1] & 0x7f) * std::min((u8)dataBytes[2] | ((u8)dataBytes[3] << 8), 1023);
				}
				else
				{
					const size_t idx = (u8)dataBytes[0];
					const int w = (dataBytes[1] & 0x7f) * 8;
					const int h = std::min((u8)dataBytes[2] | ((u8)dataBytes[3] << 8), 1023);
					//printf("bitmap[%zd] %d x %d\n", idx, w, h);
					// B&W 1bit
					std::vector<u8> bitmap(w / 8 * h);
					memcpy(bitmap.data(), &dataBytes[4], bitmap.size());
					if (bitmaps.size() <= idx)
						bitmaps.resize(idx + 1);
					bitmaps[idx].width = w;
					bitmaps[idx].height = h;
					bitmaps[idx].msb = getWriter().getBitmapMSB();
					std::swap(bitmaps[idx].data, bitmap);
				}
			}
			break;
		case 'U':
			{
				const u32 idx = (u8)dataBytes[0];
				if (idx < bitmaps.size())
				{
					bitmaps[idx].width = bitmaps[idx].height = 0;
					bitmaps[idx].data.clear();
				}
			}
			break;
		case 'p':
			// This very unlikely related to bitmaps but is used to distinguish between f355 and tduno[2] handling of bitmaps.
			if (dataBytes[0] != 0)
				getWriter().advancePenOnBitmap();
			break;
		default:
			//printf("DC2 %c", curCommand);
			//for (auto c : dataBytes)
			//	printf(" %d", (u8)c);
			//printf("\n");
			break;
		}
	}
	void dc3Command(char c)
	{
		curCommand = c;
		switch (c)
		{
		case '(':
			dc3Lock = true;
			break;
		case ')':
			dc3Lock = false;
			state = Default;
			break;
		case '+': // Enable ruled line buffer printing
			if (!dc3Lock)
				state = Default;
			getWriter().setRuledLineMode(true);
			break;
		case '-': // Disable ruled line buffer printing
			if (!dc3Lock)
				state = Default;
			getWriter().setRuledLineMode(false);
			break;
		case 'A': // Select Ruled line buffer A
		case 'B': // Select Ruled line buffer B
			if (!dc3Lock)
				state = Default;
			break;
		case 'C': // Clear rule buffer
			if (!dc3Lock)
				state = Default;
			getWriter().clearRuledLine();
			break;
		case 'D': // Dot fill. Writes "1" (black) at dot position specified by (nh * 256 + nl) * dotpitch in the selected ruled line buffer.
				  // nl[0-255] nh[0-3]
			expectedDataBytes = 2;
			break;
		case 'F': // Pattern fill. Writes one line to the selected ruled line buffer using the data pattern specified by n1 and n2.
				  // pattern is arranged horizontally from left to right. (Each byte obey image lsb/msb selection ESC =)
			expectedDataBytes = 2;
			break;
		case 'P': // Prints the data in the print buffer and prints 1 line of the ruled line buffer
			if (!dc3Lock)
				state = Default;
			getWriter().printRuledLine();
			break;
		case 'L': // Writes "1" (black) in the range from nhnl to mhml in the selected ruled line buffer.
				  // ml mh nl nh
			expectedDataBytes = 4;
			break;
		default:
			// unhandled but need to ignore data...
			INFO_LOG(NAOMI, "Unhandled DC3 [%c]\n", c);
			if (!dc3Lock)
				state = Default;
			break;
		}
	}

	void executeDc3Command()
	{
		switch (curCommand)
		{
		case 'D':
			getWriter().drawRuledLine((u8)dataBytes[0] + (u8)dataBytes[1]  * 256, (u8)dataBytes[0] + (u8)dataBytes[1]  * 256);
			break;
		case 'F':
			getWriter().drawRuledPattern((u8)dataBytes[0], (u8)dataBytes[1]);
			break;
		case 'L':
			getWriter().drawRuledLine((u8)dataBytes[0] + (u8)dataBytes[1]  * 256, (u8)dataBytes[2] + (u8)dataBytes[3] * 256);
			break;
		default:
			//printf("DC3 %c", curCommand);
			//for (auto c : dataBytes)
			//	printf(" %d", (u8)c);
			//printf("\n");
			break;
		}
	}

	enum { Default, ESC, DC2, DC3 } state = Default;
	bool dc3Lock = false;
	char curCommand = 0;
	unsigned expectedDataBytes = 0;
	std::vector<char> dataBytes;

	bool kanji = false;
	char kanjiByte0 = 0;

	struct Bitmap
	{
		int width;
		int height;
		bool msb;
		std::vector<u8> data;
	};
	std::vector<Bitmap> bitmaps;

	BitmapWriter& getWriter()
	{
		if (bitmapWriter == nullptr)
			bitmapWriter = std::make_unique<BitmapWriter>(832); // tduno test print: min 894, but uses 832 for rules lines
																// tduno2 test print ok with 832
																// f355 ok with 832
		return *bitmapWriter;
	}
	std::unique_ptr<BitmapWriter> bitmapWriter;
};

static std::unique_ptr<ThermalPrinter> printer;

void init()
{
	printer = std::make_unique<ThermalPrinter>();
	if (settings.content.gameId == "MIRAI YOSOU STUDIO")
		printer->setMaxCharWidth(64);
}

void term()
{
	printer.reset();
}

void print(char c)
{
	if (printer != nullptr)
		printer->print(c);
}

#ifndef STANDALONE_TEST
void serialize(Serializer& ser)
{
	if (printer != nullptr)
		printer->serialize(ser);
}

void deserialize(Deserializer& deser)
{
	if (printer != nullptr)
	{
		 if (deser.version() >= Deserializer::V35)
			 printer->deserialize(deser);
		 else
			 init();
	}
}
#endif

}

#ifdef STANDALONE_TEST
settings_t settings;

std::string get_writable_data_path(const std::string& s)
{
	return "./" + s;
}

void os_notify(char const*, int, char const*) {
}

int main(int argc, char *argv[])
{
	if (argc < 2)
		return 1;
	FILE *f = fopen(argv[1], "rb");
	if (f == nullptr) {
		perror(argv[1]);
		return 1;
	}
	settings.content.gameId = "F355 CHALLENGE";
	printer::ThermalPrinter printer;
//	printer.setMaxCharWidth(64);
	for (;;)
	{
		int c = fgetc(f);
		if (c == EOF)
			break;
		printer.print((char)c);
	}
	fclose(f);

	return 0;
}
#endif
