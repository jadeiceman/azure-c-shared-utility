// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/optimize_size.h"
#include "azure_c_shared_utility/xio.h"
#include "azure_c_shared_utility/strings.h"
#include "azure_c_shared_utility/xlogging.h"
#include "winsock2.h"

#ifdef USE_OPENSSL
#include "azure_c_shared_utility/tlsio_openssl.h"
#endif
#if USE_CYCLONESSL
#include "azure_c_shared_utility/tlsio_cyclonessl.h"
#endif
#if USE_WOLFSSL
#include "azure_c_shared_utility/tlsio_wolfssl.h"
#endif

#include "azure_c_shared_utility/tlsio_schannel.h"

int platform_init(void)
{
    int result;

    WSADATA wsaData;
    int error_code = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (error_code != 0)
    {
        LogError("WSAStartup failed: 0x%x", error_code);
        result = __FAILURE__;
    }
    else
    {
#ifdef USE_OPENSSL
        tlsio_openssl_init();
#endif
        result = 0;
    }
    return result;
}

const IO_INTERFACE_DESCRIPTION* platform_get_default_tlsio(void)
{
#ifdef USE_OPENSSL
    return tlsio_openssl_get_interface_description();
#elif USE_CYCLONESSL
    return tlsio_cyclonessl_get_interface_description();
#elif USE_WOLFSSL
    return tlsio_wolfssl_get_interface_description();
#else
    return tlsio_schannel_get_interface_description();
#endif
}

BOOL platform_get_device_id(char* deviceId)
{
    LONG openKey;
    DWORD options = 0;
    HKEY openResult;
    HKEY hKey = HKEY_LOCAL_MACHINE;
    LPCSTR subKey = "Software\\Microsoft\\SQMClient";
    LPCSTR lpValue = "MachineId";
    DWORD dwFlags = RRF_RT_ANY;

    LONG getRegValue = ERROR_INVALID_HANDLE;
    DWORD dataType;
    char value[255];
    DWORD size = sizeof(value);
    PVOID pvData = value;

    openKey = RegOpenKeyExA(
        hKey,
        subKey,
        options,
        KEY_READ,
        &openResult
    );

    // Get the Device ID of the Windows device from Reg Key
    if (openKey == ERROR_SUCCESS)
    {
        getRegValue = RegGetValueA(
            openResult,
            NULL,
            lpValue,
            dwFlags,
            &dataType,
            pvData,
            &size
        );
    }

    // Failed to read value, so try opening the 64-bit reg key
    // in case this is an x86 binary being run on Windows x64
    if (getRegValue != ERROR_SUCCESS)
    {
        openKey = RegOpenKeyExA(
            hKey,
            subKey,
            options,
            KEY_READ | KEY_WOW64_64KEY,
            &openResult
        );

        if (openKey == ERROR_SUCCESS)
        {
            getRegValue = RegGetValueA(
                openResult,
                NULL,
                lpValue,
                dwFlags,
                &dataType,
                pvData,
                &size
            );
        }
    }

    if (getRegValue == ERROR_SUCCESS)
    {
        strcpy(deviceId, value);
        return TRUE;
    }

    return FALSE;
}

STRING_HANDLE platform_get_platform_info_internal(char* deviceId)
{
    // Expected format: "(<runtime name>; <operating system name>; <platform>)"

    STRING_HANDLE result;
    SYSTEM_INFO sys_info;
    OSVERSIONINFO osvi;
    char *arch;

    GetSystemInfo(&sys_info);

    switch (sys_info.wProcessorArchitecture)
    {
        case PROCESSOR_ARCHITECTURE_AMD64:
            arch = "x64";
            break;

        case PROCESSOR_ARCHITECTURE_ARM:
            arch = "ARM";
            break;

        case PROCESSOR_ARCHITECTURE_IA64:
            arch = "IA64";
            break;

        case PROCESSOR_ARCHITECTURE_INTEL:
            arch = "x32";
            break;

        default:
            arch = "UNKNOWN";
            break;
    }

    result = NULL;
    memset(&osvi, 0, sizeof(osvi));
    osvi.dwOSVersionInfoSize = sizeof(osvi);
#pragma warning(disable:4996)
    if (GetVersionEx(&osvi))
    {
        DWORD product_type;
        if (GetProductInfo(osvi.dwMajorVersion, osvi.dwMinorVersion, 0, 0, &product_type))
        {
            if (deviceId != NULL)
            {
                result = STRING_construct_sprintf("(native; WindowsProduct:0x%08x %d.%d; %s; %s)", product_type, osvi.dwMajorVersion, osvi.dwMinorVersion, arch, deviceId);
            }
            else
            {
                result = STRING_construct_sprintf("(native; WindowsProduct:0x%08x %d.%d; %s)", product_type, osvi.dwMajorVersion, osvi.dwMinorVersion, arch);
            }
        }
    }

    if (result == NULL)
    {
        DWORD dwVersion = GetVersion();

        if (deviceId != NULL)
        {
            result = STRING_construct_sprintf("(native; WindowsProduct:Windows NT %d.%d; %s; %s)", LOBYTE(LOWORD(dwVersion)), HIBYTE(LOWORD(dwVersion)), arch, deviceId);
        }
        else
        {
            result = STRING_construct_sprintf("(native; WindowsProduct:Windows NT %d.%d; %s)", LOBYTE(LOWORD(dwVersion)), HIBYTE(LOWORD(dwVersion)), arch);
        }
    }
#pragma warning(default:4996)

    if (result == NULL)
    {
        LogError("STRING_construct_sprintf failed");
    }
    return result;
}

STRING_HANDLE platform_get_platform_info(void)
{
    return platform_get_platform_info_internal(NULL);
}

STRING_HANDLE platform_get_platform_info_with_id(void)
{
    char deviceId[255];
    if (platform_get_device_id(deviceId))
    {
        return platform_get_platform_info_internal(deviceId);
    }
    else
    {
        return platform_get_platform_info_internal(NULL);
    }
}

void platform_deinit(void)
{
    (void)WSACleanup();

#ifdef USE_OPENSSL
    tlsio_openssl_deinit();
#endif
}

