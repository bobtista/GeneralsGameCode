$input  a_position

// TheSuperHackers @refactor bobtista 15/04/2026 Phase 4I stencil shadow
// volume vertex shader. Matches the DX8 reference path's SHADOW_VOLUME_FVF
// (XYZ only, 12 bytes). Transforms position through MVP and leaves hardware
// clipping to the backend. Manually clamping only clip.z distorts the
// homogeneous edge intersection and turns valid finite volumes into long
// screen-space triangles.
//
// u_shadowBias.x: small per-pass Z offset (unused currently, kept for
// future polygon-offset experiments).
// u_shadowBias.y: emulate depth clamp by clamping clip-space Z to the
// post-projection near/far range. Metal does not expose bgfx depth clamp
// in a way that preserves the original D3D8 open shadow volume behavior.

#include <bgfx_shader.sh>

uniform vec4 u_shadowBias;

void main()
{
	vec4 clip = mul(u_modelViewProj, vec4(a_position, 1.0));
	clip.z += u_shadowBias.x * clip.w;
	if (u_shadowBias.y > 0.5)
	{
		clip.z = clamp(clip.z, 0.0, clip.w);
	}
	gl_Position = clip;
}
