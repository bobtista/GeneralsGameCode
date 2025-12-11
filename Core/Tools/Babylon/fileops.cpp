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

//
// fileops.cpp
// TheSuperHackers @refactor bobtista 01/01/2025 Replace StdAfx.h with PlatformTypes.h for cross-platform support
//

// Include Windows headers FIRST, before ANY other headers, to avoid conflicts
#ifdef _WIN32
    // Undefine any Qt-defined types that might conflict (in case Qt was included via other headers)
    #ifdef SHORT
    #undef SHORT
    #endif
    // Ensure we use ANSI functions, not Unicode
    #ifdef UNICODE
    #undef UNICODE
    #endif
    #ifdef _UNICODE
    #undef _UNICODE
    #endif
    #ifndef NOMINMAX
    #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <direct.h>
#else
    #include <unistd.h>
    #include <sys/stat.h>
#endif

// Now include our headers (which may include Qt headers)
#include "PlatformTypes.h"
#include "fileops.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

// Undefine Windows macros that conflict with Qt (after Qt headers are included)
#ifdef _WIN32
    #ifdef connect
    #undef connect
    #endif
    #ifdef SendMessage
    #undef SendMessage
    #endif
    #ifdef GetMessage
    #undef GetMessage
    #endif
#endif




int							FileExists ( const char *filename )
{
	int fa = FileAttribs ( filename );

	return ! ( (fa == FA_NOFILE) || (fa & FA_DIRECTORY ));
}


int					 		FileAttribs ( const char *filename )
{
	int	fa = FA_NOFILE;

	#ifdef _WIN32
		WIN32_FIND_DATAA 	fi;  // Use ANSI version explicitly
		HANDLE handle;

		handle = FindFirstFileA ( filename, &fi );  // Use ANSI version explicitly

		if ( handle != INVALID_HANDLE_VALUE )
		{
			if ( fi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
			{
				fa |= FA_DIRECTORY;
			}
			if ( fi.dwFileAttributes & FILE_ATTRIBUTE_READONLY )
			{
				fa |= FA_READONLY;
			}
			else
			{
				fa |= FA_WRITEABLE;
			}

			FindClose ( handle );
		}
	#else
		// Unix: Use stat() for file attributes
		struct stat st;
		if ( stat(filename, &st) == 0 )
		{
			if ( S_ISDIR(st.st_mode) )
			{
				fa |= FA_DIRECTORY;
			}
			if ( !(st.st_mode & S_IWUSR) )  // Check if user write permission is off
			{
				fa |= FA_READONLY;
			}
			else
			{
				fa |= FA_WRITEABLE;
			}
		}
	#endif

	return fa;
}

static void make_bk_name ( char *bkname, const char *filename )
{
	const char *ext;
	char *ext1;

	strcpy ( bkname, filename );

	ext = strchr ( filename, '.' );
	ext1 = strchr ( bkname, '.' );

	if ( ext )
	{
		strcpy ( ext1, "_back_up" );
		strcat ( ext1, ext );
	}
	else
	{
		strcat ( bkname, "_back_up" );
	}

}

void	MakeBackupFile ( const char *filename )
{
	char bkname[256];

	make_bk_name ( bkname, filename );

	#ifdef _WIN32
		CopyFile ( filename, bkname, FALSE );
	#else
		// Unix: Use system() or file copy
		#include <cstdio>
		char cmd[512];
		snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s\"", filename, bkname);
		system(cmd);
	#endif

}

void	RestoreBackupFile ( const char *filename )
{
	char bkname[256];

	make_bk_name ( bkname, filename );

	if ( FileExists ( bkname ))
	{
		#ifdef _WIN32
			CopyFile ( bkname, filename, FALSE );
		#else
			// Unix: Use system() or file copy
			char cmd[512];
			snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s\"", bkname, filename);
			system(cmd);
		#endif
	}

}
