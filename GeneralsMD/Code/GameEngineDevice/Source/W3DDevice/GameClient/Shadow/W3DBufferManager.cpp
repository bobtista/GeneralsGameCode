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

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////


#include "Common/Debug.h"
#include "W3DDevice/GameClient/W3DBufferManager.h"
#include "WW3D2/dx8fvf.h"
#include "WW3D2/renderbufferclasses.h"

W3DBufferManager *TheW3DBufferManager=nullptr;	//singleton

static int FVFTypeIndexList[W3DBufferManager::MAX_FVF]=
{
	RENDER_VERTEX_FORMAT_XYZ,
	RENDER_VERTEX_FORMAT_XYZD,
	RENDER_VERTEX_FORMAT_XYZUV1,
	RENDER_VERTEX_FORMAT_XYZDUV1,
	RENDER_VERTEX_FORMAT_XYZUV2,
	RENDER_VERTEX_FORMAT_XYZDUV2,
	RENDER_VERTEX_FORMAT_XYZN,
	RENDER_VERTEX_FORMAT_XYZND,
	RENDER_VERTEX_FORMAT_XYZNUV1,
	RENDER_VERTEX_FORMAT_XYZNDUV1,
	RENDER_VERTEX_FORMAT_XYZNUV2,
	RENDER_VERTEX_FORMAT_XYZNDUV2,
	RENDER_VERTEX_FORMAT_XYZRHW,
	RENDER_VERTEX_FORMAT_XYZRHWD,
	RENDER_VERTEX_FORMAT_XYZRHWUV1,
	RENDER_VERTEX_FORMAT_XYZRHWDUV1,
	RENDER_VERTEX_FORMAT_XYZRHWUV2,
	RENDER_VERTEX_FORMAT_XYZRHWDUV2
};

Int W3DBufferManager::getDX8Format(VBM_FVF_TYPES format)
{
	return FVFTypeIndexList[format];
}

W3DBufferManager::W3DBufferManager()
{
	m_numEmptySlotsAllocated=0;
	m_numEmptyVertexBuffersAllocated=0;
	m_numEmptyIndexSlotsAllocated=0;
	m_numEmptyIndexBuffersAllocated=0;

	Int i=0;
	for (; i<MAX_FVF; i++)
		m_W3DVertexBuffers[i]=nullptr;
	for (i=0; i<MAX_FVF; i++)
		for (Int j=0; j<MAX_VB_SIZES; j++)
			m_W3DVertexBufferSlots[i][j]=nullptr;

	m_W3DIndexBuffers=nullptr;
	for (Int j=0; j<MAX_IB_SIZES; j++)
		m_W3DIndexBufferSlots[j]=nullptr;
}

W3DBufferManager::~W3DBufferManager()
{
	freeAllSlots();
	freeAllBuffers();
}

void W3DBufferManager::freeAllSlots()
{
	Int i,j;

	for (i=0; i<MAX_FVF; i++)
	{
		for (j=0; j<MAX_VB_SIZES; j++)
		{
			//Release all slots allocated for each size
			W3DVertexBufferSlot *vbSlot = m_W3DVertexBufferSlots[i][j];
			while (vbSlot)
			{
				if (vbSlot->m_prevSameVB)
					vbSlot->m_prevSameVB->m_nextSameVB=vbSlot->m_nextSameVB;
				else
					vbSlot->m_VB->m_usedSlots=nullptr;

				if (vbSlot->m_nextSameVB)
					vbSlot->m_nextSameVB->m_prevSameVB=vbSlot->m_prevSameVB;
				vbSlot=vbSlot->m_nextSameSize;
				m_numEmptySlotsAllocated--;
			}
			m_W3DVertexBufferSlots[i][j]=nullptr;
		}
	}

	for (j=0; j<MAX_IB_SIZES; j++)
	{
		//Release all slots allocated for each size
		W3DIndexBufferSlot *ibSlot = m_W3DIndexBufferSlots[j];
		while (ibSlot)
		{
			if (ibSlot->m_prevSameIB)
				ibSlot->m_prevSameIB->m_nextSameIB=ibSlot->m_nextSameIB;
			else
				ibSlot->m_IB->m_usedSlots=nullptr;

			if (ibSlot->m_nextSameIB)
				ibSlot->m_nextSameIB->m_prevSameIB=ibSlot->m_prevSameIB;
			ibSlot=ibSlot->m_nextSameSize;
			m_numEmptyIndexSlotsAllocated--;
		}
		m_W3DIndexBufferSlots[j]=nullptr;
	}

	DEBUG_ASSERTCRASH(m_numEmptySlotsAllocated==0, ("Failed to free all empty vertex buffer slots"));
	DEBUG_ASSERTCRASH(m_numEmptyIndexSlotsAllocated==0, ("Failed to free all empty index buffer slots"));
}

void W3DBufferManager::freeAllBuffers()
{
	Int i;

	//Make sure all slots are free
	freeAllSlots();	///<release all slots to pool.

	for (i=0; i<MAX_FVF; i++)
	{
		W3DVertexBuffer *vb = m_W3DVertexBuffers[i];
		while (vb)
		{	DEBUG_ASSERTCRASH(vb->m_usedSlots == nullptr, ("Freeing Non-Empty Vertex Buffer"));
			REF_PTR_RELEASE(vb->m_renderVertexBuffer);
			m_numEmptyVertexBuffersAllocated--;
			vb=vb->m_nextVB;	//get next vertex buffer of this type
		}
		m_W3DVertexBuffers[i]=nullptr;
	}

	W3DIndexBuffer *ib = m_W3DIndexBuffers;
	while (ib)
	{	DEBUG_ASSERTCRASH(ib->m_usedSlots == nullptr, ("Freeing Non-Empty Index Buffer"));
		REF_PTR_RELEASE(ib->m_renderIndexBuffer);
		m_numEmptyIndexBuffersAllocated--;
		ib=ib->m_nextIB;	//get next vertex buffer of this type
	}
	m_W3DIndexBuffers=nullptr;

	DEBUG_ASSERTCRASH(m_numEmptyVertexBuffersAllocated==0, ("Failed to free all empty vertex buffers"));
	DEBUG_ASSERTCRASH(m_numEmptyIndexBuffersAllocated==0, ("Failed to free all empty index buffers"));
}

void W3DBufferManager::ReleaseResources()
{
	for (Int i=0; i<MAX_FVF; i++)
	{
		W3DVertexBuffer *vb = m_W3DVertexBuffers[i];
		while (vb)
		{
			REF_PTR_RELEASE(vb->m_renderVertexBuffer);
			vb=vb->m_nextVB;	//get next vertex buffer of this type
		}
	}

	W3DIndexBuffer *ib = m_W3DIndexBuffers;
	while (ib)
	{
		REF_PTR_RELEASE(ib->m_renderIndexBuffer);
		ib=ib->m_nextIB;	//get next vertex buffer of this type
	}
}

Bool W3DBufferManager::ReAcquireResources()
{
	for (Int i=0; i<MAX_FVF; i++)
	{
		W3DVertexBuffer *vb = m_W3DVertexBuffers[i];
		while (vb)
		{	DEBUG_ASSERTCRASH( vb->m_renderVertexBuffer == nullptr, ("ReAcquire of existing vertex buffer"));
			vb->m_renderVertexBuffer=NEW_REF(RenderVertexBufferClass,(FVFTypeIndexList[vb->m_format],vb->m_size,Render_Buffer_Usage_Default<RenderVertexBufferClass>()));
			DEBUG_ASSERTCRASH( vb->m_renderVertexBuffer, ("Failed ReAcquire of vertex buffer"));
			if (!vb->m_renderVertexBuffer)
				return FALSE;
			vb=vb->m_nextVB;	//get next vertex buffer of this type
		}
	}

	W3DIndexBuffer *ib = m_W3DIndexBuffers;
	while (ib)
	{	DEBUG_ASSERTCRASH( ib->m_renderIndexBuffer == nullptr, ("ReAcquire of existing index buffer"));
		ib->m_renderIndexBuffer=NEW_REF(RenderIndexBufferClass,(ib->m_size,Render_Buffer_Usage_Default<RenderIndexBufferClass>()));
		DEBUG_ASSERTCRASH( ib->m_renderIndexBuffer, ("Failed ReAcquire of index buffer"));
		if (!ib->m_renderIndexBuffer)
			return FALSE;
		ib=ib->m_nextIB;	//get next vertex buffer of this type
	}

	return TRUE;
}

/**Searches through previously allocated vertex buffer slots and returns a matching type.  If none found,
   creates a new slot and adds it to the pool.  Returns a pointer to the VB slot.
   Returns nullptr in case of failure.
*/
W3DBufferManager::W3DVertexBufferSlot *W3DBufferManager::getSlot(VBM_FVF_TYPES fvfType, Int size)
{
	W3DVertexBufferSlot *vbSlot=nullptr;

	//round size to next multiple of minimum slot size.
	//should help avoid fragmentation.
	size = (size + (MIN_SLOT_SIZE-1)) & (~(MIN_SLOT_SIZE-1));
	Int sizeIndex = (size >> MIN_SLOT_SIZE_SHIFT)-1;

	DEBUG_ASSERTCRASH(sizeIndex < MAX_VB_SIZES && size > 0, ("Allocating too large vertex buffer slot"));
	// TheSuperHackers @bugfix xezon 18/05/2025 Protect against indexing slots beyond the max size.
	// This will happen when a mesh is too complex to draw shadows with.
	if (sizeIndex >= MAX_VB_SIZES || size <= 0) {
		return nullptr;
	}

	if ((vbSlot=m_W3DVertexBufferSlots[fvfType][sizeIndex]) != nullptr)
	{	//found a previously allocated slot matching required size
		m_W3DVertexBufferSlots[fvfType][sizeIndex]=vbSlot->m_nextSameSize;
		if (vbSlot->m_nextSameSize)
			vbSlot->m_nextSameSize->m_prevSameSize=nullptr;
		return vbSlot;
	}
	else
	{	//need to allocate a new slot
		return allocateSlotStorage(fvfType, size);
	}

	return nullptr;
}

/**Returns vertex buffer space back to pool so it can be reused later*/
void W3DBufferManager::releaseSlot(W3DVertexBufferSlot *vbSlot)
{
	Int sizeIndex = (vbSlot->m_size >> MIN_SLOT_SIZE_SHIFT)-1;

	vbSlot->m_nextSameSize=m_W3DVertexBufferSlots[vbSlot->m_VB->m_format][sizeIndex];
	if (m_W3DVertexBufferSlots[vbSlot->m_VB->m_format][sizeIndex])
		m_W3DVertexBufferSlots[vbSlot->m_VB->m_format][sizeIndex]->m_prevSameSize=vbSlot;

	m_W3DVertexBufferSlots[vbSlot->m_VB->m_format][sizeIndex]=vbSlot;
}

/**Reserves space inside existing vertex buffer or allocates a new one to fit the required size.
*/
W3DBufferManager::W3DVertexBufferSlot * W3DBufferManager::allocateSlotStorage(VBM_FVF_TYPES fvfType, Int size)
{

	W3DVertexBuffer *pVB;
	W3DVertexBufferSlot *vbSlot;
//	Int sizeIndex = (size >> MIN_SLOT_SIZE_SHIFT)-1;

	DEBUG_ASSERTCRASH(m_numEmptySlotsAllocated < MAX_NUMBER_SLOTS, ("No more VB Slots"));
	// TheSuperHackers @bugfix xezon 18/05/2025 Protect against allocating slot storage beyond the max size.
	// This will happen when there are too many meshes in the scene to draw shadows with.
	if (m_numEmptySlotsAllocated >= MAX_NUMBER_SLOTS) {
		return nullptr;
	}

	pVB=m_W3DVertexBuffers[fvfType];
	while (pVB)
	{
		if ((pVB->m_size - pVB->m_startFreeIndex) >= size)
		{	//found enough free space in this vertex buffer
			vbSlot=&m_W3DVertexBufferEmptySlots[m_numEmptySlotsAllocated];
			vbSlot->m_size=size;
			vbSlot->m_start=pVB->m_startFreeIndex;
			vbSlot->m_VB=pVB;
			//Link to VB list of slots
			vbSlot->m_nextSameVB=pVB->m_usedSlots;
			vbSlot->m_prevSameVB=nullptr;	//this will be the new head
			if (pVB->m_usedSlots)
				pVB->m_usedSlots->m_prevSameVB=vbSlot;
			vbSlot->m_prevSameSize=vbSlot->m_nextSameSize=nullptr;
			pVB->m_usedSlots=vbSlot;
			pVB->m_startFreeIndex += size;
			m_numEmptySlotsAllocated++;
			return vbSlot;
		}
		pVB = pVB->m_nextVB;
	}

	pVB=m_W3DVertexBuffers[fvfType];	//save old list head

	//Didn't find any vertex buffers with room, create a new one
	DEBUG_ASSERTCRASH(m_numEmptyVertexBuffersAllocated < MAX_VERTEX_BUFFERS_CREATED, ("Reached Max Static VB Shadow Geometry"));

	if (m_numEmptyVertexBuffersAllocated < MAX_VERTEX_BUFFERS_CREATED)
	{
		m_W3DVertexBuffers[fvfType] = &m_W3DEmptyVertexBuffers[m_numEmptyVertexBuffersAllocated];
		m_W3DVertexBuffers[fvfType]->m_nextVB=pVB;	//link to list
		m_numEmptyVertexBuffersAllocated++;

		pVB=m_W3DVertexBuffers[fvfType];	//get new list head

		Int vbSize=__max(DEFAULT_VERTEX_BUFFER_SIZE,size);

		pVB->m_renderVertexBuffer=NEW_REF(RenderVertexBufferClass,(FVFTypeIndexList[fvfType],vbSize,Render_Buffer_Usage_Default<RenderVertexBufferClass>()));
		pVB->m_format=fvfType;
		pVB->m_startFreeIndex=size;
		pVB->m_size=vbSize;
		vbSlot=&m_W3DVertexBufferEmptySlots[m_numEmptySlotsAllocated];
		m_numEmptySlotsAllocated++;
		pVB->m_usedSlots=vbSlot;
		vbSlot->m_size=size;
		vbSlot->m_start=0;
		vbSlot->m_VB=pVB;
		vbSlot->m_prevSameVB=vbSlot->m_nextSameVB=nullptr;
		vbSlot->m_prevSameSize=vbSlot->m_nextSameSize=nullptr;
		return vbSlot;
	}

	return nullptr;
}

//******************************** Index Buffer code ******************************************************
/**Searches through previously allocated index buffer slots and returns a matching type.  If none found,
   creates a new slot and adds it to the pool.  Returns a pointer to the IB slot.
   Returns nullptr in case of failure.
*/
W3DBufferManager::W3DIndexBufferSlot *W3DBufferManager::getSlot(Int size)
{
	W3DIndexBufferSlot *ibSlot=nullptr;

	//round size to next multiple of minimum slot size.
	//should help avoid fragmentation.
	size = (size + (MIN_SLOT_SIZE-1)) & (~(MIN_SLOT_SIZE-1));
	Int sizeIndex = (size >> MIN_SLOT_SIZE_SHIFT)-1;

	DEBUG_ASSERTCRASH(sizeIndex < MAX_IB_SIZES && size > 0, ("Allocating too large index buffer slot"));
	// TheSuperHackers @bugfix xezon 18/05/2025 Protect against indexing slots beyond the max size.
	// This will happen when a mesh is too complex to draw shadows with.
	if (sizeIndex >= MAX_IB_SIZES || size <= 0) {
		return nullptr;
	}

	if ((ibSlot=m_W3DIndexBufferSlots[sizeIndex]) != nullptr)
	{	//found a previously allocated slot matching required size
		m_W3DIndexBufferSlots[sizeIndex]=ibSlot->m_nextSameSize;
		if (ibSlot->m_nextSameSize)
			ibSlot->m_nextSameSize->m_prevSameSize=nullptr;
		return ibSlot;
	}
	else
	{	//need to allocate a new slot
		return allocateSlotStorage(size);
	}

	return nullptr;
}

/**Returns index buffer space back to pool so it can be reused later*/
void W3DBufferManager::releaseSlot(W3DIndexBufferSlot *ibSlot)
{
	Int sizeIndex = (ibSlot->m_size >> MIN_SLOT_SIZE_SHIFT)-1;

	ibSlot->m_nextSameSize=m_W3DIndexBufferSlots[sizeIndex];
	if (m_W3DIndexBufferSlots[sizeIndex])
		m_W3DIndexBufferSlots[sizeIndex]->m_prevSameSize=ibSlot;

	m_W3DIndexBufferSlots[sizeIndex]=ibSlot;
}

/**Reserves space inside existing index buffer or allocates a new one to fit the required size.
*/
W3DBufferManager::W3DIndexBufferSlot * W3DBufferManager::allocateSlotStorage(Int size)
{

	W3DIndexBuffer *pIB;
	W3DIndexBufferSlot *ibSlot;
//	Int sizeIndex = (size >> MIN_SLOT_SIZE_SHIFT)-1;

	DEBUG_ASSERTCRASH(m_numEmptyIndexSlotsAllocated < MAX_NUMBER_SLOTS, ("No more IB Slots"));
	// TheSuperHackers @bugfix xezon 18/05/2025 Protect against allocating slot storage beyond the max size.
	// This will happen when there are too many meshes in the scene to draw shadows with.
	if (m_numEmptyIndexSlotsAllocated >= MAX_NUMBER_SLOTS) {
		return nullptr;
	}

	pIB=m_W3DIndexBuffers;
	while (pIB)
	{
		if ((pIB->m_size - pIB->m_startFreeIndex) >= size)
		{	//found enough free space in this index buffer
			ibSlot=&m_W3DIndexBufferEmptySlots[m_numEmptyIndexSlotsAllocated];
			ibSlot->m_size=size;
			ibSlot->m_start=pIB->m_startFreeIndex;
			ibSlot->m_IB=pIB;
			//Link to IB list of slots
			ibSlot->m_nextSameIB=pIB->m_usedSlots;
			ibSlot->m_prevSameIB=nullptr;	//this will be the new head
			if (pIB->m_usedSlots)
				pIB->m_usedSlots->m_prevSameIB=ibSlot;
			ibSlot->m_prevSameSize=ibSlot->m_nextSameSize=nullptr;
			pIB->m_usedSlots=ibSlot;
			pIB->m_startFreeIndex += size;
			m_numEmptyIndexSlotsAllocated++;
			return ibSlot;
		}
		pIB = pIB->m_nextIB;
	}

	pIB=m_W3DIndexBuffers;	//save old list head

	//Didn't find any index buffers with room, create a new one
	DEBUG_ASSERTCRASH(m_numEmptyIndexBuffersAllocated < MAX_INDEX_BUFFERS_CREATED, ("Reached Max Static IB Shadow Geometry"));

	if (m_numEmptyIndexBuffersAllocated < MAX_INDEX_BUFFERS_CREATED)
	{
		m_W3DIndexBuffers = &m_W3DEmptyIndexBuffers[m_numEmptyIndexBuffersAllocated];
		m_W3DIndexBuffers->m_nextIB=pIB;	//link to list
		m_numEmptyIndexBuffersAllocated++;

		pIB=m_W3DIndexBuffers;	//get new list head

		Int ibSize=__max(DEFAULT_INDEX_BUFFER_SIZE,size);

		pIB->m_renderIndexBuffer=NEW_REF(RenderIndexBufferClass,(ibSize,Render_Buffer_Usage_Default<RenderIndexBufferClass>()));
		pIB->m_startFreeIndex=size;
		pIB->m_size=ibSize;
		ibSlot=&m_W3DIndexBufferEmptySlots[m_numEmptyIndexSlotsAllocated];
		m_numEmptyIndexSlotsAllocated++;
		pIB->m_usedSlots=ibSlot;
		ibSlot->m_size=size;
		ibSlot->m_start=0;
		ibSlot->m_IB=pIB;
		ibSlot->m_prevSameIB=ibSlot->m_nextSameIB=nullptr;
		ibSlot->m_prevSameSize=ibSlot->m_nextSameSize=nullptr;
		return ibSlot;
	}

	return nullptr;
}
