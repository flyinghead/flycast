/*
	Copyright 2019 flyinghead

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
#if defined(__ANDROID__) || defined(TARGET_IPHONE)

#include "vgamepad.h"
#include "gui.h"
#include "stdclass.h"
#include "imgui.h"
#include "rend/osd.h"
#include "imgui_driver.h"
#include "input/gamepad.h"
#include "input/gamepad_device.h"
#include "oslib/storage.h"
#include "oslib/resources.h"
#include "cfg/cfg.h"
#include "input/gamepad.h"
#include "input/mouse.h"
#include "hw/naomi/naomi_cart.h"
#include "hw/naomi/card_reader.h"
#include "hw/maple/maple_devs.h"
#include <stb_image.h>

namespace vgamepad
{
static void stopEditing(bool canceled);
static void loadLayout();

struct Control
{
	Control() = default;
	Control(float x, float y, float w = 64.f, float h = 64.f)
		: pos(x, y), size(w, h), uv0(0, 0), uv1(1, 1) {}

	ImVec2 pos;
	ImVec2 size;
	ImVec2 uv0;
	ImVec2 uv1;
	bool disabled = false;
};
static Control Controls[_Count];
static bool Visible = true;
static bool serviceMode;
static float AlphaTrans = 1.f;
static ImVec2 StickPos;	// analog stick position [-1, 1]
constexpr char const *BTN_PATH = "picture/buttons.png";
constexpr char const *BTN_PATH_ARCADE = "picture/buttons-arcade.png";
constexpr char const *CFG_SECTION = "vgamepad";

void displayCommands()
{
	draw();
    centerNextWindow();

    ImGui::Begin("##vgamepad", NULL, ImGuiWindowFlags_NoDecoration);

	if (ImGui::Button("Save", ScaledVec2(150, 50)))
	{
		stopEditing(false);
		gui_setState(GuiState::Settings);
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset", ScaledVec2(150, 50)))
	{
		resetEditing();
		startEditing();
		gui_setState(GuiState::VJoyEdit);
	}

	ImGui::SameLine();
	if (ImGui::Button("Cancel", ScaledVec2(150, 50)))
	{
		stopEditing(true);
		gui_setState(GuiState::Settings);
	}
    ImGui::End();
}

static const char *getButtonsResPath() {
	return settings.platform.isConsole() ? BTN_PATH : BTN_PATH_ARCADE;
}

static const char *getButtonsCfgName() {
	return settings.platform.isConsole() ? "image" : "image_arcade";
}

static bool loadOSDButtons(const std::string& path)
{
	if (path.empty())
		return false;
	FILE *file = hostfs::storage().openFile(path, "rb");
	if (file == nullptr)
		return false;

	stbi_set_flip_vertically_on_load(1);
    int width, height, n;
	u8 *image_data = stbi_load_from_file(file, &width, &height, &n, STBI_rgb_alpha);
	std::fclose(file);
	if (image_data == nullptr)
		return false;
    try {
        imguiDriver->updateTexture(getButtonsResPath(), image_data, width, height, false);
    } catch (...) {
        // vulkan can throw during resizing
    }
    free(image_data);

    return true;
}

static ImTextureID loadOSDButtons()
{
	ImTextureID id{};
	// custom image
	std::string path = cfgLoadStr(CFG_SECTION, getButtonsCfgName(), "");
	if (loadOSDButtons(path))
		return id;
	if (settings.platform.isConsole())
	{
		// legacy buttons.png in data folder
		if (loadOSDButtons(get_readonly_data_path("buttons.png")))
			return id;
		// also try the home folder (android)
		if (loadOSDButtons(get_readonly_config_path("buttons.png")))
			return id;
	}
	// default in resource
	size_t size;
	std::unique_ptr<u8[]> data = resource::load(getButtonsResPath(), size);
	stbi_set_flip_vertically_on_load(1);
	int width, height, n;
	u8 *image_data = stbi_load_from_memory(data.get(), (int)size, &width, &height, &n, STBI_rgb_alpha);
    if (image_data != nullptr)
    {
        try {
            id = imguiDriver->updateTexture(getButtonsResPath(), image_data, width, height, false);
        } catch (...) {
            // vulkan can throw during resizing
        }
        free(image_data);
    }
    return id;
}

ImTextureID ImguiVGamepadTexture::getId()
{
	ImTextureID id = imguiDriver->getTexture(getButtonsResPath());
	if (id == ImTextureID())
		id = loadOSDButtons();

	return id;
}

constexpr float vjoy_tex[_Count][4] = {
	// L
	{   0,   0,  64,  64 },
	// U
	{  64,   0,  64,  64 },
	// R
	{ 128,   0,  64,  64 },
	// D
	{ 192,   0,  64,  64 },
	// Y, btn3
	{ 256,   0,  64,  64 },
	// X, btn2
	{ 320,   0,  64,  64 },
	// B, btn1
	{ 384,   0,  64,  64 },
	// A, btn0
	{ 448,   0,  64,  64 },

	// Start
	{   0,  64,  64,  64 },
	// LT
	{  64,  64,  90,  64 },
	// RT
	{ 154,  64,  90,  64 },
	// Analog
	{ 244,  64, 128, 128 },
	// Stick
	{ 372,  64,  64,  64 },
	// Fast forward
	{ 436,  64,  64,  64 },

	// C, btn4
	{   0, 128,  64,  64 },
	// Z, btn5
	{  64, 128,  64,  64 },

	// service mode
	{   0, 192,  64,  64 },
	// insert card
	{  64, 192,  64,  64 },

	// Special controls
	// service
	{ 128, 128,  64,  64 },
	// coin
	{ 384, 128,  64,  64 },
	// test
	{ 448, 128,  64,  64 },
};

static ImVec2 coinUV0, coinUV1;
static ImVec2 serviceUV0, serviceUV1;
static ImVec2 testUV0, testUV1;

constexpr float OSD_TEX_W = 512.f;
constexpr float OSD_TEX_H = 256.f;

static void setUV()
{
	int i = 0;

	for (auto& control : Controls)
	{
		control.uv0.x = (vjoy_tex[i][0] + 1) / OSD_TEX_W;
		control.uv0.y = 1.f - (vjoy_tex[i][1] + 1) / OSD_TEX_H;
		control.uv1.x = (vjoy_tex[i][0] + vjoy_tex[i][2] - 1) / OSD_TEX_W;
		control.uv1.y = 1.f - (vjoy_tex[i][1] + vjoy_tex[i][3] - 1) / OSD_TEX_H;
		i++;
		if (i >= _VisibleCount)
			break;
	}
	serviceUV0.x = (vjoy_tex[i][0] + 1) / OSD_TEX_W;
	serviceUV0.y = 1.f - (vjoy_tex[i][1] + 1) / OSD_TEX_H;
	serviceUV1.x = (vjoy_tex[i][0] + vjoy_tex[i][2] - 1) / OSD_TEX_W;
	serviceUV1.y = 1.f - (vjoy_tex[i][1] + vjoy_tex[i][3] - 1) / OSD_TEX_H;
	i++;
	coinUV0.x = (vjoy_tex[i][0] + 1) / OSD_TEX_W;
	coinUV0.y = 1.f - (vjoy_tex[i][1] + 1) / OSD_TEX_H;
	coinUV1.x = (vjoy_tex[i][0] + vjoy_tex[i][2] - 1) / OSD_TEX_W;
	coinUV1.y = 1.f - (vjoy_tex[i][1] + vjoy_tex[i][3] - 1) / OSD_TEX_H;
	i++;
	testUV0.x = (vjoy_tex[i][0] + 1) / OSD_TEX_W;
	testUV0.y = 1.f - (vjoy_tex[i][1] + 1) / OSD_TEX_H;
	testUV1.x = (vjoy_tex[i][0] + vjoy_tex[i][2] - 1) / OSD_TEX_W;
	testUV1.y = 1.f - (vjoy_tex[i][1] + vjoy_tex[i][3] - 1) / OSD_TEX_H;
	i++;

}
static OnLoad _(&setUV);

void show() {
	Visible = true;
}

void hide() {
	Visible = false;
}

ControlId hitTest(float x, float y)
{
	for (const auto& control : Controls)
		if (!control.disabled
				&& x >= control.pos.x && x < control.pos.x + control.size.x
				&& y >= control.pos.y && y < control.pos.y + control.size.y)
			return static_cast<ControlId>(&control - &Controls[0]);
	return None;
}

static u32 buttonMap[_Count] {
	DC_DPAD_LEFT,
	DC_DPAD_UP,
	DC_DPAD_RIGHT,
	DC_DPAD_DOWN,
	DC_BTN_X,
	DC_BTN_Y,
	DC_BTN_B,
	DC_BTN_A,
	DC_BTN_START,
	DC_AXIS_LT,
	DC_AXIS_RT,
	0,			// not used: analog area
	0,			// not used: analog stick
	EMU_BTN_FFORWARD,
	DC_BTN_Y,	// button 4
	DC_BTN_Z,	// button 5
	EMU_BTN_SRVMODE,
	DC_BTN_INSERT_CARD,
	DC_DPAD_LEFT | DC_DPAD_UP,
	DC_DPAD_RIGHT | DC_DPAD_UP,
	DC_DPAD_LEFT | DC_DPAD_DOWN,
	DC_DPAD_RIGHT | DC_DPAD_DOWN,
};

void setButtonMap()
{
	const bool arcade = settings.platform.isArcade();
	if (serviceMode)
	{
		buttonMap[A] = DC_BTN_D;
		buttonMap[B] = DC_DPAD2_UP;
		buttonMap[X] = DC_DPAD2_DOWN;
	}
	else
	{
		buttonMap[A] = DC_BTN_A;
		buttonMap[B] = DC_BTN_B;
		buttonMap[X] = arcade ? DC_BTN_C : DC_BTN_X;
	}
	buttonMap[Y] = arcade ? DC_BTN_X : DC_BTN_Y;
}

u32 controlToDcKey(ControlId control)
{
	if (control >= Left && control < _Count)
		return buttonMap[control];
	else
		return 0;
}

void setAnalogStick(float x, float y) {
	StickPos.x = x;
	StickPos.y = y;
}

float getControlWidth(ControlId control) {
	return Controls[control].size.x;	
}

void toggleServiceMode()
{
	serviceMode = !serviceMode;
	if (serviceMode)
	{
		Controls[A].disabled = false;
		Controls[B].disabled = false;
		Controls[X].disabled = false;
		setButtonMap();
	}
	else {
		startGame();
	}
}

static void drawButtonDim(ImDrawList *drawList, const Control& control, int state)
{
	if (control.disabled)
		return;
	float scale_h = settings.display.height / 480.f;
	float offs_x = (settings.display.width - scale_h * 640.f) / 2.f;
	ImVec2 pos = control.pos * scale_h;
	ImVec2 size = control.size * scale_h;
	pos.x += offs_x;
	ControlId controlId = static_cast<ControlId>(&control - &Controls[0]);
	if (controlId == AnalogStick)
		pos += StickPos * size;

	float col = (0.5f - 0.25f * state / 255) * AlphaTrans;
	float alpha = (100.f - config::VirtualGamepadTransparency) / 100.f * AlphaTrans;
	ImVec4 color(col, col, col, alpha);

	const ImVec2* uv0 = &control.uv0;
	const ImVec2* uv1 = &control.uv1;
	if (serviceMode)
		switch (controlId)
		{
		case A:
			uv0 = &coinUV0;
			uv1 = &coinUV1;
			break;
		case B:
			uv0 = &serviceUV0;
			uv1 = &serviceUV1;
			break;
		case X:
			uv0 = &testUV0;
			uv1 = &testUV1;
			break;
		default:
			break;
		}

    ImguiVGamepadTexture tex;
	tex.draw(drawList, pos, size, *uv0, *uv1, color);
}

static void drawButton(ImDrawList *drawList, const Control& control, bool state) {
	drawButtonDim(drawList, control, state ? 0 : 255);
}

void draw()
{
	if (Controls[Left].pos.x == 0.f)
	{
		loadLayout();
		if (Controls[Left].pos.x == 0.f)
			// mark done
			Controls[Left].pos.x = 1e-12f;
	}
		
	ImDrawList *drawList = ImGui::GetBackgroundDrawList();
	drawButton(drawList, Controls[Left], kcode[0] & buttonMap[Left]);
	drawButton(drawList, Controls[Up], kcode[0] & buttonMap[Up]);
	drawButton(drawList, Controls[Right], kcode[0] & buttonMap[Right]);
	drawButton(drawList, Controls[Down], kcode[0] & buttonMap[Down]);

	drawButton(drawList, Controls[X], kcode[0] & buttonMap[X]);
	drawButton(drawList, Controls[Y], kcode[0] & buttonMap[Y]);
	drawButton(drawList, Controls[B], kcode[0] & buttonMap[B]);
	drawButton(drawList, Controls[A], kcode[0] & buttonMap[A]);

	drawButton(drawList, Controls[Start], kcode[0] & buttonMap[Start]);

	drawButtonDim(drawList, Controls[LeftTrigger], lt[0] >> 8);

	drawButtonDim(drawList, Controls[RightTrigger], rt[0] >> 8);

	drawButton(drawList, Controls[AnalogArea], true);
	drawButton(drawList, Controls[AnalogStick], false);

	drawButton(drawList, Controls[FastForward], false);

	drawButton(drawList, Controls[Btn4], kcode[0] & buttonMap[Btn4]);
	drawButton(drawList, Controls[Btn5], kcode[0] & buttonMap[Btn5]);
	drawButton(drawList, Controls[ServiceMode], !serviceMode);
	drawButton(drawList, Controls[InsertCard], kcode[0] & buttonMap[InsertCard]);

	AlphaTrans += ((float)Visible - AlphaTrans) / 2;
}

static float getUIScale() {
	// scale is 1.1 for a 320 dpi screen of height 750
	return 1.1f * 750.f / settings.display.height * settings.display.dpi / 320.f;
}

struct LayoutElement
{
	const std::string name;
	const float dx, dy;	// default pos in dc coords, relative to sides (or middle if 0)
	const float dw, dh;	// default size in dc coords

	float x, y;			// normalized coordinates [0, 1]
	float w, h;			// normalized coordinates [0, 1], scaled with uiScale
	float scale;		// user scale

	void load()
	{
		x = cfgLoadFloat(CFG_SECTION, name + "_x", x);
		y = cfgLoadFloat(CFG_SECTION, name + "_y", y);
		scale = cfgLoadFloat(CFG_SECTION, name + "_scale", scale);
	}
	void save() const
	{
		cfgSaveFloat(CFG_SECTION, name + "_x", x);
		cfgSaveFloat(CFG_SECTION, name + "_y", y);
		cfgSaveFloat(CFG_SECTION, name + "_scale", scale);
	}

	bool hitTest(float nx, float ny) const {
		return nx >= x && nx < x + w * scale
			&& ny >= y && ny < y + h * scale;
	}

	void applyUiScale()
	{
		const float dcw = 480.f * (float)settings.display.width / settings.display.height;
		const float uiscale = getUIScale();
		w = dw / dcw * uiscale;
		h = dh / 480.f * uiscale;
	}

	void reset()
	{
		applyUiScale();
		scale = 1.f;
		const float dcw = 480.f * (float)settings.display.width / settings.display.height;
		const float uiscale = getUIScale();
		if (dx == 0)
			x = 0.5f - w / 2;
		else if (dx > 0)
			x = dx / dcw * uiscale;
		else
			x = 1.f - w + dx / dcw * uiscale;
		if (dy == 0)
			y = 0.5f - h / 2;
		else if (dy > 0)
			y = dy / 480.f * uiscale;
		else
			y = 1.f - h + dy / 480.f * uiscale;
	}
};
static LayoutElement Layout[] {
	{ "dpad",      32.f, -24.f, 192.f, 192.f },
	{ "buttons",  -24.f, -24.f, 192.f, 192.f },
	{ "start",      0.f, -24.f,  64.f,  64.f },
	{ "LT",      -134.f,-240.f,  90.f,  64.f },
	{ "RT",       -32.f,-240.f,  90.f,  64.f },
	{ "analog",    40.f,-320.f, 128.f, 128.f },
	{ "fforward", -24.f,  24.f,  64.f,  64.f },

	{ "btn4",     -24.f,-216.f,  64.f,  64.f },
	{ "btn5",    -152.f,-216.f,  64.f,  64.f },
	{ "service",  -24.f,  96.f,  64.f,  64.f },
	{ "inscard",   40.f,-250.f,  64.f,  64.f },
};

static void applyLayout()
{
	const float dcw = 480.f * (float)settings.display.width / settings.display.height;
	const float dx = (dcw - 640.f) / 2;
	const float uiscale = getUIScale();
	float x, y, scale;

	// DPad
	x = Layout[Elem_DPad].x * dcw - dx;
	y = Layout[Elem_DPad].y * 480.f;
	scale = Layout[Elem_DPad].scale * uiscale;
	Controls[Left].pos =   { x + 0.f * scale, y + 64.f * scale };
	Controls[Up].pos =     { x + 64.f * scale, y + 0.f * scale };
	Controls[Right].pos =  { x + 128.f * scale, y + 64.f * scale };
	Controls[Down].pos =   { x + 64.f * scale, y + 128.f * scale };
	for (int control = Left; control <= Down; control++)
		Controls[control].size = { 64.f * scale, 64.f * scale };

	Controls[LeftUp].pos =     { x + 0.f * scale, y + 0.f * scale };
	Controls[LeftDown].pos =   { x + 0.f * scale, y + 128.f * scale };
	Controls[RightUp].pos =    { x + 128.f * scale, y + 0.f * scale };
	Controls[RightDown].pos =  { x + 128.f * scale, y + 128.f * scale };
	for (int control = LeftUp; control <= RightDown; control++)
		Controls[control].size = { 64.f * scale, 64.f * scale };

	// Buttons
	x = Layout[Elem_Buttons].x * dcw - dx;
	y = Layout[Elem_Buttons].y * 480.f;
	scale = Layout[Elem_Buttons].scale * uiscale;
	Controls[X].pos =  { x + 0.f * scale, y + 64.f * scale };
	Controls[Y].pos =  { x + 64.f * scale, y + 0.f * scale };
	Controls[B].pos =  { x + 128.f * scale, y + 64.f * scale };
	Controls[A].pos =  { x + 64.f * scale, y + 128.f * scale };
	for (int control = X; control <= A; control++)
		Controls[control].size = { 64.f * scale, 64.f * scale };

	// Start
	scale = Layout[Elem_Start].scale * uiscale;
	Controls[Start].pos =   { Layout[Elem_Start].x * dcw - dx, Layout[Elem_Start].y * 480.f };
	Controls[Start].size =  { Layout[Elem_Start].dw * scale, Layout[Elem_Start].dh * scale };

	// Left trigger
	scale = Layout[Elem_LT].scale * uiscale;
	Controls[LeftTrigger].pos = { Layout[Elem_LT].x * dcw - dx, Layout[Elem_LT].y * 480.f };
	Controls[LeftTrigger].size = { Layout[Elem_LT].dw * scale, Layout[Elem_LT].dh * scale };

	// Right trigger
	scale = Layout[Elem_RT].scale * uiscale;
	Controls[RightTrigger].pos =  { Layout[Elem_RT].x * dcw - dx, Layout[Elem_RT].y * 480.f };
	Controls[RightTrigger].size = { Layout[Elem_RT].dw * scale, Layout[Elem_RT].dh * scale };

	// Analog
	x = Layout[Elem_Analog].x * dcw - dx;
	y = Layout[Elem_Analog].y * 480.f;
	scale = Layout[Elem_Analog].scale * uiscale;
	Controls[AnalogArea].pos =   { x, y };
	Controls[AnalogArea].size =  { Layout[Elem_Analog].dw * scale, Layout[Elem_Analog].dh * scale };
	Controls[AnalogStick].pos =  { x + 32.f * scale, y + 32.f * scale };
	Controls[AnalogStick].size = { 64.f * scale, 64.f * scale };

	// Fast forward
	scale = Layout[Elem_FForward].scale * uiscale;
	Controls[FastForward].pos =  { Layout[Elem_FForward].x * dcw - dx, Layout[Elem_FForward].y * 480.f };
	Controls[FastForward].size = { Layout[Elem_FForward].dw * scale, Layout[Elem_FForward].dh * scale };

	// ARCADE
	// Button 4
	scale = Layout[Elem_Btn4].scale * uiscale;
	Controls[Btn4].pos =  { Layout[Elem_Btn4].x * dcw - dx, Layout[Elem_Btn4].y * 480.f };
	Controls[Btn4].size = { Layout[Elem_Btn4].dw * scale, Layout[Elem_Btn4].dh * scale };
	// Button 5
	scale = Layout[Elem_Btn5].scale * uiscale;
	Controls[Btn5].pos =  { Layout[Elem_Btn5].x * dcw - dx, Layout[Elem_Btn5].y * 480.f };
	Controls[Btn5].size = { Layout[Elem_Btn5].dw * scale, Layout[Elem_Btn5].dh * scale };

	// Service Mode
	scale = Layout[Elem_ServiceMode].scale * uiscale;
	Controls[ServiceMode].pos =  { Layout[Elem_ServiceMode].x * dcw - dx, Layout[Elem_ServiceMode].y * 480.f };
	Controls[ServiceMode].size = { Layout[Elem_ServiceMode].dw * scale, Layout[Elem_ServiceMode].dh * scale };

	// Insert Card
	scale = Layout[Elem_InsertCard].scale * uiscale;
	Controls[InsertCard].pos =  { Layout[Elem_InsertCard].x * dcw - dx, Layout[Elem_InsertCard].y * 480.f };
	Controls[InsertCard].size = { Layout[Elem_InsertCard].dw * scale, Layout[Elem_InsertCard].dh * scale };
}

void applyUiScale() {
	for (auto& element : Layout)
		element.applyUiScale();
}

static void loadLayout()
{
	for (auto& element : Layout) {
		element.reset();
		element.load();
	}
	applyLayout();
}

static void saveLayout()
{
	cfgSetAutoSave(false);
	for (auto& element : Layout)
		element.save();
	cfgSetAutoSave(false);
}

static void resetLayout()
{
	for (auto& element : Layout)
		element.reset();
	applyLayout();
}

Element layoutHitTest(float x, float y)
{
	for (const auto& element : Layout)
		if (element.hitTest(x, y))
			return static_cast<Element>(&element - &Layout[0]);
	return Elem_None;
}

void translateElement(Element element, float dx, float dy)
{
	LayoutElement& e = Layout[element];
	e.x += dx;
	e.y += dy;
	applyLayout();
}

void scaleElement(Element element, float factor)
{
	LayoutElement& e = Layout[element];
	float dx = e.w * e.scale * (factor - 1.f) / 2.f;
	float dy = e.h * e.scale * (factor - 1.f) / 2.f;
	e.scale *= factor;
	// keep centered
	translateElement(element, -dx, -dy);
}

void loadImage(const std::string& path)
{
	if (path.empty()) {
		cfgSaveStr(CFG_SECTION, getButtonsCfgName(), "");
		loadOSDButtons();
	}
	else if (loadOSDButtons(path)) {
		cfgSaveStr(CFG_SECTION, getButtonsCfgName(), path);
	}
}

static void enableAllControls()
{
	for (auto& control : Controls)
		control.disabled = false;
}

static void disableControl(ControlId ctrlId)
{
#ifdef TARGET_IPHONE
	if (ctrlId == Up || ctrlId == Down)
		// Needed to pause the emulator
		return;
#endif

	Controls[ctrlId].disabled = true;
	switch (ctrlId)
	{
	case Left:
		Controls[LeftUp].disabled = true;
		Controls[LeftDown].disabled = true;
		break;
	case Right:
		Controls[RightUp].disabled = true;
		Controls[RightDown].disabled = true;
		break;
	case Up:
		Controls[LeftUp].disabled = true;
		Controls[RightUp].disabled = true;
		break;
	case Down:
		Controls[LeftDown].disabled = true;
		Controls[RightDown].disabled = true;
		break;
	case AnalogArea:
	case AnalogStick:
		Controls[AnalogArea].disabled = true;
		Controls[AnalogStick].disabled = true;
		break;
	default:
		break;
	}
}

void startGame()
{
	enableAllControls();
	serviceMode = false;
	setButtonMap();
	bool enableTouchMouse = false;
	if (settings.platform.isConsole())
	{
		disableControl(Btn4);
		disableControl(Btn5);
		disableControl(ServiceMode);
		disableControl(InsertCard);
		switch (config::MapleMainDevices[0])
		{
		case MDT_LightGun:
			enableTouchMouse = true;
			disableControl(AnalogArea);
			disableControl(LeftTrigger);
			disableControl(RightTrigger);
			disableControl(A);
			disableControl(X);
			disableControl(Y);
			break;
		case MDT_AsciiStick:
			// TODO add CZ
			disableControl(AnalogArea);
			disableControl(LeftTrigger);
			disableControl(RightTrigger);
			break;
		case MDT_PopnMusicController:
			// TODO add C btn
			disableControl(AnalogArea);
			disableControl(LeftTrigger);
			disableControl(RightTrigger);
			break;
		case MDT_RacingController:
			disableControl(X);
			disableControl(Y);
			break;
		default:
			break;
		}
	}
	else
	{
		// arcade game
		if (!card_reader::readerAvailable())
			disableControl(InsertCard);
		if (settings.platform.isAtomiswave()) {
			disableControl(Btn5);
		}
		else if (settings.platform.isSystemSP())
		{
			disableControl(Btn4);
			disableControl(Btn5);
		}
		if (NaomiGameInputs != nullptr)
		{
			bool fullAnalog = false;
			bool rt = false;
			bool lt = false;
			for (const auto& axis : NaomiGameInputs->axes)
			{
				if (axis.name == nullptr)
					break;
				switch (axis.axis)
				{
				case 0:
				case 1:
					fullAnalog = true;
					break;
				case 4:
					rt = true;
					break;
				case 5:
					lt = true;
					break;
				}
			}
			if (!fullAnalog)
				disableControl(AnalogArea);
			if (!lt)
				disableControl(LeftTrigger);
			else
				disableControl(Btn5);
			if (!rt)
				disableControl(RightTrigger);
			else
				disableControl(Btn4);
			u32 usedButtons = 0;
			for (const auto& button : NaomiGameInputs->buttons)
			{
				if (button.name == nullptr)
					break;
				usedButtons |= button.source;
			}
			if (settings.platform.isAtomiswave())
			{
				// button order: A B X Y B4
				if ((usedButtons & AWAVE_BTN0_KEY) == 0 || settings.input.lightgunGame)
					disableControl(A);
				if ((usedButtons & AWAVE_BTN1_KEY) == 0)
					disableControl(B);
				if ((usedButtons & AWAVE_BTN2_KEY) == 0)
					disableControl(X);
				if ((usedButtons & AWAVE_BTN3_KEY) == 0)
					disableControl(Y);
				if ((usedButtons & AWAVE_BTN4_KEY) == 0)
					disableControl(Btn4);
				if ((usedButtons & AWAVE_UP_KEY) == 0)
					disableControl(Up);
				if ((usedButtons & AWAVE_DOWN_KEY) == 0)
					disableControl(Down);
				if ((usedButtons & AWAVE_LEFT_KEY) == 0)
					disableControl(Left);
				if ((usedButtons & AWAVE_RIGHT_KEY) == 0)
					disableControl(Right);
				if ((usedButtons & AWAVE_START_KEY) == 0)
					disableControl(Start);
			}
			else if (settings.platform.isSystemSP())
			{
				if ((usedButtons & DC_BTN_A) == 0)
					disableControl(A);
				if ((usedButtons & DC_BTN_B) == 0)
					disableControl(B);
				if ((usedButtons & DC_BTN_C) == 0)
					disableControl(X);
				if ((usedButtons & DC_BTN_X) == 0)
					disableControl(Y);
				if ((usedButtons & DC_DPAD_UP) == 0)
					disableControl(Up);
				if ((usedButtons & DC_DPAD_DOWN) == 0)
					disableControl(Down);
				if ((usedButtons & DC_DPAD_LEFT) == 0)
					disableControl(Left);
				if ((usedButtons & DC_DPAD_RIGHT) == 0)
					disableControl(Right);
				if ((usedButtons & DC_BTN_START) == 0)
					disableControl(Start);
			}
			else
			{
				if ((usedButtons & NAOMI_BTN0_KEY) == 0 || settings.input.lightgunGame)
					disableControl(A);
				if ((usedButtons & (NAOMI_BTN1_KEY | NAOMI_RELOAD_KEY)) == 0)
					disableControl(B);
				else if (settings.input.lightgunGame
						&& (usedButtons & NAOMI_RELOAD_KEY) != 0
						&& (usedButtons & NAOMI_BTN1_KEY) == 0)
					// Remap button 1 to reload for lightgun games that need it
					buttonMap[B] = DC_BTN_RELOAD;
				if ((usedButtons & NAOMI_BTN2_KEY) == 0)
					// C
					disableControl(X);
				if ((usedButtons & NAOMI_BTN3_KEY) == 0)
					// X
					disableControl(Y);
				if ((usedButtons & NAOMI_BTN4_KEY) == 0)
					// Y
					disableControl(Btn4);
				if ((usedButtons & NAOMI_BTN5_KEY) == 0)
					// Z
					disableControl(Btn5);
				if ((usedButtons & NAOMI_UP_KEY) == 0)
					disableControl(Up);
				if ((usedButtons & NAOMI_DOWN_KEY) == 0)
					disableControl(Down);
				if ((usedButtons & NAOMI_LEFT_KEY) == 0)
					disableControl(Left);
				if ((usedButtons & NAOMI_RIGHT_KEY) == 0)
					disableControl(Right);
				if ((usedButtons & NAOMI_START_KEY) == 0)
					disableControl(Start);
			}
			if (settings.input.lightgunGame)
				enableTouchMouse = true;
		}
		else
		{
			if (settings.input.lightgunGame)
			{
				enableTouchMouse = true;
				disableControl(A);
				disableControl(X);
				disableControl(Y);
				disableControl(Btn4);
				disableControl(Btn5);
				disableControl(AnalogArea);
				disableControl(LeftTrigger);
				disableControl(RightTrigger);
				disableControl(Up);
				disableControl(Down);
				disableControl(Left);
				disableControl(Right);
			}
			else
			{
				// all analog games *should* have an input description
				disableControl(AnalogArea);
				disableControl(LeftTrigger);
				disableControl(RightTrigger);
			}
		}
	}
	std::shared_ptr<TouchMouse> touchMouse = GamepadDevice::GetGamepad<TouchMouse>();
	if (touchMouse != nullptr)
	{
		if (enableTouchMouse) {
			if (touchMouse->maple_port() == -1)
				touchMouse->set_maple_port(0);
		}
		else {
			if (touchMouse->maple_port() == 0)
				touchMouse->set_maple_port(-1);
		}
	}
}

void resetEditing() {
	resetLayout();
}

void startEditing()
{
	enableAllControls();
	show();
	setEditMode(true);
}

void pauseEditing() {
	setEditMode(false);
}

static void stopEditing(bool canceled)
{
	setEditMode(false);
	if (canceled)
		loadLayout();
	else
		saveLayout();
}

}	// namespace vgamepad

#endif // __ANDROID__ || TARGET_IPHONE
