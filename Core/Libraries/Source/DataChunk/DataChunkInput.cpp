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

// DataChunkInput.cpp
// Implementation of DataChunkInput
// TheSuperHackers @feature bobtista 14/11/2025 Extract chunk I/O to platform-neutral library

#include "DataChunk/DataChunk.h"
#include <string.h>
#include <stdlib.h>

namespace DataChunk {

//----------------------------------------------------------------------
// DataChunkInput
//----------------------------------------------------------------------

DataChunkInput::DataChunkInput(DataChunkInputStream* pStream) :
	m_file(pStream),
	m_userData(NULL),
	m_currentObject(NULL),
	m_chunkStack(NULL),
	m_parserList(NULL)
{
	m_contents.read(*m_file);
	m_fileposOfFirstChunk = (ChunkInt)(int)m_file->tell();
}

DataChunkInput::~DataChunkInput()
{
	clearChunkStack();

	UserParser* p, *next;
	for (p = m_parserList; p; p = next)
	{
		next = p->next;
		delete p;
	}
}

void DataChunkInput::clearChunkStack()
{
	InputChunk* c, *next;

	for (c = m_chunkStack; c; c = next)
	{
		next = c->next;
		delete c;
	}

	m_chunkStack = NULL;
}

void DataChunkInput::registerParser(const ChunkString& label, const ChunkString& parentLabel,
                                     DataChunkParserPtr parser, void* userData)
{
	UserParser* p = new UserParser();

	p->label = label;
	p->parentLabel = parentLabel;
	p->parser = parser;
	p->userData = userData;
	p->next = m_parserList;
	m_parserList = p;
}

bool DataChunkInput::parse(void* userData)
{
	ChunkString label;
	ChunkString parentLabel;
	DataChunkVersionType ver;
	UserParser* parser;
	bool scopeOK;
	DataChunkInfo info;

	if (!m_contents.isOpenedForRead())
	{
		return false;
	}

	if (m_chunkStack)
		parentLabel = m_contents.getName(m_chunkStack->id);

	while (!atEndOfFile())
	{
		if (m_chunkStack)
		{
			if (m_chunkStack->dataLeft < CHUNK_HEADER_BYTES)
			{
				if (m_chunkStack->dataLeft != 0)
				{
					return false;
				}
				break;
			}
		}

		label = openDataChunk(&ver);
		if (atEndOfFile())
		{
			break;
		}

		for (parser = m_parserList; parser; parser = parser->next)
		{
			if (parser->label == label)
			{
				scopeOK = true;

				if (parentLabel != parser->parentLabel)
					scopeOK = false;

				if (scopeOK)
				{
					info.label = label;
					info.parentLabel = parentLabel;
					info.version = ver;
					info.dataSize = getChunkDataSize();

					if (!parser->parser(*this, &info, userData ? userData : parser->userData))
						return false;
					break;
				}
			}
		}

		closeDataChunk();
	}

	return true;
}

bool DataChunkInput::isValidFileType()
{
	return m_contents.isOpenedForRead();
}

ChunkString DataChunkInput::openDataChunk(DataChunkVersionType* ver)
{
	InputChunk* c = new InputChunk();
	c->id = 0;
	c->version = 0;
	c->dataSize = 0;

	m_file->read((char*)&c->id, sizeof(ChunkUInt));
	decrementDataLeft(sizeof(ChunkUInt));

	m_file->read((char*)&c->version, sizeof(DataChunkVersionType));
	decrementDataLeft(sizeof(DataChunkVersionType));

	m_file->read((char*)&c->dataSize, sizeof(ChunkInt));
	decrementDataLeft(sizeof(ChunkInt));

	c->dataLeft = c->dataSize;
	c->chunkStart = (ChunkInt)(int)m_file->tell();

	*ver = c->version;

	c->next = m_chunkStack;
	m_chunkStack = c;

	if (atEndOfFile())
	{
		return ChunkString();
	}
	return m_contents.getName(c->id);
}

void DataChunkInput::closeDataChunk()
{
	if (m_chunkStack == NULL)
	{
		return;
	}

	if (m_chunkStack->dataLeft > 0)
	{
		unsigned int newPos = (unsigned int)(m_file->tell() + m_chunkStack->dataLeft);
		m_file->seek(newPos);
		decrementDataLeft(m_chunkStack->dataLeft);
	}

	InputChunk* c = m_chunkStack;
	m_chunkStack = m_chunkStack->next;
	delete c;
}

bool DataChunkInput::atEndOfFile()
{
	return m_file->eof();
}

bool DataChunkInput::atEndOfChunk()
{
	if (m_chunkStack)
	{
		if (m_chunkStack->dataLeft <= 0)
			return true;
		return false;
	}

	return true;
}

void DataChunkInput::reset()
{
	clearChunkStack();
	m_file->seek(m_fileposOfFirstChunk);
}

ChunkString DataChunkInput::getChunkLabel()
{
	if (m_chunkStack == NULL)
	{
		return ChunkString();
	}

	return m_contents.getName(m_chunkStack->id);
}

DataChunkVersionType DataChunkInput::getChunkVersion()
{
	if (m_chunkStack == NULL)
	{
		return 0;
	}

	return m_chunkStack->version;
}

ChunkUInt DataChunkInput::getChunkDataSize()
{
	if (m_chunkStack == NULL)
	{
		return 0;
	}

	return (ChunkUInt)m_chunkStack->dataSize;
}

ChunkUInt DataChunkInput::getChunkDataSizeLeft()
{
	if (m_chunkStack == NULL)
	{
		return 0;
	}

	return (ChunkUInt)m_chunkStack->dataLeft;
}

void DataChunkInput::decrementDataLeft(ChunkInt size)
{
	InputChunk* c;

	c = m_chunkStack;
	while (c)
	{
		c->dataLeft -= size;
		c = c->next;
	}
}

ChunkReal DataChunkInput::readReal()
{
	ChunkReal r;
	if (m_chunkStack->dataLeft < (ChunkInt)sizeof(ChunkReal))
	{
		return 0.0f;
	}
	m_file->read((char*)&r, sizeof(ChunkReal));
	decrementDataLeft(sizeof(ChunkReal));
	return r;
}

ChunkInt DataChunkInput::readInt()
{
	ChunkInt i;
	if (m_chunkStack->dataLeft < (ChunkInt)sizeof(ChunkInt))
	{
		return 0;
	}
	m_file->read((char*)&i, sizeof(ChunkInt));
	decrementDataLeft(sizeof(ChunkInt));
	return i;
}

ChunkByte DataChunkInput::readByte()
{
	ChunkByte b;
	if (m_chunkStack->dataLeft < (ChunkInt)sizeof(ChunkByte))
	{
		return 0;
	}
	m_file->read((char*)&b, sizeof(ChunkByte));
	decrementDataLeft(sizeof(ChunkByte));
	return b;
}

void DataChunkInput::readArrayOfBytes(char* ptr, ChunkInt len)
{
	if (m_chunkStack->dataLeft < len)
	{
		return;
	}
	m_file->read(ptr, len);
	decrementDataLeft(len);
}

ChunkString DataChunkInput::readAsciiString()
{
	ChunkUShort len;
	if (m_chunkStack->dataLeft < (ChunkInt)sizeof(ChunkUShort))
	{
		return ChunkString();
	}
	m_file->read((char*)&len, sizeof(ChunkUShort));
	decrementDataLeft(sizeof(ChunkUShort));

	if (m_chunkStack->dataLeft < len)
	{
		return ChunkString();
	}

	ChunkString theString;
	if (len > 0)
	{
		char* buffer = new char[len + 1];
		m_file->read(buffer, len);
		decrementDataLeft(len);
		buffer[len] = '\0';
		theString = buffer;
		delete[] buffer;
	}

	return theString;
}

ChunkWideString DataChunkInput::readUnicodeString()
{
	ChunkUShort len;
	if (m_chunkStack->dataLeft < (ChunkInt)sizeof(ChunkUShort))
	{
		return ChunkWideString();
	}
	m_file->read((char*)&len, sizeof(ChunkUShort));
	decrementDataLeft(sizeof(ChunkUShort));

	if (m_chunkStack->dataLeft < len * sizeof(wchar_t))
	{
		return ChunkWideString();
	}

	ChunkWideString theString;
	if (len > 0)
	{
		wchar_t* buffer = new wchar_t[len + 1];
		m_file->read((char*)buffer, len * sizeof(wchar_t));
		decrementDataLeft(len * sizeof(wchar_t));
		buffer[len] = L'\0';
		theString = buffer;
		delete[] buffer;
	}

	return theString;
}

} // namespace DataChunk

