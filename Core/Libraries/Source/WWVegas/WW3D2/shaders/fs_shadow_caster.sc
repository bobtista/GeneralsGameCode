$input v_sceneDepth, v_texcoord0

// TheSuperHackers @feature bobtista 16/06/2026 Shadow-map caster fragment shader.
// Writes normalized light-space depth (R32F) like fs_scene_depth, but first applies
// the same alpha test the main pass uses, so alpha-tested cutout geometry (infantry,
// foliage) casts its real silhouette instead of a solid bounding quad. Opaque casters
// pass u_atestParams.y <= 0.5 and skip the test entirely.

#include <bgfx_shader.sh>

SAMPLER2D(s_tex0, 0);
uniform vec4 u_atestParams; // x = alpha ref, y = func id (> 0.5 = test active)

void main()
{
	if (u_atestParams.y > 0.5)
	{
		float a = texture2D(s_tex0, v_texcoord0).a;
		if (a < u_atestParams.x)
		{
			discard;
		}
	}
	gl_FragColor = vec4(v_sceneDepth.x, v_sceneDepth.x, v_sceneDepth.x, 1.0);
}
