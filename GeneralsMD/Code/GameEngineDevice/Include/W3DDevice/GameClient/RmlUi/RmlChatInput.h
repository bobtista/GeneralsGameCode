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
// FILE: RmlChatInput.h //////////////////////////////////////////////////////
// Shared by the chat screens' commit handlers (Enter in the entry, or the send button).
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/ObserverPtr.h>

#include <stdio.h>
#include <vector>

// Ronin @bugfix 31/10/2026 DX9: a chat line's colour from the game (Color, ARGB) as #RRGGBB for the line's style, as the .wnd
// listboxes draw it; the game's plain white comes back empty, so the pane keeps its own colour.
inline Rml::String RmlChatColorHex(int color)
{
	const int rgb = color & 0xFFFFFF;
	if (rgb == 0xFFFFFF)
		return Rml::String();
	char hex[8];
	_snprintf_s(hex, sizeof(hex), _TRUNCATE, "#%02X%02X%02X", (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
	return Rml::String(hex);
}

// Chat entries cleared this frame, cleared again once the commit event has finished (see below).
inline std::vector<Rml::ObserverPtr<Rml::Element>> &RmlPendingChatClears()
{
	static std::vector<Rml::ObserverPtr<Rml::Element>> s_pending;
	return s_pending;
}

// Empties the chat entry now instead of leaving it to the data model. The data view only writes a
// cleared model back to the element on the next update, so a second commit event in the same frame
// would read the old text out of the element again and send the line twice. Works from the
// entry's own "change" event or from a sibling send button's click (the entry is the document's
// input.chat-input).
//
// The entry's data-value controller listens to the same "change" event, and RmlUi runs an
// element's data controllers in attribute-hash order: when it runs after the commit handler it
// writes the event's old "value" back into the model, and the view then puts the sent text back
// into the entry. So the clear is repeated after the event (RmlFlushChatClears(), next frame),
// with a "change" that carries the empty value to the controller; its linebreak is false, so no
// commit handler takes it for Enter.
inline void RmlClearChatInput(Rml::Event &ev)
{
	Rml::Element *element = ev.GetCurrentElement();
	if (element && element->GetTagName() != "input")
	{
		Rml::ElementDocument *document = element->GetOwnerDocument();
		element = document ? document->QuerySelector("input.chat-input") : nullptr;
	}
	if (Rml::ElementFormControl *control = rmlui_dynamic_cast<Rml::ElementFormControl *>(element))
	{
		control->SetValue(Rml::String());
		RmlPendingChatClears().push_back(control->GetObserverPtr());
	}
}

// RmlUiManager::shutdown(): the observers reference RmlUi pool memory that Rml::Shutdown() frees, so
// they have to go first, not when this static is destroyed at process exit.
inline void RmlDropChatClears()
{
	RmlPendingChatClears().clear();
}

// Once per frame from RmlUiManager::update(), so after the commit event has run to its end.
inline void RmlFlushChatClears()
{
	std::vector<Rml::ObserverPtr<Rml::Element>> pending;
	pending.swap(RmlPendingChatClears());
	for (Rml::ObserverPtr<Rml::Element> &observer : pending)
	{
		Rml::ElementFormControl *control = rmlui_dynamic_cast<Rml::ElementFormControl *>(observer.get());
		if (!control)
			continue;
		control->SetValue(Rml::String());
		Rml::Dictionary parameters;
		parameters["value"] = Rml::String();
		parameters["linebreak"] = false;
		control->DispatchEvent(Rml::EventId::Change, parameters);
	}
}
