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
#include "W3DDevice/GameClient/RmlUi/RmlMessageBox.h"

#include "Common/UnicodeString.h"
#include "Common/UnicodeUtf8.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/RmlUiScreenRegistry.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>

#include <windows.h>

namespace
{
	Rml::Context *s_context = nullptr;
	Rml::ElementDocument *s_document = nullptr;
	Rml::DataModelHandle s_modelHandle;

	// Id of the open box (0 = none); the placeholder GameWindow gogoMessageBox() returned carries it.
	UnsignedInt s_currentId = 0;
	UnsignedInt s_nextId = 1;

	GameWinMsgBoxFunc s_yesCallback = nullptr;
	GameWinMsgBoxFunc s_noCallback = nullptr;
	GameWinMsgBoxFunc s_okCallback = nullptr;
	GameWinMsgBoxFunc s_cancelCallback = nullptr;

	struct Model
	{
		Rml::String title;
		Rml::String body;
		bool showOk = false;
		bool showYes = false;
		bool showNo = false;
		bool showCancel = false;
		bool useLogo = false;
		Rml::String okLabel, yesLabel, noLabel, cancelLabel; ///< empty = default gametext label
	};
	Model &model()
	{
		static Model s_model;
		return s_model;
	}

	// Tears down whatever box is currently showing, without running its callback. Only reachable
	// two ways: RmlUiManager::shutdown(), or a new gogoMessageBox() call arriving while one is
	// already open. The latter never happens for a box still awaiting its own click -- the .wnd
	// version only ever opens a new box from inside an old one's own Ok/Yes/No/Cancel callback
	// (see e.g. PopupReplay::reallySaveReplay opening an error box), and that old box is already
	// spent by the time its callback runs and calls back in here; this just finishes its teardown
	// a little earlier than the normal post-callback winDestroy would have.
	void closeCurrent()
	{
		if (s_document && s_context)
		{
			s_context->UnloadDocument(s_document);
			s_context->RemoveDataModel("messagebox");
		}
		s_document = nullptr;
		s_currentId = 0;
		s_yesCallback = s_noCallback = s_okCallback = s_cancelCallback = nullptr;
	}

	// winDestroy() of a placeholder: ignore ids of boxes that were already replaced or clicked.
	void closeById(UnsignedInt id)
	{
		if (id == 0 || id == s_currentId)
			closeCurrent();
	}

	// Mirrors MessageBoxSystem: run the callback, then destroy the box's window.
	void finishClick(UnsignedInt id, GameWinMsgBoxFunc callback)
	{
		if (callback)
			callback();
		RmlUiScreenRegistry::destroyMessageBox(id);
	}

	void onOk(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &)
	{
		GameWinMsgBoxFunc callback = s_okCallback;
		const UnsignedInt id = s_currentId;
		closeCurrent();
		finishClick(id, callback);
	}
	void onYes(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &)
	{
		GameWinMsgBoxFunc callback = s_yesCallback;
		const UnsignedInt id = s_currentId;
		closeCurrent();
		finishClick(id, callback);
	}
	void onNo(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &)
	{
		GameWinMsgBoxFunc callback = s_noCallback;
		const UnsignedInt id = s_currentId;
		closeCurrent();
		finishClick(id, callback);
	}
	void onCancel(Rml::DataModelHandle, Rml::Event &, const Rml::VariantList &)
	{
		GameWinMsgBoxFunc callback = s_cancelCallback;
		const UnsignedInt id = s_currentId;
		closeCurrent();
		finishClick(id, callback);
	}

	UnsignedInt ShowRmlMessageBox(UnsignedShort buttonFlags, const UnicodeString &titleString, const UnicodeString &bodyString,
		GameWinMsgBoxFunc yesCallback, GameWinMsgBoxFunc noCallback,
		GameWinMsgBoxFunc okCallback, GameWinMsgBoxFunc cancelCallback,
		Bool useLogo, const RmlUiMessageBoxLabels &labels)
	{
		if (!s_context)
			return 0;

		closeCurrent(); // see closeCurrent() comment: only matters for the nested-open case

		Model &m = model();
		m.title = unicodeToUtf8(titleString);
		m.body = unicodeToUtf8(bodyString);
		// Same slot rule as GameWindowManager::gogoMessageBox: Ok and Yes never coexist, and
		// share the primary slot; No/Cancel fill the remaining slot(s) in that order.
		m.showOk = (buttonFlags & MSG_BOX_OK) != 0;
		m.showYes = !m.showOk && (buttonFlags & MSG_BOX_YES) != 0;
		m.showNo = (buttonFlags & MSG_BOX_NO) != 0;
		m.showCancel = (buttonFlags & MSG_BOX_CANCEL) != 0;
		m.useLogo = useLogo != 0;
		m.okLabel = unicodeToUtf8(labels.ok);
		m.yesLabel = unicodeToUtf8(labels.yes);
		m.noLabel = unicodeToUtf8(labels.no);
		m.cancelLabel = unicodeToUtf8(labels.cancel);

		s_currentId = s_nextId++;
		s_yesCallback = yesCallback;
		s_noCallback = noCallback;
		s_okCallback = okCallback;
		s_cancelCallback = cancelCallback;

		Rml::DataModelConstructor constructor = s_context->CreateDataModel("messagebox");
		if (constructor)
		{
			constructor.Bind("title", &m.title);
			constructor.Bind("body", &m.body);
			constructor.Bind("show_ok", &m.showOk);
			constructor.Bind("show_yes", &m.showYes);
			constructor.Bind("show_no", &m.showNo);
			constructor.Bind("show_cancel", &m.showCancel);
			constructor.Bind("use_logo", &m.useLogo);
			constructor.Bind("ok_label", &m.okLabel);
			constructor.Bind("yes_label", &m.yesLabel);
			constructor.Bind("no_label", &m.noLabel);
			constructor.Bind("cancel_label", &m.cancelLabel);
			constructor.BindEventCallback("ok", &onOk);
			constructor.BindEventCallback("yes", &onYes);
			constructor.BindEventCallback("no", &onNo);
			constructor.BindEventCallback("cancel_click", &onCancel);
			s_modelHandle = constructor.GetModelHandle();
		}

		s_document = s_context->LoadDocument("Data/UI/MessageBox.rml");	// Ronin @feature 31/10/2026 DX9: our documents live in Data/UI
		if (s_document)
			s_document->Show(Rml::ModalFlag::Modal);
		return s_currentId;
	}

	void raiseBox()
	{
		if (!s_document || !s_context)
			return;
		const int count = s_context->GetNumDocuments();
		if (count > 0 && s_context->GetDocument(count - 1) != s_document)
			s_document->PullToFront();
	}
}

bool AnyRmlMessageBoxOpen()
{
	return s_document != nullptr;
}

void RegisterRmlMessageBoxHook(Rml::Context *context)
{
	s_context = context;
	RmlUiMessageBoxHook::setHandler(&ShowRmlMessageBox);
	RmlUiMessageBoxHook::setCloseHandler(&closeById);
	RmlUiMessageBoxHook::setRaiseHandler(&raiseBox);
	RmlUiMessageBoxHook::setOpenQuery(&AnyRmlMessageBoxOpen);
}

void UnregisterRmlMessageBoxHook()
{
	RmlUiMessageBoxHook::setHandler(nullptr);
	RmlUiMessageBoxHook::setCloseHandler(nullptr);
	RmlUiMessageBoxHook::setRaiseHandler(nullptr);
	RmlUiMessageBoxHook::setOpenQuery(nullptr);
	closeCurrent();
	s_context = nullptr;
}
