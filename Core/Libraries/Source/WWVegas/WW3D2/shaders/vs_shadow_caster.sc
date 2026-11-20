$input a_position, a_texcoord0
$output v_sceneDepth, v_texcoord0

// TheSuperHackers @feature bobtista 16/06/2026 Shadow-map caster vertex shader.
// Like vs_scene_depth, but also forwards the base UV so the fragment shader can
// alpha-test cutout geometry (infantry, foliage) into the shadow map.

#include <bgfx_shader.sh>

void main()
{
	vec4 clip = mul(u_modelViewProj, vec4(a_position, 1.0));
	gl_Position = clip;
	v_sceneDepth = vec4(clamp(clip.z / max(clip.w, 1e-6), 0.0, 1.0), 0.0, 0.0, 0.0);
	v_texcoord0 = a_texcoord0;
}
