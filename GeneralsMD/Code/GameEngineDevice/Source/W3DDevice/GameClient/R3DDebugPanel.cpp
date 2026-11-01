/*
**	Command & Conquer Generals Zero Hour(tm)
**	DX9 debug panel — hosts the HUD readouts in one translucent, draggable game window.
*/

// Ronin @feature 14/09/2026 DX9: debug panel. Design, steps and what the window system requires: docs/Debug_Panel_Design.md.

#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <tlhelp32.h>

#include "Common/GameEngine.h"
#include "Common/GameMemory.h"
#include "Common/GlobalData.h"
#include "Common/MessageStream.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/Recorder.h"
#include "Common/ThingFactory.h"
#include "Common/ThingSort.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"	// Ronin @feature 06/10/2026 DX9: `timesetting` walks the objects
#include "GameLogic/PartitionManager.h"	// Ronin @feature 06/10/2026 DX9: `mapvision` reads the shroud
#include "GameLogic/TerrainLogic.h"
#include "GameClient/Color.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayString.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameClient.h"
#include "GameClient/Gadget.h"
#include "GameClient/GadgetListBox.h"
#include "GameClient/GadgetTextEntry.h"
#include "GameClient/GameFont.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Keyboard.h"
#include "GameClient/KeyDefs.h"
#include "GameClient/Mouse.h"
#include "GameClient/SelectionXlat.h"
#include "GameClient/View.h"
#include "W3DDevice/GameClient/R3DDebugGrid.h"
#include "W3DDevice/GameClient/R3DDebugPanel.h"
#include "W3DDevice/GameClient/W3DWater.h"
#include "WW3D2/dx8wrapper.h"
#include "W3DDevice/GameClient/R3DTaa.h"
#include "W3DDevice/GameClient/R3DSsao.h"	// Ronin @feature 26/09/2026 DX9: `ssao` command
#include "W3DDevice/GameClient/R3DLocalLights.h"	// Ronin @feature 09/10/2026 DX9: `lights` command and the [LIGHT] row
#include "W3DDevice/GameClient/R3DShadowMap.h"	// Ronin @feature 26/09/2026 DX9: `shadows` command
#include "W3DDevice/GameClient/R3DTreeFire.h"	// Ronin @feature 25/10/2026 DX9: `treefire` command
// Ronin @diagnostic 28/09/2026 DX9: [WATER] row - chipset gate and the soft-edge test.
#include "W3DDevice/GameClient/W3DShaderManager.h"
#include "GameClient/Water.h"
#include "W3DDevice/GameClient/R3DWaterSeaTuning.h"	// Ronin @feature 29/09/2026 DX9: the WaterSea look knobs
#include "W3DDevice/GameClient/R3DWaterSeaIni.h"	// Ronin @feature 04/10/2026 DX9: `water save` / `water load`
#include "W3DDevice/GameClient/R3DSkyCube.h"		// Ronin @feature 12/10/2026 DX9: `sky`
#include "GameClient/ParticleSys.h"				// Ronin @diagnostic 15/10/2026 DX9: `glowfx`
// Ronin @feature 28/09/2026 DX9: the readouts moved in from W3DDisplay.cpp.
#include "W3DDevice/GameClient/BaseHeightMap.h"
#include "W3DDevice/GameClient/WorldHeightMap.h"
#include "WW3D2/dx8instancing.h"
#include "WW3D2/statistics.h"

// Ronin @feature 28/09/2026 DX9: [SR] counters, in dx8instancing.cpp. Declared out here: inside the namespace below they
// would name internal functions that do not exist.
extern unsigned DX8_Get_Single_Rigid_Last_Frame_Draw_Count();
extern unsigned DX8_Get_Single_Rigid_Last_Frame_Flush_Count();

namespace
{
	// Ronin @feature 14/09/2026 DX9: ids no .wnd layout uses. Both windows are looked up every frame, because
	// GameWindowManager::reset() destroys every window and a cached pointer would dangle.
	const Int PANEL_WINDOW_ID = 0x7DEB0001;
	const Int ENTRY_WINDOW_ID = 0x7DEB0002;
	const Int LIST_WINDOW_ID  = 0x7DEB0003;	// Ronin @feature 16/09/2026 DX9: spawn picker list
	const Int PAD             = 4;
	const Int TITLE_HEIGHT    = 14;
	const Int ENTRY_HEIGHT    = 20;			// the entry draws its text 5 px down (W3DTextEntry.cpp:301)
	const Int ENTRY_MIN_WIDTH = 320;
	const Int OUTPUT_WRAP     = 110;		// Ronin @bugfix 27/09/2026 DX9: output characters per line before printAscii wraps
	const Int DEFAULT_X       = 3;
	const Int DEFAULT_Y       = 300;		// where the first readout line used to sit
	// Ronin @feature 16/09/2026 DX9: spawn picker size; the list's column width is set once at creation, so its width is fixed.
	const Int PICKER_WIDTH    = 600;
	const Int PICKER_HEIGHT   = 240;
	const Int LABEL_HEIGHT    = 14;
	// Ronin @feature 15/09/2026 DX9: OUTPUT_LINES 6 -> 10, `help` plus its echo no longer fitted.
	// Ronin @feature 03/10/2026 DX9: 10 -> 16 - the `water` table and its echo take 14.
	enum { OUTPUT_LINES = 16, HISTORY_SIZE = 16, PANEL_MAX_COMMANDS = 64, MAX_ARGS = 16, PICK_ROWS = 400, MAX_SIDES = 48,
		   // Ronin @feature 16/09/2026 DX9: copies per drop, and ghosts previewing them. Structures are big and rarely wanted
		   // in bulk; units are cheap to look at and useful in numbers.
		   PLACE_MAX_UNITS = 30, PLACE_MAX_STRUCTURES = 3, PLACE_MAX_COUNT = PLACE_MAX_UNITS };

	struct PanelRow
	{
		DisplayString *text;
		Color          color;
		UnsignedInt    stamp;					// panel frame the row was last set in
	};

	struct PanelCommand
	{
		const char                 *name;
		const char                 *help;
		R3DDebugPanel::CommandFunc  func;
	};

	PanelRow       s_rows[R3DDebugPanel::ROW_COUNT] = {};
	UnsignedInt    s_frame        = 1;
	Bool           s_available    = FALSE;
	Bool           s_visible      = false;
	Int            s_posX         = DEFAULT_X;
	Int            s_posY         = DEFAULT_Y;
	Int            s_outputTop    = 0;		// output area's y inside the panel, set by update for the draw
	DisplayString *s_title        = nullptr;
	Bool           s_mouseWasDown = FALSE;

	DisplayString *s_out[OUTPUT_LINES]      = {};
	Color          s_outColor[OUTPUT_LINES] = {};
	Int            s_outCount               = 0;
	// Ronin @feature 21/10/2026 DX9: SCROLLBACK - the output keeps its last OUTPUT_BACK lines and shows OUTPUT_LINES of them. PgUp / PgDn
	// in the command box or the wheel over the panel move the view; new output brings it back to the newest.
	enum { OUTPUT_BACK = 512 };
	UnicodeString  s_backText[OUTPUT_BACK];
	Color          s_backColor[OUTPUT_BACK] = {};
	Int            s_backStart  = 0;		// the oldest line's slot
	Int            s_backCount  = 0;
	Int            s_backScroll = 0;		// how many lines the view's last line sits above the newest

	AsciiString    s_history[HISTORY_SIZE];
	Int            s_historyCount = 0;
	Int            s_historyPos   = 0;		// == s_historyCount means "past the newest", an empty box

	PanelCommand   s_commands[PANEL_MAX_COMMANDS] = {};
	Int            s_commandCount = 0;
	Bool           s_builtinsDone = FALSE;

	// Ronin @feature 28/09/2026 DX9: `rows` switches, defaults as W3DDisplay.cpp's old SHOW_* constants. [DRAW2] shares draw's
	// name, so one switch drives both.
	const char    *const ROW_NAMES[R3DDebugPanel::ROW_COUNT] =
		{ "sr", "inst", "draw", "draw", "shadow", "perf", "depth", "terrain", "ui", "rstate", "taa", "water", "light", "light" };
	Bool           s_rowOn[R3DDebugPanel::ROW_COUNT] =
		{ TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, TRUE, FALSE, TRUE, TRUE, TRUE, TRUE };
	DisplayString *s_rowText[R3DDebugPanel::ROW_COUNT] = {};	// each row's string, made on first use
	DisplayString *s_noPanel      = nullptr;

	// Ronin @feature 28/09/2026 DX9: [DRAW] and [PERF] read monotonic counters, so they are sampled every frame - hidden,
	// switched off or with no panel - or a skipped frame folds into the next reading.
	unsigned       s_drawPrev[Debug_Statistics::DRAW_SUBSYS_COUNT] = {};
	unsigned       s_drawNow[Debug_Statistics::DRAW_SUBSYS_COUNT]  = {};	// this frame's draws per subsystem
	unsigned       s_drawTotal    = 0;
	Int64          s_perfFreq     = 0;
	Int64          s_perfNow      = 0;
	Int64          s_perfFirst    = 0;
	Int64          s_perfLast     = 0;
	UnsignedInt    s_perfFrames   = 0;
	double         s_perfSMSum    = 0.0;
	Real           s_perfWorstMs  = 0.0f;
	WaterDebugStats s_waterNow    = {};		// Ronin @diagnostic 28/09/2026 DX9: [WATER], last frame's copy of TheWaterStats

	// Ronin @feature 16/09/2026 DX9: spawn picker. The categories it lists, in [Category] label order, with their row colour.
	struct PickCategory
	{
		EditorSortingType  sorting;
		const char        *name;
		UnsignedByte       r, g, b;
	};
	const PickCategory PICK_CATEGORIES[] =
	{
		{ ES_STRUCTURE,     "Structure", 255, 210, 120 },
		{ ES_INFANTRY,      "Infantry",  140, 230, 140 },
		{ ES_VEHICLE,       "Vehicle",   130, 200, 255 },
		{ ES_MISC_MAN_MADE, "ManMade",   210, 210, 210 },
		{ ES_MISC_NATURAL,  "Natural",   180, 200, 150 },
		{ ES_SHRUBBERY,     "Shrubbery", 150, 190, 130 },
	};
	const Int PICK_CATEGORY_COUNT = (Int)(sizeof(PICK_CATEGORIES) / sizeof(PICK_CATEGORIES[0]));

	Bool           s_pickerOpen      = FALSE;
	Bool           s_pickerDirty     = TRUE;	// refill next frame: a label changed or the list was recreated
	UnicodeString  s_pickerText;				// box text the list was last filled from
	Int            s_pickCount       = 1;		// a number typed after "spawn "
	Int            s_pickedID        = -1;		// template id clicked, spawned by update() next frame
	Bool           s_listPressed     = FALSE;	// the left press started on the list
	Int            s_sideIndex       = -1;		// -1 = All
	Int            s_categoryIndex   = -1;		// -1 = All
	AsciiString    s_sides[MAX_SIDES];
	Int            s_sideCount       = -1;		// -1 = not collected yet
	DisplayString *s_sideLabel       = nullptr;
	DisplayString *s_categoryLabel   = nullptr;
	Int            s_labelTop        = 0;		// labels' y inside the panel, set by update for the draw and the click test
	Int            s_sideLabelW      = 0;
	Int            s_categoryLabelX  = 0;
	Int            s_categoryLabelW  = 0;
	Int            s_pressX          = 0;		// last left press on the panel, to tell a label click from a drag
	Int            s_pressY          = 0;

	// Ronin @feature 16/09/2026 DX9: placement mode. The picked object rides the cursor until a click drops one.
	Int            s_placeID         = -1;		// template id on the cursor, -1 = not placing
	Int            s_placeCount      = 1;
	DrawableID     s_ghostIDs[PLACE_MAX_COUNT] = {};	// one ghost per copy, laid out like the drop will be
	Int            s_ghostCount      = 0;
	ICoord2D       s_placePress      = { 0, 0 };	// where the last press landed, to tell a click from a drag
	Bool           s_eatEscUp        = FALSE;	// the Esc that stopped placing must not reach the menu on its way up
	Bool           s_translatorReady = FALSE;
	// Ronin @feature 16/09/2026 DX9: facing. Holding the left button pins the spot and the drag turns the object, like the
	// dozer's placement arrow; the angle is kept for the next drop, and travels with the spawn message.
	Real           s_placeAngle      = 0.0f;
	Bool           s_placeAnchored   = FALSE;		// left button held down over the map
	Coord3D        s_placeAnchor     = { 0.0f, 0.0f, 0.0f };
	const Int      PLACE_DRAG_PIXELS = 5;			// same slack as PlaceEventTranslator's placement arrow
	// Ronin @feature 16/09/2026 DX9: snapping, while Ctrl is held.
	// Ronin @feature 27/09/2026 DX9: 20 = two height-map cells (MAP_XY_FACTOR is 10). The grid draws this same cell.
	const Real     SNAP_CELL         = 20.0f;
	const Real     SNAP_ANGLE        = 1.5707963f;	// a quarter turn; 0.7853982f would snap to eighths instead

	GameFont *panelFont()
	{
		return (TheFontLibrary != nullptr) ? TheFontLibrary->getFont("FixedSys", 8, FALSE) : nullptr;
	}

	// Ronin @feature 28/09/2026 DX9: every row is set inside update(), before the panel draws, so only this frame's rows are
	// live - a row switched off is gone the same frame. (The one-frame grace was for readouts that ran after the draw.)
	Bool rowIsLive(Int r)
	{
		return s_rows[r].text != nullptr && s_rows[r].stamp == s_frame;
	}

	// Ronin @feature 21/10/2026 DX9: the scrollback's window into the shown lines; scrolled up, the last one says what waits below.
	void showOutput()
	{
		if (TheDisplayStringManager == nullptr)
			return;
		const Int maxScroll = (s_backCount > OUTPUT_LINES) ? (s_backCount - OUTPUT_LINES) : 0;
		s_backScroll = (s_backScroll < 0) ? 0 : ((s_backScroll > maxScroll) ? maxScroll : s_backScroll);
		const Int n     = (s_backCount < OUTPUT_LINES) ? s_backCount : OUTPUT_LINES;
		const Int first = s_backCount - s_backScroll - n;
		for (Int i = 0; i < n; ++i)
		{
			if (s_out[i] == nullptr)
			{
				s_out[i] = TheDisplayStringManager->newDisplayString();
				if (s_out[i] == nullptr)
				{
					s_outCount = i;
					return;
				}
				s_out[i]->setFont(panelFont());
			}
			const Int slot = (s_backStart + first + i) % OUTPUT_BACK;
			s_out[i]->setText(s_backText[slot]);
			s_outColor[i] = s_backColor[slot];
		}
		s_outCount = n;
		if (s_backScroll > 0 && n > 0)
		{
			UnicodeString more;
			more.format(L"  ... %d newer line%s below - PgDn or the mouse wheel", s_backScroll, (s_backScroll == 1) ? L"" : L"s");
			s_out[n - 1]->setText(more);
			s_outColor[n - 1] = GameMakeColor(255, 220, 120, 255);
		}
	}

	void scrollOutput(Int lines)
	{
		s_backScroll += lines;
		showOutput();
	}

	// Ronin @feature 03/10/2026 DX9: `color` 0 = the default grey (the `water` table colours its headers).
	void printLine(const UnicodeString &text, Bool isError, Color color = 0)
	{
		Int slot;
		if (s_backCount < OUTPUT_BACK)
		{
			slot = (s_backStart + s_backCount) % OUTPUT_BACK;
			++s_backCount;
		}
		else
		{
			slot = s_backStart;		// full: the oldest line goes
			s_backStart = (s_backStart + 1) % OUTPUT_BACK;
		}
		s_backText[slot]  = text;
		s_backColor[slot] = isError ? GameMakeColor(255, 120, 80, 255) : (color != 0) ? color : GameMakeColor(220, 220, 220, 255);
		s_backScroll = 0;
		showOutput();
	}

	// Ronin @bugfix 27/09/2026 DX9: the panel sizes to its widest line, so one long line pushed it past the screen edge and
	// hid the command box. Wrapped at a space every OUTPUT_WRAP characters; continuation lines are indented.
	void printAscii(const AsciiString &text, Bool isError, Color color = 0)
	{
		const char *s   = text.str();
		Int         len = (Int)strlen(s);
		Bool        first = TRUE;
		do
		{
			const Int room = first ? OUTPUT_WRAP : OUTPUT_WRAP - 2;
			Int take = len;
			if (len > room)
			{
				take = room;
				while (take > room / 2 && s[take] != ' ')
					--take;
				if (s[take] != ' ')
					take = room;		// no space in the second half: cut hard
			}
			char buf[OUTPUT_WRAP + 1];
			Int  n = 0;
			if (!first)
			{
				buf[n++] = ' ';
				buf[n++] = ' ';
			}
			memcpy(buf + n, s, take);
			buf[n + take] = 0;
			UnicodeString u;
			u.translate(AsciiString(buf));
			printLine(u, isError, color);
			s   += take;
			len -= take;
			while (len > 0 && *s == ' ')
			{
				++s;
				--len;
			}
			first = FALSE;
		} while (len > 0);
	}

	void cmdHelp(Int argc, const AsciiString *argv)
	{
		for (Int i = 0; i < s_commandCount; ++i)
		{
			// Ronin @feature 15/09/2026 DX9: `help <command>` shows just that one.
			if (argc >= 2 && stricmp(s_commands[i].name, argv[1].str()) != 0)
				continue;
			AsciiString line;
			line.format("%s - %s", s_commands[i].name, s_commands[i].help);
			printAscii(line, FALSE);
		}
		if (argc < 2)		// Ronin @feature 21/10/2026 DX9: the list is longer than the panel shows
			printAscii(AsciiString("(PgUp / PgDn in the command box, or the mouse wheel over the panel, scroll this output)"), FALSE, GameMakeColor(255, 220, 120, 255));
	}

	void cmdClear(Int argc, const AsciiString *argv)
	{
		s_outCount   = 0;
		s_backStart  = 0;		// Ronin @feature 21/10/2026 DX9: and the scrollback
		s_backCount  = 0;
		s_backScroll = 0;
	}

	// Ronin @feature 14/09/2026 DX9: same as Ctrl+Shift+Z. update() hides the panel next frame and hands the keys back.
	void cmdClose(Int argc, const AsciiString *argv)
	{
		s_visible = FALSE;
	}

	// Ronin @feature 14/09/2026 DX9: the engine's own benchmark exit (GameEngine.cpp:968-976) — finish the replay, drop the
	// match, quit. Not the Alt+F4 path: in a match that opens the quit menu first. Online, the others see a disconnect.
	void cmdShutdown(Int argc, const AsciiString *argv)
	{
		if (TheGameLogic != nullptr && TheGameLogic->isInGame())
		{
			if (TheRecorder != nullptr && TheRecorder->getMode() == RECORDERMODETYPE_RECORD)
				TheRecorder->stopRecording();
			TheGameLogic->clearGameData(FALSE);
		}
		if (TheGameEngine != nullptr)
			TheGameEngine->setQuitting(TRUE);
	}

	// Ronin @feature 15/09/2026 DX9: water on/off. `water` prints the switches; `water <flat|mesh> <0|1>` sets one.
	// Ronin @feature 04/10/2026 DX9: the WaterSea look knobs live in one table, TheWaterProfileKnobs /
	// TheWaterGlobalKnobs (R3DWaterSeaTuning.h): the water ini reads the same one.

	// Ronin @feature 03/10/2026 DX9: one column of a panel table (`water`, `lights`) - a group's title and its `name=value` cells.
	enum { PANEL_COLUMN_ROWS = 17 };	// water's SWELL holds 9, lights' COLOUR 16 (Ronin 23/10/2026: 15 -> 17 for the night's vibrance and contrast)
	struct PanelColumn
	{
		const char *title;
		AsciiString cell[PANEL_COLUMN_ROWS];
		Int         count;
	};

	void waterCell(PanelColumn &col, const char *name, Real value)
	{
		if (col.count < PANEL_COLUMN_ROWS)
			col.cell[col.count++].format("%s=%.3g", name, value);
	}

	// Ronin @feature 03/10/2026 DX9: a knob's cell by its command name, so the table only shows names `water` takes.
	void waterKnobCell(PanelColumn &col, const WaterSeaTuning &t, const char *name)
	{
		const WaterKnobInfo *k = WaterSea_FindKnob(TheWaterProfileKnobs, TheWaterProfileKnobCount, name);
		if (k != nullptr)
			waterCell(col, name, WaterSea_GetKnob(&t, *k));
	}

	// Ronin @feature 25/10/2026 DX9: the same for an all-water knob (the WAKES column).
	void waterGlobalCell(PanelColumn &col, const WaterSeaGlobal &g, const char *name)
	{
		const WaterKnobInfo *k = WaterSea_FindKnob(TheWaterGlobalKnobs, TheWaterGlobalKnobCount, name);
		if (k != nullptr)
			waterCell(col, name, WaterSea_GetKnob(&g, *k));
	}

	// Ronin @feature 26/10/2026 DX9: ` | name <lo..hi>` for every knob of a water table - the usage line comes from the tables,
	// so it lists each knob and its range as the code holds them.
	void waterUsageKnobs(AsciiString &line, const WaterKnobInfo *table, Int count)
	{
		for (Int k = 0; k < count; k++)
		{
			AsciiString one;
			if (table[k].kind == WATER_KNOB_BOOL)
				one.format(" | %s <0|1>", table[k].name);
			else
				one.format(" | %s <%g..%g>", table[k].name, (double)table[k].lo, (double)table[k].hi);
			line.concat(one);
		}
	}

	// Ronin @feature 26/10/2026 DX9: `water` with a word it does not take - the usage, then what each command and knob does,
	// in the printout's groups (as `lights` does).
	void waterHelp()
	{
		AsciiString usage("usage: water profile sea|lake|river | save | load | defaults | flat|mesh|waves|sea <0|1> | view (alone: the list) | the profile's:");
		waterUsageKnobs(usage, TheWaterProfileKnobs, TheWaterProfileKnobCount);
		usage.concat(" | all water's:");
		waterUsageKnobs(usage, TheWaterGlobalKnobs, TheWaterGlobalKnobCount);
		printAscii(usage, TRUE);
		printAscii(AsciiString("  profile: which water the profile's knobs edit - sea (standing water that reaches the map's edge), lake (other standing water), river. save / load / defaults: Data/INI/DX9Water.ini - write every knob, read it back, the built-in look (the file is untouched)"), FALSE);
		printAscii(AsciiString("  flat / mesh / waves / sea: draw the flat water / the grid mesh (dams) / the 2003 shore-wave sprites at all / the old infinite sea plane instead of the map's water - debug switches; view: one ingredient alone, 0 = the water"), FALSE);
		printAscii(AsciiString("  REFLECTION - refl: how much the water reflects; fresnel 1: more toward the horizon - f0 looking straight down, fpow how fast it rises, fresnelripple how much the ripples move it; distort: how far the ripples bend the reflection"), FALSE);
		printAscii(AsciiString("  RIPPLES - normals 0|1: the ripple maps (0 = the old caust ripple); ripple: their strength, 0 = mirror-flat; speed / tile: how fast they move / how big they are (a river's speed is its current); cross: degrees between the two layers, low = the wind's streaks line up; refract: how far they bend the water's own texture; bend: how far they bend the seabed seen through"), FALSE);
		printAscii(AsciiString("  LIGHT - spec / gloss: the sun's sharp glitter, its strength / tightness; sheen / sheenpow: the broad sun sheen, the same; shade: light and shade the ripples put on the water body; shadow: how much a shadow darkens the water, 0 = no shadows"), FALSE);
		printAscii(AsciiString("  DEPTH - deep: deep water's brightness, 1 = no darkening; deepdist: the depth it takes full effect by; opacity: how opaque past the shore (ownopacity 1: the map's own see-through instead); shore: the soft edge's width from the waterline; shoredepth: shore effects stop by this depth; texture: how much of the map's water texture shows"), FALSE);
		printAscii(AsciiString("  FOAM - foam: the shore foam's amount (0 = none, ~4 = a solid band); foamwidth: its reach from the waterline; foamtile: its fleck size; seafoam / seafoamdepth / seafoamtile: the open water's foam - amount, the depth it fades out by, fleck size"), FALSE);
		printAscii(AsciiString("  whitecaps: foam on the highest crests, 0 = none; whitecapat: how high a crest must be (lower = more foam); breakers 1: the shore foam's wash and incoming front land with each crest of the swell"), FALSE);
		printAscii(AsciiString("  SWELL - swell 0|1: waves that move the surface (not on rivers); swellheight / swellsize / swellspeed: their height, length and speed; swelldir: the wind, degrees; swelldepth: full height from this depth; swellcell: the grid's cell; swellsharp: 0 round crests .. 1 pointed; swelltint: crests lighter, troughs darker"), FALSE);
		printAscii(AsciiString("  WAKES (all water) - wakes: the trails moving units leave, their strength, 0 = none; wakefoam: their foam; wakeheight: how high they raise the water; wakesharp: 0 round crests .. 1 pointed; waketint: crests lighter, the hollow darker; wakeshade: how much their slopes catch the light"), FALSE);
		printAscii(AsciiString("  wakerings: the rings round a unit at rest; wakesurge: the wave that runs on ahead of a unit that stops; oldwakes 1: the 2003 wake sprites too; ride: how much floating units ride the swell, 0 = not"), FALSE);
		printAscii(AsciiString("  ALL WATER - depth 0: the 2003 flat colour and shore (no depth, no foam); refraction 0|1: the seabed bent by the ripples; oldwaves 1: the 2003 shore-wave sprites beside the shore foam"), FALSE);
		printAscii(AsciiString("  MIRROR - mirror 0|1: the reflections (Options.ini DX9WaterReflections); mirrorres: 1 the screen's size, 2 half; mirrorclip 1: clipped at the water; mirrorfar: its far plane, x the view's; mirrorsoft: a softer read against crawling edges, 0 = one tap"), FALSE);
	}

	// Ronin @feature 19/10/2026 DX9: how wide each column must be - at least `width` already holds, so tables printed one under
	// another can share their widths and line up.
	void columnWidths(const PanelColumn *cols, Int colCount, Int *width)
	{
		for (Int c = 0; c < colCount && c < 8; c++)
		{
			Int w = (Int)strlen(cols[c].title);
			for (Int r = 0; r < cols[c].count; r++)
				if ((Int)cols[c].cell[r].getLength() > w)
					w = (Int)cols[c].cell[r].getLength();
			if (w + 2 > width[c])
				width[c] = w + 2;
		}
	}

	// Ronin @feature 03/10/2026 DX9: the columns side by side, each as wide as its widest cell; headers in their own colour.
	// Ronin @feature 19/10/2026 DX9: `shared` = widths from columnWidths over several tables, so they line up.
	void printColumns(const PanelColumn *cols, Int colCount, const Int *shared = nullptr)
	{
		Int width[8] = {};
		Int rows = 0;
		for (Int c = 0; c < colCount && c < 8; c++)
		{
			width[c] = (shared != nullptr) ? shared[c] : 0;
			if (cols[c].count > rows)
				rows = cols[c].count;
		}
		columnWidths(cols, colCount, width);
		for (Int r = -1; r < rows; r++)	// -1 = the header row
		{
			AsciiString line;
			for (Int c = 0; c < colCount && c < 8; c++)
			{
				const char *text = (r < 0) ? cols[c].title : (r < cols[c].count) ? cols[c].cell[r].str() : "";
				AsciiString one;
				if (c + 1 < colCount)
					one.format("%-*s", width[c], text);
				else
					one = text;	// no trailing pad: the panel sizes to its widest line
				line.concat(one);
			}
			printAscii(line, FALSE, (r < 0) ? GameMakeColor(110, 190, 255, 255) : 0);
		}
	}

	void cmdWater(Int argc, const AsciiString *argv)
	{
		// Ronin @feature 02/10/2026 DX9: phase 4 - the knobs and the profile switches edit the profile `water profile` selects;
		// the other switches are global.
		static const char *const kindNames[WATER_KIND_COUNT] = { "sea", "lake", "river" };
		WaterSeaGlobal &g = TheWaterSeaGlobal;
		if (argc == 3 && stricmp(argv[1].str(), "profile") == 0)
		{
			Int pick = -1;
			for (Int k = 0; k < WATER_KIND_COUNT; k++)
				if (stricmp(argv[2].str(), kindNames[k]) == 0)
					pick = k;
			if (pick < 0)
			{
				printAscii(AsciiString("usage: water profile sea|lake|river"), TRUE);
				return;
			}
			g.edit = pick;	// then print the newly selected profile below
		}
		WaterSeaTuning &t = WaterSea_Profile(g.edit);
		// Ronin @feature 03/10/2026 DX9: `water view` alone lists the debug views (it was a long tail on every `water` print).
		if (argc == 2 && stricmp(argv[1].str(), "view") == 0)
		{
			AsciiString views;
			views.format("view=%d: 0 water 1 fresnel 2 N.V 3 normal 4 sun 5 refl 6 body 7 depth 8 foam 9 shore 10 swell 11 shadow 12 opacity 13 shore gates 14 wakes",
				g.view);
			printAscii(views, FALSE);
			return;
		}
		Bool ok = (argc == 1);
		// Ronin @feature 04/10/2026 DX9: the water ini (R3DWaterSeaIni.h): `save` writes every knob as it stands, `load` reads
		// the file again, `defaults` returns to the built-in look. Each then prints the state below.
		if (argc == 2)
		{
			const AsciiString path = WaterSea_IniPath();
			AsciiString note;
			if (stricmp(argv[1].str(), "save") == 0)
			{
				ok = WaterSea_SaveIni(path.str());
				note.format(ok ? "water: saved to %s" : "water: could not write %s", path.str());
				printAscii(note, !ok);
				if (!ok)
					return;
			}
			else if (stricmp(argv[1].str(), "load") == 0)
			{
				Int skipped = 0;
				const Int count = WaterSea_LoadIni(path.str(), &skipped);
				if (count < 0)
				{
					note.format("water: no file %s", path.str());
					printAscii(note, TRUE);
					return;
				}
				note.format("water: %d values read from %s (%d lines skipped)", count, path.str(), skipped);
				printAscii(note, skipped > 0);
				ok = TRUE;
			}
			else if (stricmp(argv[1].str(), "defaults") == 0)
			{
				WaterSea_ResetDefaults();
				printAscii(AsciiString("water: the built-in look (the file is untouched; `water save` to keep it)"), FALSE);
				ok = TRUE;
			}
		}
		if (argc == 3)
		{
			const char *name = argv[1].str();
			const Bool on = (atoi(argv[2].str()) != 0);
			if      (stricmp(name, "profile") == 0) { ok = TRUE; }	// selected above
			else if (stricmp(name, "flat") == 0) { TheWaterDebug.skipFlat = !on; ok = TRUE; }
			else if (stricmp(name, "mesh") == 0) { TheWaterDebug.skipMesh = !on; ok = TRUE; }
			// Ronin @feature 28/09/2026 DX9: shore waves - Water_Work.md F5, nearly all of [DRAW] water on ocean maps.
			else if (stricmp(name, "waves") == 0) { TheWaterDebug.skipWaves = !on; ok = TRUE; }
			// Ronin @feature 28/09/2026 DX9: WaterType 2 - the original infinite sea plane instead of the map's polygons.
			else if (stricmp(name, "sea") == 0) { TheWaterDebug.seaPlane = on; ok = TRUE; }
			// Ronin @feature 04/10/2026 DX9: every look switch and knob by its name: the selected profile's, then all water's
			else
			{
				const WaterKnobInfo *k = WaterSea_FindKnob(TheWaterProfileKnobs, TheWaterProfileKnobCount, name);
				void *base = &t;
				if (k == nullptr)
				{
					k = WaterSea_FindKnob(TheWaterGlobalKnobs, TheWaterGlobalKnobCount, name);
					base = &g;
				}
				if (k != nullptr)
				{
					WaterSea_SetKnob(base, *k, (Real)atof(argv[2].str()));
					ok = TRUE;
				}
			}
		}
		if (!ok)
		{
			waterHelp();	// Ronin @feature 26/10/2026 DX9: every command and knob with what it does
			return;
		}
		// Ronin @feature 03/10/2026 DX9: the global switches, then the selected profile as a table, one column per group - one
		// run-on line wrapped over four was hard to read.
		AsciiString state;
		state.format("all water   flat=%d  mesh=%d  waves=%d  sea=%d  depth=%d  oldwaves=%d  refraction=%d  view=%d (`water view`)",
			TheWaterDebug.skipFlat ? 0 : 1, TheWaterDebug.skipMesh ? 0 : 1, TheWaterDebug.skipWaves ? 0 : 1,
			TheWaterDebug.seaPlane ? 1 : 0, g.depth ? 1 : 0, g.oldWaves ? 1 : 0, g.refraction ? 1 : 0, g.view);
		printAscii(state, FALSE);
		state.format("            mirror=%d  mirrorres=%d  mirrorclip=%d  mirrorfar=%.3g  mirrorsoft=%.3g  ride=%.3g", g.mirror ? 1 : 0,
			g.mirrorRes, g.mirrorClip ? 1 : 0, g.mirrorFar, g.mirrorSoft, g.ride);
		printAscii(state, FALSE);
		state.format("profile %s  (`water profile sea|lake|river` picks the one these edit)", kindNames[g.edit]);
		printAscii(state, FALSE, GameMakeColor(255, 220, 120, 255));

		PanelColumn cols[7] = {};
		cols[0].title = "REFLECTION";
		waterKnobCell(cols[0], t, "refl");
		waterCell(cols[0], "fresnel", t.fresnel ? 1.0f : 0.0f);
		waterKnobCell(cols[0], t, "f0");
		waterKnobCell(cols[0], t, "fpow");
		waterKnobCell(cols[0], t, "fresnelripple");
		waterKnobCell(cols[0], t, "distort");
		cols[1].title = "RIPPLES";
		waterCell(cols[1], "normals", t.normals ? 1.0f : 0.0f);
		waterKnobCell(cols[1], t, "ripple");
		waterKnobCell(cols[1], t, "speed");
		waterKnobCell(cols[1], t, "tile");
		waterKnobCell(cols[1], t, "cross");	// Ronin @feature 01/11/2026 DX9: the angle between the two ripple layers
		waterKnobCell(cols[1], t, "refract");
		waterKnobCell(cols[1], t, "bend");
		cols[2].title = "LIGHT";
		waterKnobCell(cols[2], t, "spec");
		waterKnobCell(cols[2], t, "gloss");
		waterKnobCell(cols[2], t, "sheen");
		waterKnobCell(cols[2], t, "sheenpow");
		waterKnobCell(cols[2], t, "shade");
		waterKnobCell(cols[2], t, "shadow");
		cols[3].title = "DEPTH";
		waterKnobCell(cols[3], t, "deepdist");
		waterKnobCell(cols[3], t, "deep");
		waterCell(cols[3], "ownopacity", t.ownOpacity ? 1.0f : 0.0f);
		waterKnobCell(cols[3], t, "opacity");
		waterKnobCell(cols[3], t, "shore");
		waterKnobCell(cols[3], t, "shoredepth");
		waterKnobCell(cols[3], t, "texture");
		cols[4].title = "FOAM";
		waterKnobCell(cols[4], t, "foam");
		waterKnobCell(cols[4], t, "foamwidth");
		waterKnobCell(cols[4], t, "foamtile");
		waterKnobCell(cols[4], t, "seafoam");
		waterKnobCell(cols[4], t, "seafoamdepth");
		waterKnobCell(cols[4], t, "seafoamtile");
		waterKnobCell(cols[4], t, "whitecaps");
		waterKnobCell(cols[4], t, "whitecapat");
		waterCell(cols[4], "breakers", t.breakers ? 1.0f : 0.0f);
		// Ronin @feature 03/10/2026 DX9: rivers never swell (no vertices mid-river; WaterSea_ps.hlsl): say so, not dead knobs.
		cols[5].title = (g.edit == WATER_KIND_RIVER) ? "SWELL (unused on rivers)" : "SWELL";
		waterCell(cols[5], "swell", t.swell ? 1.0f : 0.0f);
		waterKnobCell(cols[5], t, "swellheight");
		waterKnobCell(cols[5], t, "swellsize");
		waterKnobCell(cols[5], t, "swellspeed");
		waterKnobCell(cols[5], t, "swelldir");
		waterKnobCell(cols[5], t, "swelldepth");
		waterKnobCell(cols[5], t, "swellcell");
		waterKnobCell(cols[5], t, "swellsharp");
		waterKnobCell(cols[5], t, "swelltint");
		// Ronin @feature 25/10/2026 DX9: wakes beside the swell - all water's knobs, not the profile's (they were a line above)
		cols[6].title = "WAKES (all water)";
		waterGlobalCell(cols[6], g, "wakes");
		waterGlobalCell(cols[6], g, "wakefoam");
		waterGlobalCell(cols[6], g, "wakeheight");
		waterGlobalCell(cols[6], g, "wakesharp");
		waterGlobalCell(cols[6], g, "waketint");
		waterGlobalCell(cols[6], g, "wakeshade");
		waterGlobalCell(cols[6], g, "wakerings");
		waterGlobalCell(cols[6], g, "wakesurge");
		waterCell(cols[6], "oldwakes", g.oldWakes ? 1.0f : 0.0f);
		printColumns(cols, 7);
	}

	// Ronin @feature 28/09/2026 DX9: [PERF] run mean. `perf reset` starts it over, so a runtime switch gets its own mean
	// instead of one blended across the whole session.
	void cmdPerf(Int argc, const AsciiString *argv)
	{
		if (argc == 2 && stricmp(argv[1].str(), "reset") == 0)
		{
			s_perfFirst   = 0;		// sampleReadouts restarts the run on its next tracked frame
			s_perfLast    = 0;
			s_perfFrames  = 0;
			s_perfSMSum   = 0.0;
			s_perfWorstMs = 0.0f;
		}
		else if (argc != 1)
		{
			printAscii(AsciiString("usage: perf [reset]"), TRUE);
			return;
		}
		const double secs = (s_perfFrames > 0 && s_perfFreq > 0) ? ((double)(s_perfNow - s_perfFirst) / (double)s_perfFreq) : 0.0;
		AsciiString state;
		state.format("perf: frames=%u  avgMs=%.3f  worstMs=%.1f%s", s_perfFrames,
			(s_perfFrames > 0) ? secs * 1000.0 / (double)s_perfFrames : 0.0, s_perfWorstMs, (argc == 2) ? "  (reset)" : "");
		printAscii(state, FALSE);
	}

	// Ronin @bugfix 27/09/2026 DX9: a lone word after a command must be a number. atoi read `taa margin` as 0 and switched TAA
	// off; now it prints the usage instead.
	Bool isWholeNumber(const char *s)
	{
		if (s == nullptr || *s == 0)
			return FALSE;
		if (*s == '-')
			++s;
		if (*s == 0)
			return FALSE;
		for (; *s != 0; ++s)
			if (*s < '0' || *s > '9')
				return FALSE;
		return TRUE;
	}

	// Ronin @feature 28/09/2026 DX9: `rows` lists every row's switch; `rows <name>|all <0|1>` sets one or all. A row that is
	// on can still be empty: shadow needs shadow maps, taa needs `taa 1`.
	void cmdRows(Int argc, const AsciiString *argv)
	{
		Bool ok = (argc == 1);
		if (argc == 3 && isWholeNumber(argv[2].str()))
		{
			const Bool all = (stricmp(argv[1].str(), "all") == 0);
			for (Int r = 0; r < R3DDebugPanel::ROW_COUNT; ++r)
			{
				if (all || stricmp(argv[1].str(), ROW_NAMES[r]) == 0)
				{
					s_rowOn[r] = (atoi(argv[2].str()) != 0);
					ok = TRUE;
				}
			}
		}
		if (!ok)
		{
			printAscii(AsciiString("usage: rows [<name>|all 0|1] - names: sr inst draw shadow perf depth terrain ui rstate taa"), TRUE);
			return;
		}
		AsciiString state("rows:");
		for (Int r = 0; r < R3DDebugPanel::ROW_COUNT; ++r)
		{
			if (r == R3DDebugPanel::ROW_DRAW2 || r == R3DDebugPanel::ROW_LIGHT2)
				continue;		// same switch as draw / light
			AsciiString one;
			one.format("  %s=%d", ROW_NAMES[r], s_rowOn[r] ? 1 : 0);
			state.concat(one);
		}
		printAscii(state, FALSE);
	}

	// Ronin @feature 20/09/2026 DX9: TAA step 1 is the JITTER ONLY — no history, no resolve. With it on the image should
	// shimmer very slightly: that shimmer is the subpixel sampling a later step accumulates, and seeing it is how the
	// jitter is verified. It must NOT move placement or picking — those read the unjittered matrix. Hold Ctrl and check
	// the placement grid does not crawl. docs/AntiAliasing_Work.md.
	void cmdTaa(Int argc, const AsciiString *argv)
	{
		Bool ok = (argc == 1);
		if (argc == 2 && isWholeNumber(argv[1].str()))
		{
			R3DTaa::setEnabled(atoi(argv[1].str()) != 0);
			ok = TRUE;
		}
		else if (argc == 3)
		{
			if      (stricmp(argv[1].str(), "weight")   == 0) { R3DTaa::setWeight((float)atof(argv[2].str())); ok = TRUE; }
			else if (stricmp(argv[1].str(), "debug")    == 0) { R3DTaa::setDebug(atoi(argv[2].str())); ok = TRUE; }
			else if (stricmp(argv[1].str(), "sharpen")  == 0) { R3DTaa::setSharpen((float)atof(argv[2].str())); ok = TRUE; }
			else if (stricmp(argv[1].str(), "mipbias")  == 0) { R3DTaa::setMipBias((float)atof(argv[2].str())); ok = TRUE; }
			else if (stricmp(argv[1].str(), "clamp")    == 0) { R3DTaa::setClamp((float)atof(argv[2].str())); ok = TRUE; }
			else if (stricmp(argv[1].str(), "shadowmask")== 0) { R3DTaa::setShadowMask(atoi(argv[2].str()) != 0); ok = TRUE; }
			else if (stricmp(argv[1].str(), "maskcap")  == 0) { R3DTaa::setMaskCap(atoi(argv[2].str()) != 0); ok = TRUE; }
			else if (stricmp(argv[1].str(), "vel")      == 0) { R3DTaa::setVelocity(atoi(argv[2].str()) != 0); ok = TRUE; }
			else if (stricmp(argv[1].str(), "disocc")   == 0) { R3DTaa::setDisocc(atoi(argv[2].str())); ok = TRUE; }
			else if (stricmp(argv[1].str(), "disoccv")  == 0) { R3DTaa::setDisoccV((float)atof(argv[2].str())); ok = TRUE; }
			else if (stricmp(argv[1].str(), "reactive") == 0) { R3DTaa::setReactive((float)atof(argv[2].str())); ok = TRUE; }
			else if (stricmp(argv[1].str(), "autoreact")== 0) { R3DTaa::setAutoReact((float)atof(argv[2].str())); ok = TRUE; }
			else if (stricmp(argv[1].str(), "autofull") == 0) { R3DTaa::setAutoFull((float)atof(argv[2].str())); ok = TRUE; }
			// Ronin @bugfix 29/09/2026 DX9: the water's reactive mask - its polygons in the velocity pass.
			else if (stricmp(argv[1].str(), "water")    == 0) { R3DTaa::setWaterMask(atoi(argv[2].str()) != 0); ok = TRUE; }
			// Ronin @feature 18/10/2026 DX9: the reflections' change mask - a pane marks where its screen-space reflection changed.
			else if (stricmp(argv[1].str(), "reflmark") == 0) { R3DTaa::setReflMark(atoi(argv[2].str())); ok = TRUE; }
		}
		if (!ok)
		{
			printAscii(AsciiString("usage: taa [0|1] | weight <0..0.99> | debug <0..15> | sharpen <0..1> | mipbias <-2..0> | clamp <0..1> | shadowmask <0|1> | maskcap <0|1> | vel <0|1> | disocc <0..2> | disoccv <px> | reactive <0..1> | autoreact <0..1> | autofull <0..1> | water <0|1> | reflmark <0..2>"), TRUE);
			printAscii(AsciiString("  debug 1=reproj 2=depth 3=samples 4=history 13=velocity/reactive mask (orange = auto-reactive, by how much; red = an effect just left; white = a local light changed, pink = a reflection changed, purple = and a unit moved there, teal = the fog of war changed) 14=moving shadows (blue = in one now, red = clamped, yellow = also capped) 15=mesh motion px/frame (navy<.05 blue<.1 cyan<.2 green<.35 yellow<.5 orange<1 red)"), FALSE);
			return;
		}
		// Ronin @diagnostic 26/09/2026 DX9: labels are the knob names, so what is read here is what gets typed.
		// Ronin @bugfix 27/09/2026 DX9: three short lines, grouped; one line had grown wider than the screen.
		AsciiString state;
		state.format("taa: enabled=%d  running=%d  weight=%.2f  sharpen=%.2f  mipbias=%.2f  debug=%d",
			R3DTaa::isEnabled() ? 1 : 0, R3DTaa::isActive() ? 1 : 0, R3DTaa::getWeight(), R3DTaa::getSharpen(), R3DTaa::getMipBias(), R3DTaa::getDebug());
		printAscii(state, FALSE);
		state.format("  clamp=%.2f  shadowmask=%d  maskcap=%d  reactive=%.2f  autoreact=%.2f  autofull=%.2f",
			R3DTaa::getClamp(), R3DTaa::getShadowMask() ? 1 : 0, R3DTaa::getMaskCap() ? 1 : 0,
			R3DTaa::getReactive(), R3DTaa::getAutoReact(), R3DTaa::getAutoFull());
		printAscii(state, FALSE);
		// Ronin @bugfix 03/10/2026 DX9: the water mask's source - "own" = the water marked its pixels as it drew; else the
		// flat polygons' triangle count (the fallback).
		AsciiString maskFrom;
		if (R3DTaa::getWaterMaskDrawn())
			maskFrom = "own";
		else
			maskFrom.format("%d tris", R3DTaa::getWaterMaskTris());
		state.format("  vel=%d  disocc=%d  disoccv=%.2f  water=%d (%s)  reflmark=%d",
			R3DTaa::getVelocity() ? 1 : 0, R3DTaa::getDisocc(), R3DTaa::getDisoccV(),
			R3DTaa::getWaterMask() ? 1 : 0, maskFrom.str(), R3DTaa::getReflMark());
		printAscii(state, FALSE);
	}

	// Ronin @feature 26/09/2026 DX9: SSAO quality at runtime, 0 Off .. 3 Ultra - the Options.ini SSAOQuality value, not
	// saved back. R3DSsao reads it every frame and resizes its targets itself, so no restart.
	void cmdSsao(Int argc, const AsciiString *argv)
	{
		if (argc == 2 && TheWritableGlobalData != nullptr && isWholeNumber(argv[1].str()))
		{
			Int q = atoi(argv[1].str());
			if (q < 0) q = 0;
			if (q > 3) q = 3;
			TheWritableGlobalData->m_ssaoQuality = q;
		}
		// Ronin @feature 26/09/2026 DX9: `ssao trees auto|0|1` - AO on trees while TAA runs (auto), never, or always.
		else if (argc == 3 && stricmp(argv[1].str(), "trees") == 0)
			R3DSsao::setTrees(stricmp(argv[2].str(), "auto") == 0 ? R3DSsao::TREES_AUTO : ((atoi(argv[2].str()) != 0) ? R3DSsao::TREES_ON : R3DSsao::TREES_OFF));
		// Ronin @diagnostic 07/10/2026 DX9: `ssao view 0..3` - corner view: 1 scene depth, 2 raw AO, 3 the blurred AO.
		else if (argc == 3 && stricmp(argv[1].str(), "view") == 0 && isWholeNumber(argv[2].str()))
			R3DSsao::setDebugView(atoi(argv[2].str()));
		// Ronin @feature 24/10/2026 DX9: `ssao sun|bleed|colour <v>` - the lights table's aosun, aobleed, aocolour; `ssao save` keeps sun.
		else if (argc == 3 && (stricmp(argv[1].str(), "sun") == 0 || stricmp(argv[1].str(), "bleed") == 0 || stricmp(argv[1].str(), "colour") == 0))
		{
			const char *name = (stricmp(argv[1].str(), "sun") == 0) ? "aosun" : ((stricmp(argv[1].str(), "bleed") == 0) ? "aobleed" : "aocolour");
			const R3DLocalLights::Knob *knob = R3DLocalLights::findKnob(name);
			if (knob != nullptr)
				knob->set((float)atof(argv[2].str()));
		}
		else if (argc == 2 && stricmp(argv[1].str(), "save") == 0)
		{
			char path[512];
			R3DLocalLights::iniPath(path, (Int)sizeof(path));
			const Bool saved = R3DLocalLights::saveIni();
			AsciiString note;
			note.format(saved ? "ssao: sun saved to %s (with the lights' values)" : "ssao: could not write %s", path);
			printAscii(note, !saved);
			if (!saved)
				return;
		}
		else if (argc != 1)
		{
			printAscii(AsciiString("usage: ssao [0..3]  (0 off, 1 normal, 2 high, 3 ultra) | ssao trees <auto|0|1> | ssao view [0..4]  (4 = the colour bleed's bounce) | ssao sun <0..3> | ssao bleed <0..8> | ssao colour <0..1> | ssao save  (sun only)"), TRUE);
			printAscii(AsciiString("  sun: AO where the sun reaches - 1 = it shades the map's ambient only (all of it in a sun shadow, the ambient's share in the sun), more = less still in the sun, 0 = everywhere alike"), FALSE);
			printAscii(AsciiString("  bleed: the colour of what blocks the light bounced onto its surroundings, 0 = none; colour: the AO in the surface's own colour, lighter on bright ones, 0 = grey. Both under `lights 0|1`, not saved; save writes sun to DX9Lights.ini"), FALSE);
			return;
		}
		static const char *names[] = { "off", "normal", "high", "ultra" };
		const Int q = R3DSsao::getQuality();
		AsciiString state;
		// Ronin @bugfix 27/09/2026 DX9: no `active` here - it is decided at the next frame start, so it read stale right after
		// a change. The [DEPTH] row shows whether it actually ran.
		const Int tm = R3DSsao::getTrees();
		state.format("ssao: quality=%d (%s)  trees=%s%s  view=%d  sun=%.2f  bleed=%.2f  colour=%.2f", q, names[(q < 0) ? 0 : ((q > 3) ? 3 : q)],
			(tm == R3DSsao::TREES_AUTO) ? "auto" : ((tm == R3DSsao::TREES_ON) ? "1" : "0"),
			(tm == R3DSsao::TREES_AUTO) ? (R3DSsao::treesNow() ? " (on: TAA running)" : " (off: TAA not running)") : "",
			R3DSsao::getDebugView(), R3DLocalLights::getAoSun(), R3DSsao::getBleed(), R3DSsao::getColourAO());
		printAscii(state, FALSE);
	}

	// Ronin @feature 26/09/2026 DX9: shadow-map quality at runtime, 0 Off .. 3 Ultra - the Options.ini ShadowQuality value,
	// not saved back. applyQuality tears the receivers down and rebuilds the maps; at 0 the stencil volumes and decals,
	// which every object keeps, draw again (W3DShadow.cpp shadowMapsDriving).
	void cmdShadows(Int argc, const AsciiString *argv)
	{
		if (argc == 2 && TheWritableGlobalData != nullptr && isWholeNumber(argv[1].str()))
		{
			Int q = atoi(argv[1].str());
			if (q < 0) q = 0;
			if (q > 3) q = 3;
			TheWritableGlobalData->m_shadowMapQuality = q;
			R3DShadowMap::applyQuality();
		}
		else if (argc != 1)
		{
			printAscii(AsciiString("usage: shadows [0..3]  (0 off = stencil shadows, 1 normal, 2 high, 3 ultra)"), TRUE);
			return;
		}
		static const char *names[] = { "off", "normal", "high", "ultra" };
		const Int q = R3DShadowMap::getQuality();
		AsciiString state;
		state.format("shadows: quality=%d (%s)  maps=%d", q, names[(q < 0) ? 0 : ((q > 3) ? 3 : q)],
			TheUseShadowMaps ? 1 : 0);
		printAscii(state, FALSE);
	}

	// Ronin @feature 09/10/2026 DX9: `lights` - local lights drawn in screen space (R3DLocalLights). 0 hands them back to
	// the old per-vertex terrain and per-object paths, for an A/B at one spot. The knobs are kept by `lights save` (DX9Lights.ini).
	// Ronin @feature 19/10/2026 DX9: a `lights` knob's cell by its name, printed as `lights` takes it; placed[] marks the knobs shown.
	enum { LIGHT_KNOBS_MAX = 128 };
	void lightCell(PanelColumn &col, const char *name, Bool *placed)
	{
		for (Int k = 0; k < R3DLocalLights::knobCount() && k < LIGHT_KNOBS_MAX; ++k)
		{
			const R3DLocalLights::Knob *knob = R3DLocalLights::knobAt(k);
			if (stricmp(knob->name, name) != 0)
				continue;
			placed[k] = TRUE;
			if (col.count >= PANEL_COLUMN_ROWS)
				return;
			AsciiString &cell = col.cell[col.count++];
			if (knob->whole)
				cell.format("%s=%d", knob->name, (Int)knob->get());
			else
				cell.format("%s=%.2f", knob->name, knob->get());
			if (stricmp(knob->name, "boost") == 0 && knob->get() <= 0.0f)
				cell.concat(" (auto)");
			return;
		}
	}

	void cmdLights(Int argc, const AsciiString *argv)
	{
		Bool ok = (argc == 1);
		if (argc == 2 && isWholeNumber(argv[1].str()))
		{
			R3DLocalLights::setEnabled(atoi(argv[1].str()) != 0);
			ok = TRUE;
		}
		// Ronin @feature 12/10/2026 DX9: the lights' ini, as the water's: `save` writes every knob as it stands, `load` reads the file
		// again, `defaults` returns to the built-in values. Each then prints the state below.
		else if (argc == 2)
		{
			char path[512];
			R3DLocalLights::iniPath(path, (Int)sizeof(path));
			AsciiString note;
			if (stricmp(argv[1].str(), "save") == 0)
			{
				ok = R3DLocalLights::saveIni();
				note.format(ok ? "lights: saved to %s" : "lights: could not write %s", path);
				printAscii(note, !ok);
				if (!ok)
					return;
			}
			else if (stricmp(argv[1].str(), "load") == 0)
			{
				Int skipped = 0;
				const Int count = R3DLocalLights::loadIni(&skipped);
				if (count < 0)
				{
					note.format("lights: no file %s", path);
					printAscii(note, TRUE);
					return;
				}
				note.format("lights: %d values read from %s (%d lines skipped)", count, path, skipped);
				printAscii(note, skipped > 0);
				ok = TRUE;
			}
			else if (stricmp(argv[1].str(), "defaults") == 0)
			{
				R3DLocalLights::resetDefaults();
				printAscii(AsciiString("lights: the built-in values (the file is untouched; `lights save` to keep them)"), FALSE);
				ok = TRUE;
			}
		}
		// Ronin @feature 12/10/2026 DX9: every knob by its name (R3DLocalLights' table); what each does: the usage below.
		else if (argc == 3)
		{
			const R3DLocalLights::Knob *knob = R3DLocalLights::findKnob(argv[1].str());
			if (knob != nullptr)
			{
				knob->set((float)atof(argv[2].str()));
				ok = TRUE;
			}
		}
		if (!ok)
		{
			printAscii(AsciiString("usage: lights [0|1] | locallighting <0|1> | save | load | defaults | intensity <0..8> | range <0.25..4> | small <0|1> | over <1..8> | boost <0..8> | view <0..6|9> | beams <0..4> | shafts <0..2> | rays <0..2> | rayturn <0|1> | lampwidth <0..4> | spill <0..1> | spillwide <1.2..4> | glows <0..4> | glowreach <0.25..4> | glowjoin <0..10> | coronas <0..4> | coronasize <0.25..4> | pools <0|1> | beamreach <1..3> | poolfill <0..4> | hotspot <0..0.9> | sunfill <0..1> | sunerase <0..8> | mapshadow <0..1> | reflect <0..2> | reflectground <0..1> | fresnel <0..4> | ssr <0|1> | bounce <0..2> | sunshafts <0..4> | shaftgaps <0.5..1> | shaftlayer <20..400> | shaftdust <0..4> | vibrance <0..2> | localcontrast <0..2> | nightvibrance <0..2> | nightcontrast <0..2> | fxlights <0..64> | fxstrength <0..4> | fxreach <0.25..4> | fxsmooth <0..0.95> | night... (as `night`) | lampmaps <0|1> | lampshadows <0..1> | lampbias <0..4> | lampself <0|1> | lampselfnear <0..0.5> | pointmaps <0|1> | pointsides <0|1>"), TRUE);
			printAscii(AsciiString("  beams: headlights and lamp cones as spot lights, their strength; 0 = the art's own beams, no lights; shafts: the beam in the air, drawn by the shader - its strength beside the light's, 0 = none; rays: soft rays from the lamp in that beam, 0 = the smooth cone; rayturn: 1 = they turn with the world as the camera goes round a beam, 0 = they stay with the view; lampwidth: how wide the beam leaves its lamp, in the art's own width there, 0 = from a point"), FALSE);
			printAscii(AsciiString("  spill: the dim light around a beam's cone, its strength beside the cone's, 0 = a hard cone; spillwide: how wide it reaches, in cone widths"), FALSE);
			printAscii(AsciiString("  glows: the lamp glows (street lamps, warning and runway lights) as point lights, their strength, 0 = none; glowreach: their reach; glowjoin: lamps this near each other (art units) light as one - a string of bulbs - each keeping its corona, 0 = each its own"), FALSE);
			printAscii(AsciiString("  coronas: the lamp's own glow drawn by the shader, its strength, 0 = the art's glow textures; coronasize: its radius, in the art glow's own"), FALSE);
			printAscii(AsciiString("  beamreach: how far a beam's light runs on surfaces, in the art cone's length (it fades all the way); poolfill: the light that stands in for a painted pool with no lamp near, 0 = none; hotspot: the share of a beam's cone at full strength, 0 = it fades from the axis out"), FALSE);
			printAscii(AsciiString("  sunfill: how much of the sun's shadow a lamp fills on a mesh, 1 = all of it, 0 = the lamp only scales what the shadow left"), FALSE);
			printAscii(AsciiString("  sunerase: how fast a lamp gives back what the sun's shadow took - the shadow is gone where the lamp's strength reaches 1 / this; 0 = never. Night only (by sunnight..sunmorning)"), FALSE);
			printAscii(AsciiString("  mapshadow: the SUN's shadows on terrain and meshes - 1 = they leave what this map's light is without its sun (depth and tint), 0 = 0.45 on every map"), FALSE);
			printAscii(AsciiString("  sunmorning / sunafternoon / sunevening / sunnight: how far that time of day is day for the local lights (1, 1, .5, 0)"), FALSE);
			printAscii(AsciiString("  daysun / dayshade: by day a local light's strength where the sun shines / in a sun shadow (.2 / .5); a shadow never gets brighter than the sun beside it"), FALSE);
			printAscii(AsciiString("  pulsemin / pulsefade: the game's own light pulses (muzzle flashes, blasts) reach at least this far and fade over at least this many frames; 0 = as the FX asks"), FALSE);
			printAscii(AsciiString("  searchlight: the police car's own light, as in 2003 - its colour follows the beacons, which keep only their glow (0 = our glow lights instead)"), FALSE);
			printAscii(AsciiString("  fxlights: glowing effects (fire, flames, explosions) as lights in their own colour - how many, the strongest first; fxstrength / fxreach: their strength and reach; fxmerge 1: systems at one spot (a blast's parts) are one light; fxflares 1: lens flares light too (a camera's glare: off); fxfire: how far fires reach past their size, by their brightness; fxsmooth: how much of last frame's light an effect light keeps (at 30 fps) - it eases into a new strength, reach and place instead of jumping, 0 = none"), FALSE);
			printAscii(AsciiString("  bounce: the ground as a light - a wall or an underside takes what the ground under it sends up instead of the flat ambient (warm over sand, darker over asphalt); 0 = off"), FALSE);
			printAscii(AsciiString("  reflect: what glass and polished meshes mirror - 0 = the art's one picture (a lake at dusk, on every map), 1 = this map's sky cube, 2 = the cube with the sky in every pane (seen from above a pane mirrors the ground)"), FALSE);
			printAscii(AsciiString("  reflectground: a reflection looking down shows the ground it meets (0 = the sky, as before; 1 = all ground); fresnel: reflections stronger at grazing angles, weaker facing (0 = off); ssr: what last frame's picture holds where a reflection lands - streets, units, lights (0 = ground and sky only)"), FALSE);
			printAscii(AsciiString("  lampshadows: how much the shadow of a beam (headlight, lamp cone) takes - cast by the solid meshes in its light, from a depth map drawn at the lamp; 0 = none. lampmaps 0 = the maps are not drawn at all"), FALSE);
			printAscii(AsciiString("  lampbias: the shadows' offset from their casters - lower = tighter, stripes on lit surfaces if too low; higher = shadows let go of what casts them"), FALSE);
			printAscii(AsciiString("  lampself: 1 = the lamp's own object casts in its light too; lampselfnear: ... but only past this share of the beam's length (a housing round the bulb would put the lamp out)"), FALSE);
			printAscii(AsciiString("  pointmaps: 1 = a firing flash or a blast (the game's own point lights) is shadowed by what is under it - one map each, straight down; 0 = they light the ground under a unit through its hull"), FALSE);
			printAscii(AsciiString("  pointsides: 1 = a big point light low over the ground (a blast) is shadowed sideways too - four more maps, level: what stands round it shadows the ground behind; 0 = only what is under it"), FALSE);
			printAscii(AsciiString("  view 4: the maps, a tile a lamp, from the top right - bright = near the lamp, dark = far, blue = nothing in its light"), FALSE);
			printAscii(AsciiString("  lights 0|1: ALL the new lighting, HDR included (Options.ini DX9Lighting); locallighting 0|1: the lamps under it - lights, beams, glows, effects, lamp shadows (DX9LightEffects); 0 = the game's own"), FALSE);
			printAscii(AsciiString("  intensity: every light's strength; range: every light's reach; small: keep the pulses dropped as too small for the vertex grid"), FALSE);
			printAscii(AsciiString("  over: ceiling above a surface's own colour - only a light stronger than 1 reaches it; boost: frame-to-surface factor, 0 = from the map's light; view: 1 light alone, 2 facing, 3 rectangles, 4 the lamps' shadow maps, 5 the sky cube, 6 the ground light (grey = none, lighter = added, darker = taken)"), FALSE);
			return;
		}
		AsciiString state;
		state.format("lights: enabled=%d", R3DLocalLights::isEnabled() ? 1 : 0);
		printAscii(state, FALSE);
		// Ronin @feature 19/10/2026 DX9: the knobs in columns by what they light, as `water` prints its own.
		// Ronin @feature 23/10/2026 DX9: the lamps' columns first (`locallighting`), then what only the master (`lights`) holds.
		static const char *const groups[10][PANEL_COLUMN_ROWS] = {
			{ "LIGHTS", "locallighting", "intensity", "range", "small", "over", "boost", "view", NULL },
			{ "BEAMS", "beams", "shafts", "rays", "rayturn", "lampwidth", "spill", "spillwide", "beamreach", "hotspot", NULL },
			{ "GLOWS", "glows", "glowreach", "glowjoin", "coronas", "coronasize", "pools", "poolfill", "searchlight", NULL },
			{ "EFFECTS", "fxlights", "fxstrength", "fxreach", "fxmerge", "fxflares", "fxfire", "fxsmooth", "pulsemin", "pulsefade", NULL },
			{ "LAMP SHADOWS", "lampmaps", "lampshadows", "lampbias", "lampself", "lampselfnear", "pointmaps", "pointsides", NULL },
			{ "LAMPS & SUN", "sunfill", "sunerase", "daysun", "dayshade", "sunmorning", "sunafternoon", "sunevening", "sunnight", NULL },
			{ "SUN", "mapshadow", "sunshafts", "shaftgaps", "shaftlayer", "shaftdust", NULL },
			{ "GLASS & GROUND", "reflect", "reflectground", "fresnel", "ssr", "bounce", NULL },
			{ "COLOUR", "night", "nighttint", "nightlo", "nighthi", "nightfloor", "nightfull", "nightlift", "grade", "gradewarm", "gradesat",
			  "grademids", "vibrance", "localcontrast", "nightvibrance", "nightcontrast", NULL },		// Ronin @feature 21/10/2026 DX9: the final pass's colour - the night look, the dark lift, the grade
			// Ronin @feature 21/10/2026 DX9: HDR's look - the sun's lift by the time of day, its warmth, the curve's white (on / off: Options.ini)
			{ "HDR", "hdrmorning", "hdrafternoon", "hdrevening", "hdrnight", "hdrwarm", "hdrwhite", NULL },
		};
		Bool placed[LIGHT_KNOBS_MAX] = {};
		PanelColumn cols[10] = {};
		for (Int g = 0; g < 10; ++g)
		{
			cols[g].title = groups[g][0];
			for (Int i = 1; i < PANEL_COLUMN_ROWS && groups[g][i] != NULL; ++i)
				lightCell(cols[g], groups[g][i], placed);
		}
		Int shared[8] = {};		// Ronin @feature 19/10/2026 DX9: one width per column across both tables, so they line up
		columnWidths(cols, 5, shared);		// Ronin @bugfix 23/10/2026 DX9: five and five - six wrapped past the panel's width
		columnWidths(cols + 5, 5, shared);
		printColumns(cols, 5, shared);
		printColumns(cols + 5, 5, shared);
		// Ronin @feature 19/10/2026 DX9: a knob no group names (one added to the table later) still prints, on a line of its own.
		AsciiString more, one;
		for (Int k = 0; k < R3DLocalLights::knobCount() && k < LIGHT_KNOBS_MAX; ++k)
		{
			const R3DLocalLights::Knob *knob = R3DLocalLights::knobAt(k);
			// Ronin @feature 24/10/2026 DX9: SSAO's three are `ssao`'s to show.
			if (placed[k] || stricmp(knob->name, "aosun") == 0 || stricmp(knob->name, "aobleed") == 0 || stricmp(knob->name, "aocolour") == 0)
				continue;
			if (knob->whole)
				one.format("  %s=%d", knob->name, (Int)knob->get());
			else
				one.format("  %s=%.2f", knob->name, knob->get());
			more.concat(one);
		}
		if (!more.isEmpty())
		{
			state.format("more:%s", more.str());
			printAscii(state, FALSE);
		}
	}

	// Ronin @diagnostic 15/10/2026 DX9: `glowfx` - global light item 7, measured first: the glowing (additive) particle systems now, and
	// the strongest by colour x alpha x size^2 - what a light per system would have to carry, and how many there would be.
	void cmdGlowFx(Int argc, const AsciiString *argv)
	{
		(void)argc; (void)argv;
		if (TheParticleSystemManager == NULL)
		{
			printAscii(AsciiString("glowfx: no particle manager"), TRUE);
			return;
		}
		struct Glow { AsciiString name; Int parts; Int seen; float power; float col[3]; float pos[3]; float spread; };
		const Int TOP = 8;
		Glow top[TOP];
		Int topN = 0, systems = 0, additive = 0, onScreen = 0, parts = 0, seenParts = 0;
		float powerAll = 0.0f;
		ParticleSystemManager::ParticleSystemList &all = TheParticleSystemManager->getAllParticleSystems();
		for (ParticleSystemManager::ParticleSystemListIt it = all.begin(); it != all.end(); ++it)
		{
			ParticleSystem *sys = *it;
			++systems;
			if (sys == NULL || sys->getShaderType() != ParticleSystemInfo::ADDITIVE)
				continue;
			++additive;
			Glow g;
			g.parts = 0; g.seen = 0; g.power = 0.0f; g.spread = 0.0f;
			float wsum = 0.0f;
			for (Int k = 0; k < 3; ++k) { g.col[k] = 0.0f; g.pos[k] = 0.0f; }
			for (Particle *p = sys->getFirstParticle(); p != NULL; p = p->m_systemNext)
			{
				++g.parts;
				if (p->isCulled())
					continue;
				const RGBColor *c = p->getColor();
				const float w = p->getSize() * p->getSize();		// Ronin @bugfix 16/10/2026 DX9: not by alpha - additive is drawn ONE / ONE
				if (w <= 0.0f)
					continue;
				++g.seen;
				const Coord3D *at = p->getPosition();
				g.col[0] += c->red * w; g.col[1] += c->green * w; g.col[2] += c->blue * w;
				g.pos[0] += at->x * w; g.pos[1] += at->y * w; g.pos[2] += at->z * w;
				g.power += (0.299f * c->red + 0.587f * c->green + 0.114f * c->blue) * w;
				wsum += w;
			}
			parts += g.parts;
			if (g.seen == 0 || wsum <= 0.0f)
				continue;
			++onScreen;
			seenParts += g.seen;
			powerAll += g.power;
			for (Int k = 0; k < 3; ++k) { g.col[k] /= wsum; g.pos[k] /= wsum; }
			for (Particle *p = sys->getFirstParticle(); p != NULL; p = p->m_systemNext)
			{
				if (p->isCulled())
					continue;
				const Coord3D *at = p->getPosition();
				const float dx = at->x - g.pos[0], dy = at->y - g.pos[1], dz = at->z - g.pos[2];
				const float d = sqrtf(dx * dx + dy * dy + dz * dz) + 0.5f * p->getSize();
				if (d > g.spread)
					g.spread = d;
			}
			const ParticleSystemTemplate *tmpl = sys->getTemplate();
			g.name = (tmpl != NULL) ? tmpl->getName() : AsciiString("?");
			Int slot = (topN < TOP) ? topN++ : TOP - 1;
			if (slot == TOP - 1 && topN == TOP && g.power <= top[TOP - 1].power)
				continue;
			while (slot > 0 && top[slot - 1].power < g.power)
			{
				top[slot] = top[slot - 1];
				--slot;
			}
			top[slot] = g;
		}
		AsciiString line;
		line.format("glowfx: %d particle systems, %d glowing (additive), %d of them on screen; particles %d, on screen %d; total power %.0f",
					systems, additive, onScreen, parts, seenParts, powerAll);
		printAscii(line, FALSE);
		for (Int i = 0; i < topN; ++i)
		{
			const Glow &g = top[i];
			line.format("  %-28s power %6.0f (%2.0f%%)  colour %.2f %.2f %.2f  at %.0f %.0f %.0f  spread %.0f  particles %d/%d",
						g.name.str(), g.power, (powerAll > 0.0f) ? 100.0f * g.power / powerAll : 0.0f, g.col[0], g.col[1], g.col[2],
						g.pos[0], g.pos[1], g.pos[2], g.spread, g.seen, g.parts);
			printAscii(line, FALSE);
		}
	}

	// Ronin @feature 15/10/2026 DX9: `night` - the night look at night (TAA's pass 2, or its own without TAA): dim colour into one
	// tint from the map's light, bright (lamps) keeps its own. Not saved.
	void cmdNight(Int argc, const AsciiString *argv)
	{
		if (argc == 2)
			R3DTaa::setNight((atoi(argv[1].str()) != 0) ? TRUE : FALSE);
		else if (argc == 3 && stricmp(argv[1].str(), "tint") == 0)
			R3DTaa::setNightTint((float)atof(argv[2].str()));
		else if (argc == 3 && stricmp(argv[1].str(), "lo") == 0)
			R3DTaa::setNightLo((float)atof(argv[2].str()));
		else if (argc == 3 && stricmp(argv[1].str(), "hi") == 0)
			R3DTaa::setNightHi((float)atof(argv[2].str()));
		else if (argc == 3 && stricmp(argv[1].str(), "floor") == 0)
			R3DTaa::setNightFloor((float)atof(argv[2].str()));
		else if (argc == 3 && stricmp(argv[1].str(), "full") == 0)
			R3DTaa::setNightFull((float)atof(argv[2].str()));
		else if (argc == 3 && stricmp(argv[1].str(), "lift") == 0)
			R3DTaa::setNightLift((float)atof(argv[2].str()));
		else if (argc != 1)
		{
			printAscii(AsciiString("usage: night [0|1] | night tint <0..1> | night lo <0..1> | night hi <0..1> | night floor <0..1> | night full <0.2..1> | night lift <0.4..1>   (at night; `lights save` keeps them)"), TRUE);
			printAscii(AsciiString("  tint: how much of the map light's colour the dim parts take, 0 = grey; lo / hi: how bright a pixel must be to start / to fully keep its own colour"), FALSE);
			printAscii(AsciiString("  floor: the least of its own colour any pixel keeps - team colours in the dark; 1 = the look off"), FALSE);
			printAscii(AsciiString("  lift: the dark lift - the darkest parts come up most, lamps hardly move; 1 = none, .9 the default (.75 brighter)"), FALSE);
			printAscii(AsciiString("  full: how coloured the map's light must be (its saturation) for the whole look; milder lights get less. Lamp-lit pixels keep their colour"), FALSE);
			return;
		}
		char line[512];
		R3DTaa::describeNight(line, (Int)sizeof(line));
		printAscii(AsciiString(line), FALSE);
	}

	// Ronin @feature 12/10/2026 DX9: `hdr` - HDR stage 1: the scene in a 16-bit float target (TAA's redirect, or its own with TAA off), the sun
	// lifted above 1 there and a tone curve back to the screen. Not saved.
	void cmdHdr(Int argc, const AsciiString *argv)
	{
		if (argc == 2)
			R3DTaa::setFloatScene((atoi(argv[1].str()) != 0) ? TRUE : FALSE);
		else if (argc == 3 && stricmp(argv[1].str(), "sun") == 0)
			R3DTaa::setSunGain((float)atof(argv[2].str()));
		else if (argc == 3 && stricmp(argv[1].str(), "warm") == 0)
			R3DTaa::setSunWarm((float)atof(argv[2].str()));
		else if (argc == 3 && stricmp(argv[1].str(), "white") == 0)
			R3DTaa::setWhite((float)atof(argv[2].str()));
		else if (argc != 1)
		{
			printAscii(AsciiString("usage: hdr [0|1] | hdr sun <1..4> | hdr warm <0..1> | hdr white <1..2>   (needs MSAA off; the sun lift also needs the lights on and the sun's shadow maps)"), TRUE);
			printAscii(AsciiString("  sun: how bright the sun is on what it reaches, for the time of day now showing; 1 = as before"), FALSE);
			printAscii(AsciiString("  warm: the lift's tint, 0 = none, 1 = strongly warm;  white: the scene value shown as white - higher keeps more detail in the brightest, flatter"), FALSE);
			return;
		}
		char line[512];
		R3DTaa::describeScene(line, (Int)sizeof(line));
		printAscii(AsciiString(line), FALSE);
	}

	// Ronin @feature 21/10/2026 DX9: `grade` - the daytime colour in the final pass (R3DTaa): warm tint, saturation, darker mid-tones,
	// from the "Zero Hour: Enhanced" shots measured. Never at night - the night look owns it.
	void cmdGrade(Int argc, const AsciiString *argv)
	{
		if (argc == 2)
			R3DTaa::setGrade((atoi(argv[1].str()) != 0) ? TRUE : FALSE);
		else if (argc == 3 && stricmp(argv[1].str(), "warm") == 0)
			R3DTaa::setGradeWarm((float)atof(argv[2].str()));
		else if (argc == 3 && stricmp(argv[1].str(), "sat") == 0)
			R3DTaa::setGradeSat((float)atof(argv[2].str()));
		else if (argc == 3 && stricmp(argv[1].str(), "mids") == 0)
			R3DTaa::setGradeMids((float)atof(argv[2].str()));
		else if (argc != 1)
		{
			printAscii(AsciiString("usage: grade [0|1] | grade warm <0..2> | grade sat <0..2.5> | grade mids <0.5..2>   (by day; `lights save` keeps it in DX9Lights.ini)"), TRUE);
			printAscii(AsciiString("  warm: 0 = none, 1 = the measured tint (blue to 72%, green to 88%, brightness kept);  sat: 1 = none;  mids: 1 = none, above 1 darker mid-tones"), FALSE);
			return;
		}
		AsciiString line;
		line.format("grade=%d  warm=%.2f  sat=%.2f  mids=%.2f  (by day only: the night look owns the night)", R3DTaa::getGrade() ? 1 : 0,
					R3DTaa::getGradeWarm(), R3DTaa::getGradeSat(), R3DTaa::getGradeMids());
		printAscii(line, FALSE);
	}

	// Ronin @feature 12/10/2026 DX9: `sky` - the map's sky cube (R3DSkyCube): which sky it was made from, its ground, the six
	// colours an ambient takes from it. `sky rebuild` makes it again; `lights view 5` shows it.
	void cmdSky(Int argc, const AsciiString *argv)
	{
		if (argc == 2 && stricmp(argv[1].str(), "rebuild") == 0)
			R3DSkyCube::rebuild();
		else if (argc != 1)
		{
			printAscii(AsciiString("usage: sky | sky rebuild   (`lights view 5` shows the cube)"), TRUE);
			return;
		}
		char line[512];
		R3DSkyCube::describe(line, (Int)sizeof(line));
		printAscii(AsciiString(line), FALSE);
	}

	// Ronin @feature 18/10/2026 DX9: `campitch [degrees | default]` - the camera's tilt, as the debug build's pitch drag sets it
	// (View::userSetPitch). A middle click resets it.
	void cmdCamPitch(Int argc, const AsciiString *argv)
	{
		if (TheTacticalView == nullptr)
			return;
		if (argc == 2 && stricmp(argv[1].str(), "default") == 0)
			TheTacticalView->userSetPitchToDefault();
		else if (argc == 2)
			TheTacticalView->userSetPitch(DEG_TO_RADF((Real)atof(argv[1].str())));
		else if (argc != 1)
		{
			printAscii(AsciiString("usage: campitch [degrees | default]   (0.1..89.9; a middle click resets it)"), TRUE);
			return;
		}
		AsciiString line;
		line.format("campitch: %.1f degrees (default %.1f)", TheTacticalView->getPitch() * 180.0f / PI,
					TheTacticalView->getDefaultPitch() * 180.0f / PI);
		printAscii(line, FALSE);
	}

	// Ronin @feature 06/10/2026 DX9: `timesetting` - the time-of-day lighting, what the debug build's Ctrl+Shift+D does
	// (CommandXlat MSG_META_DEMO_TIME_OF_DAY). Not saved: a map load sets the map's own again.
	void cmdTimeSetting(Int argc, const AsciiString *argv)
	{
		if (argc == 2 && TheWritableGlobalData != nullptr && TheGameClient != nullptr)
		{
			Int tod = isWholeNumber(argv[1].str()) ? atoi(argv[1].str()) : TIME_OF_DAY_INVALID;
			for (Int i = TIME_OF_DAY_FIRST; i < TIME_OF_DAY_COUNT && tod == TIME_OF_DAY_INVALID; ++i)
				if (stricmp(argv[1].str(), TimeOfDayNames[i]) == 0)
					tod = i;
			if (tod < TIME_OF_DAY_FIRST || tod >= TIME_OF_DAY_COUNT)
			{
				printAscii(AsciiString("usage: timesetting [1..4]  (1 morning, 2 afternoon, 3 evening, 4 night - or the name)"), TRUE);
				return;
			}
			// Ronin @feature 06/10/2026 DX9: Object.cpp sets MODELCONDITION_NIGHT from it, so one player alone must not change it.
			if (TheGameLogic != nullptr && TheGameLogic->isInMultiplayerGame())
			{
				printAscii(AsciiString("refused: not in LAN/online matches"), TRUE);
				return;
			}
			if (TheWritableGlobalData->setTimeOfDay((TimeOfDay)tod))
			{
				TheGameClient->setTimeOfDay(TheGlobalData->m_timeOfDay);
				if (TheGlobalData->m_forceModelsToFollowTimeOfDay && TheGameLogic != nullptr)
				{
					for (Object *obj = TheGameLogic->getFirstObject(); obj; obj = obj->getNextObject())
					{
						Drawable *d = obj->getDrawable();
						if (d)
						{
							// Ronin @feature 06/10/2026 DX9: as the hotkey does - this only forces a refresh.
							ModelConditionFlags empty;
							d->clearAndSetModelConditionFlags(empty, empty);
						}
					}
				}
			}
		}
		else if (argc != 1)
		{
			printAscii(AsciiString("usage: timesetting [1..4]  (1 morning, 2 afternoon, 3 evening, 4 night - or the name)"), TRUE);
			return;
		}
		const Int now = (TheGlobalData != nullptr) ? (Int)TheGlobalData->m_timeOfDay : TIME_OF_DAY_INVALID;
		AsciiString state;
		state.format("timesetting: %d (%s)", now, TimeOfDayNames[(now < 0 || now >= TIME_OF_DAY_COUNT) ? 0 : now]);
		printAscii(state, FALSE);
	}

	// Ronin @feature 06/10/2026 DX9: `on` / `off` or a whole number, for the switch commands. FALSE = it is neither.
	Bool parseOnOff(const char *s, Bool *value)
	{
		if (stricmp(s, "on") == 0)
			*value = TRUE;
		else if (stricmp(s, "off") == 0)
			*value = FALSE;
		else if (isWholeNumber(s))
			*value = (atoi(s) != 0);
		else
			return FALSE;
		return TRUE;
	}

	// Ronin @feature 06/10/2026 DX9: `clouds on|off` - the cloud shadows on terrain and models, the Options checkbox's own
	// flag; every reader tests it per frame. Not saved. Night never draws them (BaseHeightMap useCloud).
	void cmdClouds(Int argc, const AsciiString *argv)
	{
		Bool on = FALSE;
		if (argc == 2 && TheWritableGlobalData != nullptr && parseOnOff(argv[1].str(), &on))
			TheWritableGlobalData->m_useCloudMap = on;
		else if (argc != 1)
		{
			printAscii(AsciiString("usage: clouds [on|off]"), TRUE);
			return;
		}
		const Bool set = (TheGlobalData != nullptr) && TheGlobalData->m_useCloudMap;
		const Bool night = (TheGlobalData != nullptr) && TheGlobalData->m_timeOfDay == TIME_OF_DAY_NIGHT;
		AsciiString state;
		state.format("clouds: %s%s", set ? "on" : "off", (set && night) ? "  (not drawn: it is night)" : "");
		printAscii(state, FALSE);
	}

	Bool screenCentreOnTerrain(Coord3D *out)
	{
		if (TheTacticalView == nullptr)
			return FALSE;
		Int x0 = 0, y0 = 0;
		TheTacticalView->getOrigin(&x0, &y0);
		ICoord2D pixel;
		pixel.x = x0 + TheTacticalView->getWidth() / 2;
		pixel.y = y0 + TheTacticalView->getHeight() / 2;
		return TheTacticalView->screenToTerrain(&pixel, out);
	}

	// Ronin @feature 25/10/2026 DX9: `treefire` - the map's trees on fire (R3DTreeFire). `here` lights the tree nearest the middle of
	// the screen; `area` / `scorch <r>` report there what a flame weapon / the nuke reports. Not saved.
	void cmdTreeFire(Int argc, const AsciiString *argv)
	{
		Bool on = FALSE;
		const char *a1 = (argc >= 2) ? argv[1].str() : "";
		const Bool here = (argc == 2 && _stricmp(a1, "here") == 0);
		const Bool area = (argc == 3 && (_stricmp(a1, "area") == 0 || _stricmp(a1, "scorch") == 0));
		if (argc == 2 && parseOnOff(a1, &on))
			R3DTreeFire::setEnabled(on);
		else if (argc == 3 && _stricmp(a1, "cap") == 0 && isWholeNumber(argv[2].str()))
			R3DTreeFire::setCap(atoi(argv[2].str()));
		else if (argc == 3 && _stricmp(a1, "spread") == 0 && parseOnOff(argv[2].str(), &on))
			R3DTreeFire::setSpread(on);
		else if (argc == 3 && _stricmp(a1, "thin") == 0)
			R3DTreeFire::setThin((Real)atof(argv[2].str()));
		else if (here || area)
		{
			Coord3D centre;
			if (!screenCentreOnTerrain(&centre))
			{
				printAscii(AsciiString("treefire: no ground in the middle of the screen"), TRUE);
				return;
			}
			if (here && !R3DTreeFire::igniteNearest(&centre, 80.0f))
				printAscii(AsciiString("treefire: no green tree that can burn within 80 of the middle of the screen"), TRUE);
			if (area)
				R3DTreeFire::onFire(&centre, (Real)atof(argv[2].str()), _stricmp(a1, "scorch") == 0);
		}
		else if (argc != 1)
		{
			printAscii(AsciiString("usage: treefire [0|1] | here | area <radius> | scorch <radius> | cap <fires> | spread 0|1 | thin <0..1>"), TRUE);
			return;
		}
		Int fires = 0, burnt = 0, burntTypes = 0, types = 0;
		R3DTreeFire::stats(&fires, &burnt, &burntTypes, &types);
		AsciiString state;
		state.format("treefire: %s, spread %s, cap %d, thin %.2f | burning %d, burnt %d | tree types %d, %d of them burnt art",
			R3DTreeFire::isEnabled() ? "on" : "off", R3DTreeFire::getSpread() ? "on" : "off", R3DTreeFire::getCap(),
			R3DTreeFire::getThin(), fires, burnt, types, burntTypes);
		printAscii(state, FALSE);
	}

	// Ronin @diagnostic 08/10/2026 DX9: device-object tracking, on only with DX9Track.txt beside the exe at start-up. Every
	// default-pool object is remembered with its call stack and held by one more reference; resetReport lists survivors.
	enum { TRACK_MAX = 16384, TRACK_FRAMES = 12 };
	struct TrackedObject
	{
		IUnknown   *obj;
		const char *kind;
		UINT        a, b;
		DWORD       usage;
		Int         format;
		USHORT      frames;
		void       *stack[TRACK_FRAMES];
	};
	TrackedObject *s_tracked      = nullptr;
	Int            s_trackedCount = 0;
	Int            s_trackedLost  = 0;		// made while the table was full
	Int            s_trackedTotal = 0;
	Int            s_trackSeen    = 0;		// every creation the hooks saw, whatever its pool
	Int            s_trackHist[12][4] = {};	// the same, per hook and memory pool (0 default, 1 managed, 2 system, 3 scratch)
	DWORD          s_trackBirthThread = 0;	// the thread the device was made on (a reset from another one fails)
	Bool           s_trackOn      = FALSE;
	Bool           s_trackHooked  = FALSE;

	void trackAdd(IUnknown *obj, const char *kind, UINT a, UINT b, DWORD usage, Int format)
	{
		if (!s_trackOn || obj == nullptr || s_tracked == nullptr)
			return;
		++s_trackedTotal;
		if (s_trackedCount >= TRACK_MAX)
		{
			++s_trackedLost;
			return;
		}
		TrackedObject &t = s_tracked[s_trackedCount++];
		obj->AddRef();
		t.obj    = obj;
		t.kind   = kind;
		t.a      = a;
		t.b      = b;
		t.usage  = usage;
		t.format = format;
		t.frames = CaptureStackBackTrace(2, TRACK_FRAMES, t.stack, nullptr);	// 2: this function and the hook
	}

	// Ronin @diagnostic 08/10/2026 DX9: forgets every object whose only remaining reference is the tracker's own.
	Int trackSweep(void)
	{
		Int kept = 0;
		for (Int i = 0; i < s_trackedCount; ++i)
		{
			IUnknown *obj = s_tracked[i].obj;
			obj->AddRef();
			if (obj->Release() <= 1)
			{
				obj->Release();
				continue;
			}
			if (kept != i)
				s_tracked[kept] = s_tracked[i];
			++kept;
		}
		s_trackedCount = kept;
		return kept;
	}

	typedef HRESULT (__stdcall *TrackSwapChainFn)(IDirect3DDevice9 *, D3DPRESENT_PARAMETERS *, IDirect3DSwapChain9 **);
	typedef HRESULT (__stdcall *TrackTextureFn)(IDirect3DDevice9 *, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture9 **, HANDLE *);
	typedef HRESULT (__stdcall *TrackVolumeFn)(IDirect3DDevice9 *, UINT, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DVolumeTexture9 **, HANDLE *);
	typedef HRESULT (__stdcall *TrackCubeFn)(IDirect3DDevice9 *, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DCubeTexture9 **, HANDLE *);
	typedef HRESULT (__stdcall *TrackVBFn)(IDirect3DDevice9 *, UINT, DWORD, DWORD, D3DPOOL, IDirect3DVertexBuffer9 **, HANDLE *);
	typedef HRESULT (__stdcall *TrackIBFn)(IDirect3DDevice9 *, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DIndexBuffer9 **, HANDLE *);
	typedef HRESULT (__stdcall *TrackSurfaceFn)(IDirect3DDevice9 *, UINT, UINT, D3DFORMAT, D3DMULTISAMPLE_TYPE, DWORD, BOOL, IDirect3DSurface9 **, HANDLE *);
	typedef HRESULT (__stdcall *TrackPlainFn)(IDirect3DDevice9 *, UINT, UINT, D3DFORMAT, D3DPOOL, IDirect3DSurface9 **, HANDLE *);
	typedef HRESULT (__stdcall *TrackStateFn)(IDirect3DDevice9 *, D3DSTATEBLOCKTYPE, IDirect3DStateBlock9 **);
	typedef HRESULT (__stdcall *TrackEndStateFn)(IDirect3DDevice9 *, IDirect3DStateBlock9 **);
	typedef HRESULT (__stdcall *TrackQueryFn)(IDirect3DDevice9 *, D3DQUERYTYPE, IDirect3DQuery9 **);

	TrackSwapChainFn s_origSwapChain = nullptr;
	TrackTextureFn   s_origTexture   = nullptr;
	TrackVolumeFn    s_origVolume    = nullptr;
	TrackCubeFn      s_origCube      = nullptr;
	TrackVBFn        s_origVB        = nullptr;
	TrackIBFn        s_origIB        = nullptr;
	TrackSurfaceFn   s_origTarget    = nullptr;
	TrackSurfaceFn   s_origDepth     = nullptr;
	TrackPlainFn     s_origPlain     = nullptr;
	TrackStateFn     s_origState     = nullptr;
	TrackEndStateFn  s_origEndState  = nullptr;
	TrackQueryFn     s_origQuery     = nullptr;

	HRESULT __stdcall trackSwapChain(IDirect3DDevice9 *dev, D3DPRESENT_PARAMETERS *pp, IDirect3DSwapChain9 **out)
	{
		++s_trackSeen;
		++s_trackHist[0][((Int)0) & 3];
		const HRESULT hr = s_origSwapChain(dev, pp, out);
		if (SUCCEEDED(hr) && out != nullptr)
			trackAdd(*out, "swap chain", 0, 0, 0, 0);
		return hr;
	}
	HRESULT __stdcall trackTexture(IDirect3DDevice9 *dev, UINT w, UINT h, UINT levels, DWORD usage, D3DFORMAT fmt, D3DPOOL pool, IDirect3DTexture9 **out, HANDLE *shared)
	{
		++s_trackSeen;
		++s_trackHist[1][((Int)pool) & 3];
		const HRESULT hr = s_origTexture(dev, w, h, levels, usage, fmt, pool, out, shared);
		if (SUCCEEDED(hr) && out != nullptr && pool == D3DPOOL_DEFAULT)
			trackAdd(*out, "texture", w, h, usage, (Int)fmt);
		return hr;
	}
	HRESULT __stdcall trackVolume(IDirect3DDevice9 *dev, UINT w, UINT h, UINT d, UINT levels, DWORD usage, D3DFORMAT fmt, D3DPOOL pool, IDirect3DVolumeTexture9 **out, HANDLE *shared)
	{
		++s_trackSeen;
		++s_trackHist[2][((Int)pool) & 3];
		const HRESULT hr = s_origVolume(dev, w, h, d, levels, usage, fmt, pool, out, shared);
		if (SUCCEEDED(hr) && out != nullptr && pool == D3DPOOL_DEFAULT)
			trackAdd(*out, "volume texture", w, h, usage, (Int)fmt);
		return hr;
	}
	HRESULT __stdcall trackCube(IDirect3DDevice9 *dev, UINT edge, UINT levels, DWORD usage, D3DFORMAT fmt, D3DPOOL pool, IDirect3DCubeTexture9 **out, HANDLE *shared)
	{
		++s_trackSeen;
		++s_trackHist[3][((Int)pool) & 3];
		const HRESULT hr = s_origCube(dev, edge, levels, usage, fmt, pool, out, shared);
		if (SUCCEEDED(hr) && out != nullptr && pool == D3DPOOL_DEFAULT)
			trackAdd(*out, "cube texture", edge, edge, usage, (Int)fmt);
		return hr;
	}
	HRESULT __stdcall trackVB(IDirect3DDevice9 *dev, UINT bytes, DWORD usage, DWORD fvf, D3DPOOL pool, IDirect3DVertexBuffer9 **out, HANDLE *shared)
	{
		++s_trackSeen;
		++s_trackHist[4][((Int)pool) & 3];
		const HRESULT hr = s_origVB(dev, bytes, usage, fvf, pool, out, shared);
		if (SUCCEEDED(hr) && out != nullptr && pool == D3DPOOL_DEFAULT)
			trackAdd(*out, "vertex buffer", bytes, fvf, usage, 0);
		return hr;
	}
	HRESULT __stdcall trackIB(IDirect3DDevice9 *dev, UINT bytes, DWORD usage, D3DFORMAT fmt, D3DPOOL pool, IDirect3DIndexBuffer9 **out, HANDLE *shared)
	{
		++s_trackSeen;
		++s_trackHist[5][((Int)pool) & 3];
		const HRESULT hr = s_origIB(dev, bytes, usage, fmt, pool, out, shared);
		if (SUCCEEDED(hr) && out != nullptr && pool == D3DPOOL_DEFAULT)
			trackAdd(*out, "index buffer", bytes, 0, usage, (Int)fmt);
		return hr;
	}
	HRESULT __stdcall trackTarget(IDirect3DDevice9 *dev, UINT w, UINT h, D3DFORMAT fmt, D3DMULTISAMPLE_TYPE ms, DWORD q, BOOL lockable, IDirect3DSurface9 **out, HANDLE *shared)
	{
		++s_trackSeen;
		++s_trackHist[6][((Int)0) & 3];
		const HRESULT hr = s_origTarget(dev, w, h, fmt, ms, q, lockable, out, shared);
		if (SUCCEEDED(hr) && out != nullptr)
			trackAdd(*out, "render target surface", w, h, (DWORD)ms, (Int)fmt);
		return hr;
	}
	HRESULT __stdcall trackDepth(IDirect3DDevice9 *dev, UINT w, UINT h, D3DFORMAT fmt, D3DMULTISAMPLE_TYPE ms, DWORD q, BOOL discard, IDirect3DSurface9 **out, HANDLE *shared)
	{
		++s_trackSeen;
		++s_trackHist[7][((Int)0) & 3];
		const HRESULT hr = s_origDepth(dev, w, h, fmt, ms, q, discard, out, shared);
		if (SUCCEEDED(hr) && out != nullptr)
			trackAdd(*out, "depth surface", w, h, (DWORD)ms, (Int)fmt);
		return hr;
	}
	HRESULT __stdcall trackPlain(IDirect3DDevice9 *dev, UINT w, UINT h, D3DFORMAT fmt, D3DPOOL pool, IDirect3DSurface9 **out, HANDLE *shared)
	{
		++s_trackSeen;
		++s_trackHist[8][((Int)pool) & 3];
		const HRESULT hr = s_origPlain(dev, w, h, fmt, pool, out, shared);
		if (SUCCEEDED(hr) && out != nullptr && pool == D3DPOOL_DEFAULT)
			trackAdd(*out, "plain surface", w, h, 0, (Int)fmt);
		return hr;
	}
	HRESULT __stdcall trackState(IDirect3DDevice9 *dev, D3DSTATEBLOCKTYPE type, IDirect3DStateBlock9 **out)
	{
		++s_trackSeen;
		++s_trackHist[9][((Int)0) & 3];
		const HRESULT hr = s_origState(dev, type, out);
		if (SUCCEEDED(hr) && out != nullptr)
			trackAdd(*out, "state block", (UINT)type, 0, 0, 0);
		return hr;
	}
	HRESULT __stdcall trackEndState(IDirect3DDevice9 *dev, IDirect3DStateBlock9 **out)
	{
		++s_trackSeen;
		++s_trackHist[10][((Int)0) & 3];
		const HRESULT hr = s_origEndState(dev, out);
		if (SUCCEEDED(hr) && out != nullptr)
			trackAdd(*out, "state block (recorded)", 0, 0, 0, 0);
		return hr;
	}
	HRESULT __stdcall trackQuery(IDirect3DDevice9 *dev, D3DQUERYTYPE type, IDirect3DQuery9 **out)
	{
		++s_trackSeen;
		++s_trackHist[11][((Int)0) & 3];
		const HRESULT hr = s_origQuery(dev, type, out);
		if (SUCCEEDED(hr) && out != nullptr)	// out is null when the caller only asks whether the type exists
			trackAdd(*out, "query", (UINT)type, 0, 0, 0);
		return hr;
	}

	// Ronin @diagnostic 08/10/2026 DX9: who takes references on the frame's own surfaces - the device's Get* calls, by call
	// site. A site seen once or twice that is not matched by a release is what a failing reset points at.
	enum { LEDGER_MAX = 512, LEDGER_FRAMES = 6 };
	struct LedgerEntry
	{
		char      kind;		// B back buffer, R render target 0, D depth, S swap chain
		void     *object;
		UnsignedInt count;
		USHORT    frames;
		void     *stack[LEDGER_FRAMES];
	};
	LedgerEntry s_ledger[LEDGER_MAX];
	Int         s_ledgerCount = 0;
	Int         s_ledgerLost  = 0;

	void ledgerAdd(char kind, void *object)
	{
		if (!s_trackOn || object == nullptr)
			return;
		void *stack[LEDGER_FRAMES] = {};
		const USHORT frames = CaptureStackBackTrace(2, LEDGER_FRAMES, stack, nullptr);
		for (Int i = 0; i < s_ledgerCount; ++i)
		{
			LedgerEntry &e = s_ledger[i];
			if (e.kind == kind && e.object == object && e.stack[0] == stack[0] && e.stack[1] == stack[1] && e.stack[2] == stack[2])
			{
				++e.count;
				return;
			}
		}
		if (s_ledgerCount >= LEDGER_MAX)
		{
			++s_ledgerLost;
			return;
		}
		LedgerEntry &e = s_ledger[s_ledgerCount++];
		e.kind   = kind;
		e.object = object;
		e.count  = 1;
		e.frames = frames;
		memcpy(e.stack, stack, sizeof(stack));
	}

	typedef HRESULT (__stdcall *LedgerSwapFn)(IDirect3DDevice9 *, UINT, IDirect3DSwapChain9 **);
	typedef HRESULT (__stdcall *LedgerBackFn)(IDirect3DDevice9 *, UINT, UINT, D3DBACKBUFFER_TYPE, IDirect3DSurface9 **);
	typedef HRESULT (__stdcall *LedgerTargetFn)(IDirect3DDevice9 *, DWORD, IDirect3DSurface9 **);
	typedef HRESULT (__stdcall *LedgerDepthFn)(IDirect3DDevice9 *, IDirect3DSurface9 **);
	LedgerSwapFn   s_origGetSwap   = nullptr;
	LedgerBackFn   s_origGetBack   = nullptr;
	LedgerTargetFn s_origGetTarget = nullptr;
	LedgerDepthFn  s_origGetDepth  = nullptr;

	HRESULT __stdcall ledgerGetSwap(IDirect3DDevice9 *dev, UINT index, IDirect3DSwapChain9 **out)
	{
		const HRESULT hr = s_origGetSwap(dev, index, out);
		if (SUCCEEDED(hr) && out != nullptr)
			ledgerAdd('S', *out);
		return hr;
	}
	HRESULT __stdcall ledgerGetBack(IDirect3DDevice9 *dev, UINT chain, UINT index, D3DBACKBUFFER_TYPE type, IDirect3DSurface9 **out)
	{
		const HRESULT hr = s_origGetBack(dev, chain, index, type, out);
		if (SUCCEEDED(hr) && out != nullptr)
			ledgerAdd('B', *out);
		return hr;
	}
	HRESULT __stdcall ledgerGetTarget(IDirect3DDevice9 *dev, DWORD index, IDirect3DSurface9 **out)
	{
		const HRESULT hr = s_origGetTarget(dev, index, out);
		if (SUCCEEDED(hr) && out != nullptr && index == 0)
			ledgerAdd('R', *out);
		return hr;
	}
	HRESULT __stdcall ledgerGetDepth(IDirect3DDevice9 *dev, IDirect3DSurface9 **out)
	{
		const HRESULT hr = s_origGetDepth(dev, out);
		if (SUCCEEDED(hr) && out != nullptr)
			ledgerAdd('D', *out);
		return hr;
	}

	// Ronin @diagnostic 08/10/2026 DX9: the ledger's entries for one object, fewest calls first - a leak is a rare caller.
	void ledgerReport(FILE *f, const char *label, void *object, char onlyKind)
	{
		Int shown = 0;
		for (UnsignedInt want = 1; want != 0 && shown < 24; want = (want < 0x40000000u) ? want * 4 : 0)
		{
			for (Int i = 0; i < s_ledgerCount && shown < 24; ++i)
			{
				const LedgerEntry &e = s_ledger[i];
				if ((object != nullptr && e.object != object) || (onlyKind != 0 && e.kind != onlyKind) || e.count >= want * 4 || e.count < want)
					continue;
				fprintf(f, "  %s: taken %u times via %c from:", label, e.count, e.kind);
				for (USHORT s = 0; s < e.frames; ++s)
					fprintf(f, " %08X", (unsigned)(UINT_PTR)e.stack[s]);
				fputc('\n', f);
				++shown;
			}
		}
		fflush(f);
	}

	// Ronin @diagnostic 08/10/2026 DX9: one line into the report, flushed at once - a failed reset can take the game down.
	void traceWrite(FILE *f, const char *format, ...)
	{
		va_list args;
		va_start(args, format);
		vfprintf(f, format, args);
		va_end(args);
		fputc('\n', f);
		fflush(f);
	}

	// Ronin @diagnostic 08/10/2026 DX9: every module in the process with its address range - shows an injected overlay,
	// and maps a logged stack address that lies outside the exe.
	void traceModules(FILE *f)
	{
		const HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
		if (snap == INVALID_HANDLE_VALUE)
		{
			traceWrite(f, "modules: no snapshot (%u)", (unsigned)GetLastError());
			return;
		}
		MODULEENTRY32 entry;
		entry.dwSize = sizeof(entry);
		Int count = 0;
		fprintf(f, "modules:\n ");
		for (BOOL more = Module32First(snap, &entry); more; more = Module32Next(snap, &entry))
		{
			fprintf(f, " %s@%08X+%X", entry.szModule, (unsigned)(UINT_PTR)entry.modBaseAddr, (unsigned)entry.modBaseSize);
			if (++count % 4 == 0)
				fprintf(f, "\n ");
		}
		fputc('\n', f);
		fflush(f);
		CloseHandle(snap);
	}

	// Ronin @diagnostic 08/10/2026 DX9: what the last device reset found; `resetcheck` prints it in the panel.
	struct ResetSeen
	{
		Bool    seen;
		HRESULT hr;
		Int     alive;			// tracked default-pool objects still alive as Reset was called
		Int     backHeld;		// references others hold on the back buffer, and on the bound depth
		Int     depthHeld;
		void   *back;
		void   *depth;
	};
	ResetSeen s_lastReset;
	Int       s_resetCount  = 0;
	Bool      s_checkArmed  = FALSE;	// `resetcheck` wants the report even when the reset works
	Bool      s_reportBegun = FALSE;	// the file starts afresh once per run, then grows
	typedef HRESULT (__stdcall *TrackResetFn)(IDirect3DDevice9 *, D3DPRESENT_PARAMETERS *);
	TrackResetFn s_origReset = nullptr;

	// Ronin @diagnostic 08/10/2026 DX9: one block of DX9ResetCheck.txt (beside Options.ini). A survivor is listed with the
	// call stack of whoever made it; `others` counts the references that are not the tracker's own.
	void resetReport(IDirect3DDevice9 *dev, HRESULT hr)
	{
		if (TheGlobalData == nullptr)
			return;
		AsciiString path = TheGlobalData->getPath_UserData();
		path.concat("DX9ResetCheck.txt");
		FILE *f = fopen(path.str(), s_reportBegun ? "at" : "wt");
		if (f == nullptr)
			return;
		s_reportBegun = TRUE;
		const UINT_PTR base = (UINT_PTR)GetModuleHandleA(nullptr);
		const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)base;
		const IMAGE_NT_HEADERS *nt  = (const IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
		dev->AddRef();
		const Int deviceRefs = (Int)dev->Release();
		traceWrite(f, "---- reset %d at panel frame %u: %s (%08X)  aa=%d", s_resetCount, s_frame, SUCCEEDED(hr) ? "OK" : "FAILED",
			(unsigned)hr, DX8Wrapper::Get_Anti_Aliasing_Level());
		traceWrite(f, "exe base=%08X size=%08X  thread=%u (device made on %u)  deviceRefs=%d", (unsigned)base,
			(unsigned)nt->OptionalHeader.SizeOfImage, (unsigned)GetCurrentThreadId(), (unsigned)s_trackBirthThread, deviceRefs);
		traceWrite(f, "as Reset was called: %d tracked objects alive, back buffer held by %d, depth held by %d", s_lastReset.alive,
			s_lastReset.backHeld, s_lastReset.depthHeld);
		traceWrite(f, "since start: %d creations seen, %d default-pool, %d missed (table full)", s_trackSeen, s_trackedTotal,
			s_trackedLost);
		static const char *hookNames[12] = { "swap chain", "texture", "volume texture", "cube texture", "vertex buffer", "index buffer",
			"render target surface", "depth surface", "plain surface", "state block", "state block (recorded)", "query" };
		for (Int k = 0; k < 12; ++k)
			if (s_trackHist[k][0] + s_trackHist[k][1] + s_trackHist[k][2] + s_trackHist[k][3] > 0)
				traceWrite(f, "  seen: %-22s default=%d managed=%d system=%d scratch=%d", hookNames[k], s_trackHist[k][0], s_trackHist[k][1],
					s_trackHist[k][2], s_trackHist[k][3]);
		for (Int i = 0; i < s_trackedCount; ++i)
		{
			const TrackedObject &t = s_tracked[i];
			t.obj->AddRef();
			const ULONG refs = t.obj->Release();
			fprintf(f, "  alive: %s  a=%u b=%u usage=%08X format=%d others=%u  stack:", t.kind, t.a, t.b, (unsigned)t.usage, t.format,
				(unsigned)(refs - 1));
			for (USHORT s = 0; s < t.frames; ++s)
				fprintf(f, " %08X", (unsigned)(UINT_PTR)t.stack[s]);
			fputc('\n', f);
			fflush(f);
		}
		if (s_lastReset.backHeld > 0)
			ledgerReport(f, "back buffer", s_lastReset.back, 0);
		if (s_lastReset.depthHeld > 0)
			ledgerReport(f, "depth", s_lastReset.depth, 0);
		if (FAILED(hr))
			traceModules(f);
		fclose(f);
	}

	// Ronin @diagnostic 08/10/2026 DX9: IDirect3DDevice9::Reset. The sweep first, so a reference of the tracker's own never
	// fails one; what is left is exactly what the game's release list missed. A failed reset is always reported.
	HRESULT __stdcall trackReset(IDirect3DDevice9 *dev, D3DPRESENT_PARAMETERS *pp)
	{
		++s_resetCount;
		s_lastReset.alive    = s_trackOn ? trackSweep() : 0;
		s_lastReset.backHeld = s_lastReset.depthHeld = -1;
		s_lastReset.back     = s_lastReset.depth = nullptr;
		IDirect3DSurface9 *surf = nullptr;
		if (SUCCEEDED(s_origGetBack(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &surf)) && surf != nullptr)
		{
			s_lastReset.back     = surf;
			s_lastReset.backHeld = (Int)surf->Release();	// what is left once ours is gone: anyone else's
		}
		surf = nullptr;
		if (SUCCEEDED(s_origGetDepth(dev, &surf)) && surf != nullptr)
		{
			s_lastReset.depth     = surf;
			s_lastReset.depthHeld = (Int)surf->Release();
		}
		const HRESULT hr = s_origReset(dev, pp);
		s_lastReset.seen = TRUE;
		s_lastReset.hr   = hr;
		if (FAILED(hr) || s_checkArmed)
			resetReport(dev, hr);
		return hr;
	}

	// Ronin @diagnostic 08/10/2026 DX9: patches the device's method table in place - IDirect3DDevice9's slots by number.
	// The originals are kept and every hook calls through, so with tracking off they cost one branch.
	Bool trackInstall(IDirect3DDevice9 *dev)
	{
		if (s_trackHooked)
			return TRUE;
		void **table = *(void ***)dev;
		DWORD oldProtect = 0;
		if (!VirtualProtect(table, 119 * sizeof(void *), PAGE_READWRITE, &oldProtect))
			return FALSE;
		s_origSwapChain = (TrackSwapChainFn)table[13];	table[13]  = (void *)trackSwapChain;
		s_origTexture   = (TrackTextureFn)table[23];	table[23]  = (void *)trackTexture;
		s_origVolume    = (TrackVolumeFn)table[24];		table[24]  = (void *)trackVolume;
		s_origCube      = (TrackCubeFn)table[25];		table[25]  = (void *)trackCube;
		s_origVB        = (TrackVBFn)table[26];			table[26]  = (void *)trackVB;
		s_origIB        = (TrackIBFn)table[27];			table[27]  = (void *)trackIB;
		s_origTarget    = (TrackSurfaceFn)table[28];	table[28]  = (void *)trackTarget;
		s_origDepth     = (TrackSurfaceFn)table[29];	table[29]  = (void *)trackDepth;
		s_origPlain     = (TrackPlainFn)table[36];		table[36]  = (void *)trackPlain;
		s_origState     = (TrackStateFn)table[59];		table[59]  = (void *)trackState;
		s_origEndState  = (TrackEndStateFn)table[61];	table[61]  = (void *)trackEndState;
		s_origQuery     = (TrackQueryFn)table[118];		table[118] = (void *)trackQuery;
		s_origGetSwap   = (LedgerSwapFn)table[14];		table[14]  = (void *)ledgerGetSwap;
		s_origGetBack   = (LedgerBackFn)table[18];		table[18]  = (void *)ledgerGetBack;
		s_origGetTarget = (LedgerTargetFn)table[38];	table[38]  = (void *)ledgerGetTarget;
		s_origGetDepth  = (LedgerDepthFn)table[40];		table[40]  = (void *)ledgerGetDepth;
		s_origReset     = (TrackResetFn)table[16];		table[16]  = (void *)trackReset;
		VirtualProtect(table, 119 * sizeof(void *), oldProtect, &oldProtect);
		s_trackHooked = TRUE;
		return TRUE;
	}

	// Ronin @diagnostic 08/10/2026 DX9: tracking from the device's birth. IDirect3D9::CreateDevice is slot 16; the device
	// hooks go in before the game makes its first object, so what start-up creates is seen too.
	typedef HRESULT (__stdcall *TrackBirthFn)(IDirect3D9 *, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS *, IDirect3DDevice9 **);
	typedef IDirect3D9 *(__stdcall *TrackCreate9Fn)(UINT);
	TrackBirthFn   s_origBirth   = nullptr;
	TrackCreate9Fn s_origCreate9 = nullptr;

	HRESULT __stdcall trackBirth(IDirect3D9 *d3d, UINT adapter, D3DDEVTYPE type, HWND window, DWORD flags, D3DPRESENT_PARAMETERS *pp,
		IDirect3DDevice9 **out)
	{
		const HRESULT hr = s_origBirth(d3d, adapter, type, window, flags, pp, out);
		if (SUCCEEDED(hr))
			s_trackBirthThread = GetCurrentThreadId();
		if (SUCCEEDED(hr) && out != nullptr && *out != nullptr && !s_trackHooked)
		{
			if (s_tracked == nullptr)
				s_tracked = new TrackedObject[TRACK_MAX];
			if (trackInstall(*out))
				s_trackOn = TRUE;
		}
		return hr;
	}

	IDirect3D9 * __stdcall trackCreate9(UINT sdkVersion)
	{
		IDirect3D9 *d3d = s_origCreate9(sdkVersion);
		if (d3d != nullptr && s_origBirth == nullptr)
		{
			void **table = *(void ***)d3d;
			DWORD oldProtect = 0;
			if (VirtualProtect(&table[16], sizeof(void *), PAGE_READWRITE, &oldProtect))
			{
				s_origBirth = (TrackBirthFn)table[16];
				table[16]   = (void *)trackBirth;
				VirtualProtect(&table[16], sizeof(void *), oldProtect, &oldProtect);
			}
		}
		return d3d;
	}

	// Ronin @diagnostic 08/10/2026 DX9: runs before main(). Only with DX9Track.txt beside the exe: points d3d9's
	// Direct3DCreate9 export at the hook above, which the wrapper then finds through GetProcAddress. 32-bit only.
	struct TrackBirthHook
	{
		TrackBirthHook()
		{
			char path[MAX_PATH];
			const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
			if (length == 0 || length >= MAX_PATH - 16)
				return;
			char *slash = strrchr(path, '\\');
			if (slash == nullptr)
				return;
			strcpy(slash + 1, "DX9Track.txt");
			if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES)
				return;
			const HMODULE lib = LoadLibraryA("D3D9.DLL");
			if (lib == nullptr)
				return;
			BYTE *base = (BYTE *)lib;
			const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)base;
			const IMAGE_NT_HEADERS *nt  = (const IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
			const IMAGE_DATA_DIRECTORY &dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
			if (dir.VirtualAddress == 0)
				return;
			const IMAGE_EXPORT_DIRECTORY *exports = (const IMAGE_EXPORT_DIRECTORY *)(base + dir.VirtualAddress);
			const DWORD *names    = (const DWORD *)(base + exports->AddressOfNames);
			const WORD  *ordinals = (const WORD *)(base + exports->AddressOfNameOrdinals);
			DWORD       *entries  = (DWORD *)(base + exports->AddressOfFunctions);
			for (DWORD i = 0; i < exports->NumberOfNames; ++i)
			{
				if (strcmp((const char *)(base + names[i]), "Direct3DCreate9") != 0)
					continue;
				DWORD *entry = &entries[ordinals[i]];
				DWORD oldProtect = 0;
				if (VirtualProtect(entry, sizeof(DWORD), PAGE_READWRITE, &oldProtect))
				{
					s_origCreate9 = (TrackCreate9Fn)(base + *entry);
					*entry = (DWORD)((UINT_PTR)trackCreate9 - (UINT_PTR)base);
					VirtualProtect(entry, sizeof(DWORD), oldProtect, &oldProtect);
				}
				break;
			}
		}
	};
	TrackBirthHook s_trackBirthHook;

	// Ronin @diagnostic 08/10/2026 DX9: `resetcheck` - one device reset at the same settings, by the game's own path. With
	// DX9Track.txt beside the exe at start-up, DX9ResetCheck.txt lists what was still alive when Reset was called.
	void cmdResetCheck(Int argc, const AsciiString *argv)
	{
		if (argc != 1 || TheDisplay == nullptr || DX8Wrapper::_Get_D3D_Device8() == nullptr)
		{
			printAscii(AsciiString("usage: resetcheck"), TRUE);
			return;
		}
		s_lastReset.seen = FALSE;
		s_checkArmed = TRUE;
		const Bool ok = TheDisplay->setDisplayMode(TheDisplay->getWidth(), TheDisplay->getHeight(), TheDisplay->getBitDepth(),
			TheDisplay->getWindowed());
		s_checkArmed = FALSE;
		AsciiString line;
		if (!s_trackHooked)
			line.format("resetcheck: reset %s  (tracking off: start the game with DX9Track.txt beside the exe for the list)",
				ok ? "OK" : "FAILED");
		else if (s_lastReset.seen)
			line.format("resetcheck: reset %s (%08X)  alive=%d backHeld=%d depthHeld=%d  - DX9ResetCheck.txt in the user data folder",
				ok ? "OK" : "FAILED", (unsigned)s_lastReset.hr, s_lastReset.alive, s_lastReset.backHeld, s_lastReset.depthHeld);
		else
			line.format("resetcheck: %s, but the device was never asked to reset", ok ? "OK" : "FAILED");
		printAscii(line, ok ? FALSE : TRUE);
	}

	// Ronin @feature 07/10/2026 DX9: `msaa 0|2|4|8` - the Options menu's anti-aliasing at runtime, by the device reset that
	// menu does for it (OptionsMenu.cpp saveOptions). Not saved.
	void cmdMsaa(Int argc, const AsciiString *argv)
	{
		if (argc == 2 && isWholeNumber(argv[1].str()) && TheWritableGlobalData != nullptr && TheDisplay != nullptr)
		{
			Int level = atoi(argv[1].str());
			level = (level <= 0) ? 0 : ((level <= 2) ? 2 : ((level <= 4) ? 4 : 8));
			if (DX8Wrapper::Get_Anti_Aliasing_Level() != level)
			{
				TheWritableGlobalData->m_antiAliasLevel = level;
				if (!TheDisplay->setDisplayMode(TheDisplay->getWidth(), TheDisplay->getHeight(), TheDisplay->getBitDepth(),
						TheDisplay->getWindowed()))
				{
					TheWritableGlobalData->m_antiAliasLevel = DX8Wrapper::Get_Anti_Aliasing_Level();
					printAscii(AsciiString("msaa: the device reset FAILED - `resetcheck` says what blocks it"), TRUE);
					return;
				}
			}
		}
		else if (argc != 1)
		{
			printAscii(AsciiString("usage: msaa [0|2|4|8]"), TRUE);
			return;
		}
		AsciiString state;
		state.format("msaa: %d%s", DX8Wrapper::Get_Anti_Aliasing_Level(),
			(DX8Wrapper::Get_Anti_Aliasing_Level() == 0) ? "" : "  (TAA does not run with it)");
		printAscii(state, FALSE);
	}

	// Ronin @diagnostic 16/09/2026 DX9: tune the placement grid while it is on screen (hold Ctrl). `grid` prints the knobs,
	// `grid <knob> <value>` sets one.
	void cmdGrid(Int argc, const AsciiString *argv)
	{
		Bool ok = (argc == 1);
		if (argc == 3)
		{
			const Real value = (Real)atof(argv[2].str());
			if      (stricmp(argv[1].str(), "lift")   == 0) { TheDebugGrid.lift            = value;      ok = TRUE; }
			else if (stricmp(argv[1].str(), "width")  == 0) { TheDebugGrid.halfWidthPixels = value;      ok = TRUE; }
			else if (stricmp(argv[1].str(), "alpha")  == 0) { TheDebugGrid.alpha           = value;      ok = TRUE; }
			else if (stricmp(argv[1].str(), "radius") == 0) { TheDebugGrid.radius          = (Int)value; ok = TRUE; }
		}
		if (!ok)
		{
			printAscii(AsciiString("usage: grid [lift|width|alpha|radius <value>] - radius 0 sizes it from the drop"), TRUE);
			return;
		}
		AsciiString state;
		state.format("grid: lift=%.2f width=%.2fpx alpha=%.2f radius=%d%s",
			TheDebugGrid.lift, TheDebugGrid.halfWidthPixels, TheDebugGrid.alpha, TheDebugGrid.radius,
			(TheDebugGrid.radius > 0) ? "" : " (from the drop)");
		printAscii(state, FALSE);

	}

	// Ronin @feature 16/09/2026 DX9: edit distance (insert/delete/replace) between two names, each capped at 64 characters.
	Int nameDistance(const char *a, const char *b)
	{
		enum { MAXLEN = 64 };
		Int la = (Int)strlen(a), lb = (Int)strlen(b);
		if (la > MAXLEN) la = MAXLEN;
		if (lb > MAXLEN) lb = MAXLEN;
		Int prev[MAXLEN + 1], cur[MAXLEN + 1];
		for (Int j = 0; j <= lb; ++j) prev[j] = j;
		for (Int i = 1; i <= la; ++i)
		{
			cur[0] = i;
			for (Int j = 1; j <= lb; ++j)
			{
				Int best = prev[j - 1] + ((a[i - 1] == b[j - 1]) ? 0 : 1);
				if (prev[j] + 1 < best)   best = prev[j] + 1;
				if (cur[j - 1] + 1 < best) best = cur[j - 1] + 1;
				cur[j] = best;
			}
			for (Int j = 0; j <= lb; ++j) prev[j] = cur[j];
		}
		return prev[lb];
	}


	// Ronin @feature 15/09/2026 DX9: game-changing commands are single player only (campaign, skirmish). isInMultiplayerGame()
	// is exactly LAN|INTERNET; replays must play back only what was recorded.
	// Ronin @feature 16/09/2026 DX9: the refusal text on its own, so the picker can show it as its only row.
	const char *logicRefusal()
	{
		if (TheGameLogic == nullptr || !TheGameLogic->isInGame() || TheGameLogic->isInShellGame())
			return "refused: only in a match";
		if (TheGameLogic->isInMultiplayerGame())
			return "refused: not in LAN/online matches";
		if (TheGameLogic->isInReplayGame())
			return "refused: not while watching a replay";
		return nullptr;
	}

	Bool logicCommandAllowed()
	{
		const char *refusal = logicRefusal();
		if (refusal != nullptr)
			printAscii(AsciiString(refusal), TRUE);
		return refusal == nullptr;
	}

	// Ronin @feature 28/09/2026 DX9: `credits` prints your money, `credits <amount>` adds it. A logic message like spawn, so
	// GameLogic applies it next logic frame and a skirmish replay reproduces it.
	void cmdCredits(Int argc, const AsciiString *argv)
	{
		Player *local = (ThePlayerList != nullptr) ? ThePlayerList->getLocalPlayer() : nullptr;
		const Bool give = (argc == 2 && isWholeNumber(argv[1].str()) && atoi(argv[1].str()) > 0);
		if (!give && argc != 1)
		{
			printAscii(AsciiString("usage: credits [amount] - a whole number above 0"), TRUE);
			return;
		}
		if (local == nullptr)
		{
			printAscii(AsciiString("credits: no local player"), TRUE);
			return;
		}
		AsciiString line;
		if (give)
		{
			if (!logicCommandAllowed())
				return;
			GameMessage *msg = TheMessageStream->appendMessage(GameMessage::MSG_DEV_ADD_CASH);
			msg->appendIntegerArgument(atoi(argv[1].str()));
			line.format("credits: +%d (had %u)", atoi(argv[1].str()), local->getMoney()->countMoney());
		}
		else
			line.format("credits: %u", local->getMoney()->countMoney());
		printAscii(line, FALSE);
	}

	// Ronin @feature 28/09/2026 DX9: `power` prints the state, `power 0|1` switches unlimited power for your player. plants and
	// used are the real numbers - the power bar keeps showing them while it is on.
	void cmdPower(Int argc, const AsciiString *argv)
	{
		Player *local = (ThePlayerList != nullptr) ? ThePlayerList->getLocalPlayer() : nullptr;
		const Bool set = (argc == 2 && isWholeNumber(argv[1].str()));
		if (!set && argc != 1)
		{
			printAscii(AsciiString("usage: power [0|1] - 1 = never short of power, 0 = your real output again"), TRUE);
			return;
		}
		if (local == nullptr)
		{
			printAscii(AsciiString("power: no local player"), TRUE);
			return;
		}
		const Energy *energy = local->getEnergy();
		Bool unlimited = energy->isUnlimited();
		if (set)
		{
			if (!logicCommandAllowed())
				return;
			unlimited = (atoi(argv[1].str()) != 0);
			GameMessage *msg = TheMessageStream->appendMessage(GameMessage::MSG_DEV_SET_POWER);
			msg->appendIntegerArgument(unlimited ? 1 : 0);
		}
		AsciiString line;
		line.format("power: unlimited=%d  plants=%d  used=%d", unlimited ? 1 : 0, energy->getProduction(), energy->getConsumption());
		printAscii(line, FALSE);
	}

	// Ronin @feature 06/10/2026 DX9: is every partition cell clear for this player - the engine's permanent reveal is on.
	Bool mapFullyRevealed(Int playerIndex)
	{
		if (ThePartitionManager == nullptr || TheGameLogic == nullptr || !TheGameLogic->isInGame())
			return FALSE;
		const Int cellsX = ThePartitionManager->getCellCountX();
		const Int cellsY = ThePartitionManager->getCellCountY();
		for (Int y = 0; y < cellsY; ++y)
			for (Int x = 0; x < cellsX; ++x)
				if (ThePartitionManager->getShroudStatusForPlayer(playerIndex, x, y) != CELLSHROUD_CLEAR)
					return FALSE;
		return (cellsX > 0 && cellsY > 0);
	}

	// Ronin @feature 06/10/2026 DX9: `mapvision on|off` - the whole map revealed for your player. A logic message like
	// credits, so a skirmish replay reproduces it. `off` leaves the map explored but fogged.
	void cmdMapVision(Int argc, const AsciiString *argv)
	{
		Bool want = FALSE;
		const Bool set = (argc == 2 && parseOnOff(argv[1].str(), &want));
		if (!set && argc != 1)
		{
			printAscii(AsciiString("usage: mapvision [on|off]"), TRUE);
			return;
		}
		Player *local = (ThePlayerList != nullptr) ? ThePlayerList->getLocalPlayer() : nullptr;
		if (local == nullptr)
		{
			printAscii(AsciiString("mapvision: no local player"), TRUE);
			return;
		}
		const Bool now = mapFullyRevealed(local->getPlayerIndex());
		AsciiString line;
		if (set)
		{
			if (!logicCommandAllowed())
				return;
			GameMessage *msg = TheMessageStream->appendMessage(GameMessage::MSG_DEV_SET_MAPVISION);
			msg->appendIntegerArgument(want ? 1 : 0);
			line.format("mapvision: %s%s", want ? "on" : "off", (want == now) ? " (it already was)" : "");
		}
		else
			line.format("mapvision: %s", now ? "on" : "off");
		printAscii(line, FALSE);
	}

	// Ronin @feature 16/09/2026 DX9: the command box, for a command that leaves text in it (spawn reopens its picker).
	GameWindow *findEntry()
	{
		GameWindow *panel = TheWindowManager->winGetWindowFromId(nullptr, PANEL_WINDOW_ID);
		return (panel != nullptr && panel->winGetChild() != nullptr)
			? TheWindowManager->winGetWindowFromId(panel->winGetChild(), ENTRY_WINDOW_ID) : nullptr;
	}

	// Ronin @feature 16/09/2026 DX9: placement mode (step 7c3). The object rides the cursor, a left click drops one where it
	// points and the cursor keeps it, right click or Esc stops. Like the dozer's build cursor, except nothing is built: every
	// click sends MSG_DEV_SPAWN_OBJECT, so GameLogic creates it and a skirmish replay records it.

	// Ronin @feature 16/09/2026 DX9: hold Ctrl to line things up — the spot snaps to SNAP_CELL cells and the facing
	// to quarter turns. An odd footprint sits on a cell centre, an even one on the corner where four cells meet, so buildings
	// snapped this way stand edge to edge instead of half a cell apart.
	Bool snapHeld()
	{
		return TheKeyboard != nullptr && TheKeyboard->isCtrl();
	}

	void snapToCells(const ThingTemplate *thing, Coord3D *world)
	{
		Real offset = 0.0f;
		if (thing != nullptr)
		{
			const Int cells = (Int)(thing->getTemplateGeometryInfo().getMajorRadius() * 2.0f / SNAP_CELL + 0.5f);
			if ((cells % 2) != 0)
				offset = SNAP_CELL * 0.5f;
		}
		world->x = (Real)REAL_TO_INT_FLOOR((world->x - offset) / SNAP_CELL + 0.5f) * SNAP_CELL + offset;
		world->y = (Real)REAL_TO_INT_FLOOR((world->y - offset) / SNAP_CELL + 0.5f) * SNAP_CELL + offset;
	}

	Real snapToQuarterTurn(Real angle)
	{
		return (Real)REAL_TO_INT_FLOOR(angle / SNAP_ANGLE + 0.5f) * SNAP_ANGLE;
	}

	void endPlacement()
	{
		for (Int i = 0; i < s_ghostCount; ++i)
		{
			Drawable *ghost = (TheGameClient != nullptr) ? TheGameClient->findDrawableByID(s_ghostIDs[i]) : nullptr;
			if (ghost != nullptr)
				TheGameClient->destroyDrawable(ghost);
			s_ghostIDs[i] = INVALID_DRAWABLE_ID;
		}
		s_ghostCount    = 0;
		s_placeID       = -1;
		s_placeAnchored = FALSE;
		R3DDebugGrid::hide();
	}

	// Ronin @feature 16/09/2026 DX9: where copy `index` of `count` lands. MUST match the grid in GameLogic::onDevSpawnObject
	// (GameLogicDispatch.cpp) — the preview is a promise about where they will be.
	void placementSpot(const ThingTemplate *thing, Int index, Int count, const Coord3D &centre, Coord3D *out)
	{
		Int side = 1;
		while (side * side < count)
			++side;
		const Real spacing = thing->getTemplateGeometryInfo().getBoundingCircleRadius() * 2.0f + 5.0f;
		out->x = centre.x + ((Real)(index % side) - (Real)(side - 1) * 0.5f) * spacing;
		out->y = centre.y + ((Real)(index / side) - (Real)(side - 1) * 0.5f) * spacing;
		out->z = (TheTerrainLogic != nullptr) ? TheTerrainLogic->getGroundHeight(out->x, out->y) : centre.z;
	}

	// Ronin @feature 16/09/2026 DX9: how far the block of copies reaches from its centre, in world units. The grid highlights
	// the cells inside this, and sizes its patch to reach past it — a block of 20 covers far more ground than a block of 5.
	Real placementHalfExtent(const ThingTemplate *thing, Int count)
	{
		Int side = 1;
		while (side * side < count)
			++side;
		const Real spacing = thing->getTemplateGeometryInfo().getBoundingCircleRadius() * 2.0f + 5.0f;
		return ((Real)(side - 1) * 0.5f + 0.5f) * spacing;
	}

	Int gridRadiusFor(const ThingTemplate *thing, Int count)
	{
		const Int cells = (Int)(placementHalfExtent(thing, count) / SNAP_CELL) + 2;
		return (cells < 4) ? 4 : cells;
	}


	void startPlacement(const ThingTemplate *thing, Int count)
	{
		endPlacement();
		if (thing == nullptr)
			return;
		const Int limit = (thing->getEditorSorting() == ES_STRUCTURE) ? PLACE_MAX_STRUCTURES : PLACE_MAX_UNITS;
		if (count < 1)
			count = 1;
		if (count > limit)
		{
			AsciiString capped;
			capped.format("%d is more than this drops at once - using %d", count, limit);
			printAscii(capped, FALSE);
			count = limit;
		}
		s_placeID      = (Int)thing->getTemplateID();
		s_placeCount   = count;
		s_placeAngle   = thing->getPlacementViewAngle();
		s_placeAnchored = FALSE;

		AsciiString line;
		line.format("placing %d x %s - left click drops one (hold and drag to turn it, Ctrl to snap), right click or Esc stops",
			count, thing->getName().str());
		printAscii(line, FALSE);
	}

	// One drop at the pinned spot, facing s_placeAngle; the cursor keeps the object for the next click.
	Bool placeAtWorld(const Coord3D &world)
	{
		if (s_placeID < 0 || TheMessageStream == nullptr || ThePlayerList == nullptr)
			return FALSE;
		if (!logicCommandAllowed())
		{
			endPlacement();
			return FALSE;
		}
		Player *local = ThePlayerList->getLocalPlayer();
		if (local == nullptr)
			return FALSE;

		// Snapped here as well as in the preview, so a click and release inside one frame still lands on the cell.
		Coord3D at = world;
		if (snapHeld())
		{
			snapToCells((TheThingFactory != nullptr) ? TheThingFactory->findByTemplateID((UnsignedShort)s_placeID) : nullptr, &at);
			s_placeAngle = snapToQuarterTurn(s_placeAngle);
		}

		GameMessage *msg = TheMessageStream->appendMessage(GameMessage::MSG_DEV_SPAWN_OBJECT);
		msg->appendIntegerArgument(s_placeID);
		msg->appendLocationArgument(at);
		msg->appendIntegerArgument(s_placeCount);
		msg->appendIntegerArgument((Int)local->getPlayerIndex());
		msg->appendRealArgument(s_placeAngle);
		return TRUE;
	}

	// Ronin @feature 16/09/2026 DX9: the things on the cursor are drawables with no object, like the dozer's build icon, so the
	// selection code and the game logic never see them. Looked up by id every frame: the end of a match destroys every drawable.
	void updatePlacement()
	{
		if (s_placeID < 0)
			return;
		if (!s_visible || logicRefusal() != nullptr || TheThingFactory == nullptr || TheGameClient == nullptr)
		{
			endPlacement();
			return;
		}
		const ThingTemplate *thing = TheThingFactory->findByTemplateID((UnsignedShort)s_placeID);
		if (thing == nullptr)
		{
			endPlacement();
			return;
		}
		if (TheMouse == nullptr || TheTacticalView == nullptr)
			return;

		// Where the block of copies is centred: the pinned spot while the button is held, the cursor otherwise.
		ICoord2D pixel = TheMouse->getMouseStatus()->pos;
		Coord3D centre;
		if (s_placeAnchored)
		{
			// Held: the copies stay on the spot that was pressed and the drag away from it picks the facing.
			const Int dx = pixel.x - s_placePress.x;
			const Int dy = pixel.y - s_placePress.y;
			Coord3D world;
			if (dx * dx + dy * dy >= PLACE_DRAG_PIXELS * PLACE_DRAG_PIXELS &&
				TheTacticalView->screenToTerrain(&pixel, &world))
			{
				Coord2D facing;
				facing.x = world.x - s_placeAnchor.x;
				facing.y = world.y - s_placeAnchor.y;
				if (facing.x != 0.0f || facing.y != 0.0f)
					s_placeAngle = facing.toAngle();
			}
			// Snapping the pinned spot again every frame costs nothing and catches Ctrl pressed after the button went down.
			if (snapHeld())
				snapToCells(thing, &s_placeAnchor);
			centre = s_placeAnchor;
		}
		else
		{
			if (!TheTacticalView->screenToTerrain(&pixel, &centre))
				return;
			if (snapHeld())
				snapToCells(thing, &centre);
		}
		if (snapHeld())
			s_placeAngle = snapToQuarterTurn(s_placeAngle);

		// One ghost per copy, on the same grid the drop will use. Their spots are kept so the grid can mark the cells they
		// stand on — the same numbers, so the green cannot disagree with the ghosts.
		Coord3D spots[PLACE_MAX_COUNT];
		s_ghostCount = (s_placeCount < PLACE_MAX_COUNT) ? s_placeCount : PLACE_MAX_COUNT;
		for (Int i = 0; i < s_ghostCount; ++i)
		{
			Drawable *ghost = TheGameClient->findDrawableByID(s_ghostIDs[i]);
			if (ghost == nullptr)
			{
				// Ronin @bugfix 18/09/2026 DX9: CURSOR_PREVIEW, or the shadow code bakes these as static scenery — they have
				// no object — and their shadows trail behind the cursor.
				ghost = TheThingFactory->newDrawable(thing,
					(DrawableStatusBits)(DRAWABLE_STATUS_NO_STATE_PARTICLES | DRAWABLE_STATUS_CURSOR_PREVIEW));
				if (ghost == nullptr)
					break;
				ghost->setDrawableOpacity((TheGlobalData != nullptr) ? TheGlobalData->m_objectPlacementOpacity : 0.5f);
				s_ghostIDs[i] = ghost->getID();
			}
			placementSpot(thing, i, s_ghostCount, centre, &spots[i]);
			ghost->setPosition(&spots[i]);
			ghost->setOrientation(s_placeAngle);
		}

		// Ronin @feature 16/09/2026 DX9: the cells only mean anything while Ctrl snaps to them, so that is exactly when the
		// grid is drawn — lines you cannot land on would be lying.
		if (snapHeld())
		{
			R3DDebugGrid::show(centre, SNAP_CELL, gridRadiusFor(thing, s_ghostCount), spots, s_ghostCount,
				thing->getTemplateGeometryInfo().getBoundingCircleRadius());
		}
		else
		{
			R3DDebugGrid::hide();
		}

	}

	// Ronin @feature 16/09/2026 DX9: while placing, the mouse is ours — taken before the selection (50) and command (70)
	// translators, the way PlaceEventTranslator takes it for the dozer. Does nothing while nothing is on the cursor.
	class DevPlaceTranslator : public GameMessageTranslator
	{
	public:
		virtual GameMessageDisposition translateGameMessage(const GameMessage *msg) override
		{
			const GameMessage::Type type = msg->getType();

			if (type == GameMessage::MSG_RAW_KEY_UP && s_eatEscUp && msg->getArgument(0)->integer == KEY_ESC)
			{
				s_eatEscUp = FALSE;
				return DESTROY_MESSAGE;
			}
			if (s_placeID < 0)
				return KEEP_MESSAGE;

			switch (type)
			{
				case GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_DOWN:
				case GameMessage::MSG_RAW_MOUSE_LEFT_DOUBLE_CLICK:
				{
					// The press pins the spot; until the button comes up the drag only turns the object.
					ICoord2D pixel = msg->getArgument(0)->pixel;
					Coord3D world;
					if (TheTacticalView != nullptr && TheTacticalView->screenToTerrain(&pixel, &world))
					{
						s_placePress    = pixel;
						s_placeAnchor   = world;
						s_placeAnchored = TRUE;
					}
					return DESTROY_MESSAGE;
				}

				case GameMessage::MSG_RAW_MOUSE_LEFT_DRAG:
					return DESTROY_MESSAGE;

				case GameMessage::MSG_RAW_MOUSE_LEFT_BUTTON_UP:
					if (s_placeAnchored)
					{
						placeAtWorld(s_placeAnchor);
						s_placeAnchored = FALSE;
					}
					return DESTROY_MESSAGE;

				case GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_DOWN:
					s_placePress = msg->getArgument(0)->pixel;
					return KEEP_MESSAGE;				// a right drag still scrolls the camera

				case GameMessage::MSG_RAW_MOUSE_RIGHT_BUTTON_UP:
				{
					const ICoord2D pixel = msg->getArgument(0)->pixel;
					if (abs(pixel.x - s_placePress.x) > 2 || abs(pixel.y - s_placePress.y) > 2)
						return KEEP_MESSAGE;			// that was a scroll, not a click
					endPlacement();
					printAscii(AsciiString("placement off"), FALSE);
					return DESTROY_MESSAGE;				// eaten, so nothing selected gets a move order
				}

				case GameMessage::MSG_RAW_KEY_DOWN:
					if (msg->getArgument(0)->integer == KEY_ESC)
					{
						endPlacement();
						printAscii(AsciiString("placement off"), FALSE);
						s_eatEscUp = TRUE;
						return DESTROY_MESSAGE;
					}
					break;

				default:
					break;
			}
			return KEEP_MESSAGE;
		}
	};

	// Ronin @feature 15/09/2026 DX9: `spawn <ThingTemplate> [count]` spawns directly.
	// Ronin @feature 16/09/2026 DX9: `spawn` alone, or a name that is no template, leaves the text in the box: the picker opens.
	void cmdSpawn(Int argc, const AsciiString *argv)
	{
		if (!logicCommandAllowed() || TheThingFactory == nullptr)
			return;

		GameWindow *entry = findEntry();
		if (argc < 2)
		{
			if (entry != nullptr)
				GadgetTextEntrySetText(entry, UnicodeString(L"spawn "));
			printAscii(AsciiString("type to filter, click a row to put it on the cursor; click [Side]/[Category] to step, right click back"), FALSE);
			return;
		}

		// Exact name, then the same name ignoring case.
		const ThingTemplate *thing = TheThingFactory->findTemplate(argv[1], FALSE);
		if (thing == nullptr)
		{
			for (const ThingTemplate *t = TheThingFactory->firstTemplate(); t != nullptr; t = t->friend_getNextTemplate())
			{
				if (stricmp(t->getName().str(), argv[1].str()) == 0)
				{
					thing = t;
					break;
				}
			}
		}
		if (thing == nullptr)
		{
			AsciiString err;
			err.format("no object named '%s'", argv[1].str());
			printAscii(err, TRUE);
			// Ronin @feature 16/09/2026 DX9: suggest the closest names — ones containing the text first, then by edit distance.
			AsciiString needle = argv[1];
			needle.toLower();
			enum { MAX_SUGGEST = 4 };
			const ThingTemplate *best[MAX_SUGGEST] = {};
			Int bestScore[MAX_SUGGEST] = { 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF };
			for (const ThingTemplate *t = TheThingFactory->firstTemplate(); t != nullptr; t = t->friend_getNextTemplate())
			{
				AsciiString lower = t->getName();
				lower.toLower();
				const Int score = (strstr(lower.str(), needle.str()) != nullptr) ? 0 : nameDistance(needle.str(), lower.str());
				for (Int s = 0; s < MAX_SUGGEST; ++s)
				{
					if (score < bestScore[s])
					{
						for (Int k = MAX_SUGGEST - 1; k > s; --k)
						{
							best[k] = best[k - 1];
							bestScore[k] = bestScore[k - 1];
						}
						best[s] = t;
						bestScore[s] = score;
						break;
					}
				}
			}
			const Int limit = needle.getLength() / 3 + 2;	// further than this the "closest" name is noise
			for (Int s = 0; s < MAX_SUGGEST; ++s)
			{
				if (best[s] != nullptr && bestScore[s] <= limit)
				{
					AsciiString line;
					line.format("  did you mean %s", best[s]->getName().str());
					printAscii(line, FALSE);
				}
			}

			// Ronin @feature 16/09/2026 DX9: back into the box as typed, so the picker shows what the words match.
			if (entry != nullptr)
			{
				AsciiString line = argv[0];
				for (Int i = 1; i < argc; ++i)
				{
					line.concat(' ');
					line.concat(argv[i]);
				}
				UnicodeString u;
				u.translate(line);
				GadgetTextEntrySetText(entry, u);
			}
			return;
		}

		startPlacement(thing, (argc >= 3) ? atoi(argv[2].str()) : 1);
	}

	void runCommand(const UnicodeString &line)
	{
		AsciiString text;
		text.translate(line);
		text.trim();
		if (text.isEmpty())
			return;

		// History, oldest first; a repeat of the last command is not stored twice.
		if (s_historyCount == 0 || strcmp(s_history[s_historyCount - 1].str(), text.str()) != 0)
		{
			if (s_historyCount == HISTORY_SIZE)
			{
				for (Int i = 0; i < HISTORY_SIZE - 1; ++i)
					s_history[i] = s_history[i + 1];
				--s_historyCount;
			}
			s_history[s_historyCount++] = text;
		}
		s_historyPos = s_historyCount;

		UnicodeString echo;
		echo.format(L"> %ls", line.str());
		printLine(echo, FALSE);

		AsciiString argv[MAX_ARGS];
		Int argc = 0;
		AsciiString rest = text;
		AsciiString token;
		while (argc < MAX_ARGS && rest.nextToken(&token, " \t"))
			argv[argc++] = token;
		if (argc == 0)
			return;

		for (Int i = 0; i < s_commandCount; ++i)
		{
			if (stricmp(s_commands[i].name, argv[0].str()) == 0)
			{
				s_commands[i].func(argc, argv);
				return;
			}
		}
		AsciiString err;
		err.format("unknown command '%s' - type help", argv[0].str());
		printAscii(err, TRUE);
	}

	void recallHistory(GameWindow *entry, Int step)
	{
		if (s_historyCount == 0)
			return;
		s_historyPos += step;
		if (s_historyPos < 0)
			s_historyPos = 0;
		if (s_historyPos >= s_historyCount)
		{
			s_historyPos = s_historyCount;
			GadgetTextEntrySetText(entry, UnicodeString::TheEmptyString);
			return;
		}
		UnicodeString u;
		u.translate(s_history[s_historyPos]);
		GadgetTextEntrySetText(entry, u);
	}

	// Ronin @feature 16/09/2026 DX9: spawn picker (docs/Debug_Panel_Design.md step 7c2). Index into PICK_CATEGORIES, -1 for
	// objects it does not list (system, audio, test, debris, roads, waypoints).
	Int categoryOf(const ThingTemplate *t)
	{
		const EditorSortingType sorting = t->getEditorSorting();
		for (Int c = 0; c < PICK_CATEGORY_COUNT; ++c)
		{
			if (PICK_CATEGORIES[c].sorting == sorting)
				return c;
		}
		return -1;
	}

	// Ronin @feature 16/09/2026 DX9: the owning sides of the listed objects, sorted, collected once.
	void collectSides()
	{
		if (s_sideCount >= 0 || TheThingFactory == nullptr)
			return;
		s_sideCount = 0;
		for (const ThingTemplate *t = TheThingFactory->firstTemplate(); t != nullptr; t = t->friend_getNextTemplate())
		{
			const AsciiString &side = t->getDefaultOwningSide();
			if (categoryOf(t) < 0 || side.isEmpty())
				continue;
			Int at = 0;
			while (at < s_sideCount && stricmp(s_sides[at].str(), side.str()) < 0)
				++at;
			if (s_sideCount == MAX_SIDES || (at < s_sideCount && stricmp(s_sides[at].str(), side.str()) == 0))
				continue;
			for (Int k = s_sideCount; k > at; --k)
				s_sides[k] = s_sides[k - 1];
			s_sides[at] = side;
			++s_sideCount;
		}
	}

	void setLabel(DisplayString **label, const AsciiString &text)
	{
		if (*label == nullptr && TheDisplayStringManager != nullptr)
		{
			*label = TheDisplayStringManager->newDisplayString();
			if (*label != nullptr)
				(*label)->setFont(panelFont());
		}
		if (*label != nullptr)
		{
			UnicodeString u;
			u.translate(text);
			(*label)->setText(u);
		}
	}

	// Ronin @feature 16/09/2026 DX9: case-insensitive substring test; the needle is already lower case.
	Bool containsNoCase(const char *hay, const char *needle)
	{
		if (needle[0] == 0)
			return TRUE;
		for (; *hay != 0; ++hay)
		{
			Int i = 0;
			while (needle[i] != 0 && tolower((unsigned char)hay[i]) == needle[i])
				++i;
			if (needle[i] == 0)
				return TRUE;
		}
		return FALSE;
	}

	int compareTemplateNames(const void *a, const void *b)
	{
		const ThingTemplate *ta = *(const ThingTemplate *const *)a;
		const ThingTemplate *tb = *(const ThingTemplate *const *)b;
		return stricmp(ta->getName().str(), tb->getName().str());
	}

	// Ronin @feature 16/09/2026 DX9: box text "spawn <words>" -> TRUE and the words after "spawn ".
	Bool pickerFilter(const UnicodeString &text, AsciiString *filter)
	{
		AsciiString ascii;
		ascii.translate(text);
		const char *s = ascii.str();
		while (*s == ' ' || *s == '\t')
			++s;
		if (strnicmp(s, "spawn", 5) != 0 || (s[5] != ' ' && s[5] != '\t'))
			return FALSE;
		*filter = AsciiString(s + 6);
		return TRUE;
	}

	// Ronin @feature 16/09/2026 DX9: every word must appear in the name, side or category; a number is the count. Sorted by
	// name; past PICK_ROWS a last row says how many more. Each row keeps its template id + 1 as item data (0 = not a pick).
	void fillPicker(GameWindow *list, const AsciiString &filter)
	{
		GadgetListBoxReset(list);
		s_pickCount = 1;
		collectSides();

		AsciiString label;
		label.format("[Side: %s]", (s_sideIndex >= 0 && s_sideIndex < s_sideCount) ? s_sides[s_sideIndex].str() : "All");
		setLabel(&s_sideLabel, label);
		label.format("[Category: %s]", (s_categoryIndex >= 0) ? PICK_CATEGORIES[s_categoryIndex].name : "All");
		setLabel(&s_categoryLabel, label);

		const char *refusal = logicRefusal();
		if (refusal != nullptr || TheThingFactory == nullptr)
		{
			UnicodeString u;
			u.translate(AsciiString((refusal != nullptr) ? refusal : "no objects loaded"));
			GadgetListBoxAddEntryText(list, u, GameMakeColor(255, 120, 80, 255), -1, 0);
			return;
		}

		AsciiString words[MAX_ARGS];
		Int wordCount = 0;
		AsciiString rest = filter;
		AsciiString token;
		while (wordCount < MAX_ARGS && rest.nextToken(&token, " \t"))
		{
			if (token.str()[0] >= '0' && token.str()[0] <= '9')
			{
				s_pickCount = atoi(token.str());
				continue;
			}
			token.toLower();
			words[wordCount++] = token;
		}

		enum { MAX_MATCHES = 8192 };
		static const ThingTemplate *matches[MAX_MATCHES];
		Int matchCount = 0;
		for (const ThingTemplate *t = TheThingFactory->firstTemplate(); t != nullptr; t = t->friend_getNextTemplate())
		{
			const Int category = categoryOf(t);
			if (category < 0 || (s_categoryIndex >= 0 && category != s_categoryIndex))
				continue;
			const char *side = t->getDefaultOwningSide().str();
			if (s_sideIndex >= 0 && s_sideIndex < s_sideCount && stricmp(side, s_sides[s_sideIndex].str()) != 0)
				continue;
			Bool match = TRUE;
			for (Int w = 0; w < wordCount && match; ++w)
			{
				match = containsNoCase(t->getName().str(), words[w].str()) || containsNoCase(side, words[w].str()) ||
						containsNoCase(PICK_CATEGORIES[category].name, words[w].str());
			}
			if (match && matchCount < MAX_MATCHES)
				matches[matchCount++] = t;
		}
		qsort(matches, matchCount, sizeof(matches[0]), compareTemplateNames);

		const Int shown = (matchCount < PICK_ROWS) ? matchCount : PICK_ROWS;
		for (Int i = 0; i < shown; ++i)
		{
			const PickCategory &category = PICK_CATEGORIES[categoryOf(matches[i])];
			AsciiString line;
			line.format("%-38s %-10s %s", matches[i]->getName().str(), category.name, matches[i]->getDefaultOwningSide().str());
			UnicodeString u;
			u.translate(line);
			const Int row = GadgetListBoxAddEntryText(list, u, GameMakeColor(category.r, category.g, category.b, 255), -1, 0);
			if (row >= 0)
				GadgetListBoxSetItemData(list, (void *)(intptr_t)(matches[i]->getTemplateID() + 1), row, 0);
		}
		if (matchCount == 0 || matchCount > shown)
		{
			AsciiString line;
			if (matchCount == 0)
				line = "no match";
			else
				line.format("... %d more - keep typing to narrow it down", matchCount - shown);
			UnicodeString u;
			u.translate(line);
			GadgetListBoxAddEntryText(list, u, GameMakeColor(150, 150, 150, 255), -1, 0);
		}
	}

	// Ronin @feature 16/09/2026 DX9: the row under a screen point, measured where W3DGadgetListBoxDraw draws rows (4 px down).
	// The stock click test skips those 4 px, so the hover bar and the click both use this.
	Int listRowAt(GameWindow *list, Int screenX, Int screenY)
	{
		ListboxData *data = (ListboxData *)list->winGetUserData();
		if (data == nullptr || list->winIsHidden())
			return -1;
		Int x = 0, y = 0, w = 0, h = 0;
		list->winGetScreenPosition(&x, &y);
		list->winGetSize(&w, &h);
		if (screenX <= x || screenX >= x + w - 1 || screenY <= y || screenY >= y + h - 1)
			return -1;
		const Int local = screenY - (y + 4) + data->displayPos;
		if (local < 0)
			return -1;
		for (Int i = 0; i < data->endPos; ++i)
		{
			if (data->listData[i].listHeight > local)
				return i;
		}
		return -1;
	}

	// Ronin @feature 16/09/2026 DX9: the stock list input takes keyboard focus and picks with the offset test, so only the
	// wheel goes to it. A click that started on the list picks the row; update() spawns it next frame.
	WindowMsgHandledType listInput(GameWindow *window, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2)
	{
		switch (msg)
		{
			case GWM_WHEEL_UP:
			case GWM_WHEEL_DOWN:
				return GadgetListBoxInput(window, msg, mData1, mData2);

			case GWM_LEFT_DOWN:
				s_listPressed = TRUE;
				return MSG_HANDLED;

			case GWM_LEFT_UP:
			{
				if (s_listPressed)
				{
					const Int row = listRowAt(window, (Int)(mData1 & 0xFFFF), (Int)((mData1 >> 16) & 0xFFFF));
					const void *data = (row >= 0) ? GadgetListBoxGetItemData(window, row, 0) : nullptr;
					if (data != nullptr)
						s_pickedID = (Int)(intptr_t)data - 1;
				}
				s_listPressed = FALSE;
				// Ignored, so a drag-select from the battlefield released here still reaches panelInput, which ends it.
				return MSG_IGNORED;
			}

			case GWM_CHAR:
			case GWM_MOUSE_POS:
				return MSG_IGNORED;
		}
		return MSG_HANDLED;
	}

	// Ronin @feature 16/09/2026 DX9: the stock colour draw, plus a bar on the row under the mouse (its selection bar is image-only).
	void listDraw(GameWindow *window, WinInstanceData *instData)
	{
		GameWinDrawFunc draw = TheWindowManager->getListBoxDrawFunc();
		if (draw != nullptr)
			draw(window, instData);
		if (TheMouse == nullptr)
			return;
		const MouseIO *mouse = TheMouse->getMouseStatus();
		const Int row = listRowAt(window, mouse->pos.x, mouse->pos.y);
		if (row < 0 || GadgetListBoxGetItemData(window, row, 0) == nullptr)
			return;
		ListboxData *data = (ListboxData *)window->winGetUserData();
		Int x = 0, y = 0, w = 0, h = 0;
		window->winGetScreenPosition(&x, &y);
		window->winGetSize(&w, &h);
		Int top    = y + 4 - data->displayPos + ((row > 0) ? data->listData[row - 1].listHeight : 0);
		Int bottom = top + data->listData[row].height + 1;
		if (top < y + 1)
			top = y + 1;
		if (bottom > y + h - 1)
			bottom = y + h - 1;
		TheWindowManager->winFillRect(GameMakeColor(255, 255, 255, 45), 1.0f, x + 1, top, x + w - 1, bottom);
	}

	GameWindow *createList(GameWindow *panel)
	{
		WinInstanceData inst;
		inst.init();
		inst.m_style = GWS_SCROLL_LISTBOX;

		ListboxData data;
		memset(&data, 0, sizeof(data));
		data.listLength = PICK_ROWS + 1;		// + the "N more" row
		data.columns    = 1;
		data.scrollBar  = FALSE;				// its arrows and thumb are image-only; the wheel scrolls

		GameWindow *list = TheWindowManager->gogoGadgetListBox(panel, WIN_STATUS_ENABLED | WIN_STATUS_ONE_LINE,
			PAD, 0, PICKER_WIDTH, PICKER_HEIGHT, &inst, &data, panelFont(), FALSE);
		if (list == nullptr)
			return nullptr;

		list->winSetWindowId(LIST_WINDOW_ID);
		list->winSetInputFunc(listInput);
		list->winSetDrawFunc(listDraw);
		list->winHide(TRUE);

		const Color back   = GameMakeColor(0, 0, 0, 110);
		const Color border = GameMakeColor(160, 160, 160, 170);
		const Color none   = WIN_COLOR_UNDEFINED;
		GadgetListBoxSetColors(list, back, border, none, none, back, border, none, none, back, border, none, none);
		return list;
	}

	// Ronin @feature 16/09/2026 DX9: -1 is All; stepping wraps through it.
	Int wrapIndex(Int index, Int step, Int count)
	{
		index += step;
		if (index >= count)
			index = -1;
		if (index < -1)
			index = count - 1;
		return index;
	}

	// Ronin @feature 16/09/2026 DX9: a click on the Side or Category label steps it (left next, right back); update() refills.
	void cycleLabelAt(GameWindow *panel, Int screenX, Int screenY, Int step)
	{
		Int px = 0, py = 0;
		panel->winGetScreenPosition(&px, &py);
		const Int lx = screenX - px;
		const Int ly = screenY - py;
		if (!s_pickerOpen || ly < s_labelTop || ly >= s_labelTop + LABEL_HEIGHT)
			return;
		if (lx >= PAD && lx < PAD + s_sideLabelW)
			s_sideIndex = wrapIndex(s_sideIndex, step, (s_sideCount > 0) ? s_sideCount : 0);
		else if (lx >= s_categoryLabelX && lx < s_categoryLabelX + s_categoryLabelW)
			s_categoryIndex = wrapIndex(s_categoryIndex, step, PICK_CATEGORY_COUNT);
		else
			return;
		s_pickerDirty = TRUE;
	}

	// Ronin @feature 14/09/2026 DX9: a wall for mouse input, so the panel drags and the battlefield under it is not clicked.
	// Same as GameWinBlockInput, which touches the selection translator, tactical view and InGameUI without null checks.
	WindowMsgHandledType panelInput(GameWindow *window, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2)
	{
		if (msg == GWM_CHAR || msg == GWM_MOUSE_POS)
			return MSG_IGNORED;
		if (msg == GWM_WHEEL_UP || msg == GWM_WHEEL_DOWN)		// Ronin @feature 21/10/2026 DX9: the output's scrollback
		{
			scrollOutput((msg == GWM_WHEEL_UP) ? 3 : -3);
			return MSG_HANDLED;
		}

		// Ronin @feature 16/09/2026 DX9: picker labels. A press that moved is a drag of the panel, not a click.
		const Int mouseX = (Int)(mData1 & 0xFFFF);
		const Int mouseY = (Int)((mData1 >> 16) & 0xFFFF);
		if (msg == GWM_LEFT_DOWN)
		{
			s_pressX = mouseX;
			s_pressY = mouseY;
		}
		else if (msg == GWM_LEFT_UP && abs(mouseX - s_pressX) <= 2 && abs(mouseY - s_pressY) <= 2)
		{
			cycleLabelAt(window, mouseX, mouseY, 1);
		}
		else if (msg == GWM_RIGHT_UP)
		{
			cycleLabelAt(window, mouseX, mouseY, -1);
		}

		if (msg == GWM_LEFT_UP)
		{
			// Stop a drag-select that was released over the panel.
			if (TheSelectionTranslator != nullptr)
			{
				TheSelectionTranslator->setLeftMouseButton(FALSE);
				TheSelectionTranslator->setDragSelecting(FALSE);
			}
			if (TheTacticalView != nullptr)
				TheTacticalView->setMouseLock(FALSE);
			if (TheInGameUI != nullptr)
			{
				TheInGameUI->setSelecting(FALSE);
				TheInGameUI->endAreaSelectHint(nullptr);
			}
		}
		return MSG_HANDLED;
	}

	// Ronin @feature 14/09/2026 DX9: Enter in the command box — the entry sends GEM_EDIT_DONE to its owner, this panel.
	WindowMsgHandledType panelSystem(GameWindow *window, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2)
	{
		if (msg != GEM_EDIT_DONE)
			return MSG_IGNORED;

		GameWindow *entry = (GameWindow *)mData1;
		if (entry != nullptr)
		{
			const UnicodeString line = GadgetTextEntryGetText(entry);
			GadgetTextEntrySetText(entry, UnicodeString::TheEmptyString);
			runCommand(line);
		}
		return MSG_HANDLED;
	}

	// Ronin @feature 14/09/2026 DX9: the stock entry input, minus keys that would leak: ESC reached the game menu, arrows and
	// Tab moved focus to menu buttons. Up/Down recall history; ESC clears on press and hands the keys back on release.
	WindowMsgHandledType entryInput(GameWindow *window, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2)
	{
		if (msg == GWM_CHAR)
		{
			const Bool down = BitIsSet(mData2, KEY_STATE_DOWN);
			switch (mData1)
			{
				case KEY_ESC:
					if (down)
					{
						GadgetTextEntrySetText(window, UnicodeString::TheEmptyString);
						s_historyPos = s_historyCount;
					}
					else
					{
						TheWindowManager->winSetFocus(nullptr);
					}
					return MSG_HANDLED;

				case KEY_UP:
				case KEY_DOWN:
					if (down)
						recallHistory(window, (mData1 == KEY_UP) ? -1 : 1);
					return MSG_HANDLED;

				case KEY_PGUP:		// Ronin @feature 21/10/2026 DX9: the output's scrollback, a page less two lines at a time
				case KEY_PGDN:
					if (down)
						scrollOutput((mData1 == KEY_PGUP) ? (OUTPUT_LINES - 2) : -(OUTPUT_LINES - 2));
					return MSG_HANDLED;

				case KEY_TAB:
				case KEY_LEFT:
				case KEY_RIGHT:
					return MSG_HANDLED;
			}
		}
		return GadgetTextEntryInput(window, msg, mData1, mData2);
	}

	GameWindow *createEntry(GameWindow *panel)
	{
		WinInstanceData inst;
		inst.init();
		inst.m_style = GWS_ENTRY_FIELD;

		EntryData data;
		memset(&data, 0, sizeof(data));
		data.maxTextLen = ENTRY_TEXT_LEN;
		data.aSCIIOnly  = TRUE;

		GameFont *font = panelFont();
		GameWindow *entry = TheWindowManager->gogoGadgetTextEntry(panel, WIN_STATUS_ENABLED,
			PAD, TITLE_HEIGHT + PAD, ENTRY_MIN_WIDTH, ENTRY_HEIGHT, &inst, &data, font, FALSE);
		if (entry == nullptr)
			return nullptr;

		entry->winSetWindowId(ENTRY_WINDOW_ID);
		entry->winSetInputFunc(entryInput);
		if (font != nullptr)
			GadgetTextEntrySetFont(entry, font);

		// Translucent like the panel; a brighter border while it has focus (focus sets the hilite state).
		const Color back   = GameMakeColor(0, 0, 0, 110);
		const Color border = GameMakeColor(160, 160, 160, 170);
		const Color drop   = GameMakeColor(0, 0, 0, 255);
		const Color white  = GameMakeColor(255, 255, 255, 255);
		GadgetTextEntrySetEnabledColor(entry, back);
		GadgetTextEntrySetEnabledBorderColor(entry, border);
		GadgetTextEntrySetHiliteColor(entry, back);
		GadgetTextEntrySetHiliteBorderColor(entry, GameMakeColor(230, 230, 230, 230));
		GadgetTextEntrySetDisabledColor(entry, back);
		GadgetTextEntrySetDisabledBorderColor(entry, border);
		entry->winSetEnabledTextColors(white, drop);
		entry->winSetHiliteTextColors(white, drop);
		entry->winSetDisabledTextColors(GameMakeColor(140, 140, 140, 255), drop);
		entry->winSetIMECompositeTextColors(white, drop);
		return entry;
	}

	// Ronin @feature 28/09/2026 DX9: every frame, before update() can return early - see s_drawPrev.
	void sampleReadouts()
	{
		s_drawTotal = 0;
		for (Int i = 0; i < Debug_Statistics::DRAW_SUBSYS_COUNT; ++i)
		{
			const unsigned now = Debug_Statistics::Get_Total_Draw_Calls_By_Subsystem(i);
			s_drawNow[i]  = now - s_drawPrev[i];	// unsigned wrap is fine: monotonic counters
			s_drawPrev[i] = now;
			s_drawTotal  += s_drawNow[i];
		}

		// Ronin @diagnostic 09/09/2026 DX9: §29i.3 step 2. [PERF] run mean: single frames drift with scene activity all
		// session, so a cascade A/B needs means over a tour, not one screenshot.
		if (s_perfFreq == 0)
			QueryPerformanceFrequency((LARGE_INTEGER *)&s_perfFreq);
		QueryPerformanceCounter((LARGE_INTEGER *)&s_perfNow);
		if (TheGameLogic != nullptr && TheGameLogic->getFrame() > 60)
		{
			if (s_perfFirst == 0)
			{
				s_perfFirst = s_perfNow;		// the first tracked frame has nothing to time against
			}
			else
			{
				const Real ms = (Real)((double)(s_perfNow - s_perfLast) * 1000.0 / (double)s_perfFreq);
				if (ms > s_perfWorstMs) s_perfWorstMs = ms;
				s_perfSMSum += (double)s_drawNow[Debug_Statistics::DRAW_SUBSYS_SHADOWMAP];
				++s_perfFrames;
			}
			s_perfLast = s_perfNow;
		}

		// Ronin @diagnostic 28/09/2026 DX9: [WATER] - keep this frame's water draws, zero them for the next. W3DWater.cpp counts
		// only while the row is on and the panel shown, so `rows water 0` keeps the check out of a [PERF] A/B.
		s_waterNow = TheWaterStats;
		TheWaterStats.flatDraws    = 0;
		TheWaterStats.flatPSDraws  = 0;
		TheWaterStats.riverDraws   = 0;
		TheWaterStats.riverPSDraws = 0;
		TheWaterStats.meshDraws    = 0;
		TheWaterStats.meshPSDraws  = 0;
		TheWaterStats.seaDraws     = 0;
		TheWaterStats.seaPSDraws   = 0;
		TheWaterStats.flatVerts    = 0;	// Ronin @diagnostic 29/09/2026 DX9: phase 2's grid
		TheWaterStats.flatCell     = 0.0f;	// Ronin @bugfix 03/10/2026 DX9: its real cell in view
		TheWaterStats.seaPolys     = 0;	// Ronin @diagnostic 01/10/2026 DX9: sea / lake
		TheWaterStats.lakePolys    = 0;
		TheWaterStats.wakeTrails   = 0;	// Ronin @feature 03/10/2026 DX9: phase 5 - wakes
		TheWaterStats.wakeVerts    = 0;
		TheWaterStats.wakeRipples  = 0;
		TheWaterStats.wakeTexel    = 0.0f;
		TheWaterStats.enabled      = (s_rowOn[R3DDebugPanel::ROW_WATER] && s_visible) ? TRUE : FALSE;
	}

	// Ronin @feature 28/09/2026 DX9: sets the row's text and hands it to the panel for this frame.
	void putRow(R3DDebugPanel::Row row, const UnicodeString &text, Color color)
	{
		if (s_rowText[row] == nullptr && TheDisplayStringManager != nullptr && TheFontLibrary != nullptr)
		{
			s_rowText[row] = TheDisplayStringManager->newDisplayString();
			if (s_rowText[row] != nullptr)
				s_rowText[row]->setFont(panelFont());
		}
		if (s_rowText[row] == nullptr)
			return;
		s_rowText[row]->setText(text);
		R3DDebugPanel::setRow(row, s_rowText[row], color);
	}

	// Ronin @diagnostic 28/09/2026 DX9: [WATER] - a water shader's last creation attempt as text.
	void formatWaterShader(UnicodeString &out, Int stage, HRESULT hr)
	{
		switch (stage)
		{
			case WaterDebugStats::PS_OK:            out = L"ok"; break;
			case WaterDebugStats::PS_ASM_FAILED:    out.format(L"FAIL(asm %08X)", (UnsignedInt)hr); break;
			case WaterDebugStats::PS_CREATE_FAILED: out.format(L"FAIL(create %08X)", (UnsignedInt)hr); break;
			default:                                out = L"not-tried"; break;
		}
	}

	// Ronin @feature 28/09/2026 DX9: every row, each behind its `rows` switch. The first seven lived in W3DDisplay.cpp.
	void renderReadouts()
	{
		UnicodeString text;

		// Ronin @diagnostic 21/06/2026 DX9: single-rigid records per frame; draws/frame equals [INST] recs.
		if (s_rowOn[R3DDebugPanel::ROW_SR])
		{
			const unsigned draws   = DX8_Get_Single_Rigid_Last_Frame_Draw_Count();
			const unsigned flushes = DX8_Get_Single_Rigid_Last_Frame_Flush_Count();
			text.format(L"[SR] draws/frame=%u  flushes=%u  avgBatch=%.1f",
				draws, flushes, (flushes > 0) ? ((float)draws / (float)flushes) : 0.0f);
			putRow(R3DDebugPanel::ROW_SR, text, GameMakeColor(255, 255, 0, 255));
		}

		if (s_rowOn[R3DDebugPanel::ROW_INST])
		{
			const unsigned runs       = TheDX8InstanceManager.Get_Last_Frame_Instanced_Draw_Calls();
			const unsigned instMeshes = TheDX8InstanceManager.Get_Last_Frame_Instanced_Meshes();
			text.format(L"[INST] recs=%u  runs=%u  inst=%u  indiv=%u  nrm=%u/%u  lbrk=%u  refl=%u  flushes=%u  avgRun=%.1f",
				TheDX8InstanceManager.Get_Last_Frame_Instanced_Records(), runs, instMeshes,
				TheDX8InstanceManager.Get_Last_Frame_Instanced_Individual_Draws(),
				TheDX8InstanceManager.Get_Last_Frame_Instanced_Normal_Mapped_Merged(),
				TheDX8InstanceManager.Get_Last_Frame_Instanced_Normal_Mapped(),
				TheDX8InstanceManager.Get_Last_Frame_Instanced_Light_Breaks(),
				TheDX8InstanceManager.Get_Last_Frame_Reflective_Draws(),
				TheDX8InstanceManager.Get_Last_Frame_Instanced_Flushes(),
				(runs > 0) ? ((float)instMeshes / (float)runs) : 0.0f);
			putRow(R3DDebugPanel::ROW_INST, text, GameMakeColor(0, 255, 255, 255));
		}

		// Ronin @diagnostic 02/08/2026 DX9: where the frame's draw calls come from; total includes the programmable rigid path.
		// sortAdd = draws batchable outside the depth sort (Windowednew.md §19b.1); [DRAW2] splits `other` (§19b.2).
		if (s_rowOn[R3DDebugPanel::ROW_DRAW])
		{
			text.format(L"[DRAW] total=%u  terr=%u  shroud=%u  sortAdd=%u  sortAlpha=%u  sortOth=%u  shadow=%u  rigid=%u  water=%u  skin=%u",
				s_drawTotal,
				s_drawNow[Debug_Statistics::DRAW_SUBSYS_TERRAIN],
				s_drawNow[Debug_Statistics::DRAW_SUBSYS_SHROUD],
				s_drawNow[Debug_Statistics::DRAW_SUBSYS_SORTED_ADD],
				s_drawNow[Debug_Statistics::DRAW_SUBSYS_SORTED_ALPHA],
				s_drawNow[Debug_Statistics::DRAW_SUBSYS_SORTED],
				s_drawNow[Debug_Statistics::DRAW_SUBSYS_SHADOW],
				s_drawNow[Debug_Statistics::DRAW_SUBSYS_RIGID_PROG],
				s_drawNow[Debug_Statistics::DRAW_SUBSYS_WATER],
				s_drawNow[Debug_Statistics::DRAW_SUBSYS_SKIN]);
			putRow(R3DDebugPanel::ROW_DRAW, text, GameMakeColor(255, 160, 0, 255));
		}
		if (s_rowOn[R3DDebugPanel::ROW_DRAW2])
		{
			text.format(L"[DRAW2] rigidFFP=%u  matpass=%u  fxLine=%u  fxPoint=%u  ui2D=%u  other=%u  shadowMap=%u  shadowRecv=%u  prepass=%u",
				s_drawNow[Debug_Statistics::DRAW_SUBSYS_RIGID_FFP],
				s_drawNow[Debug_Statistics::DRAW_SUBSYS_MATPASS],
				s_drawNow[Debug_Statistics::DRAW_SUBSYS_FX_LINE],
				s_drawNow[Debug_Statistics::DRAW_SUBSYS_FX_POINT],
				s_drawNow[Debug_Statistics::DRAW_SUBSYS_UI2D],
				s_drawNow[Debug_Statistics::DRAW_SUBSYS_OTHER],
				s_drawNow[Debug_Statistics::DRAW_SUBSYS_SHADOWMAP],
				s_drawNow[Debug_Statistics::DRAW_SUBSYS_SHADOWRECV],
				s_drawNow[Debug_Statistics::DRAW_SUBSYS_PREPASS]);
			putRow(R3DDebugPanel::ROW_DRAW2, text, GameMakeColor(255, 160, 0, 255));
		}

		// Ronin @diagnostic 06/09/2026 DX9: §29j.13k. Shadow fit - texel moves with zoom, yaw and sun. No maps, no row.
		if (s_rowOn[R3DDebugPanel::ROW_SHADOW] && TheUseShadowMaps)
		{
			// Ronin @diagnostic 09/09/2026 DX9: §29i.3 step 2. PEAK = the worst fit seen while moving; the near split's win is
			// one quantiser step wide. Starts over on a tier change (`shadows <n>`) or a new match.
			static Real        s_reqPeak     = 0.0f;
			static Real        s_extPeak     = 0.0f;
			static Int         s_peakQuality = -1;
			static UnsignedInt s_peakFrame   = 0;
			const UnsignedInt  logicFrame    = (TheGameLogic != nullptr) ? TheGameLogic->getFrame() : 0;
			if (R3DShadowMap::getQuality() != s_peakQuality || logicFrame < s_peakFrame)
			{
				s_peakQuality = R3DShadowMap::getQuality();
				s_reqPeak     = 0.0f;
				s_extPeak     = 0.0f;
			}
			s_peakFrame = logicFrame;
			if (logicFrame > 60)
			{
				const Real reqNow = R3DShadowMap::getFitRequired();
				const Real exX    = R3DShadowMap::getFitExtentX();
				const Real exY    = R3DShadowMap::getFitExtentY();
				const Real extNow = (exX > exY) ? exX : exY;
				if (reqNow > s_reqPeak) s_reqPeak = reqNow;
				if (extNow > s_extPeak) s_extPeak = extNow;
			}
			// Ronin @diagnostic 13/09/2026 DX9: texel is NEAR/FAR, both cascades live at once; at one split far reads 0.00.
			text.format(L"[SHADOW] texel=%.2f/%.2f ext=%.0fx%.0f req=%.0f box=%.0fx%.0f bare=%.0fx%.0f hdrm=%.0f  PEAK req=%.0f ext=%.0f",
				R3DShadowMap::getTexelWorldSize(0),
				(R3DShadowMap::getSplitCount() > 1) ? R3DShadowMap::getTexelWorldSize(1) : 0.0f,
				R3DShadowMap::getFitExtentX(),
				R3DShadowMap::getFitExtentY(),
				R3DShadowMap::getFitRequired(),
				R3DShadowMap::getFitBoxX(),
				R3DShadowMap::getFitBoxY(),
				R3DShadowMap::getFitBareBoxX(),
				R3DShadowMap::getFitBareBoxY(),
				R3DShadowMap::getHeadroom(),
				s_reqPeak,
				s_extPeak);
			putRow(R3DDebugPanel::ROW_SHADOW, text, GameMakeColor(120, 200, 255, 255));
		}

		if (s_rowOn[R3DDebugPanel::ROW_PERF])
		{
			const double secs    = (s_perfFrames > 0) ? ((double)(s_perfNow - s_perfFirst) / (double)s_perfFreq) : 0.0;
			const Real   meanFps = (secs > 0.0) ? (Real)((double)s_perfFrames / secs) : 0.0f;
			const Real   meanMs  = (s_perfFrames > 0) ? (Real)(secs * 1000.0 / (double)s_perfFrames) : 0.0f;
			const Real   meanSM  = (s_perfFrames > 0) ? (Real)(s_perfSMSum / (double)s_perfFrames) : 0.0f;
			text.format(L"[PERF] frames=%u  avgFps=%.1f  avgMs=%.3f  worstMs=%.1f  avgShadowMap=%.0f",
				s_perfFrames, meanFps, meanMs, s_perfWorstMs, meanSM);
			putRow(R3DDebugPanel::ROW_PERF, text, GameMakeColor(255, 230, 120, 255));
		}

		// Ronin @diagnostic 13/09/2026 DX9: can the card read scene depth, and did this frame use it. aoRes/samples prove a
		// quality change took effect - the tiers differ subtly by eye.
		if (s_rowOn[R3DDebugPanel::ROW_DEPTH])
		{
			Int aoW = 0, aoH = 0;
			R3DSsao::getTargetSize(&aoW, &aoH);
			// Ronin @diagnostic 07/10/2026 DX9: bound = samples of the colour target / the depth the 3D views left on the device,
			// -1 = none. Under MSAA 4x both must read 4: a single-sampled or missing depth lets the trees' second target bind.
			Int rtSamples = -1, dsSamples = -1;
			IDirect3DDevice9 *boundDev = DX8Wrapper::_Get_D3D_Device8();
			if (boundDev != nullptr)
			{
				IDirect3DSurface9 *surf = nullptr;
				D3DSURFACE_DESC sd;
				if (SUCCEEDED(boundDev->GetRenderTarget(0, &surf)) && surf != nullptr)
				{
					if (SUCCEEDED(surf->GetDesc(&sd)))
						rtSamples = (Int)sd.MultiSampleType;
					surf->Release();
				}
				surf = nullptr;
				if (SUCCEEDED(boundDev->GetDepthStencilSurface(&surf)) && surf != nullptr)
				{
					if (SUCCEEDED(surf->GetDesc(&sd)))
						dsSamples = (Int)sd.MultiSampleType;
					surf->Release();
				}
			}
			// Ronin @diagnostic 06/10/2026 DX9: prepass = this frame's depth came from the MSAA prepass: 1 NULL target, 2 ARGB.
			text.format(L"[DEPTH] ssao=%d  intz=%d  active=%d  aa=%d  prepass=%d  bound=%d/%d  aoRes=%dx%d  samples=%d",
				R3DSsao::getQuality(),
				R3DSsao::isSupported() ? 1 : 0,
				R3DSsao::isActive() ? 1 : 0,
				DX8Wrapper::Get_Anti_Aliasing_Level(),
				R3DSsao::getPrepassTarget(),
				rtSamples, dsSamples,
				aoW, aoH,
				R3DSsao::getSampleCount());
			putRow(R3DDebugPanel::ROW_DEPTH, text, GameMakeColor(140, 255, 140, 255));
		}

		// Ronin @diagnostic 14/09/2026 DX9: §19e.3. Splat bake under channel reuse; droppedCells must be 0.
		if (s_rowOn[R3DDebugPanel::ROW_TERRAIN] && TheTerrainRenderObject != nullptr && TheTerrainRenderObject->getMap() != nullptr)
		{
			WorldHeightMap *map = TheTerrainRenderObject->getMap();
			text.format(L"[TERRAIN] materials=%d  channels=%d  droppedCells=%d  pages=%d  maxPerTile=%d",
				map->getSplatWeightableClasses(),
				map->getActiveMaterialCount(),
				map->getSplatDroppedCells(),
				map->getTerrainTexturePageCount(),
				map->getSplatMaxPerTile());
			putRow(R3DDebugPanel::ROW_TERRAIN, text, GameMakeColor(255, 255, 255, 255));
		}

		// Ronin @diagnostic 15/09/2026 DX9: [UI] - who owns input right now, for "map clicks stop until the options menu opens".
		// Panel window ids: 7DEB0001 panel, 7DEB0002 command box, 7DEB0003 spawn picker list.
		if (s_rowOn[R3DDebugPanel::ROW_UI])
		{
			GameWindow *focus   = TheWindowManager->winGetFocus();
			GameWindow *grab    = TheWindowManager->winGetGrabWindow();
			GameWindow *capture = TheWindowManager->winGetCapture();
			// Ronin @diagnostic 16/09/2026 DX9: the three things that can swallow a left click before the selection code acts
			// on it - `quit` destroys it outright (SelectionXlat.cpp onMouseLeftClick), `place` takes it at priority 30 and
			// `gui` at 40. A raw down/up trace cannot show this: raw clicks always run to the end of the stream.
			text.format(L"[UI] input=%d selecting=%d mouseLock=%d quit=%d place=%d gui=%d focus=%08X grab=%08X capture=%08X",
				(TheInGameUI != nullptr && TheInGameUI->getInputEnabled()) ? 1 : 0,
				(TheInGameUI != nullptr && TheInGameUI->isSelecting()) ? 1 : 0,
				(TheTacticalView != nullptr && TheTacticalView->isMouseLocked()) ? 1 : 0,
				(TheInGameUI != nullptr && TheInGameUI->isQuitMenuVisible()) ? 1 : 0,
				(TheInGameUI != nullptr && TheInGameUI->getPendingPlaceType() != nullptr) ? 1 : 0,
				(TheInGameUI != nullptr && TheInGameUI->getGUICommand() != nullptr) ? 1 : 0,
				focus   ? (UnsignedInt)focus->winGetWindowId()   : 0u,
				grab    ? (UnsignedInt)grab->winGetWindowId()    : 0u,
				capture ? (UnsignedInt)capture->winGetWindowId() : 0u);
			putRow(R3DDebugPanel::ROW_UI, text, GameMakeColor(200, 170, 255, 255));
		}

		// Ronin @diagnostic 20/09/2026 DX9: [RSTATE] - what the bypassed render-state cache costs, off by default. rs is every
		// Set_DX8_Render_State reaching the device (dx8wrapper.h:1334); rs/draw says whether fixing it is worth the risk.
		// docs/RenderStateCache.md.
		if (s_rowOn[R3DDebugPanel::ROW_RSTATE])
		{
			const DX8FrameStatistics &fs = DX8Wrapper::Get_Last_Frame_Statistics();
			const Real perDraw = (fs.draw_calls > 0) ? ((Real)fs.render_state_changes / (Real)fs.draw_calls) : 0.0f;
			text.format(L"[RSTATE] rs=%u  rs/draw=%.1f  tss=%u  tex=%u  mat=%u  vb=%u  draw=%u  dx8=%u",
				fs.render_state_changes, perDraw, fs.texture_stage_state_changes, fs.texture_changes,
				fs.material_changes, fs.vertex_buffer_changes, fs.draw_calls, fs.dx8_calls);
			putRow(R3DDebugPanel::ROW_RSTATE, text, GameMakeColor(120, 220, 255, 255));
		}

		// Ronin @diagnostic 20/09/2026 DX9: [TAA] - live state while TAA is on. docs/Debug_Panel_Design.md §7 explains each field.
		if (s_rowOn[R3DDebugPanel::ROW_TAA] && R3DTaa::isEnabled())
		{
			// moving = meshes given motion vectors / meshes drawn; untracked should stay 0.
			UnicodeString moving;
			if (R3DTaa::getVelocity())
				moving.format(L"%d/%d", R3DTaa::getMoverCount(), R3DTaa::getMeshSeen());
			else
				moving = L"off";
			// Ronin @diagnostic 27/09/2026 DX9: gpu = TAA's whole GPU time per frame, mask = the moving-shadow pass inside it, with
			// how many meshes it drew (off = `taa shadowmask 0`, - = nothing moved or no shadow maps).
			UnicodeString gpu;
			float gpuTotal = -1.0f, gpuMask = -1.0f;
			R3DTaa::getGpuMs(&gpuTotal, &gpuMask);
			const Int maskDraws = R3DTaa::getShadowMaskDraws();
			if (gpuTotal < 0.0f)
				gpu = L"n/a";
			else if (!R3DTaa::getShadowMask())
				gpu.format(L"%.2fms  mask=off", gpuTotal);
			else if (maskDraws < 0)
				gpu.format(L"%.2fms  mask=-", gpuTotal);
			else
				gpu.format(L"%.2fms  mask=%.2fms/%d", gpuTotal, gpuMask, maskDraws);
			text.format(L"[TAA] state=%s  weight=%.2f  moving=%s  reactive=%d  skinned=%d  fx-mask=%s  untracked=%d  view=%d  gpu=%s",
				R3DTaa::isActive() ? L"running" : L"off(MSAA)", R3DTaa::getWeight(),
				moving.str(),
				R3DTaa::getReactiveCount(), R3DTaa::getSkinDrawCount(), R3DTaa::getOpaqueOK() ? L"on" : L"off",
				R3DTaa::getTrackMisses(),
				R3DTaa::getDebug(), gpu.str());
			putRow(R3DDebugPanel::ROW_TAA, text, GameMakeColor(255, 200, 120, 255));
		}

		// Ronin @diagnostic 28/09/2026 DX9: [WATER] - Water_Work.md §5 step 1. flat/river/mesh = draws / draws with the water
		// shader bound on the device; N/0 = the fixed-function fallback. other = the rest of [DRAW] water (sky, shore waves).
		if (s_rowOn[R3DDebugPanel::ROW_WATER] && TheWaterRenderObj != nullptr)
		{
			UnicodeString flatPS, riverPS;
			formatWaterShader(flatPS, TheWaterStats.flatPSStage, TheWaterStats.flatPSHr);
			formatWaterShader(riverPS, TheWaterStats.riverPSStage, TheWaterStats.riverPSHr);
			const unsigned all   = s_drawNow[Debug_Statistics::DRAW_SUBSYS_WATER];
			const unsigned known = s_waterNow.flatDraws + s_waterNow.riverDraws + s_waterNow.meshDraws;
			// the soft shore edge, the same three terms as W3DWater.cpp drawTrapezoidWater
			const Bool edge = DX8Wrapper::getBackBufferFormat() == WW3D_FORMAT_A8R8G8B8 && TheGlobalData->m_showSoftWaterEdge &&
				TheWaterTransparency->m_transparentWaterDepth != 0;
			// Ronin @feature 28/09/2026 DX9: WaterType 2 - the ported sea shaders, the caust frames built, the sea's patch draws.
			if (TheGlobalData->m_waterType == 2)
			{
				UnicodeString seaVS, seaPS;
				formatWaterShader(seaVS, TheWaterStats.seaVSStage, TheWaterStats.seaVSHr);
				formatWaterShader(seaPS, TheWaterStats.seaPSStage, TheWaterStats.seaPSHr);
				// flat/river/sea = draws / with both WaterSea shaders bound; lvl = the mirror plane's height (lvl* = from GameData);
				// s/l = standing polygons, sea / lake; D / O = the map's soft shore; nrm = the normal maps (docs/Debug_Panel_Design.md)
				const wchar_t *nrm = (TheWaterStats.normalMap == 2) ? L"ok" : (TheWaterStats.normalMap == 1) ? L"missing" : L"none";
				// Ronin @feature 02/10/2026 DX9: phase 4 - nrmLake / nrmRiver = WaterNormalLake / River.tga, the same codes (missing = the sea's)
				const wchar_t *nrmLake = (TheWaterStats.normalMapLake == 2) ? L"ok" : (TheWaterStats.normalMapLake == 1) ? L"missing" : L"none";
				const wchar_t *nrmRiver = (TheWaterStats.normalMapRiver == 2) ? L"ok" : (TheWaterStats.normalMapRiver == 1) ? L"missing" : L"none";
				// Ronin @feature 29/09/2026 DX9: hmap = the terrain-height texture (F = R32F the vertex shader reads, L8 = no swell);
				// verts = the flat grid's vertices; refr = the frame copy: ok, FAIL or off
				const wchar_t *refr = (TheWaterStats.refraction == 2) ? L"ok" : (TheWaterStats.refraction == 1) ? L"FAIL" : L"off";
				// Ronin @diagnostic 02/10/2026 DX9: compact - it ran off the screen. Detail only where something is wrong: sh = both
				// shaders and all 4 PS builds, nrm = all three normal maps, bump only when short; lvl* = from the ini, not the map.
				const Bool shOk  = (TheWaterStats.seaVSStage == WaterDebugStats::PS_OK && TheWaterStats.seaPSStage == WaterDebugStats::PS_OK &&
					TheWaterStats.psVariants == 4) ? TRUE : FALSE;
				const Bool nrmOk = (TheWaterStats.normalMap == 2 && TheWaterStats.normalMapLake == 2 && TheWaterStats.normalMapRiver == 2) ? TRUE : FALSE;
				const wchar_t *hmap = (TheWaterStats.heightW <= 0) ? L"none" : (TheWaterStats.heightVTF ? L"F" : L"L8");
				UnicodeString part;
				if (shOk)
					text.format(L"[WATER] type=2  sh=ok");
				else
					text.format(L"[WATER] type=2  VS=%s PS=%s psv=%d/4", seaVS.str(), seaPS.str(), TheWaterStats.psVariants);
				if (TheWaterStats.bumpFrames != 32)
				{
					part.format(L"  bump=%d/32", TheWaterStats.bumpFrames);
					text.concat(part);
				}
				if (nrmOk)
					part.format(L"  nrm=ok");
				else
					part.format(L"  nrm=%s/%s/%s", nrm, nrmLake, nrmRiver);
				text.concat(part);
				// Ronin @bugfix 03/10/2026 DX9: c = the swell grid's real cell in view, world units (`swellcell`, or coarser
				// when the view outgrows the vertex budget)
				part.format(L"  hmap=%s  flat=%u/%u %uv c%.0f  river=%u/%u", hmap, s_waterNow.flatDraws, s_waterNow.flatPSDraws,
					s_waterNow.flatVerts, s_waterNow.flatCell, s_waterNow.riverDraws, s_waterNow.riverPSDraws);
				text.concat(part);
				if (s_waterNow.seaDraws > 0)
				{
					part.format(L"  sea=%u/%u", s_waterNow.seaDraws, s_waterNow.seaPSDraws);
					text.concat(part);
				}
				// Ronin @feature 03/10/2026 DX9: phase 5 - wake = trails drawn + rings (04/10) / their vertices, t = world units a wake
				// texel; +m = the mesh rises too. off = `wakes` 0 or no WaterWake shaders, none = the card cannot (RGBA16F target)
				if (s_waterNow.wakeState <= 1)
					part.format(L"  wake=%s", (s_waterNow.wakeState == 1) ? L"none" : L"off");
				else
					part.format(L"  wake=%u+%ur/%uv t%.2g%s", s_waterNow.wakeTrails, s_waterNow.wakeRipples, s_waterNow.wakeVerts, s_waterNow.wakeTexel,
						(s_waterNow.wakeState == 3) ? L"+m" : L"");
				text.concat(part);
				part.format(L"  s/l=%u/%u  lvl%s=%.1f  refr=%s  mir=%dx%d  D=%.1f O=%.2f", s_waterNow.seaPolys, s_waterNow.lakePolys,
					TheWaterStats.seaLevelFromMap ? L"" : L"*", TheWaterStats.seaLevel, refr, TheWaterStats.mirrorW, TheWaterStats.mirrorH,
					TheWaterTransparency->m_transparentWaterDepth, TheWaterTransparency->m_minWaterOpacity);
				text.concat(part);
			}
			else
			{
				text.format(L"[WATER] type=%d  chip=%d  flatPS=%s  riverPS=%s  flat=%u/%u  river=%u/%u  mesh=%u/%u  other=%u  edge=%d",
					TheGlobalData->m_waterType, (Int)W3DShaderManager::getChipset(), flatPS.str(), riverPS.str(),
					s_waterNow.flatDraws, s_waterNow.flatPSDraws,
					s_waterNow.riverDraws, s_waterNow.riverPSDraws,
					s_waterNow.meshDraws, s_waterNow.meshPSDraws,
					(all > known) ? (all - known) : 0u,
					edge ? 1 : 0);
			}
			putRow(R3DDebugPanel::ROW_WATER, text, GameMakeColor(110, 190, 255, 255));
		}

		// Ronin @diagnostic 09/10/2026 DX9: [LIGHT] - last frame. on = the screen-space pass owns the lights (else the old paths
		// do); live = enabled lights; drawn = on screen; area = their rectangles in screens; surf = frame-to-surface factor.
		// Ronin @diagnostic 17/10/2026 DX9: and the lamp glows, pool fills and effect lights (drawn / candidates / glowing).
		if (s_rowOn[R3DDebugPanel::ROW_LIGHT])
		{
			R3DLocalLights::Stats ls;
			R3DLocalLights::getStats(&ls);
			text.format(L"[LIGHT] on=%d  depth=%s  live=%d  drawn=%d  area=%.2f  taamask=%d  surf=%.2f/%.2f/%.2f  sunshade=%d  glows=%d  poolfill=%d  fx=%d/%d/%d",
				ls.active ? 1 : 0, ls.active ? (ls.ownsDepth ? L"own" : L"shared") : L"none",
				ls.live, ls.drawn, ls.coverage, ls.masked ? 1 : 0, ls.sceneLight[0], ls.sceneLight[1], ls.sceneLight[2], ls.sunShade,
				ls.glows, ls.pools, ls.fx, ls.fxCandidates, ls.fxGlowing);
			putRow(R3DDebugPanel::ROW_LIGHT, text, GameMakeColor(255, 190, 110, 255));
		}
		// Ronin @diagnostic 11/10/2026 DX9: [LIGHT2] - the beams as spot lights (art meshes skipped, models read); the lamp shadow
		// maps: casters = solid meshes drawn, lampmaps = lights with a map, then what was drawn into them.
		if (s_rowOn[R3DDebugPanel::ROW_LIGHT2])
		{
			R3DLocalLights::Stats ls;
			R3DLocalLights::getStats(&ls);
			text.format(L"[LIGHT2] beams=%d (meshes skipped %d, models %d)  casters=%d  lampmaps=%d (%d point lights, %d with sides; %d draws, %.1fk tris)",
				ls.beams, ls.beamMeshes, ls.beamModels, ls.casters, ls.lampMaps, ls.lampPoints, ls.lampSides, ls.lampDraws,
				(float)ls.lampTris / 1000.0f);
			// Ronin @diagnostic 20/10/2026 DX9: GPU ms - scene = the 3D frame up to the lights, shade = the sun's-shade pass, lights = the pass
			UnicodeString gpu;
			if (ls.gpuScene < 0.0f && ls.gpuLights < 0.0f)
				gpu = L"  gpu=n/a";
			else
				gpu.format(L"  gpu: scene=%.2fms shade=%.2fms lights=%.2fms", ls.gpuScene, ls.gpuShade, ls.gpuLights);
			text.concat(gpu);
			putRow(R3DDebugPanel::ROW_LIGHT2, text, GameMakeColor(255, 190, 110, 255));
		}
	}

	// Ronin @feature 28/09/2026 DX9: no panel window, so no rows. One line where the first row used to sit says so, instead of
	// a second layout for every readout. Drawn after the 3D views and their TAA resolve, before the UI.
	void drawNoPanel()
	{
		if (s_noPanel == nullptr && TheDisplayStringManager != nullptr && TheFontLibrary != nullptr)
		{
			s_noPanel = TheDisplayStringManager->newDisplayString();
			if (s_noPanel != nullptr)
			{
				s_noPanel->setFont(panelFont());
				s_noPanel->setText(UnicodeString(L"[PANEL] the debug panel window could not be created - no readouts"));
			}
		}
		if (s_noPanel != nullptr)
			s_noPanel->draw(DEFAULT_X, DEFAULT_Y, GameMakeColor(255, 255, 0, 255), GameMakeColor(0, 0, 0, 255));
	}

	// Ronin @feature 14/09/2026 DX9: translucent body, a darker title strip to grab, the rows in their own colours, then the
	// output lines under the command box (the box draws itself, as a child, after this).
	void panelDraw(GameWindow *window, WinInstanceData *instData)
	{
		Int x, y, w, h;
		window->winGetScreenPosition(&x, &y);
		window->winGetSize(&w, &h);

		TheWindowManager->winFillRect(GameMakeColor(0, 0, 0, 95), 1.0f, x, y, x + w, y + h);
		TheWindowManager->winFillRect(GameMakeColor(60, 60, 60, 140), 1.0f, x, y, x + w, y + TITLE_HEIGHT);
		TheWindowManager->winOpenRect(GameMakeColor(160, 160, 160, 170), 1.0f, x, y, x + w, y + h);

		const Color drop = GameMakeColor(0, 0, 0, 255);
		if (s_title != nullptr)
			s_title->draw(x + PAD, y + 1, GameMakeColor(220, 220, 220, 255), drop);

		Int rowY = y + TITLE_HEIGHT + PAD;
		for (Int r = 0; r < R3DDebugPanel::ROW_COUNT; ++r)
		{
			if (!rowIsLive(r))
				continue;
			Int tw = 0, th = 0;
			s_rows[r].text->getSize(&tw, &th);
			s_rows[r].text->draw(x + PAD, rowY, s_rows[r].color, drop);
			rowY += th;
		}

		// Ronin @feature 16/09/2026 DX9: the picker's Side/Category labels, above its list (the list draws itself, as a child).
		if (s_pickerOpen && s_sideLabel != nullptr && s_categoryLabel != nullptr)
		{
			const Color label = GameMakeColor(255, 220, 120, 255);
			s_sideLabel->draw(x + PAD, y + s_labelTop, label, drop);
			s_categoryLabel->draw(x + s_categoryLabelX, y + s_labelTop, label, drop);
		}

		Int outY = y + s_outputTop;
		for (Int i = 0; i < s_outCount; ++i)
		{
			Int tw = 0, th = 0;
			s_out[i]->getSize(&tw, &th);
			s_out[i]->draw(x + PAD, outY, s_outColor[i], drop);
			outY += th;
		}
	}
}

//-------------------------------------------------------------------------------------------------
void R3DDebugPanel::update(void)
{
	++s_frame;
	s_available = FALSE;
	// Ronin @diagnostic 08/10/2026 DX9: device-object tracking - let go of what everyone else already has, so the table
	// only holds what is alive.
	if (s_trackOn)
		trackSweep();
	sampleReadouts();

	if (TheWindowManager == nullptr || TheGlobalData == nullptr || TheGlobalData->m_headless)
		return;

	if (!s_builtinsDone)
	{
		s_builtinsDone = TRUE;
		registerCommand("help", "list the commands", cmdHelp);
		registerCommand("clear", "clear this output", cmdClear);
		registerCommand("close", "hide this panel (Ctrl+Shift+Z shows it again)", cmdClose);
		registerCommand("shutdown", "quit the game to the desktop now", cmdShutdown);
		registerCommand("water", "water [profile sea|lake|river] | water <switch> 0|1 | water view [0..14] | water <knob> <v> | water save|load|defaults - `water` alone shows the table", cmdWater);
		registerCommand("perf", "perf [reset] - the [PERF] run mean; reset starts it over (measure each switch state apart)", cmdPerf);
		registerCommand("rows", "rows [<name>|all 0|1] - list the readout rows, or switch one or all", cmdRows);
		registerCommand("taa", "taa [0|1] | taa <knob> <v> - temporal AA; the [TAA] row shows live state", cmdTaa);
		registerCommand("ssao", "ssao [0..3] | trees <auto|0|1> | view [0..4] | sun|bleed|colour <v> | save - quality (not saved); trees: AO on trees; view: 1 depth, 2 raw AO, 3 blurred AO, 4 the bleed; sun kept by save", cmdSsao);
		registerCommand("shadows", "shadows [0..3] - shadow-map quality, 0 off (stencil shadows) .. 3 ultra (not saved)", cmdShadows);
		registerCommand("lights", "lights [0|1] | save|load|defaults | intensity|range|small|over|boost|view|beams|shafts|rays|rayturn|lampwidth|spill|spillwide|glows|glowreach|glowjoin|coronas|coronasize|pools|beamreach|poolfill|hotspot|sunfill|sunerase|mapshadow|reflect|reflectground|fresnel|ssr|bounce|fxlights|fxstrength|fxreach|fxmerge|fxflares|fxfire|fxsmooth|pulsemin|pulsefade|searchlight|night*|daysun|dayshade|sun*|lampmaps|lampshadows|lampbias|lampself|lampselfnear|pointmaps|pointsides <v> - local lights per pixel from the scene depth; 0 = the old vertex paths; save keeps the knobs in DX9Lights.ini", cmdLights);
		registerCommand("glowfx", "glowfx - the glowing (additive) particle systems now: how many, how many on screen, the strongest by colour x alpha x size^2", cmdGlowFx);
		registerCommand("night", "night [0|1] | night tint|lo|hi|floor|full|lift <v> - the night look and the dark lift: dim colour into one tint from the map's light, lamps keep theirs (at night; not saved)", cmdNight);
		registerCommand("hdr", "hdr [0|1] | hdr sun <1..4> | hdr warm <0..1> | hdr white <1..2> - HDR stage 1: a float scene, the sun lifted above 1, a tone curve (needs MSAA off; on / off: Options.ini DX9HDR; the rest: DX9Lights.ini)", cmdHdr);
		registerCommand("grade", "grade [0|1] | grade warm|sat|mids <v> - the daytime colour: warm tint, saturation, darker mid-tones (by day; off by default; DX9Lights.ini: grade, gradewarm, gradesat, grademids)", cmdGrade);
		registerCommand("campitch", "campitch [degrees | default] - the camera's tilt (a middle click resets it)", cmdCamPitch);
		registerCommand("sky", "sky | sky rebuild - the map's sky cube: its source, its ground, the ambient's six colours (`lights view 5` shows it)", cmdSky);
		registerCommand("timesetting", "timesetting [1..4] - time of day: 1 morning, 2 afternoon, 3 evening, 4 night (not saved; not in LAN/online)", cmdTimeSetting);
		registerCommand("clouds", "clouds [on|off] - cloud shadows on terrain and models (not saved; none at night)", cmdClouds);
		registerCommand("treefire", "treefire [0|1] | here | area <radius> | scorch <radius> | cap <fires> | spread 0|1 | thin <0..1> - the map's trees on fire: here = the tree nearest the middle of the screen; area / scorch = what a flame weapon / the nuke reports there; thin = the burnt canopy's share gone (not saved)", cmdTreeFire);
		registerCommand("msaa", "msaa [0|2|4|8] - anti-aliasing level, by a device reset (not saved); TAA does not run with it", cmdMsaa);
		registerCommand("resetcheck", "resetcheck - resets the device at the same settings and says whether it worked; started with DX9Track.txt beside the exe, DX9ResetCheck.txt lists what was still alive", cmdResetCheck);
		registerCommand("grid", "grid [lift|width|alpha|radius <value>] - the placement grid Ctrl draws", cmdGrid);
		registerCommand("spawn", "spawn [words] [count] - pick from a list, or spawn <ThingTemplate> [count]; single player only", cmdSpawn);
		registerCommand("credits", "credits [amount] - add money to your player; single player only", cmdCredits);
		registerCommand("power", "power [0|1] - unlimited power for your player, 0 = your real output; single player only", cmdPower);
		registerCommand("mapvision", "mapvision [on|off] - the whole map revealed for your player; single player only", cmdMapVision);
	}

	// Ronin @feature 16/09/2026 DX9: placement takes the mouse after the window translator (10) and before meta events (20),
	// selection (50) and commands (70). Attached once; MessageStream owns it from here on and reset() keeps it.
	if (!s_translatorReady && TheMessageStream != nullptr)
	{
		s_translatorReady = TRUE;
		TheMessageStream->attachTranslator(NEW DevPlaceTranslator, 15);
	}

	GameWindow *win = TheWindowManager->winGetWindowFromId(nullptr, PANEL_WINDOW_ID);
	if (win == nullptr)
	{
		// Created, or recreated after a reset, where the player last left it — kept on screen if the resolution shrank.
		if (TheDisplay != nullptr)
		{
			if (s_posX > (Int)TheDisplay->getWidth() - 40)  s_posX = DEFAULT_X;
			if (s_posY > (Int)TheDisplay->getHeight() - 40) s_posY = DEFAULT_Y;
		}
		win = TheWindowManager->winCreate(nullptr,
			WIN_STATUS_ENABLED | WIN_STATUS_DRAGGABLE | WIN_STATUS_ABOVE | WIN_STATUS_NO_FOCUS,
			s_posX, s_posY, 100, TITLE_HEIGHT + 2 * PAD, nullptr, nullptr);
		if (win == nullptr)
		{
			drawNoPanel();
			return;
		}
		win->winSetWindowId(PANEL_WINDOW_ID);
		win->winSetInputFunc(panelInput);
		win->winSetSystemFunc(panelSystem);
		win->winSetDrawFunc(panelDraw);
	}
	s_available = TRUE;

	// Remember where the player dragged it, for the next time a reset destroys it.
	win->winGetPosition(&s_posX, &s_posY);

	// Search the panel's children only — a nullptr start would search every window.
	GameWindow *entry = (win->winGetChild() != nullptr)
		? TheWindowManager->winGetWindowFromId(win->winGetChild(), ENTRY_WINDOW_ID) : nullptr;
	if (entry == nullptr)
		entry = createEntry(win);
	// Ronin @feature 16/09/2026 DX9: the spawn picker list, recreated and refilled after a reset like the box.
	GameWindow *list = (win->winGetChild() != nullptr)
		? TheWindowManager->winGetWindowFromId(win->winGetChild(), LIST_WINDOW_ID) : nullptr;
	if (list == nullptr)
	{
		list = createList(win);
		s_pickerDirty = TRUE;
	}

	// Ronin @feature 16/09/2026 DX9: a row clicked since last frame — spawn it and close the picker. Done here, not inside the
	// list's input call, because clearing the box empties that same list.
	if (s_pickedID >= 0)
	{
		const Int picked = s_pickedID;
		s_pickedID = -1;
		const ThingTemplate *thing = (TheThingFactory != nullptr) ? TheThingFactory->findByTemplateID((UnsignedShort)picked) : nullptr;
		if (thing != nullptr && logicCommandAllowed())
			startPlacement(thing, s_pickCount);
		if (entry != nullptr)
			GadgetTextEntrySetText(entry, UnicodeString::TheEmptyString);
		TheWindowManager->winSetFocus(nullptr);
	}

	// Ronin @feature 16/09/2026 DX9: the picker is open while the box reads "spawn "; refilled when the text or a label changes.
	{
		const UnicodeString text = (entry != nullptr) ? GadgetTextEntryGetText(entry) : UnicodeString::TheEmptyString;
		if (list == nullptr)
		{
			s_pickerOpen = FALSE;
		}
		else if (s_pickerDirty || text.compare(s_pickerText) != 0)
		{
			s_pickerDirty = FALSE;
			s_pickerText  = text;
			AsciiString filter;
			s_pickerOpen = pickerFilter(text, &filter);
			if (s_pickerOpen)
				fillPicker(list, filter);
			else
				GadgetListBoxReset(list);
		}
	}
	updatePlacement();
	const Bool entryFocused = (entry != nullptr && TheWindowManager->winGetFocus() == entry);

	renderReadouts();

	// Ronin @feature 14/09/2026 DX9: the window manager never takes focus back on a click elsewhere, so a box left focused
	// would keep swallowing hotkeys. A button press outside the panel hands the keys back to the game.
	if (TheMouse != nullptr)
	{
		const MouseIO *mouse = TheMouse->getMouseStatus();
		const Bool down = mouse->leftState == MBS_Down || mouse->leftState == MBS_DoubleClick ||
						  mouse->rightState == MBS_Down || mouse->rightState == MBS_DoubleClick;
		if (down && !s_mouseWasDown && entryFocused)
		{
			Int px = 0, py = 0, pw = 0, ph = 0;
			win->winGetScreenPosition(&px, &py);
			win->winGetSize(&pw, &ph);
			if (mouse->pos.x < px || mouse->pos.x >= px + pw || mouse->pos.y < py || mouse->pos.y >= py + ph)
				TheWindowManager->winSetFocus(nullptr);
		}
		s_mouseWasDown = down;
	}

	if (s_title == nullptr && TheDisplayStringManager != nullptr && TheFontLibrary != nullptr)
	{
		s_title = TheDisplayStringManager->newDisplayString();
		if (s_title != nullptr)
		{
			s_title->setFont(panelFont());
			s_title->setText(UnicodeString(L"DEBUG"));
		}
	}

	// Size to the widest of title, rows, command box and output; rows, box and output stack under the title.
	Int width = 0, rowsHeight = 0, outHeight = 0;
	if (s_title != nullptr)
	{
		Int tw = 0, th = 0;
		s_title->getSize(&tw, &th);
		width = tw;
	}
	for (Int r = 0; r < ROW_COUNT; ++r)
	{
		if (!rowIsLive(r))
			continue;
		Int tw = 0, th = 0;
		s_rows[r].text->getSize(&tw, &th);
		if (tw > width) width = tw;
		rowsHeight += th;
	}
	for (Int i = 0; i < s_outCount; ++i)
	{
		Int tw = 0, th = 0;
		s_out[i]->getSize(&tw, &th);
		if (tw > width) width = tw;
		outHeight += th;
	}
	if (width < ENTRY_MIN_WIDTH)
		width = ENTRY_MIN_WIDTH;
	// Ronin @feature 16/09/2026 DX9: the picker list's column width is fixed at creation, so the panel fits the list.
	if (s_pickerOpen && width < PICKER_WIDTH)
		width = PICKER_WIDTH;

	const Bool show = s_visible && !TheGlobalData->m_loadScreenRender;
	if (!show)
	{
		// Hiding clears focus WITHOUT telling the box (GameWindowManager::windowHiding), which would leave the IME hooked to it.
		if (entryFocused)
			TheWindowManager->winSetFocus(nullptr);
		if (!win->winIsHidden())
			win->winHide(TRUE);
		return;
	}
	if (win->winIsHidden())
		win->winHide(FALSE);

	const Int entryY = TITLE_HEIGHT + PAD + rowsHeight + ((rowsHeight > 0) ? PAD : 0);
	// Ronin @feature 16/09/2026 DX9: with the picker open, its Side/Category labels and list sit between the box and the output.
	s_labelTop = entryY + ENTRY_HEIGHT + PAD;
	const Int listY = s_labelTop + LABEL_HEIGHT + PAD;
	s_outputTop = s_pickerOpen ? listY + PICKER_HEIGHT + PAD : entryY + ENTRY_HEIGHT + PAD;
	const Int panelW = width + 2 * PAD;
	const Int panelH = s_outputTop + ((outHeight > 0) ? outHeight + PAD : 0);

	Int curW = 0, curH = 0;
	win->winGetSize(&curW, &curH);
	if (curW != panelW || curH != panelH)
	{
		win->winSetSize(panelW, panelH);
		// Ronin @bugfix 03/10/2026 DX9: grown past the screen's bottom (the `water` table) - lift it back on.
		Int px = 0, py = 0;
		win->winGetPosition(&px, &py);
		const Int maxY = (TheDisplay != nullptr) ? (Int)TheDisplay->getHeight() - panelH : py;
		if (py > maxY)
			win->winSetPosition(px, (maxY > 0) ? maxY : 0);
	}

	if (entry != nullptr)
	{
		Int ex = 0, ey = 0, ew = 0, eh = 0;
		entry->winGetPosition(&ex, &ey);
		entry->winGetSize(&ew, &eh);
		if (ex != PAD || ey != entryY)
			entry->winSetPosition(PAD, entryY);
		if (ew != width || eh != ENTRY_HEIGHT)
			entry->winSetSize(width, ENTRY_HEIGHT);
	}

	if (list != nullptr)
	{
		if (list->winIsHidden() == s_pickerOpen)
			list->winHide(!s_pickerOpen);
		Int lx = 0, ly = 0;
		list->winGetPosition(&lx, &ly);
		if (lx != PAD || ly != listY)
			list->winSetPosition(PAD, listY);
	}
	// Ronin @feature 16/09/2026 DX9: label widths, for panelInput's click test.
	if (s_pickerOpen && s_sideLabel != nullptr && s_categoryLabel != nullptr)
	{
		Int tw = 0, th = 0;
		s_sideLabel->getSize(&tw, &th);
		s_sideLabelW     = tw;
		s_categoryLabelX = PAD + tw + 16;
		s_categoryLabel->getSize(&tw, &th);
		s_categoryLabelW = tw;
	}
}

//-------------------------------------------------------------------------------------------------
void R3DDebugPanel::toggleVisible(void)
{
	s_visible = !s_visible;
}

//-------------------------------------------------------------------------------------------------
Bool R3DDebugPanel::setRow(Row row, DisplayString *text, Color color)
{
	if (!s_available || row < 0 || row >= ROW_COUNT || text == nullptr)
		return FALSE;

	s_rows[row].text  = text;
	s_rows[row].color = color;
	s_rows[row].stamp = s_frame;
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
Bool R3DDebugPanel::registerCommand(const char *name, const char *help, CommandFunc func)
{
	if (name == nullptr || func == nullptr)
		return FALSE;
	for (Int i = 0; i < s_commandCount; ++i)
	{
		if (stricmp(s_commands[i].name, name) == 0)
		{
			s_commands[i].help = (help != nullptr) ? help : "";
			s_commands[i].func = func;
			return TRUE;
		}
	}
	if (s_commandCount >= PANEL_MAX_COMMANDS)
		return FALSE;
	s_commands[s_commandCount].name = name;
	s_commands[s_commandCount].help = (help != nullptr) ? help : "";
	s_commands[s_commandCount].func = func;
	++s_commandCount;
	return TRUE;
}

//-------------------------------------------------------------------------------------------------
void R3DDebugPanel::print(const UnicodeString &text, Bool isError)
{
	printLine(text, isError);
}
