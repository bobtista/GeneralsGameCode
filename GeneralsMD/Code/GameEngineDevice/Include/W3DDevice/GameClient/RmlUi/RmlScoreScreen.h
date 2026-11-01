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
// FILE: RmlScoreScreen.h ///////////////////////////////////////////////////
// RmlScreen for Data/UI/ScoreScreen.rml. Registered for Menus/ScoreScreen.wnd
// (pushed by GameLogicDispatch), so unlike the .wnd version this never gets a
// ScoreScreenInit() WIN_CREATE callback -- open() replicates that function's
// isInReplayGame()/isInInternetGame()/isInLanGame()/isInSkirmishGame() mode
// dispatch itself, then builds ScoreScreenData + ScoreScreenLayout exactly the
// way ScoreScreen.cpp's initX() functions do and binds them into the model.
//
// Scope: covers skirmish/LAN/internet/replay (the modes Generals Online actually
// exercises after a match) and single player, including ScoreScreen.cpp's
// finishSinglePlayerInit() (challenge win/loss splash, campaign-completion movie
// playback, auto-save, Retry/EndCampaign/SaveAndContinue caption switch) --
// see finishSinglePlayerIfNeeded() and ScoreScreenActions::finishSinglePlayer().
// Incoming LAN chat/emotes and disconnect notices reach this screen the same way
// via ScoreScreenSignals::chatLine (see LANAPICallbacks.h/.cpp).
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/Signal.h"
#include "GameClient/GUI/GUICallbacks/Menus/ScoreScreenData.h"
#include "W3DDevice/GameClient/RmlUi/RmlHqStatus.h"
#include "W3DDevice/GameClient/RmlUi/RmlScreen.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

namespace Rml { class Context; class ElementDocument; class Event; }

//-------------------------------------------------------------------------------------------------
// Ronin @feature 31/10/2026 DX9: our RmlScreen differs (open / close), so no base here, as batch 1's screens.
class RmlScoreScreen
{
public:
	static RmlScoreScreen &instance();
	// RmlUiManager::shutdown(): Rml::Shutdown() frees the document and context, and this outlives them.
	void releaseRml() { m_document = nullptr; m_context = nullptr; m_modelHandle = Rml::DataModelHandle(); }

	// RmlScreen ----------------------------------------------------------------------------
	virtual void load(Rml::Context *context);
	virtual void show();
	virtual void hide();
	virtual bool isVisible() const;
	virtual void onBack(); // Escape: same as ButtonOk (see ScoreScreen.cpp's GWM_CHAR/KEY_ESC)
	virtual void update(); // one-shot single player campaign finish, see finishSinglePlayerIfNeeded()

	// Appends a chat/emote/system line to the chat log. Called by the local onSendChat/onSendEmote
	// handlers (so the sender always sees their own line) and by ScoreScreenSignals::chatLine
	// (see LANAPICallbacks.h/.cpp) while this screen is showing, which forwards everything
	// LANAPI::OnChat() would otherwise only write into the .wnd path's listboxChatWindowScoreScreen
	// -- other players' LAN chat/emotes and the WOLGameSetupMenu.cpp/WOLQuickMatchMenu.cpp
	// disconnect notices.
	void appendChatLine(const Rml::String &line, const Rml::String &color = Rml::String());	// Ronin @bugfix 31/10/2026 DX9: color: RmlChatColorHex()

private:
	RmlScoreScreen() {}

	void refreshFromGameState(); // mode dispatch + ScoreScreenData/ScoreScreenLayout -> m_model
	void finishSinglePlayerIfNeeded(); // single player campaign finish, called once from update()
	void markLeaders(); // best_* flags and the highlight tiles from m_model.rows

	void onOk(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &);
	void onContinue(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &);
	void onBuddies(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &);
	void onSaveReplay(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &);
	void onSendChat(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &);
	void onChatEntryCommitted(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &);
	void onSendEmote(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &);

	Rml::Context *m_context = nullptr;
	Rml::ElementDocument *m_document = nullptr;
	Rml::DataModelHandle m_modelHandle;
	RmlHqStatus m_hq; // .hq-header status
	ScoreScreenModeType m_mode = SCORESCREENMODE_SINGLEPLAYER;
	SignalConnection m_chatConnection; // ScoreScreenSignals::chatLine, connected while showing

	// Single player campaign finish (ScoreScreen.cpp's finishSinglePlayerInit()), deferred to the
	// first update() after show() so the screen has a chance to render once first -- same timing
	// as the .wnd path's s_needToFinishSinglePlayerInit/ScoreScreenUpdate().
	bool m_needsFinishSinglePlayer = false;
	// ScoreScreenActions::pressContinue()'s buttonIsFinishCampaign, set by finishSinglePlayerIfNeeded().
	bool m_buttonIsFinishCampaign = false;

	// One row's worth of fields for the data-for-bound player table (data-for/RegisterStruct/
	// RegisterArray -- see load()). Field names/types mirror ScoreScreenPlayerRow.
	struct RowModel
	{
		Rml::String displayName;
		bool isObserver = false;
		int moneyEarned = 0;
		int unitsBuilt = 0;
		int unitsLost = 0;
		int unitsDestroyed = 0;
		int buildingsBuilt = 0;
		int buildingsLost = 0;
		int buildingsDestroyed = 0;
		Rml::String sideIconImage; // mapped image name; empty means no icon
		bool showSideIcon = false;
		Rml::String badgeImage; // the general's badge, empty means none
		Rml::String colorHex; // the player's colour, #RRGGBB
		bool isLocal = false;
		bool isVictor = false;
		// This row leads the game in that stat (ties all lead; zero never does).
		bool bestMoney = false;
		bool bestUnitsBuilt = false;
		bool bestUnitsDestroyed = false;
		bool bestBuildingsBuilt = false;
		bool bestBuildingsDestroyed = false;
	};

	// One "best of the match" tile: which stat (0 supplies, 1 units destroyed, 2 buildings
	// destroyed, 3 units built), who leads it, in their colour, and by how much.
	struct HighlightModel
	{
		int stat = 0;
		Rml::String name;
		Rml::String colorHex;
		int value = 0;
	};

	// Ronin @bugfix 31/10/2026 DX9: a chat line and the colour the game gave it (RmlChatColorHex(); empty = the pane's own)
	struct ChatLineModel
	{
		Rml::String text;
		Rml::String color;
	};

	struct Model
	{
		Rml::Vector<RowModel> rows;
		Rml::Vector<HighlightModel> highlights;

		// The match header: mode (ScoreScreenModeType), the local side's outcome, map and length.
		int mode = 0;
		bool isVictory = false;
		bool isDefeat = false;
		Rml::String mapName;
		Rml::String gameTime; // h:mm:ss or m:ss
		bool canSaveReplay = false; // the game was recorded (ScoreScreen.cpp's canSaveReplay)
		Rml::String backgroundImage;
		bool hasBackgroundImage = false;

		// ScoreScreenLayout fields, same meaning as ScoreScreen.cpp's applyScoreScreenLayout().
		bool showChatEntry = false;
		bool showEmoteButton = false;
		bool showChatBoxBorder = false;
		bool showChatLog = false;
		bool showBuddiesButton = false;
		bool showContinueButton = false;
		bool showDefaultContinueLabel = true; // continueButtonCaption is empty: show the localized default label instead
		Rml::String continueButtonCaption;
		bool showAcademyPanel = false;
		Rml::Vector<Rml::String> academyAdvice;

		// Single player campaign finish (see finishSinglePlayerIfNeeded()). showChallengeSplash
		// mirrors ScoreScreen.cpp's displayChallengeWinLoss(): when true, the table/academy/chat
		// are replaced by the challenge win/loss portrait and text.
		bool showChallengeSplash = false;
		Rml::String challengePortraitImage;
		Rml::String challengeHeaderText;
		Rml::String challengeRemarksText;
		bool showSaveGameText = false;

		Rml::String chatEntryText;
		Rml::Vector<ChatLineModel> chatLines;
	} m_model;
};

// Registry entry point (see RmlUiManager::init()).
void OpenRmlScoreScreen();
void CloseRmlScoreScreen();
