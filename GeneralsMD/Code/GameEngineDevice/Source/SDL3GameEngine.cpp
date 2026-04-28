/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#include "SDL3GameEngine.h"

#if defined(SAGE_USE_SDL3)

#include "Common/AudioRequest.h"
#include "Common/GameAudio.h"
#include "GameClient/Keyboard.h"
#include "GameClient/Mouse.h"
#include "GameClient/ParticleSys.h"
#include "GameLogic/GameLogic.h"
#include "GameNetwork/NetworkInterface.h"
#if defined(SAGE_USE_OPENAL)
#include "OpenALAudioDevice/OpenALAudioManager.h"
#endif
#include "SDL3Device/GameClient/SDL3Keyboard.h"
#include "SDL3Device/GameClient/SDL3Mouse.h"
#include "StdDevice/Common/StdBIGFileSystem.h"
#include "StdDevice/Common/StdLocalFileSystem.h"
#include "W3DDevice/Common/W3DFunctionLexicon.h"
#include "W3DDevice/Common/W3DModuleFactory.h"
#include "W3DDevice/Common/W3DRadar.h"
#include "W3DDevice/Common/W3DThingFactory.h"
#include "W3DDevice/GameClient/W3DGameClient.h"
#include "W3DDevice/GameClient/W3DParticleSys.h"
#include "W3DDevice/GameLogic/W3DGameLogic.h"

extern Mouse *TheMouse;
extern Keyboard *TheKeyboard;

SDL3GameEngine::SDL3GameEngine() :
	m_sdlWindow(NULL),
	m_isInitialized(false)
{
}

SDL3GameEngine::~SDL3GameEngine()
{
}

void SDL3GameEngine::init()
{
	m_sdlWindow = TheSDL3Window;
	m_isInitialized = (m_sdlWindow != NULL);
	GameEngine::init();
}

void SDL3GameEngine::reset()
{
	GameEngine::reset();
}

void SDL3GameEngine::update()
{
	pollSDL3Events();
	GameEngine::update();
}

void SDL3GameEngine::serviceWindowsOS()
{
	pollSDL3Events();
}

Bool SDL3GameEngine::isActive()
{
	return m_isActive;
}

void SDL3GameEngine::setIsActive(Bool isActive)
{
	m_isActive = isActive;
}

void SDL3GameEngine::pollSDL3Events()
{
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
			case SDL_EVENT_QUIT:
				setQuitting(true);
				break;

			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
			case SDL_EVENT_WINDOW_FOCUS_GAINED:
			case SDL_EVENT_WINDOW_FOCUS_LOST:
				handleWindowEvent(event.window);
				break;

			case SDL_EVENT_KEY_DOWN:
			case SDL_EVENT_KEY_UP:
				handleKeyboardEvent(event.key);
				break;

			case SDL_EVENT_MOUSE_MOTION:
				handleMouseMotionEvent(event.motion);
				break;

			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
				handleMouseButtonEvent(event.button);
				break;

			case SDL_EVENT_MOUSE_WHEEL:
				handleMouseWheelEvent(event.wheel);
				break;

			default:
				break;
		}
	}
}

void SDL3GameEngine::handleKeyboardEvent(const SDL_KeyboardEvent &event)
{
	SDL3Keyboard *keyboard = static_cast<SDL3Keyboard *>(TheKeyboard);
	if (keyboard != NULL)
	{
		keyboard->addSDL3KeyEvent(event);
	}
}

void SDL3GameEngine::handleMouseMotionEvent(const SDL_MouseMotionEvent &event)
{
	SDL3Mouse *mouse = static_cast<SDL3Mouse *>(TheMouse);
	if (mouse != NULL)
	{
		mouse->addSDL3MotionEvent(event);
	}
}

void SDL3GameEngine::handleMouseButtonEvent(const SDL_MouseButtonEvent &event)
{
	SDL3Mouse *mouse = static_cast<SDL3Mouse *>(TheMouse);
	if (mouse != NULL)
	{
		mouse->addSDL3ButtonEvent(event);
	}
}

void SDL3GameEngine::handleMouseWheelEvent(const SDL_MouseWheelEvent &event)
{
	SDL3Mouse *mouse = static_cast<SDL3Mouse *>(TheMouse);
	if (mouse != NULL)
	{
		mouse->addSDL3WheelEvent(event);
	}
}

void SDL3GameEngine::handleWindowEvent(const SDL_WindowEvent &event)
{
	if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
	{
		setQuitting(true);
	}
	else if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED)
	{
		setIsActive(true);
	}
	else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST)
	{
		setIsActive(false);
	}
}

GameLogic *SDL3GameEngine::createGameLogic()
{
	return NEW W3DGameLogic;
}

GameClient *SDL3GameEngine::createGameClient()
{
	return NEW W3DGameClient;
}

ModuleFactory *SDL3GameEngine::createModuleFactory()
{
	return NEW W3DModuleFactory;
}

ThingFactory *SDL3GameEngine::createThingFactory()
{
	return NEW W3DThingFactory;
}

FunctionLexicon *SDL3GameEngine::createFunctionLexicon()
{
	return NEW W3DFunctionLexicon;
}

LocalFileSystem *SDL3GameEngine::createLocalFileSystem()
{
	return NEW StdLocalFileSystem;
}

ArchiveFileSystem *SDL3GameEngine::createArchiveFileSystem()
{
	return NEW StdBIGFileSystem;
}

NetworkInterface *SDL3GameEngine::createNetwork()
{
	return NetworkInterface::createNetwork();
}

Radar *SDL3GameEngine::createRadar(Bool dummy)
{
	if (dummy)
	{
		return NEW RadarDummy;
	}
	return NEW W3DRadar;
}

WebBrowser *SDL3GameEngine::createWebBrowser()
{
	return NULL;
}

AudioManager *SDL3GameEngine::createAudioManager(Bool dummy)
{
#if defined(SAGE_USE_OPENAL)
    if (!dummy)
    {
        return NEW OpenALAudioManager;
    }
#endif
    return NULL;
}

ParticleSystemManager *SDL3GameEngine::createParticleSystemManager(Bool dummy)
{
	if (dummy)
	{
		return NEW ParticleSystemManagerDummy;
	}
	return NEW W3DParticleSystemManager;
}

GameEngine *CreateGameEngine()
{
	return NEW SDL3GameEngine;
}

BOOL GGC_GetClientRect_SDL3(HWND hwnd, LPRECT rect)
{
	if (rect == nullptr)
	{
		return FALSE;
	}
	rect->left = 0;
	rect->top = 0;
	rect->right = 0;
	rect->bottom = 0;
	if (TheSDL3Window != nullptr)
	{
		int w = 0, h = 0;
		SDL_GetWindowSize(TheSDL3Window, &w, &h);
		rect->right = w;
		rect->bottom = h;
	}
	return TRUE;
}

#endif
