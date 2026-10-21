#include "../MiaoZip.Features/LiveEdit.h"
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <stdexcept>
using namespace MiaoZip;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void write(const fs::path& file, const std::string& text) { std::ofstream output(file, std::ios::binary | std::ios::trunc); output << text; output.close(); }
std::optional<LiveEdit::Commit> waitReady()
{
    for (unsigned i = 0; i < 75; ++i)
    {
        auto item = LiveEdit::TakeReady(); if (item) return item;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return {};
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
        LiveEdit::Shutdown();
        require(fs::exists(session/L"session.ini") && fs::exists(atomicDoc),"shutdown preserves recovery");
        std::cout << "PASS repeated saves, in-flight generation, pause, Office replacement, recovery on failure and close\n";
        return 0;
    }
    catch(const std::exception& error) { std::cerr<<"FAIL "<<error.what()<<"\n";LiveEdit::Shutdown();return 1; }
}
