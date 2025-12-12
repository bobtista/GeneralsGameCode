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
**	BUT WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// TheSuperHackers @refactor bobtista 01/01/2025 Migrated from MFC to Qt

#include "DebugWindow.h"
#include "DebugWindowDialog.h"
#include "DebugWindowExport.h"

#include <QtWidgets/QApplication>
#include <QtCore/QCoreApplication>

#ifdef _WIN32
#include <windows.h>
#endif

DebugWindowManager::DebugWindowManager() :
	m_DialogWindow(nullptr)
{
}

DebugWindowDialog* DebugWindowManager::GetDialogWindow(void)
{
	return m_DialogWindow;
}

void DebugWindowManager::SetDialogWindow(DebugWindowDialog* pWnd)
{
	m_DialogWindow = pWnd;
}

DebugWindowManager::~DebugWindowManager()
{
}

DebugWindowManager theManager;

extern "C" {

void CreateDebugDialog(void)
{
	if (!QCoreApplication::instance()) {
		static int argc = 1;
		static char* argv[] = { const_cast<char*>("DebugWindow"), nullptr };
		new QApplication(argc, argv);
	}

	DebugWindowDialog* tmpWnd = new DebugWindowDialog;
	if (QCoreApplication::instance() && qobject_cast<QApplication*>(QCoreApplication::instance())) {
		tmpWnd->SetAllowClose(true);
	}
	tmpWnd->show();
	
	void* mainWnd = tmpWnd->GetMainWndHWND();
	if (mainWnd) {
#ifdef _WIN32
		::SetFocus(static_cast<HWND>(mainWnd));
#endif
	}

	theManager.SetDialogWindow(tmpWnd);
}

void DestroyDebugDialog(void)
{
	DebugWindowDialog* tmpWnd = theManager.GetDialogWindow();

	if (tmpWnd) {
		tmpWnd->close();
		delete tmpWnd;
		theManager.SetDialogWindow(nullptr);
	}
}

bool CanAppContinue(void)
{
	DebugWindowDialog* pDbg = theManager.GetDialogWindow();
	if (!pDbg) {
		return true;
	}

	return pDbg->CanProceed();
}

bool IsPaused(void)
{
	DebugWindowDialog* pDbg = theManager.GetDialogWindow();
	if (!pDbg) {
		return false;
	}

	return pDbg->IsPaused();
}

void ForceAppContinue(void)
{
	DebugWindowDialog* pDbg = theManager.GetDialogWindow();
	if (!pDbg) {
		return;
	}

	pDbg->ForceContinue();
}

bool RunAppFast(void)
{
	DebugWindowDialog* pDbg = theManager.GetDialogWindow();
	if (!pDbg) {
		return true;
	}

	return pDbg->RunAppFast();
}

void AppendMessage(const char* messageToPass)
{
	DebugWindowDialog* pDbg = theManager.GetDialogWindow();

	if (pDbg) {
		pDbg->AppendMessage(messageToPass);
	}
}

void SetFrameNumber(int frameNumber)
{
	DebugWindowDialog* pDbg = theManager.GetDialogWindow();

	if (pDbg) {
		pDbg->SetFrameNumber(frameNumber);
	}
}

void AppendMessageAndPause(const char* messageToPass)
{
	DebugWindowDialog* pDbg = theManager.GetDialogWindow();

	if (pDbg) {
		pDbg->AppendMessage(messageToPass);
		pDbg->ForcePause();
	}
}

void AdjustVariable(const char* variable, const char* value)
{
	DebugWindowDialog* pDbg = theManager.GetDialogWindow();

	if (pDbg) {
		pDbg->AdjustVariable(variable, value);
	}
}

void AdjustVariableAndPause(const char* variable, const char* value)
{
	DebugWindowDialog* pDbg = theManager.GetDialogWindow();

	if (pDbg) {
		pDbg->AdjustVariable(variable, value);
		pDbg->ForcePause();
	}
}

}
