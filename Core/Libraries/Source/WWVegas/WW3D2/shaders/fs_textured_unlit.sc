$input v_texcoord0

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4D.1 unlit textured
// fragment shader. Samples stages 0-3 straight to output. Phase 4G.3 /
// 4G.4 added stages 1-3; Phase 4G.9 adds the material diffuse tint so
// UI sprites and unlit-preset meshes still pick up team colors.
// Unused stages default to a 1x1 white texture so single-texture draws
// are unaffected.

#include <bgfx_shader.sh>

SAMPLER2D(s_tex0, 0);
SAMPLER2D(s_tex1, 1);
SAMPLER2D(s_tex2, 2);
SAMPLER2D(s_tex3, 3);
uniform vec4 u_matDiffuse;

void main()
{
	vec4 tex0 = texture2D(s_tex0, v_texcoord0);
	vec4 tex1 = texture2D(s_tex1, v_texcoord0);
	vec4 tex2 = texture2D(s_tex2, v_texcoord0);
	vec4 tex3 = texture2D(s_tex3, v_texcoord0);
	gl_FragColor = tex0 * tex1 * tex2 * tex3 * u_matDiffuse;
}
