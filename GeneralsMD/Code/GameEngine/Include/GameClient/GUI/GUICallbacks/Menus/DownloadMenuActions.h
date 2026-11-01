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
// FILE: DownloadMenuActions.h ///////////////////////////////////////////////
// Widget-agnostic download screen logic: the download manager that reports the update's file, size,
// status and progress into DownloadMenuData (emitting DownloadMenuSignals::changed()), and what the
// player does about it (cancel). DownloadMenu.cpp's .wnd callbacks and RmlDownloadScreen call these
// instead of duplicating the rules.
///////////////////////////////////////////////////////////////////////////////

#pragma once

class WindowLayout;

namespace DownloadMenuActions
{
	// The screen comes up: forget the last download and start a download manager for the update
	// to report to. layout is what closes with the screen; null if there is nothing to tear down.
	void open( WindowLayout *layout );

	void close(); ///< the screen went down: drop the download manager

	void cancel(); ///< Cancel button and Escape: stops the update and closes the screen

	// Refreshes the time left about once a second (not in Generals Online builds, which show none).
	// The view calls this every frame while it is up.
	void update();
}
