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

#include "indexbufferbase.h"

class DX8Wrapper;
class SortingRendererClass;
class SortingIndexBufferClass;

// HY 2/14/01
// Created
class DynamicIBAccessClass
{
	W3DMPO_CODE(DynamicIBAccessClass)

	friend DX8Wrapper;
	friend SortingRendererClass;

	unsigned Type;
	unsigned short IndexCount;
	unsigned short IndexBufferOffset;
	IndexBufferClass* IndexBuffer;

	void Allocate_Sorting_Dynamic_Buffer();
	void Allocate_Backend_Dynamic_Buffer();

public:
	DynamicIBAccessClass(unsigned short type, unsigned short index_count);
	~DynamicIBAccessClass();

	unsigned Get_Type() const { return Type; }
	unsigned short Get_Index_Count() const { return IndexCount; }
	unsigned short Get_Index_Buffer_Offset() const { return IndexBufferOffset; }
	IndexBufferClass * Get_Index_Buffer() const { return IndexBuffer; }

	// Call at the end of the execution, or at whatever time you wish to release
	// the recycled dynamic index buffer.
	static void _Deinit();
	static void _Reset(bool frame_changed);
	static unsigned short Get_Default_Index_Count();	///<current size of dynamic index buffer

	// To lock the index buffer, create instance of this write class locally.
	// The buffer is automatically unlocked when you exit the scope.
	class WriteLockClass
	{
		DynamicIBAccessClass* DynamicIBAccess;
		unsigned short* Indices;
		bool DirectBackendWrite;
	public:
		WriteLockClass(DynamicIBAccessClass* ib_access);
		~WriteLockClass();
		unsigned short* Get_Index_Array() { return Indices; }
	};

	friend WriteLockClass;
};

class SortingIndexBufferClass : public IndexBufferClass
{
	W3DMPO_CODE(SortingIndexBufferClass)

	friend DX8Wrapper;
	friend SortingRendererClass;
	friend IndexBufferClass::WriteLockClass;
	friend IndexBufferClass::AppendLockClass;
	friend DynamicIBAccessClass::WriteLockClass;
public:
	SortingIndexBufferClass(unsigned short index_count);
	virtual ~SortingIndexBufferClass() override;

protected:
	unsigned short* index_buffer;
};
