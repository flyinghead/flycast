/*
 Copyright 2026 flyinghead
 
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
#include "gui_font.h"
#include "gui.h"
#include "settings.h"
#include "oslib/resources.h"
#include "IconsFontAwesome6.h"
#include "imgui/misc/freetype/imgui_freetype.h"

ImFont *boldFont;

namespace FontFace
{
enum class Noto : ImU32 { Thin = 0x0, Light = 0x20000, DemiLight = 0x30000, Regular = 0x40000, Medium = 0x50000, Bold = 0x60000, Black = 0x70000 };
enum class PingFang : ImU32 { Ultralight = 15, Thin = 12, Light = 9, Regular = 0, Medium = 3, Semibold = 6 };
enum class GothicNeo : ImU32 { UltraLight = 12, Thin = 10, Light = 8, Regular = 0, Medium = 2, SemiBold = 4, ExtraBold = 14, Heavy = 16 };
enum class Gulim : ImU32 { Round = 0, RoundMono = 1, Sharp = 2, SharpMono = 3 };
enum class MSHei : ImU32 { Legacy = 0, UI = 1};
};

struct FontNo
{
	ImU32 value = 0;
	
	constexpr FontNo() = default;
	constexpr FontNo(ImU32 v) : value(v) {}
	
	template <typename E, typename = std::enable_if_t<std::is_enum<E>::value>>
	constexpr FontNo(E e) : value(static_cast<ImU32>(e)) {}
};

struct FontEntry
{
	const char* lang;		// "ja", "ko", "zh", "zh_HK", "zh_TW", "zh_CN", "cjk" or nullptr for "generic"
	std::vector<std::string> paths;
#if defined(__linux__) && !defined(__ANDROID__)
	std::vector<std::string> fcNames;
#endif
	FontNo fontNo{};
	float size = 1.0f;
	float offsetY = 0.0f;
};

#if defined(__linux__) && !defined(__ANDROID__)
#include <dlfcn.h>

static std::string getFontPath(const char* patternText)
{
	static void* libfontconfig = dlopen("libfontconfig.so.1", RTLD_LAZY);
	if (!libfontconfig)
	{
		return "";
	}
	
	typedef struct _FcConfig FcConfig;
	typedef struct _FcPattern FcPattern;
	typedef unsigned char FcChar8;
	typedef enum _FcResult
	{
		FcResultMatch,
		FcResultNoMatch,
		FcResultTypeMismatch,
		FcResultNoId,
		FcResultOutOfMemory
	} FcResult;
	
	static auto fcInitLoadConfigAndFonts = (FcConfig * (*)())dlsym(libfontconfig, "FcInitLoadConfigAndFonts");
	static auto fcConfigDestroy = (void (*)(FcConfig*))dlsym(libfontconfig, "FcConfigDestroy");
	static auto fcPatternDestroy = (void (*)(FcPattern*))dlsym(libfontconfig, "FcPatternDestroy");
	static auto fcPatternGetString = (FcResult (*)(const FcPattern*, const char*, int, FcChar8**))dlsym(libfontconfig, "FcPatternGetString");
	static auto fcNameParse = (FcPattern * (*)(const FcChar8*))dlsym(libfontconfig, "FcNameParse");
	static auto fcConfigSubstitute = (int (*)(FcConfig*, FcPattern*, int))dlsym(libfontconfig, "FcConfigSubstitute");
	static auto fcDefaultSubstitute = (void (*)(FcPattern*))dlsym(libfontconfig, "FcDefaultSubstitute");
	static auto fcFontMatch = (FcPattern * (*)(FcConfig*, FcPattern*, FcResult*))dlsym(libfontconfig, "FcFontMatch");
	
	if (!fcInitLoadConfigAndFonts ||
		!fcConfigDestroy ||
		!fcPatternDestroy ||
		!fcPatternGetString ||
		!fcNameParse ||
		!fcConfigSubstitute ||
		!fcDefaultSubstitute ||
		!fcFontMatch)
	{
		return "";
	}
	
	const int fcMatchPattern = 0;
	
	std::string path;
	FcConfig* config = fcInitLoadConfigAndFonts();
	if (config)
	{
		FcPattern* pattern = fcNameParse(reinterpret_cast<const FcChar8*>(patternText));
		if (pattern)
		{
			fcConfigSubstitute(config, pattern, fcMatchPattern);
			fcDefaultSubstitute(pattern);
			
			FcResult result = FcResultNoMatch;
			FcPattern* match = fcFontMatch(config, pattern, &result);
			if (match)
			{
				if (result == FcResultMatch)
				{
					FcChar8* file = nullptr;
					if (fcPatternGetString(match, "file", 0, &file) == FcResultMatch && file)
					{
						path = reinterpret_cast<const char*>(file);
					}
				}
				
				fcPatternDestroy(match);
			}
			
			fcPatternDestroy(pattern);
		}
		
		fcConfigDestroy(config);
	}
	
	return path;
}
#endif

static void registerFont(std::vector<FontEntry>& target, FontEntry entry)
{
#if defined(__linux__) && !defined(__ANDROID__)
	for (const std::string& name : entry.fcNames)
	{
		std::string path = getFontPath(name.c_str());
		if (!path.empty())
			entry.paths.push_back(path);
	}
#endif
	
	std::string locale = i18n::getCurrentLocale();
	
	auto bucketRank = [&](const char* l) -> int
	{
		if (!l) return 3;                      // generic last
		if (strcmp(l, "cjk") == 0) return 0;   // cjk first
		if (locale.rfind(l, 0) == 0) return 1; // user locale
		return 2;                              // remaining langs
	};
	
	const int rank = bucketRank(entry.lang);
	auto it = target.begin();
	for (; it != target.end(); ++it)
	{
		if (bucketRank(it->lang) > rank)
			break;
	}
	target.insert(it, entry);
}

static void registerFont(std::initializer_list<std::vector<FontEntry>*> targets, const FontEntry& entry)
{
	for (auto* target : targets)
		registerFont(*target, entry);
}

static void loadFonts(const std::vector<FontEntry>& entries, const ImFontConfig& cfg)
{
	std::unordered_set<std::string> satisfiedLangs;
	satisfiedLangs.reserve(8);
	
	bool cjkLoaded = false;
	
	for (const FontEntry& e : entries)
	{
		const char* lang = e.lang ? e.lang : "generic";
		
		// Skip specific CJK langs if a generic CJK font already loaded
		if (cjkLoaded && (!strcmp(lang, "ja") || !strcmp(lang, "ko") || !strncmp(lang, "zh", 2)))
			continue;
		
		// Break-per-langTag
		if (satisfiedLangs.count(lang))
			continue;
		
		bool loaded = false;
		
		for (const std::string& path : e.paths)
		{
			ImFontConfig entryCfg = cfg;
			entryCfg.FontNo = e.fontNo.value;
			entryCfg.GlyphOffset.y = e.size * e.offsetY;
			static ImWchar faRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
			entryCfg.GlyphExcludeRanges = faRanges;
			
			ImFont* font = ImGui::GetIO().Fonts->AddFontFromFileTTF(path.c_str(), e.size, &entryCfg, nullptr);
			if (font)
			{
				loaded = true;
				break;
			}
		}
		
		if (!loaded)
			continue;
		
		// Mark this lang as satisfied
		satisfiedLangs.emplace(lang);
		
		// Traditional Chinese group: TW and HK satisfy each other
		if (strcmp(lang, "zh_HK") == 0 || strcmp(lang, "zh_TW") == 0)
		{
			satisfiedLangs.emplace("zh_HK");
			satisfiedLangs.emplace("zh_TW");
		}
		
		// Umbrella rule: zh satisfies all specific Chinese locales
		if (strcmp(lang, "zh") == 0)
		{
			satisfiedLangs.emplace("zh_HK");
			satisfiedLangs.emplace("zh_TW");
			satisfiedLangs.emplace("zh_CN");
		}
		
		// Special rule: cjk satisfies all CJK languages
		if (strcmp(lang, "cjk") == 0)
		{
			cjkLoaded = true;
			satisfiedLangs.emplace("ja");
			satisfiedLangs.emplace("ko");
			satisfiedLangs.emplace("zh");
			satisfiedLangs.emplace("zh_HK");
			satisfiedLangs.emplace("zh_TW");
			satisfiedLangs.emplace("zh_CN");
		}
	}
}

void gui_loadFonts()
{
	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->Clear();
	io.Fonts->SetFontLoader(ImGuiFreeType::GetFontLoader());
	
	// Regular font
	ImGuiStyle& style = ImGui::GetStyle();
	const float fontSize = uiScaled(17.f);
	style.FontSizeBase = fontSize;
	
	size_t dataSize;
	std::unique_ptr<u8[]> data = resource::load("fonts/Roboto-Medium.ttf", dataSize);
	verify(data != nullptr);
	ImFont *regularFont = io.Fonts->AddFontFromMemoryTTF(data.release(), (int)dataSize, fontSize, nullptr, nullptr);
	ImFontConfig fontConfig;
	fontConfig.MergeMode = true;
	fontConfig.DstFont = regularFont;
	
	// Bold font
	data = resource::load("fonts/Roboto-Bold.ttf", dataSize);
	verify(data != nullptr);
	boldFont = io.Fonts->AddFontFromMemoryTTF(data.release(), (int)dataSize, fontSize, nullptr, nullptr);
	ImFontConfig boldFontConfig;
	boldFontConfig.MergeMode = true;
	boldFontConfig.DstFont = boldFont;
	
	std::vector<FontEntry> fonts;
	std::vector<FontEntry> boldFonts;
	fontConfig.Flags |= ImFontFlags_NoLoadError;
	boldFontConfig.Flags |= ImFontFlags_NoLoadError;
	
	ImFontConfig emojiConfig = fontConfig;
	emojiConfig.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_LoadColor | ImGuiFreeTypeBuilderFlags_Bitmap;
	ImFontConfig emojiBoldConfig = boldFontConfig;
	emojiBoldConfig.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_LoadColor | ImGuiFreeTypeBuilderFlags_Bitmap;
	
#ifdef _WIN32
	// Windows
	std::string fontDir = std::string(nowide::getenv("SYSTEMROOT")) + "\\Fonts\\";
	
	ImGui::GetIO().Fonts->AddFontFromFileTTF((fontDir + "seguiemj.ttf").c_str(), fontSize, &emojiConfig);
	ImGui::GetIO().Fonts->AddFontFromFileTTF((fontDir + "seguiemj.ttf").c_str(), fontSize, &emojiBoldConfig);
	
	/*
	registerFont(fonts,     { .lang="ja", .paths={fontDir + "NotoSansJP-VF.ttf"}, .fontNo=FontFace::Noto::Medium, .size=fontSize * 1.05f });
	registerFont(boldFonts, { .lang="ja", .paths={fontDir + "NotoSansJP-VF.ttf"}, .fontNo=FontFace::Noto::Bold,   .size=fontSize * 1.05f });
	registerFont({&fonts, &boldFonts}, { .lang="ja", .paths={fontDir + "BIZ-UDGothicB.ttc"}, .size=fontSize * 0.85f });
	
	registerFont(fonts,     { .lang="ko", .paths={fontDir + "NotoSansKR-VF.ttf"}, .fontNo=FontFace::Noto::Medium, .size=fontSize * 1.05f });
	registerFont(boldFonts, { .lang="ko", .paths={fontDir + "NotoSansKR-VF.ttf"}, .fontNo=FontFace::Noto::Bold,   .size=fontSize * 1.05f });
	registerFont({&fonts, &boldFonts}, { .lang="ko", .paths={fontDir + "malgunbd.ttf"}, .size=fontSize * 1.124891f });
	registerFont({&fonts, &boldFonts}, { .lang="ko", .paths={fontDir + "Gulim.ttc"}, .fontNo=FontFace::Gulim::Sharp, .size=fontSize * 0.9f });
	
	registerFont(fonts,     { .lang="zh_HK", .paths={fontDir + "NotoSansHK-VF.ttf"}, .fontNo=FontFace::Noto::Medium, .size=fontSize * 1.05f });
	registerFont(boldFonts, { .lang="zh_HK", .paths={fontDir + "NotoSansHK-VF.ttf"}, .fontNo=FontFace::Noto::Bold,   .size=fontSize * 1.05f });
	registerFont(fonts,     { .lang="zh_TW", .paths={fontDir + "NotoSansTC-VF.ttf"}, .fontNo=FontFace::Noto::Medium, .size=fontSize * 1.05f });
	registerFont(boldFonts, { .lang="zh_TW", .paths={fontDir + "NotoSansTC-VF.ttf"}, .fontNo=FontFace::Noto::Bold,   .size=fontSize * 1.05f });
	registerFont(fonts,     { .lang="zh_CN", .paths={fontDir + "NotoSansSC-VF.ttf"}, .fontNo=FontFace::Noto::Medium, .size=fontSize * 1.05f });
	registerFont(boldFonts, { .lang="zh_CN", .paths={fontDir + "NotoSansSC-VF.ttf"}, .fontNo=FontFace::Noto::Bold,   .size=fontSize * 1.05f });
	
	registerFont({&fonts, &boldFonts}, { .lang="zh_HK", .paths={fontDir + "msjhbd.ttc"}, .fontNo=FontFace::MSHei::UI, .size=fontSize * 1.13000f, .offsetY=0.015f });
	registerFont({&fonts, &boldFonts}, { .lang="zh_TW", .paths={fontDir + "msjhbd.ttc"}, .fontNo=FontFace::MSHei::UI, .size=fontSize * 1.13000f, .offsetY=0.015f });
	registerFont({&fonts, &boldFonts}, { .lang="zh_CN", .paths={fontDir + "msyhbd.ttc"}, .fontNo=FontFace::MSHei::UI, .size=fontSize * 1.13000f, .offsetY=0.015f });
	
	// Windows 7
	registerFont({&fonts, &boldFonts}, { .lang="zh_HK", .paths={fontDir + "msjhbd.ttf"}, .size=fontSize * 1.13000f, .offsetY=0.015f });
	registerFont({&fonts, &boldFonts}, { .lang="zh_TW", .paths={fontDir + "msjhbd.ttf"}, .size=fontSize * 1.13000f, .offsetY=0.015f });
	registerFont({&fonts, &boldFonts}, { .lang="zh_CN", .paths={fontDir + "msyhbd.ttf"}, .size=fontSize * 1.13000f, .offsetY=0.015f });
	 */
	{
		FontEntry entry; entry.lang = "ja"; entry.paths = { fontDir + "NotoSansJP-VF.ttf" }; entry.fontNo = FontFace::Noto::Medium; entry.size = fontSize * 1.05f;
		registerFont(fonts, entry);
	}
	{
		FontEntry entry; entry.lang = "ja"; entry.paths = { fontDir + "NotoSansJP-VF.ttf" }; entry.fontNo = FontFace::Noto::Bold; entry.size = fontSize * 1.05f;
		registerFont(boldFonts, entry);
	}
	{
		FontEntry entry; entry.lang = "ja"; entry.paths = { fontDir + "BIZ-UDGothicB.ttc" }; entry.size = fontSize * 0.85f;
		registerFont({ &fonts, &boldFonts }, entry);
	}

	{
		FontEntry entry; entry.lang = "ko"; entry.paths = { fontDir + "NotoSansKR-VF.ttf" }; entry.fontNo = FontFace::Noto::Medium; entry.size = fontSize * 1.05f;
		registerFont(fonts, entry);
	}
	{
		FontEntry entry; entry.lang = "ko"; entry.paths = { fontDir + "NotoSansKR-VF.ttf" }; entry.fontNo = FontFace::Noto::Bold; entry.size = fontSize * 1.05f;
		registerFont(boldFonts, entry);
	}
	{
		FontEntry entry; entry.lang = "ko"; entry.paths = { fontDir + "malgunbd.ttf" }; entry.size = fontSize * 1.124891f;
		registerFont({ &fonts, &boldFonts }, entry);
	}
	{
		FontEntry entry; entry.lang = "ko"; entry.paths = { fontDir + "Gulim.ttc" }; entry.fontNo = FontFace::Gulim::Sharp; entry.size = fontSize * 0.9f;
		registerFont({ &fonts, &boldFonts }, entry);
	}

	{
		FontEntry entry; entry.lang = "zh_HK"; entry.paths = { fontDir + "NotoSansHK-VF.ttf" }; entry.fontNo = FontFace::Noto::Medium; entry.size = fontSize * 1.05f;
		registerFont(fonts, entry);
	}
	{
		FontEntry entry; entry.lang = "zh_HK"; entry.paths = { fontDir + "NotoSansHK-VF.ttf" }; entry.fontNo = FontFace::Noto::Bold; entry.size = fontSize * 1.05f;
		registerFont(boldFonts, entry);
	}
	{
		FontEntry entry; entry.lang = "zh_TW"; entry.paths = { fontDir + "NotoSansTC-VF.ttf" }; entry.fontNo = FontFace::Noto::Medium; entry.size = fontSize * 1.05f;
		registerFont(fonts, entry);
	}
	{
		FontEntry entry; entry.lang = "zh_TW"; entry.paths = { fontDir + "NotoSansTC-VF.ttf" }; entry.fontNo = FontFace::Noto::Bold; entry.size = fontSize * 1.05f;
		registerFont(boldFonts, entry);
	}
	{
		FontEntry entry; entry.lang = "zh_CN"; entry.paths = { fontDir + "NotoSansSC-VF.ttf" }; entry.fontNo = FontFace::Noto::Medium; entry.size = fontSize * 1.05f;
		registerFont(fonts, entry);
	}
	{
		FontEntry entry; entry.lang = "zh_CN"; entry.paths = { fontDir + "NotoSansSC-VF.ttf" }; entry.fontNo = FontFace::Noto::Bold; entry.size = fontSize * 1.05f;
		registerFont(boldFonts, entry);
	}

	{
		FontEntry entry; entry.lang = "zh_HK"; entry.paths = { fontDir + "msjhbd.ttc" }; entry.fontNo = FontFace::MSHei::UI; entry.size = fontSize * 1.13000f; entry.offsetY = 0.015f;
		registerFont({ &fonts, &boldFonts }, entry);
	}
	{
		FontEntry entry; entry.lang = "zh_TW"; entry.paths = { fontDir + "msjhbd.ttc" }; entry.fontNo = FontFace::MSHei::UI; entry.size = fontSize * 1.13000f; entry.offsetY = 0.015f;
		registerFont({ &fonts, &boldFonts }, entry);
	}
	{
		FontEntry entry; entry.lang = "zh_CN"; entry.paths = { fontDir + "msyhbd.ttc" }; entry.fontNo = FontFace::MSHei::UI; entry.size = fontSize * 1.13000f; entry.offsetY = 0.015f;
		registerFont({ &fonts, &boldFonts }, entry);
	}

	// Windows 7
	{
		FontEntry entry; entry.lang = "zh_HK"; entry.paths = { fontDir + "msjhbd.ttf" }; entry.size = fontSize * 1.13000f; entry.offsetY = 0.015f;
		registerFont({ &fonts, &boldFonts }, entry);
	}
	{
		FontEntry entry; entry.lang = "zh_TW"; entry.paths = { fontDir + "msjhbd.ttf" }; entry.size = fontSize * 1.13000f; entry.offsetY = 0.015f;
		registerFont({ &fonts, &boldFonts }, entry);
	}
	{
		FontEntry entry; entry.lang = "zh_CN"; entry.paths = { fontDir + "msyhbd.ttf" }; entry.size = fontSize * 1.13000f; entry.offsetY = 0.015f;
		registerFont({ &fonts, &boldFonts }, entry);
	}
	
#elif defined(TARGET_OS_MAC)
	emojiConfig.GlyphOffset = { 0.0f, fontSize * (0.2f) };
	emojiBoldConfig.GlyphOffset = emojiConfig.GlyphOffset;
	ImGui::GetIO().Fonts->AddFontFromFileTTF("/System/Library/Fonts/Apple Color Emoji.ttc", fontSize, &emojiConfig);
	ImGui::GetIO().Fonts->AddFontFromFileTTF("/System/Library/Fonts/Apple Color Emoji.ttc", fontSize, &emojiBoldConfig);
	
	registerFont(fonts,		{ .lang = "ja", .paths = { "/System/Library/Fonts/ヒラギノ角ゴシック W5.ttc" }, .size=fontSize * 0.85f } );
	registerFont(boldFonts,	{ .lang = "ja", .paths = { "/System/Library/Fonts/ヒラギノ角ゴシック W7.ttc" }, .size=fontSize * 0.85f } );
	
	registerFont({&fonts, &boldFonts}, { .lang="ko", .paths = { "/System/Library/Fonts/AppleSDGothicNeo.ttc" }, .fontNo=FontFace::GothicNeo::SemiBold, .size=fontSize * 1.05f });
	
	registerFont(fonts,		{ .lang="zh", .paths = { "/System/Library/Fonts/PingFang.ttc" }, .fontNo=FontFace::PingFang::Medium, .size=fontSize * 1.185f, .offsetY=-0.03f });
	registerFont(boldFonts,	{ .lang="zh", .paths = { "/System/Library/Fonts/PingFang.ttc" }, .fontNo=FontFace::PingFang::Semibold, .size=fontSize * 1.185f, .offsetY=-0.03f });
	
#elif defined(__ANDROID__)
	ImGui::GetIO().Fonts->AddFontFromFileTTF("/system/fonts/NotoColorEmoji.ttf", fontSize, &emojiConfig);
	ImGui::GetIO().Fonts->AddFontFromFileTTF("/system/fonts/NotoColorEmoji.ttf", fontSize, &emojiBoldConfig);
	
	registerFont(fonts,     { .lang="cjk", .paths={
		"/system/fonts/NotoSansCJK-Medium.ttc",
		"/system/fonts/NotoSansCJK-Regular.ttc"
	}, .size=fontSize * 1.05f });
	registerFont(boldFonts, { .lang="cjk", .paths={
		"/system/fonts/NotoSansCJK-Bold.ttc",
		"/system/fonts/NotoSansCJK-Medium.ttc",
		"/system/fonts/NotoSansCJK-Regular.ttc"
	}, .size=fontSize * 1.05f });
	
#elif defined(__linux__)
	std::string emojiPath = getFontPath("Noto Color Emoji");
	if (!emojiPath.empty())
	{
		ImGui::GetIO().Fonts->AddFontFromFileTTF(emojiPath.c_str(), fontSize, &emojiConfig);
		ImGui::GetIO().Fonts->AddFontFromFileTTF(emojiPath.c_str(), fontSize, &emojiBoldConfig);
	}
	
	registerFont(fonts,		{ .lang="cjk", .fcNames={ "Noto Sans CJK JP:medium", "Noto Sans CJK JP:regular" }, .size=fontSize * 1.05f });
	registerFont(boldFonts,	{ .lang="cjk", .fcNames={ "Noto Sans CJK JP:bold" }, .size=fontSize * 1.05f });
	registerFont({&fonts, &boldFonts}, { .lang="cjk", .fcNames={ "Droid Sans Fallback" }, .size=fontSize * 1.2f });
	
	registerFont(fonts,		{ .lang="ja", .fcNames={ "Source Han Sans JP:medium" }, .size=fontSize * 1.05f });
	registerFont(boldFonts,	{ .lang="ja", .fcNames={ "Source Han Sans JP:bold" }, .size=fontSize * 1.05f });
	registerFont({&fonts, &boldFonts}, { .lang="ja", .fcNames={ "IPAPGothic" }, .size=fontSize * 0.9f });
	registerFont({&fonts, &boldFonts}, { .lang="ja", .fcNames={ "VL Gothic" }, .size=fontSize * 1.15f });
	registerFont({&fonts, &boldFonts}, { .lang="ja", .fcNames={ "TakaoPGothic" }, .size=fontSize });
	
	registerFont(fonts,		{ .lang="ko", .fcNames={ "Source Han Sans KR:medium" }, .size=fontSize * 1.05f });
	registerFont(boldFonts,	{ .lang="ko", .fcNames={ "Source Han Sans KR:bold" }, .size=fontSize * 1.05f });
	
	registerFont(fonts,		{ .lang="ko", .fcNames={ "NanumGothic:bold" }, .size=fontSize * 0.8f });
	registerFont(boldFonts,	{ .lang="ko", .fcNames={ "NanumGothic:extrabold" }, .size=fontSize * 0.8f });
	registerFont({&fonts, &boldFonts}, { .lang="ko", .fcNames={ "NanumGothicCoding:bold" }, .size=fontSize * 0.8f });
	registerFont({&fonts, &boldFonts}, { .lang="ko", .fcNames={ "UnDotum:bold" }, .size=fontSize, .offsetY=-0.1f });
	registerFont({&fonts, &boldFonts}, { .lang="ko", .fcNames={ "Baekmuk Dotum" }, .size=fontSize * 1.18f, .offsetY=-0.02f });
	
	registerFont(fonts,		{ .lang="zh_HK", .fcNames={ "Source Han Sans HK:medium" }, .size=fontSize * 1.05f });
	registerFont(boldFonts,	{ .lang="zh_HK", .fcNames={ "Source Han Sans HK:bold" }, .size=fontSize * 1.05f });
	registerFont(fonts,		{ .lang="zh_TW", .fcNames={ "Source Han Sans TW:medium" }, .size=fontSize * 1.05f });
	registerFont(boldFonts,	{ .lang="zh_TW", .fcNames={ "Source Han Sans TW:bold" }, .size=fontSize * 1.05f });
	registerFont(fonts,		{ .lang="zh_CN", .fcNames={ "Source Han Sans SC:medium" }, .size=fontSize * 1.05f });
	registerFont(boldFonts,	{ .lang="zh_CN", .fcNames={ "Source Han Sans SC:bold" }, .size=fontSize * 1.05f });
	registerFont({&fonts, &boldFonts}, { .lang="zh", .fcNames={ "WenQuanYi Zen Hei" }, .size=fontSize, .offsetY=-0.05f });
	
	// TODO BSD, iOS, ...
#endif
	
	loadFonts(fonts, fontConfig);
	loadFonts(boldFonts, boldFontConfig);
	
	// Font Awesome symbols
	data = resource::load("fonts/" FONT_ICON_FILE_NAME_FAS, dataSize);
	verify(data != nullptr);
	
	ImFontConfig faFontConfig = fontConfig;
	faFontConfig.FontDataOwnedByAtlas = false;
	io.Fonts->AddFontFromMemoryTTF(data.get(), (int)dataSize, fontSize, &faFontConfig);
	boldFontConfig.FontDataOwnedByAtlas = true;
	io.Fonts->AddFontFromMemoryTTF(data.release(), (int)dataSize, fontSize, &boldFontConfig);
}
