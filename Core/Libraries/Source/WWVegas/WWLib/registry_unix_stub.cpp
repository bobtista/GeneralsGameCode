/*
**	Command & Conquer Generals Zero Hour(tm)
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

// TheSuperHackers @build bobtista 29/04/2026 Non-Win stub for the legacy
// WWLib RegistryClass. The real implementation is Win-registry only; this
// gives DX8Wrapper / W3DDisplay something to link against on macOS/Linux.
// Render-device persistence is rerouted through registry-unix's RegistryIni
// in a follow-up.

#include "registry.h"

#include <string.h>

bool RegistryClass::IsLocked = false;

bool RegistryClass::Exists(const char * /*sub_key*/) { return false; }

RegistryClass::RegistryClass(const char * /*sub_key*/, bool /*create*/)
{
	IsValid = false;
}

RegistryClass::~RegistryClass()
{
}

int  RegistryClass::Get_Int(const char * /*name*/, int def_value) { return def_value; }
void RegistryClass::Set_Int(const char * /*name*/, int /*value*/) {}

bool  RegistryClass::Get_Bool(const char * /*name*/, bool def_value) { return def_value; }
void  RegistryClass::Set_Bool(const char * /*name*/, bool /*value*/) {}

float RegistryClass::Get_Float(const char * /*name*/, float def_value) { return def_value; }
void  RegistryClass::Set_Float(const char * /*name*/, float /*value*/) {}

char *RegistryClass::Get_String(const char * /*name*/, char *value, int value_size, const char *default_string)
{
	if (value && value_size > 0)
	{
		if (default_string != nullptr)
		{
			std::strncpy(value, default_string, static_cast<size_t>(value_size) - 1);
			value[value_size - 1] = '\0';
		}
		else
		{
			value[0] = '\0';
		}
	}
	return value;
}

void RegistryClass::Get_String(const char * /*name*/, StringClass &string, const char *default_string)
{
	string = (default_string != nullptr) ? default_string : "";
}

void RegistryClass::Set_String(const char * /*name*/, const char * /*value*/) {}

void RegistryClass::Get_String(const WCHAR * /*name*/, WideStringClass &string, const WCHAR *default_string)
{
	string = (default_string != nullptr) ? default_string : L"";
}

void RegistryClass::Set_String(const WCHAR * /*name*/, const WCHAR * /*value*/) {}

void RegistryClass::Get_Bin(const char * /*name*/, void * /*buffer*/, int /*buffer_size*/) {}
int  RegistryClass::Get_Bin_Size(const char * /*name*/) { return 0; }
void RegistryClass::Set_Bin(const char * /*name*/, const void * /*buffer*/, int /*buffer_size*/) {}

void RegistryClass::Get_Value_List(DynamicVectorClass<StringClass> & /*list*/) {}

void RegistryClass::Delete_Value(const char * /*name*/) {}
void RegistryClass::Deleta_All_Values() {}
