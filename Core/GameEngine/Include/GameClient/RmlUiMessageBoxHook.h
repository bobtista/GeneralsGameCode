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

// Ronin @feature 31/10/2026 DX9: tintinhamans' (GeneralsGameCode, arctic/go/feat/rmlui), ported as he wrote it.
// FILE: RmlUiMessageBoxHook.h ///////////////////////////////////////////////
// Single function-pointer hook GameWindowManager::gogoMessageBox() calls
// instead of depending on GameEngineDevice/RmlUi directly -- same layering
// reasoning as RmlUiScreenRegistry.h, but parameterized (title/body/buttons/
// callbacks) instead of .wnd-path-keyed, since every message box shares one
// document and one caller-facing signature. GameEngineDevice's RmlUiManager
// sets the handler at init() and clears it at shutdown(); it stays unset on
// targets that don't link RmlUi, so isAvailable() is always safe to call.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameClient/GameWindow.h" // GameWinMsgBoxFunc, MSG_BOX_*
#include "Common/UnicodeString.h"

// Button text overrides; an empty entry keeps the default GUI:Ok/Yes/No/Cancel label.
struct RmlUiMessageBoxLabels
{
	UnicodeString ok, yes, no, cancel;
};

typedef UnsignedInt (*RmlUiMessageBoxFunc)(UnsignedShort buttonFlags,
	const UnicodeString &titleString, const UnicodeString &bodyString,
	GameWinMsgBoxFunc yesCallback, GameWinMsgBoxFunc noCallback,
	GameWinMsgBoxFunc okCallback, GameWinMsgBoxFunc cancelCallback,
	Bool useLogo, const RmlUiMessageBoxLabels &labels);
typedef void (*RmlUiMessageBoxCloseFunc)(UnsignedInt id); ///< id 0 = whichever box is open
typedef void (*RmlUiMessageBoxRaiseFunc)();
typedef bool (*RmlUiMessageBoxOpenFunc)();

//-------------------------------------------------------------------------------------------------
class RmlUiMessageBoxHook
{
public:
	static void setHandler(RmlUiMessageBoxFunc handler);
	static void setCloseHandler(RmlUiMessageBoxCloseFunc closeHandler);
	static void setRaiseHandler(RmlUiMessageBoxRaiseFunc raiseHandler);
	static void setOpenQuery(RmlUiMessageBoxOpenFunc openQuery);
	static bool isAvailable();
	static void setPendingLabels(const RmlUiMessageBoxLabels &labels); ///< applied to the next show() only
	static void clearPendingLabels(); ///< drops labels no show() consumed (legacy .wnd path)
	static UnsignedInt show(UnsignedShort buttonFlags, const UnicodeString &titleString, const UnicodeString &bodyString,
		GameWinMsgBoxFunc yesCallback, GameWinMsgBoxFunc noCallback,
		GameWinMsgBoxFunc okCallback, GameWinMsgBoxFunc cancelCallback,
		Bool useLogo); ///< returns the box id (0 if !isAvailable())
	static void closeCurrent(); ///< dismiss the open box without running its callback; no-op if none open.
	static void close(UnsignedInt id); ///< same, but only if box id is the one still open
	static void raise(); ///< pull the open box above every other RmlUi document
	static bool isOpen(); ///< a box is showing right now (RmlUi's own state)
};
