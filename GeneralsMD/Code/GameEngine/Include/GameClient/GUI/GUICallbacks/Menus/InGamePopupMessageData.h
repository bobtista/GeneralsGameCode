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
// FILE: InGamePopupMessageData.h ////////////////////////////////////////////
// Widget-agnostic state of the script popup message (InGamePopupMessage.wnd and its RmlUi replacement):
// the text, where the script wants it and in what color, and whether it paused the game. Map scripts
// raise it through InGameUI::popupMessage(); InGamePopupMessageActions copies it in when a screen
// comes up and the screens only draw it.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/UnicodeString.h"
#include "GameClient/Color.h"

struct InGamePopupMessageData
{
	static InGamePopupMessageData &instance()
	{
		static InGamePopupMessageData s_data;
		return s_data;
	}

	UnicodeString m_message;
	Int m_x = 0; ///< screen position of the popup's top left corner, in pixels
	Int m_y = 0;
	Int m_width = 0; ///< in pixels; the height follows from the text
	Color m_textColor = 0xFFFFFFFF;
	Bool m_pause = FALSE; ///< the game is paused until OK and other windows are blocked meanwhile
};
