// Exercise the production exporter with a real Syphon Metal client.
// SPDX-License-Identifier: GPL-2.0-or-later
#import <Syphon/Syphon.h>

void MetalContext::validateSyntheticSyphon()
{
	const bool oldScale = config::VideoRoutingScale;
	const int oldHeight = config::VideoRoutingVRes;
	auto check = [](bool condition, const char *message) {
		if (!condition) throw RendererException(message);
	};
	auto wait = [&](auto predicate, const char *message) {
		NSDate *deadline = [NSDate dateWithTimeIntervalSinceNow:3];
		while (!predicate() && deadline.timeIntervalSinceNow > 0)
			[[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.01]];
		check(predicate(), message);
	};
	auto pass = [](const char *name) { NOTICE_LOG(RENDERER, "Metal Syphon PASS: %s", name); };
	os_VideoRoutingTermMetal();
	for (auto& slot : impl->slots) slot.routed = nil;
	config::VideoRoutingScale = true; config::VideoRoutingVRes = 16;
	std::array<u32, 32> pixels;
	for (int y = 0; y < 4; ++y) for (int x = 0; x < 8; ++x)
		pixels[y * 8 + x] = y < 2 ? (x < 4 ? 0xffff0000 : 0xff00ff00) : (x < 4 ? 0xff0000ff : 0xffffffff);
	id<MTLTexture> input = makeTexture(8, 4, MTLPixelFormatBGRA8Unorm, MTLTextureUsageShaderRead, false, true);
	[input replaceRegion:MTLRegionMake2D(0, 0, 8, 4) mipmapLevel:0 withBytes:pixels.data() bytesPerRow:32];
	auto send = [&]() {
		auto cb = beginFrame(); publishVideoRouting(input, cb);
		[cb commit]; impl->frame = nil; [cb waitUntilCompleted];
		checkMetal(cb.error, "Syphon integration submission");
	};
	send();
	check(os_VideoRoutingMetalServerDescriptionForTests() != nil, "Syphon server registration missing");
	for (auto& slot : impl->slots) check(slot.routed == nil, "Syphon scaled without clients");
	pass("registered exporter avoids scaling without clients");
	SyphonMetalClient *client = [[SyphonMetalClient alloc] initWithServerDescription:os_VideoRoutingMetalServerDescriptionForTests()
		device:impl->device options:nil newFrameHandler:nil];
	check(client.isValid, "Syphon client connection invalid");
	wait([&]() { return os_VideoRoutingMetalHasClients(impl->device); }, "Syphon client did not connect");
	auto receive = [&](SyphonMetalClient *receiver, int w, int h) {
		wait([&]() { return bool(receiver.hasNewFrame); }, "Syphon did not deliver frame");
		id<MTLTexture> image = [receiver newFrameImage];
		check(image && image.width == w && image.height == h, "Syphon output dimensions mismatch");
		std::vector<u8> out; readTexture(image, out);
		const int xy[4][2] = {{w / 8, h / 8}, {w * 7 / 8, h / 8}, {w / 8, h * 7 / 8}, {w * 7 / 8, h * 7 / 8}};
		const u32 expected[] = {0xffff0000, 0xff00ff00, 0xff0000ff, 0xffffffff};
		for (int i = 0; i < 4; ++i) {
			u32 color; memcpy(&color, out.data() + (xy[i][1] * w + xy[i][0]) * 4, 4);
			check(color == expected[i], "Syphon image color/orientation mismatch");
		}
	};
	std::array<id<MTLTexture>, 3> cached;
	for (int i = 0; i < 9; ++i) {
		send(); receive(client, 32, 16);
		auto texture = impl->slots[impl->currentSlot].routed;
		if (i < 3) cached[impl->currentSlot] = texture;
		else check(cached[impl->currentSlot] == texture, "Syphon resized unchanged output storage");
	}
	pass("client receives scaled pixels with stable orientation and reused storage");
	config::VideoRoutingVRes = 8; send(); receive(client, 16, 8);
	pass("connected output resize");
	config::VideoRoutingScale = false; send(); receive(client, 8, 4);
	pass("unscaled output");
	SyphonMetalClient *second = [[SyphonMetalClient alloc] initWithServerDescription:os_VideoRoutingMetalServerDescriptionForTests()
		device:impl->device options:nil newFrameHandler:nil];
	check(second.isValid, "Second Syphon client connection invalid");
	send(); receive(client, 8, 4); receive(second, 8, 4);
	[second stop]; second = nil;
	pass("two simultaneous Metal clients");
	[client stop]; client = nil;
	wait([&]() { return !os_VideoRoutingMetalHasClients(impl->device); }, "Syphon client did not disconnect");
	pass("client disconnect");
	NSDictionary *oldDescription = os_VideoRoutingMetalServerDescriptionForTests();
	os_VideoRoutingTermMetal();
	check(os_VideoRoutingMetalServerDescriptionForTests() == nil, "Syphon server remained registered after stop");
	os_VideoRoutingMetalHasClients(impl->device);
	check(![oldDescription isEqual:os_VideoRoutingMetalServerDescriptionForTests()], "Syphon server was not recreated");
	pass("stop and restart exporter");
	client = [[SyphonMetalClient alloc] initWithServerDescription:os_VideoRoutingMetalServerDescriptionForTests()
		device:impl->device options:nil newFrameHandler:nil];
	check(client.isValid, "Syphon reconnection invalid");
	wait([&]() { return os_VideoRoutingMetalHasClients(impl->device); }, "Syphon client did not reconnect");
	send(); receive(client, 8, 4);
	[client stop]; client = nil;
	pass("client receives pixels after exporter restart");
	os_VideoRoutingTermMetal(); waitIdle();
	for (auto& slot : impl->slots) slot.routed = nil;
	config::VideoRoutingScale = oldScale; config::VideoRoutingVRes = oldHeight;
}
