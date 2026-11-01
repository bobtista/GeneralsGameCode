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

// Ronin @feature 28/10/2026 DX9: the RmlUi context over the game. Ported from tintinhamans' GeneralsGameCode fork
// (arctic/go/feat/rmlui): his context, input routing and fonts, without his screens; the test panel and `ui` are ours.
#include "W3DDevice/GameClient/RmlUi/RmlUiManager.h"

#include "Common/AsciiString.h"
#include "Common/AudioEventRTS.h"
#include "Common/FileSystem.h"
#include "Common/GameAudio.h"
#include "Common/OptionPreferences.h"
#include "GameClient/ControlBarData.h"	// ControlBar.h after the headers it needs
#include "GameClient/Display.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/InGameUI.h"
#include "GameClient/KeyDefs.h"
#include "GameClient/RmlUiScreenRegistry.h"
#include "GameLogic/GameLogic.h"
#include "GameClient/Mouse.h"
#include "W3DDevice/GameClient/R3DDebugPanel.h"
#include "GameClient/RmlUiMessageBoxHook.h"
#include "W3DDevice/GameClient/RmlUi/RmlChallengeMenuScreen.h"
#include "W3DDevice/GameClient/RmlUi/RmlChatInput.h"
#include "W3DDevice/GameClient/RmlUi/RmlControlBarScreen.h"
#include "W3DDevice/GameClient/RmlUi/RmlCreditsScreen.h"
#include "W3DDevice/GameClient/RmlUi/RmlDisconnectScreen.h"
#include "W3DDevice/GameClient/RmlUi/RmlDownloadScreen.h"
#include "W3DDevice/GameClient/RmlUi/RmlEndGameOverlayScreen.h"
#include "W3DDevice/GameClient/RmlUi/RmlInGamePopupScreen.h"
#include "W3DDevice/GameClient/RmlUi/RmlLanGameSetupScreen.h"
#include "W3DDevice/GameClient/RmlUi/RmlLanLobbyScreen.h"
#include "W3DDevice/GameClient/RmlUi/RmlLanMapSelectScreen.h"
#include "W3DDevice/GameClient/RmlUi/RmlLoadScreen.h"
#include "W3DDevice/GameClient/RmlUi/RmlMainMenuScreen.h"
#include "W3DDevice/GameClient/RmlUi/RmlMessageBox.h"
#include "W3DDevice/GameClient/RmlUi/RmlNetworkDirectConnectScreen.h"
#include "W3DDevice/GameClient/RmlUi/RmlOptionsScreen.h"
#include "W3DDevice/GameClient/RmlUi/RmlPopupReplayScreen.h"
#include "W3DDevice/GameClient/RmlUi/RmlQuitMenuScreen.h"
#include "W3DDevice/GameClient/RmlUi/RmlReplayMenuScreen.h"
#include "W3DDevice/GameClient/RmlUi/RmlSaveLoadScreen.h"
#include "W3DDevice/GameClient/RmlUi/RmlScoreScreen.h"
#include "W3DDevice/GameClient/RmlUi/RmlSkirmishMapSelectScreen.h"
#include "W3DDevice/GameClient/RmlUi/RmlSkirmishSetupScreen.h"
#include "W3DDevice/GameClient/RmlUi/RmlRulerDecorator.h"
#include "W3DDevice/GameClient/RmlUi/RmlTransitionSounds.h"
#include "W3DDevice/GameClient/RmlUi/RmlUiElements.h"
#include "W3DDevice/GameClient/RmlUi/RmlUiText.h"

#include <RmlUi/Core/ComputedValues.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/ElementInstancer.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Factory.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/StringUtilities.h>
#include <RmlUi/Debugger.h>

#include <stdio.h>
#include <string.h>
#include <vector>

RmlUiManager *TheRmlUiManager = nullptr;

static GameWindow *wndWindowUnder(int x, int y);	// below, with the input

namespace
{
	const char *const kTestDocument = "Data/UI/Test.rml";

	// The test panel's clicks, printed in the panel: proof a click over it reached RmlUi and not the world.
	class TestClickListener : public Rml::EventListener
	{
	public:
		virtual void ProcessEvent(Rml::Event &event) override
		{
			Rml::Element *e = event.GetTargetElement();
			while (e && e->GetId().empty())
				e = e->GetParentNode();
			char line[160];
			_snprintf_s(line, sizeof(line), _TRUNCATE, "ui test: %s clicked", e ? e->GetId().c_str() : "the panel");
			RmlUiPrint(line, false);
		}
	};
	TestClickListener s_testClick;

	// Ronin @feature 29/10/2026 DX9: a shell screen's press sounds as a 2003 push button's (GadgetPushButton's GWM_LEFT_DOWN);
	// a greyed one sounds as the window manager's disabled click.
	class ClickSoundListener : public Rml::EventListener
	{
	public:
		virtual void ProcessEvent(Rml::Event &event) override
		{
			if (!TheAudio || event.GetParameter<int>("button", 0) != 0)
				return;
			for (Rml::Element *e = event.GetTargetElement(); e; e = e->GetParentNode())
			{
				const Rml::String &tag = e->GetTagName();
				if (tag == "button" || (tag == "input" && e->GetAttribute<Rml::String>("type", "") == "checkbox"))
				{
					AudioEventRTS click(e->HasAttribute("disabled") ? "GUIClickDisabled" : "GUIClick");
					TheAudio->addAudioEvent(&click);
					return;
				}
			}
		}
	};
	ClickSoundListener s_clickSound;

	// Ronin @feature 29/10/2026 DX9: the shell screens, for their per-frame update, `ui reload` and shutdown.
	const int kShellScreenCount = 3;
	RmlScreen *shellScreen(int i)
	{
		switch (i)
		{
			case 0: return &RmlMainMenuScreen::instance();
			case 1: return &RmlQuitMenuScreen::instance();
			case 2: return &RmlOptionsScreen::instance();
			default: return nullptr;
		}
	}

	void cmdUi(Int argc, const AsciiString *argv)
	{
		if (TheRmlUiManager == nullptr)
		{
			RmlUiPrint("ui: RmlUi is not running", true);
			return;
		}
		const char *what = argc >= 2 ? argv[1].str() : "";
		if (stricmp(what, "test") == 0)
			TheRmlUiManager->toggleTestDocument();
		else if (stricmp(what, "reload") == 0)
			TheRmlUiManager->reloadDocuments();
		else if (stricmp(what, "debugger") == 0)
			TheRmlUiManager->toggleDebugger();
		else
		{
			char line[512];
			TheRmlUiManager->describe(line, sizeof(line));
			RmlUiPrint(line, false);
			RmlUiPrint("ui test - the test panel on/off; ui reload - documents and style sheets from Data/UI again;"
				" ui debugger - RmlUi's element inspector", false);
		}
	}

	int keyModifiers()
	{
		int mods = 0;
		if (::GetKeyState(VK_SHIFT) & 0x8000) mods |= Rml::Input::KM_SHIFT;
		if (::GetKeyState(VK_CONTROL) & 0x8000) mods |= Rml::Input::KM_CTRL;
		if (::GetKeyState(VK_MENU) & 0x8000) mods |= Rml::Input::KM_ALT;
		if (::GetKeyState(VK_CAPITAL) & 1) mods |= Rml::Input::KM_CAPSLOCK;
		return mods;
	}

	// The engine's DirectInput scan codes for what a text field needs; characters themselves come in a later phase.
	Rml::Input::KeyIdentifier engineKeyToRmlKey(unsigned char key)
	{
		using namespace Rml::Input;
		switch (key)
		{
			case KEY_ESC: return KI_ESCAPE;
			case KEY_TAB: return KI_TAB;
			case KEY_ENTER: return KI_RETURN;
			case KEY_KPENTER: return KI_NUMPADENTER;
			case KEY_BACKSPACE: return KI_BACK;
			case KEY_UP: return KI_UP;
			case KEY_DOWN: return KI_DOWN;
			case KEY_LEFT: return KI_LEFT;
			case KEY_RIGHT: return KI_RIGHT;
			case KEY_HOME: return KI_HOME;
			case KEY_END: return KI_END;
			case KEY_DEL: return KI_DELETE;
			default: return KI_UNKNOWN;
		}
	}
}

//-------------------------------------------------------------------------------------------------
void RmlUiManager::createInstance()
{
	if (TheRmlUiManager == nullptr)
		TheRmlUiManager = new RmlUiManager();
}

void RmlUiManager::destroyInstance()
{
	if (TheRmlUiManager != nullptr)
	{
		TheRmlUiManager->shutdown();
		delete TheRmlUiManager;
		TheRmlUiManager = nullptr;
	}
}

RmlUiManager::RmlUiManager()
	: m_context(nullptr), m_testDocument(nullptr), m_gameTextInstancer(nullptr), m_radarInstancer(nullptr), m_videoInstancer(nullptr),
	m_mappedImageInstancer(nullptr),
	m_mapPreviewInstancer(nullptr),
	m_pendingHighSurrogate(0), m_modernUI(false), m_width(0), m_height(0), m_initialized(false), m_debuggerInitialized(false),
	m_debuggerVisible(false)
{
}

RmlUiManager::~RmlUiManager()
{
	shutdown();
}

void RmlUiManager::init(int width, int height)
{
	if (m_initialized)
		return;
	m_width = width;
	m_height = height;

	Rml::SetSystemInterface(&m_systemInterface);
	Rml::SetFileInterface(&m_fileInterface);
	Rml::SetRenderInterface(&m_renderInterface);
	if (!Rml::Initialise())
	{
		RmlUiPrint("RmlUi did not start", true);
		return;
	}
	loadFonts();

	m_gameTextInstancer = new Rml::ElementInstancerGeneric<RmlGameTextElement>();
	Rml::Factory::RegisterElementInstancer("gametext", m_gameTextInstancer);
	m_radarInstancer = new Rml::ElementInstancerGeneric<RmlRadarElement>();
	Rml::Factory::RegisterElementInstancer("radar", m_radarInstancer);
	m_videoInstancer = new Rml::ElementInstancerGeneric<RmlVideoElement>();
	Rml::Factory::RegisterElementInstancer("video", m_videoInstancer);
	m_mappedImageInstancer = new Rml::ElementInstancerGeneric<RmlMappedImageElement>();	// Ronin @feature 29/10/2026 DX9: the shell screens'
	Rml::Factory::RegisterElementInstancer("mappedimage", m_mappedImageInstancer);
	m_mapPreviewInstancer = new Rml::ElementInstancerGeneric<RmlMapPreviewElement>();	// Ronin @feature 30/10/2026 DX9: his skirmish screens'
	Rml::Factory::RegisterElementInstancer("mappreview", m_mapPreviewInstancer);
	m_scrollLogInstancer = new Rml::ElementInstancerGeneric<RmlScrollLogElement>();	// Ronin @feature 31/10/2026 DX9: his chat logs
	Rml::Factory::RegisterElementInstancer("scrolllog", m_scrollLogInstancer);
	RmlRulerTicksInstancer::registerWithRml();	// ruler-ticks: the shell screens' frame

	// ModernUI = yes|no (1|0 too): the modern control bar. No while it is being built; the 2003 bar stays behind no.
	{
		OptionPreferences prefs;
		OptionPreferences::const_iterator it = prefs.find("ModernUI");
		m_modernUI = it != prefs.end() && (stricmp(it->second.str(), "yes") == 0 || atoi(it->second.str()) != 0);
	}

	m_context = Rml::CreateContext("main", Rml::Vector2i(width, height));
	if (m_context)
		m_context->SetDensityIndependentPixelRatio(height / 1080.0f);	// dp is a pixel at 1080 lines, so the UI scales by height
	if (m_modernUI && m_context)
	{
		registerScreens();
		RegisterRmlMessageBoxHook(m_context);	// Ronin @feature 31/10/2026 DX9: his message box, for gogoMessageBox()
	}

	R3DDebugPanel::registerCommand("ui", "ui [test|reload|debugger] - the RmlUi layer: its test panel, reload from Data/UI, inspector", cmdUi);
	TheRmlUiInputHook = this;
	m_initialized = true;
}

// Every .ttf and .otf in Data/UI/Fonts; a document asks for one by its family name.
void RmlUiManager::loadFonts()
{
	if (!TheFileSystem)
		return;
	AsciiString dir(RmlUiRoot);
	dir.concat("Fonts/");
	FilenameList files;
	TheFileSystem->getFileListInDirectory(dir, AsciiString("*.ttf"), files, FALSE);
	TheFileSystem->getFileListInDirectory(dir, AsciiString("*.otf"), files, FALSE);
	int loaded = 0;
	for (FilenameListIter it = files.begin(); it != files.end(); ++it)
		if (Rml::LoadFontFace(it->str()))
			++loaded;
	if (loaded == 0)
		RmlUiPrint("RmlUi: no fonts in Data/UI/Fonts - text will not draw", true);
}

// Ronin @feature 29/10/2026 DX9: the .wnd paths the 2003 code opens these by. The game menu is opened by ToggleQuitMenu() itself,
// for QuitMenu.wnd and QuitNoSave.wnd both.
void RmlUiManager::registerScreens()
{
	RmlUiScreenHooks main;
	main.open = [] { RmlMainMenuScreen::instance().open(); };
	main.close = [] { RmlMainMenuScreen::instance().close(); };
	main.isVisible = [] { return RmlMainMenuScreen::instance().isVisible(); };
	main.back = [] { RmlMainMenuScreen::instance().back(); };
	main.created = &RmlMainMenuScreen::created;
	main.destroyed = &RmlMainMenuScreen::destroyed;
	main.update = &RmlMainMenuScreen::shellUpdate;
	RmlUiScreenRegistry::registerScreen("Menus/MainMenu.wnd", main);

	RmlUiScreenHooks options;
	options.open = [] { RmlOptionsScreen::instance().open(); };
	options.close = [] { RmlOptionsScreen::instance().close(); };
	options.isVisible = [] { return RmlOptionsScreen::instance().isVisible(); };
	options.back = [] { RmlOptionsScreen::instance().back(); };
	RmlUiScreenRegistry::registerScreen("Menus/OptionsMenu.wnd", options);

	RmlUiScreenHooks game;
	game.open = [] { RmlQuitMenuScreen::instance().open(); };
	game.close = [] { RmlQuitMenuScreen::instance().close(); };
	game.isVisible = [] { return RmlQuitMenuScreen::instance().isVisible(); };
	game.back = [] { RmlQuitMenuScreen::instance().back(); };
	RmlUiScreenRegistry::registerScreen("Menus/QuitMenu.wnd", game);

	// Ronin @feature 30/10/2026 DX9: his single-player screens, as his manager registers them (each opens and closes itself).
	RmlUiScreenHooks skirmish;
	skirmish.open = &OpenRmlSkirmishSetupScreen;
	skirmish.close = &CloseRmlSkirmishSetupScreen;
	skirmish.isVisible = [] { return RmlSkirmishSetupScreen::instance().isVisible(); };
	skirmish.back = [] { RmlSkirmishSetupScreen::instance().onBack(); };
	RmlUiScreenRegistry::registerScreen("Menus/SkirmishGameOptionsMenu.wnd", skirmish);

	RmlUiScreenHooks mapSelect;
	mapSelect.open = &OpenRmlSkirmishMapSelectScreen;
	mapSelect.close = &CloseRmlSkirmishMapSelectScreen;
	mapSelect.isVisible = [] { return RmlSkirmishMapSelectScreen::instance().isVisible(); };
	mapSelect.back = [] { RmlSkirmishMapSelectScreen::instance().back(); };
	RmlUiScreenRegistry::registerScreen("Menus/SkirmishMapSelectMenu.wnd", mapSelect);

	RmlUiScreenHooks credits;
	credits.open = &OpenRmlCreditsScreen;
	credits.close = &CloseRmlCreditsScreen;
	credits.isVisible = [] { return RmlCreditsScreen::instance().isVisible(); };
	credits.back = [] { RmlCreditsScreen::instance().onBack(); };
	RmlUiScreenRegistry::registerScreen("Menus/CreditsMenu.wnd", credits);

	RmlUiScreenHooks saveLoad;
	saveLoad.open = &OpenRmlSaveLoadScreen;
	saveLoad.close = &CloseRmlSaveLoadScreen;
	saveLoad.isVisible = [] { return RmlSaveLoadScreen::instance().isVisible(); };
	saveLoad.back = [] { RmlSaveLoadScreen::instance().back(); };
	RmlUiScreenRegistry::registerScreen("Menus/SaveLoad.wnd", saveLoad);

	RmlUiScreenHooks popupSaveLoad = saveLoad;
	popupSaveLoad.open = &OpenRmlPopupSaveLoadScreen;
	popupSaveLoad.close = &CloseRmlPopupSaveLoadScreen;
	RmlUiScreenRegistry::registerScreen("Menus/PopupSaveLoad.wnd", popupSaveLoad);

	RmlUiScreenHooks replays;
	replays.open = &OpenRmlReplayMenuScreen;
	replays.close = &CloseRmlReplayMenuScreen;
	replays.isVisible = [] { return RmlReplayMenuScreen::instance().isVisible(); };
	replays.back = [] { RmlReplayMenuScreen::instance().back(); };
	RmlUiScreenRegistry::registerScreen("Menus/ReplayMenu.wnd", replays);

	RmlUiScreenHooks challenge;
	challenge.open = &OpenRmlChallengeMenuScreen;
	challenge.close = &CloseRmlChallengeMenuScreen;
	challenge.isVisible = [] { return RmlChallengeMenuScreen::instance().isVisible(); };
	challenge.back = [] { RmlChallengeMenuScreen::instance().back(); };
	RmlUiScreenRegistry::registerScreen("Menus/ChallengeMenu.wnd", challenge);

	// Ronin @feature 31/10/2026 DX9: his screens around a match, as his manager registers them. The score screen is pushed
	// after a match (GameLogicDispatch); the save-replay popup opens over it.
	RmlUiScreenHooks score;
	score.open = &OpenRmlScoreScreen;
	score.close = &CloseRmlScoreScreen;
	score.isVisible = [] { return RmlScoreScreen::instance().isVisible(); };
	score.back = [] { RmlScoreScreen::instance().onBack(); };
	RmlUiScreenRegistry::registerScreen("Menus/ScoreScreen.wnd", score);

	RmlUiScreenHooks popupReplay;
	popupReplay.open = &OpenRmlPopupReplayScreen;
	popupReplay.close = &CloseRmlPopupReplayScreen;
	popupReplay.isVisible = [] { return RmlPopupReplayScreen::instance().isVisible(); };
	popupReplay.back = [] { RmlPopupReplayScreen::instance().back(); };
	RmlUiScreenRegistry::registerScreen("Menus/PopupReplay.wnd", popupReplay);

	// Over live play, so they take no input but their own (ScriptActions makes the four through winCreateFromScript()).
	RmlUiScreenHooks endGame;
	endGame.isVisible = [] { return RmlEndGameOverlayScreen::instance().isVisible(); };
	endGame.capturesInput = false;
	endGame.open = &OpenRmlVictoriousScreen;
	endGame.close = &CloseRmlVictoriousScreen;
	RmlUiScreenRegistry::registerScreen("Menus/Victorious.wnd", endGame);
	endGame.open = &OpenRmlDefeatScreen;
	endGame.close = &CloseRmlDefeatScreen;
	RmlUiScreenRegistry::registerScreen("Menus/Defeat.wnd", endGame);
	endGame.open = &OpenRmlLocalDefeatScreen;
	endGame.close = &CloseRmlLocalDefeatScreen;
	RmlUiScreenRegistry::registerScreen("Menus/LocalDefeat.wnd", endGame);
	endGame.open = &OpenRmlObserverQuitScreen;
	endGame.close = &CloseRmlObserverQuitScreen;
	RmlUiScreenRegistry::registerScreen("Menus/ObserverQuit.wnd", endGame);

	RmlUiScreenHooks disconnect;
	disconnect.open = &OpenRmlDisconnectScreen;
	disconnect.close = &CloseRmlDisconnectScreen;
	disconnect.isVisible = [] { return RmlDisconnectScreen::instance().isVisible(); };
	disconnect.capturesInput = false;
	RmlUiScreenRegistry::registerScreen("Menus/DisconnectScreen.wnd", disconnect);

	RmlUiScreenHooks download;
	download.open = &OpenRmlDownloadScreen;
	download.close = &CloseRmlDownloadScreen;
	download.isVisible = [] { return RmlDownloadScreen::instance().isVisible(); };
	download.back = [] { RmlDownloadScreen::instance().back(); };
	RmlUiScreenRegistry::registerScreen("Menus/DownloadMenu.wnd", download);

	// A map script's popup (InGameUI::popupMessage()): over live play, but it takes Enter and Escape as the .wnd did.
	RmlUiScreenHooks popup;
	popup.open = &OpenRmlInGamePopupScreen;
	popup.close = &CloseRmlInGamePopupScreen;
	popup.isVisible = [] { return RmlInGamePopupScreen::instance().isVisible(); };
	popup.capturesInput = false;
	popup.keys = [](unsigned char key, unsigned short state) { return RmlInGamePopupScreen::instance().onKey(key, state); };
	RmlUiScreenRegistry::registerScreen("InGamePopupMessage.wnd", popup);

	// The LoadScreen classes make these through winCreateFromScript().
	RmlUiScreenHooks load;
	load.open = &OpenRmlMapTransferScreen;
	load.close = &CloseRmlMapTransferScreen;
	load.isVisible = [] { return RmlLoadScreen::instance(RmlLoadScreen::KIND_MAP_TRANSFER).isVisible(); };
	RmlUiScreenRegistry::registerScreen("Menus/MapTransferScreen.wnd", load);
	load.open = &OpenRmlMultiplayerLoadScreen;
	load.close = &CloseRmlMultiplayerLoadScreen;
	load.isVisible = [] { return RmlLoadScreen::instance(RmlLoadScreen::KIND_MULTIPLAYER).isVisible(); };
	RmlUiScreenRegistry::registerScreen("Menus/MultiplayerLoadScreen.wnd", load);
	load.open = &OpenRmlOnlineLoadScreen;
	load.close = &CloseRmlOnlineLoadScreen;
	load.isVisible = [] { return RmlLoadScreen::instance(RmlLoadScreen::KIND_ONLINE).isVisible(); };
	RmlUiScreenRegistry::registerScreen("Menus/GameSpyLoadScreen.wnd", load);
	load.open = &OpenRmlShellLoadScreen;
	load.close = &CloseRmlShellLoadScreen;
	load.isVisible = [] { return RmlLoadScreen::instance(RmlLoadScreen::KIND_SHELL).isVisible(); };
	RmlUiScreenRegistry::registerScreen("Menus/ShellGameLoadScreen.wnd", load);
	load.open = &OpenRmlSinglePlayerLoadScreen;
	load.close = &CloseRmlSinglePlayerLoadScreen;
	load.isVisible = [] { return RmlLoadScreen::instance(RmlLoadScreen::KIND_SINGLE_PLAYER).isVisible(); };
	RmlUiScreenRegistry::registerScreen("Menus/SinglePlayerLoadScreen.wnd", load);
	load.open = &OpenRmlChallengeLoadScreen;
	load.close = &CloseRmlChallengeLoadScreen;
	load.isVisible = [] { return RmlLoadScreen::instance(RmlLoadScreen::KIND_CHALLENGE).isVisible(); };
	RmlUiScreenRegistry::registerScreen("Menus/ChallengeLoadScreen.wnd", load);

	// Ronin @feature 31/10/2026 DX9: his LAN screens. The lobby's update runs while its layout is on the shell's stack, so it
	// polls the LAN under the game setup too, as LanLobbyMenuUpdate() did.
	RmlUiScreenHooks lanLobby;
	lanLobby.open = &OpenRmlLanLobbyScreen;
	lanLobby.close = &CloseRmlLanLobbyScreen;
	lanLobby.isVisible = [] { return RmlLanLobbyScreen::instance().isVisible(); };
	lanLobby.back = [] { RmlLanLobbyScreen::instance().onBack(); };
	lanLobby.update = [] { RmlLanLobbyScreen::instance().update(); };
	RmlUiScreenRegistry::registerScreen("Menus/LanLobbyMenu.wnd", lanLobby);

	RmlUiScreenHooks directConnect;
	directConnect.open = &OpenRmlNetworkDirectConnectScreen;
	directConnect.close = &CloseRmlNetworkDirectConnectScreen;
	directConnect.isVisible = [] { return RmlNetworkDirectConnectScreen::instance().isVisible(); };
	directConnect.back = [] { RmlNetworkDirectConnectScreen::instance().onBack(); };
	RmlUiScreenRegistry::registerScreen("Menus/NetworkDirectConnect.wnd", directConnect);

	RmlUiScreenHooks lanSetup;
	lanSetup.open = &OpenRmlLanGameSetupScreen;
	lanSetup.close = &CloseRmlLanGameSetupScreen;
	lanSetup.isVisible = [] { return RmlLanGameSetupScreen::instance().isVisible(); };
	lanSetup.back = [] { RmlLanGameSetupScreen::instance().onBack(); };
	RmlUiScreenRegistry::registerScreen("Menus/LanGameOptionsMenu.wnd", lanSetup);

	RmlUiScreenHooks lanMapSelect;
	lanMapSelect.open = &OpenRmlLanMapSelectScreen;
	lanMapSelect.close = &CloseRmlLanMapSelectScreen;
	lanMapSelect.isVisible = [] { return RmlLanMapSelectScreen::instance().isVisible(); };
	lanMapSelect.back = [] { RmlLanMapSelectScreen::instance().back(); };
	RmlUiScreenRegistry::registerScreen("Menus/LanMapSelectMenu.wnd", lanMapSelect);
}

Rml::ElementDocument *RmlUiManager::loadScreen(const char *path)
{
	Rml::ElementDocument *doc = m_context ? m_context->LoadDocument(path) : nullptr;
	if (doc)
		doc->AddEventListener(Rml::EventId::Mousedown, &s_clickSound);
	else
	{
		char line[160];
		_snprintf_s(line, sizeof(line), _TRUNCATE, "RmlUi: %s did not load - its menu stays empty", path);
		RmlUiPrint(line, true);
	}
	return doc;
}

void RmlUiManager::showScreen(Rml::ElementDocument *document)
{
	if (!document)
		return;
	if (!document->IsVisible())
		document->Show(Rml::ModalFlag::None, Rml::FocusFlag::Document);
	document->PullToFront();
}

// Ronin @feature 31/10/2026 DX9: a capturing screen or a message box; overlays over live play do not count.
bool RmlUiManager::screenUp() const
{
	return m_context && RmlUiScreenRegistry::ownsInput();
}

bool RmlUiManager::capturesInput() const
{
	return screenUp();
}

void RmlUiManager::shutdown()
{
	if (!m_initialized)
		return;
	if (TheRmlUiInputHook == this)
		TheRmlUiInputHook = nullptr;
	if (m_debuggerInitialized)
	{
		Rml::Debugger::Shutdown();
		m_debuggerInitialized = false;
		m_debuggerVisible = false;
	}
	UnregisterRmlMessageBoxHook();	// Ronin @feature 31/10/2026 DX9: closes a box still open, as his shutdown does
	RmlControlBarScreen::instance().shutdown();
	RmlUiScreenRegistry::unregisterAll();
	for (int i = 0; i < kShellScreenCount; ++i)
		shellScreen(i)->release();
	RmlSkirmishSetupScreen::instance().releaseRml();	// Ronin @feature 30/10/2026 DX9: his screens, likewise
	RmlSkirmishMapSelectScreen::instance().releaseRml();
	RmlCreditsScreen::instance().releaseRml();
	RmlSaveLoadScreen::instance().releaseRml();
	RmlReplayMenuScreen::instance().releaseRml();
	RmlChallengeMenuScreen::instance().releaseRml();
	RmlPopupReplayScreen::instance().releaseRml();
	RmlScoreScreen::instance().releaseRml();	// Ronin @feature 31/10/2026 DX9: his screens around a match, likewise
	RmlEndGameOverlayScreen::instance().releaseRml();
	RmlDisconnectScreen::instance().releaseRml();
	RmlDownloadScreen::instance().releaseRml();
	RmlInGamePopupScreen::instance().releaseRml();
	RmlLanLobbyScreen::instance().releaseRml();	// Ronin @feature 31/10/2026 DX9: his LAN screens, likewise
	RmlNetworkDirectConnectScreen::instance().releaseRml();
	RmlLanGameSetupScreen::instance().releaseRml();
	RmlLanMapSelectScreen::instance().releaseRml();
	for (int i = 0; i < RmlLoadScreen::KIND_COUNT; ++i)
		RmlLoadScreen::instance((RmlLoadScreen::Kind)i).releaseRml();
	RmlDropChatClears();
	if (m_context)
	{
		Rml::RemoveContext(m_context->GetName());
		m_context = nullptr;
	}
	m_testDocument = nullptr;
	m_tooltipDocument = nullptr;	// went with the context
	m_tooltipElement = nullptr;
	m_tooltipShown = false;
	Rml::Shutdown();
	delete m_gameTextInstancer;
	m_gameTextInstancer = nullptr;
	delete m_radarInstancer;
	m_radarInstancer = nullptr;
	delete m_videoInstancer;
	m_videoInstancer = nullptr;
	delete m_mappedImageInstancer;
	m_mappedImageInstancer = nullptr;
	delete m_mapPreviewInstancer;
	m_mapPreviewInstancer = nullptr;
	delete m_scrollLogInstancer;
	m_scrollLogInstancer = nullptr;
	m_renderInterface.releaseAll();
	m_initialized = false;
}

//-------------------------------------------------------------------------------------------------
// The debugger counts as shown at once: its documents only turn visible in the update this lets run.
void RmlUiManager::update()
{
	if (!m_context)
		return;
	RmlControlBarScreen::tick();	// shows or hides the bar with ControlBarParent, so it runs while nothing is up
	TransitionSounds::update();		// Ronin @feature 30/10/2026 DX9: the menus' transition sounds that are due, menu shown or not
	RmlReplayMenuScreen::tick();	// Ronin @feature 30/10/2026 DX9: his screens' own ticks and updates, as his manager runs them
	RmlChallengeMenuScreen::tick();
	if (RmlCreditsScreen::instance().isVisible())
		RmlCreditsScreen::instance().update();
	if (RmlSkirmishSetupScreen::instance().isVisible())
		RmlSkirmishSetupScreen::instance().update();
	RmlFlushChatClears();			// Ronin @feature 31/10/2026 DX9: his screens around a match
	if (RmlScoreScreen::instance().isVisible())
		RmlScoreScreen::instance().update();
	if (RmlLanGameSetupScreen::instance().isVisible())	// Ronin @feature 31/10/2026 DX9: his LAN screens' header status
		RmlLanGameSetupScreen::instance().update();
	if (RmlNetworkDirectConnectScreen::instance().isVisible())
		RmlNetworkDirectConnectScreen::instance().update();
	RmlLoadScreen::tick();
	RmlPopupReplayScreen::tick();
	RmlDisconnectScreen::tick();
	RmlDownloadScreen::tick();
	RmlUiMessageBoxHook::raise();	// a box stays above screens shown after it
	if (!hasVisibleDocument() && !m_debuggerVisible)
		return;
	for (int i = 0; i < kShellScreenCount; ++i)
		if (shellScreen(i)->isVisible())
			shellScreen(i)->update();
	m_context->Update();
	endHoverOverWorld();
	updateTooltip();
}

// Ronin @feature 30/10/2026 DX9: tintinhamans' tooltips (arctic/go/feat/rmlui): data-tooltip (a string key) or data-tooltip-text.
// His sit on the controls; a row (a label, his .opt-row / .opt-toggle, a button) shows the tip of the control inside it.
static Rml::Element *tooltipSource(Rml::Element *hover)
{
	for (Rml::Element *e = hover; e; e = e->GetParentNode())
	{
		if (e->HasAttribute("data-tooltip") || e->HasAttribute("data-tooltip-text"))
			return e;
		if (e->GetTagName() == "label" || e->GetTagName() == "button" || e->IsClassSet("opt-row") || e->IsClassSet("opt-toggle"))
		{
			if (Rml::Element *inner = e->QuerySelector("[data-tooltip]"))
				return inner;
			if (Rml::Element *inner = e->QuerySelector("[data-tooltip-text]"))
				return inner;
		}
	}
	return nullptr;
}

void RmlUiManager::hideTooltip()
{
	if (m_tooltipDocument && m_tooltipShown)
		m_tooltipDocument->Hide();
	m_tooltipShown = false;
}

// Over a shell screen only (the control bar has its own); beside the pointer, to its left or above it near the screen's edges.
void RmlUiManager::updateTooltip()
{
	Rml::Element *source = (m_context && screenUp()) ? tooltipSource(m_context->GetHoverElement()) : nullptr;
	const double now = Rml::GetSystemInterface() ? Rml::GetSystemInterface()->GetElapsedTime() : 0.0;
	if (source != m_tooltipElement)
	{
		m_tooltipElement = source;
		m_tooltipSince = now;
		hideTooltip();
	}
	if (!source || m_tooltipShown || now - m_tooltipSince < 0.5)
		return;

	Rml::String text;
	const Rml::String key = source->GetAttribute<Rml::String>("data-tooltip", "");
	if (!key.empty() && TheGameText)
		text = unicodeToUtf8(TheGameText->fetch(AsciiString(key.c_str())));
	else
		text = source->GetAttribute<Rml::String>("data-tooltip-text", "");
	const MouseIO *io = TheMouse ? TheMouse->getMouseStatus() : nullptr;
	if (text.empty() || !io)
		return;
	if (!m_tooltipDocument)
		m_tooltipDocument = m_context->LoadDocument("Data/UI/Tooltip.rml");
	Rml::Element *tip = m_tooltipDocument ? m_tooltipDocument->GetElementById("tip") : nullptr;
	if (!tip)
		return;
	tip->SetInnerRML(Rml::StringUtilities::EncodeRml(text));

	const float dp = m_height / 1080.0f;
	char value[32];
	if (io->pos.x > m_width - 460.0f * dp)
	{
		tip->RemoveProperty("left");
		_snprintf_s(value, sizeof(value), _TRUNCATE, "%dpx", (int)(m_width - io->pos.x + 8.0f * dp));
		tip->SetProperty("right", value);
	}
	else
	{
		tip->RemoveProperty("right");
		_snprintf_s(value, sizeof(value), _TRUNCATE, "%dpx", (int)(io->pos.x + 16.0f * dp));
		tip->SetProperty("left", value);
	}
	if (io->pos.y > m_height - 140.0f * dp)
	{
		tip->RemoveProperty("top");
		_snprintf_s(value, sizeof(value), _TRUNCATE, "%dpx", (int)(m_height - io->pos.y + 8.0f * dp));
		tip->SetProperty("bottom", value);
	}
	else
	{
		tip->RemoveProperty("bottom");
		_snprintf_s(value, sizeof(value), _TRUNCATE, "%dpx", (int)(io->pos.y + 24.0f * dp));
		tip->SetProperty("top", value);
	}
	m_tooltipDocument->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
	m_tooltipDocument->PullToFront();
	m_tooltipShown = true;
}

void RmlUiManager::render()
{
	if (!m_context)
		return;
	const int width = TheDisplay ? (int)TheDisplay->getWidth() : m_width;
	const int height = TheDisplay ? (int)TheDisplay->getHeight() : m_height;
	if (width != m_width || height != m_height)
	{
		m_width = width;
		m_height = height;
		m_context->SetDimensions(Rml::Vector2i(width, height));
		m_context->SetDensityIndependentPixelRatio(height / 1080.0f);
	}
	if (!hasVisibleDocument() && !m_debuggerVisible)
		return;
	m_renderInterface.beginFrame(m_width, m_height);
	m_context->Render();
	m_renderInterface.endFrame();
}

bool RmlUiManager::hasVisibleDocument() const
{
	if (!m_context)
		return false;
	for (int i = 0; i < m_context->GetNumDocuments(); ++i)
	{
		Rml::ElementDocument *doc = m_context->GetDocument(i);
		if (doc && doc->IsVisible())
			return true;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------
void RmlUiManager::toggleTestDocument()
{
	if (!m_context)
		return;
	if (m_testDocument == nullptr)
	{
		m_testDocument = m_context->LoadDocument(kTestDocument);
		if (m_testDocument == nullptr)
		{
			RmlUiPrint("ui test: Data/UI/Test.rml did not load", true);
			return;
		}
		m_testDocument->AddEventListener(Rml::EventId::Click, &s_testClick);
	}
	if (m_testDocument->IsVisible())
		m_testDocument->Hide();
	else
		m_testDocument->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
	RmlUiPrint(m_testDocument->IsVisible() ? "ui test: shown" : "ui test: hidden", false);
}

// Documents under Data/UI close and load again with the style sheets read anew; the debugger's own are left alone.
void RmlUiManager::reloadDocuments()
{
	if (!m_context)
		return;
	Rml::Factory::ClearStyleSheetCache();
	Rml::Factory::ClearTemplateCache();

	struct Reload { Rml::String url; bool visible; bool test; };
	std::vector<Reload> reloads;
	Rml::ElementDocument *bar = RmlControlBarScreen::instance().document();
	for (int i = m_context->GetNumDocuments() - 1; i >= 0; --i)
	{
		Rml::ElementDocument *doc = m_context->GetDocument(i);
		if (!doc || doc == bar || doc == m_tooltipDocument || doc->GetSourceURL().rfind(RmlUiRoot, 0) != 0)
			continue;
		bool screen = false;	// the shell screens reload themselves below, keeping their models
		for (int s = 0; s < kShellScreenCount; ++s)
			screen = screen || doc == shellScreen(s)->document();
		if (screen)
			continue;
		Reload r = { doc->GetSourceURL(), doc->IsVisible(), doc == m_testDocument };
		reloads.push_back(r);
		doc->Close();
	}
	m_testDocument = nullptr;

	int loaded = 0;
	for (size_t i = 0; i < reloads.size(); ++i)
	{
		Rml::ElementDocument *doc = m_context->LoadDocument(reloads[i].url);
		if (!doc)
			continue;
		++loaded;
		if (reloads[i].test)
		{
			m_testDocument = doc;
			doc->AddEventListener(Rml::EventId::Click, &s_testClick);
		}
		if (reloads[i].visible)
			doc->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
	}
	if (bar)
		RmlControlBarScreen::instance().reload();	// the bar keeps its data model
	for (int s = 0; s < kShellScreenCount; ++s)
		shellScreen(s)->reload();
	if (m_tooltipDocument)	// loads again on the next tip
	{
		m_tooltipDocument->Close();
		m_tooltipDocument = nullptr;
		m_tooltipShown = false;
	}
	char line[96];
	_snprintf_s(line, sizeof(line), _TRUNCATE, "ui reload: %d of %d documents%s", loaded, (int)reloads.size(), bar ? ", and the control bar" : "");
	RmlUiPrint(line, loaded != (int)reloads.size());
}

void RmlUiManager::toggleDebugger()
{
	if (!m_context)
		return;
	if (!m_debuggerInitialized)
		m_debuggerInitialized = Rml::Debugger::Initialise(m_context);
	if (!m_debuggerInitialized)
	{
		RmlUiPrint("ui debugger: did not start", true);
		return;
	}
	m_debuggerVisible = !m_debuggerVisible;
	Rml::Debugger::SetVisible(m_debuggerVisible);
	RmlUiPrint(m_debuggerVisible ? "ui debugger: shown - its bar is at the top; Element Info, then click an element" : "ui debugger: hidden", false);
}

void RmlUiManager::describe(char *out, int size) const
{
	int shown = 0;
	const int count = m_context ? m_context->GetNumDocuments() : 0;
	for (int i = 0; i < count; ++i)
		if (m_context->GetDocument(i)->IsVisible())
			++shown;
	const char *bar = !m_modernUI ? "off (Options.ini ModernUI = yes turns it on)" : (TheControlBar && TheControlBar->isHeadless()) ? "on" : "on from the next game";
	// the 2003 window that keeps the mouse at the pointer, if one does: the first thing to read when a panel ignores clicks
	const MouseIO *io = TheMouse ? TheMouse->getMouseStatus() : nullptr;
	GameWindow *under = io ? wndWindowUnder(io->pos.x, io->pos.y) : nullptr;
	const char *underName = under ? under->winGetInstanceData()->m_decoratedNameString.str() : "none";
	_snprintf_s(out, size, _TRUNCATE, "ui: RmlUi %s, %d x %d, %d documents, %d shown; modern control bar %s; 2003 window at the pointer: %s",
		Rml::GetVersion().c_str(), m_width, m_height, count, shown, bar, underName);
}

//-------------------------------------------------------------------------------------------------
// Ronin @feature 28/10/2026 DX9: the 2003 windows draw over the layer, so a shown one under the pointer keeps the mouse and an
// open modal one keeps all of it; see-through windows that take no input do not count, as in the window manager's search.
static GameWindow *wndWindowUnder(int x, int y)
{
	if (!TheWindowManager)
		return nullptr;
	GameWindow *win = TheWindowManager->getWindowUnderCursor(x, y);
	return (win && !BitIsSet(win->winGetStatus(), WIN_STATUS_NO_INPUT)) ? win : nullptr;
}

bool RmlUiManager::wndWindowAt(int x, int y) const
{
	return wndWindowUnder(x, y) != nullptr;
}

// Ronin @feature 28/10/2026 DX9: the in-game menu is up (and the Options or Save/Load it opens), or the game is paused: the
// layer's panels take no clicks, as nothing behind the menu should. The debugger, a tool, still does.
bool RmlUiManager::gameHalted() const
{
	if (m_debuggerVisible)
		return false;
	return (TheInGameUI && TheInGameUI->isQuitMenuVisible()) || (TheGameLogic && TheGameLogic->isGamePaused());
}

// A document's box is not enough (a full-screen body with one corner panel would take the whole mouse): what counts is
// an element at the point that takes pointer events. A body opts out with pointer-events: none.
bool RmlUiManager::wantsMouseInput(int x, int y) const
{
	if (!m_context || wndWindowAt(x, y))
		return false;
	if (screenUp())		// Ronin @feature 29/10/2026 DX9: a shell screen takes the whole mouse; nothing behind it is clicked
		return true;
	// Ronin @feature 31/10/2026 DX9: halted, the HUD takes no clicks; an overlay over the halt (the script popup) still does.
	const bool halted = gameHalted();
	for (int i = 0; i < m_context->GetNumDocuments(); ++i)
	{
		Rml::ElementDocument *doc = m_context->GetDocument(i);
		if (!doc || !doc->IsVisible() || (halted && doc == RmlControlBarScreen::instance().document()))
			continue;
		const float left = doc->GetAbsoluteLeft();
		const float top = doc->GetAbsoluteTop();
		if (x < left || y < top || x > left + doc->GetOffsetWidth() || y > top + doc->GetOffsetHeight())
			continue;
		Rml::Element *hit = m_context->GetElementAtPoint(Rml::Vector2f((float)x, (float)y), nullptr, doc);
		if (hit && hit->GetComputedValues().pointer_events() == Rml::Style::PointerEvents::Auto)
			return true;
	}
	return false;
}

// An element marked data-world-middle (the radar, later) leaves the middle button and the wheel to the world.
bool RmlUiManager::leavesMiddleToWorld(int x, int y) const
{
	if (!m_context)
		return false;
	for (Rml::Element *e = m_context->GetElementAtPoint(Rml::Vector2f((float)x, (float)y)); e; e = e->GetParentNode())
		if (e->HasAttribute("data-world-middle"))
			return true;
	return false;
}

bool RmlUiManager::wantsKeyboardInput() const
{
	// a screen for its Escape (processKey passes every other key on); Ronin @feature 31/10/2026 DX9: an overlay's own keys
	return textFieldFocused() || screenUp() || RmlUiScreenRegistry::wantsOverlayKeys();
}

bool RmlUiManager::textFieldFocused() const
{
	Rml::Element *focus = m_context ? m_context->GetFocusElement() : nullptr;
	if (!focus || !focus->IsVisible())
		return false;
	const Rml::String &tag = focus->GetTagName();
	if (tag == "textarea")
		return true;
	if (tag != "input")
		return false;
	const Rml::String type = focus->GetAttribute<Rml::String>("type", "text");
	return type == "text" || type == "password";
}

void RmlUiManager::endHoverOverWorld()
{
	if (!m_context || !TheMouse || !m_context->GetHoverElement())
		return;
	const MouseIO *io = TheMouse->getMouseStatus();
	if (io && !wantsMouseInput(io->pos.x, io->pos.y))
		m_context->ProcessMouseLeave();
}

void RmlUiManager::processMouseMove(int x, int y)
{
	if (m_context)
		m_context->ProcessMouseMove(x, y, keyModifiers());
}

void RmlUiManager::processMouseButton(int button, bool down)
{
	if (!m_context)
		return;
	if (down)
		m_context->ProcessMouseButtonDown(button, keyModifiers());
	else
		m_context->ProcessMouseButtonUp(button, keyModifiers());
}

void RmlUiManager::processMouseWheel(float delta)
{
	if (m_context)
		m_context->ProcessMouseWheel(-delta, keyModifiers());	// the engine counts up as +, RmlUi down as +
}

// Ronin @feature 29/10/2026 DX9: a typed character for the focused field. Control characters come as keys (processKey);
// a character past the BMP (an emoji) comes as two units, high then low: the high one waits for its pair.
void RmlUiManager::processTextInput(unsigned short utf16Char)
{
	if (!m_context || utf16Char < 32 || utf16Char == 127 || !textFieldFocused())
		return;
	Rml::Character codePoint = (Rml::Character)utf16Char;
	if (utf16Char >= 0xD800 && utf16Char <= 0xDBFF)
	{
		m_pendingHighSurrogate = utf16Char;
		return;
	}
	if (utf16Char >= 0xDC00 && utf16Char <= 0xDFFF)
	{
		if (!m_pendingHighSurrogate)
			return;
		codePoint = (Rml::Character)(0x10000 + ((m_pendingHighSurrogate - 0xD800) << 10) + (utf16Char - 0xDC00));
	}
	m_pendingHighSurrogate = 0;
	m_context->ProcessTextInput(codePoint);
}

bool RmlUiManager::processKey(unsigned char engineKey, unsigned short engineKeyState)
{
	if (!m_context)
		return false;
	// Ronin @feature 31/10/2026 DX9: an overlay's own keys come first (the script popup's Enter and Escape), as his do.
	if (RmlUiScreenRegistry::overlayKey(engineKey, engineKeyState))
		return true;
	// Ronin @feature 29/10/2026 DX9: Escape is the newest screen's Back - unless a 2003 popup holds the pointer (a modal one holds
	// it anywhere): the save/load popup and the message boxes keep their own Escape.
	if (engineKey == KEY_ESC && screenUp())
	{
		const MouseIO *io = TheMouse ? TheMouse->getMouseStatus() : nullptr;
		if (io && wndWindowAt(io->pos.x, io->pos.y))
			return false;
		if ((engineKeyState & KEY_STATE_DOWN) != 0 && (engineKeyState & KEY_STATE_AUTOREPEAT) == 0)
			RmlUiScreenRegistry::escape();
		return true;
	}
	if (!textFieldFocused())
		return false;
	if (RmlControlBarScreen::instance().onKey(engineKey, engineKeyState))
		return true;	// Escape in the beacon's text

	const Rml::Input::KeyIdentifier key = engineKeyToRmlKey(engineKey);
	if (key == Rml::Input::KI_UNKNOWN)
		return true;	// a focused field keeps its keys from the game's hotkeys; its characters come from WM_CHAR
	// a held Enter must not send the line again with every repeat (RmlUi's field fires "change" on each Enter down)
	if ((engineKeyState & KEY_STATE_AUTOREPEAT) != 0 && (key == Rml::Input::KI_RETURN || key == Rml::Input::KI_NUMPADENTER))
		return true;
	if ((engineKeyState & KEY_STATE_DOWN) != 0)
		m_context->ProcessKeyDown(key, keyModifiers());
	else
		m_context->ProcessKeyUp(key, keyModifiers());
	return true;
}
