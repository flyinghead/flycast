// Analytic fixtures from the existing GLES Naomi-2 lighting semantics.
// SPDX-License-Identifier: GPL-2.0-or-later
void validateNaomi2(GPU& g)
{
	const auto oldLights = g.lights;
	const auto oldNaomi2 = g.naomi2;
	const auto oldTexture = g.tex0;
	MetalPolyUniforms p{};
	auto reset = [&]() {
		p = {}; g.naomi2 = {}; p.flags = (1 << 5) | (1 << 2); p.trilinear = 1;
		p.tsp0 = p.tsp1 = (1 << 20) | (2 << 22);
		g.naomi2.mv = g.naomi2.normal = g.naomi2.projection = glm::mat4(1); g.naomi2.gloss = glm::vec2(1);
		g.lights = {};
	};
	auto red = g.triangle({255, 0, 0, 128}, 1);
	reset(); g.lights.base[0][0] = .5f; g.lights.materialBase[0] = 1;
	expect("Naomi2 ambient material multiplication", g.draw(p, g.triangle({128, 0, 0, 128}, 1)), {64, 0, 0, 128});
	g.lights.materialBase[0] = 0; g.lights.base[0][0] = .25f;
	expect("Naomi2 ambient material replacement", g.draw(p, red), {64, 0, 0, 128});
	g.lights.base[0][0] = 0; g.lights.offset[0][1] = .25f;
	expect("Naomi2 untextured ambient offset", g.draw(p, red), {0, 64, 0, 128});
	reset(); g.lights.count = 16;
	for (auto& l : g.lights.lights) {
		l.parallel = 1; l.direction[2] = 1; l.color[0] = 1.f / 64; l.diffuse[0] = 1;
	}
	expect("Naomi2 all sixteen lights", g.draw(p, red), {128, 0, 0, 128});
	reset(); g.lights.count = 1;
	auto& l = g.lights.lights[0];
	l.parallel = 1; l.direction[2] = -1; l.color[0] = .25f; l.diffuse[0] = 1;
	expect("Naomi2 back-facing single-sided diffuse", g.draw(p, red), {0, 0, 0, 128});
	l.dmode = 1;
	expect("Naomi2 double-sided diffuse", g.draw(p, red), {128, 0, 0, 128});
	l.dmode = 2; l.direction[2] = 0; l.direction[0] = 1;
	expect("Naomi2 special diffuse mode", g.draw(p, red), {128, 0, 0, 128});
	l.routing = 8; g.lights.base[0][0] = .75f;
	expect("Naomi2 subtractive light routing", g.draw(p, red), {64, 0, 0, 128});
	l.routing = 4; g.lights.base[0][0] = 0;
	expect("Naomi2 diffuse alpha routing", g.draw(p, red), {0, 0, 0, 255});
	reset(); g.lights.count = 1;
	l.parallel = 0; l.position[2] = 100; l.color[0] = .25f; l.diffuse[0] = 1;
	l.distA = 1; l.angleA = 1;
	expect("Naomi2 point light", g.draw(p, red), {128, 0, 0, 128});
	l.distA = 0; l.distB = 49.5f; l.distMode = 0;
	expect("Naomi2 inverse-distance attenuation", g.draw(p, red), {64, 0, 0, 128});
	l.distB = 1.f / 198; l.distMode = 1;
	expect("Naomi2 distance attenuation", g.draw(p, red), {64, 0, 0, 128});
	l.distA = 1; l.distB = 0; l.angleA = 1; l.angleB = -1; l.direction[2] = -1;
	expect("Naomi2 spotlight rejects opposite cone", g.draw(p, red), {0, 0, 0, 128});
	reset(); g.lights.count = 1;
	l.parallel = 1; l.direction[2] = -1; l.color[0] = .25f; l.specular[0] = 1; l.routing = 1;
	auto specular = g.triangle({0, 0, 0, 128}, 100);
	for (auto& v : specular) v.offset[0] = 128;
	expect("Naomi2 specular offset routing", g.draw(p, specular), {64, 0, 0, 128});
	l.direction[2] = 1; l.smode = 1;
	expect("Naomi2 double-sided specular", g.draw(p, specular), {64, 0, 0, 128});
	l.smode = 3; l.routing = 0;
	expect("Naomi2 specular base routing", g.draw(p, specular), {64, 0, 0, 128});
	l.routing = 4;
	expect("Naomi2 specular alpha routing", g.draw(p, specular), {0, 0, 0, 255});
	reset(); p.flags |= 1 | 2; p.tsp0 |= 1 << 6;
	g.lights.base[0][0] = 1.25f; g.lights.baseOver = 1;
	uint8_t white[] = {255, 255, 255, 255}; g.tex0 = g.texture(MTLPixelFormatRGBA8Unorm, 1, 1, white, 4);
	expect("Naomi2 base overflow reaches offset", g.draw(p, red), {255, 0, 0, 255});
	// Use a half-strength texture so overflow routing changes a visible pixel.
	uint8_t half[] = {128, 128, 128, 255}; g.tex0 = g.texture(MTLPixelFormatRGBA8Unorm, 1, 1, half, 4);
	expect("Naomi2 overflow with textured material", g.draw(p, red), {192, 0, 0, 255});
	g.lights.baseOver = 0;
	expect("Naomi2 overflow disabled", g.draw(p, red), {128, 0, 0, 255});
	reset(); p.flags |= 1; p.tsp0 |= 1 << 6; g.naomi2.constantColor = glm::ivec2(1);
	uint8_t quadrants[] = {255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,255,255};
	g.tex0 = g.texture(MTLPixelFormatRGBA8Unorm, 2, 2, quadrants, 8);
	auto env = g.triangle({255, 255, 255, 128}, 1);
	for (auto& v : env) v.uv[0] = v.uv[1] = 0;
	expect("Naomi2 ordinary texture coordinates", g.draw(p, env), {255, 0, 0, 255});
	g.naomi2.env.x = 1;
	expect("Naomi2 environment coordinates", g.draw(p, env), {255, 255, 255, 255});
	g.naomi2.normal[0] = glm::vec4(0, 0, 1, 0); g.naomi2.normal[2] = glm::vec4(-1, 0, 0, 0);
	expect("Naomi2 transformed environment normal", g.draw(p, env), {0, 0, 255, 255});
	p.flags |= 1 << 14;
	expect("Naomi2 position-only pass skips environment mapping", g.draw(p, env), {255, 0, 0, 255});
	g.tex0 = oldTexture;
	reset(); p.flags |= (1 << 6) | (1 << 13);
	g.lights.base[0][0] = .5f; g.lights.base[1][1] = .25f;
	uint8_t shadow = 0x81; [g.stencil replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:&shadow bytesPerRow:1];
	expect("Naomi2 second-volume material", g.draw(p, red), {0, 64, 0, 255});
	shadow = 0; [g.stencil replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:&shadow bytesPerRow:1];
	expect("Naomi2 first-volume material", g.draw(p, red), {128, 0, 0, 128});
	reset(); g.naomi2.constantColor = glm::ivec2(1); g.naomi2.mv[3][0] = 4;
	expect("Naomi2 model-view transformation clips geometry", g.draw(p, red), {0, 0, 0, 255});
	g.naomi2.mv = glm::mat4(1);
	expect("Naomi2 projection near depth", g.draw(p, red, .47f), {255, 0, 0, 128});
	g.naomi2.projection[3][3] = 2;
	expect("Naomi2 projection reciprocal depth", g.draw(p, red, .47f), {0, 0, 0, 255});
	g.lights = oldLights;
	g.naomi2 = oldNaomi2;
}
