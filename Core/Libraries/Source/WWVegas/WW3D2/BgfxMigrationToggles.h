#pragma once

enum class BgfxMigrationToggle
{
	TextureOwnership,
	SurfaceOwnership,
	RenderTargets,
	BufferOwnership,
	SemanticState,
};

const char *Get_Bgfx_Migration_Toggle_Name(BgfxMigrationToggle toggle);
const char *Get_Bgfx_Migration_Toggle_Env(BgfxMigrationToggle toggle);
bool Is_Bgfx_Migration_Toggle_Enabled(BgfxMigrationToggle toggle);
