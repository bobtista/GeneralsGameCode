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
// FILE: ScoreScreenActions.h /////////////////////////////////////////////////
// Widget-agnostic score screen actions: the terminal effect of each score screen
// button (ok/exit, continue, buddies, save replay, chat/emote send), with no
// GameWindow/gadget coupling. ScoreScreen.cpp's GBM_SELECTED/GEM_EDIT_DONE
// handlers call these instead of duplicating the logic, so a future non-.wnd
// front end (e.g. RmlUi) can call the exact same functions.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "GameClient/GUI/GUICallbacks/Menus/ScoreScreenData.h" // ScoreScreenModeType

class UnicodeString;

namespace ScoreScreenActions
{
	// ButtonOk: close the buddy overlay, pop the score screen, clear the campaign, and
	// stop simulation replay playback if one was running.
	void pressOk();

	// ButtonContinue: quits out of simulation replay playback if one is running; otherwise
	// marks the continue press (unless we're finishing a campaign) and, in single player
	// with no next map, pops the score screen; in an internet game, opens the match's
	// online view in the default browser. Returns the resulting "continue was pressed" flag.
	Bool pressContinue(ScoreScreenModeType mode, Bool buttonIsFinishCampaign);

	// ButtonBuddy: toggles the buddy list overlay.
	void toggleBuddyOverlay();

	// ButtonSaveReplay: brings up the save-replay popup layout.
	void startSaveReplayFlow();

	// ButtonEmote / TextEntryChat (GEM_EDIT_DONE): sends the trimmed chat text over LAN,
	// as an emote or a normal chat line. No-op if text is empty or there's no LAN session.
	void sendChat(const UnicodeString &text, Bool isEmote);

	// Single player campaign finish, moved out of ScoreScreen.cpp's finishSinglePlayerInit() --
	// see ScoreScreenCampaignFinish. Call once per single player screen entry.
	ScoreScreenCampaignFinish finishSinglePlayer();

	// Blocking campaign-completion movie playback (ScoreScreen.cpp's PlayMovieAndBlock()), with
	// its own transient full-screen canvas window instead of ScoreScreen.cpp's own s_blankLayout,
	// so a non-.wnd front end can call it too. No-op if movieName is empty.
	void playCampaignCompletionMovie(const AsciiString &movieName);
}
