$input v_color0, v_texcoord0

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4D.1 alpha-tested
// lit textured fragment shader. Same as fs_textured_lit but with a
// discard when the sampled alpha falls below u_atestParams.x. Used by
// the ATest* presets (trees, fences, see-through foliage). See
// PHASE4_INVENTORY.md shader bucket D.
//
// u_atestParams: x = reference in [0,1]. The engine should write
// ShaderClass::Get_Alpha_Reference() normalized by 255 into .x.
//
// Note: deliberately NOT named u_alphaRef - bgfx_shader.sh has an
// internal u_alphaRef4 from its legacy state-based alpha test path,
// and shaderc rewrites uniforms ending in vec4 size, causing a
// "redefinition of u_alphaRef4" error in the generated HLSL.

#include <bgfx_shader.sh>

SAMPLER2D(s_tex0, 0);
uniform vec4 u_atestParams;

void main()
{
	vec4 texColor = texture2D(s_tex0, v_texcoord0);
	vec4 lit      = texColor * v_color0;
	if (lit.a < u_atestParams.x)
	{
		discard;
	}
	gl_FragColor = lit;
}
