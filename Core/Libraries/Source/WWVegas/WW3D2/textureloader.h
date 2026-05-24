/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
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

/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : DX8 Texture Manager                                          *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/textureloader.h                            $*
 *                                                                                             *
 *              Original Author:: vss_sync                                                   *
 *                                                                                             *
 *                       Author : Kenny Mitchell                                               *
 *                                                                                             *
 *                     $Modtime:: 06/27/02 1:27p                                              $*
 *                                                                                             *
 *                    $Revision:: 2                                                           $*
 *                                                                                             *
 * 06/27/02 KM Texture class abstraction																			*
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#pragma once

#include "always.h"
#include "texture.h"

class StringClass;
class TextureLoadTaskClass;
class TextureLoadTaskListClass;

class TextureLoader
{
public:
	static void Init();
	static void Deinit();

	// Modify given texture size to nearest valid size on current hardware.
	static void Validate_Texture_Size(unsigned& width, unsigned& height, unsigned& depth);

	static void	Request_Thumbnail(TextureBaseClass* tc);
	static bool Load_Surface_Image_Immediate(
		const char *filename,
		WW3DFormat texture_format,
		bool allow_compression,
		SurfaceClass::SurfaceImageData &image);

	// Adds a loading task to the system. The task if processed in a separate
	// thread as soon as possible. The task will appear in finished tasks list
	// when it's been completed. The texture will be refreshed on the next
	// update call after appearing to the finished tasks list.
	static void Request_Background_Loading(TextureBaseClass* tc);

	// Textures can only be created and locked by the main thread so this function sends a request to the texture
	// handling system to load the texture immediatelly next time it enters the main thread. If this function
	// is called from the main thread the texture is loaded immediatelly.
	static void Request_Foreground_Loading(TextureBaseClass* tc);

	static void	Flush_Pending_Load_Tasks();
	static void Update(void(*network_callback)() = nullptr);

	// returns true if current thread of execution is allowed to make DX8 calls.
	static bool Is_Main_Render_Thread();

	static void Suspend_Texture_Load();
	static void Continue_Texture_Load();

	static void Set_Texture_Inactive_Override_Time(int time_ms) {TextureInactiveOverrideTime = time_ms;}

private:
	friend class TextureBaseClass;

	static void Delete_Texture_Load_Tasks(TextureBaseClass *tc);
	static void Process_Foreground_Load			(TextureLoadTaskClass *task);
	static void Process_Foreground_Thumbnail	(TextureLoadTaskClass *task);

	static void Begin_Load_And_Queue				(TextureLoadTaskClass *task);
	static void Load_Thumbnail						(TextureBaseClass *tc);

	static bool TextureLoadSuspended;

	// The time in ms before a texture is thrown out.
	// The default is zero.  The scripted movies set this to reduce texture stalls in movies.
	static int	TextureInactiveOverrideTime;
};
