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
#include "PreRTS.h"
#include "GameClient/ClientInstance.h"

#ifndef _WIN32
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

#define GENERALS_GUID "685EAFF2-3216-4265-B047-251C5F4B82F3"

namespace rts
{
HANDLE ClientInstance::s_mutexHandle = nullptr;
UnsignedInt ClientInstance::s_instanceIndex = 0;

#ifndef _WIN32
// TheSuperHackers @fix bobtista 08/07/2026 The Win32 CreateMutex shim is a no-op
// on POSIX platforms, which left instance detection permanently uninitialized:
// isInitialized() stayed false and multiple clients all claimed instance 0. Use
// an advisory flock on a per-user temp file instead; the kernel releases it when
// the process exits, matching the auto-release of an abandoned Win32 mutex.
enum InstanceLockResult
{
	INSTANCE_LOCK_ACQUIRED,
	INSTANCE_LOCK_BUSY,
	INSTANCE_LOCK_ERROR
};

static int s_instanceLockFd = -1;

static InstanceLockResult acquireInstanceLock(const char* name)
{
	const char* tmpDir = std::getenv("TMPDIR");
	std::string path = (tmpDir != nullptr && tmpDir[0] != '\0') ? tmpDir : "/tmp";
	if (path[path.size() - 1] != '/')
	{
		path.push_back('/');
	}
	path.append(name);
	path.append(".lock");

	const int fd = ::open(path.c_str(), O_CREAT | O_RDWR, 0644);
	if (fd < 0)
	{
		return INSTANCE_LOCK_ERROR;
	}
	if (::flock(fd, LOCK_EX | LOCK_NB) != 0)
	{
		const InstanceLockResult result =
			(errno == EWOULDBLOCK) ? INSTANCE_LOCK_BUSY : INSTANCE_LOCK_ERROR;
		::close(fd);
		return result;
	}
	s_instanceLockFd = fd;
	return INSTANCE_LOCK_ACQUIRED;
}
#endif

#if defined(RTS_MULTI_INSTANCE)
Bool ClientInstance::s_isMultiInstance = true;
#else
Bool ClientInstance::s_isMultiInstance = false;
#endif

bool ClientInstance::initialize()
{
	if (isInitialized())
	{
		return true;
	}

	// Create a mutex with a unique name to Generals in order to determine if our app is already running.
	// WARNING: DO NOT use this number for any other application except Generals.
	while (true)
	{
		if (isMultiInstance())
		{
			std::string guidStr = getFirstInstanceName();
			if (s_instanceIndex > 0u)
			{
				char idStr[33];
				itoa(s_instanceIndex, idStr, 10);
				guidStr.push_back('-');
				guidStr.append(idStr);
			}
#ifdef _WIN32
			s_mutexHandle = CreateMutex(nullptr, FALSE, guidStr.c_str());
			if (GetLastError() == ERROR_ALREADY_EXISTS)
			{
				if (s_mutexHandle != nullptr)
				{
					CloseHandle(s_mutexHandle);
					s_mutexHandle = nullptr;
				}
				// Try again with a new instance.
				++s_instanceIndex;
				continue;
			}
#else
			const InstanceLockResult result = acquireInstanceLock(guidStr.c_str());
			if (result == INSTANCE_LOCK_BUSY)
			{
				// Try again with a new instance.
				++s_instanceIndex;
				continue;
			}
			if (result == INSTANCE_LOCK_ERROR)
			{
				break;
			}
#endif
		}
		else
		{
#ifdef _WIN32
			s_mutexHandle = CreateMutex(nullptr, FALSE, getFirstInstanceName());
			if (GetLastError() == ERROR_ALREADY_EXISTS)
			{
				if (s_mutexHandle != nullptr)
				{
					CloseHandle(s_mutexHandle);
					s_mutexHandle = nullptr;
				}
				return false;
			}
#else
			const InstanceLockResult result = acquireInstanceLock(getFirstInstanceName());
			if (result == INSTANCE_LOCK_BUSY)
			{
				return false;
			}
			if (result == INSTANCE_LOCK_ERROR)
			{
				break;
			}
#endif
		}
		break;
	}

	return true;
}

bool ClientInstance::isInitialized()
{
#ifdef _WIN32
	return s_mutexHandle != nullptr;
#else
	return s_instanceLockFd >= 0;
#endif
}

bool ClientInstance::isMultiInstance()
{
	return s_isMultiInstance;
}

void ClientInstance::setMultiInstance(bool v)
{
	if (isInitialized())
	{
		DEBUG_CRASH(("ClientInstance::setMultiInstance(%d) - cannot set multi instance after initialization", (int)v));
		return;
	}
	s_isMultiInstance = v;
}

void ClientInstance::skipPrimaryInstance()
{
	if (isInitialized())
	{
		DEBUG_CRASH(("ClientInstance::skipPrimaryInstance() - cannot skip primary instance after initialization"));
		return;
	}
	s_instanceIndex = 1;
}

UnsignedInt ClientInstance::getInstanceIndex()
{
	DEBUG_ASSERTLOG(isInitialized(), ("ClientInstance::isInitialized() failed"));
	return s_instanceIndex;
}

UnsignedInt ClientInstance::getInstanceId()
{
	return getInstanceIndex() + 1;
}

const char* ClientInstance::getFirstInstanceName()
{
	return GENERALS_GUID;
}

} // namespace rts
