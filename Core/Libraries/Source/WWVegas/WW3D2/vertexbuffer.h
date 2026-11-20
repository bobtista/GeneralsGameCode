/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
*/

#pragma once

#include "dx8fvf.h"
#include "vertexbufferbase.h"

const unsigned dynamic_fvf_type=DX8_FVF_FLAG_XYZ|DX8_FVF_FLAG_NORMAL|DX8_FVF_TEX2|DX8_FVF_FLAG_DIFFUSE;

class DX8Wrapper;
class SortingRendererClass;

/**
** Dynamic vertex buffer access is a wrapper to a single cycled dynamic vertex
** buffer.
** DynamicVBAccess gains an access to the dynamic vertex buffer and only
** only of these are allowed at any one time.
**
** The dynamic fvf buffers are always of the same type.
**
** NOTE: Dynamic vertex buffers accessors should only be used locally!
**
*/

class DynamicVBAccessClass
{
	friend DX8Wrapper;
	friend SortingRendererClass;

	const FVFInfoClass& FVFInfo;
	unsigned Type;
	unsigned short VertexCount;
	unsigned short VertexBufferOffset;
	VertexBufferClass* VertexBuffer;
//	static VertexFormatXYZNDUV2* _Get_Sorting_Vertex_Array();

	void Allocate_Sorting_Dynamic_Buffer();
	void Allocate_Backend_Dynamic_Buffer();
public:
	// Type parameter can be either BUFFER_TYPE_DYNAMIC or BUFFER_TYPE_DYNAMIC_SORTING.

	// Note: Even though the constructor takes fvf as a parameter, currently the
	// only acceptable parameter is "dynamic_fvf_type". Any other type will
	// result to an assert.
	DynamicVBAccessClass(unsigned type,unsigned fvf,unsigned short vertex_count);
	~DynamicVBAccessClass();

	// Access fvf
	const FVFInfoClass& FVF_Info() const { return FVFInfo; }
	unsigned Get_Type() const { return Type; }
	unsigned short Get_Vertex_Count() const { return VertexCount; }
	unsigned short Get_Vertex_Buffer_Offset() const { return VertexBufferOffset; }
	VertexBufferClass * Get_Vertex_Buffer() const { return VertexBuffer; }

	// Call at the end of the execution, or at whatever time you wish to release
	// the recycled dynamic vertex buffer.
	static void _Deinit();
	static void _Reset(bool frame_changed);
	static unsigned short Get_Default_Vertex_Count();	///<current size of dynamic vertex buffer

	// To lock the vertex buffer, create instance of this write class locally.
	// The buffer is automatically unlocked when you exit the scope.
	class WriteLockClass// : public VertexBufferLockClass
	{
		DynamicVBAccessClass* DynamicVBAccess;
		VertexFormatXYZNDUV2 * Vertices;
		bool DirectBackendWrite;
	public:
		WriteLockClass(DynamicVBAccessClass* vb_access);
		~WriteLockClass();

		// Use this function to get a pointer to the first vertex you can write into.
		// If we ever change the format used by DynamicVBAccessClass, then the
		// return type of this function will change and we'll easily find all code
		// using it.
		VertexFormatXYZNDUV2 * Get_Formatted_Vertex_Array();
	};
	friend WriteLockClass;
};

// ----------------------------------------------------------------------------

inline VertexFormatXYZNDUV2 * DynamicVBAccessClass::WriteLockClass::Get_Formatted_Vertex_Array()
{
	// assert that the format of the dynamic vertex buffer is still what we think it is.
	WWASSERT(DynamicVBAccess->VertexBuffer->FVF_Info().Get_FVF() == dynamic_fvf_type);
	return Vertices;
}

// ----------------------------------------------------------------------------

/**
** SortingVertexBufferClass
** This class acts as a vertex buffer for the vertices that need to be passed to alpha renderer.
*/
class SortingVertexBufferClass : public VertexBufferClass
{
	W3DMPO_CODE(SortingVertexBufferClass)

	friend DX8Wrapper;
	friend SortingRendererClass;
	friend VertexBufferClass::WriteLockClass;
	friend VertexBufferClass::AppendLockClass;
	friend DynamicVBAccessClass::WriteLockClass;

	VertexFormatXYZNDUV2* VertexBuffer;

protected:
	virtual ~SortingVertexBufferClass() override;
public:
	SortingVertexBufferClass(unsigned short VertexCount);
};
