/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#include "SDL3Device/GameClient/SDL3Mouse.h"

#if defined(SAGE_USE_SDL3)

SDL3Mouse::SDL3Mouse() :
	m_nextGetIndex(0),
	m_nextFreeIndex(0)
{
	reset();
}

SDL3Mouse::~SDL3Mouse()
{
}

void SDL3Mouse::init()
{
	Mouse::init();
}

void SDL3Mouse::reset()
{
	Mouse::reset();
	for (UnsignedInt i = 0; i < NUM_MOUSE_EVENTS; ++i)
	{
		m_buffer[i].leftState = MBS_None;
		m_buffer[i].rightState = MBS_None;
		m_buffer[i].middleState = MBS_None;
		m_buffer[i].leftEvent = MOUSE_EVENT_NONE;
		m_buffer[i].rightEvent = MOUSE_EVENT_NONE;
		m_buffer[i].middleEvent = MOUSE_EVENT_NONE;
		m_buffer[i].pos.x = 0;
		m_buffer[i].pos.y = 0;
		m_buffer[i].deltaPos.x = 0;
		m_buffer[i].deltaPos.y = 0;
		m_buffer[i].wheelPos = 0;
		m_buffer[i].time = 0;
	}
	m_nextGetIndex = 0;
	m_nextFreeIndex = 0;
}

void SDL3Mouse::initCursorResources()
{
}

void SDL3Mouse::setCursor(MouseCursor cursor)
{
	m_currentCursor = cursor;
}

void SDL3Mouse::setPosition(Int x, Int y)
{
	Mouse::setPosition(x, y);
	SDL_WarpMouseInWindow(NULL, static_cast<float>(x), static_cast<float>(y));
}

void SDL3Mouse::capture()
{
	SDL_SetWindowRelativeMouseMode(SDL_GetMouseFocus(), true);
}

void SDL3Mouse::releaseCapture()
{
	SDL_SetWindowRelativeMouseMode(SDL_GetMouseFocus(), false);
}

UnsignedByte SDL3Mouse::getMouseEvent(MouseIO *result, Bool flush)
{
	if (m_nextGetIndex == m_nextFreeIndex)
	{
		return MOUSE_NONE;
	}

	*result = m_buffer[m_nextGetIndex];
	if (!flush)
	{
		m_nextGetIndex = (m_nextGetIndex + 1) % NUM_MOUSE_EVENTS;
	}
	return MOUSE_OK;
}

void SDL3Mouse::addSDL3MotionEvent(const SDL_MouseMotionEvent &event)
{
	MouseIO io;
	io.leftState = MBS_None;
	io.rightState = MBS_None;
	io.middleState = MBS_None;
	io.leftEvent = MOUSE_EVENT_NONE;
	io.rightEvent = MOUSE_EVENT_NONE;
	io.middleEvent = MOUSE_EVENT_NONE;
	io.pos.x = static_cast<Int>(event.x);
	io.pos.y = static_cast<Int>(event.y);
	io.deltaPos.x = static_cast<Int>(event.xrel);
	io.deltaPos.y = static_cast<Int>(event.yrel);
	io.wheelPos = 0;
	io.time = SDL_GetTicks();
	pushEvent(io);
}

void SDL3Mouse::addSDL3ButtonEvent(const SDL_MouseButtonEvent &event)
{
	MouseIO io;
	io.leftState = MBS_None;
	io.rightState = MBS_None;
	io.middleState = MBS_None;
	io.leftEvent = MOUSE_EVENT_NONE;
	io.rightEvent = MOUSE_EVENT_NONE;
	io.middleEvent = MOUSE_EVENT_NONE;
	io.pos.x = static_cast<Int>(event.x);
	io.pos.y = static_cast<Int>(event.y);
	io.deltaPos.x = 0;
	io.deltaPos.y = 0;
	io.wheelPos = 0;
	io.time = SDL_GetTicks();

	MouseButtonState state = (event.down != 0) ? MBS_Down : MBS_Up;
	if (event.button == SDL_BUTTON_LEFT)
	{
		io.leftState = state;
	}
	else if (event.button == SDL_BUTTON_RIGHT)
	{
		io.rightState = state;
	}
	else if (event.button == SDL_BUTTON_MIDDLE)
	{
		io.middleState = state;
	}

	pushEvent(io);
}

void SDL3Mouse::addSDL3WheelEvent(const SDL_MouseWheelEvent &event)
{
	MouseIO io;
	io.leftState = MBS_None;
	io.rightState = MBS_None;
	io.middleState = MBS_None;
	io.leftEvent = MOUSE_EVENT_NONE;
	io.rightEvent = MOUSE_EVENT_NONE;
	io.middleEvent = MOUSE_EVENT_NONE;
	io.pos.x = m_currMouse.pos.x;
	io.pos.y = m_currMouse.pos.y;
	io.deltaPos.x = 0;
	io.deltaPos.y = 0;
	io.wheelPos = static_cast<Int>(event.y * MOUSE_WHEEL_DELTA);
	io.time = SDL_GetTicks();
	pushEvent(io);
}

void SDL3Mouse::pushEvent(const MouseIO &event)
{
	m_buffer[m_nextFreeIndex] = event;
	m_nextFreeIndex = (m_nextFreeIndex + 1) % NUM_MOUSE_EVENTS;
	if (m_nextFreeIndex == m_nextGetIndex)
	{
		m_nextGetIndex = (m_nextGetIndex + 1) % NUM_MOUSE_EVENTS;
	}
}

#endif
