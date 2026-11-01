/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Ronin @feature 28/10/2026 DX9: the RmlUi context over the game - its renderer, files, fonts and input, drawn after the
// 2003 UI and under the cursor (docs/UI_Work.md). Ported from tintinhamans' GeneralsGameCode fork (arctic/go/feat/rmlui).
#pragma once

#include "GameClient/RmlUiInputHook.h"
#include "W3DDevice/GameClient/RmlUi/RmlUiFileInterface.h"
#include "W3DDevice/GameClient/RmlUi/RmlUiRenderInterface.h"
#include "W3DDevice/GameClient/RmlUi/RmlUiSystemInterface.h"

namespace Rml { class Context; class ElementDocument; class ElementInstancer; }

class RmlUiManager : public RmlUiInputHook
{
public:
	static void createInstance();
	static void destroyInstance();	// before the device goes

	void init(int width, int height);	// once the device and the display exist
	void shutdown();

	void update();	// layout, events, animations; nothing while no document is up
	void render();	// follows the display's size, so a resolution change needs no call of its own

	bool hasVisibleDocument() const;
	Rml::Context *getContext() const { return m_context; }

	// Ronin @feature 29/10/2026 DX9: the shell screens' documents (RmlScreen): loaded with the 2003 buttons' click sound,
	// shown over every other document.
	Rml::ElementDocument *loadScreen(const char *path);
	void showScreen(Rml::ElementDocument *document);

	// The panel's `ui` command.
	void toggleTestDocument();
	void reloadDocuments();		// every document and style sheet from disk again, as they were shown
	void toggleDebugger();
	void describe(char *out, int size) const;

	// RmlUiInputHook
	virtual bool wantsMouseInput(int mouseX, int mouseY) const override;
	virtual bool capturesInput() const override;
	virtual bool leavesMiddleToWorld(int mouseX, int mouseY) const override;
	virtual bool wantsKeyboardInput() const override;
	virtual void processMouseMove(int x, int y) override;
	virtual void processMouseButton(int button, bool down) override;
	virtual void processMouseWheel(float delta) override;
	virtual bool processKey(unsigned char engineKey, unsigned short engineKeyState) override;
	virtual bool drawsControlBar() const override { return m_modernUI; }
	virtual void processTextInput(unsigned short utf16Char) override;

private:
	RmlUiManager();
	virtual ~RmlUiManager();

	void loadFonts();
	void registerScreens();				// ModernUI: main menu, game menu, Options in place of their .wnd
	bool screenUp() const;				// a shell screen is shown: it takes the whole mouse, and Escape
	bool wndWindowAt(int x, int y) const;	// a 2003 window over the layer keeps the mouse there
	bool gameHalted() const;				// the in-game menu is up or the game is paused: no clicks for the panels
	bool textFieldFocused() const;
	void endHoverOverWorld();	// the pointer left every document over the world: no element keeps its hover
	// Ronin @feature 30/10/2026 DX9: his data-tooltip / data-tooltip-text over a screen, for the whole row, drawn by Data/UI/Tooltip.rml
	// in the menus' description font once the pointer rests half a second.
	void updateTooltip();
	void hideTooltip();
	Rml::Element *m_tooltipElement = nullptr;			// the element the pointer rests on that carries a tip
	Rml::ElementDocument *m_tooltipDocument = nullptr;
	double m_tooltipSince = 0.0;						// when the pointer came to it
	bool m_tooltipShown = false;

	RmlUiSystemInterface m_systemInterface;
	RmlUiFileInterface m_fileInterface;
	RmlUiRenderInterface m_renderInterface;
	Rml::Context *m_context;
	Rml::ElementDocument *m_testDocument;
	Rml::ElementInstancer *m_gameTextInstancer;	// <gametext>; RmlUi does not own instancers
	Rml::ElementInstancer *m_radarInstancer;		// <radar>
	Rml::ElementInstancer *m_videoInstancer;		// <video>
	Rml::ElementInstancer *m_mappedImageInstancer;	// <mappedimage>
	Rml::ElementInstancer *m_mapPreviewInstancer;	// <mappreview> (Ronin @feature 30/10/2026 DX9: his skirmish screens')
	Rml::ElementInstancer *m_scrollLogInstancer = nullptr;	// <scrolllog> (Ronin @feature 31/10/2026 DX9: his chat logs)
	unsigned short m_pendingHighSurrogate;			// the first half of a WM_CHAR surrogate pair
	bool m_modernUI;								// Options.ini ModernUI, read once at start
	int m_width;
	int m_height;
	bool m_initialized;
	bool m_debuggerInitialized;
	bool m_debuggerVisible;	// ours: the debugger's IsVisible() lags its SetVisible() by a context update
};

extern RmlUiManager *TheRmlUiManager;
