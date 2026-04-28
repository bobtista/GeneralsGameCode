/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#include "SDL3Device/GameClient/SDL3Keyboard.h"

#if defined(SAGE_USE_SDL3)

#include "GameClient/KeyDefs.h"

SDL3Keyboard::SDL3Keyboard() :
	m_nextGetIndex(0),
	m_nextFreeIndex(0),
	m_capsState(false)
{
	reset();
}

SDL3Keyboard::~SDL3Keyboard()
{
}

void SDL3Keyboard::init()
{
	Keyboard::init();
}

void SDL3Keyboard::reset()
{
	Keyboard::reset();
	for (UnsignedInt i = 0; i < MAX_BUFFERED_KEYS; ++i)
	{
		m_buffer[i].key = KEY_NONE;
		m_buffer[i].status = KeyboardIO::STATUS_UNUSED;
		m_buffer[i].state = KEY_STATE_NONE;
		m_buffer[i].keyDownTimeMsec = 0;
	}
	m_nextGetIndex = 0;
	m_nextFreeIndex = 0;
}

void SDL3Keyboard::update()
{
	Keyboard::update();
}

Bool SDL3Keyboard::getCapsState()
{
	return m_capsState;
}

void SDL3Keyboard::addSDL3KeyEvent(const SDL_KeyboardEvent &event)
{
	KeyDefType key = translateScancode(event.scancode);
	if (key == KEY_NONE)
	{
		return;
	}

	UnsignedShort state = (event.down != 0) ? KEY_STATE_DOWN : KEY_STATE_UP;
	if (event.repeat != 0)
	{
		state |= KEY_STATE_AUTOREPEAT;
	}
	if (event.scancode == SDL_SCANCODE_CAPSLOCK && event.down != 0)
	{
		m_capsState = !m_capsState;
	}

	pushKey(key, state);
}

void SDL3Keyboard::getKey(KeyboardIO *key)
{
	if (m_nextGetIndex == m_nextFreeIndex)
	{
		key->key = KEY_NONE;
		key->status = KeyboardIO::STATUS_UNUSED;
		key->state = KEY_STATE_NONE;
		key->keyDownTimeMsec = 0;
		return;
	}

	*key = m_buffer[m_nextGetIndex];
	m_nextGetIndex = (m_nextGetIndex + 1) % MAX_BUFFERED_KEYS;
}

void SDL3Keyboard::pushKey(KeyDefType key, UnsignedShort state)
{
	KeyboardIO &slot = m_buffer[m_nextFreeIndex];
	slot.key = key;
	slot.status = KeyboardIO::STATUS_UNUSED;
	slot.state = state;
	slot.keyDownTimeMsec = SDL_GetTicks();

	m_nextFreeIndex = (m_nextFreeIndex + 1) % MAX_BUFFERED_KEYS;
	if (m_nextFreeIndex == m_nextGetIndex)
	{
		m_nextGetIndex = (m_nextGetIndex + 1) % MAX_BUFFERED_KEYS;
	}
}

KeyDefType SDL3Keyboard::translateScancode(SDL_Scancode scancode) const
{
	switch (scancode)
	{
		case SDL_SCANCODE_ESCAPE: return KEY_ESC;
		case SDL_SCANCODE_BACKSPACE: return KEY_BACKSPACE;
		case SDL_SCANCODE_RETURN: return KEY_ENTER;
		case SDL_SCANCODE_SPACE: return KEY_SPACE;
		case SDL_SCANCODE_TAB: return KEY_TAB;
		case SDL_SCANCODE_F1: return KEY_F1;
		case SDL_SCANCODE_F2: return KEY_F2;
		case SDL_SCANCODE_F3: return KEY_F3;
		case SDL_SCANCODE_F4: return KEY_F4;
		case SDL_SCANCODE_F5: return KEY_F5;
		case SDL_SCANCODE_F6: return KEY_F6;
		case SDL_SCANCODE_F7: return KEY_F7;
		case SDL_SCANCODE_F8: return KEY_F8;
		case SDL_SCANCODE_F9: return KEY_F9;
		case SDL_SCANCODE_F10: return KEY_F10;
		case SDL_SCANCODE_F11: return KEY_F11;
		case SDL_SCANCODE_F12: return KEY_F12;
		case SDL_SCANCODE_A: return KEY_A;
		case SDL_SCANCODE_B: return KEY_B;
		case SDL_SCANCODE_C: return KEY_C;
		case SDL_SCANCODE_D: return KEY_D;
		case SDL_SCANCODE_E: return KEY_E;
		case SDL_SCANCODE_F: return KEY_F;
		case SDL_SCANCODE_G: return KEY_G;
		case SDL_SCANCODE_H: return KEY_H;
		case SDL_SCANCODE_I: return KEY_I;
		case SDL_SCANCODE_J: return KEY_J;
		case SDL_SCANCODE_K: return KEY_K;
		case SDL_SCANCODE_L: return KEY_L;
		case SDL_SCANCODE_M: return KEY_M;
		case SDL_SCANCODE_N: return KEY_N;
		case SDL_SCANCODE_O: return KEY_O;
		case SDL_SCANCODE_P: return KEY_P;
		case SDL_SCANCODE_Q: return KEY_Q;
		case SDL_SCANCODE_R: return KEY_R;
		case SDL_SCANCODE_S: return KEY_S;
		case SDL_SCANCODE_T: return KEY_T;
		case SDL_SCANCODE_U: return KEY_U;
		case SDL_SCANCODE_V: return KEY_V;
		case SDL_SCANCODE_W: return KEY_W;
		case SDL_SCANCODE_X: return KEY_X;
		case SDL_SCANCODE_Y: return KEY_Y;
		case SDL_SCANCODE_Z: return KEY_Z;
		case SDL_SCANCODE_1: return KEY_1;
		case SDL_SCANCODE_2: return KEY_2;
		case SDL_SCANCODE_3: return KEY_3;
		case SDL_SCANCODE_4: return KEY_4;
		case SDL_SCANCODE_5: return KEY_5;
		case SDL_SCANCODE_6: return KEY_6;
		case SDL_SCANCODE_7: return KEY_7;
		case SDL_SCANCODE_8: return KEY_8;
		case SDL_SCANCODE_9: return KEY_9;
		case SDL_SCANCODE_0: return KEY_0;
		case SDL_SCANCODE_MINUS: return KEY_MINUS;
		case SDL_SCANCODE_EQUALS: return KEY_EQUAL;
		case SDL_SCANCODE_LEFTBRACKET: return KEY_LBRACKET;
		case SDL_SCANCODE_RIGHTBRACKET: return KEY_RBRACKET;
		case SDL_SCANCODE_SEMICOLON: return KEY_SEMICOLON;
		case SDL_SCANCODE_APOSTROPHE: return KEY_APOSTROPHE;
		case SDL_SCANCODE_GRAVE: return KEY_TICK;
		case SDL_SCANCODE_BACKSLASH: return KEY_BACKSLASH;
		case SDL_SCANCODE_COMMA: return KEY_COMMA;
		case SDL_SCANCODE_PERIOD: return KEY_PERIOD;
		case SDL_SCANCODE_SLASH: return KEY_SLASH;
		case SDL_SCANCODE_CAPSLOCK: return KEY_CAPS;
		case SDL_SCANCODE_LCTRL: return KEY_LCTRL;
		case SDL_SCANCODE_LALT: return KEY_LALT;
		case SDL_SCANCODE_LSHIFT: return KEY_LSHIFT;
		case SDL_SCANCODE_RSHIFT: return KEY_RSHIFT;
		case SDL_SCANCODE_UP: return KEY_UP;
		case SDL_SCANCODE_DOWN: return KEY_DOWN;
		case SDL_SCANCODE_LEFT: return KEY_LEFT;
		case SDL_SCANCODE_RIGHT: return KEY_RIGHT;
		case SDL_SCANCODE_RALT: return KEY_RALT;
		case SDL_SCANCODE_RCTRL: return KEY_RCTRL;
		case SDL_SCANCODE_HOME: return KEY_HOME;
		case SDL_SCANCODE_END: return KEY_END;
		case SDL_SCANCODE_PAGEUP: return KEY_PGUP;
		case SDL_SCANCODE_PAGEDOWN: return KEY_PGDN;
		case SDL_SCANCODE_INSERT: return KEY_INS;
		case SDL_SCANCODE_DELETE: return KEY_DEL;
		default: return KEY_NONE;
	}
}

#endif
