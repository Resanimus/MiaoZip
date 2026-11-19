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
        constexpr ULONGLONG SaveQuietTimeMs = 2000;
        constexpr ULONGLONG ContentCheckIntervalMs = 60000;

        struct Session
        {
            Commit Item;
            std::wstring Committed, Observed, Error;
            std::vector<fs::path> Backups;
            ULONGLONG ChangedAt = 0;
            bool Ready = false, InFlight = false, Failed = false;
            // Only the worker reads or rearms this watch after Add publishes the session.
            HANDLE DirectoryWatch = INVALID_HANDLE_VALUE;
            ULONGLONG LastContentCheck = 0;

            ~Session()
            {
                if (DirectoryWatch != INVALID_HANDLE_VALUE)
                    FindCloseChangeNotification(DirectoryWatch);
            }

            bool DirectoryChanged()
            {
                if (DirectoryWatch == INVALID_HANDLE_VALUE) return true;
                const DWORD wait = WaitForSingleObject(DirectoryWatch, 0);
                if (wait == WAIT_TIMEOUT) return false;
                if (wait != WAIT_OBJECT_0 || !FindNextChangeNotification(DirectoryWatch))
                {
                    FindCloseChangeNotification(DirectoryWatch);
                    DirectoryWatch = INVALID_HANDLE_VALUE;
                }
                return true;
            }
        };

        enum class ScanError { None, Link, TooLarge, Recovery };

        struct DocumentScan
        {
            std::wstring Hash;
            fs::path Snapshot;
            ScanError Error = ScanError::None;
        };

        DocumentScan ScanDocument(Session& session, const std::wstring& observed,
            bool pending, bool snapshotDue)
        {
            const fs::path path = session.Item.WorkingFile;
            const DWORD attributes = GetFileAttributesW(path.c_str());
            if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
                return { {}, {}, ScanError::Link };

            // Check the size even if no directory event was delivered, and
            // keep polling while a save is pending or a read must be retried.
            std::error_code error;
            const auto size = fs::file_size(path, error);
            if (error)
            {
                session.LastContentCheck = 0;
                return {};
            }
            if (size > DocumentLimit) return { {}, {}, ScanError::TooLarge };
            const bool changed = session.DirectoryChanged();
            const auto now = GetTickCount64();
            if (!changed && !pending && session.LastContentCheck != 0 &&
                now - session.LastContentCheck < ContentCheckIntervalMs)
                return {};

            session.LastContentCheck = 0;
            DocumentScan scan;
            scan.Hash = HashFile(path, DocumentLimit);
            if (scan.Hash.empty()) return scan;
            session.LastContentCheck = now;
            if (!snapshotDue || scan.Hash != observed) return scan;

            const auto snapshot = path.parent_path() / (L".miaozip-snapshot-" + NewId());
            if (!CopyFileW(path.c_str(), snapshot.c_str(), TRUE)) return scan;
            if (HashFile(snapshot, DocumentLimit) != scan.Hash || HashFile(path, DocumentLimit) != scan.Hash)
            {
                DeleteFileW(snapshot.c_str());
                return scan;
            }
            scan.Snapshot = snapshot;
            return scan;
        }

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
            unsigned EnableGeneration = 0;
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
                    for (size_t i = 0; !Stop && Enabled && i < Sessions.size(); ++i)
                    {
                        // Sessions are not removed while the worker is alive.
                        auto& session = *Sessions[i];
                        if (session.Ready || session.InFlight || session.Failed) continue;
                        try
                        {
                            const auto generation = EnableGeneration;
                            const auto observed = session.Observed;
                            const bool pending = observed != session.Committed;
                            const bool snapshotDue = pending && !observed.empty() &&
                                GetTickCount64() - session.ChangedAt >= SaveQuietTimeMs;
                            DocumentScan scan;
                            // Large reads and snapshot copies must not hold up
                            // the UI's status, pause or ready-session queries.
                            lock.unlock();
                            try { scan = ScanDocument(session, observed, pending, snapshotDue); }
                            catch (...) { scan.Error = ScanError::Recovery; }
                            lock.lock();
                            if (Stop || !Enabled || generation != EnableGeneration)
                            {
                                if (!scan.Snapshot.empty()) DeleteFileW(scan.Snapshot.c_str());
                                continue;
                            }
                            if (scan.Error != ScanError::None)
                            {
                                session.Failed = true;
                                if (scan.Error == ScanError::Link)
                                {
                                    session.Error = L"工作副本变为链接，已暂停同步";
                                    Journal(session, L"paused-link");
                                }
                                else if (scan.Error == ScanError::TooLarge)
                                {
                                    session.Error = L"文档已超过 " + std::to_wstring(DocumentLimitMiB) + L" MiB，已暂停同步";
                                    Journal(session, L"paused-size");
                                }
                                else session.Error = L"恢复文件不可写，已暂停同步";
                                continue;
                            }
                            if (!scan.Snapshot.empty())
                            {
                                session.Item.Snapshot = scan.Snapshot.wstring();
                                session.Item.Hash = scan.Hash;
                                session.Ready = true;
                                Journal(session, L"pending");
                            }
                            if (!scan.Hash.empty() && scan.Hash != session.Observed)
                            {
                                session.Observed = std::move(scan.Hash);
                                session.ChangedAt = GetTickCount64();
                            }
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
        auto& state = Get();
        { std::lock_guard lock(state.Mutex);
          state.Enabled = enabled;
          ++state.EnableGeneration;
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
        session->DirectoryWatch = FindFirstChangeNotificationW(workingFile.parent_path().c_str(), FALSE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES |
            FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE);
        session->Committed = HashFile(workingFile, DocumentLimit);
        session->Observed = session->Committed;
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
        return state.Sessions.empty() ? L"包内编辑已开启 · 双击文档开始" : L"包内编辑：已同步 · " + std::to_wstring(state.Sessions.size()) + L" 个工作副本";
    }
    size_t Count() { auto& state = Get(); std::lock_guard lock(state.Mutex); return state.Sessions.size(); }
    void Shutdown() { Get().Close(); }
}
