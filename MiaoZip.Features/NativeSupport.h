// MiaoZip native file transactions. Original code, MIT licensed.
#pragma once
#include <windows.h>
#include <objbase.h>
#include <bcrypt.h>
#include <filesystem>
#include <string>
#include <vector>
#pragma comment(lib, "bcrypt.lib")

namespace MiaoZip
{
    namespace fs = std::filesystem;
    constexpr unsigned long long DocumentLimit = 20ull * 1024 * 1024;
    constexpr unsigned long long ArchiveLimit = 256ull * 1024 * 1024;

    struct Handle
    {
        HANDLE Value = INVALID_HANDLE_VALUE;
        explicit Handle(HANDLE value = INVALID_HANDLE_VALUE) : Value(value) {}
        ~Handle() { if (Value != INVALID_HANDLE_VALUE && Value) CloseHandle(Value); }
        Handle(const Handle&) = delete;
        Handle& operator=(const Handle&) = delete;
        explicit operator bool() const { return Value && Value != INVALID_HANDLE_VALUE; }
    };

    inline std::wstring HashHandle(HANDLE file)
    {
        LARGE_INTEGER zero{};
        if (!SetFilePointerEx(file, zero, nullptr, FILE_BEGIN)) return {};
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return {};
        DWORD objectSize = 0, readSize = 0;
        BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize), &readSize, 0);
        std::vector<unsigned char> object(objectSize), buffer(1024 * 1024);
        unsigned char digest[32]{};
        bool ok = BCryptCreateHash(algorithm, &hash, object.data(), objectSize, nullptr, 0, 0) >= 0;
        while (ok)
        {
            DWORD count = 0;
            if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr)) { ok = false; break; }
            if (!count) break;
            ok = BCryptHashData(hash, buffer.data(), count, 0) >= 0;
        }
        if (ok) ok = BCryptFinishHash(hash, digest, sizeof(digest), 0) >= 0;
        if (hash) BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0);
        if (!ok) return {};
        std::wstring result;
        constexpr wchar_t digits[] = L"0123456789abcdef";
        for (auto value : digest) { result += digits[value >> 4]; result += digits[value & 15]; }
        return result;
    }

    inline std::wstring HashFile(const fs::path& path)
    {
        Handle file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
        return file ? HashHandle(file.Value) : std::wstring();
    }

    inline bool SafeEntry(const std::wstring& name)
    {
        if (name.empty() || name.front() == L'/' || name.front() == L'\\' || name.find(L':') != std::wstring::npos) return false;
        auto path = fs::path(name);
        for (const auto& part : path)
            if (part == L".." || part == L"." || part.empty()) return false;
        return !path.is_absolute();
    }

    inline std::wstring NewId()
    {
        GUID id{};
        if (FAILED(CoCreateGuid(&id))) return {};
        wchar_t text[40]{};
        StringFromGUID2(id, text, 40);
        return std::wstring(text + 1, 36);
    }

    struct SafeUpdateContext
    {
        std::wstring ExpectedHash;
        std::wstring Backup;
        std::wstring Candidate;
    };
    // Agent and progress worker are linked into the same native executable.
    inline thread_local SafeUpdateContext* CurrentUpdate = nullptr;
    struct SafeUpdateScope
    {
        SafeUpdateContext* Previous;
        explicit SafeUpdateScope(SafeUpdateContext* context) : Previous(CurrentUpdate) { CurrentUpdate = context; }
        ~SafeUpdateScope() { CurrentUpdate = Previous; }
    };

    struct ArchiveMutex
    {
        Handle Mutex;
        bool Locked = false;
        explicit ArchiveMutex(const fs::path& archive)
        {
            auto identity = fs::absolute(archive).lexically_normal().wstring();
            CharLowerBuffW(identity.data(), static_cast<DWORD>(identity.size()));
            unsigned long long key = 14695981039346656037ull;
            for (const auto character : identity) { key ^= static_cast<unsigned short>(character); key *= 1099511628211ull; }
            const auto name = L"Local\\MiaoZip.EditTransaction." + std::to_wstring(key);
            Mutex.Value = CreateMutexW(nullptr, FALSE, name.c_str());
            if (Mutex)
            {
                const auto result = WaitForSingleObject(Mutex.Value, 5000);
                Locked = result == WAIT_OBJECT_0 || result == WAIT_ABANDONED;
            }
        }
        ~ArchiveMutex() { if (Locked) ReleaseMutex(Mutex.Value); }
    };

    inline HRESULT ReplaceArchive(const fs::path& original, const fs::path& candidate, SafeUpdateContext& context)
    {
        context.Candidate = candidate.wstring();
        ArchiveMutex transaction(original);
        if (!transaction.Locked) return HRESULT_FROM_WIN32(ERROR_BUSY);
        {
            Handle output(CreateFileW(candidate.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
            if (!output || !FlushFileBuffers(output.Value)) return HRESULT_FROM_WIN32(GetLastError());
        }
        // Deny new writers while comparing the entire archive and replacing it.
        Handle originalLock(CreateFileW(original.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
        if (!originalLock) return HRESULT_FROM_WIN32(GetLastError());
        const auto actual = HashHandle(originalLock.Value);
        if (actual.empty() || actual != context.ExpectedHash) return HRESULT_FROM_WIN32(ERROR_FILE_INVALID);
        if (!ReplaceFileW(original.c_str(), candidate.c_str(), context.Backup.c_str(), 0, nullptr, nullptr))
            return HRESULT_FROM_WIN32(GetLastError());
        return S_OK;
    }
}
