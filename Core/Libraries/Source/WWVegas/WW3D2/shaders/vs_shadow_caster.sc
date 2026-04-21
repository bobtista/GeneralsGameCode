$input  a_position

// TheSuperHackers @refactor bobtista 16/04/2026 Phase 4I.2 shadow map
// caster pass. Uses the SAME u_shadowLightViewProj uniform as the
// receiver (vs_uber) to guarantee bit-identical depth values —
// bgfx's auto-composed u_modelViewProj differs due to float
// matrix multiplication associativity, causing self-shadow.

#include <bgfx_shader.sh>

uniform mat4 u_shadowLightViewProj;

void main()
{
	gl_Position = mul(u_shadowLightViewProj, vec4(a_position, 1.0));
}
