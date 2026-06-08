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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

#include "Common/AudioRequest.h"
#include "Common/Debug.h"
#include "Common/GameAudio.h"
#include "Common/GlobalData.h"
#include "GameClient/Display.h"
#include "GameClient/Gadget.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/HeaderTemplate.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Keyboard.h"
#include "GameClient/Mouse.h"
#include "GameClient/ParticleSys.h"
#include "GameClient/Shell.h"
#include "GameClient/View.h"
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
	m_textInputActive(false),
	m_resizePending(false),
	m_pendingWidth(0),
	m_pendingHeight(0),
	m_resizeStableCount(0)
{
}

SDL3GameEngine::~SDL3GameEngine()
{
}

void SDL3GameEngine::init()
{
	m_sdlWindow = TheSDL3Window;
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
	updateTextInputState();

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
			case SDL_EVENT_WINDOW_MOUSE_ENTER:
			case SDL_EVENT_WINDOW_MOUSE_LEAVE:
			case SDL_EVENT_WINDOW_RESIZED:
				handleWindowEvent(event.window);
				break;

			case SDL_EVENT_KEY_DOWN:
			case SDL_EVENT_KEY_UP:
				handleKeyboardEvent(event.key);
				break;

			case SDL_EVENT_TEXT_INPUT:
				handleTextInputEvent(event.text);
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

	// TheSuperHackers @bugfix bobtista 08/06/2026 Sync the engine resolution only once the
	// window size has been stable for a few polls; macOS fullscreen animates through
	// intermediate sizes and applying one mid-transition resets the device on a stale size.
	if (m_resizePending && m_sdlWindow != NULL)
	{
		int curW = 0;
		int curH = 0;
		SDL_GetWindowSize(m_sdlWindow, &curW, &curH);
		if (curW == m_pendingWidth && curH == m_pendingHeight)
		{
			const Int kStablePolls = 8;
			if (++m_resizeStableCount >= kStablePolls)
			{
				m_resizePending = false;
				m_resizeStableCount = 0;
				applyPendingWindowResize();
			}
		}
		else
		{
			m_pendingWidth = curW;
			m_pendingHeight = curH;
			m_resizeStableCount = 0;
		}
	}
}

void SDL3GameEngine::applyPendingWindowResize()
{
	m_resizePending = false;

	// Read the actual, current window content size rather than the resize event's payload: macOS
	// emits several events while a fullscreen transition settles, and we only want the final size.
	int actualW = 0;
	int actualH = 0;
	if (m_sdlWindow != NULL)
	{
		SDL_GetWindowSize(m_sdlWindow, &actualW, &actualH);
	}
	const Int newWidth = actualW;
	const Int newHeight = actualH;

	if (TheDisplay == NULL || newWidth <= 0 || newHeight <= 0)
	{
		return;
	}

	// Nothing to do if the engine is already at this size.
	if ((Int)TheDisplay->getWidth() == newWidth && (Int)TheDisplay->getHeight() == newHeight)
	{
		return;
	}

	DEBUG_LOG(("SDL3GameEngine::applyPendingWindowResize from %dx%d to %dx%d (windowed=%d)",
		TheDisplay->getWidth(), TheDisplay->getHeight(), newWidth, newHeight, TheDisplay->getWindowed()));

	// Engine-side resolution sync only. applyExternalResize updates the render device's view of the
	// resolution and the 2D coordinate range without touching the SDL window (bgfx already tracks
	// the real size). This fixes the mouse mapping (display now equals the window) and the rendered
	// viewport.
	if (!TheDisplay->applyExternalResize(newWidth, newHeight))
	{
		DEBUG_LOG(("SDL3GameEngine::applyPendingWindowResize applyExternalResize FAILED, size unchanged"));
		return;
	}

	if (TheWritableGlobalData != NULL)
	{
		TheWritableGlobalData->m_xResolution = newWidth;
		TheWritableGlobalData->m_yResolution = newHeight;
	}

	// Lightweight, event-loop-safe relayout. NOTE: TheShell->recreateWindowLayouts() and
	// TheInGameUI->recreateControlBar()/refreshCustomUiResources() are deliberately NOT called here -
	// they destroy and rebuild live GameWindow layouts and crash when invoked mid-frame from the
	// event poll (they are only safe from the options-menu GUI flow). onResolutionChanged() and the
	// camera re-default are safe and cover the mouse bounds and camera framing.
	if (TheHeaderTemplateManager != NULL)
	{
		TheHeaderTemplateManager->onResolutionChanged();
	}
	if (TheMouse != NULL)
	{
		TheMouse->onResolutionChanged();
	}
	if (TheTacticalView != NULL)
	{
		TheTacticalView->setDefaultView(
			DEG_TO_RADF(TheGlobalData->m_cameraPitch),
			DEG_TO_RADF(TheGlobalData->m_cameraYaw),
			1.0f);
		TheTacticalView->setZoomToDefault();
	}

	DEBUG_LOG(("SDL3GameEngine::applyPendingWindowResize done at %dx%d", newWidth, newHeight));
}

void SDL3GameEngine::handleKeyboardEvent(const SDL_KeyboardEvent &event)
{
	SDL3Keyboard *keyboard = static_cast<SDL3Keyboard *>(TheKeyboard);
	if (keyboard != NULL)
	{
		keyboard->addSDL3KeyEvent(event);
	}
}

void SDL3GameEngine::handleTextInputEvent(const SDL_TextInputEvent &event)
{
	if (TheWindowManager == NULL || event.text == NULL)
	{
		return;
	}

	GameWindow *window = TheWindowManager->winGetFocus();
	if (window == NULL)
	{
		return;
	}

	const char *text = event.text;
	size_t remaining = SDL_strlen(text);
	while (remaining > 0)
	{
		Uint32 codepoint = SDL_StepUTF8(&text, &remaining);
		if (codepoint == SDL_INVALID_UNICODE_CODEPOINT)
		{
			continue;
		}
		if (codepoint >= 32 || codepoint == '\n')
		{
			TheWindowManager->winSendInputMsg(window, GWM_IME_CHAR, static_cast<WindowMsgData>(codepoint), 0);
		}
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
		if (TheKeyboard != NULL)
		{
			TheKeyboard->resetKeys();
		}
		if (TheMouse != NULL)
		{
			TheMouse->regainFocus();
			if (SDL_GetMouseFocus() == m_sdlWindow)
			{
				TheMouse->onCursorMovedInside();
			}
			else if (TheMouse->isCursorInside())
			{
				TheMouse->onCursorMovedOutside();
			}
		}
	}
	else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST)
	{
		setIsActive(false);
		if (TheKeyboard != NULL)
		{
			TheKeyboard->resetKeys();
		}
		if (TheMouse != NULL)
		{
			TheMouse->loseFocus();
			if (TheMouse->isCursorInside())
			{
				TheMouse->onCursorMovedOutside();
			}
		}
	}
	else if (event.type == SDL_EVENT_WINDOW_MOUSE_ENTER)
	{
		if (TheMouse != NULL)
		{
			TheMouse->onCursorMovedInside();
		}
	}
	else if (event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE)
	{
		if (TheMouse != NULL && TheMouse->isCursorInside())
		{
			TheMouse->onCursorMovedOutside();
		}
	}
	else if (event.type == SDL_EVENT_WINDOW_RESIZED)
	{
		// data1/data2 carry the new window size in logical points, matching the units used by
		// SDL_GetWindowSize() and the mouse mapping. Restart the settle timer so we only apply once
		// the size stops changing.
		m_pendingWidth = event.data1;
		m_pendingHeight = event.data2;
		m_resizePending = true;
		m_resizeStableCount = 0;
	}
}

void SDL3GameEngine::updateTextInputState()
{
	if (m_sdlWindow == NULL)
	{
		return;
	}

	Bool wantsTextInput = FALSE;
	if (TheWindowManager != NULL && isActive())
	{
		GameWindow *focus = TheWindowManager->winGetFocus();
		if (focus != NULL)
		{
			const UnsignedInt style = focus->winGetStyle();
			wantsTextInput = ((style & GWS_ENTRY_FIELD) != 0 || (style & GWS_COMBO_BOX) != 0);
		}
	}

	if (wantsTextInput && !m_textInputActive)
	{
		m_textInputActive = SDL_StartTextInput(m_sdlWindow) ? TRUE : FALSE;
	}
	else if (!wantsTextInput && m_textInputActive)
	{
		SDL_StopTextInput(m_sdlWindow);
		m_textInputActive = FALSE;
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
	// TheSuperHackers @bugfix bobtista 30/04/2026 GGC_NO_AUDIO=1 forces the dummy audio
	// manager even when not headless (useful on any SDL3+OpenAL platform); only truthy
	// values are honored so a leftover GGC_NO_AUDIO=0 does not silently disable audio.
	{
		const char *envVal = std::getenv("GGC_NO_AUDIO");
		if (envVal != NULL && (strcmp(envVal, "1") == 0 || strcasecmp(envVal, "true") == 0))
		{
			dummy = TRUE;
		}
	}
	return NEW OpenALAudioManager(dummy);
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
