$input  a_position, a_color0
$output v_color0

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4B.2 trivial
// passthrough vertex shader. Transforms position by the bgfx-provided
// model/view/projection matrix and forwards vertex color.

#include <bgfx_shader.sh>

void main()
{
	gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
	v_color0 = a_color0;
}
