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

// TableOfContents.cpp
// Implementation of DataChunkTableOfContents
// TheSuperHackers @feature bobtista 14/11/2025 Extract chunk I/O to platform-neutral library

#include "DataChunk/TableOfContents.h"
#include <string.h>
#include <stdlib.h>

namespace DataChunk {

//----------------------------------------------------------------------
// DataChunkTableOfContents
//----------------------------------------------------------------------

DataChunkTableOfContents::DataChunkTableOfContents() :
	m_list(NULL),
	m_nextID(1),
	m_listLength(0),
	m_headerOpened(false)
{
}

DataChunkTableOfContents::~DataChunkTableOfContents()
{
	Mapping *m, *next;

	for (m = m_list; m; m = next)
	{
		next = m->next;
		delete m;
	}
}

Mapping* DataChunkTableOfContents::findMapping(const ChunkString& name)
{
	Mapping *m;

	for (m = m_list; m; m = m->next)
	{
		if (name == m->name)
			return m;
	}

	return NULL;
}

ChunkUInt DataChunkTableOfContents::getID(const ChunkString& name)
{
	Mapping *m = findMapping(name);

	if (m)
		return m->id;

	return 0;
}

ChunkString DataChunkTableOfContents::getName(ChunkUInt id)
{
	Mapping *m;

	for (m = m_list; m; m = m->next)
	{
		if (m->id == id)
			return m->name;
	}

	return ChunkString();
}

ChunkUInt DataChunkTableOfContents::allocateID(const ChunkString& name)
{
	Mapping *m = findMapping(name);

	if (m)
		return m->id;
	else
	{
		m = new Mapping();

		m->id = m_nextID++;
		m->name = name;
		m->next = m_list;
		m_list = m;

		m_listLength++;

		return m->id;
	}
}

void DataChunkTableOfContents::write(DataChunkOutputStream& s)
{
	Mapping *m;
	unsigned char len;

	unsigned char tag[4] = {'C', 'k', 'M', 'p'};
	s.write(tag, sizeof(tag));

	s.write((void*)&m_listLength, sizeof(ChunkInt));

	for (m = m_list; m; m = m->next)
	{
		len = (unsigned char)m->name.length();
		s.write((char*)&len, sizeof(unsigned char));
		if (len > 0)
		{
			s.write((char*)m->name.c_str(), len);
		}
		s.write((char*)&m->id, sizeof(ChunkUInt));
	}
}

void DataChunkTableOfContents::read(DataChunkInputStream& s)
{
	ChunkInt count, i;
	ChunkUInt maxID = 0;
	unsigned char len;
	Mapping *m;

	unsigned char tag[4] = {'x', 'x', 'x', 'x'};
	s.read(tag, sizeof(tag));
	if (tag[0] != 'C' || tag[1] != 'k' || tag[2] != 'M' || tag[3] != 'p')
	{
		return;
	}

	s.read((char*)&count, sizeof(ChunkInt));

	for (i = 0; i < count; i++)
	{
		m = new Mapping();

		s.read((char*)&len, sizeof(unsigned char));

		if (len > 0)
		{
			char* buffer = new char[len + 1];
			s.read(buffer, len);
			buffer[len] = '\0';
			m->name = buffer;
			delete[] buffer;
		}

		s.read((char*)&m->id, sizeof(ChunkUInt));

		m->next = m_list;
		m_list = m;

		m_listLength++;

		if (m->id > maxID)
			maxID = m->id;
	}
	m_headerOpened = (count > 0 && !s.eof());

	if (m_nextID < maxID + 1)
		m_nextID = maxID + 1;
}

} // namespace DataChunk

