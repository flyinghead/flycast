// Solution adapted from https://github.com/ocornut/imgui/issues/9066

#include "imgui_cjk.h"
#include "imgui_internal.h"
#include <array>
#include <vector>
#include <string>
#include <unordered_set>
#include <algorithm>
#include <cstdint>

// From imgui_draw.cpp
extern float BuildLoadGlyphGetAdvanceOrFallback(ImFontBaked* baked, unsigned int codepoint);

namespace {

// --- Bitmap helpers ---
template <size_t N>
struct Bitmap {
	std::array<uint8_t, N> bits{};

	void set(size_t idx) {
		bits[idx] = 1;
	}

	bool test(size_t idx) const {
		return bits[idx] != 0;
	}
};

// --- Build bitmaps for dense ranges ---
static constexpr size_t RANGE_3000_30FF_SIZE = 0x100; // 256 code points
static Bitmap<RANGE_3000_30FF_SIZE> bitmap_3000_30FF{};

static constexpr size_t RANGE_31F0_31FF_SIZE = 0x10; // 16 code points
static Bitmap<RANGE_31F0_31FF_SIZE> bitmap_31F0_31FF{};

static constexpr size_t RANGE_FF61_FF70_SIZE = 0x10; // 16 code points
static Bitmap<RANGE_FF61_FF70_SIZE> bitmap_FF61_FF70{};

// --- Hash set for scattered values ---
static const std::unordered_set<unsigned int> HeadProhibitedScatter = {
	0x00A2, 0x00B0, 0x2032, 0x2033, 0x2030, 0x2103,
	0x201D, 0x3009, 0x300B, 0x300D, 0x300F, 0x3011, 0x3015,
	0xFF09, 0xFF3D, 0xFF5D, 0xFF63
	// add other scattered ones here
};

// --- Initialization (fill bitmaps) ---
struct InitBitmaps {
	InitBitmaps() {
		// Fill 0x3000–0x30FF bitmap
		unsigned int dense_3000_30FF[] = {
			0x3001, 0x3002, 0x3005, 0x303B, 0x30FB,
			0x309D, 0x309E, 0x30FD, 0x30FE, 0x30FC,
			0x30A1, 0x30A3, 0x30A5, 0x30A7, 0x30A9,
			0x30C3, 0x30E3, 0x30E5, 0x30E7, 0x30EE,
			0x30F5, 0x30F6
		};
		for (auto c : dense_3000_30FF) {
			bitmap_3000_30FF.set(c - 0x3000);
		}

		// Fill 0x31F0–0x31FF bitmap
		for (unsigned int c = 0x31F0; c <= 0x31FF; ++c) {
			bitmap_31F0_31FF.set(c - 0x31F0);
		}

		// Fill 0xFF61–0xFF70 bitmap
		for (unsigned int c = 0xFF61; c <= 0xFF70; ++c) {
			bitmap_FF61_FF70.set(c - 0xFF61);
		}
	}
};

// Static initializer
static InitBitmaps initBitmaps;

// --- Lookup function ---
inline bool ImCharIsHeadProhibitedW(unsigned int c)
{
	if (c >= 0x3000 && c <= 0x30FF) {
		return bitmap_3000_30FF.test(c - 0x3000);
	}
	if (c >= 0x31F0 && c <= 0x31FF) {
		return bitmap_31F0_31FF.test(c - 0x31F0);
	}
	if (c >= 0xFF61 && c <= 0xFF70) {
		return bitmap_FF61_FF70.test(c - 0xFF61);
	}
	return HeadProhibitedScatter.find(c) != HeadProhibitedScatter.end();
}

inline bool ImCharIsHeadProhibitedA(char c) { return c == ' ' || c == '\t' || c == '}' || c == ')' || c == ']' || c == '?' || c == '!' || c == '|' || c == '/' || c == '&' || c == '.' || c == ',' || c == ':' || c == ';';}
inline bool ImCharIsHeadProhibited(unsigned int c)  { return (c < 128 && ImCharIsHeadProhibitedA((char)c)) || ImCharIsHeadProhibitedW(c); }
inline bool ImCharIsTailProhibitedA(unsigned int c) { return c == '(' || c == '[' || c == '{' || c == '+'; }
const unsigned int TailProhibitedW[] = { 0x2018, 0x201c, 0x3008, 0x300a, 0x300c, 0x300e, 0x3010, 0x3014, 0xff08, 0xff3b, 0xff5b, 0xff62, 0xa3, 0xa5, 0xff04, 0xffe1, 0xffe5, 0xff0b };
inline bool ImCharIsTailProhibitedW(unsigned int c) { for (int i = 0; i < IM_ARRAYSIZE(TailProhibitedW); i++) if (c == TailProhibitedW[i]) return true; return false; }
inline bool ImCharIsTailProhibited(unsigned int c)  { return (c < 128 && ImCharIsTailProhibitedA(c)) || ImCharIsTailProhibitedW(c); }

inline bool ImCharIsLineBreakableW(unsigned int c)
{
	return
		(unsigned)(c - 0x3040) <= (0x9FFF - 0x3040) ||
		(unsigned)(c - 0x20000) <= (0xDFFFF - 0x20000) ||
		(unsigned)(c - 0xAC00) <= (0xD7FF - 0xAC00) ||
		(unsigned)(c - 0xF900) <= (0xFAFF - 0xF900) ||
		(unsigned)(c - 0x1100) <= (0x11FF - 0x1100) ||
		(unsigned)(c - 0x2E80) <= (0x2FFF - 0x2E80);
}

const char *CalcWordWrapPositionCJK(ImFont *font, float size, const char *text, const char *text_end, float wrap_width)
{
	if (wrap_width <= 0.0f)
		return text_end;

	ImFontBaked *baked = font->GetFontBaked(size);
	const float scale = size / baked->Size;

	float line_width = 0.0f;
	float word_width = 0.0f;
	float blank_width = 0.0f;
	wrap_width /= scale;

	const char *word_end = text;
	const char *prev_s = NULL;
	const char *s = NULL;
	const char *next_s = text;
	unsigned int prev_c = 0;
	unsigned int c = 0;
	unsigned int next_c = 0;

	bool next_char_is_line_break_able = false;
	bool char_line_is_break_able = false;
	bool next_char_is_head_prohibited = false;
	bool char_is_head_prohibited = false;

#define IM_ADVANCE_WORD()						\
	do											\
	{											\
		word_end = s;							\
		line_width += word_width + blank_width;	\
		word_width = blank_width = 0.0f;		\
	} while (0)

	IM_ASSERT(text_end != NULL);
	while (s < text_end)
	{
		// prev_s is the END of prev_c, which actually points to c
		// same for s and next_s.
		prev_s = s;
		s = next_s;
		prev_c = c;
		c = next_c;
		char_line_is_break_able = next_char_is_line_break_able;
		char_is_head_prohibited = next_char_is_head_prohibited;
		
		next_c = (unsigned int) *next_s;
		if (next_c < 0x80)
			next_s = next_s + 1;
		else
			next_s = next_s + ImTextCharFromUtf8(&next_c, next_s, text_end);
		if (next_s > text_end)
			next_c = 0;
		
		if (prev_s == NULL)
		{
			continue;
		}
		if (c < 0x20)
		{
			if (c == '\n')
				return prev_s;
			if (c == '\r')
				continue;
		}
		// Optimized inline version of 'float char_width = GetCharAdvance((ImWchar)c);'
		float char_width = (c < (unsigned int) baked->IndexAdvanceX.Size) ? baked->IndexAdvanceX.Data[c] : -1.0f;
		if (char_width < 0.0f)
			char_width = BuildLoadGlyphGetAdvanceOrFallback(baked, c);
		if (ImCharIsBlankW(c))
			blank_width += char_width;
		else
		{
			word_width += char_width + blank_width;
			blank_width = 0.0f;
		}
		// We ignore blank width at the end of the line (they can be skipped)
		if (line_width + word_width > wrap_width)
		{
			// Words that cannot possibly fit within an entire line will be cut anywhere.
			if (word_width < wrap_width)
				s = word_end;
			else
				s = prev_s;
			break;
		}
		
		if (!next_c)
		{
			IM_ADVANCE_WORD();
		}
		else if (c)
		{
			next_char_is_line_break_able = ImCharIsLineBreakableW(next_c);
			next_char_is_head_prohibited = ImCharIsHeadProhibited(next_c);
			if (prev_c >= '0' && prev_c <= '9' && next_c >= '0' && next_c <= '9' && !next_char_is_line_break_able)
				continue;
			if (next_char_is_line_break_able && !next_char_is_head_prohibited && !ImCharIsTailProhibited(c))
				IM_ADVANCE_WORD();
			if ((char_is_head_prohibited || char_line_is_break_able) && !next_char_is_head_prohibited)
				IM_ADVANCE_WORD();
		}
	}
#undef IM_ADVANCE_WORD

	if (s == text && text < text_end)
		return s + ImTextCountUtf8BytesFromChar(s, text_end);
	return (s > text_end) ? text_end : s;
}

} // namespace

void ImGui::RenderTextWrappedCJK(ImVec2 pos, const char* text, const char* text_end, float wrap_width)
{
	ImGuiContext& g = *GImGui;
	ImGuiWindow* window = g.CurrentWindow;

	if (!text_end)
		text_end = text + strlen(text);

	if (text == text_end)
		return;

	ImFont* font = g.Font;
	float fontSize = g.FontSize;

	const char* s = text;
	ImVec2 current_pos = pos;

	while (s < text_end)
	{
		const char* line_end = CalcWordWrapPositionCJK(font, fontSize, s, text_end, wrap_width);

		if (s != line_end)
		{
			window->DrawList->AddText(font, fontSize, current_pos, GetColorU32(ImGuiCol_Text), s, line_end, 0.0f);
		}

		current_pos.y += fontSize;
		s = ImTextCalcWordWrapNextLineStart(line_end, text_end);
	}

	if (g.LogEnabled)
		LogRenderedText(&pos, text, text_end);
}

ImVec2 ImGui::CalcTextSizeCJK(ImFont* font, float size, float max_width, float wrap_width, const char* text_begin, const char* text_end, const char** out_remaining)
{
	if (!text_end)
		text_end = text_begin + strlen(text_begin);

	if (text_begin == text_end)
		return ImVec2(0.0f, size);

	ImVec2 text_size(0.0f, 0.0f);
	const char* s = text_begin;

	while (s < text_end)
	{
		const char* line_end = CalcWordWrapPositionCJK(font, size, s, text_end, wrap_width);

		ImVec2 line_size = font->CalcTextSizeA(size, FLT_MAX, 0.0f, s, line_end);
		text_size.x = ImMax(text_size.x, line_size.x);
		text_size.y += size;

		s = ImTextCalcWordWrapNextLineStart(line_end, text_end);
	}

	if (out_remaining)
		*out_remaining = s;

	return text_size;
}
