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
**	MERCHANTIBILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

class DX8IndexBufferClass;
class DX8VertexBufferClass;

// Transitional neutral names for runtime code that only needs WW3D render
// buffers, not raw Direct3D buffer objects. The concrete implementation is
// still DX8VertexBufferClass/DX8IndexBufferClass until the compatibility class
// split is complete.
using RenderIndexBufferClass = DX8IndexBufferClass;
using RenderVertexBufferClass = DX8VertexBufferClass;
