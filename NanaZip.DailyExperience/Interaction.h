// Daily experience additions. MIT license, as with NanaZip-owned code.
#pragma once
#include <Windows.h>
#include <Shlwapi.h>
#include <string>

namespace NanaZip::DailyExperience
{
    inline constexpr ULONG_PTR FileNameFilterMessage = 0x4E5A464C;
    inline bool MatchesFileName(
        const wchar_t* Name,
        std::size_t NameLength,
        const wchar_t* Filter)
    {
        if (!Filter || !*Filter)
        {
            return true;
        }
        std::wstring Value(Name, NameLength);
        return nullptr != ::StrStrIW(Value.c_str(), Filter);
    }
}
