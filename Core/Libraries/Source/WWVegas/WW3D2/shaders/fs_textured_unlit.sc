$input v_texcoord0

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4D.1 unlit textured
// fragment shader. Samples stage 0 straight to output. Used by
// Opaque2D / OpaqueSprite and friends (GRADIENT_DISABLE + TEXTURING_ENABLE
// presets). See PHASE4_INVENTORY.md shader bucket B.

#include <bgfx_shader.sh>

SAMPLER2D(s_tex0, 0);

void main()
{
	gl_FragColor = texture2D(s_tex0, v_texcoord0);
}
