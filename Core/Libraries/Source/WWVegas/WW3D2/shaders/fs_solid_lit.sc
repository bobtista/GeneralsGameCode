$input v_color0

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4D.1 lit solid
// fragment shader. Outputs the interpolated vertex diffuse directly -
// equivalent to D3DTOP_SELECTARG1(D3DTA_DIFFUSE) with no texture stage.
// See PHASE4_INVENTORY.md shader bucket C.

#include <bgfx_shader.sh>

void main()
{
	gl_FragColor = v_color0;
}
