// Production overlay checks without games. SPDX-License-Identifier: GPL-2.0-or-later
#include "input/mouse.h"

void MetalContext::validateSyntheticOverlays()
{
	const auto oldDisplay = settings.display;
	const auto oldPlatform = settings.platform;
	const auto oldInput = settings.input;
	const bool oldFloat = config::FloatVMUs, oldRotate = config::Rotate90;
	const int oldSize = config::CrosshairSize, oldStretch = config::ScreenStretching;
	std::array<u32, 4> colors;
	std::array<MapleDeviceType, 4> devices;
	std::array<float, 4> mx, my;
	std::array<bool, 8> status;
	std::array<u64, 8> generations;
	std::array<std::array<u32, 48 * 32>, 8> screens;
	for (int i = 0; i < 4; ++i) {
		colors[i] = config::CrosshairColor[i]; devices[i] = config::MapleMainDevices[i];
		mx[i] = mo_x_abs[i]; my[i] = mo_y_abs[i]; config::CrosshairColor[i] = 0;
	}
	for (int i = 0; i < 8; ++i) {
		status[i] = vmu_lcd_status[i]; generations[i] = vmuLastChanged[i];
		memcpy(screens[i].data(), vmu_lcd_data[i], sizeof(vmu_lcd_data[i]));
	}
	settings.display.width = 640; settings.display.height = 480; settings.display.uiScale = 1;
	settings.platform.system = DC_PLATFORM_DREAMCAST;
	config::FloatVMUs = true; config::Rotate90 = false; config::ScreenStretching = 100;
	std::array<u8, 48 * 32> lcd;
	lcd.fill(0); std::fill_n(lcd.begin(), 48 * 16, 1);
	for (int i = 0; i < 8; ++i) push_vmu_screen(i / 2, i % 2, lcd.data());
	auto render = [&]() {
		// Overlay coordinates use the acquired surface, not the SDL window's
		// logical size. beginFrame alone deliberately does not acquire one.
		auto presentation = beginPresentation();
		if (!presentation) throw RendererException("Metal overlay test could not acquire a drawable");
		[presentation endEncoding]; impl->encoder = nil;
		auto cb = impl->frame;
		const auto [width, height] = presentationSize();
		auto texture = makeTexture(width, height, MTLPixelFormatBGRA8Unorm);
		auto pass = [MTL4RenderPassDescriptor new];
		pass.colorAttachments[0].texture = texture;
		pass.colorAttachments[0].loadAction = MTLLoadActionClear;
		pass.colorAttachments[0].storeAction = MTLStoreActionStore;
		pass.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
		auto e = [cb renderCommandEncoderWithDescriptor:pass];
		drawOverlays(e); [e endEncoding]; submitCommandBuffer(cb);
		impl->frame = nil; impl->drawable = nil; impl->drawableRequested = false;
		return texture;
	};
	auto pixel = [&](id<MTLTexture> texture, int x, int y, std::array<int, 3> rgb) {
		std::vector<u8> pixels; readTexture(texture, pixels);
		if (x < 0 || y < 0 || x >= texture.width || y >= texture.height)
			throw RendererException("Metal overlay test sample outside drawable");
		const u8 *p = pixels.data() + (y * texture.width + x) * 4;
		if (std::abs(int(p[2]) - rgb[0]) > 1 || std::abs(int(p[1]) - rgb[1]) > 1 || std::abs(int(p[0]) - rgb[2]) > 1)
			throw RendererException("Metal overlay pixel mismatch at " + std::to_string(x) + "," + std::to_string(y));
	};
	auto pass = [](const char *name) { NOTICE_LOG(RENDERER, "Metal overlay PASS: %s", name); };
	auto first = render();
	for (int i = 0; i < 8; ++i) {
		int x = i & 2 ? int(first.width) - 104 : 8;
		int y = i & 4 ? int(first.height) - 72 - (i & 1 ? 72 : 0) : 8 + (i & 1 ? 72 : 0);
		pixel(first, x + 4, y + 4, {0, 0, 0});
		pixel(first, x + 4, y + 60, {143, 143, 143});
	}
	pass("eight VMUs, placement, orientation and opacity");
	std::vector<id<MTLTexture>> pending;
	std::array<id<MTLTexture>, 3> cached;
	for (int i = 0; i < 12; ++i) {
		lcd.fill(i & 1);
		push_vmu_screen(0, 0, lcd.data());
		pending.push_back(render());
		auto texture = impl->slots[impl->currentSlot].vmu[0];
		if (i < 3) cached[impl->currentSlot] = texture;
		else if (cached[impl->currentSlot] != texture) throw RendererException("Metal VMU texture was reallocated");
	}
	for (int i = 0; i < 12; ++i) {
		int c = i & 1 ? 143 : 0; pixel(pending[i], 16, 16, {c, c, c});
	}
	pass("queued VMU updates reuse storage without corrupting older frames");
	u64 a = ~0ull; u32 snapshot[48 * 32];
	getVmuScreenSnapshot(0, a, snapshot);
	push_vmu_screen(0, 0, lcd.data());
	u64 b = a; getVmuScreenSnapshot(0, b, snapshot);
	if (b == a) throw RendererException("VMU generation did not advance");
	blankVmus(); pixel(render(), 16, 16, {0, 0, 0});
	pass("VMU generations and blanking");
	resetVmuScreens();
	for (int i = 0; i < 3; ++i) {
		pixel(render(), 16, 16, {0, 0, 0});
		for (auto texture : impl->slots[impl->currentSlot].vmu)
			if (texture) throw RendererException("Inactive VMU texture was retained");
	}
	pass("VMU disconnect releases completed slot textures");
	lcd.fill(1); push_vmu_screen(0, 0, lcd.data());
	settings.platform.system = DC_PLATFORM_NAOMI;
	pixel(render(), 16, 16, {0, 0, 0});
	pass("arcade VMU suppression");
	settings.platform.system = DC_PLATFORM_DREAMCAST; config::FloatVMUs = false;
	config::CrosshairSize = 16;
	const u32 gunColors[] = {0xff0000ff, 0xff00ff00, 0xffff0000, 0xffffffff};
	for (int i = 0; i < 4; ++i) {
		config::MapleMainDevices[i] = MDT_LightGun; config::CrosshairColor[i] = gunColors[i];
		mo_x_abs[i] = 80 * (i + 1); mo_y_abs[i] = 240;
	}
	auto guns = render();
	for (int i = 0; i < 4; ++i) {
		pixel(guns, 80 * (i + 1) - 1, 232, {i == 0 || i == 3 ? 255 : 0, i == 1 || i == 3 ? 255 : 0, i >= 2 ? 255 : 0});
		pixel(guns, 80 * (i + 1), 240, {0, 0, 0});
	}
	pass("four lightguns, colors and transparent center");
	mo_x_abs[1] = mo_x_abs[0]; config::CrosshairColor[1] = 0x8000ff00;
	pixel(render(), 79, 232, {127, 128, 0});
	mo_x_abs[1] = 160; config::CrosshairColor[1] = gunColors[1];
	pass("overlapping instanced lightguns preserve alpha order");
	for (int i = 1; i < 4; ++i) config::CrosshairColor[i] = 0;
	mo_x_abs[0] = mo_y_abs[0] = 0;
	pixel(render(), 2, 0, {255, 0, 0});
	mo_x_abs[0] = mo_y_abs[0] = -64;
	pixel(render(), 2, 0, {0, 0, 0});
	pass("lightgun edge clipping and offscreen aim");
	config::Rotate90 = true; mo_x_abs[0] = 200; mo_y_abs[0] = 150;
	auto [x, y] = getCrosshairPosition(0);
	if (std::abs(x - 252.5f) > .01f || std::abs(y - 329.25f) > .01f) throw RendererException("Rotated lightgun mapping mismatch");
	pixel(render(), 252, 322, {255, 0, 0});
	pass("rotated lightgun coordinates");
	config::Rotate90 = false; config::ScreenStretching = 125;
	mo_x_abs[0] = 320; mo_y_abs[0] = 240;
	pixel(render(), 319, 232, {255, 0, 0});
	pass("stretched lightgun coordinates");
	settings.display.uiScale = 1.5f; config::CrosshairSize = 32; config::CrosshairColor[0] = 0x800000ff;
	pixel(render(), 319, 216, {128, 0, 0});
	config::CrosshairSize = 0;
	pixel(render(), 319, 216, {0, 0, 0});
	pass("lightgun DPI scaling, alpha and zero size");
	settings.display.uiScale = 1; config::CrosshairSize = 16; config::CrosshairColor[0] = gunColors[0];
	settings.platform.system = DC_PLATFORM_NAOMI; settings.input.lightgunGame = false;
	pixel(render(), 319, 232, {0, 0, 0});
	settings.input.lightgunGame = true;
	pixel(render(), 319, 232, {255, 0, 0});
	pass("arcade lightgun detection");
	config::ScreenStretching = 100; config::CrosshairColor[2] = gunColors[2];
	settings.input.fourPlayerGames = false;
	pixel(render(), 239, 232, {0, 0, 0});
	settings.input.fourPlayerGames = true;
	pixel(render(), 239, 232, {0, 0, 255});
	pass("arcade two-player and four-player lightguns");
	waitIdle();
	for (int i = 0; i < 4; ++i) {
		config::CrosshairColor[i] = colors[i]; config::MapleMainDevices[i] = devices[i];
		mo_x_abs[i] = mx[i]; mo_y_abs[i] = my[i];
	}
	for (int i = 0; i < 8; ++i) {
		vmu_lcd_status[i] = status[i]; vmuLastChanged[i] = generations[i];
		memcpy(vmu_lcd_data[i], screens[i].data(), sizeof(vmu_lcd_data[i]));
	}
	for (auto& slot : impl->slots) { slot.vmu.fill(nil); slot.vmuChanged.fill(~0ull); }
	config::FloatVMUs = oldFloat; config::Rotate90 = oldRotate; config::CrosshairSize = oldSize; config::ScreenStretching = oldStretch;
	settings.display = oldDisplay; settings.platform = oldPlatform; settings.input = oldInput;
}
