$input  a_position, a_color0, a_texcoord0
$output v_color0, v_texcoord0

// TheSuperHackers @feature bobtista 27/04/2026 Dedicated bgfx
// smudge/heat-haze vertex shader. The legacy smudge geometry already
// carries screen-copy UVs and a radial alpha mask in vertex diffuse.

#include <bgfx_shader.sh>

void main()
{
	gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
	v_color0 = a_color0.bgra;
	v_texcoord0 = a_texcoord0;
}
