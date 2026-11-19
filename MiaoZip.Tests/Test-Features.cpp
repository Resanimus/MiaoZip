#include "../MiaoZip.Features/LiveEdit.h"
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <stdexcept>
using namespace MiaoZip;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void write(const fs::path& file, const std::string& text) { std::ofstream output(file, std::ios::binary | std::ios::trunc); output << text; output.close(); }
std::optional<LiveEdit::Commit> waitReady(unsigned retries = 75)
{
    for (unsigned i = 0; i < retries; ++i)
    {
        auto item = LiveEdit::TakeReady(); if (item) return item;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return {};
}

static void WriteSizedDocument(const fs::path& path, unsigned long long size, char lastByte)
{
    Handle file(CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    require(static_cast<bool>(file), "open synthetic large document");
    LARGE_INTEGER target;
    target.QuadPart = static_cast<LONGLONG>(size - 1);
    DWORD written = 0;
    require(SetFilePointerEx(file.Value, target, nullptr, FILE_BEGIN) &&
        WriteFile(file.Value, &lastByte, 1, &written, nullptr) && written == 1,
        "write synthetic large document");
}

int wmain(int argc, wchar_t** argv)
{
    try
    {
        require(argc == 2, "test directory missing");
        const fs::path root = argv[1]; fs::create_directories(root);
        const auto original=root/L"archive.zip", candidate=root/L"candidate.zip", backup=root/L"before.zip";
        write(original, "abc");
        require(HashFile(original) == L"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA256 vector");
        require(SafeEntry(L"资料\\合同.docx") && !SafeEntry(L"..\\file") && !SafeEntry(L"C:\\file") && !SafeEntry(L"file:stream") && !SafeEntry(L"\\absolute"), "entry path gate");
        write(candidate,"version2");
        SafeUpdateContext context{HashFile(original),backup.wstring(),{}};
        require(ReplaceArchive(original,candidate,context)==S_OK,"safe replacement failed");
        require(HashFile(backup)==context.ExpectedHash && !fs::exists(candidate),"backup and replacement readback");
        const auto preserved=HashFile(original);
        write(candidate,"conflicting-candidate");
        SafeUpdateContext conflict{L"deliberately-stale",(root/L"conflict-backup.zip").wstring(),{}};
        require(ReplaceArchive(original,candidate,conflict)==HRESULT_FROM_WIN32(ERROR_FILE_INVALID),"external modification rejected");
        require(HashFile(original)==preserved && fs::exists(candidate),"conflict preserved both files");
        std::cout << "PASS hash, paths, atomic replacement, backup, conflict preservation\n";

        auto session=LiveEdit::CreateSessionDirectory(); require(!session.empty(),"persistent session directory");
        auto document=session/L"合同.txt";write(document,"first");
        require(LiveEdit::Add(original.wstring(),L"资料\\合同.txt",document,preserved),"add session");
        require(LiveEdit::ExistingFile(original.wstring(),L"资料\\合同.txt")==document.wstring(),"reuse active working file");
        LiveEdit::SetEnabled(true);
        write(document,"save one");
        auto first=waitReady(); require(first.has_value() && first->Hash==HashFile(document),"first stable save");
        // The editor saves again while the first archive generation is still in flight.
        write(document,"save two while in flight");
        const auto temp=root/L"generation2.zip";write(temp,"archive containing save one");
        SafeUpdateContext firstContext{first->ArchiveHash,first->Backup,{}};
        require(ReplaceArchive(original,temp,firstContext)==S_OK,"first generation native transaction");
        LiveEdit::Complete(*first,S_OK,firstContext.Candidate);
        auto second=waitReady(); require(second.has_value() && second->Hash==HashFile(document) && second->ArchiveHash==HashFile(original),"repeat save and archive baseline advancement");
        LiveEdit::Complete(*second,E_ABORT,L"");
        require(fs::exists(document) && fs::exists(second->Snapshot),"failure keeps snapshot and document");

        auto atomicDir=LiveEdit::CreateSessionDirectory();auto atomicDoc=atomicDir/L"atomic.txt";write(atomicDoc,"old");
        require(LiveEdit::Add(original.wstring(),L"atomic.txt",atomicDoc,HashFile(original)),"second session");
        LiveEdit::SetEnabled(false);write(atomicDoc,"paused");
        std::this_thread::sleep_for(std::chrono::milliseconds(3500));require(!LiveEdit::TakeReady(),"switch pauses writeback");
        auto officeTemp=atomicDir/L"office.tmp";write(officeTemp,"Office atomic save");
        require(ReplaceFileW(atomicDoc.c_str(),officeTemp.c_str(),nullptr,0,nullptr,nullptr),"simulate Office rename-save");
        LiveEdit::SetEnabled(true);
        auto atomic=waitReady();require(atomic.has_value() && atomic->Hash==HashFile(atomicDoc),"Office save with no child process");
        LiveEdit::Complete(*atomic,E_ABORT,L"");

        const auto largeArchive = root / L"large.zip";
        write(largeArchive, "original large archive");
        const auto largeSession = LiveEdit::CreateSessionDirectory();
        require(!largeSession.empty(), "large document recovery directory");
        const auto largeDocument = largeSession / L"large.txt";
        WriteSizedDocument(largeDocument, DocumentLimit, 'a');
        require(LiveEdit::Add(largeArchive.wstring(), L"large.txt", largeDocument, HashFile(largeArchive)),
            "a document at the new size limit is editable");
        const auto previousTimestamp = fs::last_write_time(largeDocument);
        WriteSizedDocument(largeDocument, DocumentLimit, 'b');
        fs::last_write_time(largeDocument, previousTimestamp);
        auto large = waitReady(180);
        require(large && fs::file_size(large->Snapshot) == DocumentLimit &&
            HashFile(large->Snapshot) == HashFile(largeDocument),
            "same-size large save with unchanged last-write timestamp reaches a verified snapshot");
        const auto largeCandidate = root / L"large-generation.zip";
        write(largeCandidate, "updated large archive");
        SafeUpdateContext largeContext{large->ArchiveHash, large->Backup, {}};
        require(ReplaceArchive(largeArchive, largeCandidate, largeContext) == S_OK, "commit large document");
        LiveEdit::Complete(*large, S_OK, largeContext.Candidate);
        WriteSizedDocument(largeDocument, DocumentLimit + 1, 'c');
        const auto stableArchive = HashFile(largeArchive);
        const auto manifest = largeSession / L"session.ini";
        std::wstring phase;
        for (unsigned i = 0; i < 80 && phase != L"paused-size"; ++i)
        {
            require(!LiveEdit::TakeReady(), "oversized document was queued for writeback");
            wchar_t state[64]{};
            GetPrivateProfileStringW(L"MiaoZip", L"State", L"", state, 64, manifest.c_str());
            phase = state;
            if (phase != L"paused-size") std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        require(phase == L"paused-size" && HashFile(largeArchive) == stableArchive &&
            fs::file_size(largeDocument) == DocumentLimit + 1,
            "over-limit save pauses and preserves archive and work copy");
        require(!LiveEdit::Add(largeArchive.wstring(), L"second.txt", largeDocument, stableArchive),
            "a new session rejects a document greater than the size limit");
        std::cout << "PASS 128 MiB document boundary, unchanged-timestamp save, overflow recovery\n";
        LiveEdit::Shutdown();
        require(fs::exists(session/L"session.ini") && fs::exists(atomicDoc),"shutdown preserves recovery");
        std::cout << "PASS repeated saves, in-flight generation, pause, Office replacement, recovery on failure and close\n";
        return 0;
    }
    catch(const std::exception& error) { std::cerr<<"FAIL "<<error.what()<<"\n";LiveEdit::Shutdown();return 1; }
}
