/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
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

// TheSuperHackers @diag bobtista 04/06/2026 Cross-TU render-frame attribution.
// Lightweight phase timers usable from any engine TU (W3DScene, particle system,
// sorting renderer, terrain) so we can attribute the per-frame render CPU top-down.
// Accumulators + the per-frame reset and CSV emit live in BgfxBackend.cpp; the
// numbers are written alongside the existing frame-timing CSV when it is active.
// Accumulation is a couple of QueryPerformanceCounter reads per scope (cheap) and
// always on; only the CSV emit is gated by GGC_BGFX_FRAME_TIMING_*.

#pragma once

namespace GGCRenderProfile
{
	enum Phase
	{
		// Top-level sequential buckets inside W3DDisplay::draw. These do NOT overlap and
		// sum (plus the unattributed remainder) to FRAME_DRAW.
		FRAME_DRAW = 0,     // whole W3DDisplay::draw (the real per-frame envelope)
		UPDATE_VIEWS,       // updateViews(): per-view recompute + visible-terrain refresh
		PARTICLE_UPDATE,    // TheParticleSystemManager->update(): particle simulation/spawn
		RTT,                // water reflection + projected-shadow render-target passes
		DRAW_VIEWS,         // drawViews(): the on-screen scene draw (contains RENDER_TOTAL)
		UI_DRAW,            // TheInGameUI->DRAW() + mouse draw
		END_RENDER,         // WW3D::End_Render (device flush / present / bgfx::frame)

		// Nested detail inside DRAW_VIEWS (and partly RTT) — reported but NOT subtracted
		// from the top-level remainder, since they are subsets of the buckets above.
		RENDER_TOTAL,       // RTS3DScene::Render (summed across on-screen + RTT passes)
		TRAVERSAL,          // Customized_Render: scene walk + per-object dispatch
		MESH_FLUSH,         // DX8MeshRenderer::Flush (opaque/rigid mesh submission)
		SORT_FLUSH,         // SortingRendererClass::Flush (sorted translucents)
		PARTICLES,          // DoParticles (particle render build, subset of RENDER_TOTAL)
		TERRAIN,            // terrain heightmap render (nested detail)
		PHASE_COUNT
	};

	void Begin(Phase phase);
	void End(Phase phase);
	// Snapshot+reset the accumulators at a frame boundary (call at the top of the
	// per-frame draw, before any scope of the new frame opens). The CSV emit reads
	// the snapshot, so scopes that enclose the emit still report complete times.
	void EndFrame();

	struct Scope
	{
		Phase m_phase;
		explicit Scope(Phase phase) : m_phase(phase) { Begin(phase); }
		~Scope() { End(m_phase); }
	};
}

#define GGC_RPROFILE_CONCAT_(a, b) a##b
#define GGC_RPROFILE_CONCAT(a, b) GGC_RPROFILE_CONCAT_(a, b)
#define GGC_RPROFILE(phase) \
	GGCRenderProfile::Scope GGC_RPROFILE_CONCAT(_ggc_rprof_, __LINE__)(GGCRenderProfile::phase)
