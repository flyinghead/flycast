/*
    Created on: Oct 19, 2019

	Copyright 2019 flyinghead

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
#include "gl_context.h"
#if defined(LIBRETRO)
#include "libretro.h"
#elif defined(TARGET_IPHONE)
#include "osx.h"
#elif defined(USE_SDL)
#include "sdl.h"
#elif defined(__ANDROID__) || defined(SUPPORT_X11)
#include "egl.h"
#else
#error Unsupported window system
#endif

#ifndef LIBRETRO
#include "rend/gles/opengl_driver.h"
#endif

#include <cstring>

// GLES 3.0 headers used by libretro don't declare the GLES 3.1 limit
// tokens, even though the entry points are loaded dynamically.
#ifndef GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS
#define GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS 0x92DC
#endif
#ifndef GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE
#define GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE 0x92D8
#endif
#ifndef GL_MAX_COMBINED_SHADER_OUTPUT_RESOURCES
#define GL_MAX_COMBINED_SHADER_OUTPUT_RESOURCES 0x8F39
#endif
#ifndef GL_MAX_FRAGMENT_ATOMIC_COUNTERS
#define GL_MAX_FRAGMENT_ATOMIC_COUNTERS 0x92D6
#endif
#ifndef GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS
#define GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS 0x92D0
#endif
#ifndef GL_MAX_FRAGMENT_IMAGE_UNIFORMS
#define GL_MAX_FRAGMENT_IMAGE_UNIFORMS 0x90CE
#endif
#ifndef GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS
#define GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS 0x90DA
#endif
#ifndef GL_MAX_IMAGE_UNITS
#define GL_MAX_IMAGE_UNITS 0x8F38
#endif
#ifndef GL_MAX_SHADER_STORAGE_BLOCK_SIZE
#define GL_MAX_SHADER_STORAGE_BLOCK_SIZE 0x90DE
#endif
#ifndef GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS
#define GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS 0x90DD
#endif

static bool hasExtension(const char *extensions, const char *name)
{
	if (extensions == nullptr)
		return false;
	const size_t nameLength = strlen(name);
	for (const char *match = strstr(extensions, name); match != nullptr; match = strstr(match + nameLength, name))
	{
		const bool startsToken = match == extensions || match[-1] == ' ';
		const bool endsToken = match[nameLength] == '\0' || match[nameLength] == ' ';
		if (startsToken && endsToken)
			return true;
	}
	return false;
}

bool GLGraphicsContext::checkPerPixelSupport() const
{
	const bool versionSupported = _isGLES
			? majorVersion > 3 || (majorVersion == 3 && minorVersion >= 1)
			: majorVersion > 4 || (majorVersion == 4 && minorVersion >= 3);
	if (!versionSupported)
		return false;
	const bool imageAtomicsSupported = !_isGLES
			|| majorVersion > 3 || (majorVersion == 3 && minorVersion >= 2)
			|| hasExtension((const char *)glGetString(GL_EXTENSIONS), "GL_OES_shader_image_atomic");

	GLint fragmentStorageBlocks = 0;
	GLint fragmentImages = 0;
	GLint fragmentAtomicCounters = 0;
	GLint fragmentAtomicBuffers = 0;
	GLint storageBindings = 0;
	GLint imageUnits = 0;
	GLint atomicBindings = 0;
	GLint atomicBufferSize = 0;
	GLint combinedOutputs = 0;
	GLint64 storageBlockSize = 0;
	glGetIntegerv(GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS, &fragmentStorageBlocks);
	glGetIntegerv(GL_MAX_FRAGMENT_IMAGE_UNIFORMS, &fragmentImages);
	glGetIntegerv(GL_MAX_FRAGMENT_ATOMIC_COUNTERS, &fragmentAtomicCounters);
	glGetIntegerv(GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS, &fragmentAtomicBuffers);
	glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &storageBindings);
	glGetIntegerv(GL_MAX_IMAGE_UNITS, &imageUnits);
	glGetIntegerv(GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS, &atomicBindings);
	glGetIntegerv(GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE, &atomicBufferSize);
	glGetIntegerv(GL_MAX_COMBINED_SHADER_OUTPUT_RESOURCES, &combinedOutputs);
	glGetInteger64v(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &storageBlockSize);

	// Flycast PPLL uses two fragment SSBOs, one image, one atomic counter,
	// and one color output in its resolve pass. A Pixel node is 16 bytes.
	const bool supported = imageAtomicsSupported
			&& fragmentStorageBlocks >= 2
			&& fragmentImages >= 1
			&& fragmentAtomicCounters >= 1
			&& fragmentAtomicBuffers >= 1
			&& storageBindings >= 2
			&& imageUnits >= 1
			&& atomicBindings >= 1
			&& atomicBufferSize >= 4
			&& combinedOutputs >= 4
			&& storageBlockSize >= 16;
	if (!supported)
	{
		WARN_LOG(RENDERER, "Per-pixel sorting resources unavailable: image atomics %s, fragment SSBOs %d, images %d, atomic counters %d/%d, bindings SSBO/image/atomic %d/%d/%d, combined outputs %d, block sizes %lld/%d",
				imageAtomicsSupported ? "yes" : "no", fragmentStorageBlocks, fragmentImages, fragmentAtomicCounters, fragmentAtomicBuffers,
				storageBindings, imageUnits, atomicBindings, combinedOutputs,
				(long long)storageBlockSize, atomicBufferSize);
	}
	return supported;
}

void GLGraphicsContext::findGLVersion()
{
	while (true)
		if (glGetError() == GL_NO_ERROR)
			break;
	glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
	if (glGetError() == GL_INVALID_ENUM)
		majorVersion = 2;
	else
	{
		glGetIntegerv(GL_MINOR_VERSION, &minorVersion);
	}
	const char *version = (const char *)glGetString(GL_VERSION);
	_isGLES = !strncmp(version, "OpenGL ES", 9);
	INFO_LOG(RENDERER, "OpenGL version: %s", version);

	const char *p = (const char *)glGetString(GL_RENDERER);
	driverName = p != nullptr ? p : "unknown";
	p = (const char *)glGetString(GL_VERSION);
	driverVersion = p != nullptr ? p : "unknown";
	p = (const char *)glGetString(GL_VENDOR);
	std::string vendor = p != nullptr ? p : "";
	if (vendor.substr(0, 4) == "ATI ")
		amd = true;
	else if (driverName.find(" ATI ") != std::string::npos
			|| driverName.find(" AMD ") != std::string::npos)
		// mesa
		amd = true;
	else
		amd = false;

	perPixelSupported = checkPerPixelSupport();
	if (perPixelSupported)
		INFO_LOG(RENDERER, "OpenGL%s supports PPLL per-pixel sorting", _isGLES ? " ES" : "");
}

void GLGraphicsContext::postInit()
{
	findGLVersion();
	resetUIDriver();
}

void GLGraphicsContext::preTerm()
{
#ifndef LIBRETRO
	imguiDriver.reset();
#endif
}

void GLGraphicsContext::resetUIDriver()
{
#ifndef LIBRETRO
	imguiDriver.reset();
	imguiDriver = std::unique_ptr<ImGuiDriver>(new OpenGLDriver());
#endif
}

void GLGraphicsContext::setSwapInterval(int interval)
{
	if (interval <= 0 || interval == gameSwapInterval)
		return;
	gameSwapInterval = interval;
	gameSwapIntervalChanged = true;
}

void GLGraphicsContext::Create(void *window, void *display)
{
#if defined(LIBRETRO)
	LibretroGraphicsContext::Create(window, display);
#elif defined(TARGET_IPHONE)
	OSXGraphicsContext::Create(window, display);
#elif defined(USE_SDL)
	SDLGLGraphicsContext::Create(window, display);
#elif defined(__ANDROID__) || defined(SUPPORT_X11)
	EGLGraphicsContext::Create(window, display);
#endif
}
