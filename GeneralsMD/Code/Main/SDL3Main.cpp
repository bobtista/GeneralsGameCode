/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#if defined(SAGE_USE_SDL3)

#include <SDL3/SDL.h>

#include "Common/GameEngine.h"
#include "Common/GameMemory.h"
#include "Common/version.h"
#include "SDL3GameEngine.h"

namespace
{
	const char * const kWindowTitle = "Command & Conquer Generals Zero Hour";
	const int kDefaultWindowWidth = 800;
	const int kDefaultWindowHeight = 600;
}

int __argc = 0;
char **__argv = NULL;

SDL_Window *TheSDL3Window = NULL;
void *ApplicationHWnd = NULL;

extern Int GameMain();

int main(int argc, char **argv)
{
	__argc = argc;
	__argv = argv;

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
	{
		SDL_Log("SDL_Init failed: %s", SDL_GetError());
		return 1;
	}

	Uint32 windowFlags = SDL_WINDOW_RESIZABLE;
#if defined(__APPLE__)
	windowFlags |= SDL_WINDOW_METAL;
#endif
	int windowW = kDefaultWindowWidth;
	int windowH = kDefaultWindowHeight;
	{
		const SDL_DisplayID primaryDisplay = SDL_GetPrimaryDisplay();
		const SDL_DisplayMode *desktopMode = SDL_GetDesktopDisplayMode(primaryDisplay);
		if (desktopMode != nullptr && desktopMode->w > 0 && desktopMode->h > 0)
		{
			windowW = desktopMode->w;
			windowH = desktopMode->h;
		}
	}
	TheSDL3Window = SDL_CreateWindow(kWindowTitle, windowW, windowH, windowFlags);
	if (TheSDL3Window == NULL)
	{
		SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
		SDL_Quit();
		return 1;
	}
	SDL_SetWindowFullscreenMode(TheSDL3Window, nullptr);
	SDL_SetWindowFullscreen(TheSDL3Window, true);
	SDL_SyncWindow(TheSDL3Window);

	ApplicationHWnd = TheSDL3Window;

	Int result = GameMain();

	SDL_DestroyWindow(TheSDL3Window);
	TheSDL3Window = NULL;
	ApplicationHWnd = NULL;
	SDL_Quit();

	return result;
}

#endif
