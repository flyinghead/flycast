#pragma once
#include "imgui.h"

namespace ImGui {
	// CJK-aware text wrapping function.
	// Use this instead of ImGui::RenderTextWrapped for better CJK support.
	void RenderTextWrappedCJK(ImVec2 pos, const char* text, const char* text_end, float wrap_width);
	ImVec2 CalcTextSizeCJK(ImFont* font, float size, float max_width, float wrap_width, const char* text_begin, const char* text_end, const char** out_remaining = nullptr);
}
