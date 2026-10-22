// Preferences shared by whole-archive and selection extraction dialogs.
#pragma once
#include <Windows.h>
#pragma comment(lib, "advapi32.lib")

namespace MiaoZip::Extraction
{
    inline bool ReadOption(const wchar_t* name)
    {
        DWORD value = 0, size = sizeof(value);
        return RegGetValueW(HKEY_CURRENT_USER, L"Software\\MiaoZip\\Extraction",
            name, RRF_RT_REG_DWORD, nullptr, &value, &size) == ERROR_SUCCESS
            && value != 0;
    }

    inline void SaveOption(const wchar_t* name, bool enabled)
    {
        HKEY key{};
        if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\MiaoZip\\Extraction", 0,
            nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) return;
        const DWORD value = enabled ? 1 : 0;
        RegSetValueExW(key, name, 0, REG_DWORD,
            reinterpret_cast<const BYTE*>(&value), sizeof(value));
        RegCloseKey(key);
    }

    inline bool OrganizeOuterFolder() { return ReadOption(L"OrganizeOuterFolder"); }
    inline void SaveOrganizeOuterFolder(bool enabled) { SaveOption(L"OrganizeOuterFolder", enabled); }
    inline bool ExitAfterExtraction() { return ReadOption(L"ExitAfterExtraction"); }
    inline void SaveExitAfterExtraction(bool enabled) { SaveOption(L"ExitAfterExtraction", enabled); }
}
