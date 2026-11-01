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
// FILE: DownloadMenuData.h //////////////////////////////////////////////////
// Widget-agnostic state of the download screen (DownloadMenu.wnd and its RmlUi replacement): the file
// being fetched, the byte count, the time left, the status text and the progress. The download
// manager (DownloadMenuActions.cpp) writes it and emits DownloadMenuSignals::changed(); the screens
// only draw it.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/Signal.h"
#include "Common/UnicodeString.h"

struct DownloadMenuData
{
	static DownloadMenuData &instance()
	{
		static DownloadMenuData s_data;
		return s_data;
	}

	void reset(); ///< the screen comes up: nothing shown yet

	UnicodeString m_file; ///< the file name, without its path
	UnicodeString m_size; ///< "bytes of total"
	UnicodeString m_time; ///< time left; stays empty in Generals Online builds
	UnicodeString m_status;
	Int m_percent = 0; ///< 0 to 100
};

namespace DownloadMenuSignals
{
	// Any of the data above changed. Each screen connects while it is up and drops the connection
	// when it goes down.
	Signal0 &changed();
}
