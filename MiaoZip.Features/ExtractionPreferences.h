// The single extraction preference is shared by whole-archive and selection dialogs.
#pragma once
#include <Windows.h>
#pragma comment(lib, "advapi32.lib")

namespace MiaoZip::Extraction
{
    inline bool OrganizeOuterFolder()
    {
        DWORD value = 0, size = sizeof(value);
        return RegGetValueW(HKEY_CURRENT_USER, L"Software\\MiaoZip\\Extraction",
            L"OrganizeOuterFolder", RRF_RT_REG_DWORD, nullptr, &value, &size) == ERROR_SUCCESS
            && value != 0;
    }

    inline void SaveOrganizeOuterFolder(bool enabled)
    {
        HKEY key{};
        if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\MiaoZip\\Extraction", 0,
            nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) return;
        const DWORD value = enabled ? 1 : 0;
        RegSetValueExW(key, L"OrganizeOuterFolder", 0, REG_DWORD,
            reinterpret_cast<const BYTE*>(&value), sizeof(value));
        RegCloseKey(key);
    }
}
