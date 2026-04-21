$input  a_position, a_normal, a_color0, a_texcoord0
$output v_color0, v_texcoord0, v_normal

#include <bgfx_shader.sh>

void main()
{
	gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));

	// D3D8 vertex diffuse is D3DCOLOR (BGRA byte order) but bgfx Color0
	// expects RGBA. Swap R and B to correct the channel mapping.
	// Check RGB only (not alpha) for the zero-detection: D3D11 defaults
	// missing Color0 to (0,0,0,1), so including alpha makes the sum > 0
	// and the white fallback never fires for meshes without a diffuse
	// attribute (infantry, vehicles).
	vec4 c = a_color0.bgra;
	if (c.r + c.g + c.b < 0.001)
	{
		c = vec4(1.0, 1.0, 1.0, 1.0);
	}
	v_color0    = c;
	v_texcoord0 = a_texcoord0;
	v_normal    = mul(u_model[0], vec4(a_normal, 0.0)).xyz;
}
