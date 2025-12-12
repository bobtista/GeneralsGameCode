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

#include "DebugWindowDialog.h"
#include "ui_DebugWindowDialog.h"

#include <QtWidgets/QApplication>
#include <QtCore/QString>
#include <QtGui/QCloseEvent>
#include <QtGui/QKeyEvent>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

DebugWindowDialog::DebugWindowDialog(QWidget* parent) :
	QDialog(parent),
	ui(new Ui::DebugWindowDialog),
	mStepping(false),
	mRunFast(false),
	mNumberOfStepsAllowed(-1),
	mMainWndHWND(nullptr),
	mAllowClose(false)
{
	ui->setupUi(this);

#ifdef _WIN32
	mMainWndHWND = ::FindWindow(NULL, L"Command & Conquer: Generals");
#endif

	connect(ui->pauseButton, &QPushButton::clicked, this, &DebugWindowDialog::onPauseClicked);
	connect(ui->stepButton, &QPushButton::clicked, this, &DebugWindowDialog::onStepClicked);
	connect(ui->stepTenButton, &QPushButton::clicked, this, &DebugWindowDialog::onStepTenClicked);
	connect(ui->runFastCheckBox, &QCheckBox::toggled, this, &DebugWindowDialog::onRunFastToggled);
	connect(ui->clearButton, &QPushButton::clicked, this, &DebugWindowDialog::onClearClicked);
}

DebugWindowDialog::~DebugWindowDialog()
{
	delete ui;
}

void* DebugWindowDialog::GetMainWndHWND(void)
{
	return mMainWndHWND;
}

void DebugWindowDialog::ForcePause(void)
{
	mNumberOfStepsAllowed = 0;
	_UpdatePauseButton();
}

void DebugWindowDialog::ForceContinue(void)
{
	mNumberOfStepsAllowed = -1;
	_UpdatePauseButton();
}

void DebugWindowDialog::onPauseClicked()
{
	if (mNumberOfStepsAllowed < 0) {
		mNumberOfStepsAllowed = 0;
		AppendMessage("PAUSED");
	} else {
		mNumberOfStepsAllowed = -1;
		AppendMessage("RESUMED");
	}
	_UpdatePauseButton();
}

void DebugWindowDialog::onStepClicked()
{
	mStepping = true;
	mNumberOfStepsAllowed = 1;
	AppendMessage("STEP (1 frame)");
	_UpdatePauseButton();
}

void DebugWindowDialog::onRunFastToggled(bool checked)
{
	mRunFast = checked;
}

void DebugWindowDialog::onStepTenClicked()
{
	mStepping = true;
	mNumberOfStepsAllowed = 10;
	AppendMessage("STEP 10 (10 frames)");
	_UpdatePauseButton();
}

void DebugWindowDialog::onClearClicked()
{
	mMessages.clear();
	mVariables.clear();
	_RebuildMesgString();
	_RebuildVarsString();
}

bool DebugWindowDialog::CanProceed(void)
{
	if (mNumberOfStepsAllowed < 0) {
		return true;
	} else if (mNumberOfStepsAllowed == 0) {
		if (mStepping) {
			mStepping = false;
			_UpdatePauseButton();
		}
		return false;
	}

	--mNumberOfStepsAllowed;
	return true;
}

bool DebugWindowDialog::RunAppFast(void)
{
	return mRunFast;
}

void DebugWindowDialog::AppendMessage(const std::string& messageToAppend)
{
	mMessages.push_back(messageToAppend);
	_RebuildMesgString();
}

void DebugWindowDialog::AdjustVariable(const std::string& varName, const std::string& varValue)
{
	for (VecPairStringIt it = mVariables.begin(); it != mVariables.end(); it++) {
		if (it->first == varName) {
			it->second = varValue;
			_RebuildVarsString();
			return;
		}
	}

	PairString newPair;
	newPair.first = varName;
	newPair.second = varValue;
	mVariables.push_back(newPair);

	_RebuildVarsString();
}

void DebugWindowDialog::SetFrameNumber(int frameNumber)
{
	mFrameNumber = std::to_string(frameNumber);
	QString frameText = QString::fromStdString(mFrameNumber);
	ui->frameNumberLabel->setText(frameText);
	ui->frameNumberLabel->update();
}

void DebugWindowDialog::_RebuildVarsString(void)
{
	mVariablesString = "";

	for (VecPairStringIt it = mVariables.begin(); it != mVariables.end(); it++) {
		mVariablesString += it->first;
		mVariablesString += " = ";
		mVariablesString += it->second;
		mVariablesString += "\n";
	}

	ui->variablesTextEdit->setPlainText(QString::fromStdString(mVariablesString));
}

void DebugWindowDialog::_RebuildMesgString(void)
{
	mMessagesString = "";

	for (VecStringIt it = mMessages.begin(); it != mMessages.end(); it++) {
		mMessagesString += (*it);
		mMessagesString += "\n";
	}

	ui->messagesTextEdit->setPlainText(QString::fromStdString(mMessagesString));
	
	QTextCursor cursor = ui->messagesTextEdit->textCursor();
	cursor.movePosition(QTextCursor::End);
	ui->messagesTextEdit->setTextCursor(cursor);
}

void DebugWindowDialog::_UpdatePauseButton(void)
{
	if (!ui->pauseButton) {
		return;
	}

	ui->pauseButton->setChecked(mNumberOfStepsAllowed == 0);
}

void DebugWindowDialog::closeEvent(QCloseEvent* event)
{
	if (mAllowClose) {
		event->accept();
		QDialog::closeEvent(event);
	} else {
		showMinimized();
		event->ignore();
	}
}

void DebugWindowDialog::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::WindowStateChange) {
		QWindowStateChangeEvent* stateEvent = static_cast<QWindowStateChangeEvent*>(event);
		if (isMinimized()) {
#ifdef _WIN32
			if (mMainWndHWND) {
				::SetFocus(static_cast<HWND>(mMainWndHWND));
			}
#endif
		}
	}
	QDialog::changeEvent(event);
}

void DebugWindowDialog::keyPressEvent(QKeyEvent* event)
{
	if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
		if (mAllowClose) {
			close();
		}
	} else {
		QDialog::keyPressEvent(event);
	}
}
