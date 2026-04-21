// TheSuperHackers @refactor bobtista 15/04/2026 Phase 4I stencil shadow
// volume fragment shader. The engine disables color writes for the
// volume pass (Set_Color_Write_Mask(0)) so this output is discarded at
// the output-merger; what matters is stencil increment/decrement, which
// is driven by pipeline state, not the shader. We still need to emit
// something for bgfx.

#include <bgfx_shader.sh>

void main()
{
	gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
}
