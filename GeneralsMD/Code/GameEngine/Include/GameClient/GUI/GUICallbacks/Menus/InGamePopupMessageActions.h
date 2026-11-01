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
// FILE: InGamePopupMessageActions.h /////////////////////////////////////////
// Widget-agnostic script popup message logic: taking over what the script asked for, going modal
// while the game is paused, and dismissing it with OK, Enter or Escape. InGamePopupMessage.cpp's
// .wnd callbacks and RmlInGamePopupScreen call these instead of duplicating the rules.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Lib/BaseType.h"

class GameWindow;

namespace InGamePopupMessageActions
{
	// The popup comes up: copies the script's message into InGamePopupMessageData and, when it paused
	// the game, makes window (the .wnd's parent, or the placeholder standing in for the RmlUi popup)
	// modal so the other windows stop taking the mouse. FALSE if the script has none.
	Bool open( GameWindow *window );

	void ok(); ///< OK button: the script's message is dismissed

	// Enter and Escape dismiss it, on the key coming up. Returns TRUE for both states of those keys,
	// which no one else gets to see.
	Bool key( UnsignedByte key, UnsignedByte state );
}
