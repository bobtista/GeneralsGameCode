$input v_texcoord0

// TheSuperHackers @feature bobtista 15/06/2026 Trivial fullscreen texture copy.
// Used to resolve the (possibly MSAA) scene color into the single-sample smudge
// snapshot, since bgfx::blit cannot read a multisampled source.

#include <bgfx_shader.sh>

SAMPLER2D(s_tex0, 0);

void main()
{
	gl_FragColor = texture2D(s_tex0, v_texcoord0);
}
