// Extraction-only completion policy. Never used by live archive editing.
#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <string>

namespace MiaoZip::Extraction
{
    inline bool ShouldExitAfterExtraction(bool enabled, bool testMode,
        bool success, bool callbackOK, bool messages)
    { return enabled && !testMode && success && callbackOK && !messages; }

    inline std::wstring ProcessPath(DWORD processId)
    {
        std::wstring path(32768, L'\0');
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
        if (!process) return {};
        DWORD length = static_cast<DWORD>(path.size());
        using QueryPath = BOOL (WINAPI*)(HANDLE,DWORD,LPWSTR,PDWORD);
        static const auto query = reinterpret_cast<QueryPath>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "QueryFullProcessImageNameW"));
        const bool ok = query && query(process, 0, path.data(), &length) != FALSE;
        CloseHandle(process);
        if (!ok) return {};
        path.resize(length); return path;
    }
    struct MainOwner { HWND Window{}; DWORD ProcessId{}; };

    inline MainOwner FindExtractionOwner(HWND parent)
    {
        if (parent)
        {
            MainOwner owner{GetAncestor(parent, GA_ROOT), 0};
            GetWindowThreadProcessId(owner.Window, &owner.ProcessId);
            wchar_t name[128]{}; GetClassNameW(owner.Window, name, 128);
            if (_wcsicmp(name, L"NanaZip.Modern.FileManager") == 0) return owner;
        }
        // The full-archive extractor is a child process. Only accept the direct
        // MiaoZip parent in this installation, never an arbitrary open instance.
        DWORD parentId = 0;
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) return {};
        PROCESSENTRY32W entry{sizeof(entry)};
        if (Process32FirstW(snapshot, &entry)) do
        {
            if (entry.th32ProcessID == GetCurrentProcessId())
            { parentId = entry.th32ParentProcessID; break; }
        } while (Process32NextW(snapshot, &entry));
        CloseHandle(snapshot);
        auto own = ProcessPath(GetCurrentProcessId());
        auto expected = own.substr(0, own.find_last_of(L"\\/") + 1) + L"MiaoZip.exe";
        if (!parentId || _wcsicmp(ProcessPath(parentId).c_str(), expected.c_str()) != 0) return {};
        MainOwner owner{nullptr, parentId};
        EnumWindows([](HWND window, LPARAM value) -> BOOL
        {
            auto target = reinterpret_cast<MainOwner*>(value);
            DWORD pid = 0; GetWindowThreadProcessId(window, &pid);
            wchar_t name[128]{}; GetClassNameW(window, name, 128);
            if (pid == target->ProcessId && _wcsicmp(name, L"NanaZip.Modern.FileManager") == 0)
            { target->Window = window; return FALSE; }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&owner));
        return owner;
    }
    inline bool RequestOwnerClose(const MainOwner& owner)
    {
        DWORD pid = 0; GetWindowThreadProcessId(owner.Window, &pid);
        if (!owner.Window || !pid || pid != owner.ProcessId || !IsWindowEnabled(owner.Window)) return false;
        // WM_CLOSE preserves the existing in-archive editing close guard.
        return PostMessageW(owner.Window, WM_CLOSE, 0, 0) != FALSE;
    }
}
