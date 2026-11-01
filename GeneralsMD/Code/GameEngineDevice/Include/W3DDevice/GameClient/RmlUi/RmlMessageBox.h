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
// FILE: RmlMessageBox.h ///////////////////////////////////////////////////////
// RmlUiMessageBoxHook handler backing MessageBoxOk/YesNo/OkCancel/etc. and the
// quit confirmation boxes (see GameWindowManager::gogoMessageBox). Unlike
// RmlOptionsScreen, this is not a single reused RmlScreen: gogoMessageBox can
// be called reentrantly (an Ok callback can itself pop a new message box
// before the first one's own close finishes -- see PopupReplay::reallySaveReplay
// for a real example), so instances are heap-allocated and stacked, each
// owning its own RmlUi document and data model so they render one above the
// other and close independently and in the order they were opened.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameClient/RmlUiMessageBoxHook.h"

namespace Rml { class Context; }

void RegisterRmlMessageBoxHook(Rml::Context *context); ///< RmlUiManager::init()
void UnregisterRmlMessageBoxHook();                    ///< RmlUiManager::shutdown(); closes any boxes still open

// True while at least one message box is open. RmlUiManager::processKey() checks this before
// routing Escape to the current screen's onBack() -- a box on top must swallow Escape instead of
// letting it fall through and cancel whatever it is stacked over (matches the .wnd version, whose
// MessageBoxSystem/QuitMessageBoxSystem never handle Escape at all: no button, no dismissal).
bool AnyRmlMessageBoxOpen();
