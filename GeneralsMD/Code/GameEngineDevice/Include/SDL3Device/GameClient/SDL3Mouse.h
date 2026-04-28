/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#pragma once

#if defined(SAGE_USE_SDL3)

#include <SDL3/SDL.h>

#include "GameClient/Mouse.h"

class SDL3Mouse : public Mouse
{
public:
	SDL3Mouse();
	virtual ~SDL3Mouse() override;

	virtual void init() override;
	virtual void reset() override;
	virtual void initCursorResources() override;
	virtual void setCursor(MouseCursor cursor) override;
	virtual void setPosition(Int x, Int y) override;

	void addSDL3MotionEvent(const SDL_MouseMotionEvent &event);
	void addSDL3ButtonEvent(const SDL_MouseButtonEvent &event);
	void addSDL3WheelEvent(const SDL_MouseWheelEvent &event);

protected:
	virtual void capture() override;
	virtual void releaseCapture() override;
	virtual UnsignedByte getMouseEvent(MouseIO *result, Bool flush) override;

private:
	void pushEvent(const MouseIO &event);

	MouseIO m_buffer[NUM_MOUSE_EVENTS];
	UnsignedInt m_nextGetIndex;
	UnsignedInt m_nextFreeIndex;
};

#endif
