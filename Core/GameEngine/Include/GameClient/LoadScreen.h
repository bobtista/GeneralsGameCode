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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: LoadScreen.h /////////////////////////////////////////////////////////////////////////////////
// Author: Chris Huybregts, March 2002
// Desc:   The file will hold the LoadScreen Base class and it's derived classes
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// SYSTEM INCLUDES ////////////////////////////////////////////////////////////

// USER INCLUDES //////////////////////////////////////////////////////////////
#include "Lib/BaseType.h"
#include "Common/SubsystemInterface.h"
#include "GameClient/GameWindow.h"
#include "GameNetwork/GameInfo.h"
#include "GameClient/CampaignManager.h"
#include "GameClient/ChallengeGenerals.h"
#include "GameClient/WindowVideoManager.h"

// FORWARD REFERENCES /////////////////////////////////////////////////////////

// TYPE DEFINES ///////////////////////////////////////////////////////////////
class VideoBuffer;
class VideoStreamInterface;
class LoadScreenView;



///////////////////////////////////////////////////////////////////////////////////////////////////
// Class LoadScreen is the parent class for each other kind of load screen
///////////////////////////////////////////////////////////////////////////////////////////////////
class LoadScreen
{
public:
	LoadScreen();
	virtual ~LoadScreen();

	virtual void init( GameInfo *game ) = 0;		///< Init the loadscreen
	virtual void reset() = 0;		///< Reset the system
	virtual void update() = 0;  ///< Update the state of the slider bars
	virtual void update( Int percent ); ///< Update the state of the slider bars
	virtual void processProgress(Int playerId, Int percentage) = 0;
	virtual void setProgressRange( Int min, Int max ) = 0;
protected:
	void setLoadScreen( GameWindow *g ) { m_loadScreen = g; }
	void publishData();					///< bump LoadScreenData and refresh the legacy view
	void publishProgress( Int percent );	///< the load bar of the shell, single player and challenge screens
	GameWindow *m_loadScreen;		///< The GameWindow that is our loadscreen
	LoadScreenView *m_view;			///< .wnd presentation of LoadScreenData, null when RmlUi draws the screen (m_loadScreen is then an empty placeholder)

private:

};

///////////////////////////////////////////////////////////////////////////////////////////////////
// class SinglePlayerLoadScreen is to be used only when we're loading a single player mission
///////////////////////////////////////////////////////////////////////////////////////////////////
class SinglePlayerLoadScreen : public LoadScreen
{
public:
	SinglePlayerLoadScreen();
	virtual ~SinglePlayerLoadScreen() override;

	virtual void init( GameInfo *game ) override;		///< Init the loadscreen
	virtual void reset() override;		///< Reset the system
	virtual void update() override
	{
		DEBUG_CRASH(("Call update(Int) instead.  This update isn't supported"));
	};
	virtual void update(Int percent) override;		 ///< Update the state of the progress bar
	virtual void processProgress(Int playerId, Int percentage) override
	{
		DEBUG_CRASH(("We Got to a single player load screen throw the Network..."));
	}

	virtual void setProgressRange( Int min, Int max ) override;

private:
	Int m_currentObjectiveLine;
	Int m_currentObjectiveLineCharacter;
	Int m_currentObjectiveWidthOffset;
	Bool m_finishedObjectiveText;

	UnicodeString m_unicodeObjectiveLines[MAX_OBJECTIVE_LINES];

	VideoBuffer *m_videoBuffer;
	VideoStreamInterface *m_videoStream;

	void moveWindows( Int frame );

	AudioEventRTS m_ambientLoop;
	AudioHandle m_ambientLoopHandle;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// class LoadScreenMovie is one movie of the challenge load screen: WindowVideoManager's playback
// for a single stream, without a window. The screen publishes buffer() as a LoadScreenData video.
///////////////////////////////////////////////////////////////////////////////////////////////////
class LoadScreenMovie
{
public:
	LoadScreenMovie();
	~LoadScreenMovie();

	void play( const AsciiString &movieName, WindowVideoPlayType playType ); ///< stays empty if the movie can't be opened
	void update(); ///< decode the next frame when it is due
	VideoBuffer *buffer() const; ///< what is on screen: null when nothing plays or it stopped

private:
	void release();

	VideoStreamInterface *m_stream;
	VideoBuffer *m_buffer;
	WindowVideoPlayType m_playType;
	WindowVideoStates m_state;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// class ChallengeLoadScreen is to be used only when we're loading a Generals' Challenge mission
///////////////////////////////////////////////////////////////////////////////////////////////////
class ChallengeLoadScreen : public LoadScreen
{
public:
	ChallengeLoadScreen();
	virtual ~ChallengeLoadScreen() override;

	virtual void init( GameInfo *game ) override;		///< Init the loadscreen
	virtual void reset() override;		///< Reset the system
	virtual void update() override
	{
		DEBUG_CRASH(("Call update(Int) instead.  This update isn't supported"));
	};
	virtual void update(Int percent) override;		 ///< Update the state of the progress bar
	virtual void processProgress(Int playerId, Int percentage) override
	{
		DEBUG_CRASH(("We Got to a single player load screen throw the Network..."));
	}

	virtual void setProgressRange( Int min, Int max ) override;

private:
	VideoBuffer *m_videoBuffer;
	VideoStreamInterface *m_videoStream;

	LoadScreenMovie m_portraitMovieLeft;
	LoadScreenMovie m_portraitMovieRight;
	LoadScreenMovie m_versusMovie;

	AudioEventRTS m_ambientLoop;
	AudioHandle m_ambientLoopHandle;

	// full bio texts; LoadScreenData holds what the teletype has shown of them
	UnicodeString m_bioName[2];
	UnicodeString m_bioRank[2];
	UnicodeString m_bioStrategy[2];
	Int m_textPosBigName[2];
	Int m_textPosName[2];
	Int m_textPosRank[2];
	Int m_textPosStrategy[2];

	void activatePieces( Int frame, const GeneralPersona *generalPlayer, const GeneralPersona *generalOpponent );
	void activatePiecesMinSpec(const GeneralPersona *generalPlayer, const GeneralPersona *generalOpponent);
	void updateMovies(); ///< advance the three overlay movies and publish their buffers
};



///////////////////////////////////////////////////////////////////////////////////////////////////
// class ShellGameLoadScreen is to be used for the Shell Game loadscreen
////	///////////////////////////////////////////////////////////////////////////////////////////////
class ShellGameLoadScreen : public LoadScreen
{
public:
	ShellGameLoadScreen();
	virtual ~ShellGameLoadScreen() override;

	virtual void init( GameInfo *game ) override;		///< Init the loadscreen
	virtual void reset() override;		///< Reset the system
	virtual void update() override
	{
		DEBUG_CRASH(("Call update(Int) instead.  This update isn't supported"));
	};
	virtual void update(Int percent) override;		 ///< Update the state of the progress bar
	virtual void processProgress(Int playerId, Int percentage) override
	{
		DEBUG_CRASH(("We Got to a single player load screen throw the Network..."));
	}
	virtual void setProgressRange( Int min, Int max ) override { }

};


///////////////////////////////////////////////////////////////////////////////////////////////////
// class MultiPlayerLoadScreen is to be used for multiplayer communication on the loadscreens
////	///////////////////////////////////////////////////////////////////////////////////////////////
class MultiPlayerLoadScreen : public LoadScreen
{
public:
	MultiPlayerLoadScreen();
	virtual ~MultiPlayerLoadScreen() override;

	virtual void init( GameInfo *game ) override;		///< Init the loadscreen
	virtual void reset() override;		///< Reset the system
	virtual void update() override
	{
		DEBUG_CRASH(("Call update(Int) instead.  This update isn't supported"));
	};
	virtual void update(Int percent) override;		 ///< Update the state of the progress bar
	virtual void processProgress(Int playerId, Int percentage) override;
	virtual void setProgressRange( Int min, Int max ) override { }
private:
	Int m_playerLookup[MAX_SLOTS];					///< lookup table to translate network slot info screen slot (to account for holes in the slot list)
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// class MultiPlayerLoadScreen is to be used for multiplayer communication on the loadscreens
////	///////////////////////////////////////////////////////////////////////////////////////////////
class GameSpyLoadScreen : public LoadScreen
{
public:
	GameSpyLoadScreen();
	virtual ~GameSpyLoadScreen() override;

	virtual void init( GameInfo *game ) override;		///< Init the loadscreen
	virtual void reset() override;		///< Reset the system
	virtual void update() override
	{
		DEBUG_CRASH(("Call update(Int) instead.  This update isn't supported"));
	};
	virtual void update(Int percent) override;		 ///< Update the state of the progress bar
	virtual void processProgress(Int playerId, Int percentage) override;
	virtual void setProgressRange( Int min, Int max ) override { }
private:
	Int m_playerLookup[MAX_SLOTS];					///< lookup table to translate network slot info screen slot (to account for holes in the slot list)
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// class MapTransferLoadScreen is to be used for map transfers before multiplayer game load screens
////	///////////////////////////////////////////////////////////////////////////////////////////////
class MapTransferLoadScreen : public LoadScreen
{
public:
	MapTransferLoadScreen();
	virtual ~MapTransferLoadScreen() override;

	virtual void init( GameInfo *game ) override;		///< Init the loadscreen
	virtual void reset() override;							///< Reset the system
	virtual void update() override
	{
		DEBUG_CRASH(("Call update(Int) instead.  This update isn't supported"));
	};
	virtual void update(Int percent) override;				///< Update the state of the progress bar
	virtual void processProgress(Int playerId, Int percentage) override
	{
		DEBUG_CRASH(("Call processProgress(Int, Int, AsciiString) instead."));
	}
	void processProgress(Int playerId, Int percentage, AsciiString stateStr);
	virtual void setProgressRange( Int min, Int max ) override { }
	void processTimeout(Int secondsLeft);
	void setCurrentFilename(AsciiString filename);
private:
	Int m_playerLookup[MAX_SLOTS];					///< lookup table to translate network slot info screen slot (to account for holes in the slot list)
	Int m_oldProgress[MAX_SLOTS];						///< old vals, so we can call processProgress() every frame and not touch the GUI
	Int m_oldTimeout;												///< old val, so we can call processTimeout() every frame and not touch the GUI
};
