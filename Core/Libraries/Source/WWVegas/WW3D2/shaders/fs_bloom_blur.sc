$input v_texcoord0

// TheSuperHackers @feature bobtista 15/06/2026 Separable Gaussian blur for bloom.
// Run once horizontally then once vertically (direction from u_bloomBlurDir) over
// the half-res bright-pass target.

#include <bgfx_shader.sh>

SAMPLER2D(s_tex0, 0);

uniform vec4 u_bloomBlurDir; // xy = texel step in the blur direction

void main()
{
	// 5-tap Gaussian using linear-sampling offsets/weights (9-tap kernel folded to 5 fetches).
	vec2 d = u_bloomBlurDir.xy;
	vec3 sum = texture2D(s_tex0, v_texcoord0).rgb * 0.227027;
	sum += texture2D(s_tex0, v_texcoord0 + d * 1.3846).rgb * 0.316216;
	sum += texture2D(s_tex0, v_texcoord0 - d * 1.3846).rgb * 0.316216;
	sum += texture2D(s_tex0, v_texcoord0 + d * 3.2308).rgb * 0.070270;
	sum += texture2D(s_tex0, v_texcoord0 - d * 3.2308).rgb * 0.070270;
	gl_FragColor = vec4(sum, 1.0);
}
