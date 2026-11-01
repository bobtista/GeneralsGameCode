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

// Ronin @feature 29/10/2026 DX9: the shell screens RmlUi draws in place of their .wnd. After tintinhamans' registry
// (GeneralsGameCode, arctic/go/feat/rmlui), cut to what the main menu, the game menu and Options need.
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "GameClient/RmlUiScreenRegistry.h"

#include "Common/AsciiString.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/RmlUiMessageBoxHook.h"
#include "GameClient/Shell.h"
#include "GameClient/WindowLayout.h"

#include <vector>

namespace
{
	struct Entry
	{
		const char *wndPath;
		RmlUiScreenHooks hooks;
		unsigned order;		// when it last opened: the newest open screen takes Escape
	};

	unsigned s_openCount = 0;

	std::vector<Entry> &entries()
	{
		static std::vector<Entry> s_entries;
		return s_entries;
	}

	Entry *find(const AsciiString &wndPath)
	{
		std::vector<Entry> &e = entries();
		for (size_t i = 0; i < e.size(); ++i)
			if (wndPath.compareNoCase(e[i].wndPath) == 0)
				return &e[i];
		return nullptr;
	}

	bool visible(const Entry &e)
	{
		return e.hooks.isVisible && e.hooks.isVisible();
	}

	// Ronin @feature 31/10/2026 DX9: the hidden windows standing in for a screen (wndPath) or a message box (boxId), as his
	// registry keeps them: winDestroy() of one closes what it stands for.
	struct Placeholder
	{
		GameWindow *window;
		const char *wndPath;
		UnsignedInt boxId;
	};

	std::vector<Placeholder> &placeholders()
	{
		static std::vector<Placeholder> s_placeholders;
		return s_placeholders;
	}

	// The newest visible capturing screen, or null.
	const Entry *topCapturing()
	{
		const Entry *top = nullptr;
		const std::vector<Entry> &e = entries();
		for (size_t i = 0; i < e.size(); ++i)
			if (e[i].hooks.capturesInput && visible(e[i]) && (!top || e[i].order > top->order))
				top = &e[i];
		return top;
	}

	// GWM_DESTROY reaches the placeholder from the destroy list a frame after winDestroy(); winGetUserData() names the
	// screen and must be cleared by then (processDestroyList asserts it).
	WindowMsgHandledType placeholderSystem(GameWindow *window, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2)
	{
		if (msg == GWM_DESTROY)
		{
			const char *wndPath = (const char *)window->winGetUserData();
			window->winSetUserData(nullptr);
			Entry *e = wndPath ? find(AsciiString(wndPath)) : nullptr;
			if (e && e->hooks.destroyed)
				e->hooks.destroyed();
			return MSG_HANDLED;
		}
		return GameWinDefaultSystem(window, msg, mData1, mData2);
	}

	void layoutInit(WindowLayout *layout, void *userData)
	{
		layout->hide(FALSE);	// routed: opens the screen
	}

	void layoutUpdate(WindowLayout *layout, void *userData)
	{
		Entry *e = find(layout->getFilename());
		if (e && e->hooks.update)
			e->hooks.update();
	}

	// No closing animation to wait for: the shell's push or pop completes now.
	void layoutShutdown(WindowLayout *layout, void *userData)
	{
		layout->hide(TRUE);
		if (TheShell && TheShell->top() == layout)
			TheShell->shutdownComplete(layout);
	}
}

//-------------------------------------------------------------------------------------------------
void RmlUiScreenRegistry::registerScreen(const char *wndPath, const RmlUiScreenHooks &hooks)
{
	Entry entry = { wndPath, hooks, 0 };
	Entry *existing = find(AsciiString(wndPath));
	if (existing)
		*existing = entry;
	else
		entries().push_back(entry);
}

void RmlUiScreenRegistry::unregisterAll()
{
	entries().clear();
	placeholders().clear();	// Ronin @feature 31/10/2026 DX9: windows destroyed after this close nothing
}

bool RmlUiScreenRegistry::isRegistered(const AsciiString &wndPath)
{
	return find(wndPath) != nullptr;
}

// A screen already up keeps its place: bringForward() and hide(FALSE) reopen it on their way.
bool RmlUiScreenRegistry::open(const AsciiString &wndPath)
{
	Entry *e = find(wndPath);
	if (!e || !e->hooks.open)
		return false;
	if (!visible(*e))
		e->order = ++s_openCount;
	e->hooks.open();
	return true;
}

bool RmlUiScreenRegistry::close(const AsciiString &wndPath)
{
	Entry *e = find(wndPath);
	if (!e || !e->hooks.close)
		return false;
	e->hooks.close();
	return true;
}

bool RmlUiScreenRegistry::isOpen(const AsciiString &wndPath)
{
	const Entry *e = find(wndPath);
	return e && visible(*e);
}

bool RmlUiScreenRegistry::back(const AsciiString &wndPath)
{
	Entry *e = find(wndPath);
	if (!e || !e->hooks.back)
		return false;
	e->hooks.back();
	return true;
}

bool RmlUiScreenRegistry::anyOpen()
{
	const std::vector<Entry> &e = entries();
	for (size_t i = 0; i < e.size(); ++i)
		if (visible(e[i]))
			return true;
	return false;
}

// Ronin @feature 31/10/2026 DX9: overlays over live play take no Escape; a message box takes it without acting, as a .wnd box.
bool RmlUiScreenRegistry::escape()
{
	if (RmlUiMessageBoxHook::isOpen())
		return true;
	const Entry *top = topCapturing();
	if (!top)
		return false;
	if (top->hooks.back)
		top->hooks.back();
	return true;
}

bool RmlUiScreenRegistry::ownsInput()
{
	return topCapturing() != nullptr || RmlUiMessageBoxHook::isOpen();
}

bool RmlUiScreenRegistry::wantsOverlayKeys()
{
	const std::vector<Entry> &e = entries();
	for (size_t i = 0; i < e.size(); ++i)
		if (e[i].hooks.keys && visible(e[i]))
			return true;
	return false;
}

// Nothing is offered while a capturing screen is up: that owns the keyboard.
bool RmlUiScreenRegistry::overlayKey(unsigned char key, unsigned short state)
{
	if (ownsInput())
		return false;
	const Entry *top = nullptr;
	const std::vector<Entry> &e = entries();
	for (size_t i = 0; i < e.size(); ++i)
		if (e[i].hooks.keys && visible(e[i]) && (!top || e[i].order > top->order))
			top = &e[i];
	return top && top->hooks.keys(key, state);
}

bool RmlUiScreenRegistry::routesToRmlUi(const AsciiString &wndPath)
{
	return isRegistered(wndPath);
}

// Only over an RmlUi screen: our layer draws under the 2003 windows, so over a 2003 screen (LAN, online) the box stays .wnd.
bool RmlUiScreenRegistry::routesMessageBoxToRmlUi()
{
	return RmlUiMessageBoxHook::isAvailable() && anyOpen();
}

//-------------------------------------------------------------------------------------------------
// Ronin @feature 31/10/2026 DX9: one hidden window per screen layout, script window or message box, kept in placeholders().
static GameWindow *makePlaceholder(const AsciiString &wndPath, UnsignedInt boxId)
{
	Entry *e = boxId == 0 ? find(wndPath) : nullptr;
	GameWindow *window = TheWindowManager->winCreate(nullptr, WIN_STATUS_HIDDEN, 0, 0, 0, 0, placeholderSystem, nullptr);
	if (window)
	{
		window->winSetUserData((void *)(e ? e->wndPath : nullptr));
		Placeholder p = { window, e ? e->wndPath : nullptr, boxId };
		placeholders().push_back(p);
	}
	return window;
}

WindowLayout *RmlUiScreenRegistry::createLayout(const AsciiString &wndPath)
{
	Entry *e = find(wndPath);
	WindowLayout *layout = newInstance(WindowLayout);
	layout->loadEmpty(wndPath);
	if (GameWindow *placeholder = makePlaceholder(wndPath, 0))
		layout->addWindow(placeholder);
	layout->setInit(layoutInit);
	layout->setUpdate(layoutUpdate);
	layout->setShutdown(layoutShutdown);
	layout->routeToRmlUi(TRUE);
	if (e && e->hooks.created)
		e->hooks.created();
	return layout;
}

GameWindow *RmlUiScreenRegistry::createWindow(const AsciiString &wndPath)
{
	GameWindow *window = makePlaceholder(wndPath, 0);
	if (window)
		open(wndPath);
	return window;
}

WindowLayout *RmlUiScreenRegistry::layoutFor(const AsciiString &wndPath)
{
	const std::vector<Placeholder> &p = placeholders();
	for (size_t i = p.size(); i > 0; --i)
		if (p[i - 1].boxId == 0 && p[i - 1].wndPath && wndPath.compareNoCase(p[i - 1].wndPath) == 0)
			return p[i - 1].window->winGetLayout();
	return nullptr;
}

GameWindow *RmlUiScreenRegistry::createMessageBoxWindow(UnsignedInt boxId)
{
	return makePlaceholder(AsciiString::TheEmptyString, boxId);
}

void RmlUiScreenRegistry::destroyMessageBox(UnsignedInt boxId)
{
	if (boxId == 0)
		return;
	const std::vector<Placeholder> &p = placeholders();
	for (size_t i = 0; i < p.size(); ++i)
		if (p[i].boxId == boxId)
		{
			TheWindowManager->winDestroy(p[i].window);
			return;
		}
}

// A screen the shell already closed stays closed: only one still up goes with its window.
void RmlUiScreenRegistry::windowDestroyed(GameWindow *window)
{
	std::vector<Placeholder> &p = placeholders();
	for (size_t i = 0; i < p.size(); ++i)
	{
		if (p[i].window != window)
			continue;
		const Placeholder gone = p[i];
		p.erase(p.begin() + i);
		if (gone.boxId != 0)
			RmlUiMessageBoxHook::close(gone.boxId);
		else if (gone.wndPath && isOpen(AsciiString(gone.wndPath)))
			close(AsciiString(gone.wndPath));
		return;
	}
}
