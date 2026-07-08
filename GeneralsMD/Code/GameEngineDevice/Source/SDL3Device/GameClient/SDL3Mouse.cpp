/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#include "SDL3Device/GameClient/SDL3Mouse.h"

#if defined(SAGE_USE_SDL3)

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "Common/FileSystem.h"
#include "GgcRuntimeFlags.h"
#include "SDL3GameEngine.h"
#include "GameClient/Display.h"
#include "GameClient/Image.h"
#include "GameClient/InGameUI.h"
#include "WW3D2/assetmgr.h"
#include "WW3D2/ddsfile.h"
#include "WW3D2/IRenderBackend.h"
#include "WW3D2/RenderBackend.h"
#include "WW3D2/surfaceclass.h"
#include "WW3D2/texture.h"

static void MapDisplayPointToSDLPoint(Int displayX, Int displayY, float *rawX, float *rawY);

// Retail cursor art marks transparent texels with magenta (255,0,255); tolerances
// absorb texture compression. Channels at or below the noise threshold count as black.
static const UnsignedByte kKeyColorMinRedBlue = 230;
static const UnsignedByte kKeyColorMaxGreen   = 40;
static const UnsignedByte kChannelNoiseFloor  = 8;
// W3D cursor textures top out at 256x256; larger dimensions indicate a bad ANI entry.
static const Uint32 kMaxCursorDimension = 256;

SDL3Mouse::SDL3Mouse() :
	m_nextGetIndex(0),
	m_nextFreeIndex(0),
	m_lastAppliedSDLCursor(INVALID_MOUSE_CURSOR),
	m_lastAppliedSDLFrame(-1),
	m_currentAnimFrame(0.0f),
	m_lastAnimTime(0)
{
	for (Int i = 0; i < NUM_MOUSE_CURSORS; ++i)
	{
		m_cursorImages[i] = nullptr;
		for (Int j = 0; j < MAX_2D_CURSOR_ANIM_FRAMES; ++j)
		{
			m_cursorTextures[i][j] = nullptr;
			m_sdlCursors[i][j] = nullptr;
			m_sdlCursorAniAttempted[i][j] = false;
		}
	}
	reset();
}

SDL3Mouse::~SDL3Mouse()
{
	SDL_ShowCursor();
	for (Int i = 0; i < NUM_MOUSE_CURSORS; ++i)
	{
		for (Int j = 0; j < MAX_2D_CURSOR_ANIM_FRAMES; ++j)
		{
			if (m_sdlCursors[i][j] != nullptr)
			{
				SDL_DestroyCursor(m_sdlCursors[i][j]);
				m_sdlCursors[i][j] = nullptr;
			}
			m_sdlCursorAniAttempted[i][j] = false;
			REF_PTR_RELEASE(m_cursorTextures[i][j]);
		}
	}
}

void SDL3Mouse::init()
{
	Mouse::init();
	// SDL mouse events report window-local absolute coordinates, matching
	// Win32 window messages rather than DirectInput-style deltas.
	m_inputMovesAbsolute = TRUE;
	// SDL does not have the Win32/DX8 cursor paths. Draw SAGE cursor images
	// through the frame renderer instead.
	m_currentRedrawMode = RM_POLYGON;
	setCursor(ARROW);
	syncSystemCursorVisibility();
}

void SDL3Mouse::reset()
{
	Mouse::reset();
	for (UnsignedInt i = 0; i < NUM_MOUSE_EVENTS; ++i)
	{
		m_buffer[i].leftState = MBS_None;
		m_buffer[i].rightState = MBS_None;
		m_buffer[i].middleState = MBS_None;
		m_buffer[i].leftEvent = MOUSE_EVENT_NONE;
		m_buffer[i].rightEvent = MOUSE_EVENT_NONE;
		m_buffer[i].middleEvent = MOUSE_EVENT_NONE;
		m_buffer[i].pos.x = 0;
		m_buffer[i].pos.y = 0;
		m_buffer[i].deltaPos.x = 0;
		m_buffer[i].deltaPos.y = 0;
		m_buffer[i].wheelPos = 0;
		m_buffer[i].time = 0;
	}
	m_nextGetIndex = 0;
	m_nextFreeIndex = 0;
}

void SDL3Mouse::initCursorResources()
{
	for (Int i = 0; i < NUM_MOUSE_CURSORS; ++i)
	{
		m_cursorImages[i] = nullptr;
		for (Int j = 0; j < MAX_2D_CURSOR_ANIM_FRAMES; ++j)
		{
			if (m_sdlCursors[i][j] != nullptr)
			{
				SDL_DestroyCursor(m_sdlCursors[i][j]);
				m_sdlCursors[i][j] = nullptr;
			}
			m_sdlCursorAniAttempted[i][j] = false;
			REF_PTR_RELEASE(m_cursorTextures[i][j]);
		}
	}
	m_lastAppliedSDLCursor = INVALID_MOUSE_CURSOR;
	m_lastAppliedSDLFrame = -1;
	m_currentRedrawMode = RM_POLYGON;
	syncSystemCursorVisibility();
}

void SDL3Mouse::draw()
{
	syncSystemCursorVisibility();

	if (m_visible && m_currentCursor != NONE)
	{
		const Int frame = getCursorTextureFrame(m_currentCursor);
		SDL_Cursor *sdlCursor = getSDLColorCursor(m_currentCursor, frame);
		const Image *image = nullptr;
		TextureClass *texture = nullptr;
		if (sdlCursor == nullptr)
		{
			image = getCursorImage(m_currentCursor);
			texture = getCursorTexture(m_currentCursor, frame);
		}
		logCursorLookup(m_currentCursor, image, texture);
		// TheSuperHackers @bugfix bobtista 12/06/2026 Draw the image cursor whenever there is no SDL
		// hardware cursor. The previous "&& texture == nullptr" made a successfully-loaded texture
		// suppress the image draw and fall through to the (no-op) software-cursor path, leaving no
		// cursor drawn. The texture is otherwise unused on this path.
		(void)texture;
		if (sdlCursor == nullptr && image != nullptr && TheDisplay != nullptr)
		{
			const ICoord2D &hotSpot = m_cursorInfo[m_currentCursor].hotSpotPosition;
			TheDisplay->drawImage(
				image,
				m_currMouse.pos.x - hotSpot.x,
				m_currMouse.pos.y - hotSpot.y,
				m_currMouse.pos.x + image->getImageWidth() - hotSpot.x,
				m_currMouse.pos.y + image->getImageHeight() - hotSpot.y);
		}
		else
		{
			drawFallbackCursor(m_currentCursor);
		}
	}

	drawCursorText();

	if (m_visible)
	{
		drawTooltip();
	}
}

void SDL3Mouse::setCursor(MouseCursor cursor)
{
	Mouse::setCursor(cursor);
	m_currentCursor = cursor;
	m_currentRedrawMode = RM_POLYGON;
	m_currentAnimFrame = 0.0f;
	m_lastAnimTime = SDL_GetTicks();
	if (GgcFlags::Enabled(GgcFlag_CursorDiag) && cursor > NONE && cursor < NUM_MOUSE_CURSORS)
	{
		static MouseCursor s_lastLoggedCursor = INVALID_MOUSE_CURSOR;
		if (cursor == s_lastLoggedCursor)
		{
			syncSystemCursorVisibility();
			return;
		}
		s_lastLoggedCursor = cursor;
		FILE *file = std::fopen("ggc_cursor_diag.txt", "a");
		if (file != nullptr)
		{
			std::fprintf(file,
				"setCursor cursor=%d name=%s texture=%s image=%s frames=%d directions=%d hotspot=%d,%d\n",
				static_cast<Int>(cursor),
				m_cursorInfo[cursor].cursorName.str(),
				m_cursorInfo[cursor].textureName.str(),
				m_cursorInfo[cursor].imageName.str(),
				m_cursorInfo[cursor].numFrames,
				m_cursorInfo[cursor].numDirections,
				m_cursorInfo[cursor].hotSpotPosition.x,
				m_cursorInfo[cursor].hotSpotPosition.y);
			std::fclose(file);
		}
	}
	syncSystemCursorVisibility();
}

void SDL3Mouse::setPosition(Int x, Int y)
{
	Mouse::setPosition(x, y);
	if (TheSDL3Window != NULL && !GgcFlags::Enabled(GgcFlag_DisableMouseWarp))
	{
		float rawX = static_cast<float>(x);
		float rawY = static_cast<float>(y);
		MapDisplayPointToSDLPoint(x, y, &rawX, &rawY);
		SDL_WarpMouseInWindow(TheSDL3Window, rawX, rawY);
	}
}

void SDL3Mouse::setVisibility(Bool visible)
{
	Mouse::setVisibility(visible);
	syncSystemCursorVisibility();
}

void SDL3Mouse::capture()
{
	// TheSuperHackers @bugfix bobtista 27/06/2026 Win32Mouse calls ClipCursor(&windowRect);
	// SDL3's analog is SDL_SetWindowMouseGrab. capture() is only reached when the options-driven
	// Mouse::canCapture() flow allows it, so grab whenever the window exists. This restores
	// edge-of-screen scrolling, which requires the cursor to be captured. GGC_DISABLE_MOUSE_GRAB=1
	// is a debug escape hatch to turn the grab off.
	if (TheSDL3Window != NULL && !GgcFlags::Enabled(GgcFlag_DisableMouseGrab))
	{
		SDL_SetWindowMouseGrab(TheSDL3Window, true);
		onCursorCaptured(SDL_GetWindowMouseGrab(TheSDL3Window) ? TRUE : FALSE);
	}
	else
	{
		onCursorCaptured(FALSE);
	}
}

void SDL3Mouse::releaseCapture()
{
	if (TheSDL3Window != NULL)
	{
		SDL_SetWindowMouseGrab(TheSDL3Window, false);
	}
	onCursorCaptured(FALSE);
}

UnsignedByte SDL3Mouse::getMouseEvent(MouseIO *result, Bool flush)
{
	if (m_nextGetIndex == m_nextFreeIndex)
	{
		return MOUSE_NONE;
	}

	*result = m_buffer[m_nextGetIndex];
	if (flush)
	{
		m_nextGetIndex = (m_nextGetIndex + 1) % NUM_MOUSE_EVENTS;
	}
	return MOUSE_OK;
}

static SDL_Window *GetMouseEventWindow(Uint32 windowID)
{
	SDL_Window *window = SDL_GetWindowFromID(windowID);
	if (window == nullptr)
	{
		window = TheSDL3Window;
	}
	return window;
}

static Bool GetSDLWindowSize(SDL_Window *window, Int *width, Int *height)
{
	if (window == nullptr || width == nullptr || height == nullptr)
	{
		return FALSE;
	}

	int windowWidth = 0;
	int windowHeight = 0;
	// TheSuperHackers @bugfix bobtista 05/06/2026 Mouse events are in logical window
	// points, so the display mapping must divide by the window size in points too.
	// Do not fall back to SDL_GetWindowSizeInPixels: on a Retina/HiDPI display that
	// returns physical pixels (~2x), which would scale the cursor position wrong.
	SDL_GetWindowSize(window, &windowWidth, &windowHeight);
	if (windowWidth <= 0 || windowHeight <= 0)
	{
		return FALSE;
	}

	*width = windowWidth;
	*height = windowHeight;
	return TRUE;
}

static void GetLetterboxContentRect(Int windowWidth, Int windowHeight, Int *offsetX, Int *offsetY, Int *contentWidth, Int *contentHeight)
{
	*offsetX = 0;
	*offsetY = 0;
	*contentWidth = windowWidth;
	*contentHeight = windowHeight;
	if (g_renderBackend != NULL && g_renderBackend->Is_Present_Letterbox_Active())
	{
		*offsetX = g_renderBackend->Get_Present_Offset_X();
		*offsetY = g_renderBackend->Get_Present_Offset_Y();
		const Int cw = g_renderBackend->Get_Present_Content_Width();
		const Int ch = g_renderBackend->Get_Present_Content_Height();
		if (cw > 0 && ch > 0)
		{
			*contentWidth = cw;
			*contentHeight = ch;
		}
	}
}

static void MapSDLPointToDisplayPoint(float rawX, float rawY, Uint32 windowID, Int *displayX, Int *displayY)
{
	if (displayX == nullptr || displayY == nullptr)
	{
		return;
	}

	SDL_Window *window = GetMouseEventWindow(windowID);
	Int windowWidth = 0;
	Int windowHeight = 0;
	if (TheDisplay == nullptr || !GetSDLWindowSize(window, &windowWidth, &windowHeight))
	{
		*displayX = static_cast<Int>(std::lround(rawX));
		*displayY = static_cast<Int>(std::lround(rawY));
		return;
	}

	const Int displayWidth = TheDisplay->getWidth();
	const Int displayHeight = TheDisplay->getHeight();
	if (displayWidth <= 0 || displayHeight <= 0)
	{
		*displayX = static_cast<Int>(std::lround(rawX));
		*displayY = static_cast<Int>(std::lround(rawY));
		return;
	}

	// TheSuperHackers @feature bobtista 08/06/2026 When the present is letterboxed (multiplayer), the
	// game is drawn in a centered sub-rect of the window. Subtract the bar offset and map against the
	// content size so pointer coordinates land in the rendered area; clicks in the bars map outside
	// the display bounds and are ignored downstream.
	Int offsetX;
	Int offsetY;
	Int contentWidth;
	Int contentHeight;
	GetLetterboxContentRect(windowWidth, windowHeight, &offsetX, &offsetY, &contentWidth, &contentHeight);

	*displayX = static_cast<Int>(std::lround((rawX - static_cast<float>(offsetX)) * static_cast<float>(displayWidth) / static_cast<float>(contentWidth)));
	*displayY = static_cast<Int>(std::lround((rawY - static_cast<float>(offsetY)) * static_cast<float>(displayHeight) / static_cast<float>(contentHeight)));
}

static void MapDisplayPointToSDLPoint(Int displayX, Int displayY, float *rawX, float *rawY)
{
	if (rawX == nullptr || rawY == nullptr)
	{
		return;
	}

	Int windowWidth = 0;
	Int windowHeight = 0;
	if (TheDisplay == nullptr || !GetSDLWindowSize(TheSDL3Window, &windowWidth, &windowHeight))
	{
		*rawX = static_cast<float>(displayX);
		*rawY = static_cast<float>(displayY);
		return;
	}

	const Int displayWidth = TheDisplay->getWidth();
	const Int displayHeight = TheDisplay->getHeight();
	if (displayWidth <= 0 || displayHeight <= 0)
	{
		*rawX = static_cast<float>(displayX);
		*rawY = static_cast<float>(displayY);
		return;
	}

	// Mirror the letterbox mapping in MapSDLPointToDisplayPoint: scale by the content size and add the
	// bar offset so a warped cursor lands in the rendered sub-rect.
	Int offsetX;
	Int offsetY;
	Int contentWidth;
	Int contentHeight;
	GetLetterboxContentRect(windowWidth, windowHeight, &offsetX, &offsetY, &contentWidth, &contentHeight);

	*rawX = static_cast<float>(displayX) * static_cast<float>(contentWidth) / static_cast<float>(displayWidth) + static_cast<float>(offsetX);
	*rawY = static_cast<float>(displayY) * static_cast<float>(contentHeight) / static_cast<float>(displayHeight) + static_cast<float>(offsetY);
}

void SDL3Mouse::addSDL3MotionEvent(const SDL_MouseMotionEvent &event)
{
	MouseIO io;
	io.leftState = MBS_None;
	io.rightState = MBS_None;
	io.middleState = MBS_None;
	io.leftEvent = MOUSE_EVENT_NONE;
	io.rightEvent = MOUSE_EVENT_NONE;
	io.middleEvent = MOUSE_EVENT_NONE;
	MapSDLPointToDisplayPoint(event.x, event.y, event.windowID, &io.pos.x, &io.pos.y);
	io.deltaPos.x = static_cast<Int>(event.xrel);
	io.deltaPos.y = static_cast<Int>(event.yrel);
	io.wheelPos = 0;
	io.time = SDL_GetTicks();
	pushEvent(io);
}

void SDL3Mouse::addSDL3ButtonEvent(const SDL_MouseButtonEvent &event)
{
	MouseIO io;
	io.leftState = MBS_None;
	io.rightState = MBS_None;
	io.middleState = MBS_None;
	io.leftEvent = MOUSE_EVENT_NONE;
	io.rightEvent = MOUSE_EVENT_NONE;
	io.middleEvent = MOUSE_EVENT_NONE;
	MapSDLPointToDisplayPoint(event.x, event.y, event.windowID, &io.pos.x, &io.pos.y);
	io.deltaPos.x = 0;
	io.deltaPos.y = 0;
	io.wheelPos = 0;
	io.time = SDL_GetTicks();

	MouseButtonState state = (event.down != 0) ? MBS_Down : MBS_Up;
	if (event.button == SDL_BUTTON_LEFT)
	{
		io.leftState = state;
	}
	else if (event.button == SDL_BUTTON_RIGHT)
	{
		io.rightState = state;
	}
	else if (event.button == SDL_BUTTON_MIDDLE)
	{
		io.middleState = state;
	}

	pushEvent(io);
}

void SDL3Mouse::addSDL3WheelEvent(const SDL_MouseWheelEvent &event)
{
	MouseIO io;
	io.leftState = MBS_None;
	io.rightState = MBS_None;
	io.middleState = MBS_None;
	io.leftEvent = MOUSE_EVENT_NONE;
	io.rightEvent = MOUSE_EVENT_NONE;
	io.middleEvent = MOUSE_EVENT_NONE;
	io.pos.x = m_currMouse.pos.x;
	io.pos.y = m_currMouse.pos.y;
	io.deltaPos.x = 0;
	io.deltaPos.y = 0;
	io.wheelPos = static_cast<Int>(event.y * MOUSE_WHEEL_DELTA);
	io.time = SDL_GetTicks();
	pushEvent(io);
}

const Image *SDL3Mouse::getCursorImage(MouseCursor cursor)
{
	if (cursor <= NONE || cursor >= NUM_MOUSE_CURSORS || TheMappedImageCollection == nullptr)
	{
		return nullptr;
	}

	if (m_cursorImages[cursor] == nullptr && !m_cursorInfo[cursor].imageName.isEmpty())
	{
		m_cursorImages[cursor] = TheMappedImageCollection->findImageByName(m_cursorInfo[cursor].imageName);
	}

	return m_cursorImages[cursor];
}

TextureClass *SDL3Mouse::getCursorTexture(MouseCursor cursor, Int frame)
{
	if (cursor <= NONE || cursor >= NUM_MOUSE_CURSORS)
	{
		return nullptr;
	}

	Int frames = m_cursorInfo[cursor].numFrames;
	if (frames <= 0)
	{
		frames = 1;
	}
	if (frames > MAX_2D_CURSOR_ANIM_FRAMES)
	{
		frames = MAX_2D_CURSOR_ANIM_FRAMES;
	}
	if (frame < 0 || frame >= frames)
	{
		frame = 0;
	}
	if (m_cursorTextures[cursor][frame] == nullptr && !m_cursorInfo[cursor].textureName.isEmpty())
	{
		char textureName[128];
		if (frames == 1)
		{
			snprintf(textureName, sizeof(textureName), "%s.tga", m_cursorInfo[cursor].textureName.str());
		}
		else
		{
			snprintf(textureName, sizeof(textureName), "%s%04d.tga", m_cursorInfo[cursor].textureName.str(), frame);
		}

		WW3DAssetManager *assetManager = WW3DAssetManager::Get_Instance();
		if (assetManager != nullptr)
		{
			m_cursorTextures[cursor][frame] = assetManager->Get_Texture(textureName, MIP_LEVELS_1);
		}
	}

	return m_cursorTextures[cursor][frame];
}

SDL_Cursor *SDL3Mouse::getSDLColorCursor(MouseCursor cursor, Int frame)
{
	if (cursor <= NONE || cursor >= NUM_MOUSE_CURSORS)
	{
		return nullptr;
	}
	if (frame < 0 || frame >= MAX_2D_CURSOR_ANIM_FRAMES)
	{
		frame = 0;
	}

	if (m_sdlCursors[cursor][frame] == nullptr && !m_sdlCursorAniAttempted[cursor][frame])
	{
		m_sdlCursorAniAttempted[cursor][frame] = true;
		m_sdlCursors[cursor][frame] = createSDLANICursor(cursor, frame);
	}

	if (m_sdlCursors[cursor][frame] == nullptr && GgcFlags::Enabled(GgcFlag_SdlTextureCursor))
	{
		TextureClass *texture = getCursorTexture(cursor, frame);
		if (texture != nullptr)
		{
			m_sdlCursors[cursor][frame] = createSDLColorCursor(texture, m_cursorInfo[cursor].hotSpotPosition);
		}
	}

	return m_sdlCursors[cursor][frame];
}

static Uint16 ReadLE16(const Uint8 *p)
{
	return static_cast<Uint16>(static_cast<Uint16>(p[0]) | (static_cast<Uint16>(p[1]) << 8));
}

static Uint32 ReadLE32(const Uint8 *p)
{
	return static_cast<Uint32>(p[0]) |
		(static_cast<Uint32>(p[1]) << 8) |
		(static_cast<Uint32>(p[2]) << 16) |
		(static_cast<Uint32>(p[3]) << 24);
}

static Sint32 ReadLE32S(const Uint8 *p)
{
	return static_cast<Sint32>(ReadLE32(p));
}

static Bool LoadBinaryFile(const char *path, std::vector<Uint8> &data)
{
	if (TheFileSystem == nullptr)
	{
		return FALSE;
	}

	File *sageFile = TheFileSystem->openFile(path, File::READ | File::BINARY);
	if (sageFile == nullptr)
	{
		return FALSE;
	}
	const Int size = sageFile->size();
	char *buffer = sageFile->readEntireAndClose();
	if (buffer == nullptr || size <= 0)
	{
		delete[] buffer;
		return FALSE;
	}
	data.assign(reinterpret_cast<Uint8 *>(buffer), reinterpret_cast<Uint8 *>(buffer) + size);
	delete[] buffer;
	return TRUE;
}

static Bool FindANIIconChunkInRange(
	const std::vector<Uint8> &data,
	size_t begin,
	size_t end,
	size_t *iconOffset,
	size_t *iconSize)
{
	size_t pos = begin;
	while (pos + 8 <= end && pos + 8 <= data.size())
	{
		const Uint8 *chunk = data.data() + pos;
		const Uint32 chunkSize = ReadLE32(chunk + 4);
		const size_t payload = pos + 8;
		const size_t next = payload + chunkSize + (chunkSize & 1U);
		if (payload + chunkSize > end || payload + chunkSize > data.size())
		{
			return FALSE;
		}

		if (std::memcmp(chunk, "icon", 4) == 0)
		{
			*iconOffset = payload;
			*iconSize = chunkSize;
			return TRUE;
		}
		if (std::memcmp(chunk, "LIST", 4) == 0 && chunkSize >= 4)
		{
			if (FindANIIconChunkInRange(data, payload + 4, payload + chunkSize, iconOffset, iconSize))
			{
				return TRUE;
			}
		}
		pos = next;
	}
	return FALSE;
}

static Bool FindANIIconChunk(const std::vector<Uint8> &data, size_t *iconOffset, size_t *iconSize)
{
	if (data.size() < 12 || std::memcmp(data.data(), "RIFF", 4) != 0 || std::memcmp(data.data() + 8, "ACON", 4) != 0)
	{
		return FALSE;
	}
	const size_t riffEnd = std::min<size_t>(data.size(), 8 + ReadLE32(data.data() + 4));
	return FindANIIconChunkInRange(data, 12, riffEnd, iconOffset, iconSize);
}

static SDL_Cursor *CreateCursorFromCURData(const Uint8 *cur, size_t curSize, const char *diagName)
{
	if (curSize < 22 || ReadLE16(cur) != 0 || ReadLE16(cur + 2) != 2 || ReadLE16(cur + 4) < 1)
	{
		return nullptr;
	}

	const Uint8 *entry = cur + 6;
	const Uint32 width = entry[0] == 0 ? kMaxCursorDimension : entry[0];
	const Uint32 listedHeight = entry[1] == 0 ? kMaxCursorDimension : entry[1];
	const Uint16 hotX = ReadLE16(entry + 4);
	const Uint16 hotY = ReadLE16(entry + 6);
	const Uint32 imageSize = ReadLE32(entry + 8);
	const Uint32 imageOffset = ReadLE32(entry + 12);
	if (imageOffset >= curSize || imageSize > curSize - imageOffset || imageSize < 40)
	{
		return nullptr;
	}

	const Uint8 *dib = cur + imageOffset;
	const Uint32 headerSize = ReadLE32(dib);
	if (headerSize < 40 || headerSize > imageSize)
	{
		return nullptr;
	}
	const Sint32 dibWidth = ReadLE32S(dib + 4);
	const Sint32 dibHeight = ReadLE32S(dib + 8);
	const Uint16 planes = ReadLE16(dib + 12);
	const Uint16 bitCount = ReadLE16(dib + 14);
	const Uint32 compression = ReadLE32(dib + 16);
	const Uint32 clrUsed = ReadLE32(dib + 32);
	if (dibWidth <= 0 || dibHeight == 0 || planes != 1 || (compression != 0 && compression != 3))
	{
		return nullptr;
	}

	const Uint32 actualWidth = static_cast<Uint32>(dibWidth);
	const Uint32 actualHeight = static_cast<Uint32>((dibHeight < 0 ? -dibHeight : dibHeight) / 2);
	if (actualWidth == 0 || actualHeight == 0 || actualWidth > kMaxCursorDimension || actualHeight > kMaxCursorDimension)
	{
		return nullptr;
	}

	Uint32 paletteEntries = 0;
	if (bitCount <= 8)
	{
		paletteEntries = clrUsed != 0 ? clrUsed : (1U << bitCount);
	}
	const size_t paletteOffset = headerSize;
	const size_t xorOffset = paletteOffset + paletteEntries * 4;
	const Uint32 xorStride = ((actualWidth * bitCount + 31) / 32) * 4;
	const Uint32 andStride = ((actualWidth + 31) / 32) * 4;
	const size_t xorSize = static_cast<size_t>(xorStride) * actualHeight;
	const size_t andOffset = xorOffset + xorSize;
	const size_t andSize = static_cast<size_t>(andStride) * actualHeight;
	if (xorOffset > imageSize || xorSize > imageSize - xorOffset)
	{
		return nullptr;
	}

	std::vector<Uint8> pixels(static_cast<size_t>(actualWidth) * actualHeight * 4, 0);
	for (Uint32 y = 0; y < actualHeight; ++y)
	{
		const Uint32 sourceY = dibHeight > 0 ? (actualHeight - 1 - y) : y;
		const Uint8 *xorRow = dib + xorOffset + static_cast<size_t>(sourceY) * xorStride;
		const Uint8 *andRow = (andOffset + andSize <= imageSize) ? (dib + andOffset + static_cast<size_t>(sourceY) * andStride) : nullptr;
		for (Uint32 x = 0; x < actualWidth; ++x)
		{
			Uint8 r = 0;
			Uint8 g = 0;
			Uint8 b = 0;
			Uint8 a = 255;
			if (bitCount == 32)
			{
				const Uint8 *p = xorRow + x * 4;
				b = p[0];
				g = p[1];
				r = p[2];
				a = p[3];
			}
			else if (bitCount == 24)
			{
				const Uint8 *p = xorRow + x * 3;
				b = p[0];
				g = p[1];
				r = p[2];
			}
			else if (bitCount == 8 || bitCount == 4 || bitCount == 1)
			{
				Uint8 index = 0;
				if (bitCount == 8)
				{
					index = xorRow[x];
				}
				else if (bitCount == 4)
				{
					const Uint8 packed = xorRow[x / 2];
					index = (x & 1) ? (packed & 0x0F) : (packed >> 4);
				}
				else
				{
					index = (xorRow[x / 8] >> (7 - (x & 7))) & 1;
				}
				if (index >= paletteEntries)
				{
					continue;
				}
				const Uint8 *color = dib + paletteOffset + index * 4;
				b = color[0];
				g = color[1];
				r = color[2];
			}
			else
			{
				return nullptr;
			}

			if (andRow != nullptr && (andRow[x / 8] & (0x80 >> (x & 7))))
			{
				a = 0;
			}

			Uint8 *dest = pixels.data() + (static_cast<size_t>(y) * actualWidth + x) * 4;
			dest[0] = r;
			dest[1] = g;
			dest[2] = b;
			dest[3] = a;
		}
	}

	SDL_Surface *surface = SDL_CreateSurfaceFrom(
		static_cast<int>(actualWidth),
		static_cast<int>(actualHeight),
		SDL_PIXELFORMAT_RGBA32,
		pixels.data(),
		static_cast<int>(actualWidth * 4));
	SDL_Cursor *cursor = surface != nullptr ? SDL_CreateColorCursor(surface, hotX, hotY) : nullptr;
	const char *sdlError = cursor == nullptr ? SDL_GetError() : "";
	SDL_DestroySurface(surface);

	if (GgcFlags::Enabled(GgcFlag_CursorDiag))
	{
		FILE *file = std::fopen("ggc_cursor_diag.txt", "a");
		if (file != nullptr)
		{
			std::fprintf(file,
				"aniCursorBuild file=%s size=%ux%u listed=%ux%u bpp=%u hot=%u,%u cursor=%p error=%s\n",
				diagName,
				actualWidth,
				actualHeight,
				width,
				listedHeight,
				bitCount,
				hotX,
				hotY,
				static_cast<void *>(cursor),
				sdlError);
			std::fclose(file);
		}
	}

	return cursor;
}

SDL_Cursor *SDL3Mouse::createSDLANICursor(MouseCursor cursor, Int frame)
{
	if (cursor <= NONE || cursor >= NUM_MOUSE_CURSORS || m_cursorInfo[cursor].textureName.isEmpty())
	{
		return nullptr;
	}

	char path[256];
	std::vector<Uint8> fileData;
	if (m_cursorInfo[cursor].numDirections > 1)
	{
		if (frame < 0 || frame >= m_cursorInfo[cursor].numDirections)
		{
			frame = 0;
		}
		snprintf(path, sizeof(path), "Data/Cursors/%s%d.ANI", m_cursorInfo[cursor].textureName.str(), frame);
	}
	else
	{
		snprintf(path, sizeof(path), "Data/Cursors/%s.ANI", m_cursorInfo[cursor].textureName.str());
	}
	if (!LoadBinaryFile(path, fileData))
	{
		if (m_cursorInfo[cursor].numDirections > 1)
		{
			snprintf(path, sizeof(path), "Data/Cursors/%s%d.ani", m_cursorInfo[cursor].textureName.str(), frame);
		}
		else
		{
			snprintf(path, sizeof(path), "Data/Cursors/%s.ani", m_cursorInfo[cursor].textureName.str());
		}
		if (!LoadBinaryFile(path, fileData))
		{
			return nullptr;
		}
	}

	size_t iconOffset = 0;
	size_t iconSize = 0;
	if (!FindANIIconChunk(fileData, &iconOffset, &iconSize))
	{
		return nullptr;
	}

	return CreateCursorFromCURData(fileData.data() + iconOffset, iconSize, path);
}

static UnsignedByte Scale4To8(UnsignedInt value)
{
	value &= 0xF;
	return static_cast<UnsignedByte>((value << 4) | value);
}

static UnsignedByte Scale5To8(UnsignedInt value)
{
	value &= 0x1F;
	return static_cast<UnsignedByte>((value << 3) | (value >> 2));
}

static UnsignedByte Scale6To8(UnsignedInt value)
{
	value &= 0x3F;
	return static_cast<UnsignedByte>((value << 2) | (value >> 4));
}

static Bool ReadSurfacePixelRGBA(
	const SurfaceClass::SurfaceDescription &desc,
	const UnsignedByte *pixel,
	UnsignedByte *r,
	UnsignedByte *g,
	UnsignedByte *b,
	UnsignedByte *a)
{
	switch (desc.Format)
	{
		case WW3D_FORMAT_A8R8G8B8:
			*b = pixel[0];
			*g = pixel[1];
			*r = pixel[2];
			*a = pixel[3];
			return TRUE;
		case WW3D_FORMAT_X8R8G8B8:
		case WW3D_FORMAT_R8G8B8:
			*b = pixel[0];
			*g = pixel[1];
			*r = pixel[2];
			*a = 255;
			return TRUE;
		case WW3D_FORMAT_A4R4G4B4:
		{
			UnsignedShort raw = 0;
			// TheSuperHackers @bugfix bobtista 28/05/2026 Avoid an unaligned dereference; std::memcpy is portable on platforms with strict alignment.
			std::memcpy(&raw, pixel, sizeof(raw));
			const UnsignedInt value = raw;
			*a = Scale4To8(value >> 12);
			*r = Scale4To8(value >> 8);
			*g = Scale4To8(value >> 4);
			*b = Scale4To8(value);
			return TRUE;
		}
		case WW3D_FORMAT_A1R5G5B5:
		{
			UnsignedShort raw = 0;
			// TheSuperHackers @bugfix bobtista 28/05/2026 Avoid an unaligned dereference; std::memcpy is portable on platforms with strict alignment.
			std::memcpy(&raw, pixel, sizeof(raw));
			const UnsignedInt value = raw;
			*a = (value & 0x8000) ? 255 : 0;
			*r = Scale5To8(value >> 10);
			*g = Scale5To8(value >> 5);
			*b = Scale5To8(value);
			return TRUE;
		}
		case WW3D_FORMAT_R5G6B5:
		{
			UnsignedShort raw = 0;
			// TheSuperHackers @bugfix bobtista 28/05/2026 Avoid an unaligned dereference; std::memcpy is portable on platforms with strict alignment.
			std::memcpy(&raw, pixel, sizeof(raw));
			const UnsignedInt value = raw;
			*a = 255;
			*r = Scale5To8(value >> 11);
			*g = Scale6To8(value >> 5);
			*b = Scale5To8(value);
			return TRUE;
		}
		default:
			return FALSE;
	}
}

SDL_Cursor *SDL3Mouse::createSDLColorCursor(TextureClass *texture, const ICoord2D &hotSpot)
{
	if (texture == nullptr)
	{
		return nullptr;
	}

	if (!texture->Is_Initialized())
	{
		texture->Init();
	}

	SurfaceClass::SurfaceDescription desc;
	texture->Get_Level_Description(desc, 0);
	if (desc.Width == 0 || desc.Height == 0 || desc.Width > kMaxCursorDimension || desc.Height > kMaxCursorDimension)
	{
		return nullptr;
	}

	std::vector<Uint8> decodedPixels;
	const UnsignedByte *src = nullptr;
	int pitch = 0;
	UnsignedInt bytesPerPixel = Get_Bytes_Per_Pixel(desc.Format);
#if !defined(GGC_RENDER_BACKEND_BGFX)
	SurfaceClass *fallbackSurface = nullptr;
	Bool lockedSurface = FALSE;
#endif
	switch (desc.Format)
	{
		case WW3D_FORMAT_DXT1:
		case WW3D_FORMAT_DXT2:
		case WW3D_FORMAT_DXT3:
		case WW3D_FORMAT_DXT4:
		case WW3D_FORMAT_DXT5:
		{
			decodedPixels.resize(desc.Width * desc.Height * 4);
			DDSFileClass ddsFile(texture->Get_Full_Path(), 0);
			if (!ddsFile.Is_Available() || !ddsFile.Load() || ddsFile.Get_Width(0) != desc.Width ||
				ddsFile.Get_Height(0) != desc.Height)
			{
				return nullptr;
			}
			for (UnsignedInt y = 0; y < desc.Height; ++y)
			{
				for (UnsignedInt x = 0; x < desc.Width; ++x)
				{
					const UnsignedInt argb = ddsFile.Get_Pixel(0, x, y);
					Uint8 *destPixel = decodedPixels.data() + y * desc.Width * 4 + x * 4;
					destPixel[0] = static_cast<Uint8>(argb & 0xFF);
					destPixel[1] = static_cast<Uint8>((argb >> 8) & 0xFF);
					destPixel[2] = static_cast<Uint8>((argb >> 16) & 0xFF);
					destPixel[3] = static_cast<Uint8>((argb >> 24) & 0xFF);
				}
			}
			desc.Format = WW3D_FORMAT_A8R8G8B8;
			src = decodedPixels.data();
			pitch = desc.Width * 4;
			bytesPerPixel = 4;
			break;
		}
		default:
		{
			const std::vector<TextureBaseClass::TextureMipSnapshot> &mips = texture->Get_CPU_Texture_Mips();
			if (!mips.empty()) {
				const TextureBaseClass::TextureMipSnapshot &mip = mips[0];
				const UnsignedInt mipBytesPerPixel = Get_Bytes_Per_Pixel(mip.Format);
				if (mip.Format != WW3D_FORMAT_UNKNOWN &&
					mip.Width != 0 &&
					mip.Height != 0 &&
					mipBytesPerPixel != 0 &&
					mip.Pitch >= mip.Width * mipBytesPerPixel &&
					mip.Data.size() >= static_cast<size_t>(mip.Pitch) * mip.Height)
				{
					desc.Format = mip.Format;
					desc.Width = mip.Width;
					desc.Height = mip.Height;
					src = mip.Data.data();
					pitch = mip.Pitch;
					bytesPerPixel = mipBytesPerPixel;
				}
			}

			if (src == nullptr) {
#if !defined(GGC_RENDER_BACKEND_BGFX)
				fallbackSurface = texture->Get_Surface_Level();
				if (fallbackSurface == nullptr) {
					return nullptr;
				}
				fallbackSurface->Get_Description(desc);
				bytesPerPixel = fallbackSurface->Get_Bytes_Per_Pixel();
				src = static_cast<UnsignedByte *>(fallbackSurface->Lock(&pitch));
				lockedSurface = TRUE;
#else
				return nullptr;
#endif
			}
			break;
		}
	}

	if (src == nullptr)
	{
#if !defined(GGC_RENDER_BACKEND_BGFX)
		REF_PTR_RELEASE(fallbackSurface);
#endif
		return nullptr;
	}

	const UnsignedInt cursorPitch = desc.Width * 4;
	std::vector<Uint8> pixels(cursorPitch * desc.Height, 0);
	std::vector<Uint8> sourcePixels(cursorPitch * desc.Height, 0);
	UnsignedInt visiblePixels = 0;
	UnsignedInt alphaPixels = 0;
	UnsignedInt nonKeyPixels = 0;

	if (pixels.empty() || sourcePixels.empty())
	{
#if !defined(GGC_RENDER_BACKEND_BGFX)
		if (lockedSurface)
		{
			fallbackSurface->Unlock();
		}
		REF_PTR_RELEASE(fallbackSurface);
#endif
		return nullptr;
	}

	for (UnsignedInt y = 0; y < desc.Height; ++y)
	{
		for (UnsignedInt x = 0; x < desc.Width; ++x)
		{
			UnsignedByte r = 0;
			UnsignedByte g = 0;
			UnsignedByte b = 0;
			UnsignedByte a = 0;
			const UnsignedByte *sourcePixel = src + y * pitch + x * bytesPerPixel;
			if (!ReadSurfacePixelRGBA(desc, sourcePixel, &r, &g, &b, &a))
			{
				a = 0;
			}
			const Bool isKeyColor = (r > kKeyColorMinRedBlue && g < kKeyColorMaxGreen && b > kKeyColorMinRedBlue);
			Uint8 *destPixel = sourcePixels.data() + y * cursorPitch + x * 4;
			destPixel[0] = r;
			destPixel[1] = g;
			destPixel[2] = b;
			destPixel[3] = a;
			if (a > kChannelNoiseFloor)
			{
				++alphaPixels;
			}
			if (!isKeyColor && (r > kChannelNoiseFloor || g > kChannelNoiseFloor || b > kChannelNoiseFloor))
			{
				++nonKeyPixels;
			}
		}
	}

	const Bool useAlpha = (alphaPixels > 0);
	for (UnsignedInt y = 0; y < desc.Height; ++y)
	{
		for (UnsignedInt x = 0; x < desc.Width; ++x)
		{
			const Uint8 *sourcePixel = sourcePixels.data() + y * cursorPitch + x * 4;
			Uint8 *destPixel = pixels.data() + y * cursorPitch + x * 4;
			const UnsignedByte r = sourcePixel[0];
			const UnsignedByte g = sourcePixel[1];
			const UnsignedByte b = sourcePixel[2];
			const UnsignedByte a = sourcePixel[3];
			const Bool isKeyColor = (r > kKeyColorMinRedBlue && g < kKeyColorMaxGreen && b > kKeyColorMinRedBlue);
			const Bool visible = !isKeyColor && (useAlpha ? (a > kChannelNoiseFloor) : (r > kChannelNoiseFloor || g > kChannelNoiseFloor || b > kChannelNoiseFloor));
			destPixel[0] = r;
			destPixel[1] = g;
			destPixel[2] = b;
			destPixel[3] = visible ? (useAlpha ? a : 255) : 0;
			if (!visible)
			{
				continue;
			}

			++visiblePixels;
		}
	}

#if !defined(GGC_RENDER_BACKEND_BGFX)
	if (lockedSurface)
	{
		fallbackSurface->Unlock();
	}
	REF_PTR_RELEASE(fallbackSurface);
#endif

	Int hotX = hotSpot.x;
	Int hotY = hotSpot.y;
	if (hotX < 0 || hotX >= static_cast<Int>(desc.Width))
	{
		hotX = 0;
	}
	if (hotY < 0 || hotY >= static_cast<Int>(desc.Height))
	{
		hotY = 0;
	}

	SDL_Surface *cursorSurface = SDL_CreateSurfaceFrom(
		static_cast<int>(desc.Width),
		static_cast<int>(desc.Height),
		SDL_PIXELFORMAT_RGBA32,
		pixels.data(),
		static_cast<int>(cursorPitch));
	SDL_Cursor *cursor = cursorSurface != nullptr ? SDL_CreateColorCursor(cursorSurface, hotX, hotY) : nullptr;
	const char *sdlError = cursor == nullptr ? SDL_GetError() : "";
	SDL_DestroySurface(cursorSurface);

	if (GgcFlags::Enabled(GgcFlag_CursorDiag))
	{
		FILE *file = std::fopen("ggc_cursor_diag.txt", "a");
		if (file != nullptr)
		{
			std::fprintf(file,
				"cursorTextureBuild file=%s format=%d size=%ux%u alphaPixels=%u nonKeyPixels=%u visiblePixels=%u cursor=%p error=%s\n",
				texture->Get_Texture_Name().str(),
				static_cast<Int>(desc.Format),
				desc.Width,
				desc.Height,
				alphaPixels,
				nonKeyPixels,
				visiblePixels,
				static_cast<void *>(cursor),
				sdlError);
			std::fclose(file);
		}
	}

	return cursor;
}

Int SDL3Mouse::getCursorTextureFrame(MouseCursor cursor)
{
	if (cursor <= NONE || cursor >= NUM_MOUSE_CURSORS)
	{
		return 0;
	}

	const Int directions = m_cursorInfo[cursor].numDirections;
	if (directions > 1)
	{
		if (TheInGameUI != nullptr && TheInGameUI->isScrolling())
		{
			Coord2D offset = TheInGameUI->getScrollAmount();
			if (offset.x != 0.0f || offset.y != 0.0f)
			{
				offset.normalize();
				const Real theta = std::fmod(std::atan2(offset.y, offset.x) + TWO_PI, TWO_PI);
				Int direction = static_cast<Int>(theta / (TWO_PI / static_cast<Real>(directions)) + 0.5f);
				if (direction >= directions)
				{
					direction = 0;
				}
				return direction;
			}
		}
		return 0;
	}

	Int frames = m_cursorInfo[cursor].numFrames;
	if (frames <= 1)
	{
		return 0;
	}
	if (frames > MAX_2D_CURSOR_ANIM_FRAMES)
	{
		frames = MAX_2D_CURSOR_ANIM_FRAMES;
	}

	UnsignedInt now = SDL_GetTicks();
	if (m_lastAnimTime == 0)
	{
		m_lastAnimTime = now;
	}

	const Real fps = m_cursorInfo[cursor].fps;
	if (fps > 0.0f)
	{
		m_currentAnimFrame += (now - m_lastAnimTime) * (fps / (Real)MSEC_PER_SECOND);
		while (m_currentAnimFrame >= frames)
		{
			m_currentAnimFrame -= frames;
		}
	}
	m_lastAnimTime = now;

	return static_cast<Int>(m_currentAnimFrame);
}

void SDL3Mouse::drawFallbackCursor(MouseCursor cursor)
{
	if (!GgcFlags::Enabled(GgcFlag_SdlSoftwareCursor))
	{
		return;
	}

	if (TheDisplay == nullptr)
	{
		return;
	}

	// Optional diagnostic renderer.
	if (cursor != NORMAL && cursor != ARROW)
	{
		return;
	}

	const Int x = m_currMouse.pos.x;
	const Int y = m_currMouse.pos.y;
	const UnsignedInt shadow = 0xFF000000;
	const UnsignedInt white = 0xFFFFFFFF;

	TheDisplay->drawLine(x + 1, y + 1, x + 1, y + 20, 1.0f, shadow);
	TheDisplay->drawLine(x + 1, y + 1, x + 13, y + 13, 1.0f, shadow);
	TheDisplay->drawLine(x + 13, y + 13, x + 7, y + 13, 1.0f, shadow);
	TheDisplay->drawLine(x + 7, y + 13, x + 11, y + 21, 1.0f, shadow);
	TheDisplay->drawLine(x + 11, y + 21, x + 7, y + 23, 1.0f, shadow);
	TheDisplay->drawLine(x + 7, y + 23, x + 3, y + 15, 1.0f, shadow);
	TheDisplay->drawLine(x + 3, y + 15, x + 1, y + 20, 1.0f, shadow);

	TheDisplay->drawLine(x, y, x, y + 19, 1.0f, white);
	TheDisplay->drawLine(x, y, x + 12, y + 12, 1.0f, white);
	TheDisplay->drawLine(x + 12, y + 12, x + 6, y + 12, 1.0f, white);
	TheDisplay->drawLine(x + 6, y + 12, x + 10, y + 20, 1.0f, white);
	TheDisplay->drawLine(x + 10, y + 20, x + 6, y + 22, 1.0f, white);
	TheDisplay->drawLine(x + 6, y + 22, x + 2, y + 14, 1.0f, white);
	TheDisplay->drawLine(x + 2, y + 14, x, y + 19, 1.0f, white);
}

void SDL3Mouse::logCursorLookup(MouseCursor cursor, const Image *image, TextureClass *texture)
{
	if (!GgcFlags::Enabled(GgcFlag_CursorDiag))
	{
		return;
	}

	static MouseCursor s_lastCursor = INVALID_MOUSE_CURSOR;
	static const Image *s_lastImage = reinterpret_cast<const Image *>(-1);
	static TextureClass *s_lastTexture = reinterpret_cast<TextureClass *>(-1);
	if (cursor == s_lastCursor && image == s_lastImage && texture == s_lastTexture)
	{
		return;
	}
	s_lastCursor = cursor;
	s_lastImage = image;
	s_lastTexture = texture;

	FILE *file = std::fopen("ggc_cursor_diag.txt", "a");
	if (file == nullptr)
	{
		return;
	}
	std::fprintf(file,
		"cursor=%d name=%s imageName=%s textureName=%s image=%p texture=%p textureFile=%s init=%d size=%dx%d sdl=%p\n",
		static_cast<Int>(cursor),
		m_cursorInfo[cursor].cursorName.str(),
		m_cursorInfo[cursor].imageName.str(),
		m_cursorInfo[cursor].textureName.str(),
		static_cast<const void *>(image),
		static_cast<void *>(texture),
		texture != nullptr ? texture->Get_Texture_Name().str() : "",
		texture != nullptr ? texture->Is_Initialized() : 0,
		texture != nullptr ? texture->Get_Width() : 0,
		texture != nullptr ? texture->Get_Height() : 0,
		static_cast<void *>(getSDLColorCursor(cursor, getCursorTextureFrame(cursor))));
	std::fclose(file);
}

void SDL3Mouse::syncSystemCursorVisibility()
{
	if (GgcFlags::Enabled(GgcFlag_SdlOsCursor))
	{
		SDL_ShowCursor();
		return;
	}

	if (!m_visible || m_currentCursor == NONE)
	{
		SDL_HideCursor();
		return;
	}

	const Int frame = getCursorTextureFrame(m_currentCursor);
	SDL_Cursor *cursor = getSDLColorCursor(m_currentCursor, frame);
	if (cursor != nullptr)
	{
		if (m_currentCursor != m_lastAppliedSDLCursor || frame != m_lastAppliedSDLFrame)
		{
			SDL_SetCursor(cursor);
			m_lastAppliedSDLCursor = m_currentCursor;
			m_lastAppliedSDLFrame = frame;
		}
		SDL_ShowCursor();
		return;
	}

	SDL_HideCursor();
}

void SDL3Mouse::pushEvent(const MouseIO &event)
{
	m_buffer[m_nextFreeIndex] = event;
	m_nextFreeIndex = (m_nextFreeIndex + 1) % NUM_MOUSE_EVENTS;
	if (m_nextFreeIndex == m_nextGetIndex)
	{
		m_nextGetIndex = (m_nextGetIndex + 1) % NUM_MOUSE_EVENTS;
	}
}

#endif
