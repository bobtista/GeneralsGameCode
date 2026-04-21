$input  a_position, a_color0
$output v_color0

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4D.1 lit solid
// vertex shader. Used by the *Solid presets which have GRADIENT_MODULATE
// + TEXTURING_DISABLE - the engine writes the fixed-function lit color
// into the vertex diffuse and we just forward it. Identical to
// vs_passthrough but separated so the program lookup can stay 1:1 with
// the preset bucket. See PHASE4_INVENTORY.md shader bucket C.

#include <bgfx_shader.sh>

void main()
{
	gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
	v_color0    = a_color0;
}
