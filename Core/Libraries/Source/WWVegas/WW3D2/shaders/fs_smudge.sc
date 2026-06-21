$input v_color0, v_texcoord0

// TheSuperHackers @feature bobtista 27/04/2026 Dedicated bgfx
// smudge/heat-haze fragment shader. Samples the scene-color snapshot at the
// CPU-displaced UV and writes it with the radial vertex-alpha mask, so the
// host alpha blend reproduces the DX8 fixed-function heat-haze lens.

#include <bgfx_shader.sh>

SAMPLER2D(s_tex0, 0);
uniform vec4 u_smudgeClip;

void main()
{
	float mask = clamp(v_color0.a, 0.0, 1.0);
	if (mask <= 0.003)
	{
		discard;
	}

	vec2 clipUV = clamp(u_smudgeClip.zw, vec2(0.0, 0.0), vec2(1.0, 1.0));
	vec2 uv = clamp(v_texcoord0, vec2(0.0, 0.0), clipUV);
	vec3 scene = texture2D(s_tex0, uv).rgb;
	// Warm modulation matching the DX8 0xffeedd smudge diffuse tint.
	vec3 warmTint = vec3(1.0, 0.9333, 0.8667);
	gl_FragColor = vec4(scene * warmTint, mask);
}
