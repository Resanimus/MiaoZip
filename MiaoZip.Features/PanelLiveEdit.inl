// Included by the native PanelItemOpen translation unit.
namespace
{
    bool MiaoZipDocument(const UString& name)
    {
        const auto extension = MiaoZip::fs::path(name.Ptr()).extension().wstring();
        const wchar_t* supported[] = { L".txt", L".md", L".csv", L".rtf", L".doc", L".docx",
            L".xls", L".xlsx", L".ppt", L".pptx", L".odt", L".ods", L".pdf" };
        for (auto item : supported) if (_wcsicmp(extension.c_str(), item) == 0) return true;
        return false;
    }
}

void CPanel::StartMiaoZipEdit(UInt32 index, bool useEditor)
{
    CDisableTimerProcessing timer0(g_App.Panels[0]);
    CDisableTimerProcessing timer1(g_App.Panels[1]);
    const auto explain = [&](const wchar_t* reason)
    {
        std::wstring message(reason);
        message += L"\n\n可关闭“包内编辑”后使用普通预览。已建立的工作副本始终保留在“恢复文件”中。";
        ::MessageBoxW(*this, message.c_str(), L"MiaoZip 包内编辑", MB_OK | MB_ICONINFORMATION);
    };
    if (_parentFolders.Size() != 1 || IsThereReadOnlyFolder() || IsItem_Folder(index) || IsItem_AltStream(index))
    { explain(L"当前文件位于只读、嵌套压缩包或特殊数据流中，暂不支持自动回写。"); return; }
    const std::wstring archive = fs2us(_parentFolders.Front().FilePath).Ptr();
    const auto archivePath = MiaoZip::fs::path(archive);
    const DWORD attributes = GetFileAttributesW(archive.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_REPARSE_POINT)) || archive.rfind(L"\\\\", 0) == 0)
    { explain(L"首版包内编辑仅支持本地可写的普通压缩包。"); return; }
    std::error_code sizeError;
    if (MiaoZip::fs::file_size(archivePath, sizeError) > MiaoZip::ArchiveLimit || sizeError)
    { explain(L"此压缩包超过 256 MiB，自动回写已停用。"); return; }
    CMyComPtr<IGetFolderArcProps> getter;
    CMyComPtr<IFolderArcProps> properties;
    _folder.QueryInterface(IID_IGetFolderArcProps, &getter);
    if (getter) getter->GetFolderArcProps(&properties);
    UInt32 levels = 0;
    NCOM::CPropVariant format, solid, encrypted, size;
    if (!properties || properties->GetArcNumLevels(&levels) != S_OK || levels != 1 ||
        properties->GetArcProp(0, kpidType, &format) != S_OK || format.vt != VT_BSTR)
    { explain(L"无法确认此压缩包的更新格式，自动回写已停用。"); return; }
    const bool isZip = _wcsicmp(format.bstrVal, L"zip") == 0;
    const bool is7z = _wcsicmp(format.bstrVal, L"7z") == 0;
    if (!isZip && !is7z) { explain(L"首版包内编辑支持 ZIP 和非固实 7z。"); return; }
    if (is7z && (properties->GetArcProp(0, kpidSolid, &solid) != S_OK || solid.vt != VT_BOOL || solid.boolVal != VARIANT_FALSE))
    { explain(L"固实 7z 更新可能重压大量内容，暂不启用自动回写。"); return; }
    NCOM::CPropVariant symbolic, hardLink;
    _folder->GetProperty(index, kpidSymLink, &symbolic);
    _folder->GetProperty(index, kpidHardLink, &hardLink);
    if ((symbolic.vt == VT_BSTR && symbolic.bstrVal && *symbolic.bstrVal) ||
        (hardLink.vt == VT_BSTR && hardLink.bstrVal && *hardLink.bstrVal))
    { explain(L"链接文件不支持自动回写。"); return; }
    _folder->GetProperty(index, kpidEncrypted, &encrypted);
    if (_parentFolders.Front().UsePassword || (encrypted.vt == VT_BOOL && encrypted.boolVal != VARIANT_FALSE))
    { explain(L"加密压缩包暂不启用自动回写。"); return; }
    UInt64 bytes = 0;
    _folder->GetProperty(index, kpidSize, &size);
    if (!ConvertPropVariantToUInt64(size, bytes) || bytes > MiaoZip::DocumentLimit)
    { explain(L"此文档超过 20 MiB，或无法读取大小，暂不启用自动回写。"); return; }

    const UString relative = GetItemRelPath(index);
    const std::wstring folderPrefix = _currentFolderPrefix.Ptr();
    const std::wstring root = archive + L"\\";
    if (folderPrefix.size() < root.size() || _wcsnicmp(folderPrefix.c_str(), root.c_str(), root.size()) != 0)
    { explain(L"无法确定文档在压缩包内的位置。"); return; }
    const std::wstring entry = folderPrefix.substr(root.size()) + relative.Ptr();
    if (!MiaoZip::SafeEntry(entry) || !MiaoZip::SafeEntry(relative.Ptr()))
    { explain(L"文档路径包含不受支持的字符。"); return; }
    auto existing = MiaoZip::LiveEdit::ExistingFile(archive, entry);
    if (!existing.empty())
    {
        CProcess process;
        StartApplication(UString(MiaoZip::fs::path(existing).parent_path().c_str()), UString(existing.c_str()), *this, process);
        return;
    }
    if (MiaoZip::LiveEdit::Count() >= 8) { explain(L"当前已有 8 个编辑工作副本。请关闭并重新打开 MiaoZip 开始下一批；已有副本将保留。"); return; }
    const auto archiveHash = MiaoZip::HashFile(archive);
    const auto directory = MiaoZip::LiveEdit::CreateSessionDirectory();
    if (archiveHash.empty() || directory.empty()) { explain(L"无法创建恢复副本，请检查磁盘空间与目录权限。"); return; }
    CCopyToOptions options;
    options.folder = directory.wstring().c_str(); options.folder.Add_PathSepar();
    options.includeAltStreams = false;
    options.showErrorMessages = true;
    options.ZoneIdMode = NExtract::NZoneIdMode::kAll;
    options.NeedRegistryZone = false;
    CRecordVector<UInt32> indices; indices.Add(index);
    UStringVector messages;
    bool passwordDefined = false; UString password;
    const HRESULT extracted = CopyTo(options, indices, &messages, passwordDefined, password);
    if (extracted != S_OK || !messages.IsEmpty()) return;
    const auto workingFile = directory / relative.Ptr();
    if (!MiaoZip::fs::exists(workingFile) || MiaoZip::fs::file_size(workingFile, sizeError) > MiaoZip::DocumentLimit || sizeError ||
        MiaoZip::HashFile(archive) != archiveHash || !MiaoZip::LiveEdit::Add(archive, entry, workingFile, archiveHash))
    { explain(L"压缩包发生变化或无法建立编辑会话。已提取的文件保留在恢复目录。"); return; }
    if (GetFileAttributesW(workingFile.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT)
    { explain(L"提取结果是链接文件，已停止打开。"); return; }
    CProcess process;
    HRESULT launched = useEditor
        ? StartEditApplication(UString(workingFile.c_str()), true, *this, process)
        : StartApplication(UString(directory.c_str()), UString(workingFile.c_str()), *this, process);
    if (launched != S_OK) MessageBox_Error_HRESULT(launched);
    Refresh_StatusBar();
}

void CPanel::ProcessMiaoZipEdit()
{
    static bool processing = false;
    if (processing || !_processTimer || !_processNotify) return;
    const auto next = MiaoZip::LiveEdit::TakeReady();
    if (!next) return;
    processing = true;
    CDisableTimerProcessing timer0(g_App.Panels[0]);
    CDisableTimerProcessing timer1(g_App.Panels[1]);
    CDisableNotify notify0(g_App.Panels[0]);
    CDisableNotify notify1(g_App.Panels[1]);
    const auto& commit = *next;
    HRESULT result = E_FAIL;
    MiaoZip::SafeUpdateContext context{ commit.ArchiveHash, commit.Backup, {} };
    UString paths[2]; CSelectedState selected[2]; bool restore[2]{};
    try
    {
        for (unsigned i = 0; i < g_App.NumPanels; ++i)
        {
            auto& panel = g_App.Panels[i];
            if (!panel._parentFolders.IsEmpty() &&
                _wcsicmp(fs2us(panel._parentFolders.Front().FilePath), commit.Archive.c_str()) == 0)
            {
                paths[i] = panel._currentFolderPrefix; panel.SaveSelectedState(selected[i]); restore[i] = true;
                panel.ClearMiaoZipTree(); panel.CloseOpenFolders();
            }
        }
        // A fresh archive agent prevents stale indices after navigation or another session's save.
        if (MiaoZip::HashFile(commit.Archive) != commit.ArchiveHash) result = HRESULT_FROM_WIN32(ERROR_FILE_INVALID);
        else
        {
            CFfpOpen open;
            result = open.OpenFileFolderPlugin(nullptr, us2fs(UString(commit.Archive.c_str())), UString(), GetParent());
            if (result == S_OK && open.Folder)
            {
                CMyComPtr<IFolderFolder> folder = open.Folder;
                const auto entry = MiaoZip::fs::path(commit.Entry);
                for (const auto& component : entry.parent_path())
                {
                    CMyComPtr<IFolderFolder> child;
                    result = folder->BindToFolder(component.c_str(), &child);
                    if (result != S_OK || !child) { result = E_FAIL; break; }
                    folder = child;
                }
                UInt32 count = 0, index = static_cast<UInt32>(-1);
                if (result == S_OK) result = folder->GetNumberOfItems(&count);
                for (UInt32 i = 0; result == S_OK && i < count; ++i)
                {
                    NCOM::CPropVariant name;
                    folder->GetProperty(i, kpidName, &name);
                    if (name.vt == VT_BSTR && entry.filename().wstring() == name.bstrVal) { index = i; break; }
                }
                if (index == static_cast<UInt32>(-1)) result = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
                if (result == S_OK)
                {
                    CThreadCopyFrom thread;
                    folder.QueryInterface(IID_IFolderOperations, &thread.FolderOperations);
                    if (!thread.FolderOperations) result = E_NOTIMPL;
                    else
                    {
                        thread.UpdateCallbackSpec = new CUpdateCallback100Imp;
                        thread.UpdateCallback = thread.UpdateCallbackSpec;
                        thread.UpdateCallbackSpec->ProgressDialog = &thread;
                        thread.UpdateCallbackSpec->Init();
                        thread.ItemIndex = index; thread.FullPath = commit.Snapshot.c_str(); thread.SafeContext = &context;
                        MiaoZip::Handle snapshot(::CreateFileW(commit.Snapshot.c_str(), GENERIC_READ, FILE_SHARE_READ,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
                        if (!snapshot || MiaoZip::HashHandle(snapshot.Value) != commit.Hash) result = HRESULT_FROM_WIN32(ERROR_FILE_INVALID);
                        else
                        {
                            result = thread.Create(L"MiaoZip · 保存文档", GetParent());
                            if (result == S_OK) result = thread.Result;
                        }
                    }
                }
            }
        }
    }
    catch (...) { result = E_FAIL; }
    for (unsigned i = 0; i < g_App.NumPanels; ++i) if (restore[i])
    {
        auto& panel = g_App.Panels[i];
        const HRESULT reopened = panel.BindToPathAndRefresh(paths[i]);
        if (reopened != S_OK && result == S_OK) result = reopened;
        if (reopened == S_OK) panel.RefreshListCtrl(selected[i]);
    }
    MiaoZip::LiveEdit::Complete(commit, result, context.Candidate);
    g_App.RefreshTitle(true);
    processing = false;
    if (result != S_OK)
        ::MessageBoxW(GetParent(), result == HRESULT_FROM_WIN32(ERROR_FILE_INVALID) ? L"压缩包已从外部更改，本次修改未回写。\n工作副本与快照已保留，请通过“恢复文件”另存。" : L"本次回写未完成，编辑已暂停。\n工作副本与快照已保留，请通过“恢复文件”查看并另存。",
            L"MiaoZip 包内编辑", MB_OK | MB_ICONWARNING);
    Refresh_StatusBar();
}
