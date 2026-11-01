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
#include "W3DDevice/GameClient/RmlUi/RmlLanGameSetupScreen.h"
#include "W3DDevice/GameClient/RmlUi/RmlChatInput.h"

#include "Common/AsciiString.h"
#include "Common/MultiplayerSettings.h"
#include "Common/PlayerTemplate.h"
#include "Common/QuotedPrintable.h"
#include "Common/UnicodeString.h"
#include "Common/UnicodeUtf8.h"
#include "Common/UserPreferences.h"
#include "GameClient/GameText.h"
#include "GameClient/GUI/GUICallbacks/Menus/LanGameSetupActions.h"
#include "GameClient/GUI/GUICallbacks/Menus/LanGameSetupData.h"
#include "GameClient/MapUtil.h"
#include "GameClient/RmlUiScreenRegistry.h"
#include "GameClient/Shell.h"
#include "W3DDevice/GameClient/RmlUi/RmlTransitionSounds.h"
#include "GameNetwork/FirewallHelper.h"
#include "GameNetwork/LANAPI.h"
#include "GameNetwork/LANAPICallbacks.h"
#include "GameNetwork/LANGameInfo.h"
#include "GameNetwork/LANPlayer.h"
#include "W3DDevice/GameClient/RmlUi/RmlUiManager.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ElementDocument.h>

#include <cstdlib>
#include <cstdio>
#include <map>
#include <windows.h>

//-------------------------------------------------------------------------------------------------
static Rml::String rgbToHex(UnsignedInt rgb)
{
	char hex[8];
	_snprintf_s(hex, sizeof(hex), _TRUNCATE, "#%02X%02X%02X",
		(rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
	return Rml::String(hex);
}

// Fallback for an unset/unmatched color, see RmlSkirmishSetupScreen.cpp's kNoColorHex.
static const char *const kNoColorHex = "transparent";

// Mirrors playerTemplateComboBoxTooltip()/playerTemplateListBoxTooltip() (LobbyUtils.cpp): the
// "Army Tooltip" text for a faction dropdown entry.
static Rml::String factionTooltipFor(Int playerTemplate)
{
	if (playerTemplate == PLAYERTEMPLATE_RANDOM)
		return unicodeToUtf8(TheGameText->fetch("TOOLTIP:BioStrategyLong_Random"));

	const PlayerTemplate *tmpl = ThePlayerTemplateStore ? ThePlayerTemplateStore->getNthPlayerTemplate(playerTemplate) : nullptr;
	if (tmpl)
		return unicodeToUtf8(TheGameText->fetch(tmpl->getTooltip()));
	return Rml::String();
}

// Mirrors playerTooltip()/setLANPlayerTooltip() (LanGameOptionsMenu.cpp): the LAN identity tooltip
// for an occupied human slot.
static Rml::String lanPlayerTooltipFor(LANPlayer *player)
{
	if (!player)
		return Rml::String();

	UnicodeString tooltip;
	if (!player->getLogin().isEmpty() || !player->getHost().isEmpty())
		tooltip.format(TheGameText->fetch("TOOLTIP:LANPlayer"), player->getLogin().str(), player->getHost().str());

#if defined(RTS_DEBUG)
	UnicodeString ip;
	ip.format(L" - %d.%d.%d.%d", PRINTF_IP_AS_4_INTS(player->getIP()));
	tooltip.concat(ip);
#endif

	return unicodeToUtf8(tooltip);
}

//-------------------------------------------------------------------------------------------------
RmlLanGameSetupScreen &RmlLanGameSetupScreen::instance()
{
	static RmlLanGameSetupScreen s_screen;
	return s_screen;
}

void RmlLanGameSetupScreen::load(Rml::Context *context)
{
	if (m_document)
		return;

	m_context = context;

	Rml::DataModelConstructor constructor = context->CreateDataModel("langamesetup");
	if (constructor)
	{
		m_hq.bind(constructor);

		// Registered before the slot rows, which hold a per-row list of these.
		Rml::StructHandle<OptionModel> optionHandle = constructor.RegisterStruct<OptionModel>();
		if (optionHandle)
		{
			optionHandle.RegisterMember("value", &OptionModel::value);
			optionHandle.RegisterMember("label", &OptionModel::label);
			optionHandle.RegisterMember("swatch", &OptionModel::swatch);
			optionHandle.RegisterMember("icon", &OptionModel::icon);
			optionHandle.RegisterMember("taken", &OptionModel::taken);
		}
		constructor.RegisterArray<Rml::Vector<OptionModel>>();

		Rml::StructHandle<SlotRowModel> rowHandle = constructor.RegisterStruct<SlotRowModel>();
		if (rowHandle)
		{
			rowHandle.RegisterMember("slot_index", &SlotRowModel::slotIndex);
			rowHandle.RegisterMember("occupant_state", &SlotRowModel::occupantState);
			rowHandle.RegisterMember("is_occupied", &SlotRowModel::isOccupied);
			rowHandle.RegisterMember("is_human_occupant", &SlotRowModel::isHumanOccupant);
			rowHandle.RegisterMember("occupant_label", &SlotRowModel::occupantLabel);
			rowHandle.RegisterMember("can_edit", &SlotRowModel::canEdit);
			rowHandle.RegisterMember("can_edit_occupant", &SlotRowModel::canEditOccupant);
			rowHandle.RegisterMember("player_name", &SlotRowModel::playerName);
			rowHandle.RegisterMember("player_template", &SlotRowModel::playerTemplate);
			rowHandle.RegisterMember("faction_label", &SlotRowModel::factionLabel);
			rowHandle.RegisterMember("faction_tooltip", &SlotRowModel::factionTooltip);
			rowHandle.RegisterMember("color", &SlotRowModel::color);
			rowHandle.RegisterMember("color_hex", &SlotRowModel::colorHex);
			rowHandle.RegisterMember("color_name", &SlotRowModel::colorName);
			rowHandle.RegisterMember("color_options", &SlotRowModel::colorOptions);
			rowHandle.RegisterMember("team_number", &SlotRowModel::teamNumber);
			rowHandle.RegisterMember("start_position", &SlotRowModel::startPosition);
			rowHandle.RegisterMember("unused", &SlotRowModel::unused);
			rowHandle.RegisterMember("group_head", &SlotRowModel::groupHead);
			rowHandle.RegisterMember("group_team", &SlotRowModel::groupTeam);
			rowHandle.RegisterMember("fold_head", &SlotRowModel::foldHead);
			rowHandle.RegisterMember("accepted", &SlotRowModel::accepted);
			rowHandle.RegisterMember("has_map", &SlotRowModel::hasMap);
			rowHandle.RegisterMember("show_accept", &SlotRowModel::showAccept);
			rowHandle.RegisterMember("is_host_slot", &SlotRowModel::isHostSlot);
			rowHandle.RegisterMember("is_local", &SlotRowModel::isLocal);
			rowHandle.RegisterMember("player_tooltip", &SlotRowModel::playerTooltip);
		}

		Rml::StructHandle<StartMarkerModel> markerHandle = constructor.RegisterStruct<StartMarkerModel>();
		if (markerHandle)
		{
			markerHandle.RegisterMember("position", &StartMarkerModel::position);
			markerHandle.RegisterMember("x_style", &StartMarkerModel::xStyle);
			markerHandle.RegisterMember("y_style", &StartMarkerModel::yStyle);
			markerHandle.RegisterMember("is_occupied", &StartMarkerModel::isOccupied);
			markerHandle.RegisterMember("slot_number", &StartMarkerModel::slotNumber);
			markerHandle.RegisterMember("tooltip", &StartMarkerModel::tooltip);
			markerHandle.RegisterMember("color_hex", &StartMarkerModel::colorHex);
			markerHandle.RegisterMember("used", &StartMarkerModel::used);
		}

		constructor.RegisterArray<Rml::Vector<SlotRowModel>>();
		constructor.RegisterArray<Rml::Vector<StartMarkerModel>>();
		constructor.RegisterArray<Rml::Vector<Rml::String>>();
		// Ronin @bugfix 31/10/2026 DX9: chat lines carry their colour (emotes, player colours)
		Rml::StructHandle<ChatLineModel> chatLineHandle = constructor.RegisterStruct<ChatLineModel>();
		if (chatLineHandle)
		{
			chatLineHandle.RegisterMember("text", &ChatLineModel::text);
			chatLineHandle.RegisterMember("color", &ChatLineModel::color);
		}
		constructor.RegisterArray<Rml::Vector<ChatLineModel>>();

		constructor.Bind("slots", &m_model.slots);
		constructor.Bind("start_markers", &m_model.startMarkers);
		constructor.Bind("faction_options", &m_model.factionOptions);
		constructor.Bind("starting_cash_options", &m_model.startingCashOptions);
		constructor.Bind("map_name", &m_model.mapName);
		constructor.Bind("map_display_name", &m_model.mapDisplayName);
		constructor.Bind("map_found", &m_model.mapFound);
		constructor.Bind("starting_cash", &m_model.startingCash);
		constructor.Bind("superweapons_restricted", &m_model.superweaponsRestricted);
		constructor.Bind("is_host", &m_model.isHost);
		constructor.Bind("map_num_players", &m_model.mapNumPlayers);
		constructor.Bind("team_mode", &m_model.teamMode);
		constructor.Bind("unused_count", &m_model.unusedCount);
		constructor.Bind("show_unused", &m_model.showUnused);
		constructor.Bind("color_popover_slot", &m_model.colorPopoverSlot);
		constructor.Bind("start_blockers", &m_model.startBlockers);
		constructor.Bind("start_enabled", &m_model.startEnabled);
		constructor.Bind("game_name", &m_model.gameName);
		constructor.Bind("local_accepted", &m_model.localAccepted);
		constructor.Bind("chat_lines", &m_model.chatLines);
		constructor.Bind("chat_entry_text", &m_model.chatEntryText);

		constructor.BindEventCallback("slot_occupant_changed", &RmlLanGameSetupScreen::onSlotOccupantChanged, this);
		constructor.BindEventCallback("slot_occupant_picked", &RmlLanGameSetupScreen::onSlotOccupantPicked, this);
		constructor.BindEventCallback("slot_color_picked", &RmlLanGameSetupScreen::onSlotColorPicked, this);
		constructor.BindEventCallback("slot_team_picked", &RmlLanGameSetupScreen::onSlotTeamPicked, this);
		constructor.BindEventCallback("starting_cash_picked", &RmlLanGameSetupScreen::onStartingCashPicked, this);
		constructor.BindEventCallback("starting_cash_selected", &RmlLanGameSetupScreen::onStartingCashSelected, this);
		constructor.BindEventCallback("slot_faction_changed", &RmlLanGameSetupScreen::onSlotFactionChanged, this);
		constructor.BindEventCallback("slot_color_changed", &RmlLanGameSetupScreen::onSlotColorChanged, this);
		constructor.BindEventCallback("slot_team_changed", &RmlLanGameSetupScreen::onSlotTeamChanged, this);
		constructor.BindEventCallback("start_marker_clicked", &RmlLanGameSetupScreen::onStartPositionMarkerClick, this);
		constructor.BindEventCallback("start_marker_mousedown", &RmlLanGameSetupScreen::onStartPositionMarkerMouseDown, this);
		constructor.BindEventCallback("starting_cash_changed", &RmlLanGameSetupScreen::onStartingCashChanged, this);
		constructor.BindEventCallback("superweapons_changed", &RmlLanGameSetupScreen::onSuperweaponsChanged, this);
		constructor.BindEventCallback("select_map", &RmlLanGameSetupScreen::onSelectMap, this);
		constructor.BindEventCallback("fill_ai", &RmlLanGameSetupScreen::onFillAI, this);
		constructor.BindEventCallback("start", &RmlLanGameSetupScreen::onStart, this);
		constructor.BindEventCallback("back", &RmlLanGameSetupScreen::onBackPressed, this);
		constructor.BindEventCallback("chat_entry_committed", &RmlLanGameSetupScreen::onChatEntryCommitted, this);
		constructor.BindEventCallback("send_emote", &RmlLanGameSetupScreen::onSendEmote, this);

		m_modelHandle = constructor.GetModelHandle();
	}

	m_document = TheRmlUiManager->loadScreen("Data/UI/LanGameOptions.rml");	// Ronin @feature 31/10/2026 DX9: our documents live in Data/UI
}

//-------------------------------------------------------------------------------------------------
void RmlLanGameSetupScreen::connectSignals()
{
	m_connections.disconnect();
	m_connections.add(LanGameSetupSignals::slotsChanged().connect([this](LANGameInfo *game) { onSlotsChanged(game); }));
	m_connections.add(LanGameSetupSignals::optionsChanged().connect([this](LANGameInfo *game) { onOptionsChanged(game); }));
	m_connections.add(LanGameSetupSignals::startButton().connect([this](Bool enabled) { onStartButtonEnabledChanged(enabled == TRUE); }));
	m_connections.add(LanGameSetupSignals::chatLine().connect([this](const UnicodeString &line, Color color) { onChatLine(unicodeToUtf8(line), RmlChatColorHex(color)); }));
}

//-------------------------------------------------------------------------------------------------
void RmlLanGameSetupScreen::refreshFromGameState()
{
	LANGameInfo *game = TheLAN ? TheLAN->GetMyGame() : nullptr;
	LanGameSetupData data = LanGameSetupData::build(game, m_model.startEnabled);

	m_model.isHost = data.m_isHost == TRUE;
	m_model.gameName = game ? unicodeToUtf8(game->getName()) : Rml::String();
	m_model.localAccepted = false;

	Rml::Vector<SlotRowModel> rows;
	std::vector<GameSetupSlotRow> bases;
	for (size_t i = 0; i < data.m_slots.size(); ++i)
	{
		const LanGameSetupSlotRow &src = data.m_slots[i];
		bases.push_back(src.m_base);
		const GameSetupSlotRow &base = src.m_base;

		SlotRowModel row;
		row.slotIndex = (int)i;
		row.occupantState = (int)base.m_state;
		row.isOccupied = base.m_state != SLOT_OPEN && base.m_state != SLOT_CLOSED;
		row.isHumanOccupant = base.m_state == SLOT_PLAYER;
		row.occupantLabel = unicodeToUtf8(base.m_name);
		row.canEdit = base.m_canEdit == TRUE;
		row.canEditOccupant = base.m_canEditOccupant == TRUE;
		row.playerName = unicodeToUtf8(base.m_name);
		row.playerTemplate = base.m_playerTemplate;
		row.color = base.m_color;
		row.colorHex = kNoColorHex;
		row.teamNumber = base.m_teamNumber;
		row.startPosition = base.m_startPosition;
		row.accepted = src.m_accepted == TRUE;
		row.hasMap = src.m_hasMap == TRUE;
		row.showAccept = (i == 0);
		row.isHostSlot = (i == 0) && row.isHumanOccupant;
		row.isLocal = base.m_isLocalSlot == TRUE;
		if (row.isLocal)
			m_model.localAccepted = row.accepted;
		row.factionTooltip = factionTooltipFor(base.m_playerTemplate);

		for (const GameSetupFactionOption &faction : data.m_options.m_factionOptions)
		{
			if (faction.m_playerTemplate == base.m_playerTemplate)
			{
				row.factionLabel = unicodeToUtf8(faction.m_displayName);
				break;
			}
		}
		for (const GameSetupColorOption &color : data.m_options.m_colorOptions)
		{
			if (color.m_color == base.m_color)
			{
				row.colorName = unicodeToUtf8(color.m_name);
				if (color.m_color >= 0)
					row.colorHex = rgbToHex(color.m_rgb);
			}
			// Every row lists the whole palette in the same order so the swatches never move; a
			// color missing from m_colorChoices (PopulateColorComboBox()'s filter) is shown taken.
			bool offered = false;
			for (Int choice : base.m_colorChoices)
				offered = offered || choice == color.m_color;
			OptionModel option{ color.m_color, unicodeToUtf8(color.m_name), color.m_color >= 0 ? rgbToHex(color.m_rgb) : Rml::String(kNoColorHex) };
			option.taken = !offered;
			row.colorOptions.push_back(option);
		}

		if (row.isHumanOccupant && game)
		{
			LANGameSlot *slot = game->getLANSlot((Int)i);
			row.playerTooltip = lanPlayerTooltipFor(slot ? slot->getUser() : nullptr);
		}

		rows.push_back(row);
	}

	// The list's order: in capacity first (grouped by team in team mode), unused ones folded last.
	Bool teamMode = FALSE;
	Int unusedCount = 0;
	m_model.slots.clear();
	for (const GameSetupDisplayRow &at : GameSetupData::displayOrder(bases, data.m_options, &teamMode, &unusedCount))
	{
		SlotRowModel row = rows[at.m_slot];
		row.unused = at.m_unused == TRUE;
		row.groupHead = at.m_groupHead == TRUE;
		row.groupTeam = at.m_groupTeam;
		row.foldHead = at.m_foldHead == TRUE;
		m_model.slots.push_back(row);
	}
	m_model.teamMode = teamMode == TRUE;
	m_model.unusedCount = unusedCount;
	m_model.mapNumPlayers = data.m_options.m_mapFound ? data.m_options.m_mapNumPlayers : 0;

	// Why the host's Start would be refused (a client's Accept has no checks).
	m_model.startBlockers.clear();
	if (m_model.isHost)
		for (const UnicodeString &reason : GameSetupData::startBlockers(game, TRUE))
			m_model.startBlockers.push_back(unicodeToUtf8(reason));

	m_model.startMarkers.clear();
	for (const GameSetupStartPositionMarker &marker : data.m_options.m_startPositionMarkers)
	{
		StartMarkerModel markerModel;
		markerModel.position = marker.m_position;
		markerModel.used = marker.m_used == TRUE;
		char xBuf[16], yBuf[16];
		_snprintf_s(xBuf, sizeof(xBuf), _TRUNCATE, "%.3f%%", marker.m_xFraction * 100.0f);
		_snprintf_s(yBuf, sizeof(yBuf), _TRUNCATE, "%.3f%%", marker.m_yFraction * 100.0f);
		markerModel.xStyle = xBuf;
		markerModel.yStyle = yBuf;
		markerModel.isOccupied = markerModel.used && marker.m_occupantSlot >= 0;
		markerModel.slotNumber = markerModel.isOccupied ? marker.m_occupantSlot + 1 : 0;
		markerModel.colorHex = rgbToHex(markerModel.isOccupied ? marker.m_occupantColor : 0xFFFFFF);
		markerModel.tooltip = unicodeToUtf8(GameSetupData::startPositionTooltip(marker));

		m_model.startMarkers.push_back(markerModel);
	}

	m_model.factionOptions.clear();
	for (const GameSetupFactionOption &faction : data.m_options.m_factionOptions)
		m_model.factionOptions.push_back(OptionModel{ faction.m_playerTemplate, unicodeToUtf8(faction.m_displayName), Rml::String(), faction.m_iconImage.str() });

	m_model.startingCashOptions.clear();
	for (const GameSetupStartingCashOption &cash : data.m_options.m_startingCashOptions)
		m_model.startingCashOptions.push_back(OptionModel{ cash.m_amount, unicodeToUtf8(cash.m_label) });

	m_model.mapName = data.m_options.m_mapName.str();
	m_model.mapDisplayName = unicodeToUtf8(data.m_options.m_mapDisplayName);
	m_model.mapFound = data.m_options.m_mapFound == TRUE;
	m_model.startingCash = (int)data.m_options.m_startingCash.countMoney();
	m_model.superweaponsRestricted = data.m_options.m_superweaponsRestricted == TRUE;

	// Per-variable Dirty*() instead of DirtyAllVariables() -- see RmlSkirmishSetupScreen::
	// refreshFromGameState()'s comment for why (start_markers is fixed-size but slots'/markers'
	// per-row contents still change shape often enough that dirtying only what changed matters).
	if (m_modelHandle)
	{
		m_modelHandle.DirtyVariable("slots");
		m_modelHandle.DirtyVariable("start_markers");
		m_modelHandle.DirtyVariable("faction_options");
		m_modelHandle.DirtyVariable("starting_cash_options");
		m_modelHandle.DirtyVariable("map_name");
		m_modelHandle.DirtyVariable("map_display_name");
		m_modelHandle.DirtyVariable("map_found");
		m_modelHandle.DirtyVariable("starting_cash");
		m_modelHandle.DirtyVariable("superweapons_restricted");
		m_modelHandle.DirtyVariable("is_host");
		m_modelHandle.DirtyVariable("game_name");
		m_modelHandle.DirtyVariable("local_accepted");
		m_modelHandle.DirtyVariable("map_num_players");
		m_modelHandle.DirtyVariable("team_mode");
		m_modelHandle.DirtyVariable("unused_count");
		m_modelHandle.DirtyVariable("start_blockers");
	}
}

//-------------------------------------------------------------------------------------------------
void RmlLanGameSetupScreen::show()
{
	// Shell::doPush() opens a routed screen more than once, and the shell reopens its top layout
	// later too. Setting up again would wipe the chat log.
	if (!m_document || !TheLAN || m_document->IsVisible())
		return;

	LANGameInfo *game = TheLAN->GetMyGame();
	if (!game)
		return;

	if (game->isGameInProgress())
	{
		// Same as LanGameOptionsMenuInit(): returning here after a match means going straight back
		// to the lobby.
		if (TheShell)
			TheShell->popImmediate();
		return;
	}

	m_model.chatLines.clear();
	m_model.chatEntryText.clear();
	m_model.startEnabled = true;
	m_model.showUnused = false;
	m_model.colorPopoverSlot = -1;
	if (m_modelHandle)
	{
		m_modelHandle.DirtyVariable("show_unused");
		m_modelHandle.DirtyVariable("color_popover_slot");
	}

	TheMapCache->updateCache();

	if (TheLAN->AmIHost())
	{
		LANGameSlot *slot = game->getLANSlot(0);
		LANPreferences pref;
		slot->setColor(pref.getPreferredColor());
		slot->setPlayerTemplate(pref.getPreferredFaction());
		slot->setNATBehavior(FirewallHelperClass::FIREWALL_TYPE_SIMPLE);
		game->setMap(pref.getPreferredMap());
		game->setStartingCash(pref.getStartingCash());
		game->setSuperweaponRestriction(pref.getSuperweaponRestricted() ? 1 : 0);

		AsciiString lowerMap = pref.getPreferredMap();
		lowerMap.toLower();
		std::map<AsciiString, MapMetaData>::iterator it = TheMapCache->find(lowerMap);
		if (it != TheMapCache->end())
		{
			game->getSlot(0)->setMapAvailability(true);
			game->setMapCRC(it->second.m_CRC);
			game->setMapSize(it->second.m_filesize);
			game->adjustSlotsForMap();
		}

		TheLAN->RequestGameOptions(GenerateGameOptionsString(), true);
		TheLAN->RequestGameAnnounce();
	}
	else
	{
		game->setMapCRC(game->getMapCRC()); // force a recheck
		game->setMapSize(game->getMapSize()); // of if we have the map
		TheLAN->RequestHasMap();
	}

	refreshFromGameState();

	connectSignals();

	m_hq.refresh(m_modelHandle, true);
	m_document->Show();

	// LanGameOptionsMenuInit()'s entrance group, and LanGameOptionsMenuShutdown()'s reverse below.
	TransitionSounds::play("LanGameOptionsFade");
}

void RmlLanGameSetupScreen::hide()
{
	if (m_document && m_document->IsVisible())
		TransitionSounds::play("LanGameOptionsFade", TRUE);
	if (m_document)
		m_document->Hide();

	m_connections.disconnect();
}

bool RmlLanGameSetupScreen::isVisible() const
{
	return m_document && m_document->IsVisible();
}

void RmlLanGameSetupScreen::update()
{
	m_hq.refresh(m_modelHandle);
}

void RmlLanGameSetupScreen::onBack()
{
	LanGameSetupActions::leaveGame();
	hide();
	if (TheShell)
		TheShell->pop();
}

//-------------------------------------------------------------------------------------------------
void RmlLanGameSetupScreen::onSlotOccupantChanged(Rml::DataModelHandle, Rml::Event &ev, const Rml::VariantList &args)
{
	if (args.empty() || !TheLAN)
		return;
	Int slotIndex = args[0].Get<int>();
	Int state = atoi(ev.GetParameter<Rml::String>("value", "0").c_str());
	Bool isAIChanged = FALSE;
	LanGameSetupActions::selectPlayerState(TheLAN->GetMyGame(), slotIndex, (SlotState)state, &isAIChanged);
	refreshFromGameState();
}

// "Fill with AI" (host): the open slots, in order, up to the map's player count, each the same as its
// own Add AI button.
void RmlLanGameSetupScreen::onFillAI(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &args)
{
	LANGameInfo *game = TheLAN ? TheLAN->GetMyGame() : nullptr;
	if (args.empty() || !game || !TheLAN->AmIHost())
		return;
	const SlotState state = (SlotState)args[0].Get<int>();
	if (state < SLOT_EASY_AI || state > SLOT_BRUTAL_AI)
		return;

	const GameSetupData data = GameSetupData::build(game, TRUE);
	const Int capacity = data.m_options.m_mapFound && data.m_options.m_mapNumPlayers > 0 ? data.m_options.m_mapNumPlayers : MAX_SLOTS;
	Int players = 0;
	for (const GameSetupSlotRow &slot : data.m_slots)
		players += (slot.m_state != SLOT_OPEN && slot.m_state != SLOT_CLOSED) ? 1 : 0;
	for (Int i = 0; i < (Int)data.m_slots.size() && players < capacity; ++i)
	{
		if (data.m_slots[i].m_state != SLOT_OPEN || !data.m_slots[i].m_canEditOccupant)
			continue;
		Bool isAIChanged = FALSE;
		LanGameSetupActions::selectPlayerState(game, i, state, &isAIChanged);
		++players;
	}
	refreshFromGameState();
}

// An open slot's Add AI buttons: the same path as its occupant select.
void RmlLanGameSetupScreen::onSlotOccupantPicked(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &args)
{
	if (args.size() < 2 || !TheLAN)
		return;
	Bool isAIChanged = FALSE;
	LanGameSetupActions::selectPlayerState(TheLAN->GetMyGame(), args[0].Get<int>(), (SlotState)args[1].Get<int>(), &isAIChanged);
	refreshFromGameState();
}

void RmlLanGameSetupScreen::onSlotColorPicked(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &args)
{
	if (args.size() < 2 || !TheLAN)
		return;
	LanGameSetupActions::selectColor(TheLAN->GetMyGame(), args[0].Get<int>(), args[1].Get<int>(), FALSE);
	refreshFromGameState();
}

void RmlLanGameSetupScreen::onSlotTeamPicked(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &args)
{
	if (args.size() < 2 || !TheLAN)
		return;
	LanGameSetupActions::selectTeam(TheLAN->GetMyGame(), args[0].Get<int>(), args[1].Get<int>(), FALSE);
	refreshFromGameState();
}

void RmlLanGameSetupScreen::onStartingCashPicked(Rml::DataModelHandle handle, Rml::Event &ev, const Rml::VariantList &args)
{
	if (args.empty() || !m_model.isHost)
		return;
	m_model.startingCash = args[0].Get<int>();
	onStartingCashChanged(handle, ev, args);
}

// The dropdown for a long cash list. It also reports the value it is handed, so only a different
// amount counts as a pick.
void RmlLanGameSetupScreen::onStartingCashSelected(Rml::DataModelHandle handle, Rml::Event &ev, const Rml::VariantList &)
{
	const int amount = atoi(ev.GetParameter<Rml::String>("value", "").c_str());
	if (amount <= 0 || amount == m_model.startingCash)
		return;
	Rml::VariantList args;
	args.push_back(Rml::Variant(amount));
	onStartingCashPicked(handle, ev, args);
}

void RmlLanGameSetupScreen::onSlotFactionChanged(Rml::DataModelHandle, Rml::Event &ev, const Rml::VariantList &args)
{
	if (args.empty() || !TheLAN)
		return;
	Int slotIndex = args[0].Get<int>();
	Int playerTemplate = atoi(ev.GetParameter<Rml::String>("value", "0").c_str());
	Bool observerChanged = FALSE;
	LanGameSetupActions::selectPlayerTemplate(TheLAN->GetMyGame(), slotIndex, playerTemplate, FALSE, &observerChanged);
	refreshFromGameState();
}

void RmlLanGameSetupScreen::onSlotColorChanged(Rml::DataModelHandle, Rml::Event &ev, const Rml::VariantList &args)
{
	if (args.empty() || !TheLAN)
		return;
	Int slotIndex = args[0].Get<int>();
	Int color = atoi(ev.GetParameter<Rml::String>("value", "-1").c_str());
	LanGameSetupActions::selectColor(TheLAN->GetMyGame(), slotIndex, color, FALSE);
	refreshFromGameState();
}

void RmlLanGameSetupScreen::onSlotTeamChanged(Rml::DataModelHandle, Rml::Event &ev, const Rml::VariantList &args)
{
	if (args.empty() || !TheLAN)
		return;
	Int slotIndex = args[0].Get<int>();
	Int team = atoi(ev.GetParameter<Rml::String>("value", "-1").c_str());
	LanGameSetupActions::selectTeam(TheLAN->GetMyGame(), slotIndex, team, FALSE);
	refreshFromGameState();
}

void RmlLanGameSetupScreen::onStartPositionMarkerClick(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &args)
{
	if (args.empty() || !TheLAN)
		return;
	Int position = args[0].Get<int>();
	LanGameSetupActions::handleStartPositionMarkerClick(TheLAN->GetMyGame(), position);
	refreshFromGameState();
}

void RmlLanGameSetupScreen::onStartPositionMarkerMouseDown(Rml::DataModelHandle, Rml::Event &ev, const Rml::VariantList &args)
{
	// GBM_SELECTED_RIGHT equivalent: RmlUi only synthesizes "click" for the left button,
	// so the right-click marker clear has to be caught on the raw mousedown (button 1).
	if (args.empty() || !TheLAN || ev.GetParameter<int>("button", 0) != 1)
		return;
	Int position = args[0].Get<int>();
	LanGameSetupActions::handleStartPositionMarkerRightClick(TheLAN->GetMyGame(), position);
	refreshFromGameState();
}

void RmlLanGameSetupScreen::onStartingCashChanged(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &)
{
	if (!TheLAN)
		return;
	Money money;
	money.deposit((UnsignedInt)max(0, m_model.startingCash), FALSE, FALSE);
	LanGameSetupActions::setStartingCash(TheLAN->GetMyGame(), money, FALSE);
	refreshFromGameState();
}

void RmlLanGameSetupScreen::onSuperweaponsChanged(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &)
{
	if (!TheLAN)
		return;
	LanGameSetupActions::setSuperweaponRestriction(TheLAN->GetMyGame(), m_model.superweaponsRestricted ? TRUE : FALSE, FALSE);
	refreshFromGameState();
}

void RmlLanGameSetupScreen::onSelectMap(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &)
{
	// Same as LanGameOptionsMenu.cpp's ButtonSelectMap: hide this screen while the map-select
	// overlay is up, same shape as the .wnd's showLANGameOptionsUnderlyingGUIElements(FALSE).
	hide();
	RmlUiScreenRegistry::open("Menus/LanMapSelectMenu.wnd");
}

void RmlLanGameSetupScreen::onStart(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &)
{
	if (!TheLAN)
		return;
	LANGameInfo *game = TheLAN->GetMyGame();
	if (!game)
		return;

	if (TheLAN->AmIHost())
	{
		if (LanGameSetupActions::validateStart(game) == LanGameSetupActions::STARTVALIDATION_READY)
		{
			LanGameSetupActions::startGame(game);
			m_model.startEnabled = false;
			if (m_modelHandle)
				m_modelHandle.DirtyVariable("start_enabled");
		}
		// else: validateStart() already sent whatever chat notice/RequestAccept() applies.
	}
	else
	{
		LanGameSetupActions::requestAccept(game);
		m_model.startEnabled = false;
		if (m_modelHandle)
			m_modelHandle.DirtyVariable("start_enabled");
	}
	refreshFromGameState();
}

void RmlLanGameSetupScreen::onBackPressed(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &)
{
	onBack();
}

void RmlLanGameSetupScreen::onChatEntryCommitted(Rml::DataModelHandle, Rml::Event &ev, const Rml::VariantList &)
{
	// Change fires on every edit; only Enter (linebreak) commits, like the .wnd's GEM_EDIT_DONE.
	if (!ev.GetParameter<bool>("linebreak", false))
		return;

	if (TheLAN)
	{
		UnicodeString text = utf8ToUnicode(m_model.chatEntryText);
		text.trim();
		if (!text.isEmpty())
			TheLAN->RequestPlayerChat(text);	// Ronin @bugfix 31/10/2026 DX9: as upstream's .wnd screens: player chat, a line starting "/me " goes as an emote
	}
	m_model.chatEntryText.clear();
	RmlClearChatInput(ev);
	if (m_modelHandle)
		m_modelHandle.DirtyVariable("chat_entry_text");
}

void RmlLanGameSetupScreen::onSendEmote(Rml::DataModelHandle, Rml::Event &ev, const Rml::VariantList &)
{
	// Ronin @bugfix 31/10/2026 DX9: the button beside the entry, as upstream's .wnd ButtonEmote (now buttonChat): player chat.
	if (TheLAN)
	{
		UnicodeString text = utf8ToUnicode(m_model.chatEntryText);
		text.trim();
		if (!text.isEmpty())
			TheLAN->RequestPlayerChat(text);
	}
	m_model.chatEntryText.clear();
	RmlClearChatInput(ev);
	if (m_modelHandle)
		m_modelHandle.DirtyVariable("chat_entry_text");
}

//-------------------------------------------------------------------------------------------------
// LanGameSetupSignals::slotsChanged/optionsChanged targets: full snapshot rebuild, same
// reasoning as RmlLanLobbyScreen::onPlayerListChanged()/onGameListChanged().
void RmlLanGameSetupScreen::onSlotsChanged(LANGameInfo * /*game*/)
{
	refreshFromGameState();
}

void RmlLanGameSetupScreen::onOptionsChanged(LANGameInfo * /*game*/)
{
	refreshFromGameState();
}

void RmlLanGameSetupScreen::onStartButtonEnabledChanged(bool enabled)
{
	m_model.startEnabled = enabled;
	if (m_modelHandle)
		m_modelHandle.DirtyVariable("start_enabled");
}

// LanGameSetupSignals::chatLine target. No echo-on-send: LAN chat is a broadcast the sender receives too,
// same as RmlLanLobbyScreen::onChatLine().
void RmlLanGameSetupScreen::onChatLine(const Rml::String &line, const Rml::String &color)
{
	ChatLineModel row;
	row.text = line;
	row.color = color;
	m_model.chatLines.push_back(row);
	if (m_modelHandle)
		m_modelHandle.DirtyVariable("chat_lines");
}

void RmlLanGameSetupScreen::returnFromMapSelect()
{
	if (!m_document)
		return;
	refreshFromGameState();

	connectSignals();

	m_hq.refresh(m_modelHandle, true);
	m_document->Show();
}

//-------------------------------------------------------------------------------------------------
void OpenRmlLanGameSetupScreen()
{
	if (!TheRmlUiManager)
		return;
	// Ronin @feature 31/10/2026 DX9: our shell's push / pop closes the screen before; no current-screen slot here.
	RmlLanGameSetupScreen &screen = RmlLanGameSetupScreen::instance();
	screen.load(TheRmlUiManager->getContext());
	screen.show();
}

void CloseRmlLanGameSetupScreen()
{
	if (RmlLanGameSetupScreen::instance().isVisible())
		RmlLanGameSetupScreen::instance().hide();
}

void ReturnToRmlLanGameSetupScreen()
{
	RmlLanGameSetupScreen::instance().returnFromMapSelect();
}
