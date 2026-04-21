$input  a_position, a_texcoord0
$output v_texcoord0

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4D.1 unlit textured
// vertex shader. Used by 2D/Sprite presets - no vertex lighting, just
// position transform and texcoord passthrough. See PHASE4_INVENTORY.md
// shader bucket B.

#include <bgfx_shader.sh>

void main()
{
	gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
	v_texcoord0 = a_texcoord0;
}
