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
	v_sceneDepth = vec4(clamp(clip.z / clip.w, 0.0, 1.0), 0.0, 0.0, 0.0);
}
