$input  a_position, a_normal, a_color0, a_texcoord0
$output v_color0, v_texcoord0, v_texcoord1, v_normal, v_lightspace

#include <bgfx_shader.sh>

// TheSuperHackers @refactor bobtista 14/04/2026 Phase 4H port of
// Trees.nvv (DX8 vs_1_1). The engine repurposes the vertex normal
// slot to carry grass/tree animation data:
//   a_normal.x = sway table index [0..MAX_SWAY_TYPES]
//   a_normal.y = color scale factor (darkens vertex diffuse)
//   a_normal.z = base Z coordinate (height=0 point of the blade)
//
// u_swayTable packs c[8..8+MAX_SWAY_TYPES] from the original shader
// (11 entries: index 0 = noSway, indices 1..10 = per-wave offsets).
// u_shroudOffset / u_shroudScale replace c32 / c33 — they compute
// UV1 so the shroud (fog-of-war) texture can be sampled.

#define MAX_SWAY_TYPES_PLUS1 11

uniform vec4 u_swayTable[MAX_SWAY_TYPES_PLUS1];
uniform vec4 u_shroudOffset;
uniform vec4 u_shroudScale;
uniform mat4 u_shadowLightViewProj;

void main()
{
	// Height above the base of the blade — original: r2 = v0 - v1 (zzzw swizzle)
	float height = a_position.z - a_normal.z;

	// Pick the sway vector for this blade. int() is safe: caller
	// writes small non-negative integer values into the normal's x
	// via a D3DCOLOR-style pack, so a_normal.x arrives as a float in
	// [0, MAX_SWAY_TYPES].
	int waveIdx = int(a_normal.x + 0.5);
	vec4 wave = u_swayTable[waveIdx];

	// Scale the sway by height and add to original position. Tops
	// sway most; bases don't move.
	vec3 swayed = a_position + height * wave.xyz;

	gl_Position = mul(u_modelViewProj, vec4(swayed, 1.0));

	// Original: oD0 = v2 * v1.yyyw (replicate color scale). In bgfx
	// the diffuse comes in as BGRA on D3D paths — keep the same
	// channel swap the uber shader uses.
	v_color0 = a_color0.bgra * a_normal.y;

	v_texcoord0 = a_texcoord0;
	// Shroud UV: (v0.xy + c32.xy) * c33.xy.
	v_texcoord1 = (a_position.xy + u_shroudOffset.xy) * u_shroudScale.xy;

	v_normal = vec3(0.0, 0.0, 1.0);  // grass billboards always face up

	v_lightspace = mul(u_shadowLightViewProj, vec4(a_position, 1.0));
}
