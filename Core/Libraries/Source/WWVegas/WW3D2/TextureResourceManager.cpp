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

#include "TextureResourceManager.h"

TextureTrackerList TextureResourceManagerClass::Managed_Textures;

void TextureResourceManagerClass::Shutdown()
{
	while (!Managed_Textures.Is_Empty())
	{
		TextureTrackerClass *track = Managed_Textures.Remove_Head();
		delete track;
	}
}

void TextureResourceManagerClass::Add(TextureTrackerClass *track)
{
	// this function should only be called by the texture constructor
	Managed_Textures.Add(track);
}

void TextureResourceManagerClass::Remove(TextureBaseClass *tex)
{
	// this function should only be called by the texture destructor
	TextureTrackerListIterator it(&Managed_Textures);

	while (!it.Is_Done())
	{
		TextureTrackerClass *track = it.Peek_Obj();
		if (track->Get_Texture() == tex)
		{
			it.Remove_Current_Object();
			delete track;
			break;
		}
		it.Next();
	}
}

void TextureResourceManagerClass::Release_Textures()
{
	TextureTrackerListIterator it(&Managed_Textures);

	while (!it.Is_Done())
	{
		TextureTrackerClass *track = it.Peek_Obj();
		track->Release();
		it.Next();
	}
}

void TextureResourceManagerClass::Recreate_Textures()
{
	TextureTrackerListIterator it(&Managed_Textures);

	while (!it.Is_Done())
	{
		TextureTrackerClass *track = it.Peek_Obj();
		track->Recreate();
		track->Get_Texture()->Set_Dirty();
		it.Next();
	}
}
