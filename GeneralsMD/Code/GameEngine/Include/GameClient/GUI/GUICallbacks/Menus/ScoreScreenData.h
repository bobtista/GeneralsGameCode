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
// FILE: ScoreScreenData.h ////////////////////////////////////////////////////
// Widget-agnostic score screen content: what the score screen displays for every
// mode (single player, skirmish, LAN, internet, replay), with no GameWindow/gadget
// coupling. ScoreScreen.cpp's grabMultiPlayerInfo()/grabSinglePlayerInfo() build
// this once per screen entry and fill their .wnd gadgets from it, so a future
// non-.wnd front end (e.g. RmlUi) can build the same data and render it itself.
//
// Building this data can have the same side effects ScoreScreen.cpp has always had
// on screen entry (battle honor writes, online stats posting): call build* exactly
// once per screen entry, same as today.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/AsciiString.h"
#include "Common/UnicodeString.h"
#include "GameClient/Color.h"

#include <vector>

class Image;
class Player;

// Aggregated score totals for one player, or (single player only) for a group of
// allied/enemy AI opponents shown as a single combined row.
struct ScoreGather
{
	Int m_totalMoneyEarned = 0;
	Int m_totalMoneySpent = 0;
	Int m_totalUnitsDestroyed = 0;
	Int m_totalUnitsBuilt = 0;
	Int m_totalUnitsLost = 0;
	Int m_totalBuildingsDestroyed = 0;
	Int m_totalBuildingsBuilt = 0;
	Int m_totalBuildingsLost = 0;
	const Image *m_sideImage = nullptr;
};

// One row of the score screen's player list: a real player, an observer, or (single
// player only) an aggregated allied/enemy side.
struct ScoreScreenPlayerRow
{
	UnicodeString m_displayName;
	Color m_textColor = 0;
	Bool m_isObserver = FALSE;
	ScoreGather m_stats;

	// True for the row backed by ThePlayerList->getLocalPlayer() (never true for the
	// "observer watching a human" substitute row in single player). Gates the academy
	// advice listbox's visibility in populatePlayerInfo() -- unhidden whenever this is the
	// local player's row, even with an empty m_academyAdvice (e.g. single player campaign).
	Bool m_isLocalPlayerRow = FALSE;

	// Winner/side icon. m_touchSideIcon mirrors the source code's own inconsistency between
	// populatePlayerInfo() (always unhides the icon gadget) and populateSideInfo() (only
	// touches it when there is an image, otherwise leaves the gadget at its .wnd default):
	// false means "don't touch the icon gadget at all", matching populateSideInfo() exactly.
	const Image *m_sideIconImage = nullptr;
	Bool m_touchSideIcon = FALSE;

	// The general's badge (PlayerTemplate GeneralImage, else the side icon), for front ends that
	// draw a faction badge per row.
	const Image *m_badgeImage = nullptr;

	// Multiplayer rows only: this player and his allies won (VictoryConditions::hasAchievedVictory).
	Bool m_isVictor = FALSE;

	// Academy advice tips; non-empty only for the local player's row, and only in
	// skirmish/internet modes.
	std::vector<UnicodeString> m_academyAdvice;
};

enum ScoreScreenModeType
{
	SCORESCREENMODE_SINGLEPLAYER = 0,
	SCORESCREENMODE_SKIRMISH,
	SCORESCREENMODE_LAN,
	SCORESCREENMODE_INTERNET,
	SCORESCREENMODE_REPLAY
};

struct ScoreScreenData
{
	ScoreScreenModeType m_mode = SCORESCREENMODE_SINGLEPLAYER;
	std::vector<ScoreScreenPlayerRow> m_rows;

	// Score screen backdrop image (per side, or the generic multiplayer backdrop).
	const Image *m_backgroundImage = nullptr;
	Bool m_hasBackgroundImage = FALSE;

	// The match as a whole: the local side's outcome (neither for an observer or an unfinished
	// game), the map's display name as the map list shows it (empty when the map cache has no
	// entry) and how long the game ran, in seconds, up to the end frame.
	Bool m_localVictory = FALSE;
	Bool m_localDefeat = FALSE;
	UnicodeString m_mapDisplayName;
	UnsignedInt m_gameSeconds = 0;

	// Multiplayer (skirmish/LAN/internet/replay): one row per occupied slot, sorted by
	// score descending, observers appended with no stats. mode selects the per-mode
	// side effects populatePlayerInfo() used to run inline (skirmish battle honors,
	// internet stats posting). Moved from ScoreScreen.cpp's
	// grabMultiPlayerInfo()/populatePlayerInfo().
	static ScoreScreenData buildForMultiPlayer(ScoreScreenModeType mode);

	// Single player (campaign/challenge, and single player replay): the local player's
	// row, followed by one aggregated row per allied/enemy side that has participants.
	// Moved from ScoreScreen.cpp's grabSinglePlayerInfo().
	static ScoreScreenData buildForSinglePlayer();

	// Whether there is another queued simulation replay to advance to. Moved from
	// ScoreScreen.cpp's showReplayButtonContinue().
	static Bool replayHasMoreEntries();
};

// Per-mode score screen chrome: which optional gadgets (chat, emote, buddies, academy
// panel, continue button) are visible and what the continue button says. Computed once
// per entry alongside ScoreScreenData so a non-.wnd front end doesn't have to replicate
// the mode switch. Doesn't cover single player's post-campaign-completion caption
// changes (ScoreScreen.cpp's finishSinglePlayerInit), which depend on campaign
// progression, not just mode. Runtime-discovered overrides (internet match info from the
// lobby, buddies-button gated on the local GameSpy profile) are applied by mutating the
// fields after forMode() returns; the queries/side effects that produce them stay in
// ScoreScreen.cpp.
struct ScoreScreenLayout
{
	Bool m_showChatEntry = FALSE;
	Bool m_showEmoteButton = FALSE;
	Bool m_showChatBoxBorder = FALSE;
	Bool m_showChatLog = FALSE;
	Bool m_showBuddiesButton = FALSE;
	Bool m_showContinueButton = FALSE;
	UnicodeString m_continueButtonCaption; // empty: keep the .wnd default caption
	Bool m_showSaveGameText = FALSE;

	// Tri-state like ScoreScreenPlayerRow::m_touchSideIcon: skirmish and single player
	// never touch the academy panel gadgets, leaving them at their .wnd default instead
	// of forcing hidden or shown.
	Bool m_touchAcademyPanel = FALSE;
	Bool m_showAcademyPanel = FALSE;

	// Layout for initSkirmish/initLANMultiPlayer/initInternetMultiPlayer/
	// initReplayMultiPlayer/initReplaySinglePlayer (single and multiplayer replay share
	// SCORESCREENMODE_REPLAY's layout).
	static ScoreScreenLayout forMode(ScoreScreenModeType mode);
};

// Result of ScoreScreen.cpp's finishSinglePlayerInit(): the challenge win/loss splash, continue
// caption and save-indicator decisions made after single player campaign scoring, with no
// GameWindow coupling, so a non-.wnd front end can render the same thing. Building this (via
// ScoreScreenActions::finishSinglePlayer()) runs the same side effects finishSinglePlayerInit()
// always has (gotoNextMission(), battle honor writes, autosave, win/loss sound) -- call it exactly
// once per single player screen entry. Movie playback isn't included here: if
// m_campaignCompletionMovie is non-empty, also call ScoreScreenActions::playCampaignCompletionMovie()
// with it (same useLowRes gate finishSinglePlayerInit() always applied).
struct ScoreScreenCampaignFinish
{
	Bool m_victorious = FALSE;

	// Challenge win/loss splash (ScoreScreen.cpp's displayChallengeWinLoss()); other
	// m_challenge* fields are default/unused when this is false.
	Bool m_showChallengeSplash = FALSE;
	const Image *m_challengePortrait = nullptr;
	UnicodeString m_challengeHeaderText;
	UnicodeString m_challengeRemarksText;

	// True once the whole (non-challenge) campaign is complete. Passed to
	// ScoreScreenActions::pressContinue() as buttonIsFinishCampaign.
	Bool m_campaignComplete = FALSE;

	UnicodeString m_continueButtonCaption; // empty: keep the .wnd default "Continue" caption
	Bool m_showSaveGameText = FALSE;

	// Non-empty only when the campaign just completed and has a final victory movie that isn't
	// LOD-gated away.
	AsciiString m_campaignCompletionMovie;
};
