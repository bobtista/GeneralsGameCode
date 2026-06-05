$input  a_position
$output v_sceneDepth

// TheSuperHackers @feature bobtista 27/04/2026 Scene readable-depth
// vertex shader. Uses the current bgfx model-view-projection transform
// so duplicated opaque world draws populate a sampleable depth texture.

#include <bgfx_shader.sh>

void main()
{
	vec4 clip = mul(u_modelViewProj, vec4(a_position, 1.0));
	gl_Position = clip;
	// TheSuperHackers @bugfix bobtista 05/06/2026 Guard the divisor: near-plane-straddling
	// verts can drive clip.w to 0/negative, and clamp() does not sanitize the resulting
	// Inf/NaN. max() keeps valid w unchanged and yields a finite, clamped depth otherwise.
	v_sceneDepth = vec4(clamp(clip.z / max(clip.w, 1e-6), 0.0, 1.0), 0.0, 0.0, 0.0);
}
