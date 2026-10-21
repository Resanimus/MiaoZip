// Friendly Windows file types, cached by extension.
#pragma once
#include <windows.h>
#include <shellapi.h>
#include <filesystem>
#include <map>
#include <string>
namespace MiaoZip
{
    inline std::wstring FileType(const wchar_t* name, bool directory)
    {
        static std::map<std::wstring, std::wstring> cache;
        auto extension = directory ? std::wstring(L"<directory>") : std::filesystem::path(name).extension().wstring();
        for (auto& character : extension) character = towlower(character);
        const auto found = cache.find(extension);
        if (found != cache.end()) return found->second;
        SHFILEINFOW information{};
        SHGetFileInfoW(directory ? L"folder" : (L"file" + extension).c_str(),
            directory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL,
            &information, sizeof(information), SHGFI_TYPENAME | SHGFI_USEFILEATTRIBUTES);
        std::wstring type = information.szTypeName;
        if (type.empty()) type = directory ? L"文件夹" : extension.empty() ? L"文件" : extension.substr(1) + L" 文件";
        cache.emplace(extension, type);
        return type;
    }
}
