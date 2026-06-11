/*
    Created on: Oct 18, 2019

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
#pragma once
#include "types.h"
#include "context.h"

#if defined(TARGET_IPHONE)
#include <OpenGLES/ES3/gl.h>
#include <OpenGLES/ES3/glext.h>

#elif defined(LIBRETRO)
#include <libretro.h>
#include <glsm/glsm.h>
#include <glsm/glsmsym.h>

#ifndef GL_MAJOR_VERSION
#define GL_MAJOR_VERSION                  0x821B
#endif
#ifndef GL_MINOR_VERSION
#define GL_MINOR_VERSION                  0x821C
#endif

#ifndef GL_DEBUG_SEVERITY_NOTIFICATION
#define GL_DEBUG_SEVERITY_NOTIFICATION    0x826B
#endif
#ifndef GL_DEBUG_SEVERITY_HIGH
#define GL_DEBUG_SEVERITY_HIGH            0x9146
#endif
#ifndef GL_DEBUG_SEVERITY_MEDIUM
#define GL_DEBUG_SEVERITY_MEDIUM          0x9147
#endif
#ifndef GL_DEBUG_SEVERITY_LOW
#define GL_DEBUG_SEVERITY_LOW             0x9148
#endif

#elif defined(USE_OPENGL)
#include <glad/gl.h>
#endif

#ifdef TEST_AUTOMATION
void do_swap_automation();
#else
static inline void do_swap_automation() {}
#endif

class GLGraphicsContext : public GraphicsContext
{
public:
	virtual void swap() = 0;
	int getMajorVersion() const {
		return majorVersion;
	}
	int getMinorVersion() const {
		return minorVersion;
	}
	bool isGLES() const {
		return _isGLES;
	}
	std::string getDriverName() override {
		return driverName;
	}
	std::string getDriverVersion() override {
		return driverVersion;
	}
	bool isAMD() override {
		return amd;
	}
	void resetUIDriver();

	bool hasPerPixel() override
	{
		return isGLES()
				? majorVersion > 3 || (majorVersion == 3 && minorVersion >= 2)	// GL ES 3.2
				: majorVersion > 4 || (majorVersion == 4 && minorVersion >= 3); // GL 4.3
	}
	void setSwapInterval(int interval) override;

	static GLGraphicsContext *Instance() {
		return static_cast<GLGraphicsContext*>(GraphicsContext::Instance());
	}
	static void Create(void *window, void *display = nullptr);

protected:
	GLGraphicsContext(void *window, void *display)
		: GraphicsContext(window, display) {}
	void postInit();
	void preTerm();
	void findGLVersion();

	int gameSwapInterval = 1;
	bool gameSwapIntervalChanged = false;

private:
	int majorVersion = 0;
	int minorVersion = 0;
	bool _isGLES = false;
	std::string driverName;
	std::string driverVersion;
	bool amd = false;
};
