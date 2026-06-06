/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "BgfxMigrationToggles.h"

#include <cctype>
#include <cstdlib>

namespace
{

bool Is_False_Env_Value(const char *value)
{
	if (value == nullptr || value[0] == '\0') {
		return true;
	}

	char normalized[8] = {};
	unsigned int i = 0;
	for (; value[i] != '\0' && i + 1 < sizeof(normalized); ++i) {
		normalized[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(value[i])));
	}

	return (normalized[0] == '0' && normalized[1] == '\0')
		|| (normalized[0] == 'n' && normalized[1] == 'o' && normalized[2] == '\0')
		|| (normalized[0] == 'o' && normalized[1] == 'f' && normalized[2] == 'f' && normalized[3] == '\0')
		|| (normalized[0] == 'f' && normalized[1] == 'a' && normalized[2] == 'l' && normalized[3] == 's'
			&& normalized[4] == 'e' && normalized[5] == '\0');
}

bool Toggle_Default_Enabled(BgfxMigrationToggle toggle)
{
	switch (toggle) {
		case BgfxMigrationToggle::TextureOwnership:
		case BgfxMigrationToggle::SurfaceOwnership:
		case BgfxMigrationToggle::RenderTargets:
		case BgfxMigrationToggle::BufferOwnership:
			return true;
	}
	return false;
}

} // namespace

const char *Get_Bgfx_Migration_Toggle_Name(BgfxMigrationToggle toggle)
{
	switch (toggle) {
		case BgfxMigrationToggle::TextureOwnership:
			return "texture ownership";
		case BgfxMigrationToggle::SurfaceOwnership:
			return "surface ownership";
		case BgfxMigrationToggle::RenderTargets:
			return "render targets";
		case BgfxMigrationToggle::BufferOwnership:
			return "buffer ownership";
	}
	return "unknown";
}

const char *Get_Bgfx_Migration_Toggle_Env(BgfxMigrationToggle toggle)
{
	switch (toggle) {
		case BgfxMigrationToggle::TextureOwnership:
			return "GGC_BGFX_NEW_TEXTURE_OWNERSHIP";
		case BgfxMigrationToggle::SurfaceOwnership:
			return "GGC_BGFX_NEW_SURFACE_OWNERSHIP";
		case BgfxMigrationToggle::RenderTargets:
			return "GGC_BGFX_NEW_RENDER_TARGETS";
		case BgfxMigrationToggle::BufferOwnership:
			return "GGC_BGFX_NEW_BUFFER_OWNERSHIP";
	}
	return "";
}

bool Is_Bgfx_Migration_Toggle_Enabled(BgfxMigrationToggle toggle)
{
#if defined(GGC_BGFX_STANDALONE)
	switch (toggle) {
		case BgfxMigrationToggle::TextureOwnership:
		case BgfxMigrationToggle::SurfaceOwnership:
		case BgfxMigrationToggle::RenderTargets:
		case BgfxMigrationToggle::BufferOwnership:
			return true;
	}
#endif
	const char *value = std::getenv(Get_Bgfx_Migration_Toggle_Env(toggle));
	if (value == nullptr) {
		return Toggle_Default_Enabled(toggle);
	}
	return !Is_False_Env_Value(value);
}
