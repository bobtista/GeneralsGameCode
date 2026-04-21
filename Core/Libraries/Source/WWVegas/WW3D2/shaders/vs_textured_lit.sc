$input  a_position, a_normal, a_color0, a_texcoord0
$output v_color0, v_texcoord0, v_normal

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4D.1 lit textured
// vertex shader. Transforms position by the bgfx model/view/projection
// matrix, forwards vertex diffuse (carries team colors and prelit
// vertex brightness the engine bakes in), texcoord 0, and the world-
// space normal for the fragment shader to do a simple N.L.
//
// Phase 4G.8 fallback: if the bound FVF does not carry a diffuse
// attribute (e.g. 0x112 = XYZ|NORMAL|TEX1 used by rigid mesh containers
// for infantry/vehicles), bgfx defaults a_color0 to zero. Multiplying
// tex*0 produces black invisible meshes. Detect the zero case and swap
// in an opaque white so the texture shows through untinted, then the
// fragment shader modulates by u_matDiffuse for the team color. Meshes
// with a real diffuse attribute (terrain 0x242, etc.) are unaffected.

#include <bgfx_shader.sh>

void main()
{
	gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
	// D3D8 vertex diffuse is D3DCOLOR (BGRA byte order) but bgfx Color0
	// expects RGBA. Swap R and B to correct the channel mapping.
	// Check RGB only (not alpha) for the zero-detection: D3D11 defaults
	// missing Color0 to (0,0,0,1), so including alpha makes the sum > 0
	// and the white fallback never fires for meshes without a diffuse
	// attribute (infantry, vehicles).
	vec4 c = a_color0.bgra;
	if (c.r + c.g + c.b < 0.001)
	{
		c = vec4(1.0, 1.0, 1.0, 1.0);
	}
	c.a = 1.0;
	v_color0    = c;
	v_texcoord0 = a_texcoord0;
	v_normal    = mul(u_model[0], vec4(a_normal, 0.0)).xyz;
}
