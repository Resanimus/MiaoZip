// MiaoZip live document sessions. Original code, MIT licensed.
#pragma once
#include "NativeSupport.h"
#include <optional>

namespace MiaoZip::LiveEdit
{
    struct Commit
    {
        std::wstring Id, Archive, Entry, WorkingFile, Snapshot, Hash, ArchiveHash, Backup;
    };
    bool Enabled();
    void SetEnabled(bool enabled);
    fs::path RecoveryRoot();
    fs::path CreateSessionDirectory();
    std::wstring ExistingFile(const std::wstring& archive, const std::wstring& entry);
    bool Add(const std::wstring& archive, const std::wstring& entry, const fs::path& workingFile,
        const std::wstring& archiveHash);
    std::optional<Commit> TakeReady();
    void Complete(const Commit& commit, HRESULT result, const std::wstring& candidate);
    std::wstring Status();
    size_t Count();
    void Shutdown();
}
