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
#include "W3DDevice/GameClient/RmlUi/RmlScoreScreen.h"
#include "W3DDevice/GameClient/RmlUi/RmlChatInput.h"

#include "Common/AsciiString.h"
#include "Common/Recorder.h"
#include "Common/UnicodeString.h"
#include "Common/UnicodeUtf8.h"
#include "GameClient/CampaignManager.h"
#include "GameClient/GUI/GUICallbacks/Menus/ScoreScreenActions.h"
#include "GameClient/Image.h"
#include "W3DDevice/GameClient/RmlUi/RmlTransitionSounds.h"
#include "GameLogic/GameLogic.h"
#include "GameNetwork/LANAPICallbacks.h"
#include "W3DDevice/GameClient/RmlUi/RmlUiManager.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ElementDocument.h>

#include <cstdio>
#include <windows.h>

namespace
{
	Rml::String colorToHex(Color color)
	{
		char hex[8];
		_snprintf_s(hex, sizeof(hex), _TRUNCATE, "#%02X%02X%02X", (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
		return Rml::String(hex);
	}

	Rml::String formatGameTime(UnsignedInt seconds)
	{
		char text[32];
		if (seconds >= 3600)
			_snprintf_s(text, sizeof(text), _TRUNCATE, "%u:%02u:%02u", seconds / 3600, (seconds / 60) % 60, seconds % 60);
		else
			_snprintf_s(text, sizeof(text), _TRUNCATE, "%u:%02u", seconds / 60, seconds % 60);
		return Rml::String(text);
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
RmlScoreScreen &RmlScoreScreen::instance()
{
	static RmlScoreScreen s_screen;
	return s_screen;
}

void RmlScoreScreen::load(Rml::Context *context)
{
	if (m_document)
		return; // already loaded

	m_context = context;

	Rml::DataModelConstructor constructor = context->CreateDataModel("scorescreen");
	if (constructor)
	{
		m_hq.bind(constructor);
		Rml::StructHandle<RowModel> rowHandle = constructor.RegisterStruct<RowModel>();
		if (rowHandle)
		{
			rowHandle.RegisterMember("display_name", &RowModel::displayName);
			rowHandle.RegisterMember("is_observer", &RowModel::isObserver);
			rowHandle.RegisterMember("money_earned", &RowModel::moneyEarned);
			rowHandle.RegisterMember("units_built", &RowModel::unitsBuilt);
			rowHandle.RegisterMember("units_lost", &RowModel::unitsLost);
			rowHandle.RegisterMember("units_destroyed", &RowModel::unitsDestroyed);
			rowHandle.RegisterMember("buildings_built", &RowModel::buildingsBuilt);
			rowHandle.RegisterMember("buildings_lost", &RowModel::buildingsLost);
			rowHandle.RegisterMember("buildings_destroyed", &RowModel::buildingsDestroyed);
			rowHandle.RegisterMember("side_icon_image", &RowModel::sideIconImage);
			rowHandle.RegisterMember("show_side_icon", &RowModel::showSideIcon);
			rowHandle.RegisterMember("badge_image", &RowModel::badgeImage);
			rowHandle.RegisterMember("color_hex", &RowModel::colorHex);
			rowHandle.RegisterMember("is_local", &RowModel::isLocal);
			rowHandle.RegisterMember("is_victor", &RowModel::isVictor);
			rowHandle.RegisterMember("best_money", &RowModel::bestMoney);
			rowHandle.RegisterMember("best_units_built", &RowModel::bestUnitsBuilt);
			rowHandle.RegisterMember("best_units_destroyed", &RowModel::bestUnitsDestroyed);
			rowHandle.RegisterMember("best_buildings_built", &RowModel::bestBuildingsBuilt);
			rowHandle.RegisterMember("best_buildings_destroyed", &RowModel::bestBuildingsDestroyed);
		}
		constructor.RegisterArray<Rml::Vector<RowModel>>();
		Rml::StructHandle<HighlightModel> highlightHandle = constructor.RegisterStruct<HighlightModel>();
		if (highlightHandle)
		{
			highlightHandle.RegisterMember("stat", &HighlightModel::stat);
			highlightHandle.RegisterMember("name", &HighlightModel::name);
			highlightHandle.RegisterMember("color_hex", &HighlightModel::colorHex);
			highlightHandle.RegisterMember("value", &HighlightModel::value);
		}
		constructor.RegisterArray<Rml::Vector<HighlightModel>>();
		constructor.RegisterArray<Rml::Vector<Rml::String>>();
		// Ronin @bugfix 31/10/2026 DX9: chat lines carry their colour (emotes, player colours)
		Rml::StructHandle<ChatLineModel> chatLineHandle = constructor.RegisterStruct<ChatLineModel>();
		if (chatLineHandle)
		{
			chatLineHandle.RegisterMember("text", &ChatLineModel::text);
			chatLineHandle.RegisterMember("color", &ChatLineModel::color);
		}
		constructor.RegisterArray<Rml::Vector<ChatLineModel>>();

		constructor.Bind("rows", &m_model.rows);
		constructor.Bind("highlights", &m_model.highlights);
		constructor.Bind("mode", &m_model.mode);
		constructor.Bind("is_victory", &m_model.isVictory);
		constructor.Bind("is_defeat", &m_model.isDefeat);
		constructor.Bind("map_name", &m_model.mapName);
		constructor.Bind("game_time", &m_model.gameTime);
		constructor.Bind("can_save_replay", &m_model.canSaveReplay);
		constructor.Bind("background_image", &m_model.backgroundImage);
		constructor.Bind("has_background_image", &m_model.hasBackgroundImage);
		constructor.Bind("show_chat_entry", &m_model.showChatEntry);
		constructor.Bind("show_emote_button", &m_model.showEmoteButton);
		constructor.Bind("show_chat_box_border", &m_model.showChatBoxBorder);
		constructor.Bind("show_chat_log", &m_model.showChatLog);
		constructor.Bind("show_buddies_button", &m_model.showBuddiesButton);
		constructor.Bind("show_continue_button", &m_model.showContinueButton);
		constructor.Bind("show_default_continue_label", &m_model.showDefaultContinueLabel);
		constructor.Bind("continue_button_caption", &m_model.continueButtonCaption);
		constructor.Bind("show_academy_panel", &m_model.showAcademyPanel);
		constructor.Bind("academy_advice", &m_model.academyAdvice);
		constructor.Bind("show_challenge_splash", &m_model.showChallengeSplash);
		constructor.Bind("challenge_portrait_image", &m_model.challengePortraitImage);
		constructor.Bind("challenge_header_text", &m_model.challengeHeaderText);
		constructor.Bind("challenge_remarks_text", &m_model.challengeRemarksText);
		constructor.Bind("show_save_game_text", &m_model.showSaveGameText);
		constructor.Bind("chat_entry_text", &m_model.chatEntryText);
		constructor.Bind("chat_lines", &m_model.chatLines);

		constructor.BindEventCallback("ok", &RmlScoreScreen::onOk, this);
		constructor.BindEventCallback("continue_pressed", &RmlScoreScreen::onContinue, this);
		constructor.BindEventCallback("buddies", &RmlScoreScreen::onBuddies, this);
		constructor.BindEventCallback("save_replay", &RmlScoreScreen::onSaveReplay, this);
		constructor.BindEventCallback("send_chat", &RmlScoreScreen::onSendChat, this);
		constructor.BindEventCallback("chat_entry_committed", &RmlScoreScreen::onChatEntryCommitted, this);
		constructor.BindEventCallback("send_emote", &RmlScoreScreen::onSendEmote, this);

		m_modelHandle = constructor.GetModelHandle();
	}

	m_document = TheRmlUiManager->loadScreen("Data/UI/ScoreScreen.rml");	// Ronin @feature 31/10/2026 DX9: our documents live in Data/UI
}

//-------------------------------------------------------------------------------------------------
// Mode dispatch moved from ScoreScreen.cpp's ScoreScreenInit(), which never runs for this
// RmlUi-routed .wnd path (see RmlScoreScreen.h). Builds ScoreScreenData + ScoreScreenLayout the
// same way the matching initX() function does and copies them into m_model.
void RmlScoreScreen::refreshFromGameState()
{
	ScoreScreenData data;

	if (TheGameLogic->isInReplayGame())
	{
		m_mode = SCORESCREENMODE_REPLAY;
		if (TheRecorder->isMultiplayer())
			data = ScoreScreenData::buildForMultiPlayer(m_mode);
		else
			data = ScoreScreenData::buildForSinglePlayer();
	}
	else if (TheGameLogic->isInInternetGame())
	{
		m_mode = SCORESCREENMODE_INTERNET;
		data = ScoreScreenData::buildForMultiPlayer(m_mode);
	}
	else if (TheGameLogic->isInLanGame())
	{
		m_mode = SCORESCREENMODE_LAN;
		data = ScoreScreenData::buildForMultiPlayer(m_mode);
	}
	else if (TheGameLogic->isInSkirmishGame())
	{
		m_mode = SCORESCREENMODE_SKIRMISH;
		data = ScoreScreenData::buildForMultiPlayer(m_mode);
	}
	else
	{
		m_mode = SCORESCREENMODE_SINGLEPLAYER;
		data = ScoreScreenData::buildForSinglePlayer();
	}

	ScoreScreenLayout layout = ScoreScreenLayout::forMode(m_mode);

	m_model.rows.clear();
	m_model.academyAdvice.clear();
	for (const ScoreScreenPlayerRow &row : data.m_rows)
	{
		RowModel rowModel;
		rowModel.displayName = unicodeToUtf8(row.m_displayName);
		rowModel.isObserver = row.m_isObserver == TRUE;
		rowModel.moneyEarned = row.m_stats.m_totalMoneyEarned;
		rowModel.unitsBuilt = row.m_stats.m_totalUnitsBuilt;
		rowModel.unitsLost = row.m_stats.m_totalUnitsLost;
		rowModel.unitsDestroyed = row.m_stats.m_totalUnitsDestroyed;
		rowModel.buildingsBuilt = row.m_stats.m_totalBuildingsBuilt;
		rowModel.buildingsLost = row.m_stats.m_totalBuildingsLost;
		rowModel.buildingsDestroyed = row.m_stats.m_totalBuildingsDestroyed;
		if (row.m_touchSideIcon && row.m_sideIconImage)
		{
			rowModel.sideIconImage = row.m_sideIconImage->getName().str();
			rowModel.showSideIcon = true;
		}
		rowModel.badgeImage = row.m_badgeImage ? row.m_badgeImage->getName().str() : Rml::String();
		rowModel.colorHex = colorToHex(row.m_textColor);
		rowModel.isLocal = row.m_isLocalPlayerRow == TRUE;
		rowModel.isVictor = row.m_isVictor == TRUE;
		m_model.rows.push_back(rowModel);

		if (row.m_isLocalPlayerRow)
			for (const UnicodeString &tip : row.m_academyAdvice)
				m_model.academyAdvice.push_back(unicodeToUtf8(tip));
	}

	markLeaders();

	m_model.mode = (int)m_mode;
	m_model.isVictory = data.m_localVictory == TRUE;
	m_model.isDefeat = data.m_localDefeat == TRUE;
	m_model.mapName = unicodeToUtf8(data.m_mapDisplayName);
	m_model.gameTime = formatGameTime(data.m_gameSeconds);
	m_model.canSaveReplay = TheRecorder && TheRecorder->getMode() == RECORDERMODETYPE_RECORD;

	m_model.hasBackgroundImage = data.m_hasBackgroundImage == TRUE;
	m_model.backgroundImage = data.m_hasBackgroundImage && data.m_backgroundImage ? data.m_backgroundImage->getName().str() : Rml::String();

	m_model.showChatEntry = layout.m_showChatEntry == TRUE;
	m_model.showEmoteButton = layout.m_showEmoteButton == TRUE;
	m_model.showChatBoxBorder = layout.m_showChatBoxBorder == TRUE;
	m_model.showChatLog = layout.m_showChatLog == TRUE;
	m_model.showBuddiesButton = layout.m_showBuddiesButton == TRUE;
	m_model.showContinueButton = layout.m_showContinueButton == TRUE;
	m_model.showDefaultContinueLabel = layout.m_continueButtonCaption.isEmpty();
	m_model.continueButtonCaption = m_model.showDefaultContinueLabel ? Rml::String() : unicodeToUtf8(layout.m_continueButtonCaption);
	m_model.showAcademyPanel = layout.m_touchAcademyPanel == TRUE && layout.m_showAcademyPanel == TRUE;

	m_model.showChallengeSplash = false;
	m_model.challengePortraitImage.clear();
	m_model.challengeHeaderText.clear();
	m_model.challengeRemarksText.clear();
	m_model.showSaveGameText = false;
	m_buttonIsFinishCampaign = false;

	m_model.chatEntryText.clear();
	m_model.chatLines.clear();

	if (m_modelHandle)
		m_modelHandle.DirtyAllVariables();

	// See finishSinglePlayerIfNeeded(): same one-tick-later timing as the .wnd path's
	// s_needToFinishSinglePlayerInit/ScoreScreenUpdate().
	m_needsFinishSinglePlayer = (m_mode == SCORESCREENMODE_SINGLEPLAYER);
}

void RmlScoreScreen::show()
{
	if (!m_document)
		return;

	refreshFromGameState();
	m_hq.refresh(m_modelHandle, true);
	m_document->Show();

	// ScoreScreen.cpp sets ScoreScreenShow right away for every mode but the campaign, which sets it
	// once finishSinglePlayerIfNeeded() knows it is not a challenge.
	if (m_mode != SCORESCREENMODE_SINGLEPLAYER)
		TransitionSounds::play("ScoreScreenShow");

	// Forwards LANAPI::OnChat()'s score-screen chat/emote/system lines (see LANAPICallbacks.cpp), and
	// the WOLGameSetupMenu.cpp/WOLQuickMatchMenu.cpp disconnect notices, into this screen's chat log
	// while it's the active score screen. Ronin @bugfix 31/10/2026 DX9: each line in its colour (RmlChatColorHex()).
	m_chatConnection = ScoreScreenSignals::chatLine().connect([this](const UnicodeString &line, Color color)
		{
			appendChatLine(unicodeToUtf8(line), RmlChatColorHex(color));
		});
}

void RmlScoreScreen::hide()
{
	TransitionSounds::stop("ScoreScreenShow"); // ScoreScreen.cpp's remove("ScoreScreenShow")
	if (m_document)
		m_document->Hide();
	m_chatConnection.disconnect();
}

bool RmlScoreScreen::isVisible() const
{
	return m_document && m_document->IsVisible();
}

void RmlScoreScreen::onBack()
{
	// Same as ButtonOk (see ScoreScreen.cpp's GWM_CHAR/KEY_ESC); duplicated instead of routed
	// through onOk() since that takes an Rml::Event& this call site doesn't have (see
	// RmlOptionsScreen::onBack() for the same pattern).
	ScoreScreenActions::pressOk();
}

// Marks each stat's leader among the players (not observers) and builds the highlight tiles from
// the same leaders. Needs two players to compare; zero leads nothing.
void RmlScoreScreen::markLeaders()
{
	m_model.highlights.clear();

	int contenders = 0;
	for (const RowModel &row : m_model.rows)
		if (!row.isObserver)
			++contenders;
	if (contenders < 2)
		return;

	struct Stat { int RowModel::*value; bool RowModel::*best; int highlight; };
	const Stat stats[] =
	{
		{ &RowModel::moneyEarned, &RowModel::bestMoney, 0 },
		{ &RowModel::unitsDestroyed, &RowModel::bestUnitsDestroyed, 1 },
		{ &RowModel::buildingsDestroyed, &RowModel::bestBuildingsDestroyed, 2 },
		{ &RowModel::unitsBuilt, &RowModel::bestUnitsBuilt, 3 },
		{ &RowModel::buildingsBuilt, &RowModel::bestBuildingsBuilt, -1 },
	};

	for (const Stat &stat : stats)
	{
		int best = 0;
		const RowModel *leader = nullptr;
		for (const RowModel &row : m_model.rows)
			if (!row.isObserver && row.*stat.value > best)
			{
				best = row.*stat.value;
				leader = &row;
			}
		if (!leader)
			continue;

		for (RowModel &row : m_model.rows)
			row.*stat.best = !row.isObserver && row.*stat.value == best;

		if (stat.highlight >= 0)
		{
			HighlightModel highlight;
			highlight.stat = stat.highlight;
			highlight.name = leader->displayName;
			highlight.colorHex = leader->colorHex;
			highlight.value = best;
			m_model.highlights.push_back(highlight);
		}
	}
}

void RmlScoreScreen::appendChatLine(const Rml::String &line, const Rml::String &color)
{
	ChatLineModel row;
	row.text = line;
	row.color = color;
	m_model.chatLines.push_back(row);
	if (m_modelHandle)
		m_modelHandle.DirtyVariable("chat_lines");
}

//-------------------------------------------------------------------------------------------------
void RmlScoreScreen::onOk(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &)
{
	ScoreScreenActions::pressOk();
}

void RmlScoreScreen::onContinue(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &)
{
	ScoreScreenActions::pressContinue(m_mode, m_buttonIsFinishCampaign ? TRUE : FALSE);
}

void RmlScoreScreen::update()
{
	m_hq.refresh(m_modelHandle);
	finishSinglePlayerIfNeeded();
}

// Moved from ScoreScreen.cpp's finishSinglePlayerInit(), see ScoreScreenActions::finishSinglePlayer().
// Runs once, one update() tick after show() (matching the .wnd path's
// s_needToFinishSinglePlayerInit/ScoreScreenUpdate() timing).
void RmlScoreScreen::finishSinglePlayerIfNeeded()
{
	if (!m_needsFinishSinglePlayer)
		return;
	m_needsFinishSinglePlayer = false;

	ScoreScreenCampaignFinish result = ScoreScreenActions::finishSinglePlayer();
	m_buttonIsFinishCampaign = result.m_campaignComplete == TRUE;
	m_model.isVictory = result.m_victorious == TRUE;
	m_model.isDefeat = result.m_victorious != TRUE;

	if (TheCampaignManager && TheCampaignManager->getCurrentCampaign() && !TheCampaignManager->getCurrentCampaign()->isChallengeCampaign())
		TransitionSounds::play("ScoreScreenShow");

	m_model.showChallengeSplash = result.m_showChallengeSplash == TRUE;
	m_model.challengePortraitImage = result.m_challengePortrait ? result.m_challengePortrait->getName().str() : Rml::String();
	m_model.challengeHeaderText = unicodeToUtf8(result.m_challengeHeaderText);
	m_model.challengeRemarksText = unicodeToUtf8(result.m_challengeRemarksText);

	m_model.showDefaultContinueLabel = result.m_continueButtonCaption.isEmpty();
	m_model.continueButtonCaption = m_model.showDefaultContinueLabel ? Rml::String() : unicodeToUtf8(result.m_continueButtonCaption);

	m_model.showSaveGameText = result.m_showSaveGameText == TRUE;

	if (m_modelHandle)
		m_modelHandle.DirtyAllVariables();

	if (result.m_campaignCompletionMovie.isNotEmpty())
		ScoreScreenActions::playCampaignCompletionMovie(result.m_campaignCompletionMovie);
}

void RmlScoreScreen::onBuddies(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &)
{
	ScoreScreenActions::toggleBuddyOverlay();
}

void RmlScoreScreen::onSaveReplay(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &)
{
	ScoreScreenActions::startSaveReplayFlow();
}

void RmlScoreScreen::onSendChat(Rml::DataModelHandle, Rml::Event &ev, const Rml::VariantList &)
{
	UnicodeString text = utf8ToUnicode(m_model.chatEntryText);
	text.trim();
	if (!text.isEmpty())
		appendChatLine(m_model.chatEntryText);
	ScoreScreenActions::sendChat(text, FALSE);
	m_model.chatEntryText.clear();
	RmlClearChatInput(ev);
	if (m_modelHandle)
		m_modelHandle.DirtyVariable("chat_entry_text");
}

// Enter in the entry: same as the .wnd's GEM_EDIT_DONE. Change fires on every edit, so only a
// linebreak commits.
void RmlScoreScreen::onChatEntryCommitted(Rml::DataModelHandle handle, Rml::Event &ev, const Rml::VariantList &args)
{
	if (!ev.GetParameter<bool>("linebreak", false))
		return;
	onSendChat(handle, ev, args);
}

void RmlScoreScreen::onSendEmote(Rml::DataModelHandle, Rml::Event &ev, const Rml::VariantList &)
{
	UnicodeString text = utf8ToUnicode(m_model.chatEntryText);
	text.trim();
	if (!text.isEmpty())
		appendChatLine(m_model.chatEntryText);
	ScoreScreenActions::sendChat(text, TRUE);
	m_model.chatEntryText.clear();
	RmlClearChatInput(ev);
	if (m_modelHandle)
		m_modelHandle.DirtyVariable("chat_entry_text");
}

//-------------------------------------------------------------------------------------------------
void OpenRmlScoreScreen()
{
	if (!TheRmlUiManager)
		return;
	// Ronin @feature 31/10/2026 DX9: our shell's push / pop closes the screen before; no current-screen slot here.
	RmlScoreScreen &screen = RmlScoreScreen::instance();
	screen.load(TheRmlUiManager->getContext());
	screen.show();
}

void CloseRmlScoreScreen()
{
	if (RmlScoreScreen::instance().isVisible())
		RmlScoreScreen::instance().hide();
}
