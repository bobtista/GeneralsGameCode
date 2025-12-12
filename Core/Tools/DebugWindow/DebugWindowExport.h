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

#pragma once

// TheSuperHackers @refactor bobtista 01/01/2025 Cross-platform export declarations

#ifdef _WIN32
	#define DEBUGWINDOW_EXPORT __declspec(dllexport)
#else
	#define DEBUGWINDOW_EXPORT __attribute__((visibility("default")))
#endif

extern "C" {
	void DEBUGWINDOW_EXPORT CreateDebugDialog(void);
	void DEBUGWINDOW_EXPORT DestroyDebugDialog(void);
	bool DEBUGWINDOW_EXPORT CanAppContinue(void);
	bool DEBUGWINDOW_EXPORT IsPaused(void);
	void DEBUGWINDOW_EXPORT ForceAppContinue(void);
	bool DEBUGWINDOW_EXPORT RunAppFast(void);
	void DEBUGWINDOW_EXPORT AppendMessage(const char* messageToPass);
	void DEBUGWINDOW_EXPORT SetFrameNumber(int frameNumber);
	void DEBUGWINDOW_EXPORT AppendMessageAndPause(const char* messageToPass);
	void DEBUGWINDOW_EXPORT AdjustVariable(const char* variable, const char* value);
	void DEBUGWINDOW_EXPORT AdjustVariableAndPause(const char* variable, const char* value);
}
