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
#include "PreRTS.h"
#include "GameClient/RmlUiMessageBoxHook.h"

namespace
{
	RmlUiMessageBoxFunc s_handler = nullptr;
	RmlUiMessageBoxCloseFunc s_closeHandler = nullptr;
	RmlUiMessageBoxRaiseFunc s_raiseHandler = nullptr;
	RmlUiMessageBoxOpenFunc s_openQuery = nullptr;
	RmlUiMessageBoxLabels s_pendingLabels;
}

void RmlUiMessageBoxHook::setHandler(RmlUiMessageBoxFunc handler)
{
	s_handler = handler;
}

void RmlUiMessageBoxHook::setCloseHandler(RmlUiMessageBoxCloseFunc closeHandler)
{
	s_closeHandler = closeHandler;
}

void RmlUiMessageBoxHook::setRaiseHandler(RmlUiMessageBoxRaiseFunc raiseHandler)
{
	s_raiseHandler = raiseHandler;
}

void RmlUiMessageBoxHook::setOpenQuery(RmlUiMessageBoxOpenFunc openQuery)
{
	s_openQuery = openQuery;
}

void RmlUiMessageBoxHook::setPendingLabels(const RmlUiMessageBoxLabels &labels)
{
	s_pendingLabels = labels;
}

void RmlUiMessageBoxHook::clearPendingLabels()
{
	s_pendingLabels = RmlUiMessageBoxLabels();
}

bool RmlUiMessageBoxHook::isAvailable()
{
	return s_handler != nullptr;
}

UnsignedInt RmlUiMessageBoxHook::show(UnsignedShort buttonFlags, const UnicodeString &titleString, const UnicodeString &bodyString,
	GameWinMsgBoxFunc yesCallback, GameWinMsgBoxFunc noCallback,
	GameWinMsgBoxFunc okCallback, GameWinMsgBoxFunc cancelCallback,
	Bool useLogo)
{
	const RmlUiMessageBoxLabels labels = s_pendingLabels;
	clearPendingLabels();
	if (s_handler)
		return s_handler(buttonFlags, titleString, bodyString, yesCallback, noCallback, okCallback, cancelCallback, useLogo, labels);
	return 0;
}

void RmlUiMessageBoxHook::closeCurrent()
{
	close(0);
}

void RmlUiMessageBoxHook::close(UnsignedInt id)
{
	if (s_closeHandler)
		s_closeHandler(id);
}

void RmlUiMessageBoxHook::raise()
{
	if (s_raiseHandler)
		s_raiseHandler();
}

bool RmlUiMessageBoxHook::isOpen()
{
	return s_openQuery && s_openQuery();
}
