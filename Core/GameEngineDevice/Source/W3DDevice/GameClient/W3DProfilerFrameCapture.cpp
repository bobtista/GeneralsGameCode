/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2026 TheSuperHackers
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

#ifdef PROFILER_ENABLED

#include "../../../Include/W3DDevice/GameClient/W3DProfilerFrameCapture.h"

#include "WW3D2/IRenderBackend.h"
#include "WW3D2/RenderBackend.h"
#include "WW3D2/ww3d.h"

W3DProfilerFrameCapture::W3DProfilerFrameCapture()
{
}

W3DProfilerFrameCapture::~W3DProfilerFrameCapture()
{
}

bool W3DProfilerFrameCapture::ShouldReuseLastCapture(UnsignedInt currentTimeMs) const
{
	return PROFILER_FRAME_IMAGE_INTERVAL_MS > 0
		&& currentTimeMs - m_lastCaptureTimeMs < PROFILER_FRAME_IMAGE_INTERVAL_MS
		&& !m_lastCapturePixels.empty();
}

void W3DProfilerFrameCapture::Capture(UnsignedInt displayWidth, UnsignedInt displayHeight)
{
	if (!PROFILER_IS_CONNECTED)
		return;

	// the profiler expects an image every render frame. resend the last capture if we're inside the capture interval.
	const UnsignedInt currentTimeMs = WW3D::Get_Logic_Time_Milliseconds();
	if (ShouldReuseLastCapture(currentTimeMs))
	{
		PROFILER_FRAME_IMAGE(m_lastCapturePixels.data(), PROFILER_FRAME_IMAGE_SIZE, m_lastCaptureHeight, 0, false);
		return;
	}

	if (g_renderBackend == nullptr)
		return;

	m_lastCapturePixels.resize(PROFILER_FRAME_IMAGE_SIZE * PROFILER_FRAME_IMAGE_SIZE * 4);
	UnsignedInt capturedWidth = 0;
	UnsignedInt capturedHeight = 0;
	if (!g_renderBackend->Capture_Back_Buffer_RGBA(
			displayWidth,
			displayHeight,
			PROFILER_FRAME_IMAGE_SIZE,
			m_lastCapturePixels.data(),
			static_cast<UnsignedInt>(m_lastCapturePixels.size()),
			&capturedWidth,
			&capturedHeight))
	{
		m_lastCapturePixels.clear();
		return;
	}

	m_lastCaptureHeight = capturedHeight;
	m_lastCapturePixels.resize(static_cast<size_t>(capturedWidth) * capturedHeight * 4);
	PROFILER_FRAME_IMAGE(m_lastCapturePixels.data(), capturedWidth, capturedHeight, 0, false);
	m_lastCaptureTimeMs = currentTimeMs;
}

#endif // PROFILER_ENABLED
