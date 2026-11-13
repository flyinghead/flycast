/*
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
#include "hikaru.h"
#include "oslib/i18n.h"
#ifdef LIBRETRO
#include "postprocess.h"
#endif
#include <algorithm>
#include <memory>

#ifndef GL_TEXTURE_MAX_LEVEL
#define GL_TEXTURE_MAX_LEVEL 0x813D
#endif

using namespace hikaru;

// Vertex attributes
#define HIKARU_POS_ARRAY 0
#define HIKARU_NORMAL_ARRAY 1
#define HIKARU_UV_ARRAY 2
#define HIKARU_ALPHA_ARRAY 3
#define HIKARU_MODE_ARRAY 4

// Shading of a draw, same fields as hikaru::Shading
static const char *ShadingUniforms = R"(
#if TARGET_GL == GLES3
precision highp float;
precision highp int;
#endif

struct Light
{
	vec4 color;
	vec4 ambient;
	vec4 vec;
	vec4 att;
	vec4 spot;
};

uniform vec4 base0;
uniform vec4 base1;
uniform vec4 ambient;
uniform vec4 matSpecular;
uniform vec4 fogColor;
uniform vec4 fogParams;		// fog (z = 1): x density, y offset; depth cue (z = 2): x scale, y start
uniform ivec4 modes;		// lit, texture mode, light count, alternate mode
uniform vec4 alphaTest;		// min, max, mode, double-sided
uniform vec4 clipZ;			// near plane, near clip enabled, LOD bias, depth bias scale
uniform Light lights[4];
)";

// Vertices stay in object space. Each vertex picks its modelview (mode bit 0: the second one) and
// may leave out its rotation (bit 1) or its translation (bit 2), as Demul and Odin do.
// They are lit per pixel in eye space (looking down -Z). The position is output in clip space with
// the top of the screen at y = -1 (flycast's GL framebuffers are stored top row first).
// The GPU clips at the near plane (as the Hikaru does): z = w - 2 * near is inside the clip volume
// when w >= near. The depth is written by the fragment shader.
static const char *VertexShader = R"(
uniform mat4 modelview0;
uniform mat4 modelview1;
uniform mat4 projection;	// eye space to screen pixels (640x480, y down) times w

in vec3 in_pos;
in vec3 in_normal;
in vec2 in_uv;
in float in_alpha;
in float in_mode;		// integer value

out vec4 vtx_uv;		// xy: texture coordinates, w: eye depth
out float vtx_alpha;
out vec3 vtx_eye;
out vec3 vtx_normal;

void main()
{
	bool secondModelview = mod(in_mode, 2.0) >= 1.0;
	bool noRotation = mod(floor(in_mode / 2.0), 2.0) >= 1.0;
	bool noTranslation = mod(floor(in_mode / 4.0), 2.0) >= 1.0;
	mat4 mv = secondModelview ? modelview1 : modelview0;
	vec3 eye = in_pos;
	vec3 normal = in_normal;
	if (!noRotation)
	{
		eye = mat3(mv) * eye;
		normal = mat3(mv) * normal;
	}
	if (!noTranslation)
		eye += mv[3].xyz;
	vec4 pos = projection * vec4(eye, 1.0);
	float z = clipZ.y != 0.0 ? pos.w - 2.0 * clipZ.x : 0.0;
	gl_Position = vec4(pos.x / 320.0 - pos.w, pos.y / 240.0 - pos.w, z, pos.w);
	vtx_uv = vec4(in_uv, 0.0, pos.w);
	vtx_alpha = in_alpha;
	vtx_eye = eye;
	vtx_normal = normal;
}
)";

static const char *FragmentShader = R"(
uniform sampler2D tex;

in vec4 vtx_uv;
in float vtx_alpha;
in vec3 vtx_eye;
in vec3 vtx_normal;

void addLight(Light l, vec3 eye, vec3 N, inout vec3 diffuse, inout vec3 specular)
{
	vec3 L;
	float atten = 1.0;
	if (l.vec.w == 1.0)
	{
		vec3 delta = l.vec.xyz - eye;
		float d = length(delta);
		L = delta / max(d, 1e-6);
		if (l.att.z == 0.0)
		{
			float f = l.att.w == 0.0 ? d : l.att.w == 1.0 ? d * d : l.att.w == 2.0 ? 1.0 / d : 1.0 / (d * d);
			atten = clamp(l.att.x * (l.att.y + f), 0.0, 1.0);
		}
	}
	else
		L = normalize(l.vec.xyz);
	if (any(notEqual(l.spot.xyz, vec3(0.0))))
		atten *= clamp(dot(L, l.spot.xyz) - l.spot.w, 0.0, 1.0);	// spot light: fade = saturate(L . spot.xyz - spot.w), spot.xyz = axis toward the light * scale
	float nl = max(dot(N, L), 0.0);
	diffuse += atten * (nl * l.color.rgb + l.ambient.rgb);
	if (nl > 0.0 && matSpecular.w > 0.0)
	{
		vec3 R = reflect(-L, N);
		float s = max(dot(R, -normalize(eye)), 0.0);
		specular += atten * pow(s, matSpecular.w) * l.color.rgb;
	}
}

uniform float pattern;		// 0x501: pixels drawn in each 4x4 block, bit x + 4 * y

void main()
{
	// 0x501: only the pixels of the 4x4 pattern are drawn. The framebuffer is stored top row
	// first, so the window y is the screen y.
	ivec2 cell = ivec2(gl_FragCoord.xy) & 3;
	if (((int(pattern) >> (cell.x + cell.y * 4)) & 1) == 0)
		discard;
	float eyeDepth = vtx_uv.w;
	float alpha = vtx_alpha;
	vec4 res;
	res.a = alpha;
#if TEXTURED == 1
	vec4 texCol = texture(tex, vtx_uv.xy, clipZ.z);
	if (modes.w != 0)
	{
		res.rgb = mix(base0.rgb, texCol.rgb, alpha);
		res.a = texCol.a;
	}
	else if (modes.y == 0)
	{
		res.rgb = texCol.rgb;
		res.a *= texCol.a;
	}
	else if (modes.y == 1)
		res.rgb = mix(base0.rgb, texCol.rgb, texCol.a);
	else if (modes.y == 2)
	{
		res.rgb = base1.rgb * texCol.rgb;
		res.a *= texCol.a;
	}
	else if (modes.y == 5)
	{
		res.rgb = mix(base0.rgb, base1.rgb, texCol.rgb);
		res.a *= texCol.a;
	}
	else
	{
		res.rgb = base0.rgb;
		res.a *= texCol.a;
	}
#else
	if (modes.w != 0)
	{
		res.rgb = mix(base0.rgb, base1.rgb, alpha);
		res.a = 1.0;
	}
	else
		res.rgb = base0.rgb;
#endif
	vec3 eye = vtx_eye;
	if (modes.x != 0)
	{
		vec3 N = normalize(vtx_normal);
		// the back faces of the Hikaru are the front faces of DirectX, which are back faces here (y is flipped)
		if (alphaTest.w != 0.0 && !gl_FrontFacing)
			N = -N;	// double-sided: the back face takes the reversed normal
		vec3 diffuse = vec3(0.0);
		vec3 specular = vec3(0.0);
		for (int i = 0; i < modes.z; i++)
			addLight(lights[i], eye, N, diffuse, specular);
		res.rgb *= ambient.rgb + diffuse;
		res.rgb += matSpecular.rgb * specular;
	}
	res = clamp(res, 0.0, 1.0);
	if (fogParams.z == 1.0)
	{
		// visibility: 0 = fully fogged
		float fa = clamp(fogParams.x * (eye.z + fogParams.y), 0.0, 1.0);
		res.rgb = mix(fogColor.rgb, res.rgb, fa);
	}

	if (alphaTest.z == 1.0)
	{
		// alpha-tested polygons: keep the range, then opaque
		if (res.a < alphaTest.x || res.a > alphaTest.y)
			discard;
		res.a = 1.0;
	}
	else if (alphaTest.z == 2.0 && res.a < 1.0 / 255.0)
		discard;

	// logarithmic depth of 1 / eye depth: greater is nearer
	if (fogParams.z == 2.0)
	{
		// depth cue: the polygon fades into what is behind it with its eye depth (Odin)
		float cue = clamp((-eye.z - fogParams.y) * fogParams.x, 0.0, 1.0);
		if (cue >= 1.0)
			discard;
		res.a = 1.0 - cue;
	}
	gl_FragDepth = log2(1.0 + 100000.0 / max(eyeDepth * clipZ.w, 1e-6)) / 34.0;
	gl_FragColor = res;
}
)";

// Full-screen passes: the frame blender and the final picture. Point sampled.
static const char *ScreenVertexShader = R"(
in vec2 in_pos;		// 0-1, top-left first
out vec2 vtx_uv;

void main()
{
	vtx_uv = in_pos;
	gl_Position = vec4(in_pos * 2.0 - 1.0, 0.0, 1.0);
}
)";

// Frame blender: the source with the blend factor as alpha, blended over the target 3D buffer
static const char *BlendFragmentShader = R"(
#if TARGET_GL == GLES3
precision highp float;
#endif
uniform sampler2D slot0;
uniform float blendFactor;
uniform int hasSource;		// 0: the source is the target itself, it reads as black
in vec2 vtx_uv;

void main()
{
	vec3 c = hasSource != 0 ? texture(slot0, vtx_uv).rgb : vec3(0.0);
	gl_FragColor = vec4(c, blendFactor);
}
)";

// Final picture: the topmost slot whose pixel is not transparent wins, the bottom slot is opaque
static const char *CompositeFragmentShader = R"(
#if TARGET_GL == GLES3
precision highp float;
#endif
uniform sampler2D slot0;
uniform sampler2D slot1;
uniform sampler2D slot2;
uniform ivec3 slotKind;		// per slot: 0 none, 1 3D picture, 2 layer
uniform vec2 scroll0;		// layer scroll of each slot, in texture coordinates
uniform vec2 scroll1;
uniform vec2 scroll2;
uniform float keyed3D;		// 1 when the 3D picture is keyed on its alpha
in vec2 vtx_uv;

bool sampleSlot(sampler2D tex, int kind, vec2 offset, out vec4 c)
{
	if (kind == 1)
	{
		c = texture(tex, vtx_uv);
		return keyed3D != 0.0 ? c.a != 0.0 : any(notEqual(c.rgb, vec3(0.0)));
	}
	// the layer textures repeat: the scroll wraps
	c = texture(tex, vtx_uv + offset);
	return c.a != 0.0;
}

void main()
{
	vec4 c = vec4(0.0, 0.0, 0.0, 1.0);
	if (slotKind.z != 0 && sampleSlot(slot2, slotKind.z, scroll2, c))
		;
	else if (slotKind.y != 0 && sampleSlot(slot1, slotKind.y, scroll1, c))
		;
	else if (slotKind.x != 0)
		sampleSlot(slot0, slotKind.x, scroll0, c);
	else
		c = vec4(0.0);
	gl_FragColor = vec4(c.rgb, 1.0);
}
)";

namespace {

struct HikaruShader
{
	GLuint program = 0;
	GLint modelview0;
	GLint modelview1;
	GLint projection;
	GLint base0;
	GLint base1;
	GLint ambient;
	GLint matSpecular;
	GLint fogColor;
	GLint fogParams;
	GLint modes;
	GLint alphaTest;
	GLint clipZ;
	GLint pattern;
	struct {
		GLint color;
		GLint ambient;
		GLint vec;
		GLint att;
		GLint spot;
	} lights[4];
};

struct HikaruTexture
{
	GLuint id = 0;
	u32 width = 0;
	u32 height = 0;
	u32 levels = 0;
	u32 version = 0;
};

class HikaruVertexArray final : public GlVertexArray
{
protected:
	void defineVtxAttribs() override
	{
		glEnableVertexAttribArray(HIKARU_POS_ARRAY);
		glVertexAttribPointer(HIKARU_POS_ARRAY, 3, GL_FLOAT, GL_FALSE, sizeof(DrawVertex), (void *)offsetof(DrawVertex, pos));
		glEnableVertexAttribArray(HIKARU_NORMAL_ARRAY);
		glVertexAttribPointer(HIKARU_NORMAL_ARRAY, 3, GL_FLOAT, GL_FALSE, sizeof(DrawVertex), (void *)offsetof(DrawVertex, normal));
		glEnableVertexAttribArray(HIKARU_UV_ARRAY);
		glVertexAttribPointer(HIKARU_UV_ARRAY, 2, GL_FLOAT, GL_FALSE, sizeof(DrawVertex), (void *)offsetof(DrawVertex, uv));
		glEnableVertexAttribArray(HIKARU_ALPHA_ARRAY);
		glVertexAttribPointer(HIKARU_ALPHA_ARRAY, 1, GL_FLOAT, GL_FALSE, sizeof(DrawVertex), (void *)offsetof(DrawVertex, alpha));
		// read as a float so that it works without integer attributes
		glEnableVertexAttribArray(HIKARU_MODE_ARRAY);
		glVertexAttribPointer(HIKARU_MODE_ARRAY, 1, GL_UNSIGNED_INT, GL_FALSE, sizeof(DrawVertex), (void *)offsetof(DrawVertex, mode));
	}
};

class ScreenVertexArray final : public GlVertexArray
{
protected:
	void defineVtxAttribs() override
	{
		glEnableVertexAttribArray(HIKARU_POS_ARRAY);
		glVertexAttribPointer(HIKARU_POS_ARRAY, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void *)0);
		for (int i = HIKARU_NORMAL_ARRAY; i <= HIKARU_MODE_ARRAY; i++)
			glDisableVertexAttribArray(i);
	}
};

class HikaruDrawer
{
public:
	void draw(const DrawList& dl, int width, int height);
	void term();

private:
	void init();
	GLuint compileAndLink(const std::string& vertexShader, const std::string& fragmentShader);
	bool updateTexture(int id);
	GLuint textureId(int id) const;
	void uploadTextures(const DrawList& dl);
	void setScissor(int x0, int y0, int x1, int y1);
	void setBlend(Blend blend);
	void setCull(Cull cull);
	void setShading(const HikaruShader& shader, const Shading& shading);
	void ensureBuffers(int width, int height);
	void drawScreen();

	HikaruShader shaders[2];	// [textured]
	GLuint blendShader = 0;
	GLint blendFactor = -1;
	GLint blendHasSource = -1;
	GLuint compositeShader = 0;
	GLint slotKind = -1;
	GLint slotScroll[3] { -1, -1, -1 };
	GLint keyed3D = -1;
	std::unique_ptr<GlBuffer> vertexBuffer;
	std::unique_ptr<GlBuffer> indexBuffer;
	std::unique_ptr<GlBuffer> screenBuffer;
	HikaruVertexArray vertexArray;
	ScreenVertexArray screenVertexArray;
	// The 3D buffers (0x781 selects the one drawn into); they persist from frame to frame
	struct Buffer
	{
		GLuint texture = 0;
		GLuint framebuffer = 0;
	};
	Buffer buffers[HIKARU_3D_BUFFERS];
	GLuint depthBuffer = 0;
	int bufferWidth = 0;
	int bufferHeight = 0;
	std::vector<HikaruTexture> textures;	// by texture store id
	float scaleX = 1.f;
	float scaleY = 1.f;
};

HikaruDrawer drawer;

}

GLuint HikaruDrawer::compileAndLink(const std::string& vertexShader, const std::string& fragmentShader)
{
	GLuint vs = gl_CompileShader(vertexShader.c_str(), GL_VERTEX_SHADER);
	GLuint ps = gl_CompileShader(fragmentShader.c_str(), GL_FRAGMENT_SHADER);
	GLuint program = glCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, ps);
	glBindAttribLocation(program, HIKARU_POS_ARRAY, "in_pos");
	glBindAttribLocation(program, HIKARU_NORMAL_ARRAY, "in_normal");
	glBindAttribLocation(program, HIKARU_UV_ARRAY, "in_uv");
	glBindAttribLocation(program, HIKARU_ALPHA_ARRAY, "in_alpha");
	glBindAttribLocation(program, HIKARU_MODE_ARRAY, "in_mode");
#ifndef GLES
	if (!gl.is_gles)
		glBindFragDataLocation(program, 0, "FragColor");
#endif
	glLinkProgram(program);
	glDetachShader(program, vs);
	glDetachShader(program, ps);
	glDeleteShader(vs);
	glDeleteShader(ps);

	GLint result;
	glGetProgramiv(program, GL_LINK_STATUS, &result);
	if (!result)
	{
		char log[1024];
		log[0] = 0;
		glGetProgramInfoLog(program, sizeof(log), nullptr, log);
		WARN_LOG(RENDERER, "Hikaru shader linking failed: %s", log);
		INFO_LOG(RENDERER, "// VERTEX SHADER\n%s\n// END", vertexShader.c_str());
		INFO_LOG(RENDERER, "// FRAGMENT SHADER\n%s\n// END", fragmentShader.c_str());
		glDeleteProgram(program);
		throw RendererException(i18n::T("OpenGL shader compilation failed"));
	}
	return program;
}

void HikaruDrawer::init()
{
	if (shaders[0].program != 0)
		return;
	if (gl.gl_major < 3)
		throw RendererException(i18n::T("OpenGL ES 3.0+ required for Hikaru"));

	OpenGlSource vertexSource;
	vertexSource.addSource(VertexCompatShader)
		.addSource(ShadingUniforms)
		.addSource(VertexShader);
	const std::string vertexGlsl = vertexSource.generate();
	for (int textured = 0; textured < 2; textured++)
	{
		OpenGlSource fragmentSource;
		fragmentSource.addConstant("TEXTURED", textured)
			.addSource(PixelCompatShader)
			.addSource(ShadingUniforms)
			.addSource(FragmentShader);
		HikaruShader& s = shaders[textured];
		s.program = compileAndLink(vertexGlsl, fragmentSource.generate());
		glcache.UseProgram(s.program);
		glUniform1i(glGetUniformLocation(s.program, "tex"), 0);
		s.modelview0 = glGetUniformLocation(s.program, "modelview0");
		s.modelview1 = glGetUniformLocation(s.program, "modelview1");
		s.projection = glGetUniformLocation(s.program, "projection");
		s.base0 = glGetUniformLocation(s.program, "base0");
		s.base1 = glGetUniformLocation(s.program, "base1");
		s.ambient = glGetUniformLocation(s.program, "ambient");
		s.matSpecular = glGetUniformLocation(s.program, "matSpecular");
		s.fogColor = glGetUniformLocation(s.program, "fogColor");
		s.fogParams = glGetUniformLocation(s.program, "fogParams");
		s.modes = glGetUniformLocation(s.program, "modes");
		s.alphaTest = glGetUniformLocation(s.program, "alphaTest");
		s.clipZ = glGetUniformLocation(s.program, "clipZ");
		s.pattern = glGetUniformLocation(s.program, "pattern");
		for (u32 i = 0; i < std::size(s.lights); i++)
		{
			char str[64];
			snprintf(str, sizeof(str), "lights[%d].color", i);
			s.lights[i].color = glGetUniformLocation(s.program, str);
			snprintf(str, sizeof(str), "lights[%d].ambient", i);
			s.lights[i].ambient = glGetUniformLocation(s.program, str);
			snprintf(str, sizeof(str), "lights[%d].vec", i);
			s.lights[i].vec = glGetUniformLocation(s.program, str);
			snprintf(str, sizeof(str), "lights[%d].att", i);
			s.lights[i].att = glGetUniformLocation(s.program, str);
			snprintf(str, sizeof(str), "lights[%d].spot", i);
			s.lights[i].spot = glGetUniformLocation(s.program, str);
		}
	}

	OpenGlSource screenVertexSource;
	screenVertexSource.addSource(VertexCompatShader)
		.addSource(ScreenVertexShader);
	const std::string screenVertexGlsl = screenVertexSource.generate();
	OpenGlSource blendFragmentSource;
	blendFragmentSource.addSource(PixelCompatShader)
		.addSource(BlendFragmentShader);
	blendShader = compileAndLink(screenVertexGlsl, blendFragmentSource.generate());
	glcache.UseProgram(blendShader);
	glUniform1i(glGetUniformLocation(blendShader, "slot0"), 0);
	blendFactor = glGetUniformLocation(blendShader, "blendFactor");
	blendHasSource = glGetUniformLocation(blendShader, "hasSource");
	OpenGlSource compositeFragmentSource;
	compositeFragmentSource.addSource(PixelCompatShader)
		.addSource(CompositeFragmentShader);
	compositeShader = compileAndLink(screenVertexGlsl, compositeFragmentSource.generate());
	glcache.UseProgram(compositeShader);
	for (int i = 0; i < 3; i++)
	{
		char str[16];
		snprintf(str, sizeof(str), "slot%d", i);
		glUniform1i(glGetUniformLocation(compositeShader, str), i);
		snprintf(str, sizeof(str), "scroll%d", i);
		slotScroll[i] = glGetUniformLocation(compositeShader, str);
	}
	slotKind = glGetUniformLocation(compositeShader, "slotKind");
	keyed3D = glGetUniformLocation(compositeShader, "keyed3D");

	vertexBuffer = std::make_unique<GlBuffer>(GL_ARRAY_BUFFER);
	indexBuffer = std::make_unique<GlBuffer>(GL_ELEMENT_ARRAY_BUFFER);
	screenBuffer = std::make_unique<GlBuffer>(GL_ARRAY_BUFFER, GL_STATIC_DRAW);
	static const float quad[] { 0.f, 0.f,  1.f, 0.f,  0.f, 1.f,  1.f, 1.f };
	screenBuffer->update(quad, sizeof(quad));
	glCheck();
}

void HikaruDrawer::term()
{
	for (HikaruTexture& t : textures)
		if (t.id != 0)
			glcache.DeleteTextures(1, &t.id);
	textures.clear();
	for (HikaruShader& s : shaders)
	{
		glcache.DeleteProgram(s.program);
		s.program = 0;
	}
	glcache.DeleteProgram(blendShader);
	blendShader = 0;
	glcache.DeleteProgram(compositeShader);
	compositeShader = 0;
	for (Buffer& b : buffers)
	{
		if (b.framebuffer != 0)
			glDeleteFramebuffers(1, &b.framebuffer);
		if (b.texture != 0)
			glcache.DeleteTextures(1, &b.texture);
		b = Buffer{};
	}
	if (depthBuffer != 0)
		glDeleteRenderbuffers(1, &depthBuffer);
	depthBuffer = 0;
	bufferWidth = bufferHeight = 0;
	vertexArray.term();
	screenVertexArray.term();
	vertexBuffer.reset();
	indexBuffer.reset();
	screenBuffer.reset();
}

// Upload a texture store entry if it changed. Called with the store locked.
bool HikaruDrawer::updateTexture(int id)
{
	TextureStore& store = textureStore();
	if (id < 0 || id >= (int)store.entries.size())
		return false;
	TextureEntry& e = store.entries[id];
	if (e.version == 0 || e.texels.empty())
		return false;
	if (id >= (int)textures.size())
		textures.resize(id + 1);
	HikaruTexture& t = textures[id];
	if (t.version == e.version)
		return true;

	glActiveTexture(GL_TEXTURE0);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	if (t.id == 0 || t.width != e.width || t.height != e.height || t.levels != e.levels)
	{
		if (t.id != 0)
			glcache.DeleteTextures(1, &t.id);
		t.id = glcache.GenTexture();
		glcache.BindTexture(GL_TEXTURE_2D, t.id);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, e.levels - 1);
		const u32 *p = e.texels.data();
		for (u32 l = 0; l < e.levels; l++)
		{
			u32 w = std::max(e.width >> l, 1u);
			u32 h = std::max(e.height >> l, 1u);
			glTexImage2D(GL_TEXTURE_2D, l, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, p);
			p += w * h;
		}
		t.width = e.width;
		t.height = e.height;
		t.levels = e.levels;
	}
	else if (e.levels == 1 && e.dirtyY1 > e.dirtyY0)
	{
		// only the rows that changed (2D layers)
		glcache.BindTexture(GL_TEXTURE_2D, t.id);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, e.dirtyY0, e.width, e.dirtyY1 - e.dirtyY0, GL_RGBA, GL_UNSIGNED_BYTE,
				&e.texels[e.dirtyY0 * e.width]);
	}
	else
	{
		glcache.BindTexture(GL_TEXTURE_2D, t.id);
		const u32 *p = e.texels.data();
		for (u32 l = 0; l < e.levels; l++)
		{
			u32 w = std::max(e.width >> l, 1u);
			u32 h = std::max(e.height >> l, 1u);
			glTexSubImage2D(GL_TEXTURE_2D, l, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, p);
			p += w * h;
		}
	}
	glCheck();
	t.version = e.version;
	e.dirtyY0 = e.dirtyY1 = 0;
	return true;
}

GLuint HikaruDrawer::textureId(int id) const
{
	if (id < 0 || id >= (int)textures.size() || textures[id].version == 0)
		return 0;
	return textures[id].id;
}

void HikaruDrawer::uploadTextures(const DrawList& dl)
{
	TextureStore& store = textureStore();
	std::lock_guard<std::mutex> lock(store.mutex);
	// Textures freed by the emulation are released here too
	for (size_t id = 0; id < textures.size(); id++)
		if ((id >= store.entries.size() || store.entries[id].version == 0) && textures[id].id != 0)
		{
			glcache.DeleteTextures(1, &textures[id].id);
			textures[id] = HikaruTexture{};
		}
	int last = -1;
	for (const Draw& d : dl.draws)
		if (d.texture >= 0 && d.texture != last)
		{
			updateTexture(d.texture);
			last = d.texture;
		}
	updateTexture(dl.blend.texture);
	for (const CompositeSlot& slot : dl.composite.slots)
		updateTexture(slot.texture);
}

// Rectangle in 640x480 screen pixels. The framebuffer is stored top row first,
// so the GL window y is the screen y.
void HikaruDrawer::setScissor(int x0, int y0, int x1, int y1)
{
	int sx0 = (int)(x0 * scaleX + 0.5f);
	int sy0 = (int)(y0 * scaleY + 0.5f);
	int sx1 = (int)(x1 * scaleX + 0.5f);
	int sy1 = (int)(y1 * scaleY + 0.5f);
	glcache.Scissor(sx0, sy0, sx1 - sx0, sy1 - sy0);
	glcache.Enable(GL_SCISSOR_TEST);
}

// Opaque, then the material blend modes (0x081 sub 5)
void HikaruDrawer::setBlend(Blend blend)
{
	// The alpha of the product blend uses the destination alpha, as GL_DST_COLOR does
	static const GLenum src[5] { GL_ONE, GL_SRC_ALPHA, GL_DST_COLOR, GL_SRC_ALPHA, GL_ONE };
	static const GLenum dst[5] { GL_ZERO, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE, GL_ONE_MINUS_SRC_ALPHA };
	if (blend == Blend::Opaque)
		glcache.Disable(GL_BLEND);
	else
	{
		glcache.Enable(GL_BLEND);
		glcache.BlendFunc(src[(int)blend], dst[(int)blend]);
	}
}

// The Y axis is flipped compared to DirectX (front faces counter-clockwise there),
// so the faces DirectX sees as front faces are back faces here, as with the PVR cull modes.
void HikaruDrawer::setCull(Cull cull)
{
	if (cull == Cull::None)
		glcache.Disable(GL_CULL_FACE);
	else
	{
		glcache.Enable(GL_CULL_FACE);
		glcache.CullFace(cull == Cull::Front ? GL_BACK : GL_FRONT);
	}
}

void HikaruDrawer::setShading(const HikaruShader& s, const Shading& shading)
{
	glUniform4fv(s.base0, 1, shading.base0);
	glUniform4fv(s.base1, 1, shading.base1);
	glUniform4fv(s.ambient, 1, shading.ambient);
	glUniform4fv(s.matSpecular, 1, shading.specular);
	glUniform4fv(s.fogColor, 1, shading.fogColor);
	glUniform4fv(s.fogParams, 1, shading.fog);
	glUniform4iv(s.modes, 1, shading.modes);
	glUniform4fv(s.alphaTest, 1, shading.alphaTest);
	glUniform4fv(s.clipZ, 1, shading.clipZ);
	int lightCount = std::clamp(shading.modes[2], 0, (int)std::size(s.lights));
	for (int i = 0; i < lightCount; i++)
	{
		glUniform4fv(s.lights[i].color, 1, shading.lights[i].color);
		glUniform4fv(s.lights[i].ambient, 1, shading.lights[i].ambient);
		glUniform4fv(s.lights[i].vec, 1, shading.lights[i].vec);
		glUniform4fv(s.lights[i].att, 1, shading.lights[i].att);
		glUniform4fv(s.lights[i].spot, 1, shading.lights[i].spot);
	}
}

void HikaruDrawer::ensureBuffers(int width, int height)
{
	if (bufferWidth == width && bufferHeight == height)
		return;
	if (depthBuffer == 0)
		glGenRenderbuffers(1, &depthBuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	for (Buffer& b : buffers)
	{
		if (b.texture == 0)
			b.texture = glcache.GenTexture();
		glcache.BindTexture(GL_TEXTURE_2D, b.texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		if (b.framebuffer == 0)
			glGenFramebuffers(1, &b.framebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, b.framebuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, b.texture, 0);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);
		glcache.ClearColor(0.f, 0.f, 0.f, 0.f);
		glcache.Disable(GL_SCISSOR_TEST);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	bufferWidth = width;
	bufferHeight = height;
	glCheck();
}

void HikaruDrawer::drawScreen()
{
	screenVertexArray.bind(screenBuffer.get());
	setCull(Cull::None);
	glcache.Disable(GL_DEPTH_TEST);
	glcache.DepthMask(GL_FALSE);
	glcache.Disable(GL_SCISSOR_TEST);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glCheck();
}

// Point sampling, repeating for the layers (scroll wrap). Several texture units are used:
// glcache only follows unit 0, so the GL calls are direct and glcache is told afterwards.
static void setScreenSampling(GLuint id, bool repeat)
{
	glBindTexture(GL_TEXTURE_2D, id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
}

static void unbindScreenTextures()
{
	for (int i = 2; i >= 0; i--)
	{
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	glcache.Reset();
}

void HikaruDrawer::draw(const DrawList& dl, int width, int height)
{
	init();
	scaleX = (float)width / SCREEN_WIDTH;
	scaleY = (float)height / SCREEN_HEIGHT;

	uploadTextures(dl);
	if (!dl.vertices.empty())
		vertexBuffer->update(dl.vertices.data(), dl.vertices.size() * sizeof(DrawVertex));
	if (!dl.indices.empty())
		indexBuffer->update(dl.indices.data(), dl.indices.size() * sizeof(u32));

	// the output framebuffer, where the final picture goes
	GLint output = 0;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &output);
	ensureBuffers(width, height);
	const Buffer& target = buffers[dl.target % HIKARU_3D_BUFFERS];

	// The 3D picture goes into the target buffer, cleared with the clear color
	glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
	glViewport(0, 0, width, height);
	glcache.Disable(GL_SCISSOR_TEST);
	glcache.Disable(GL_STENCIL_TEST);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glcache.ClearColor(dl.clearColor[0], dl.clearColor[1], dl.clearColor[2], dl.clearColor[3]);
	glcache.DepthMask(GL_TRUE);
	glClearDepthf(0.f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (!dl.passes.empty() && !dl.vertices.empty() && !dl.indices.empty())
	{
		vertexArray.bind(vertexBuffer.get(), indexBuffer.get());
		glActiveTexture(GL_TEXTURE0);

		const HikaruShader *lastShader = nullptr;
		int lastShading = -1;
		int lastMatrices[3] { -1, -1, -1 };
		int lastPattern = -1;
		for (const Pass& pass : dl.passes)
		{
			// higher viewport priorities draw over lower ones
			glcache.Disable(GL_SCISSOR_TEST);
			glcache.DepthMask(GL_TRUE);
			glClear(GL_DEPTH_BUFFER_BIT);
			for (u32 i = pass.firstDraw; i < pass.firstDraw + pass.drawCount; i++)
			{
				const Draw& d = dl.draws[i];
				if (d.indexCount == 0 || d.clip[2] <= d.clip[0] || d.clip[3] <= d.clip[1])
					continue;
				GLuint texId = textureId(d.texture);
				if (d.texture >= 0 && texId == 0)
					continue;
				const HikaruShader& shader = shaders[texId != 0];
				if (&shader != lastShader)
				{
					// uniforms are per program
					glcache.UseProgram(shader.program);
					lastShader = &shader;
					lastShading = -1;
					lastMatrices[0] = lastMatrices[1] = lastMatrices[2] = -1;
					lastPattern = -1;
				}
				if (d.modelview[0] != lastMatrices[0] || d.modelview[1] != lastMatrices[1] || d.projection != lastMatrices[2])
				{
					glUniformMatrix4fv(shader.modelview0, 1, GL_FALSE, dl.matrices[d.modelview[0]].data());
					glUniformMatrix4fv(shader.modelview1, 1, GL_FALSE, dl.matrices[d.modelview[1]].data());
					glUniformMatrix4fv(shader.projection, 1, GL_FALSE, dl.matrices[d.projection].data());
					lastMatrices[0] = d.modelview[0];
					lastMatrices[1] = d.modelview[1];
					lastMatrices[2] = d.projection;
				}
				if (d.pattern != lastPattern)
				{
					glUniform1f(shader.pattern, (float)d.pattern);
					lastPattern = d.pattern;
				}
				if (d.shading != lastShading)
				{
					setShading(shader, dl.shadings[d.shading]);
					lastShading = d.shading;
				}
				if (texId != 0)
				{
					static const GLenum address[3] { GL_CLAMP_TO_EDGE, GL_REPEAT, GL_MIRRORED_REPEAT };
					glcache.BindTexture(GL_TEXTURE_2D, texId);
					// nearest and bilinear filtering sample the base level only (Demul)
					GLenum minFilter;
					if (d.mipmapped)
						minFilter = d.linear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST;
					else
						minFilter = d.linear ? GL_LINEAR : GL_NEAREST;
					glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
					glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, d.linear ? GL_LINEAR : GL_NEAREST);
					glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, address[(int)d.addressU]);
					glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, address[(int)d.addressV]);
				}
				setCull(d.cull);
				// depth: greater or equal (the depth grows nearer the eye), cleared to 0
				glcache.Enable(GL_DEPTH_TEST);
				glcache.DepthFunc(d.depthTest ? GL_GEQUAL : GL_ALWAYS);
				glcache.DepthMask(d.depthWrite);
				setBlend(d.blend);
				setScissor(d.clip[0], d.clip[1], d.clip[2], d.clip[3]);
				glDrawElements(GL_TRIANGLES, d.indexCount, GL_UNSIGNED_INT, (GLvoid *)(d.firstIndex * sizeof(u32)));
			}
		}
		glCheck();
	}

	glActiveTexture(GL_TEXTURE0);
	// Frame blender: target = factor * source + (1 - factor) * target, target alpha = factor.
	// A source that is the target itself reads as black.
	if (dl.blend.enabled)
	{
		GLuint src = 0;
		if (dl.blend.source < 2)
		{
			if (dl.blend.source != dl.target)
				src = buffers[dl.blend.source].texture;
		}
		else
			src = textureId(dl.blend.texture);
		glcache.UseProgram(blendShader);
		glUniform1f(blendFactor, dl.blend.factor);
		glUniform1i(blendHasSource, src != 0);
		if (src != 0)
			setScreenSampling(src, false);
		glcache.Enable(GL_BLEND);
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
		drawScreen();
	}

	// Final picture: the shown 3D buffer and the 2D layers, in the 0x1B8 order
	glBindFramebuffer(GL_FRAMEBUFFER, output);
	glViewport(0, 0, width, height);
	glcache.Disable(GL_BLEND);
	glcache.UseProgram(compositeShader);
	int kinds[3];
	for (int i = 0; i < 3; i++)
	{
		const CompositeSlot& slot = dl.composite.slots[i];
		GLuint id = slot.is3D ? buffers[dl.displayed % HIKARU_3D_BUFFERS].texture : textureId(slot.texture);
		kinds[i] = slot.is3D ? 1 : id != 0 ? 2 : 0;
		glActiveTexture(GL_TEXTURE0 + i);
		if (id != 0)
			setScreenSampling(id, !slot.is3D);
		glUniform2f(slotScroll[i], (float)slot.scrollX / SCREEN_WIDTH, (float)slot.scrollY / SCREEN_HEIGHT);
	}
	glUniform3i(slotKind, kinds[0], kinds[1], kinds[2]);
	glUniform1f(keyed3D, dl.composite.keyed3D ? 1.f : 0.f);
	drawScreen();
	unbindScreenTextures();

	GlVertexArray::unbind();
	glcache.Disable(GL_SCISSOR_TEST);
	glcache.Disable(GL_BLEND);
	glcache.DepthMask(GL_TRUE);
}

void hikaruDrawFrame(const DrawList& dl, int width, int height)
{
	drawer.draw(dl, width, height);
}

void hikaruTerm()
{
	drawer.term();
}

bool OpenGLRenderer::renderHikaruFrame()
{
	saveCurrentFramebuffer();
	initVideoRoutingFrameBuffer();
	width = gl.rendContext->framebufferWidth;
	height = gl.rendContext->framebufferHeight;
	// The PVR video output registers only apply to the PVR (not to the Sega Hikaru)
	lastFrameHikaru = true;
	gl.ofbo.shiftX = gl.ofbo.shiftY = 0.f;
#ifdef LIBRETRO
	glBindFramebuffer(GL_FRAMEBUFFER, postProcessor.getFramebuffer(width, height));
#else
	if (init_output_framebuffer(width, height) == 0)
	{
		restoreCurrentFramebuffer();
		return false;
	}
#endif
	hikaruDrawFrame(*gl.rendContext->hikaru, width, height);
	gl.ofbo.aspectRatio = 4.f / 3.f;
#ifdef LIBRETRO
	postProcessor.render(glsm_get_current_framebuffer());
#else
	gl.ofbo2.ready = false;
	renderLastFrame();
#endif
	frameRendered = true;
	clearLastFrame = false;
	drawOSD();
	renderVideoRouting();
	restoreCurrentFramebuffer();

	return true;
}
