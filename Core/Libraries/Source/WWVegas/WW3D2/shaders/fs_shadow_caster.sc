// TheSuperHackers @refactor bobtista 16/04/2026 Phase 4I.2 shadow map
// caster fragment shader. Depth-only pass: the D16 depth buffer gets
// written by the rasterizer from gl_Position.z. Fragment shader just
// needs to exist for bgfx but writes nothing meaningful.

#include <bgfx_shader.sh>

void main()
{
	gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
}
