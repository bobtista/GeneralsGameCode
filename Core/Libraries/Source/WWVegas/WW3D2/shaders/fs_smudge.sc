$input v_color0, v_texcoord0

// TheSuperHackers @feature bobtista 27/04/2026 Dedicated bgfx
// smudge/heat-haze fragment shader. Samples the scene-color snapshot
// directly instead of routing through the fixed-function uber material path.

#include <bgfx_shader.sh>

SAMPLER2D(s_tex0, 0);

void main()
{
	float mask = clamp(v_color0.a, 0.0, 1.0);
	if (mask <= 0.003)
	{
		discard;
	}

	vec2 uv = clamp(v_texcoord0, vec2(0.0, 0.0), vec2(1.0, 1.0));
	vec3 scene = texture2D(s_tex0, uv).rgb;
	gl_FragColor = vec4(scene, mask);
}
