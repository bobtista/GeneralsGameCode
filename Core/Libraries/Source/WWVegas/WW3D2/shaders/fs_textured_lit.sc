$input v_color0, v_texcoord0, v_normal

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4D.1 lit textured
// fragment shader. Samples stages 0-3, modulates by interpolated
// vertex diffuse, applies a hardcoded directional + ambient light so
// meshes have depth, and tints by the material diffuse (team color).
//
// Lighting model (Phase 4G.9) is intentionally minimal - one sun
// direction + ambient - because the W3D fixed-function light
// environment isn't wired into the bgfx path yet. Good enough to
// read unit silhouettes and shading at a glance.

#include <bgfx_shader.sh>

SAMPLER2D(s_tex0, 0);
SAMPLER2D(s_tex1, 1);
SAMPLER2D(s_tex2, 2);
SAMPLER2D(s_tex3, 3);
uniform vec4 u_matDiffuse;

void main()
{
	vec4 tex0 = texture2D(s_tex0, v_texcoord0);
	vec4 tex1 = texture2D(s_tex1, v_texcoord0);
	vec4 tex2 = texture2D(s_tex2, v_texcoord0);
	vec4 tex3 = texture2D(s_tex3, v_texcoord0);

	// Simple headlamp + ambient. sunDir points from the surface
	// toward the sun. Ambient raises the floor so backfaces are not
	// pitch black.
	vec3  sunDir   = normalize(vec3(0.35, 0.55, 0.75));
	float ambient  = 0.45;
	float nDotL    = max(0.0, dot(normalize(v_normal), sunDir));
	float lighting = min(1.0, ambient + nDotL * 0.75);

	vec4 base    = tex0 * tex1 * tex2 * tex3;
	vec4 tinted  = base * v_color0 * u_matDiffuse;
	gl_FragColor = vec4(tinted.rgb * lighting, tinted.a);
}
