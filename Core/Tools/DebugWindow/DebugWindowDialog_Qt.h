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

// TheSuperHackers @refactor bobtista 01/01/2025 Migrated from MFC CDialog to Qt QDialog

#pragma once

#include <QtWidgets/QDialog>
#include <map>
#include <string>
#include <vector>

QT_BEGIN_NAMESPACE
class QTextEdit;
class QLabel;
class QPushButton;
class QCheckBox;
QT_END_NAMESPACE

namespace Ui {
class DebugWindowDialog;
}

typedef std::pair<std::string, std::string>	PairString;
typedef std::vector<PairString>				VecPairString;
typedef std::vector<std::string>			VecString;

typedef std::vector<PairString>::iterator	VecPairStringIt;
typedef std::vector<std::string>::iterator	VecStringIt;

class DebugWindowDialog : public QDialog
{
	Q_OBJECT

public:
	explicit DebugWindowDialog(QWidget* parent = nullptr);
	~DebugWindowDialog();

	bool CanProceed(void);
	bool RunAppFast(void);
	void AppendMessage(const std::string& messageToAppend);
	void AdjustVariable(const std::string& varName, const std::string& varValue);
	void SetFrameNumber(int frameNumber);
	void* GetMainWndHWND(void);
	void ForcePause(void);
	void ForceContinue(void);
	void SetAllowClose(bool allow) { mAllowClose = allow; }
	bool IsPaused(void) const { return mNumberOfStepsAllowed == 0; }

protected:
	void closeEvent(QCloseEvent* event) override;
	void changeEvent(QEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;

private slots:
	void onPauseClicked();
	void onStepClicked();
	void onStepTenClicked();
	void onRunFastToggled(bool checked);
	void onClearClicked();

private:
	void _RebuildVarsString(void);
	void _RebuildMesgString(void);
	void _UpdatePauseButton(void);

	Ui::DebugWindowDialog* ui;

	void* mMainWndHWND;
	int mNumberOfStepsAllowed;
	std::string mVariablesString;
	std::string mMessagesString;
	std::string mFrameNumber;
	bool mStepping;
	bool mRunFast;
	bool mAllowClose;

	VecPairString mVariables;
	VecString mMessages;
};
