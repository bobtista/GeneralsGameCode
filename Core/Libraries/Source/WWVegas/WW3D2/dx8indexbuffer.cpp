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
 *                 Project Name : ww3d                                                         *
 *                                                                                             *
 *                     $Archive:: /Commando/Code/ww3d2/dx8indexbuffer.cpp                     $*
 *                                                                                             *
 *              Original Author:: Jani Penttinen                                               *
 *                                                                                             *
 *                      $Author:: Jani_p                                                      $*
 *                                                                                             *
 *                     $Modtime:: 11/09/01 3:12p                                              $*
 *                                                                                             *
 *                    $Revision:: 26                                                          $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

//#define INDEX_BUFFER_LOG

#include "dx8indexbuffer.h"
#include "dx8wrapper.h"
#include "RenderBackend.h"
#include "IRenderBackend.h"
#include "renderbufferclasses.h"
#include "sphere.h"
#include "thread.h"
#include "wwmemlog.h"
#include <cstring>

// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4C.4 capture index
// data into the active render backend at write-lock time. See the
// matching comment in dx8vertexbuffer.cpp.
#include "RenderBackend.h"
#include "IRenderBackend.h"

static constexpr unsigned short kDefaultDynamicIndexBufferSize = 5000;

static bool _DynamicSortingIndexArrayInUse=false;
static SortingIndexBufferClass* _DynamicSortingIndexArray;
static unsigned short _DynamicSortingIndexArraySize=0;
static unsigned short _DynamicSortingIndexArrayOffset=0;

static bool _DynamicBackendIndexBufferInUse=false;
static RenderIndexBufferClass* _DynamicBackendIndexBuffer=nullptr;
static unsigned short _DynamicBackendIndexBufferSize=kDefaultDynamicIndexBufferSize;
static unsigned short _DynamicBackendIndexBufferOffset=0;

static int _IndexBufferCount;
static int _IndexBufferTotalIndices;
static int _IndexBufferTotalSize;

#if !defined(GGC_BGFX_STANDALONE)
using LegacyIndexBuffer = IDirect3DIndexBuffer8;

constexpr unsigned kLegacyBufferUsageWriteOnly = D3DUSAGE_WRITEONLY, kLegacyBufferUsageDynamic = D3DUSAGE_DYNAMIC, kLegacyBufferUsageNPatches = D3DUSAGE_NPATCHES, kLegacyBufferUsageSoftwareProcessing = D3DUSAGE_SOFTWAREPROCESSING;
constexpr auto kLegacyIndexFormat = D3DFMT_INDEX16;

static unsigned BuildLegacyBufferUsage(DX8IndexBufferClass::UsageType usage)
{
	return kLegacyBufferUsageWriteOnly |
		((usage&DX8IndexBufferClass::USAGE_DYNAMIC) ? kLegacyBufferUsageDynamic : 0) |
		((usage&DX8IndexBufferClass::USAGE_NPATCHES) ? kLegacyBufferUsageNPatches : 0) |
		((usage&DX8IndexBufferClass::USAGE_SOFTWAREPROCESSING) ? kLegacyBufferUsageSoftwareProcessing : 0);
}

static auto GetLegacyBufferPool(DX8IndexBufferClass::UsageType usage)
{
	return (usage&DX8IndexBufferClass::USAGE_DYNAMIC) ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED;
}

static auto Legacy_Device()
{
	return DX8Wrapper::_Get_D3D_Device8();
}

static LegacyIndexBuffer *Legacy_Index_Buffer(DX8IndexBufferClass *index_buffer)
{
	return static_cast<LegacyIndexBuffer *>(index_buffer->Get_Legacy_Index_Buffer());
}
#endif

// ----------------------------------------------------------------------------
//
//
//
// ----------------------------------------------------------------------------

IndexBufferClass::IndexBufferClass(unsigned type_, unsigned short index_count_)
	:
		index_count(index_count_),
		type(type_),
		engine_refs(0),
		CPUBufferData(nullptr),
		CPUBufferSize(0),
		CPUBufferValid(false),
		m_backendStaticEligible(false)
{
	m_backendHandle = kInvalidRenderResource;
	WWASSERT(type==BUFFER_TYPE_STATIC || type==BUFFER_TYPE_SORTING);
	WWASSERT(index_count);

	_IndexBufferCount++;
	_IndexBufferTotalIndices+=index_count;
	_IndexBufferTotalSize+=index_count*sizeof(unsigned short);
#ifdef VERTEX_BUFFER_LOG
	WWDEBUG_SAY(("New IB, %d indices, size %d bytes",index_count,index_count*sizeof(unsigned short)));
	WWDEBUG_SAY(("Total IB count: %d, total %d indices, total size %d bytes",
		_IndexBufferCount,
		_IndexBufferTotalIndices,
		_IndexBufferTotalSize));
#endif
}

IndexBufferClass::~IndexBufferClass()
{
	_IndexBufferCount--;
	_IndexBufferTotalIndices-=index_count;
	_IndexBufferTotalSize-=index_count*sizeof(unsigned short);
	#ifdef VERTEX_BUFFER_LOG
		WWDEBUG_SAY(("Delete IB, %d indices, size %d bytes",index_count,index_count*sizeof(unsigned short)));
		WWDEBUG_SAY(("Total IB count: %d, total %d indices, total size %d bytes",
			_IndexBufferCount,
			_IndexBufferTotalIndices,
			_IndexBufferTotalSize));
	#endif
	delete[] CPUBufferData;
}

unsigned IndexBufferClass::Get_Total_Buffer_Count()
{
	return _IndexBufferCount;
}

unsigned IndexBufferClass::Get_Total_Allocated_Indices()
{
	return _IndexBufferTotalIndices;
}

unsigned IndexBufferClass::Get_Total_Allocated_Memory()
{
	return _IndexBufferTotalSize;
}

void *IndexBufferClass::Lock_CPU_Buffer_Data(unsigned byte_offset, unsigned size)
{
#if defined(GGC_BGFX_STANDALONE)
	if (type == BUFFER_TYPE_STATIC && m_backendHandle == kInvalidRenderResource) {
		WWASSERT_PRINT(
			false,
			"IndexBufferClass::Lock_CPU_Buffer_Data: standalone bgfx static index buffers require a backend resource");
		return nullptr;
	}
#endif
	const unsigned total_size = index_count * sizeof(unsigned short);
	if (byte_offset > total_size || size > total_size - byte_offset) {
		WWASSERT(0);
		return nullptr;
	}

	if (CPUBufferData == nullptr) {
		CPUBufferData = W3DNEWARRAY unsigned char[total_size];
		std::memset(CPUBufferData, 0, total_size);
		CPUBufferSize = total_size;
	}
	CPUBufferValid = true;
	return CPUBufferData + byte_offset;
}

void IndexBufferClass::Update_CPU_Buffer_Data(unsigned byte_offset, const void * data, unsigned size)
{
	if (data == nullptr || size == 0) {
		return;
	}

#if defined(GGC_BGFX_STANDALONE)
	if (type == BUFFER_TYPE_STATIC && m_backendHandle == kInvalidRenderResource) {
		WWASSERT_PRINT(
			false,
			"IndexBufferClass::Update_CPU_Buffer_Data: standalone bgfx static index buffers require a backend resource");
		return;
	}
#endif
	const unsigned total_size = index_count * sizeof(unsigned short);
	if (byte_offset > total_size || size > total_size - byte_offset) {
		WWASSERT(0);
		return;
	}

	if (CPUBufferData == nullptr) {
		CPUBufferData = W3DNEWARRAY unsigned char[total_size];
		std::memset(CPUBufferData, 0, total_size);
		CPUBufferSize = total_size;
	}

	std::memcpy(CPUBufferData + byte_offset, data, size);
	CPUBufferValid = true;
}

void IndexBufferClass::Add_Engine_Ref() const
{
	engine_refs++;
}

void IndexBufferClass::Release_Engine_Ref() const
{
	engine_refs--;
	WWASSERT(engine_refs>=0);
}

// ----------------------------------------------------------------------------
//
//
//
// ----------------------------------------------------------------------------

void IndexBufferClass::Copy(unsigned int* indices,unsigned first_index,unsigned count)
{
	WWASSERT(indices);

	if (first_index) {
		IndexBufferClass::AppendLockClass l(this,first_index,count);
		unsigned short* inds=l.Get_Index_Array();
		for (unsigned v=0;v<count;++v) {
			*inds++=(unsigned short)(*indices++);
		}
	}
	else {
		IndexBufferClass::WriteLockClass l(this);
		unsigned short* inds=l.Get_Index_Array();
		for (unsigned v=0;v<count;++v) {
			*inds++=(unsigned short)(*indices++);
		}
	}
}

// ----------------------------------------------------------------------------

void IndexBufferClass::Copy(unsigned short* indices,unsigned first_index,unsigned count)
{
	WWASSERT(indices);

	if (first_index) {
		IndexBufferClass::AppendLockClass l(this,first_index,count);
		unsigned short* inds=l.Get_Index_Array();
		for (unsigned v=0;v<count;++v) {
			*inds++=*indices++;
		}
	}
	else {
		IndexBufferClass::WriteLockClass l(this);
		unsigned short* inds=l.Get_Index_Array();
		for (unsigned v=0;v<count;++v) {
			*inds++=*indices++;
		}
	}
}

// ----------------------------------------------------------------------------
//
//
// ----------------------------------------------------------------------------


IndexBufferClass::WriteLockClass::WriteLockClass(IndexBufferClass* index_buffer_, int flags) : index_buffer(index_buffer_)
{
	DX8_THREAD_ASSERT();
	WWASSERT(index_buffer);
	WWASSERT(!index_buffer->Engine_Refs());
	index_buffer->Add_Ref();
	switch (index_buffer->Type()) {
	case BUFFER_TYPE_STATIC:
#if !defined(GGC_BGFX_STANDALONE)
		DX8_Assert();
		if (LegacyIndexBuffer *legacy = Legacy_Index_Buffer(static_cast<DX8IndexBufferClass*>(index_buffer))) {
			DX8_ErrorCode(legacy->Lock(
				0,
				index_buffer->Get_Index_Count()*sizeof(WORD),
				(unsigned char**)&indices,
				flags));
		} else
#endif
		{
			indices = static_cast<unsigned short *>(index_buffer->Lock_CPU_Buffer_Data(
				0,
				index_buffer->Get_Index_Count()*sizeof(WORD)));
		}
		break;
	case BUFFER_TYPE_SORTING:
		indices=static_cast<SortingIndexBufferClass*>(index_buffer)->index_buffer;
		break;
	default:
		WWASSERT(0);
		break;
	}
}

// ----------------------------------------------------------------------------
//
//
// ----------------------------------------------------------------------------

IndexBufferClass::WriteLockClass::~WriteLockClass()
{
	DX8_THREAD_ASSERT();
	// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4C.4 capture
	// index data into the active render backend BEFORE unlocking. After
	// Unlock the source pointer is invalid. The bgfx backend snapshots
	// the bytes; the dx8 backend ignores the call.
	// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4G.10 include
	// BUFFER_TYPE_SORTING so sorting FVF category containers feed their
	// shared IB into the bgfx dynamic IB cache alongside the rigid path.
		if (indices != NULL &&
			(index_buffer->Type() == BUFFER_TYPE_STATIC || index_buffer->Type() == BUFFER_TYPE_SORTING)) {
			const unsigned int total_bytes = index_buffer->Get_Index_Count() * sizeof(unsigned short);
			index_buffer->Update_CPU_Buffer_Data(0, indices, total_bytes);
			if (g_renderBackend != NULL) {
				g_renderBackend->Upload_Index_Buffer_Data(index_buffer, indices, total_bytes);
			}
		}
	switch (index_buffer->Type()) {
	case BUFFER_TYPE_STATIC:
#if !defined(GGC_BGFX_STANDALONE)
		DX8_Assert();
		if (LegacyIndexBuffer *legacy = Legacy_Index_Buffer(static_cast<DX8IndexBufferClass*>(index_buffer))) {
			DX8_ErrorCode(legacy->Unlock());
		}
#endif
		break;
	case BUFFER_TYPE_SORTING:
		break;
	default:
		WWASSERT(0);
		break;
	}
	index_buffer->Release_Ref();
}

// ----------------------------------------------------------------------------

IndexBufferClass::AppendLockClass::AppendLockClass(IndexBufferClass* index_buffer_,unsigned start_index, unsigned index_range, unsigned flags)
	:
	index_buffer(index_buffer_),
	AppendStartIndex(start_index),
	AppendIndexRange(index_range)
{
	DX8_THREAD_ASSERT();
	WWASSERT(start_index+index_range<=index_buffer->Get_Index_Count());
	WWASSERT(index_buffer);
	WWASSERT(!index_buffer->Engine_Refs());
	index_buffer->Add_Ref();
	switch (index_buffer->Type()) {
	case BUFFER_TYPE_STATIC:
#if !defined(GGC_BGFX_STANDALONE)
		DX8_Assert();
		if (LegacyIndexBuffer *legacy = Legacy_Index_Buffer(static_cast<DX8IndexBufferClass*>(index_buffer))) {
			DX8_ErrorCode(legacy->Lock(
				start_index*sizeof(unsigned short),
				index_range*sizeof(unsigned short),
				(unsigned char**)&indices,
				flags));
		} else
#endif
		{
			indices = static_cast<unsigned short *>(index_buffer->Lock_CPU_Buffer_Data(
				start_index*sizeof(unsigned short),
				index_range*sizeof(unsigned short)));
		}
		break;
	case BUFFER_TYPE_SORTING:
		indices=static_cast<SortingIndexBufferClass*>(index_buffer)->index_buffer+start_index;
		break;
	default:
		WWASSERT(0);
		break;
	}
}

// ----------------------------------------------------------------------------

IndexBufferClass::AppendLockClass::~AppendLockClass()
{
	DX8_THREAD_ASSERT();
	// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4G.6 write-side
	// sub-range capture for bgfx backend. Rigid mesh category containers
	// fill their shared IB via AppendLockClass; without this hook mesh
	// indices never reach bgfx and units silently fail to render.
	// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4G.10 capture
	// sorting IB sub-range writes for sorting FVF category containers.
		if (indices != NULL &&
			(index_buffer->Type() == BUFFER_TYPE_STATIC || index_buffer->Type() == BUFFER_TYPE_SORTING)) {
			const unsigned int size_bytes = AppendIndexRange * sizeof(unsigned short);
			index_buffer->Update_CPU_Buffer_Data(AppendStartIndex * sizeof(unsigned short), indices, size_bytes);
			if (g_renderBackend != NULL) {
				g_renderBackend->Upload_Index_Buffer_Sub_Range(index_buffer, indices, AppendStartIndex, size_bytes);
			}
		}
	switch (index_buffer->Type()) {
	case BUFFER_TYPE_STATIC:
#if !defined(GGC_BGFX_STANDALONE)
		DX8_Assert();
		if (LegacyIndexBuffer *legacy = Legacy_Index_Buffer(static_cast<DX8IndexBufferClass*>(index_buffer))) {
			DX8_ErrorCode(legacy->Unlock());
		}
#endif
		break;
	case BUFFER_TYPE_SORTING:
		break;
	default:
		WWASSERT(0);
		break;
	}
	index_buffer->Release_Ref();
}

// ----------------------------------------------------------------------------
//
//
//
// ----------------------------------------------------------------------------

DX8IndexBufferClass::DX8IndexBufferClass(unsigned short index_count_,UsageType usage)
	:
	IndexBufferClass(BUFFER_TYPE_STATIC,index_count_)
{
	DX8_THREAD_ASSERT();
	WWASSERT(index_count);
	Set_Backend_Static_Eligible((usage & USAGE_DYNAMIC) == 0);
	if (g_renderBackend != nullptr && !g_renderBackend->Requires_Legacy_Buffer_Resources()) {
		m_backendHandle = g_renderBackend->Register_Index_Buffer_Resource(this);
		return;
	}

#if defined(GGC_BGFX_STANDALONE)
	WWASSERT(0);
	return;
#else
	unsigned usage_flags=BuildLegacyBufferUsage(usage);
	if (!g_renderBackend || !g_renderBackend->Supports_Hardware_Transform_And_Lighting()) {
		usage_flags|=kLegacyBufferUsageSoftwareProcessing;
	}

	LegacyIndexBuffer *new_index_buffer = nullptr;
	HRESULT ret=Legacy_Device()->CreateIndexBuffer(
		sizeof(WORD)*index_count,
		usage_flags,
		kLegacyIndexFormat,
		GetLegacyBufferPool(usage),
		&new_index_buffer);
	index_buffer = new_index_buffer;

	if (SUCCEEDED(ret)) {
		// Phase 5 Stage 2: populate backend-neutral handle.
		if (g_renderBackend != nullptr) {
			m_backendHandle = g_renderBackend->Register_Index_Buffer_Resource(this);
		}
		return;
	}

	WWDEBUG_SAY(("Index buffer creation failed, trying to release assets..."));

	// Index buffer creation failed, so try releasing least used textures and flushing the mesh cache.

	// Free all textures that haven't been used in the last 5 seconds
	TextureClass::Invalidate_Old_Unused_Textures(5000);

	// Invalidate the mesh cache
	WW3D::_Invalidate_Mesh_Cache();

	// Try again...
	new_index_buffer = nullptr;
	ret=Legacy_Device()->CreateIndexBuffer(
		sizeof(WORD)*index_count,
		usage_flags,
		kLegacyIndexFormat,
		GetLegacyBufferPool(usage),
		&new_index_buffer);
	index_buffer = new_index_buffer;

	if (SUCCEEDED(ret)) {
		WWDEBUG_SAY(("...Index buffer creation successful"));
		if (g_renderBackend != nullptr) {
			m_backendHandle = g_renderBackend->Register_Index_Buffer_Resource(this);
		}
	}

	// If it still fails it is fatal
	DX8_ErrorCode(ret);
#endif
}

// ----------------------------------------------------------------------------

DX8IndexBufferClass::~DX8IndexBufferClass()
{
	// Phase 5 Stage 2: release backend-neutral handle before the legacy resource.
	if (m_backendHandle != kInvalidRenderResource && g_renderBackend != nullptr) {
		g_renderBackend->Destroy_Resource(m_backendHandle);
		m_backendHandle = kInvalidRenderResource;
	}
#if !defined(GGC_BGFX_STANDALONE)
	if (LegacyIndexBuffer *legacy = Legacy_Index_Buffer(this)) {
		legacy->Release();
	}
#endif
}

// ----------------------------------------------------------------------------

#if defined(GGC_BGFX_STANDALONE)
RenderIndexBufferClass::RenderIndexBufferClass(unsigned short index_count_, UsageType usage)
	:
	IndexBufferClass(BUFFER_TYPE_STATIC, index_count_)
{
	DX8_THREAD_ASSERT();
	WWASSERT(index_count);
	Set_Backend_Static_Eligible((usage & USAGE_DYNAMIC) == 0);
	if (g_renderBackend != nullptr) {
		m_backendHandle = g_renderBackend->Register_Index_Buffer_Resource(this);
	}
}

RenderIndexBufferClass::~RenderIndexBufferClass()
{
	if (m_backendHandle != kInvalidRenderResource && g_renderBackend != nullptr) {
		g_renderBackend->Destroy_Resource(m_backendHandle);
		m_backendHandle = kInvalidRenderResource;
	}
}
#endif

// ----------------------------------------------------------------------------
//
//
//
// ----------------------------------------------------------------------------

SortingIndexBufferClass::SortingIndexBufferClass(unsigned short index_count_)
	:
	IndexBufferClass(BUFFER_TYPE_SORTING,index_count_)
{
	WWMEMLOG(MEM_RENDERER);
	WWASSERT(index_count);

	index_buffer=W3DNEWARRAY unsigned short[index_count];
}

// ----------------------------------------------------------------------------

SortingIndexBufferClass::~SortingIndexBufferClass()
{
	delete[] index_buffer;
}

// ----------------------------------------------------------------------------
//
//
//
// ----------------------------------------------------------------------------

DynamicIBAccessClass::DynamicIBAccessClass(unsigned short type_, unsigned short index_count_)
	:
	IndexCount(index_count_),
	IndexBuffer(nullptr),
	Type(type_)
{
	WWASSERT(Type==BUFFER_TYPE_DYNAMIC || Type==BUFFER_TYPE_DYNAMIC_SORTING);
	if (Type==BUFFER_TYPE_DYNAMIC) {
		Allocate_Backend_Dynamic_Buffer();
	}
	else {
		Allocate_Sorting_Dynamic_Buffer();
	}
}

DynamicIBAccessClass::~DynamicIBAccessClass()
{
	REF_PTR_RELEASE(IndexBuffer);
	if (Type==BUFFER_TYPE_DYNAMIC) {
		_DynamicBackendIndexBufferInUse=false;
		_DynamicBackendIndexBufferOffset+=IndexCount;
	}
	else {
		_DynamicSortingIndexArrayInUse=false;
		_DynamicSortingIndexArrayOffset+=IndexCount;
	}
}

void DynamicIBAccessClass::_Deinit()
{
	WWASSERT ((_DynamicBackendIndexBuffer == nullptr) || (_DynamicBackendIndexBuffer->Num_Refs() == 1));
	REF_PTR_RELEASE(_DynamicBackendIndexBuffer);
	_DynamicBackendIndexBufferInUse=false;
	_DynamicBackendIndexBufferSize=kDefaultDynamicIndexBufferSize;
	_DynamicBackendIndexBufferOffset=0;

	WWASSERT ((_DynamicSortingIndexArray == nullptr) || (_DynamicSortingIndexArray->Num_Refs() == 1));
	REF_PTR_RELEASE(_DynamicSortingIndexArray);
	_DynamicSortingIndexArrayInUse=false;
	_DynamicSortingIndexArraySize=0;
	_DynamicSortingIndexArrayOffset=0;
}

// ----------------------------------------------------------------------------
//
//
//
// ----------------------------------------------------------------------------

DynamicIBAccessClass::WriteLockClass::WriteLockClass(DynamicIBAccessClass* ib_access_)
	:
	DynamicIBAccess(ib_access_),
	Indices(NULL),
	DirectBackendWrite(false)
{
	DX8_THREAD_ASSERT();
	DynamicIBAccess->IndexBuffer->Add_Ref();
	switch (DynamicIBAccess->Get_Type()) {
	case BUFFER_TYPE_DYNAMIC:
		WWASSERT(DynamicIBAccess);
//		WWASSERT(!dynamic_dx8_index_buffer->Engine_Refs());
#if !defined(GGC_BGFX_STANDALONE)
		DX8_Assert();
		if (LegacyIndexBuffer *legacy = Legacy_Index_Buffer(static_cast<DX8IndexBufferClass*>(DynamicIBAccess->IndexBuffer))) {
			DX8_ErrorCode(legacy->Lock(
				DynamicIBAccess->IndexBufferOffset*sizeof(WORD),
				DynamicIBAccess->Get_Index_Count()*sizeof(WORD),
				(unsigned char**)&Indices,
				!DynamicIBAccess->IndexBufferOffset ? RB_LOCK_DISCARD : RB_LOCK_NOOVERWRITE));
		} else
#endif
		{
			const unsigned int ib_bytes = DynamicIBAccess->Get_Index_Count() * sizeof(unsigned short);
			if (g_renderBackend != NULL) {
				Indices = static_cast<unsigned short *>(
					g_renderBackend->Begin_Dynamic_Index_Write(DynamicIBAccess, ib_bytes));
			}
			if (Indices != NULL) {
				DirectBackendWrite = true;
			} else {
				Indices = static_cast<unsigned short *>(DynamicIBAccess->IndexBuffer->Lock_CPU_Buffer_Data(
					DynamicIBAccess->IndexBufferOffset*sizeof(WORD),
					ib_bytes));
			}
		}
		break;
	case BUFFER_TYPE_DYNAMIC_SORTING:
		Indices=static_cast<SortingIndexBufferClass*>(DynamicIBAccess->IndexBuffer)->index_buffer;
		Indices+=DynamicIBAccess->IndexBufferOffset;
		break;
	default:
		WWASSERT(0);
		break;
	}
}

DynamicIBAccessClass::WriteLockClass::~WriteLockClass()
{
	DX8_THREAD_ASSERT();
	switch (DynamicIBAccess->Get_Type()) {
	case BUFFER_TYPE_DYNAMIC:
		// TheSuperHackers @refactor bobtista 11/04/2026 Phase 4G.2
		// write-side capture for bgfx backend. Copy locked sub-range
		// into a bgfx transient IB before Unlock.
		if (g_renderBackend != NULL && Indices != NULL) {
			const unsigned int total_bytes = DynamicIBAccess->Get_Index_Count() * sizeof(unsigned short);
			if (DirectBackendWrite) {
				g_renderBackend->End_Dynamic_Index_Write(DynamicIBAccess, Indices, total_bytes);
			} else {
				g_renderBackend->Capture_Dynamic_Index_Data(DynamicIBAccess, Indices, total_bytes);
			}
		}
#if !defined(GGC_BGFX_STANDALONE)
		DX8_Assert();
		if (LegacyIndexBuffer *legacy = Legacy_Index_Buffer(static_cast<DX8IndexBufferClass*>(DynamicIBAccess->IndexBuffer))) {
			DX8_ErrorCode(legacy->Unlock());
		}
#endif
		break;
	case BUFFER_TYPE_DYNAMIC_SORTING:
		break;
	default:
		WWASSERT(0);
		break;
	}
	DynamicIBAccess->IndexBuffer->Release_Ref();
}

// ----------------------------------------------------------------------------
//
//
//
// ----------------------------------------------------------------------------

void DynamicIBAccessClass::Allocate_Backend_Dynamic_Buffer()
{
	WWMEMLOG(MEM_RENDERER);
	WWASSERT(!_DynamicBackendIndexBufferInUse);
	_DynamicBackendIndexBufferInUse=true;

	// If requesting more indices than dynamic index buffer can fit, delete the ib
	// and adjust the size to the new count.
	if (IndexCount>_DynamicBackendIndexBufferSize) {
		REF_PTR_RELEASE(_DynamicBackendIndexBuffer);
		_DynamicBackendIndexBufferSize=IndexCount;
		if (_DynamicBackendIndexBufferSize<kDefaultDynamicIndexBufferSize) _DynamicBackendIndexBufferSize=kDefaultDynamicIndexBufferSize;
	}

	// Create a new vb if one doesn't exist currently
	if (!_DynamicBackendIndexBuffer) {
		unsigned usage=RenderIndexBufferClass::USAGE_DYNAMIC;
		if (g_renderBackend && g_renderBackend->Supports_NPatches()) {
			usage|=RenderIndexBufferClass::USAGE_NPATCHES;
		}

		_DynamicBackendIndexBuffer=NEW_REF(RenderIndexBufferClass,(
			_DynamicBackendIndexBufferSize,
			(RenderIndexBufferClass::UsageType)usage));
		_DynamicBackendIndexBufferOffset=0;
	}

	// Any room at the end of the buffer?
	if (((unsigned)IndexCount+_DynamicBackendIndexBufferOffset)>_DynamicBackendIndexBufferSize) {
		_DynamicBackendIndexBufferOffset=0;
	}

	REF_PTR_SET(IndexBuffer,_DynamicBackendIndexBuffer);
	IndexBufferOffset=_DynamicBackendIndexBufferOffset;
}

void DynamicIBAccessClass::Allocate_Sorting_Dynamic_Buffer()
{
	WWMEMLOG(MEM_RENDERER);
	WWASSERT(!_DynamicSortingIndexArrayInUse);
	_DynamicSortingIndexArrayInUse=true;

	unsigned new_index_count=_DynamicSortingIndexArrayOffset+IndexCount;
	WWASSERT(new_index_count<65536);
	if (new_index_count>_DynamicSortingIndexArraySize) {
		REF_PTR_RELEASE(_DynamicSortingIndexArray);
		_DynamicSortingIndexArraySize=new_index_count;
		if (_DynamicSortingIndexArraySize<kDefaultDynamicIndexBufferSize) _DynamicSortingIndexArraySize=kDefaultDynamicIndexBufferSize;
	}

	if (!_DynamicSortingIndexArray) {
		_DynamicSortingIndexArray=NEW_REF(SortingIndexBufferClass,(_DynamicSortingIndexArraySize));
		_DynamicSortingIndexArrayOffset=0;
	}

	REF_PTR_SET(IndexBuffer,_DynamicSortingIndexArray);
	IndexBufferOffset=_DynamicSortingIndexArrayOffset;
}

void DynamicIBAccessClass::_Reset(bool frame_changed)
{
	_DynamicSortingIndexArrayOffset=0;
	if (frame_changed) _DynamicBackendIndexBufferOffset=0;
}

unsigned short DynamicIBAccessClass::Get_Default_Index_Count()
{
	return _DynamicBackendIndexBufferSize;
}
