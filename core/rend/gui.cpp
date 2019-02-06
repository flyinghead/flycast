#include <math.h>

#include "oslib/oslib.h"
#include "cfg/cfg.h"
#include "hw/maple/maple_cfg.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/roboto_medium.h"
#include "gles/gles.h"

extern bool dc_pause_emu();
extern void dc_resume_emu(bool continue_running);
extern void dc_loadstate();
extern void dc_savestate();
extern void dc_reset();

extern int screen_width, screen_height;
extern u8 kb_shift; 		// shift keys pressed (bitmask)
extern u8 kb_key[6];		// normal keys pressed
extern s32 mo_x_abs;
extern s32 mo_y_abs;
extern u32 mo_buttons;
extern bool renderer_changed;

int screen_dpi = 96;

static bool inited = false;
static float scaling = 1.f;
static enum { Closed, Commands, Settings, ClosedNoResume } gui_state;
static bool settings_opening;
static bool touch_up;

void gui_init()
{
	if (inited)
		return;
	inited = true;

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

	io.KeyMap[ImGuiKey_Tab] = 0x2B;
	io.KeyMap[ImGuiKey_LeftArrow] = 0x50;
	io.KeyMap[ImGuiKey_RightArrow] = 0x4F;
	io.KeyMap[ImGuiKey_UpArrow] = 0x52;
	io.KeyMap[ImGuiKey_DownArrow] = 0x51;
	io.KeyMap[ImGuiKey_PageUp] = 0x4B;
	io.KeyMap[ImGuiKey_PageDown] = 0x4E;
	io.KeyMap[ImGuiKey_Home] = 0x4A;
	io.KeyMap[ImGuiKey_End] = 0x4D;
	io.KeyMap[ImGuiKey_Insert] = 0x49;
	io.KeyMap[ImGuiKey_Delete] = 0x4C;
	io.KeyMap[ImGuiKey_Backspace] = 0x2A;
	io.KeyMap[ImGuiKey_Space] = 0x2C;
	io.KeyMap[ImGuiKey_Enter] = 0x28;
	io.KeyMap[ImGuiKey_Escape] = 0x29;
	io.KeyMap[ImGuiKey_A] = 0x04;
	io.KeyMap[ImGuiKey_C] = 0x06;
	io.KeyMap[ImGuiKey_V] = 0x19;
	io.KeyMap[ImGuiKey_X] = 0x1B;
	io.KeyMap[ImGuiKey_Y] = 0x1C;
	io.KeyMap[ImGuiKey_Z] = 0x1D;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer bindings
#ifdef GLES
    ImGui_ImplOpenGL3_Init("#version 100");		// OpenGL ES 2.0
#else
    ImGui_ImplOpenGL3_Init("#version 130");		// OpenGL 3.0
#endif

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'misc/fonts/README.txt' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

    scaling = max(1.f, screen_dpi / 100.f * 0.75f);
    if (scaling > 1)
		ImGui::GetStyle().ScaleAllSizes(scaling);

    io.Fonts->AddFontFromMemoryCompressedTTF(roboto_medium_compressed_data, roboto_medium_compressed_size, 17 * scaling);
    printf("Screen DPI is %d, size %d x %d. Scaling by %.2f\n", screen_dpi, screen_width, screen_height, scaling);
}

static void ImGui_Impl_NewFrame()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui::GetIO().DisplaySize.x = screen_width;
	ImGui::GetIO().DisplaySize.y = screen_height;

	ImGuiIO& io = ImGui::GetIO();

	// Read keyboard modifiers inputs
	io.KeyCtrl = (kb_shift & (0x01 | 0x10)) != 0;
	io.KeyShift = (kb_shift & (0x02 | 0x20)) != 0;
	io.KeyAlt = false;
	io.KeySuper = false;

	memset(&io.KeysDown[0], 0, sizeof(io.KeysDown));
	for (int i = 0; i < IM_ARRAYSIZE(kb_key); i++)
		if (kb_key[i] != 0)
			io.KeysDown[kb_key[i]] = true;
		else
			break;
	float scale = screen_height / 480.0f;
	float x_offset = (screen_width - 640.0f * scale) / 2;
	int real_x = mo_x_abs * scale + x_offset;
	int real_y = mo_y_abs * scale;
	if (real_x < 0 || real_x >= screen_width || real_y < 0 || real_y >= screen_height)
		io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
	else
		io.MousePos = ImVec2(real_x, real_y);
#ifdef _ANDROID
	// Put the "mouse" outside the screen one frame after a touch up
	// This avoids buttons and the like to stay selected
	if ((mo_buttons & 0xf) == 0xf)
	{
		if (touch_up)
		{
			io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
			touch_up = false;
		}
		else if (io.MouseDown[0])
			touch_up = true;
	}
#endif
	io.MouseDown[0] = (mo_buttons & (1 << 2)) == 0;
	io.MouseDown[1] = (mo_buttons & (1 << 1)) == 0;
	io.MouseDown[2] = (mo_buttons & (1 << 3)) == 0;
	io.MouseDown[3] = (mo_buttons & (1 << 0)) == 0;
}

static double last_render;
std::vector<float> render_times;

void gui_dosmth(int width, int height)
{
	if (last_render == 0)
	{
		last_render = os_GetSeconds();
		return;
	}
	double new_time = os_GetSeconds();
	render_times.push_back((float)(new_time - last_render));
	if (render_times.size() > 100)
		render_times.erase(render_times.begin());
	last_render = new_time;

	ImGui_Impl_NewFrame();
    ImGui::NewFrame();

    ImGui::PlotLines("Render Times", &render_times[0], render_times.size(), 0, "", 0.0, 1.0 / 30.0, ImVec2(300, 50));

    // Render dear imgui into screen
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// Helper to display a little (?) mark which shows a tooltip when hovered.
static void ShowHelpMarker(const char* desc)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void gui_open_settings()
{
	if (gui_state == Closed)
	{
		gui_state = Commands;
		settings_opening = true;
	}
}

bool gui_is_open()
{
	return gui_state != Closed;
}

static void gui_display_commands()
{
	if (!dc_pause_emu())
	{
		gui_state = Closed;
		return;
	}

	ImGui_Impl_NewFrame();
    ImGui::NewFrame();
    if (!settings_opening)
    	ImGui_ImplOpenGL3_DrawBackground();

    ImGui::SetNextWindowPos(ImVec2(screen_width / 2.f, screen_height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

    ImGui::Begin("Reicast", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Spacing();
	ImGui::Columns(2, "buttons", false);
	if (ImGui::Button("Load State", ImVec2(150 * scaling, 50 * scaling)))
	{
		gui_state = ClosedNoResume;
		dc_loadstate();
	}
	ImGui::NextColumn();
	if (ImGui::Button("Save State", ImVec2(150 * scaling, 50 * scaling)))
	{
		gui_state = ClosedNoResume;
		dc_savestate();
	}
	ImGui::Spacing(); ImGui::Spacing();

	ImGui::NextColumn();
	if (ImGui::Button("Settings", ImVec2(150 * scaling, 50 * scaling)))
	{
		gui_state = Settings;
	}
	ImGui::NextColumn();
	if (ImGui::Button("Resume", ImVec2(150 * scaling, 50 * scaling)))
	{
		gui_state = Closed;
	}
	ImGui::Spacing(); ImGui::Spacing();

	ImGui::NextColumn();
	if (ImGui::Button("Restart", ImVec2(150 * scaling, 50 * scaling)))
	{
		dc_reset();
		gui_state = Closed;
	}
	ImGui::NextColumn();
	if (ImGui::Button("Exit", ImVec2(150 * scaling, 50 * scaling)))
	{
		dc_resume_emu(false);
	}
	ImGui::Spacing();

    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData(), settings_opening);
    settings_opening = false;
}

static void gui_display_settings()
{
	ImGui_Impl_NewFrame();
    ImGui::NewFrame();

	int dynarec_enabled = settings.dynarec.Enable;
	u32 renderer = settings.pvr.rend;
	int playerCount = cfgLoadInt("players", "nb", 1);

    if (!settings_opening)
    	ImGui_ImplOpenGL3_DrawBackground();

    ImGui::SetNextWindowPos(ImVec2(screen_width / 2.f, screen_height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (scaling < 1.5f)
    	ImGui::SetNextWindowSize(ImVec2(screen_height / 480.f * 640.f * 90.f / 100.f, screen_height * 90.f / 100.f));
    else
    	// Use the entire screen width
    	ImGui::SetNextWindowSize(ImVec2(screen_width * 90.f / 100.f, screen_height * 90.f / 100.f));

    ImGui::Begin("Settings", NULL, /*ImGuiWindowFlags_AlwaysAutoResize |*/ ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    if (ImGui::Button("Done", ImVec2(100 * scaling, 30 * scaling)))
    {
    	gui_state = Commands;
#if DC_PLATFORM == DC_PLATFORM_DREAMCAST
       	if (playerCount != cfgLoadInt("players", "nb", 1))
       	{
       		cfgSaveInt("players", "nb", playerCount);
       		mcfg_DestroyDevices();
       		mcfg_CreateDevicesFromConfig();
       	}
#endif
       	SaveSettings();
    }
	ImGui::Spacing(); ImGui::Spacing();
    if (ImGui::BeginTabBar("settings", ImGuiTabBarFlags_NoTooltip))
    {
		if (ImGui::BeginTabItem("General"))
		{
			const char *languages[] = { "Japanese", "English", "German", "French", "Spanish", "Italian", "Default" };
			if (ImGui::BeginCombo("Language", languages[settings.dreamcast.language], ImGuiComboFlags_None))
			{
				for (int i = 0; i < IM_ARRAYSIZE(languages); i++)
				{
					bool is_selected = settings.dreamcast.language == i;
					if (ImGui::Selectable(languages[i], &is_selected))
						settings.dreamcast.language = i;
				}
				ImGui::EndCombo();
			}
            ImGui::SameLine();
            ShowHelpMarker("The language as configured in the Dreamcast BIOS.");

			const char *broadcast[] = { "NTSC", "PAL", "PAL/M", "PAL/N", "Default" };
			if (ImGui::BeginCombo("Broadcast", broadcast[settings.dreamcast.broadcast], ImGuiComboFlags_None))
			{
				for (int i = 0; i < IM_ARRAYSIZE(broadcast); i++)
				{
					bool is_selected = settings.dreamcast.broadcast == i;
					if (ImGui::Selectable(broadcast[i], &is_selected))
						settings.dreamcast.broadcast = i;
				}
				ImGui::EndCombo();
			}

			const char *region[] = { "Japan", "USA", "Europe", "Default" };
			if (ImGui::BeginCombo("Region", region[settings.dreamcast.region], ImGuiComboFlags_None))
			{
				for (int i = 0; i < IM_ARRAYSIZE(region); i++)
				{
					bool is_selected = settings.dreamcast.region == i;
					if (ImGui::Selectable(region[i], &is_selected))
						settings.dreamcast.region = i;
				}
				ImGui::EndCombo();
			}

			const char *cable[] = { "VGA", "VGA", "RGB Component", "TV Composite" };
			if (ImGui::BeginCombo("Cable", cable[settings.dreamcast.cable], ImGuiComboFlags_None))
			{
				for (int i = 0; i < IM_ARRAYSIZE(cable); i++)
				{
					bool is_selected = settings.dreamcast.cable == i;
					if (ImGui::Selectable(cable[i], &is_selected))
						settings.dreamcast.cable = i;
				}
				ImGui::EndCombo();
			}

			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Controls"))
		{
#if DC_PLATFORM == DC_PLATFORM_DREAMCAST
			ImGui::SliderInt("Players", (int *)&playerCount, 1, 4);
			ImGui::Checkbox("Emulate keyboard", &settings.input.DCKeyboard);
			ImGui::Checkbox("Emulate mouse", &settings.input.DCMouse);
#endif
			ImGui::SliderInt("Mouse sensitivity", (int *)&settings.input.MouseSensitivity, 1, 500);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Video"))
		{
			ImGui::Text("Renderer");
			ImGui::Columns(2, "renderers", false);
			ImGui::RadioButton("Per-triangle", (int *)&settings.pvr.rend, 0);
			ImGui::NextColumn();
			ImGui::RadioButton("Per-pixel", (int *)&settings.pvr.rend, 3);
			ImGui::Columns(1, NULL, false);
			ImGui::Separator();
			ImGui::Checkbox("Synchronous rendering", &settings.pvr.SynchronousRender);
			ImGui::Checkbox("Clipping", &settings.rend.Clipping);
			ImGui::Checkbox("Shadows", &settings.rend.ModifierVolumes);
			ImGui::Checkbox("Widescreen", &settings.rend.WideScreen);
			ImGui::Checkbox("Show FPS counter", &settings.rend.ShowFPS);
			ImGui::SliderInt("Frame skipping", (int *)&settings.pvr.ta_skip, 0, 6);
			ImGui::Separator();
			ImGui::Checkbox("Render textures to VRAM", &settings.rend.RenderToTextureBuffer);
			ImGui::SliderInt("Render to texture upscaling", (int *)&settings.rend.RenderToTextureUpscale, 1, 8);
			ImGui::Separator();
			ImGui::SliderInt("Texture upscaling", (int *)&settings.rend.TextureUpscale, 1, 8);
			ImGui::SliderInt("Upscaled texture max size", (int *)&settings.rend.MaxFilteredTextureSize, 8, 1024);
			ImGui::SliderInt("Max threads", (int *)&settings.pvr.MaxThreads, 1, 8);
			ImGui::Checkbox("Load custom textures", &settings.rend.CustomTextures);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Audio"))
		{
			ImGui::Checkbox("Disable sound", &settings.aica.NoSound);
			ImGui::Checkbox("Enable DSP", &settings.aica.NoBatch);
			ImGui::Checkbox("Limit FPS", &settings.aica.LimitFPS);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Advanced"))
		{
			ImGui::Text("CPU Mode");
			ImGui::Columns(2, "cpu_modes", false);
			ImGui::RadioButton("Dynarec", &dynarec_enabled, 1);
			ImGui::NextColumn();
			ImGui::RadioButton("Interpreter", &dynarec_enabled, 0);
			ImGui::Columns(1, NULL, false);
			ImGui::Separator();
			ImGui::Checkbox("Dynarec safe mode", &settings.dynarec.safemode);
			ImGui::Checkbox("Dynarec unstable opt.", &settings.dynarec.unstable_opt);
			ImGui::Checkbox("Dynarec idle skip", &settings.dynarec.idleskip);
			ImGui::Checkbox("Serial console", &settings.debug.SerialConsole);
			ImGui::Checkbox("Dump textures", &settings.rend.DumpTextures);
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
    }
    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData(), false);

   	if (renderer != settings.pvr.rend)
   		renderer_changed = true;
   	settings.dynarec.Enable = (bool)dynarec_enabled;
}

void gui_display_ui()
{
	switch (gui_state)
	{
	case Settings:
		os_DoEvents();
		gui_display_settings();
		break;
	case Commands:
		os_DoEvents();
		gui_display_commands();
		break;
	case Closed:
		break;
	}

	if (gui_state == Closed)
		dc_resume_emu(true);
	else if (gui_state == ClosedNoResume)
		gui_state = Closed;
}
