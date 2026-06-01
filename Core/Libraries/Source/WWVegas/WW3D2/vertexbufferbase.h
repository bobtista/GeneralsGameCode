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

#include "always.h"
#include "wwdebug.h"
#include "IRenderBackend.h"
#include "RenderBufferTypes.h"

class FVFInfoClass;
class VertexBufferClass;

class VertexBufferLockClass
{
protected:
	VertexBufferClass* VertexBuffer;
	void* Vertices;

	// This class can't be used directly, so constructor as to be protected
	VertexBufferLockClass(VertexBufferClass* vertex_buffer_) : VertexBuffer(vertex_buffer_) {}
public:
	void* Get_Vertex_Array() { return Vertices; }
};

class VertexBufferClass : public RefCountClass
{
protected:
	VertexBufferClass(unsigned type, unsigned FVF, unsigned short VertexCount);
	virtual ~VertexBufferClass() override;
public:

	const FVFInfoClass& FVF_Info() const { return *fvf_info; }
	unsigned short Get_Vertex_Count() const { return VertexCount; }
	unsigned Type() const { return type; }
	const unsigned char * Peek_CPU_Buffer_Data() const { return CPUBufferData; }
	unsigned Get_CPU_Buffer_Size() const { return CPUBufferSize; }
	bool Has_CPU_Buffer_Data() const { return CPUBufferValid; }
	RenderResource Get_Backend_Resource() const { return m_backendHandle; }
	bool Has_Backend_Resource() const { return m_backendHandle != kInvalidRenderResource; }
	bool Is_Backend_Static_Eligible() const { return m_backendStaticEligible; }
	void *Lock_CPU_Buffer_Data(unsigned byte_offset, unsigned size);

	void Add_Engine_Ref() const;
	void Release_Engine_Ref() const;
	unsigned Engine_Refs() const { return engine_refs; }

	class WriteLockClass : public VertexBufferLockClass
	{
	public:
		WriteLockClass(VertexBufferClass* vertex_buffer, int flags=0);
		~WriteLockClass();
	};

	class AppendLockClass : public VertexBufferLockClass
	{
	public:
		// TheSuperHackers @refactor bobtista 15/04/2026 added
		// optional `flags` (e.g. RB_LOCK_DISCARD / RB_LOCK_NOOVERWRITE)
		// for the dynamic shadow buffer's per-batch append pattern. Default
		// of 0 keeps existing one-shot DX8VertexBufferClass::Copy callers
		// unchanged.
		AppendLockClass(VertexBufferClass* vertex_buffer,unsigned start_index, unsigned index_range, unsigned flags=0);
		~AppendLockClass();
	protected:
		// TheSuperHackers @refactor bobtista 11/04/2026
		// stored so the destructor can report the locked sub-range to
		// the bgfx write-side capture hook. Not used by the dx8 path.
		unsigned AppendStartIndex;
		unsigned AppendIndexRange;
	};

	static unsigned Get_Total_Buffer_Count();
	static unsigned Get_Total_Allocated_Vertices();
	static unsigned Get_Total_Allocated_Memory();

protected:
	unsigned					type;
	unsigned short				VertexCount;
	mutable int					engine_refs;
	FVFInfoClass*				fvf_info;
	unsigned char*				CPUBufferData;
	unsigned					CPUBufferSize;
	bool						CPUBufferValid;
	bool						m_backendStaticEligible;
	RenderResource				m_backendHandle;
	void Set_Backend_Static_Eligible(bool eligible) { m_backendStaticEligible = eligible; }
	void Update_CPU_Buffer_Data(unsigned byte_offset, const void * data, unsigned size);
};
