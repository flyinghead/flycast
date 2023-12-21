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
#include <string>

void initRenderApi(void *window = nullptr, void *display = nullptr);
void termRenderApi();
void switchRenderApi();

class GraphicsContext
{
public:
	virtual ~GraphicsContext() = default;
	virtual void term() = 0;
	virtual void resize() {}
	virtual std::string getDriverName() = 0;
	virtual std::string getDriverVersion() = 0;
	virtual bool hasPerPixel() { return false; }

	void setWindow(void *window, void *display = nullptr) {
		this->window = window;
		this->display = display;
	}
	void getWindow(void **pwindow, void **pdisplay) const {
		*pwindow = window;
		*pdisplay = display;
	}

	static GraphicsContext *Instance() {
		return instance;
	}

protected:
	void *window = nullptr;
	void *display = nullptr;
	static GraphicsContext *instance;
};
