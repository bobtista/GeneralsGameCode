$input a_position, a_texcoord0, i_data0, i_data1, i_data2, i_data3
$output v_sceneDepth, v_texcoord0

// TheSuperHackers @feature bobtista 28/06/2026 Instanced sun-shadow caster. Mirrors
// vs_shadow_caster but takes the world transform from the per-instance data, so an instanced
// opaque batch casts every instance into the shadow map in one submit. Forwards the base UV for
// alpha-tested cutout casters, same as the non-instanced path.

#include <bgfx_shader.sh>

void main()
{
	mat4 worldMtx = mtxFromCols(i_data0, i_data1, i_data2, i_data3);
	vec4 worldPos = mul(worldMtx, vec4(a_position, 1.0));
	vec4 clip = mul(u_viewProj, worldPos);
	gl_Position = clip;
	v_sceneDepth = vec4(clamp(clip.z / max(clip.w, 1e-6), 0.0, 1.0), 0.0, 0.0, 0.0);
	v_texcoord0 = a_texcoord0;
}
