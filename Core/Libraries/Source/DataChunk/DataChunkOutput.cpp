/*
**	Command & Conquer Generals(tm)
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

// DataChunkOutput.cpp
// Implementation of DataChunkOutput
// TheSuperHackers @feature bobtista 14/11/2025 Extract chunk I/O to platform-neutral library

#include "DataChunk/DataChunk.h"
#include <string.h>
#include <stdlib.h>

namespace DataChunk {

//----------------------------------------------------------------------
// DataChunkOutput
//----------------------------------------------------------------------

static const unsigned int INITIAL_BUFFER_SIZE = 4096;

void DataChunkOutput::growBuffer(unsigned int needed)
{
	unsigned int newSize = m_bufferSize;
	if (newSize == 0)
		newSize = INITIAL_BUFFER_SIZE;

	while (m_bufferPos + needed > newSize)
	{
		newSize *= 2;
	}

	char* newBuffer = new char[newSize];
	if (m_buffer)
	{
		memcpy(newBuffer, m_buffer, m_bufferPos);
		delete[] m_buffer;
	}
	m_buffer = newBuffer;
	m_bufferSize = newSize;
}

DataChunkOutput::DataChunkOutput(DataChunkOutputStream* pOut) :
	m_pOut(pOut),
	m_chunkStack(NULL),
	m_buffer(NULL),
	m_bufferSize(0),
	m_bufferPos(0)
{
}

DataChunkOutput::~DataChunkOutput()
{
	m_contents.write(*m_pOut);

	if (m_buffer && m_bufferPos > 0)
	{
		m_pOut->write(m_buffer, m_bufferPos);
	}

	delete[] m_buffer;
}

void DataChunkOutput::openDataChunk(const char* name, DataChunkVersionType ver)
{
	ChunkUInt id = m_contents.allocateID(ChunkString(name));

	OutputChunk* c = new OutputChunk();
	c->next = m_chunkStack;
	m_chunkStack = c;
	m_chunkStack->id = id;

	if (m_bufferPos + 10 > m_bufferSize)
	{
		growBuffer(10);
	}

	memcpy(&m_buffer[m_bufferPos], &id, sizeof(ChunkUInt));
	m_bufferPos += sizeof(ChunkUInt);

	memcpy(&m_buffer[m_bufferPos], &ver, sizeof(DataChunkVersionType));
	m_bufferPos += sizeof(DataChunkVersionType);

	c->filepos = (ChunkInt)m_bufferPos;

	ChunkInt dummy = 0xffff;
	memcpy(&m_buffer[m_bufferPos], &dummy, sizeof(ChunkInt));
	m_bufferPos += sizeof(ChunkInt);
}

void DataChunkOutput::closeDataChunk()
{
	if (m_chunkStack == NULL)
	{
		return;
	}

	ChunkInt here = (ChunkInt)m_bufferPos;
	ChunkInt size = here - m_chunkStack->filepos - sizeof(ChunkInt);

	memcpy(&m_buffer[m_chunkStack->filepos], &size, sizeof(ChunkInt));

	OutputChunk* c = m_chunkStack;
	m_chunkStack = m_chunkStack->next;
	delete c;
}

void DataChunkOutput::writeReal(ChunkReal r)
{
	if (m_bufferPos + sizeof(ChunkReal) > m_bufferSize)
	{
		growBuffer(sizeof(ChunkReal));
	}
	memcpy(&m_buffer[m_bufferPos], &r, sizeof(ChunkReal));
	m_bufferPos += sizeof(ChunkReal);
}

void DataChunkOutput::writeInt(ChunkInt i)
{
	if (m_bufferPos + sizeof(ChunkInt) > m_bufferSize)
	{
		growBuffer(sizeof(ChunkInt));
	}
	memcpy(&m_buffer[m_bufferPos], &i, sizeof(ChunkInt));
	m_bufferPos += sizeof(ChunkInt);
}

void DataChunkOutput::writeByte(ChunkByte b)
{
	if (m_bufferPos + sizeof(ChunkByte) > m_bufferSize)
	{
		growBuffer(sizeof(ChunkByte));
	}
	m_buffer[m_bufferPos] = b;
	m_bufferPos += sizeof(ChunkByte);
}

void DataChunkOutput::writeAsciiString(const ChunkString& theString)
{
	ChunkUShort len = (ChunkUShort)theString.length();
	if (m_bufferPos + sizeof(ChunkUShort) + len > m_bufferSize)
	{
		growBuffer(sizeof(ChunkUShort) + len);
	}
	memcpy(&m_buffer[m_bufferPos], &len, sizeof(ChunkUShort));
	m_bufferPos += sizeof(ChunkUShort);
	if (len > 0)
	{
		memcpy(&m_buffer[m_bufferPos], theString.c_str(), len);
		m_bufferPos += len;
	}
}

void DataChunkOutput::writeUnicodeString(const ChunkWideString& theString)
{
	ChunkUShort len = (ChunkUShort)theString.length();
	if (m_bufferPos + sizeof(ChunkUShort) + len * sizeof(wchar_t) > m_bufferSize)
	{
		growBuffer(sizeof(ChunkUShort) + len * sizeof(wchar_t));
	}
	memcpy(&m_buffer[m_bufferPos], &len, sizeof(ChunkUShort));
	m_bufferPos += sizeof(ChunkUShort);
	if (len > 0)
	{
		memcpy(&m_buffer[m_bufferPos], theString.c_str(), len * sizeof(wchar_t));
		m_bufferPos += len * sizeof(wchar_t);
	}
}

void DataChunkOutput::writeArrayOfBytes(const char* ptr, ChunkInt len)
{
	if (len > 0)
	{
		if (m_bufferPos + len > m_bufferSize)
		{
			growBuffer(len);
		}
		memcpy(&m_buffer[m_bufferPos], ptr, len);
		m_bufferPos += len;
	}
}

} // namespace DataChunk

