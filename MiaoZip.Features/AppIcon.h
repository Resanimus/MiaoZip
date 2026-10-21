// MiaoZip branding stays local to its UI; shared Shell image lists are never modified.
#pragma once
#include <Windows.h>
#include <filesystem>
#include <string>
#include <cwctype>

namespace MiaoZip::Icons
{
    inline HICON Application(HWND child)
    {
        const HWND root = GetAncestor(child, GA_ROOT);
        auto icon = reinterpret_cast<HICON>(GetClassLongPtrW(root, GCLP_HICONSM));
        return icon ? icon : reinterpret_cast<HICON>(GetClassLongPtrW(root, GCLP_HICON));
    }

    inline bool IsArchive(const wchar_t* name)
    {
        auto extension = std::filesystem::path(name).extension().wstring();
        for (auto& character : extension) character = static_cast<wchar_t>(towlower(character));
        for (const auto* known : { L".zip", L".7z", L".rar", L".tar", L".gz", L".bz2",
            L".xz", L".zst", L".lz", L".lzma", L".lz4", L".cab", L".wim", L".iso",
            L".cpio", L".arj", L".lzh", L".appx", L".msix", L".tgz", L".tbz2" })
            if (extension == known) return true;
        return false;
    }
}
