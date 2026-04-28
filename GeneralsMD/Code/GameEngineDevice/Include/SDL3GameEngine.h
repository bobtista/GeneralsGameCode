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

#include "Common/GameEngine.h"

extern SDL_Window *TheSDL3Window;

class SDL3GameEngine : public GameEngine
{
public:
	SDL3GameEngine();
	virtual ~SDL3GameEngine() override;

	virtual void init() override;
	virtual void reset() override;
	virtual void update() override;
	virtual void serviceWindowsOS() override;
	virtual Bool isActive() override;
	virtual void setIsActive(Bool isActive) override;

protected:
	virtual GameLogic *createGameLogic() override;
	virtual GameClient *createGameClient() override;
	virtual ModuleFactory *createModuleFactory() override;
	virtual ThingFactory *createThingFactory() override;
	virtual FunctionLexicon *createFunctionLexicon() override;
	virtual LocalFileSystem *createLocalFileSystem() override;
	virtual ArchiveFileSystem *createArchiveFileSystem() override;
	virtual NetworkInterface *createNetwork();
	virtual Radar *createRadar(Bool dummy) override;
	virtual WebBrowser *createWebBrowser() override;
	virtual AudioManager *createAudioManager(Bool dummy) override;
	virtual ParticleSystemManager *createParticleSystemManager(Bool dummy) override;

private:
	void pollSDL3Events();
	void handleKeyboardEvent(const SDL_KeyboardEvent &event);
	void handleMouseMotionEvent(const SDL_MouseMotionEvent &event);
	void handleMouseButtonEvent(const SDL_MouseButtonEvent &event);
	void handleMouseWheelEvent(const SDL_MouseWheelEvent &event);
	void handleWindowEvent(const SDL_WindowEvent &event);

	SDL_Window *m_sdlWindow;
	Bool m_isInitialized;
};

#endif
