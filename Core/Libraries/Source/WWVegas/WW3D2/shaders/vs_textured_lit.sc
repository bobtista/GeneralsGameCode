$input  a_position, a_color0, a_texcoord0
$output v_color0, v_texcoord0

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4D.1 lit textured
// vertex shader. Transforms position by the bgfx model/view/projection
// matrix, forwards vertex diffuse (which carries the fixed-function lit
// color from the engine side) and texcoord 0.

#include <bgfx_shader.sh>

void main()
{
	gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
	v_color0    = a_color0;
	v_texcoord0 = a_texcoord0;
}
