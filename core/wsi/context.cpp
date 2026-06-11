/*
	Copyright 2026 flyinghead

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
#include "context.h"

GraphicsContext *GraphicsContext::instance;

GraphicsContext::GraphicsContext(void *window, void *display)
	: window(window), display(display)
{
	instance = this;
	EventManager::listen(Event::Pause, GraphicsContext::onEvent, this);
	EventManager::listen(Event::Terminate, GraphicsContext::onEvent, this);
}

GraphicsContext::~GraphicsContext() {
	EventManager::unlisten(Event::Pause, GraphicsContext::onEvent, this);
	EventManager::unlisten(Event::Terminate, GraphicsContext::onEvent, this);
	instance = nullptr;
}

void GraphicsContext::onEvent(Event event, void *arg) {
	GraphicsContext *self = (GraphicsContext *)arg;
	self->setSwapInterval(1);
}
