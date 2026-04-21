$input  a_position

// TheSuperHackers @refactor bobtista 15/04/2026 Phase 4I stencil shadow
// volume vertex shader. Matches the DX8 reference path's SHADOW_VOLUME_FVF
// (XYZ only, 12 bytes). Transforms position through MVP, with a safety
// clamp on clip-space Z.
//
// Why clamp: RenderDoc investigation showed that certain shadow volume
// meshes (per-mesh camera-angle-dependent, not per-frame) have
// extrusion vertices placed far beyond the caster. After MVP these can
// fall outside the view frustum's depth range. D3D8 implicitly handles
// this gracefully (tight tank-shadow shapes); D3D11's strict depth
// clipping produces giant thin triangles in screen space instead.
// Clamping clip.z to the far plane (z <= w, i.e. NDC z <= 1.0) keeps
// the extruded vertex at the far plane rather than outside it,
// matching D3D8's effective behavior.
//
// u_shadowBias.x: small per-pass Z offset (unused currently, kept for
// future polygon-offset experiments).

#include <bgfx_shader.sh>

uniform vec4 u_shadowBias;

void main()
{
	vec4 clip = mul(u_modelViewProj, vec4(a_position, 1.0));
	clip.z += u_shadowBias.x * clip.w;
	// Safety clamp: keep extruded verts at/before the far plane so
	// the rasterizer sees consistent triangles regardless of how far
	// the extrusion overshoots the frustum. Preserves x/y/w so
	// screen-space XY positioning is unchanged.
	clip.z = min(clip.z, clip.w);
	gl_Position = clip;
}
