$input v_color0, v_texcoord0, v_normal

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4D.1 alpha-tested
// lit textured fragment shader. Same as fs_textured_lit but discards
// fragments whose sampled alpha falls below u_atestParams.x. Used by
// the ATest* presets (trees, fences, see-through foliage). Phase 4G.3
// / 4G.4 add stages 1-3; Phase 4G.9 adds lighting + material diffuse.
//
// u_atestParams: x = reference in [0,1]. The engine writes
// ShaderClass::Get_Alpha_Reference() normalized by 255 into .x.
//
// Note: deliberately NOT named u_alphaRef - bgfx_shader.sh has an
// internal u_alphaRef4 from its legacy state-based alpha test path,
// and shaderc rewrites uniforms ending in vec4 size, causing a
// "redefinition of u_alphaRef4" error in the generated HLSL.

#include <bgfx_shader.sh>

SAMPLER2D(s_tex0, 0);
SAMPLER2D(s_tex1, 1);
SAMPLER2D(s_tex2, 2);
SAMPLER2D(s_tex3, 3);
uniform vec4 u_matDiffuse;
uniform vec4 u_atestParams;

void main()
{
	vec4 tex0 = texture2D(s_tex0, v_texcoord0);
	vec4 tex1 = texture2D(s_tex1, v_texcoord0);
	vec4 tex2 = texture2D(s_tex2, v_texcoord0);
	vec4 tex3 = texture2D(s_tex3, v_texcoord0);

	vec3  sunDir   = normalize(vec3(0.35, 0.55, 0.75));
	float ambient  = 0.45;
	float nDotL    = max(0.0, dot(normalize(v_normal), sunDir));
	float lighting = min(1.0, ambient + nDotL * 0.75);

	vec4 base   = tex0 * tex1 * tex2 * tex3;
	vec4 tinted = base * v_color0 * u_matDiffuse;
	if (tinted.a < u_atestParams.x)
	{
		discard;
	}
	gl_FragColor = vec4(tinted.rgb * lighting, tinted.a);
}
