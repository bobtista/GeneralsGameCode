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
#if defined(_WIN32)
// TheSuperHackers @build bobtista 08/06/2026 strings.h is POSIX; MSVC has no such header.
// strcasecmp lives in <string.h> as _stricmp on Windows.
#include <string.h>
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

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
#include "WW3D2/IRenderBackend.h"
#include "WW3D2/RenderBackend.h"

// TheSuperHackers @feature bobtista 15/06/2026 LOCAL DEV AID (uncommitted): request
// a bgfx scene-framebuffer rebuild so the HDR hotkey can toggle RGBA8<->RGBA16F live.
extern "C" void GGC_RequestBgfxFramebufferRebuild();

extern Mouse *TheMouse;
extern Keyboard *TheKeyboard;

SDL3GameEngine::SDL3GameEngine() :
	m_sdlWindow(NULL),
	m_textInputActive(false),
	m_resizePending(false),
	m_resizeReadyToApply(false),
	m_letterboxActive(false),
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

	// TheSuperHackers @bugfix bobtista 08/06/2026 Apply a settled window resize between
	// frames; the full relayout destroys live GameWindow trees and crashes mid event-poll.
	if (m_resizeReadyToApply)
	{
		m_resizeReadyToApply = false;
		applyPendingWindowResize();
	}

	updatePresentMode();
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
				m_resizeReadyToApply = true;
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

// TheSuperHackers @bugfix bobtista 25/06/2026 Defined in MainMenu.cpp; rebuilds the resolution
// confirmation dialog at the current resolution after a resize settles.
extern void RecreateResolutionDialogIfActive();

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
		// TheSuperHackers @bugfix bobtista 25/06/2026 In exclusive fullscreen, macOS transiently
		// reports the pixel drawable size (not logical points) from SDL_GetWindowSize while the
		// mode-switch settles, which corrupts the UI resolution for ~1s. The requested fullscreen
		// mode is stable from the moment it is set, so prefer it to skip the transient size.
		if (TheDisplay != NULL && !TheDisplay->getWindowed())
		{
			const SDL_DisplayMode *fullscreenMode = SDL_GetWindowFullscreenMode(m_sdlWindow);
			if (fullscreenMode != NULL && fullscreenMode->w > 0 && fullscreenMode->h > 0)
			{
				actualW = fullscreenMode->w;
				actualH = fullscreenMode->h;
			}
		}
	}
	Int newWidth = actualW;
	Int newHeight = actualH;

	// When the backend is letterboxing (multiplayer), the engine renders at the centered content
	// size, not the full window: the camera aspect, 2D coordinate range and mouse bounds must match
	// the content rect, with the bars left to the backend. The backend has already recomputed the
	// content rect this frame (in Begin_Scene), so read it back as the target resolution.
	if (g_renderBackend != NULL && g_renderBackend->Is_Present_Letterbox_Active())
	{
		const Int contentW = g_renderBackend->Get_Present_Content_Width();
		const Int contentH = g_renderBackend->Get_Present_Content_Height();
		if (contentW > 0 && contentH > 0)
		{
			newWidth = contentW;
			newHeight = contentH;
		}
	}

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

	// Full relayout, matching the options-menu resolution-change sequence. This is safe here because
	// applyPendingWindowResize() runs from SDL3GameEngine::update() (between frames), not from the SDL
	// event poll: recreateWindowLayouts()/recreateControlBar() destroy and rebuild live GameWindow
	// trees, which only crashed when invoked mid event-poll.
	if (TheHeaderTemplateManager != NULL)
	{
		TheHeaderTemplateManager->onResolutionChanged();
	}
	if (TheMouse != NULL)
	{
		TheMouse->onResolutionChanged();
	}
	if (TheShell != NULL)
	{
		// TheSuperHackers @bugfix bobtista 09/06/2026 Do not rebuild the shell menu layouts
		// while a real game is running. The menus are hidden behind the match and their backing
		// state (e.g. TheLAN->GetMyGame()) may already be gone, so re-running their init callbacks
		// would dereference null. Starting a multiplayer match enables letterboxing, which lands
		// here mid-match. The shell is relaid out on the resize that fires when the match ends.
		const Bool inRealGame =
			(TheGameLogic != NULL && TheGameLogic->isInGame() && !TheGameLogic->isInShellGame());
		if (!inRealGame)
		{
			TheShell->recreateWindowLayouts();
		}
	}
	if (TheInGameUI != NULL)
	{
		// TheSuperHackers @bugfix bobtista 20/07/2026 Rebuild the whole in-game HUD for the new
		// resolution (control bar, scheme, shortcut bar, radar, and the money/superweapon/timer caches
		// that do not self-correct). Retail never changed resolution mid-match, so this path is new.
		TheInGameUI->onResolutionChanged();
	}
	if (TheTacticalView != NULL)
	{
		TheTacticalView->setDefaultView(
			DEG_TO_RADF(TheGlobalData->m_cameraPitch),
			DEG_TO_RADF(TheGlobalData->m_cameraYaw),
			1.0f);
		TheTacticalView->setZoomToDefault();
	}

	// TheSuperHackers @bugfix bobtista 25/06/2026 The resolution-confirm dialog is a runtime modal,
	// not part of the shell layouts recreated above, so rebuild it here too when it is open -
	// otherwise it stays laid out at the pre-resize resolution and renders mis-scaled.
	RecreateResolutionDialogIfActive();

	DEBUG_LOG(("SDL3GameEngine::applyPendingWindowResize done at %dx%d", newWidth, newHeight));
}

void SDL3GameEngine::updatePresentMode()
{
	// TheSuperHackers @feature bobtista 08/06/2026 Letterboxing the present to a fixed 16:9
	// keeps every player's viewable area identical regardless of window/display aspect; the
	// plumbing is kept intact but is not auto-forced for multiplayer.
	if (g_renderBackend == NULL)
	{
		return;
	}

	const Bool wantLetterbox = FALSE;

	if (wantLetterbox == m_letterboxActive)
	{
		return;
	}

	m_letterboxActive = wantLetterbox;
	g_renderBackend->Set_Present_Letterbox(wantLetterbox != FALSE, 16.0f, 9.0f);

	// Resync the engine display resolution, UI layout and mouse bounds to the new content rect on the
	// next frame, once the backend has recomputed the letterbox layout in Begin_Scene.
	m_resizeReadyToApply = true;
}

void SDL3GameEngine::handleKeyboardEvent(const SDL_KeyboardEvent &event)
{
	SDL3Keyboard *keyboard = static_cast<SDL3Keyboard *>(TheKeyboard);
	if (keyboard != NULL)
	{
		keyboard->addSDL3KeyEvent(event);
	}

	// TheSuperHackers @feature bobtista 15/06/2026 Dev hotkeys to A/B the bgfx post
	// effects live without restarting or editing INI. Hold Ctrl+Alt and press
	// W (wipe), G (color grade), B (bloom), or D (desaturate). Each toggle also
	// sets demonstrative parameter values so the change is clearly visible.
	if (event.down != 0 && event.repeat == 0 && TheWritableGlobalData != NULL
		&& (event.mod & SDL_KMOD_CTRL) != 0 && (event.mod & SDL_KMOD_ALT) != 0)
	{
		GlobalData *gd = TheWritableGlobalData;
		switch (event.scancode)
		{
			case SDL_SCANCODE_W:
				// Cycle: OFF -> static center -> follow mouse -> OFF.
				if (!gd->m_bgfxWipeEnabled)
				{
					gd->m_bgfxWipeEnabled = TRUE;
					gd->m_bgfxWipeFollowMouse = FALSE;
					gd->m_bgfxWipeSplit = 0.5f;
					if (TheInGameUI != NULL) { TheInGameUI->message(L"Wipe ON (center)"); }
				}
				else if (!gd->m_bgfxWipeFollowMouse)
				{
					gd->m_bgfxWipeFollowMouse = TRUE;
					if (TheInGameUI != NULL) { TheInGameUI->message(L"Wipe ON (follow mouse)"); }
				}
				else
				{
					gd->m_bgfxWipeEnabled = FALSE;
					gd->m_bgfxWipeFollowMouse = FALSE;
					if (TheInGameUI != NULL) { TheInGameUI->message(L"Wipe OFF"); }
				}
				break;
			case SDL_SCANCODE_G:
				gd->m_bgfxColorGrade = !gd->m_bgfxColorGrade;
				gd->m_bgfxColorGradeStrength = 1.0f;
				gd->m_bgfxColorGradeTemperature = 2.5f;
				if (TheInGameUI != NULL) { TheInGameUI->message(L"Color Grade %s", gd->m_bgfxColorGrade ? L"ON" : L"OFF"); }
				break;
			case SDL_SCANCODE_B:
				// Bloom is highlight-only and needs HDR to have over-bright content
				// to catch, so enabling bloom also turns HDR on (and rebuilds the
				// framebuffer for the format switch).
				gd->m_bgfxBloom = !gd->m_bgfxBloom;
				gd->m_bgfxBloomIntensity = 0.6f;
				if (gd->m_bgfxBloom && !gd->m_bgfxHdr)
				{
					gd->m_bgfxHdr = TRUE;
					GGC_RequestBgfxFramebufferRebuild();
				}
				if (TheInGameUI != NULL) { TheInGameUI->message(L"Bloom %s  threshold %d  (HDR %s)", gd->m_bgfxBloom ? L"ON" : L"OFF", (Int)(gd->m_bgfxBloomThreshold * 100.0f), gd->m_bgfxHdr ? L"ON" : L"OFF"); }
				break;
			case SDL_SCANCODE_D:
				// Saturation lives in the baseline post chain; make sure that master is
				// on so the toggle is never silently swallowed.
				gd->m_bgfxPostProcessing = TRUE;
				gd->m_bgfxPostSaturation = (gd->m_bgfxPostSaturation > 0.5f) ? 0.0f : 1.015f;
				if (TheInGameUI != NULL) { TheInGameUI->message(L"Desaturate %s", (gd->m_bgfxPostSaturation < 0.5f) ? L"ON" : L"OFF"); }
				break;
			case SDL_SCANCODE_H:
				// HDR changes the scene render-target format, so toggling it requires
				// rebuilding the framebuffer (unlike the per-frame shader effects).
				gd->m_bgfxHdr = !gd->m_bgfxHdr;
				GGC_RequestBgfxFramebufferRebuild();
				if (TheInGameUI != NULL) { TheInGameUI->message(L"HDR %s", gd->m_bgfxHdr ? L"ON" : L"OFF"); }
				break;
			case SDL_SCANCODE_MINUS:
				// Lower the bloom threshold (more blooms) to find the sweet spot live.
				gd->m_bgfxBloomThreshold = (gd->m_bgfxBloomThreshold > 0.15f) ? (gd->m_bgfxBloomThreshold - 0.05f) : 0.10f;
				if (TheInGameUI != NULL) { TheInGameUI->message(L"Bloom threshold %d", (Int)(gd->m_bgfxBloomThreshold * 100.0f)); }
				break;
			case SDL_SCANCODE_EQUALS:
				// Raise the bloom threshold (less blooms).
				gd->m_bgfxBloomThreshold = gd->m_bgfxBloomThreshold + 0.05f;
				if (TheInGameUI != NULL) { TheInGameUI->message(L"Bloom threshold %d", (Int)(gd->m_bgfxBloomThreshold * 100.0f)); }
				break;
			case SDL_SCANCODE_V:
				gd->m_bgfxVignette = !gd->m_bgfxVignette;
				gd->m_bgfxVignetteStrength = 0.4f;
				if (TheInGameUI != NULL) { TheInGameUI->message(L"Vignette %s", gd->m_bgfxVignette ? L"ON" : L"OFF"); }
				break;
			case SDL_SCANCODE_C:
				gd->m_bgfxChromaticAberration = !gd->m_bgfxChromaticAberration;
				gd->m_bgfxChromaticAberrationAmount = 0.8f;
				if (TheInGameUI != NULL) { TheInGameUI->message(L"Chromatic Aberration %s", gd->m_bgfxChromaticAberration ? L"ON" : L"OFF"); }
				break;
			case SDL_SCANCODE_F:
				gd->m_bgfxFilmGrain = !gd->m_bgfxFilmGrain;
				gd->m_bgfxFilmGrainStrength = 0.1f;
				if (TheInGameUI != NULL) { TheInGameUI->message(L"Film Grain %s", gd->m_bgfxFilmGrain ? L"ON" : L"OFF"); }
				break;
			case SDL_SCANCODE_O:
				// SSAO allocates depth + AO targets at framebuffer-creation time, so
				// toggling it needs a rebuild like HDR.
				gd->m_bgfxSSAO = !gd->m_bgfxSSAO;
				GGC_RequestBgfxFramebufferRebuild();
				if (TheInGameUI != NULL) { TheInGameUI->message(L"SSAO %s", gd->m_bgfxSSAO ? L"ON" : L"OFF"); }
				break;
			case SDL_SCANCODE_M:
				// Cycle scene MSAA 0 -> 2 -> 4 -> 8; the sample count is read at
				// framebuffer creation, so rebuild on change.
				if (gd->m_bgfxMsaa >= 8) { gd->m_bgfxMsaa = 0; }
				else if (gd->m_bgfxMsaa >= 4) { gd->m_bgfxMsaa = 8; }
				else if (gd->m_bgfxMsaa >= 2) { gd->m_bgfxMsaa = 4; }
				else { gd->m_bgfxMsaa = 2; }
				GGC_RequestBgfxFramebufferRebuild();
				if (TheInGameUI != NULL)
				{
					if (gd->m_bgfxMsaa > 0) { TheInGameUI->message(L"MSAA %dx", gd->m_bgfxMsaa); }
					else { TheInGameUI->message(L"MSAA OFF"); }
				}
				break;
			case SDL_SCANCODE_R:
				// Cycle render scale 1.0 -> 1.25 -> 1.5 -> 2.0; read at framebuffer
				// creation, so rebuild on change.
				if (gd->m_bgfxRenderScale >= 1.99f) { gd->m_bgfxRenderScale = 1.0f; }
				else if (gd->m_bgfxRenderScale >= 1.49f) { gd->m_bgfxRenderScale = 2.0f; }
				else if (gd->m_bgfxRenderScale >= 1.24f) { gd->m_bgfxRenderScale = 1.5f; }
				else { gd->m_bgfxRenderScale = 1.25f; }
				GGC_RequestBgfxFramebufferRebuild();
				if (TheInGameUI != NULL) { TheInGameUI->message(L"Render scale %d%%", (Int)(gd->m_bgfxRenderScale * 100.0f + 0.5f)); }
				break;
			case SDL_SCANCODE_P:
				gd->m_bgfxSpecular = !gd->m_bgfxSpecular;
				gd->m_bgfxSpecularStrength = 3.0f;
				if (TheInGameUI != NULL) { TheInGameUI->message(L"Specular %s", gd->m_bgfxSpecular ? L"ON" : L"OFF"); }
				break;
			case SDL_SCANCODE_L:
				gd->m_bgfxRimLight = !gd->m_bgfxRimLight;
				gd->m_bgfxRimStrength = 0.3f;
				gd->m_bgfxRimPower = 3.0f;
				if (TheInGameUI != NULL) { TheInGameUI->message(L"Rim Light %s", gd->m_bgfxRimLight ? L"ON" : L"OFF"); }
				break;
			case SDL_SCANCODE_K:
				gd->m_bgfxEmissiveBoost = !gd->m_bgfxEmissiveBoost;
				gd->m_bgfxEmissiveBoostScale = 2.0f;
				if (TheInGameUI != NULL) { TheInGameUI->message(L"Emissive Boost %s", gd->m_bgfxEmissiveBoost ? L"ON" : L"OFF"); }
				break;
			case SDL_SCANCODE_Y:
				// Sun shadow map allocates a light-POV depth target at framebuffer
				// creation, so toggling it needs a rebuild like HDR/SSAO.
				gd->m_bgfxShadowMaps = !gd->m_bgfxShadowMaps;
				GGC_RequestBgfxFramebufferRebuild();
				if (TheInGameUI != NULL) { TheInGameUI->message(L"Sun Shadows %s", gd->m_bgfxShadowMaps ? L"ON" : L"OFF"); }
				break;
			case SDL_SCANCODE_T:
				// Force nearest/point texture filtering (old blocky look) vs the smooth
				// linear/trilinear renderer baseline. Applies at bind time, no rebuild.
				gd->m_bgfxPointFilter = !gd->m_bgfxPointFilter;
				if (TheInGameUI != NULL) { TheInGameUI->message(L"Texture Filter: %s", gd->m_bgfxPointFilter ? L"Blocky (point)" : L"Smooth (linear)"); }
				break;
			case SDL_SCANCODE_X:
				// Master toggle for the always-on baseline post (FXAA + sharpen +
				// saturation + contrast). Lets the baked-in look be A/B'd as a whole.
				gd->m_bgfxPostProcessing = !gd->m_bgfxPostProcessing;
				if (TheInGameUI != NULL) { TheInGameUI->message(L"Baseline Post %s", gd->m_bgfxPostProcessing ? L"ON" : L"OFF"); }
				break;
			case SDL_SCANCODE_LEFTBRACKET:
				gd->m_bgfxPostProcessing = TRUE;
				gd->m_bgfxPostFxaaAmount = (gd->m_bgfxPostFxaaAmount > 0.05f) ? (gd->m_bgfxPostFxaaAmount - 0.05f) : 0.0f;
				if (TheInGameUI != NULL) { TheInGameUI->message(L"FXAA %d%%", (Int)(gd->m_bgfxPostFxaaAmount * 100.0f + 0.5f)); }
				break;
			case SDL_SCANCODE_RIGHTBRACKET:
				gd->m_bgfxPostProcessing = TRUE;
				gd->m_bgfxPostFxaaAmount = (gd->m_bgfxPostFxaaAmount < 0.95f) ? (gd->m_bgfxPostFxaaAmount + 0.05f) : 1.0f;
				if (TheInGameUI != NULL) { TheInGameUI->message(L"FXAA %d%%", (Int)(gd->m_bgfxPostFxaaAmount * 100.0f + 0.5f)); }
				break;
			case SDL_SCANCODE_SEMICOLON:
				gd->m_bgfxPostProcessing = TRUE;
				gd->m_bgfxPostSharpenAmount = (gd->m_bgfxPostSharpenAmount > 0.02f) ? (gd->m_bgfxPostSharpenAmount - 0.02f) : 0.0f;
				if (TheInGameUI != NULL) { TheInGameUI->message(L"Sharpen %d%%", (Int)(gd->m_bgfxPostSharpenAmount * 100.0f + 0.5f)); }
				break;
			case SDL_SCANCODE_APOSTROPHE:
				gd->m_bgfxPostProcessing = TRUE;
				gd->m_bgfxPostSharpenAmount = (gd->m_bgfxPostSharpenAmount < 0.98f) ? (gd->m_bgfxPostSharpenAmount + 0.02f) : 1.0f;
				if (TheInGameUI != NULL) { TheInGameUI->message(L"Sharpen %d%%", (Int)(gd->m_bgfxPostSharpenAmount * 100.0f + 0.5f)); }
				break;
			default:
				break;
		}
	}

	// TheSuperHackers @bugfix bobtista 09/06/2026 SDL does not emit a TEXT_INPUT event for Return,
	// and the text-entry gadget only commits on GWM_IME_CHAR(VK_RETURN) - the form the Windows IME
	// delivers. Bridge Return here so chat and other text fields submit on Enter. Only text-entry
	// gadgets act on GWM_IME_CHAR(VK_RETURN); the regular key path still drives buttons and lists.
	if (event.down != 0 && event.repeat == 0 && TheWindowManager != NULL &&
		(event.scancode == SDL_SCANCODE_RETURN || event.scancode == SDL_SCANCODE_KP_ENTER))
	{
		GameWindow *focus = TheWindowManager->winGetFocus();
		if (focus != NULL)
		{
			TheWindowManager->winSendInputMsg(focus, GWM_IME_CHAR, VK_RETURN, 0);
		}
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
