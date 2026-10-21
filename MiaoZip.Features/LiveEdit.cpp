// MiaoZip live document sessions. Original code, MIT licensed.
#include "LiveEdit.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <memory>
#include <chrono>

namespace MiaoZip::LiveEdit
{
    namespace
    {
        struct Session
        {
            Commit Item;
            std::wstring Committed, Observed, Error;
            std::vector<fs::path> Backups;
            ULONGLONG ChangedAt = 0;
            bool Ready = false, InFlight = false, Failed = false;
        };
        void Journal(const Session& session, const std::wstring& state)
        {
            const fs::path path = fs::path(session.Item.WorkingFile).parent_path();
            // Locate the session root even for entries in nested folders.
            auto root = path;
            while (root.parent_path() != RecoveryRoot() && root != root.parent_path()) root = root.parent_path();
            if (root.parent_path() != RecoveryRoot()) return;
            const auto manifest = root / L"session.ini";
            if (!fs::exists(manifest))
            {
                Handle file(CreateFileW(manifest.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
                if (file) { const WORD bom = 0xfeff; DWORD written; WriteFile(file.Value, &bom, 2, &written, nullptr); }
            }
            const auto put = [&](const wchar_t* key, const std::wstring& value)
            { WritePrivateProfileStringW(L"MiaoZip", key, value.c_str(), manifest.c_str()); };
            put(L"Archive", session.Item.Archive); put(L"Entry", session.Item.Entry);
            put(L"WorkingFile", session.Item.WorkingFile); put(L"Snapshot", session.Item.Snapshot);
            put(L"ArchiveHash", session.Item.ArchiveHash); put(L"CommittedHash", session.Committed);
            put(L"Backup", session.Item.Backup); put(L"State", state); put(L"Error", session.Error);
        }

        struct State
        {
            std::atomic<bool> Enabled{ false };
            std::mutex Mutex;
            std::condition_variable Wake;
            std::vector<std::unique_ptr<Session>> Sessions;
            bool Stop = false;
            std::thread Worker;
            State() : Worker([this] { Run(); }) {}
            ~State() { Close(); }
            void Close()
            {
                { std::lock_guard lock(Mutex); Stop = true; }
                Wake.notify_all();
                if (Worker.joinable()) Worker.join();
            }
            void Run() noexcept
            {
                std::unique_lock lock(Mutex);
                while (!Stop)
                {
                    Wake.wait_for(lock, std::chrono::milliseconds(1000));
                    if (Stop) break;
                    if (!Enabled) continue;
                    for (auto& pointer : Sessions)
                    {
                        auto& session = *pointer;
                        if (session.Ready || session.InFlight || session.Failed) continue;
                        try
                        {
                            std::error_code error;
                            const DWORD attributes = GetFileAttributesW(session.Item.WorkingFile.c_str());
                            if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
                            {
                                session.Failed = true; session.Error = L"工作副本变为链接，已暂停同步";
                                Journal(session, L"paused-link"); continue;
                            }
                            const auto size = fs::file_size(session.Item.WorkingFile, error);
                            if (error) continue; // An atomic editor save can temporarily remove the path.
                            if (size > DocumentLimit)
                            {
                                session.Failed = true; session.Error = L"文档已超过 20 MiB，已暂停同步";
                                Journal(session, L"paused-size"); continue;
                            }
                            auto hash = HashFile(session.Item.WorkingFile);
                            if (hash.empty()) continue;
                            if (hash == session.Committed) { session.Observed = hash; continue; }
                            if (hash != session.Observed)
                            {
                                session.Observed = hash; session.ChangedAt = GetTickCount64(); continue;
                            }
                            if (GetTickCount64() - session.ChangedAt < 2000) continue;
                            auto snapshot = fs::path(session.Item.WorkingFile).parent_path() / (L".miaozip-snapshot-" + NewId());
                            if (!CopyFileW(session.Item.WorkingFile.c_str(), snapshot.c_str(), TRUE)) continue;
                            if (HashFile(snapshot) != hash || HashFile(session.Item.WorkingFile) != hash)
                            { DeleteFileW(snapshot.c_str()); continue; }
                            session.Item.Snapshot = snapshot.wstring(); session.Item.Hash = hash;
                            session.Ready = true;
                            Journal(session, L"pending");
                        }
                        catch (...) { session.Failed = true; session.Error = L"恢复文件不可写，已暂停同步"; }
                    }
                }
                for (auto& session : Sessions) Journal(*session, L"kept-on-close");
            }
        };
        State& Get() { static State state; return state; }
    }

    fs::path RecoveryRoot()
    {
        wchar_t module[32768]{};
        GetModuleFileNameW(nullptr, module, 32768);
        auto directory = fs::path(module).parent_path();
        if (_wcsicmp(directory.filename().c_str(), L"app") == 0) return directory.parent_path() / L"Recovery";
        wchar_t local[32768]{};
        GetEnvironmentVariableW(L"LOCALAPPDATA", local, 32768);
        return fs::path(local) / L"MiaoZip" / L"Recovery";
    }
    fs::path CreateSessionDirectory()
    {
        const auto directory = RecoveryRoot() / NewId();
        std::error_code error; fs::create_directories(directory, error);
        return error ? fs::path() : directory;
    }
    bool Enabled() { return Get().Enabled; }
    void SetEnabled(bool enabled)
    {
        auto& state = Get(); state.Enabled = enabled;
        { std::lock_guard lock(state.Mutex);
          for (auto& session : state.Sessions)
          {
              if (!enabled && !session->InFlight) { session->Ready = false; session->Observed.clear(); }
              Journal(*session, enabled ? L"watching" : L"paused-by-switch");
          } }
        state.Wake.notify_all();
    }
    std::wstring ExistingFile(const std::wstring& archive, const std::wstring& entry)
    {
        auto& state = Get(); std::lock_guard lock(state.Mutex);
        for (auto& session : state.Sessions)
            if (_wcsicmp(session->Item.Archive.c_str(), archive.c_str()) == 0 && session->Item.Entry == entry)
                return session->Item.WorkingFile;
        return {};
    }
    bool Add(const std::wstring& archive, const std::wstring& entry, const fs::path& workingFile,
        const std::wstring& archiveHash)
    {
        auto& state = Get(); std::lock_guard lock(state.Mutex);
        if (state.Sessions.size() >= 8 || archiveHash.empty() || !SafeEntry(entry)) return false;
        for (auto& item : state.Sessions)
            if (_wcsicmp(item->Item.Archive.c_str(), archive.c_str()) == 0 && item->Item.Entry == entry) return false;
        auto session = std::make_unique<Session>();
        session->Item.Id = NewId(); session->Item.Archive = archive; session->Item.Entry = entry;
        session->Item.WorkingFile = workingFile.wstring(); session->Item.ArchiveHash = archiveHash;
        session->Committed = HashFile(workingFile); session->Observed = session->Committed;
        if (session->Committed.empty()) return false;
        session->Item.Backup = archive + L".miaozip-" + session->Item.Id + L".bak";
        Journal(*session, L"watching");
        state.Sessions.push_back(std::move(session)); state.Wake.notify_all(); return true;
    }
    std::optional<Commit> TakeReady()
    {
        auto& state = Get(); std::lock_guard lock(state.Mutex);
        if (!state.Enabled || state.Stop) return {};
        for (auto& session : state.Sessions) if (session->InFlight) return {};
        for (auto& session : state.Sessions)
            if (session->Ready && !session->Failed)
            {
                session->Ready = false; session->InFlight = true; Journal(*session, L"updating");
                return session->Item;
            }
        return {};
    }
    void Complete(const Commit& commit, HRESULT result, const std::wstring& candidate)
    {
        auto& state = Get(); std::lock_guard lock(state.Mutex);
        const auto newHash = SUCCEEDED(result) ? HashFile(commit.Archive) : std::wstring();
        for (auto& item : state.Sessions)
        {
            auto& session = *item;
            if (session.Item.Id == commit.Id)
            {
                session.InFlight = false;
                if (SUCCEEDED(result) && !newHash.empty())
                {
                    session.Committed = commit.Hash;
                    const auto backupCopy = fs::path(commit.WorkingFile).parent_path() / (L"archive-before-save-" + NewId() + L".bak");
                    if (CopyFileW(commit.Backup.c_str(), backupCopy.c_str(), TRUE) && HashFile(backupCopy) == commit.ArchiveHash)
                    {
                        DeleteFileW(commit.Backup.c_str()); session.Item.Backup = backupCopy.wstring();
                        session.Backups.push_back(backupCopy);
                        // Retain the initial original plus the two latest successfully verified backups.
                        if (session.Backups.size() > 3)
                        {
                            DeleteFileW(session.Backups[1].c_str());
                            session.Backups.erase(session.Backups.begin() + 1);
                        }
                    }
                    Journal(session, L"saved");
                    session.Item.Backup = commit.Archive + L".miaozip-" + NewId() + L".bak";
                    DeleteFileW(commit.Snapshot.c_str());
                }
                else
                {
                    session.Failed = true;
                    session.Error = result == HRESULT_FROM_WIN32(ERROR_FILE_INVALID)
                        ? L"压缩包已从外部更改，请从恢复文件另存"
                        : L"回写失败，工作文件与快照已保留";
                    Journal(session, L"needs-recovery");
                    const auto manifest = fs::path(commit.WorkingFile).parent_path() / L"transaction-error.txt";
                    Handle file(CreateFileW(manifest.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
                    if (file)
                    {
                        const auto message = std::wstring(L"\xfeffHRESULT=") + std::to_wstring(static_cast<unsigned long>(result)) + L"\r\nCandidate=" + candidate + L"\r\nBackup=" + commit.Backup;
                        DWORD count; WriteFile(file.Value, message.data(), static_cast<DWORD>(message.size() * 2), &count, nullptr);
                    }
                }
            }
            if (SUCCEEDED(result) && !newHash.empty() && _wcsicmp(session.Item.Archive.c_str(), commit.Archive.c_str()) == 0)
                session.Item.ArchiveHash = newHash;
        }
    }
    std::wstring Status()
    {
        auto& state = Get(); std::lock_guard lock(state.Mutex);
        for (auto& session : state.Sessions) if (session->Failed) return L"编辑暂停：" + session->Error;
        for (auto& session : state.Sessions) if (session->InFlight) return L"正在回写压缩包…";
        if (!state.Enabled) return state.Sessions.empty() ? L"" : L"编辑已暂停 · 恢复文件已保留";
        for (auto& session : state.Sessions)
            if (session->Ready || session->Observed != session->Committed) return L"等待文档保存稳定…";
        return state.Sessions.empty() ? L"包内编辑已开启 · 双击小文档开始" : L"包内编辑：已同步 · " + std::to_wstring(state.Sessions.size()) + L" 个工作副本";
    }
    size_t Count() { auto& state = Get(); std::lock_guard lock(state.Mutex); return state.Sessions.size(); }
    void Shutdown() { Get().Close(); }
}
