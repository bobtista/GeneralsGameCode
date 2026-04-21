// TheSuperHackers @refactor bobtista 15/04/2026 Phase 4I stencil shadow
// apply fragment shader. Emits u_shadowColor straight out; the blend
// state set by the engine (SRC=DEST_COLOR, DEST=ZERO) multiplies this
// against the framebuffer, which darkens stenciled pixels uniformly.

#include <bgfx_shader.sh>

uniform vec4 u_shadowColor;

void main()
{
	gl_FragColor = u_shadowColor;
}
