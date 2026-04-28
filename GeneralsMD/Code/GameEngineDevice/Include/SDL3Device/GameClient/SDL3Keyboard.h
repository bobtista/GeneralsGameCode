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

#include "GameClient/Keyboard.h"

class SDL3Keyboard : public Keyboard
{
public:
	SDL3Keyboard();
	virtual ~SDL3Keyboard() override;

	virtual void init() override;
	virtual void reset() override;
	virtual void update() override;
	virtual Bool getCapsState() override;

	void addSDL3KeyEvent(const SDL_KeyboardEvent &event);

protected:
	virtual void getKey(KeyboardIO *key) override;

private:
	enum { MAX_BUFFERED_KEYS = 256 };

	KeyDefType translateScancode(SDL_Scancode scancode) const;
	void pushKey(KeyDefType key, UnsignedShort state);

	KeyboardIO m_buffer[MAX_BUFFERED_KEYS];
	UnsignedInt m_nextGetIndex;
	UnsignedInt m_nextFreeIndex;
	Bool m_capsState;
};

#endif
