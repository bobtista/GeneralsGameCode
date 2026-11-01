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
// FILE: RmlLanGameSetupScreen.h /////////////////////////////////////////////////
// RmlScreen for Data/UI/LanGameOptions.rml. Registered for
// Menus/LanGameOptionsMenu.wnd (LanLobbyActions::hostGame()/joinGame() push that
// path). Unlike the .wnd version this never gets a LanGameOptionsMenuInit()/
// Shutdown()/Update() callback, so:
//   - show() replays LanGameOptionsMenuInit()'s host/client setup (LANPreferences
//     read for the host, map CRC/size recheck + RequestHasMap() for the client,
//     RequestGameOptions()/RequestGameAnnounce()) itself, through
//     LanGameSetupActions/LanGameSetupData -- see LanGameOptionsMenu.cpp.
//   - hide() drops the four signal connections below; leaving the game (LanGameSetupActions::
//     leaveGame()) only happens on Back, same as the .wnd's buttonBack handler.
// Slot/option state arrives via LanGameSetupSignals::slotsChanged/optionsChanged/
// startButton, and chat via LanGameSetupSignals::chatLine (LANAPICallbacks.h), connected
// in show() and dropped in hide(), same lifetime pattern as RmlLanLobbyScreen's
// LanLobbySignals. Slot/map
// data is built through LanGameSetupData/GameSetupData (widget-agnostic, see
// LanGameSetupData.h), same shape as RmlSkirmishSetupScreen for the parts they
// share.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/Signal.h"
#include "W3DDevice/GameClient/RmlUi/RmlHqStatus.h"
#include "W3DDevice/GameClient/RmlUi/RmlScreen.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

class LANGameInfo;

namespace Rml { class Context; class ElementDocument; class Event; }

//-------------------------------------------------------------------------------------------------
// Ronin @feature 31/10/2026 DX9: standalone, as his other screens are: our RmlScreen has other methods.
class RmlLanGameSetupScreen
{
public:
	static RmlLanGameSetupScreen &instance();
	// RmlUiManager::shutdown(): Rml::Shutdown() frees the document and context, and this outlives them.
	void releaseRml() { m_document = nullptr; m_context = nullptr; m_modelHandle = Rml::DataModelHandle(); }

	// RmlScreen ----------------------------------------------------------------------------
	virtual void load(Rml::Context *context);
	virtual void show();
	virtual void hide();
	virtual bool isVisible() const;
	virtual void onBack(); // same as ButtonBack
	virtual void update(); // ticks the .hq-header status

private:
	RmlLanGameSetupScreen() {}

	// LanGameSetupSignals targets, see connectSignals().
	void onSlotsChanged(LANGameInfo *game);
	void onOptionsChanged(LANGameInfo *game);
	void onStartButtonEnabledChanged(bool enabled);
	void onChatLine(const Rml::String &line, const Rml::String &color = Rml::String());	// Ronin @bugfix 31/10/2026 DX9: color: RmlChatColorHex()
	void connectSignals();

	void refreshFromGameState(); // LanGameSetupData::build(TheLAN->GetMyGame()) -> m_model

	void onSlotOccupantChanged(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &);
	void onSlotOccupantPicked(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &); // (slot, state): an open slot's Add AI buttons
	void onSlotColorPicked(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &); // (slot, color): a swatch
	void onSlotTeamPicked(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &); // (slot, team): a team segment
	void onStartingCashPicked(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &); // (amount): a cash segment
	void onStartingCashSelected(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &); // cash dropdown: same as a segment, ignores its value echo
	void onSlotFactionChanged(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &);
	void onSlotColorChanged(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &);
	void onSlotTeamChanged(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &);
	void onStartPositionMarkerClick(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &);
	void onStartPositionMarkerMouseDown(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &);
	void onStartingCashChanged(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &);
	void onSuperweaponsChanged(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &);
	void onSelectMap(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &);
	void onFillAI(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &); // "Fill with AI" (host): (occupant state)
	void onStart(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &);
	void onBackPressed(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &);
	void onChatEntryCommitted(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &); // Enter, mirrors GEM_EDIT_DONE (LANCHAT_NORMAL)
	void onSendEmote(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &); // ButtonEmote, mirrors its real LANCHAT_EMOTE behavior

public:
	// Called by RmlLanMapSelectScreen (once converted) when it closes: re-shows this screen and
	// refreshes it from TheLAN->GetMyGame(), without show()'s LanGameOptionsMenuInit()-style reset.
	void returnFromMapSelect();

private:
	Rml::Context *m_context = nullptr;
	Rml::ElementDocument *m_document = nullptr;
	SignalConnections m_connections; // LanGameSetupSignals, connected while showing
	Rml::DataModelHandle m_modelHandle;
	RmlHqStatus m_hq; // .hq-header status

	struct OptionModel
	{
		int value = 0;
		Rml::String label;
		Rml::String swatch; // color options only: "#RRGGBB", "transparent" for random
		Rml::String icon; // faction options only: the army's mapped image, empty for random
		bool taken = false; // color options only: another slot holds it
	};

	// One slot row's worth of fields for the data-for-bound slot table (see load()). Same shape as
	// RmlSkirmishSetupScreen::SlotRowModel, plus the LAN-only bits (accepted/hasMap/showAccept/
	// tooltips) LanGameSetupData layers on top of GameSetupSlotRow.
	struct SlotRowModel
	{
		int slotIndex = 0;
		int occupantState = 0; // SlotState value
		bool isOccupied = false;
		bool isHumanOccupant = false;
		Rml::String occupantLabel;
		bool canEdit = false;
		bool canEditOccupant = false;
		Rml::String playerName;
		int playerTemplate = 0;
		Rml::String factionLabel;
		Rml::String factionTooltip; // "Army Tooltip" text, mirrors playerTemplateComboBoxTooltip()
		int color = -1;
		Rml::String colorHex;
		Rml::String colorName;
		Rml::Vector<OptionModel> colorOptions; // the whole palette, taken = not in GameSetupSlotRow::m_colorChoices
		int teamNumber = -1;
		int startPosition = -1;

		bool accepted = false; // GameSlot::isAccepted()
		bool hasMap = true;    // GameSlot::hasMap(), human slots only
		bool showAccept = false; // true only for slot 0, mirrors the .wnd hiding buttonAccept[1..7]
		bool isHostSlot = false; // slot 0, where the LAN host always sits
		bool isLocal = false;    // GameSetupSlotRow::m_isLocalSlot
		Rml::String playerTooltip; // LAN identity tooltip (login/host), mirrors playerTooltip(), empty if none

		// Where the row sits in the list: see RmlSkirmishSetupScreen::SlotRowModel.
		bool unused = false;
		bool groupHead = false;
		int groupTeam = -1;
		bool foldHead = false;
	};

	// One start-position marker on the map preview -- identical shape to
	// RmlSkirmishSetupScreen::StartMarkerModel; see that header's comment for why x_style/y_style
	// are pre-formatted strings and why this array is always MAX_SLOTS long.
	struct StartMarkerModel
	{
		int position = 0;
		Rml::String xStyle;
		Rml::String yStyle;
		bool isOccupied = false;
		int slotNumber = 0;
		Rml::String colorHex;
		Rml::String tooltip;
		bool used = false;
	};

	// Ronin @bugfix 31/10/2026 DX9: a chat line and the colour the game gave it (RmlChatColorHex(); empty = the pane's own)
	struct ChatLineModel
	{
		Rml::String text;
		Rml::String color;
	};

	struct Model
	{
		Rml::Vector<SlotRowModel> slots;
		Rml::Vector<StartMarkerModel> startMarkers;
		Rml::Vector<OptionModel> factionOptions;
		Rml::Vector<OptionModel> startingCashOptions;

		Rml::String mapName;
		Rml::String mapDisplayName;
		bool mapFound = false;

		int startingCash = 0;
		bool superweaponsRestricted = false;

		bool isHost = false; // gates host-only controls; also picks the Start/Accept caption
		bool startEnabled = true; // mirrors LANEnableStartButton()/LanGameSetupSignals::startButton
		Rml::String gameName; // LANGameInfo::getName()
		bool localAccepted = false; // the local slot's accepted flag, for the client's waiting state

		Rml::Vector<ChatLineModel> chatLines; // append-only while the document is open, see onChatLine()
		Rml::String chatEntryText;

		// Presentation: see RmlSkirmishSetupScreen's Model.
		int mapNumPlayers = 0;
		bool teamMode = false;
		int unusedCount = 0;
		bool showUnused = false;
		int colorPopoverSlot = -1;
		Rml::Vector<Rml::String> startBlockers;
	} m_model;
};

// Registry entry point (see RmlUiManager::init()).
void OpenRmlLanGameSetupScreen();
void CloseRmlLanGameSetupScreen();

// Called by RmlLanMapSelectScreen (once converted) on OK/Back instead of OpenRmlLanGameSetupScreen(),
// which would re-run LanGameOptionsMenuInit()-style setup and disturb host/client state already in
// progress. Not wired up yet -- see report; LanMapSelectMenu.wnd is not converted in this change.
void ReturnToRmlLanGameSetupScreen();
