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

// ----------------------------------------------------------------------------

class IndexBufferClass : public RefCountClass
{
protected:
	virtual ~IndexBufferClass() override;
public:
	IndexBufferClass(unsigned type, unsigned short index_count);

	void Copy(unsigned int* indices,unsigned start_index,unsigned index_count);
	void Copy(unsigned short* indices,unsigned start_index,unsigned index_count);

	unsigned short Get_Index_Count() const { return index_count; }

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

	class WriteLockClass
	{
		IndexBufferClass* index_buffer;
		unsigned short* indices;
	public:
		WriteLockClass(IndexBufferClass* index_buffer, int flags=0);
		~WriteLockClass();

		unsigned short* Get_Index_Array() { return indices; }
	};

	class AppendLockClass
	{
		IndexBufferClass* index_buffer;
		unsigned short* indices;
		// TheSuperHackers @refactor bobtista 11/04/2026
		// stored so the destructor can report the locked sub-range to
		// the bgfx write-side capture hook. Not used by the dx8 path.
		unsigned AppendStartIndex;
		unsigned AppendIndexRange;
	public:
		// TheSuperHackers @refactor bobtista 15/04/2026 added
		// optional `flags` (RB_LOCK_DISCARD / RB_LOCK_NOOVERWRITE).
		AppendLockClass(IndexBufferClass* index_buffer,unsigned start_index, unsigned index_range, unsigned flags=0);
		~AppendLockClass();

		unsigned short* Get_Index_Array() { return indices; }
	};

	static unsigned Get_Total_Buffer_Count();
	static unsigned Get_Total_Allocated_Indices();
	static unsigned Get_Total_Allocated_Memory();

protected:
	mutable int					engine_refs;
	unsigned short				index_count;		// number of indices
	unsigned					type;
	unsigned char*				CPUBufferData;
	unsigned					CPUBufferSize;
	bool						CPUBufferValid;
	bool						m_backendStaticEligible;
	RenderResource				m_backendHandle;
	void Set_Backend_Static_Eligible(bool eligible) { m_backendStaticEligible = eligible; }
	void Update_CPU_Buffer_Data(unsigned byte_offset, const void * data, unsigned size);
};
