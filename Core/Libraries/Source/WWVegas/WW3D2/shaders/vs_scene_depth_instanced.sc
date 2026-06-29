$input  a_position, i_data0, i_data1, i_data2, i_data3
$output v_sceneDepth

// TheSuperHackers @feature bobtista 28/06/2026 Instanced scene-depth caster. Mirrors
// vs_scene_depth but takes the world transform from the per-instance data (mtxFromCols, like
// vs_uber_instanced) instead of u_model, so an instanced opaque batch populates the readable
// depth target for all instances in one submit.

#include <bgfx_shader.sh>

void main()
{
	mat4 worldMtx = mtxFromCols(i_data0, i_data1, i_data2, i_data3);
	vec4 worldPos = mul(worldMtx, vec4(a_position, 1.0));
	vec4 clip = mul(u_viewProj, worldPos);
	gl_Position = clip;
	v_sceneDepth = vec4(clamp(clip.z / max(clip.w, 1e-6), 0.0, 1.0), 0.0, 0.0, 0.0);
}
