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
	float threshold = u_bloomParams.x;
	// TheSuperHackers @tweak bobtista 15/06/2026 Soft-knee bright-pass so bloom
	// ramps in gently around the threshold instead of a hard cutoff that makes
	// large bright areas (sunlit terrain) glow all at once and wash the scene out.
	float knee = max(threshold * 0.5, 0.0001);
	float soft = clamp(luma - threshold + knee, 0.0, 2.0 * knee);
	soft = (soft * soft) / (4.0 * knee + 0.0001);
	float contrib = max(soft, luma - threshold);
	contrib = contrib / max(luma, 0.0001);
	gl_FragColor = vec4(c * contrib, 1.0);
}
