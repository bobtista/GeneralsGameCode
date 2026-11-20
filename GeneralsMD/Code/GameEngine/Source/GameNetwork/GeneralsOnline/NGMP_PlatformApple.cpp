#if defined(__APPLE__)

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitKeys.h>
#include <Security/Security.h>
#include <ifaddrs.h>
#include <net/if_dl.h>
#include <sys/socket.h>
#include <cstdio>
#include <cstring>
#include <string>

static CFMutableDictionaryRef NGMP_Apple_CreateKeychainQuery()
{
	CFMutableDictionaryRef query = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (query == nullptr)
	{
		return nullptr;
	}

	CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
	CFDictionarySetValue(query, kSecAttrService, CFSTR("GeneralsOnline"));
	CFDictionarySetValue(query, kSecAttrAccount, CFSTR("refresh_token"));
	return query;
}

bool NGMP_Apple_KeychainRead(std::string& strOut)
{
	strOut.clear();

	CFMutableDictionaryRef query = NGMP_Apple_CreateKeychainQuery();
	if (query == nullptr)
	{
		return false;
	}

	CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
	CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);

	CFTypeRef result = nullptr;
	OSStatus status = SecItemCopyMatching(query, &result);
	CFRelease(query);

	if (result != nullptr)
	{
		if (status == errSecSuccess && CFGetTypeID(result) == CFDataGetTypeID())
		{
			CFDataRef data = (CFDataRef)result;
			strOut.assign((const char*)CFDataGetBytePtr(data), (size_t)CFDataGetLength(data));
		}
		CFRelease(result);
	}

	return !strOut.empty();
}

bool NGMP_Apple_KeychainWrite(const std::string& strData)
{
	CFMutableDictionaryRef query = NGMP_Apple_CreateKeychainQuery();
	if (query == nullptr)
	{
		return false;
	}

	CFDataRef data = CFDataCreate(kCFAllocatorDefault, (const UInt8*)strData.data(), (CFIndex)strData.size());
	if (data == nullptr)
	{
		CFRelease(query);
		return false;
	}

	CFMutableDictionaryRef attributes = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (attributes == nullptr)
	{
		CFRelease(data);
		CFRelease(query);
		return false;
	}

	CFDictionarySetValue(attributes, kSecValueData, data);
	OSStatus status = SecItemUpdate(query, attributes);
	if (status == errSecItemNotFound)
	{
		CFDictionarySetValue(query, kSecValueData, data);
		status = SecItemAdd(query, nullptr);
	}

	CFRelease(attributes);
	CFRelease(data);
	CFRelease(query);
	return status == errSecSuccess;
}

void NGMP_Apple_KeychainDelete()
{
	CFMutableDictionaryRef query = NGMP_Apple_CreateKeychainQuery();
	if (query == nullptr)
	{
		return;
	}

	SecItemDelete(query);
	CFRelease(query);
}

std::string NGMP_Apple_GetPlatformUUID()
{
	std::string strUUID;

	io_service_t service = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("IOPlatformExpertDevice"));
	if (service == IO_OBJECT_NULL)
	{
		return strUUID;
	}

	CFTypeRef uuid = IORegistryEntryCreateCFProperty(service, CFSTR(kIOPlatformUUIDKey), kCFAllocatorDefault, 0);
	IOObjectRelease(service);

	if (uuid != nullptr)
	{
		if (CFGetTypeID(uuid) == CFStringGetTypeID())
		{
			char buffer[128];
			if (CFStringGetCString((CFStringRef)uuid, buffer, sizeof(buffer), kCFStringEncodingUTF8))
			{
				strUUID = buffer;
			}
		}
		CFRelease(uuid);
	}

	return strUUID;
}

std::string NGMP_Apple_GetPrimaryMacAddress()
{
	std::string strMac;

	struct ifaddrs* pAddrs = nullptr;
	if (getifaddrs(&pAddrs) != 0)
	{
		return strMac;
	}

	for (struct ifaddrs* pAddr = pAddrs; pAddr != nullptr; pAddr = pAddr->ifa_next)
	{
		if (pAddr->ifa_addr == nullptr || pAddr->ifa_name == nullptr || pAddr->ifa_addr->sa_family != AF_LINK || strcmp(pAddr->ifa_name, "en0") != 0)
		{
			continue;
		}

		struct sockaddr_dl* pLink = (struct sockaddr_dl*)pAddr->ifa_addr;
		if (pLink->sdl_alen != 6)
		{
			continue;
		}

		const unsigned char* pBytes = (const unsigned char*)LLADDR(pLink);
		char macAddr[32];
		snprintf(macAddr, sizeof(macAddr), "%02X:%02X:%02X:%02X:%02X:%02XZZ",
			pBytes[0], pBytes[1], pBytes[2], pBytes[3], pBytes[4], pBytes[5]);
		strMac = macAddr;
		break;
	}

	freeifaddrs(pAddrs);
	return strMac;
}

#endif
