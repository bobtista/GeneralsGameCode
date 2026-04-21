$input  a_position, a_normal, a_color0, a_texcoord0, a_texcoord1
$output v_color0, v_texcoord0, v_texcoord1, v_normal

#include <bgfx_shader.sh>

void main()
{
	gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));

	vec4 c = a_color0.bgra;
	if (c.r + c.g + c.b < 0.001)
	{
		c = vec4(1.0, 1.0, 1.0, 1.0);
	}
	v_color0    = c;
	v_texcoord0 = a_texcoord0;
	v_texcoord1 = a_texcoord1;
	v_normal    = mul(u_model[0], vec4(a_normal, 0.0)).xyz;
}
