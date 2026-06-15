$input v_texcoord0

// TheSuperHackers @feature bobtista 15/06/2026 Bloom bright-pass. Extracts the
// portion of each pixel above a luma threshold into a half-res target, which is
// then blurred and added back over the scene in the composite pass.

#include <bgfx_shader.sh>

SAMPLER2D(s_tex0, 0);

uniform vec4 u_bloomParams; // x = threshold, y = intensity (unused here)

#define LUMA_WEIGHTS vec3(0.299, 0.587, 0.114)

void main()
{
	vec3 c = texture2D(s_tex0, v_texcoord0).rgb;
	float luma = dot(c, LUMA_WEIGHTS);
	float contrib = max(luma - u_bloomParams.x, 0.0);
	vec3 bright = c * (contrib / max(luma, 0.0001));
	gl_FragColor = vec4(bright, 1.0);
}
