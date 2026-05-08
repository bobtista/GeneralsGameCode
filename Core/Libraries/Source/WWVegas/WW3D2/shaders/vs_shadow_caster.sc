$input  a_position

// TheSuperHackers @refactor bobtista 16/04/2026 Phase 4I.2 shadow map
// caster pass. Use bgfx's built-in model-view-projection composition for
// the light view, matching the normal scene transform convention.

#include <bgfx_shader.sh>

void main()
{
	gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
}
